#ifndef _proof_h_INCLUDED
#define _proof_h_INCLUDED

#include "tracer.hpp"

namespace CaDiCaL {

/*------------------------------------------------------------------------*/

class File;
struct Clause;
struct Internal;
class Observer;

/*------------------------------------------------------------------------*/

// Provides proof checking and writing through observers.

class Proof {

  Internal * internal;

  vector<ELit> clause;           // of external literals
  vector<Observer *> observers; // owned, so deleted in destructor

  void add_literal (ILit internal_lit);  // add to 'clause'
  void add_literals (Clause *);         // add to 'clause'

  void add_literals (const vector<ILit> &);      // ditto

  void add_original_clause (clause_id_t); // notify observers of original clauses
  void add_derived_clause (clause_id_t, bool, int); // notify observers of derived clauses, bool is_imported, int glue if non-unit
  void delete_clause (clause_id_t);       // notify observers of deleted clauses
  void finalize_clause (clause_id_t);     // notify observers of active clauses

public:

  Proof (Internal *);
  ~Proof ();

  void connect (Observer * v) { observers.push_back (v); }
  //we want the tracer to go before exporting to ensure the proof is written,
  //   so we put it at the beginning
  void connect_tracer (Tracer *t) { observers.insert(observers.begin(), t); }

  // Add original clauses to the proof (for online proof checking).
  //
  void add_original_clause (clause_id_t id, const vector<ILit> &);

  // Add derived (such as learned) clauses to the proof.
  //
  void add_derived_empty_clause (clause_id_t id){ add_derived_empty_clause(id, false); }
  void add_derived_empty_clause (clause_id_t id, bool is_imported);
  void add_derived_unit_clause (clause_id_t id, ILit unit, bool is_imported);
  void add_derived_clause (Clause *, bool); //bool is_imported
  void add_derived_clause (clause_id_t id, const vector<ILit> &, bool, int); //bool is_imported, int glue

  void delete_clause (clause_id_t, const vector<ILit> &);
  void delete_clause (Clause *);

  // notify observers of active clauses (deletion after empty clause)
  //
  void finalize_clause (clause_id_t, const vector<ILit> &);
  void finalize_clause (Clause *);
  void finalize_clause_ext (clause_id_t, const vector<ELit> &);

  // These two actually pretend to add and remove a clause.
  //
  void flush_clause (Clause *);           // remove falsified literals
  void strengthen_clause (Clause *, ILit); // remove second argument

  void add_todo (const vector<int64_t> &);

  void flush ();
};

}

#define PROOF_TODO(proof, s, ...) if (proof) { \
  LOG ("PROOF missing chain (" s ")"); \
  proof->add_todo({__VA_ARGS__}); }

#endif
