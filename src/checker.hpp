#ifndef _checker_hpp_INCLUDED
#define _checker_hpp_INCLUDED

#include "observer.hpp"         // Alphabetically after 'checker'.

/*------------------------------------------------------------------------*/

namespace CaDiCaL {

/*------------------------------------------------------------------------*/

// This checker implements an online forward DRUP proof checker enabled by
// 'opts.checkproof' (requires 'opts.check' also to be enabled).  This is
// useful for model basted testing (and delta-debugging), where we can not
// rely on an external proof checker such as 'drat-trim'.  We also do not
// have yet  a flow for offline incremental proof checking, while this
// checker here can also be used in an incremental setting.
//
// In essence the checker implements is a simple propagation online SAT
// solver with an additional hash table to find clauses fast for
// 'delete_clause'.  It requires its own data structure for clauses
// ('CheckerClause') and watches ('CheckerWatch').
//
// In our experiments the checker slows down overall SAT solving time by a
// factor of 3, which we contribute to its slightly less efficient
// implementation.

/*------------------------------------------------------------------------*/

struct CheckerClause {
  CheckerClause * next;         // collision chain link for hash table
  uint64_t hash;                // previously computed full 64-bit hash
  unsigned size;                // zero if this is a garbage clause
  ELit literals[2];              // otherwise 'literals' of length 'size'
};

struct CheckerWatch {
  ELit blit;
  unsigned size;
  CheckerClause * clause;
  CheckerWatch () { }
  CheckerWatch (ELit b, CheckerClause * c) :
    blit (b), size (c->size), clause (c)
  { }
};

typedef vector<CheckerWatch> CheckerWatcher;

/*------------------------------------------------------------------------*/

class Checker : public Observer {

  Internal * internal;

  // Capacity of variable values.
  //
  int64_t size_vars;

  // For the assignment we want to have an as fast access as possible and
  // thus we use an array which can also be indexed by negative literals and
  // is actually valid in the range [-size_vars+1, ..., size_vars-1].
  //
  signed char * vals;

  // The 'watchers' and 'marks' data structures are not that time critical
  // and thus we access them by first mapping a literal to 'unsigned'.
  //
  static unsigned l2u (int lit);
  vector<CheckerWatcher> watchers;      // watchers of literals
  vector<signed char> marks;            // mark bits of literals

  signed char & mark (ELit lit);
  CheckerWatcher & watcher (ELit lit);

  bool inconsistent;            // found or added empty clause

  uint64_t num_clauses;         // number of clauses in hash table
  uint64_t num_garbage;         // number of garbage clauses
  uint64_t size_clauses;        // size of clause hash table
  CheckerClause ** clauses;     // hash table of clauses
  CheckerClause * garbage;      // linked list of garbage clauses

  vector<ELit> unsimplified;     // original clause for reporting
  vector<ELit> simplified;       // clause for sorting

  vector<ELit> trail;            // for propagation

  unsigned next_to_propagate;   // next to propagate on trail

  void enlarge_vars (int64_t idx);
  void import_literal (ELit lit);
  void import_clause (const vector<ELit> &);
  bool tautological ();

  static const unsigned num_nonces = 4;

  uint64_t nonces[num_nonces];  // random numbers for hashing
  uint64_t last_hash;           // last computed hash value of clause
  uint64_t compute_hash (int64_t); // compute and save hash value of clause

  // Reduce hash value to the actual size.
  //
  static uint64_t reduce_hash (uint64_t hash, uint64_t size);

  void enlarge_clauses ();      // enlarge hash table for clauses
  void insert (int64_t);        // insert clause in hash table
  CheckerClause ** find (int64_t); // find clause position in hash table
  CheckerClause ** find_id (int64_t); // find clause position in hash table

  void add_clause (clause_id_t id, const char * type);

  void collect_garbage_clauses ();

  CheckerClause * new_clause ();
  void delete_clause (CheckerClause *);

  signed char val (ELit lit);            // returns '-1', '0' or '1'

  bool clause_satisfied (CheckerClause*);

  void assign (ELit lit);        // assign a literal to true
  void assume (ELit lit);        // assume a literal
  bool propagate ();            // propagate and check for conflicts
  bool propagate_chain (const vector<int64_t>*); // propagate and check for conflicts
  void backtrack (unsigned);    // prepare for next clause
  bool check (const vector<int64_t>*); // check simplified clause is implied

  struct {

    int64_t added;              // number of added clauses
    int64_t original;           // number of added original clauses
    int64_t derived;            // number of added derived clauses

    int64_t deleted;            // number of deleted clauses

    int64_t assumptions;        // number of assumed literals
    int64_t propagations;       // number of propagated literals

    int64_t insertions;         // number of clauses added to hash table
    int64_t collisions;         // number of hash collisions in 'find'
    int64_t searches;           // number of searched clauses in 'find'

    int64_t checks;             // number of implication checks

    int64_t collections;        // garbage collections
    int64_t units;

  } stats;

public:

  Checker (Internal *);
  ~Checker ();

  // The following three implement the 'Observer' interface.
  //
  void add_original_clause (clause_id_t, const vector<ELit> &);
  void add_derived_clause (clause_id_t, const vector<clause_id_t> *, const vector<ELit> &);
  void delete_clause (clause_id_t, const vector<ELit> &);

  void print_stats ();
  void dump ();                 // for debugging purposes only
};

}

#endif
