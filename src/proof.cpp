#include "internal.hpp"

namespace CaDiCaL {

using namespace std;

/*------------------------------------------------------------------------*/

// Enable proof logging and checking by allocating a 'Proof' object.

void Internal::new_proof_on_demand () {
  if (!proof) {
    proof = new Proof (this);
    LOG ("connecting proof to internal solver");
  }
}

// Enable proof tracing.

void Internal::trace (File * file) {
  assert (!tracer);
  new_proof_on_demand ();
  tracer = new Tracer (this, file, opts.binary, opts.lrat && !opts.frat, opts.frat, opts.proofdelete);
  LOG ("PROOF connecting proof tracer");
  proof->connect_tracer(tracer);
}

// Enable proof checking.

void Internal::check () {
  assert (!checker);
  new_proof_on_demand ();
  checker = new Checker (this);
  LOG ("PROOF connecting proof checker");
  proof->connect (checker);
}

// We want to close a proof trace and stop checking as soon we are done.

void Internal::close_trace () {
  assert (tracer);
  tracer->close ();
}

// We can flush a proof trace file before actually closing it.

void Internal::flush_trace () {
  assert (tracer);
  tracer->flush ();
}

/*------------------------------------------------------------------------*/

Proof::Proof (Internal * s) : internal (s) { LOG ("PROOF new"); }

Proof::~Proof () { LOG ("PROOF delete"); }

/*------------------------------------------------------------------------*/

inline void Proof::add_literal (int internal_lit) {
  const int external_lit = internal->externalize (internal_lit);
  clause.push_back (external_lit);
}

inline void Proof::add_literals (Clause * c) {
  for (auto const & lit : * c)
    add_literal (lit);
}

inline void Proof::add_literals (const vector<int> & c) {
  for (auto const & lit : c)
    add_literal (lit);
}

/*------------------------------------------------------------------------*/

void Proof::add_original_clause (clause_id_t id, const vector<int> & c) {
  LOG (c, "PROOF adding original internal clause [%ld]", id);
  add_literals (c);
  add_original_clause (id);
}

void Proof::add_derived_empty_clause (clause_id_t id, bool is_imported) {
  LOG ("PROOF adding empty clause [%ld]", id);
  assert (clause.empty ());
  add_derived_clause (id, is_imported, 0);
}

void Proof::add_derived_unit_clause (clause_id_t id, int internal_unit, bool is_imported) {
  LOG ("PROOF adding unit clause [%ld] %d", id, internal_unit);
  assert (clause.empty ());
  add_literal (internal_unit);
  add_derived_clause (id, is_imported, 0);
}

/*------------------------------------------------------------------------*/

void Proof::add_derived_clause (Clause * c, bool is_imported) {
  LOG (c, "PROOF adding to proof derived");
  assert (clause.empty ());
  add_literals (c);
  add_derived_clause (c->id, is_imported, c->glue);
}

void Proof::add_derived_clause (clause_id_t id, const vector<int> & c, bool is_imported, int glue) {
  LOG (internal->clause, "PROOF adding derived clause [%ld]", id);
  assert (clause.empty ());
  for (const auto & lit : c)
    add_literal (lit);
  add_derived_clause (id, is_imported, glue);
}

void Proof::delete_clause (Clause * c) {
  LOG (c, "PROOF deleting from proof");
  assert (clause.empty ());
  add_literals (c);
  delete_clause (c->id);
}

void Proof::delete_clause (clause_id_t id, const vector<int> & c) {
  LOG (c, "PROOF deleting from proof [%ld]", id);
  assert (clause.empty ());
  add_literals (c);
  delete_clause (id);
}

void Proof::finalize_clause (Clause * c) {
  if (!internal->opts.lrat) return;
  LOG (c, "PROOF finalizing");
  assert (clause.empty ());
  add_literals (c);
  finalize_clause (c->id);
}

void Proof::finalize_clause (clause_id_t id, const vector<int> & c) {
  if (!internal->opts.lrat) return;
  LOG (c, "PROOF finalizing [%ld]", id);
  assert (clause.empty ());
  add_literals (c);
  finalize_clause (id);
}

void Proof::finalize_clause_ext (clause_id_t id, const vector<int> & c) {
  clause = c;
  finalize_clause (id);
}

void Proof::add_todo (const vector<int64_t> & c) {
  for (size_t i = 0; i < observers.size (); i++)
    observers[i]->add_todo (c);
}

/*------------------------------------------------------------------------*/

// During garbage collection clauses are shrunken by removing falsified
// literals. To avoid copying the clause, we provide a specialized tracing
// function here, which traces the required 'add' and 'remove' operations.

void Proof::flush_clause (Clause * c) {
  LOG (c, "PROOF flushing falsified literals in");
  assert (clause.empty ());
  internal->chain.clear ();
  for (int i = 0; i < c->size; i++) {
    int internal_lit = c->literals[i];
    if (internal->fixed (internal_lit) < 0) {
      internal->chain.push_back(internal->var (internal_lit).unit_id);
      continue;
    }
    add_literal (internal_lit);
  }
  internal->chain.push_back(c->id);
  clause_id_t id = internal->next_clause_id();
  add_derived_clause (id, false, c->glue);
  delete_clause (c);
  c->id = id;
}

// While strengthening clauses, e.g., through self-subsuming resolutions,
// during subsumption checking, we have a similar situation, except that we
// have to remove exactly one literal.  Again the following function allows
// to avoid copying the clause and instead provides tracing of the required
// 'add' and 'remove' operations.

void Proof::strengthen_clause (Clause * c, int remove) {
  LOG (c, "PROOF strengthen by removing %d in", remove);
  assert (clause.empty ());
  for (int i = 0; i < c->size; i++) {
    int internal_lit = c->literals[i];
    if (internal_lit == remove) continue;
    add_literal (internal_lit);
  }
  clause_id_t id = internal->next_clause_id();
  add_derived_clause (id, false, c->glue);
  delete_clause (c);
  c->id = id;
}

/*------------------------------------------------------------------------*/

void Proof::add_original_clause (clause_id_t id) {
  LOG (clause, "PROOF adding original external clause");
  for (size_t i = 0; i < observers.size (); i++)
    observers[i]->add_original_clause (id, clause);
  clause.clear ();
}

void Proof::add_derived_clause (clause_id_t id, bool is_imported, int glue) {
  LOG (clause, "PROOF adding derived external clause");
  vector<clause_id_t> * chain = internal->chain.empty () ? 0 : &internal->chain;
  if ((int) clause.size() < glue){
      //fix glue size so it is not smaller than the clause size
      glue = clause.size();
  }
  else if (glue < 1){
      //fix glue size so it is at least the minimum size
      glue = 1;
  }
  for (size_t i = 0; i < observers.size (); i++)
      observers[i]->add_derived_clause (id, chain, clause, is_imported, glue);
  internal->chain.clear ();
  clause.clear ();
}

void Proof::delete_clause (clause_id_t id) {
  LOG (clause, "PROOF deleting external clause");
  for (size_t i = 0; i < observers.size (); i++)
    observers[i]->delete_clause (id, clause);
  clause.clear ();
}

void Proof::finalize_clause (clause_id_t id) {
  if (!internal->opts.lrat) return;
  LOG (clause, "PROOF finalizing external clause [%ld]", id);
  for (size_t i = 0; i < observers.size (); i++)
    observers[i]->finalize_clause (id, clause);
  clause.clear ();
}

}
