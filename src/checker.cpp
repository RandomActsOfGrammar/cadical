#include "internal.hpp"

extern "C" {
#include <inttypes.h>
}

namespace CaDiCaL {

/*------------------------------------------------------------------------*/

inline unsigned Checker::l2u (int lit) {
   assert (lit);
   assert (lit != INT_MIN);
   unsigned res = 2*(abs (lit) - 1);
   if (lit < 0) res++;
   return res;
}

inline signed char Checker::val (ELit lit) {
  assert (e_val(lit));
  assert (e_val(lit) != INT_MIN);
  assert (abs (e_val(lit)) < size_vars);
  assert (vals[e_val(lit)] == -vals[-e_val(lit)]);
  return vals[e_val(lit)];
}

signed char & Checker::mark (ELit lit) {
  const unsigned u = l2u (e_val(lit));
  assert (u < marks.size ());
  return marks[u];
}

inline CheckerWatcher & Checker::watcher (ELit lit) {
  const unsigned u = l2u (e_val(lit));
  assert (u < watchers.size ());
  return watchers[u];
}

/*------------------------------------------------------------------------*/

CheckerClause * Checker::new_clause () {
  const size_t size = simplified.size ();
  assert (size > 1), assert (size <= UINT_MAX);
  const size_t bytes = sizeof (CheckerClause) + (size - 2) * sizeof (int);
  CheckerClause * res = (CheckerClause *) new char [bytes];
  res->next = 0;
  res->hash = last_hash;
  res->size = size;
  ELit * literals = res->literals, * p = literals;
  for (const auto & lit : simplified)
    *p++ = lit;
  num_clauses++;

  // First two literals are used as watches and should not be false.
  //
  for (unsigned i = 0; i < 2; i++) {
    ELit lit = literals[i];
    if (!val (lit)) continue;
    for (unsigned j = i + 1; j < size; j++) {
      ELit other = literals[j];
      if (val (other)) continue;
      swap (literals[i], literals[j]);
      break;
    }
  }
  assert (!val (literals [0]));
  assert (!val (literals [1]));
  watcher (literals[0]).push_back (CheckerWatch (literals[1], res));
  watcher (literals[1]).push_back (CheckerWatch (literals[0], res));

  return res;
}

void Checker::delete_clause (CheckerClause * c) {
  if (c->size) {
    assert (c->size > 1);
    assert (num_clauses);
    num_clauses--;
  } else {
    assert (num_garbage);
    num_garbage--;
  }
  delete [] (char*) c;
}

void Checker::enlarge_clauses () {
  assert (num_clauses == size_clauses);
  const uint64_t new_size_clauses = size_clauses ? 2*size_clauses : 1;
  LOG ("CHECKER enlarging clauses of checker from %" PRIu64 " to %" PRIu64,
    (uint64_t) size_clauses, (uint64_t) new_size_clauses);
  CheckerClause ** new_clauses;
  new_clauses = new CheckerClause * [ new_size_clauses ];
  clear_n (new_clauses, new_size_clauses);
  for (uint64_t i = 0; i < size_clauses; i++) {
    for (CheckerClause * c = clauses[i], * next; c; c = next) {
      next = c->next;
      const uint64_t h = reduce_hash (c->hash, new_size_clauses);
      c->next = new_clauses[h];
      new_clauses[h] = c;
    }
  }
  delete [] clauses;
  clauses = new_clauses;
  size_clauses = new_size_clauses;
}

bool Checker::clause_satisfied (CheckerClause * c) {
  for (unsigned i = 0; i < c->size; i++)
    if (val (c->literals[i]) > 0)
      return true;
  return false;
}

// The main reason why we have an explicit garbage collection phase is that
// removing clauses from watcher lists eagerly might lead to an accumulated
// quadratic algorithm.  Thus we delay removing garbage clauses from watcher
// lists until garbage collection (even though we remove garbage clauses on
// the fly during propagation too).  We also remove satisfied clauses.
//
void Checker::collect_garbage_clauses () {

  stats.collections++;

  for (size_t i = 0; i < size_clauses; i++) {
    CheckerClause ** p = clauses + i, * c;
    while ((c = *p)) {
      if (clause_satisfied (c)) {
        c->size = 0;                    // mark as garbage
        *p = c->next;
        c->next = garbage;
        garbage = c;
        num_garbage++;
        assert (num_clauses);
        num_clauses--;
      } else p = &c->next;
    }
  }

  LOG ("CHECKER collecting %" PRIu64 " garbage clauses %.0f%%",
    num_garbage, percent (num_garbage, num_clauses));

  for (int lit = -size_vars + 1; lit < size_vars; lit++) {
    if (!lit) continue;
    CheckerWatcher & ws = watcher (lit);
    const auto end = ws.end ();
    auto j = ws.begin (), i = j;
    for (;i != end; i++) {
      CheckerWatch & w = *i;
      if (w.clause->size) *j++ = w;
    }
    if (j == ws.end ()) continue;
    if (j == ws.begin ()) erase_vector (ws);
    else ws.resize (j - ws.begin ());
  }

  for (CheckerClause * c = garbage, * next; c; c = next)
    next = c->next, delete_clause (c);

  assert (!num_garbage);
  garbage = 0;
}

/*------------------------------------------------------------------------*/

Checker::Checker (Internal * i)
:
  internal (i),
  size_vars (0), vals (0),
  inconsistent (false), num_clauses (0), num_garbage (0),
  size_clauses (0), clauses (0), garbage (0),
  next_to_propagate (0), last_hash (0)
{
  LOG ("CHECKER new");

  // Initialize random number table for hash function.
  //
  Random random (42);
  for (unsigned n = 0; n < num_nonces; n++) {
    uint64_t nonce = random.next ();
    if (!(nonce & 1)) nonce++;
    assert (nonce), assert (nonce & 1);
    nonces[n] = nonce;
  }

  memset (&stats, 0, sizeof (stats));           // Initialize statistics.
}

Checker::~Checker () {
  LOG ("CHECKER delete");
  vals -= size_vars;
  delete [] vals;
  for (size_t i = 0; i < size_clauses; i++)
    for (CheckerClause * c = clauses[i], * next; c; c = next)
      next = c->next, delete_clause (c);
  for (CheckerClause * c = garbage, * next; c; c = next)
    next = c->next, delete_clause (c);
  delete [] clauses;
}

/*------------------------------------------------------------------------*/

// The simplicity for accessing 'vals' and 'watchers' directly through a
// signed integer literal, comes with the price of slightly more complex
// code in deleting and enlarging the checker data structures.

void Checker::enlarge_vars (int64_t idx) {

  assert (0 < idx), assert (idx <= INT_MAX);

  int64_t new_size_vars = size_vars ? 2*size_vars : 2;
  while (idx >= new_size_vars) new_size_vars *= 2;
  LOG ("CHECKER enlarging variables of checker from %" PRId64 " to %" PRId64 "",
    size_vars, new_size_vars);

  signed char * new_vals;
  new_vals = new signed char [ 2*new_size_vars ];
  clear_n (new_vals, 2*new_size_vars);
  new_vals += new_size_vars;
  if (size_vars) // To make sanitizer happy (without '-O').
    memcpy ((void*) (new_vals - size_vars),
            (void*) (vals - size_vars), 2*size_vars);
  vals -= size_vars;
  delete [] vals;
  vals = new_vals;

  watchers.resize (2*new_size_vars);
  marks.resize (2*new_size_vars);

  assert (idx < new_size_vars);
  size_vars = new_size_vars;
}

inline void Checker::import_literal (ELit lit) {
  assert (lit);
  assert (lit != INT_MIN);
  int idx = abs (e_val(lit));
  if (idx >= size_vars) enlarge_vars (idx);
  simplified.push_back (lit);
  unsimplified.push_back (lit);
}

void Checker::import_clause (const vector<ELit> & c) {
  for (const auto & lit : c)
    import_literal (lit);
}

struct lit_smaller {
  bool operator () (ELit a, ELit b) const {
    int c = abs (e_val(a)), d = abs (e_val(b));
    if (c < d) return true;
    if (c > d) return false;
    return e_val(a) < e_val(b);
  }
};

bool Checker::tautological () {
  sort (simplified.begin (), simplified.end (), lit_smaller ());
  const auto end = simplified.end ();
  auto j = simplified.begin ();
  ELit prev = 0;
  for (auto i = j; i != end; i++) {
    ELit lit = *i;
    if (lit == prev) continue;          // duplicated literal
    if (lit == -prev) return true;      // tautological clause
    const signed char tmp = val (lit);
    if (tmp > 0) return true;           // satisfied literal and clause
    *j++ = prev = lit;
  }
  simplified.resize (j - simplified.begin ());
  return false;
}

/*------------------------------------------------------------------------*/

uint64_t Checker::reduce_hash (uint64_t hash, uint64_t size) {
  assert (size > 0);
  unsigned shift = 32;
  uint64_t res = hash;
  while ((((uint64_t)1) << shift) > size) {
    res ^= res >> shift;
    shift >>= 1;
  }
  res &= size - 1;
  assert (res < size);
  return res;
}

uint64_t Checker::compute_hash (int64_t id) {
  if (id) return last_hash = (uint64_t) id;
  unsigned i = 0, j = 0;
  uint64_t tmp = 0;
  for (i = 0; i < simplified.size (); i++) {
    ELit lit = simplified[i];
    tmp += nonces[j++] * (uint64_t) e_val(lit);
    if (j == num_nonces) j = 0;
  }
  return last_hash = tmp;
}

CheckerClause ** Checker::find_id (int64_t id) {
  assert (id);
  stats.searches++;
  CheckerClause ** res, * c;
  const uint64_t h = reduce_hash (id, size_clauses);
  for (res = clauses + h; (c = *res); res = &c->next) {
    if (c->hash == (uint64_t) id) break;
    stats.collisions++;
  }
  return res;
}

CheckerClause ** Checker::find (int64_t id) {
  stats.searches++;
  CheckerClause ** res, * c;
  const uint64_t hash = compute_hash (id);
  const unsigned size = simplified.size ();
  const uint64_t h = reduce_hash (hash, size_clauses);
  for (const auto & lit : simplified) mark (lit) = true;
  for (res = clauses + h; (c = *res); res = &c->next) {
    if (c->hash == hash && c->size == size) {
      bool found = true;
      const ELit * literals = c->literals;
      for (unsigned i = 0; found && i != size; i++)
        found = mark (literals[i]);
      if (found) break;
    }
    stats.collisions++;
  }
  for (const auto & lit : simplified) mark (lit) = false;
  return res;
}

void Checker::insert (int64_t id) {
  stats.insertions++;
  if (num_clauses == size_clauses) enlarge_clauses ();
  const uint64_t h = reduce_hash (compute_hash (id), size_clauses);
  CheckerClause * c = new_clause ();
  c->next = clauses[h];
  clauses[h] = c;
}

/*------------------------------------------------------------------------*/

inline void Checker::assign (ELit lit) {
  assert (!val (lit));
  vals[e_val(lit)] = 1;
  vals[-e_val(lit)] = -1;
  trail.push_back (lit);
}

inline void Checker::assume (ELit lit) {
  signed char tmp = val (lit);
  if (tmp > 0) return;
  assert (!tmp);
  stats.assumptions++;
  assign (lit);
}

void Checker::backtrack (unsigned previously_propagated) {

  assert (previously_propagated <= trail.size ());

  while (trail.size () > previously_propagated) {
    ELit lit = trail.back ();
    assert (val (lit) > 0);
    assert (val (-lit) < 0);
    vals[e_val(lit)] = vals[-e_val(lit)] = 0;
    trail.pop_back ();
  }

  trail.resize (previously_propagated);
  next_to_propagate = previously_propagated;
  assert (trail.size () == next_to_propagate);
}

/*------------------------------------------------------------------------*/

// This is a standard propagation routine without using blocking literals
// nor without saving the last replacement position.

bool Checker::propagate () {
  bool res = true;
  while (res && next_to_propagate < trail.size ()) {
    ELit lit = trail[next_to_propagate++];
    stats.propagations++;
    assert (val (lit) > 0);
    assert (abs (lit) < size_vars);
    CheckerWatcher & ws = watcher (-lit);
    const auto end = ws.end ();
    auto j = ws.begin (), i = j;
    for (; res && i != end; i++) {
      CheckerWatch & w = *j++ = *i;
      const ELit blit = w.blit;
      assert (blit != -lit);
      const signed char blit_val = val (blit);
      if (blit_val > 0) continue;
      const unsigned size = w.size;
      if (size == 2) {                          // not precise since
        if (blit_val < 0) res = false;          // clause might be garbage
        else assign (w.blit);                   // but still sound
      } else {
        assert (size > 2);
        CheckerClause * c = w.clause;
        if (!c->size) { j--; continue; }        // skip garbage clauses
        assert (size == c->size);
        ELit * lits = c->literals;
        ELit other = e_val(lits[0])^e_val(lits[1])^e_val(-lit);
        assert (other != -lit);
        signed char other_val = val (other);
        if (other_val > 0) { j[-1].blit = other; continue; }
        lits[0] = other, lits[1] = -lit;
        unsigned k;
        ELit replacement = 0;
        signed char replacement_val = -1;
        for (k = 2; k < size; k++)
          if ((replacement_val = val (replacement = lits[k])) >= 0)
            break;
        if (e_val(replacement_val) >= 0) {
          watcher (replacement).push_back (CheckerWatch (-lit, c));
          swap (lits[1], lits[k]);
          j--;
        } else if (!other_val) assign (other);
        else res = false;
      }
    }
    while (i != end) *j++ = *i++;
    ws.resize (j - ws.begin ());
  }
  return res;
}

bool Checker::propagate_chain (const vector<int64_t> * chain) {
  if (!chain) return propagate ();

  for (int64_t cid : *chain) {
    auto c = find_id (cid);
    if (!*c) continue;
    int size = (*c)->size;
    ELit * lits = (*c)->literals;
    ELit unit = 0;
    for (int k = 0; k < size; k++)
      if (val (lits[k]) >= 0) {
        if (e_val(unit)) return false;
        unit = lits[k];
      }
    if (!e_val(unit)) return true;
    assign (unit);
  }
  return false;
}

bool Checker::check (const vector<int64_t> * chain) {
  stats.checks++;
  if (inconsistent) return true;
  unsigned previously_propagated = next_to_propagate;
  for (const auto & lit : simplified)
    assume (-lit);
  bool res = !propagate_chain (chain);
  backtrack (previously_propagated);
  return res;
}

/*------------------------------------------------------------------------*/

void Checker::add_clause (int64_t id, const char * type) {
#ifndef LOGGING
  (void) type;
#endif

  ELit unit = 0;
  for (const auto & lit : simplified) {
    const signed char tmp = val (lit);
    if (tmp < 0) continue;
    assert (!tmp);
    if (e_val(unit)) { unit = INT_MIN; break; }
    unit = lit;
  }

  if (simplified.empty ()) {
    LOG ("CHECKER added empty %s clause", type);
    inconsistent = true;
  } if (!e_val(unit)) {
    LOG ("CHECKER added and checked falsified %s clause", type);
    inconsistent = true;
  } else if (e_val(unit) != INT_MIN) {
    LOG ("CHECKER added and checked %s unit clause %d", type, unit);
    assign (unit);
    stats.units++;
    if (!propagate ()) {
      LOG ("CHECKER inconsistent after propagating %s unit", type);
      inconsistent = true;
    }
  } else insert (id);
}

void Checker::add_original_clause (int64_t id, const vector<ELit> & c) {
  if (inconsistent) return;
  START (checking);
  LOG (c, "CHECKER addition of original clause");
  stats.added++;
  stats.original++;
  import_clause (c);
  if (tautological ())
    LOG ("CHECKER ignoring satisfied original clause");
  else add_clause (id, "original");
  simplified.clear ();
  unsimplified.clear ();
  STOP (checking);
}

void Checker::add_derived_clause (int64_t id, const vector<int64_t> * chain, const vector<ELit> & c) {
  if (inconsistent) return;
  START (checking);
  LOG (c, "CHECKER addition of derived clause");
  stats.added++;
  stats.derived++;
  import_clause (c);
  if (tautological ())
    LOG ("CHECKER ignoring satisfied derived clause");
  else if (!check (chain)) {
    fatal_message_start ();
    fputs ("failed to check derived clause:\n", stderr);
    for (const auto & lit : unsimplified)
        fprintf (stderr, "%d ", e_val(lit));
    fputc ('0', stderr);
    if (chain) {
      fputs ("\nchain:\n", stderr);
      for (const auto & cid : *chain) {
        fprintf (stderr, "%lld: ",cid);
        auto c = find_id (cid);
        if (!*c) { fprintf (stderr, "not found\n"); continue; }
        int size = (*c)->size;
        ELit * lits = (*c)->literals;
        for (int k = 0; k < size; k++)
            fprintf (stderr, "%d ", e_val(lits[k]));
        fprintf (stderr, "0\n");
      }
    }
    fatal_message_end ();
  } else add_clause (id, "derived");
  simplified.clear ();
  unsimplified.clear ();
  STOP (checking);
}

/*------------------------------------------------------------------------*/

void Checker::delete_clause (int64_t id, const vector<ELit> & c) {
  if (inconsistent) return;
  START (checking);
  LOG (c, "CHECKER checking deletion of clause");
  stats.deleted++;
  import_clause (c);
  if (!tautological ()) {
    CheckerClause ** p = find (id), * d = *p;
    if (d) {
      assert (d->size > 1);
      // Remove from hash table, mark as garbage, connect to garbage list.
      num_garbage++;
      assert (num_clauses);
      num_clauses--;
      *p = d->next;
      d->next = garbage;
      garbage = d;
      d->size = 0;
      // If there are enough garbage clauses collect them.
      if (num_garbage > 0.5 * max ((size_t) size_clauses, (size_t) size_vars))
        collect_garbage_clauses ();
    } else {
      fatal_message_start ();
      fputs ("deleted clause not in proof:\n", stderr);
      for (const auto & lit : unsimplified)
          fprintf (stderr, "%d ", e_val(lit));
      fputc ('0', stderr);
      fatal_message_end ();
    }
  }
  simplified.clear ();
  unsimplified.clear ();
  STOP (checking);
}

/*------------------------------------------------------------------------*/

void Checker::dump () {
  int max_var = 0;
  for (uint64_t i = 0; i < size_clauses; i++)
    for (CheckerClause * c = clauses[i]; c; c = c->next)
      for (unsigned i = 0; i < c->size; i++)
        if (abs (e_val(c->literals[i])) > max_var)
            max_var = abs (e_val(c->literals[i]));
  printf ("p cnf %d %" PRIu64 "\n", max_var, num_clauses);
  for (uint64_t i = 0; i < size_clauses; i++)
    for (CheckerClause * c = clauses[i]; c; c = c->next) {
      for (unsigned i = 0; i < c->size; i++)
          printf ("%d ", e_val(c->literals[i]));
      printf ("0\n");
    }
}

}
