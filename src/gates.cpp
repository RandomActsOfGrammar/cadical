#include "internal.hpp"

namespace CaDiCaL {

/*------------------------------------------------------------------------*/

// As in our original SATeLite published at SAT'05 we are trying to find
// gates in order to restrict the number of resolutions that need to be
// tried.  If there is such a gate, we only need to consider resolvents
// among gate and one non-gate clauses.  Resolvents between definitions will
// be tautological anyhow and resolvents among non-gates can actually be
// shown to be redundant too.

/*------------------------------------------------------------------------*/

// The next function returns a non-zero if the clause 'c', which is assumed
// to contain the literal 'first', after removing falsified literals is a
// binary clause.  Then the actual second literal is returned.

int
Internal::second_literal_in_binary_clause (Eliminator & eliminator,
                                           Clause * c, ILit first)
{
  assert (!c->garbage);
  ILit second = 0;
  for (const auto & lit : *c) {
    if (lit == first) continue;
    const signed char tmp = val (lit);
    if (tmp < 0) continue;
    if (tmp > 0) {
      mark_garbage (c);
      elim_update_removed_clause (eliminator, c);
      return 0;
    }
    if (i_val(second)) { second = INT_MIN; break; }
    second = lit;
  }
  if (!i_val(second)) return 0;
  if (i_val(second) == INT_MIN) return 0;
  assert (active (second));
#ifdef LOGGING
  if (c->size == 2) LOG (c, "found binary");
  else LOG (c, "found actual binary %d %d", first, second);
#endif
  return i_val(second);
}

/*------------------------------------------------------------------------*/

// Mark all other literals in binary clauses with 'first'.  During this
// marking we might also detect hyper unary resolvents producing a unit.
// If such a unit is found we propagate it and return immediately.

void Internal::mark_binary_literals (Eliminator & eliminator, ILit first) {

  if (unsat) return;
  if (val (first)) return;
  if (!eliminator.gates.empty ()) return;

  assert (!marked (first));
  assert (eliminator.marked.empty ());

  const Occs & os = occs (first);
  for (const auto & c : os) {
    if (c->garbage) continue;
    const int second =
      second_literal_in_binary_clause (eliminator, c, first);
    if (!second) continue;
    const int tmp = marked (second);
    if (tmp < 0) {
      LOG ("found binary resolved unit %d", first);
      PROOF_TODO (proof, "mark binary literals unit", 60); // TODO(Mario)
      assign_unit (first);
      elim_propagate (eliminator, first);
      return;
    }
    if (tmp > 0) {
      LOG (c, "duplicated actual binary clause");
      elim_update_removed_clause (eliminator, c);
      mark_garbage (c);
      continue;
    }
    eliminator.marked.push_back (second);
    mark (second);
    LOG ("marked second literal %d in binary clause %d %d",
      second, first, second);
  }
}

// Unmark all literals saved on the 'marked' stack.

void Internal::unmark_binary_literals (Eliminator & eliminator) {
  LOG ("unmarking %zd literals", eliminator.marked.size ());
  for (const auto & lit : eliminator.marked)
    unmark (lit);
  eliminator.marked.clear ();
}

/*------------------------------------------------------------------------*/

// Find equivalence for 'pivot'.  Requires that all other literals in binary
// clauses with 'pivot' are marked (through 'mark_binary_literals');

void Internal::find_equivalence (Eliminator & eliminator, ILit pivot) {

  if (!opts.elimequivs) return;

  assert (opts.elimsubst);

  if (unsat) return;
  if (val (pivot)) return;
  if (!eliminator.gates.empty ()) return;

  mark_binary_literals (eliminator, pivot);
  if (unsat || val (pivot)) goto DONE;

  for (const auto & c : occs (-pivot)) {

    if (c->garbage) continue;

    const int second =
      second_literal_in_binary_clause (eliminator, c, -pivot);
    if (!second) continue;
    const int tmp = marked (second);
    if (tmp > 0) {
      LOG ("found binary resolved unit %d", second);
      PROOF_TODO (proof, "find equivalence unit", 61); // TODO(Mario)
      assign_unit (second);
      elim_propagate (eliminator, second);
      if (val (pivot)) break;
      if (unsat) break;
    }
    if (tmp >= 0) continue;

    LOG ("found equivalence %d = %d", pivot, -second);
    stats.elimequivs++;
    stats.elimgates++;

    LOG (c, "first gate clause");
    assert (!c->gate);
    c->gate = true;
    eliminator.gates.push_back (c);

    Clause * d = 0;
    const Occs & ps = occs (pivot);
    for (const auto & e : ps) {
      if (e->garbage) continue;
      const int other =
        second_literal_in_binary_clause (eliminator, e, pivot);
      if (other == -second) { d = e; break; }
    }
    assert (d);

    LOG (d, "second gate clause");
    assert (!d->gate);
    d->gate = true;
    eliminator.gates.push_back (d);

    break;
  }

DONE:
  unmark_binary_literals (eliminator);
}

/*------------------------------------------------------------------------*/

// Find and gates for 'pivot' with a long clause, in which the pivot occurs
// positively.  Requires that all other literals in binary clauses with
// 'pivot' are marked (through 'mark_binary_literals');

void Internal::find_and_gate (Eliminator & eliminator, ILit pivot) {

  if (!opts.elimands) return;

  assert (opts.elimsubst);

  if (unsat) return;
  if (val (pivot)) return;
  if (!eliminator.gates.empty ()) return;

  mark_binary_literals (eliminator, pivot);
  if (unsat || val (pivot)) goto DONE;

  for (const auto & c : occs (-pivot)) {

    if (c->garbage) continue;
    if (c->size < 3) continue;

    bool all_literals_marked = true;
    unsigned arity = 0;
    for (const auto & lit : *c) {
      if (lit == -pivot) continue;
      assert (lit != pivot);
      signed char tmp = val (lit);
      if (tmp < 0) continue;
      assert (!tmp);
      tmp = marked (lit);
      if (tmp < 0) { arity++; continue; }
      all_literals_marked = false;
      break;
    }

    if (!all_literals_marked) continue;

#ifdef LOGGING
    if (opts.log) {
      Logger::print_log_prefix (this);
      tout.magenta ();
      printf ("found arity %u AND gate %d = ", arity, -pivot);
      bool first = true;
      for (const auto & lit : *c) {
        if (lit == -pivot) continue;
        assert (lit != pivot);
        if (!first) fputs (" & ", stdout);
        printf ("%d", -lit);
        first = false;
      }
      fputc ('\n', stdout);
      tout.normal ();
      fflush (stdout);
    }
#endif
    stats.elimands++;
    stats.elimgates++;

    (void) arity;
    assert (!c->gate);
    c->gate = true;
    eliminator.gates.push_back (c);
    for (const auto & lit : *c) {
      if (lit == -pivot) continue;
      assert (lit != pivot);
      signed char tmp = val (lit);
      if (tmp < 0) continue;
      assert (!tmp);
      assert (marked (lit) < 0);
      marks [vidx (lit)] *= 2;
    }

    unsigned count = 0;
    for (const auto & d : occs (pivot)) {
      if (d->garbage) continue;
      const int other =
        second_literal_in_binary_clause (eliminator, d, pivot);
      if (!other) continue;
      const int tmp = marked (other);
      if (tmp != 2) continue;
      LOG (d, "AND gate binary side clause");
      assert (!d->gate);
      d->gate = true;
      eliminator.gates.push_back (d);
      count++;
    }
    assert (count >= arity);
    (void) count;

    break;
  }

DONE:
  unmark_binary_literals (eliminator);
}

/*------------------------------------------------------------------------*/

// Find and extract ternary clauses.

bool Internal::get_ternary_clause (Clause * d, ILit & a, ILit & b, ILit & c)
{
  if (d->garbage) return false;
  if (d->size < 3) return false;
  int found = 0;
  a = b = c = 0;
  for (const auto & lit : *d) {
    if (val (lit)) continue;
       if (++found == 1) a = lit;
    else if (found == 2) b = lit;
    else if (found == 3) c = lit;
    else return false;
  }
  return found == 3;
}

// This function checks whether 'd' exists as ternary clause.

bool Internal::match_ternary_clause (Clause * d, ILit a, ILit b, ILit c) {
  if (d->garbage) return false;
  int found = 0;
  for (const auto & lit : *d) {
    if (val (lit)) continue;
    if (a != lit && b != lit && c != lit) return false;
    found++;
  }
  return found == 3;
}

Clause *
Internal::find_ternary_clause (ILit a, ILit b, ILit c) {
  if (occs (b).size () > occs (c).size ()) swap (b, c);
  if (occs (a).size () > occs (b).size ()) swap (a, b);
  for (auto d : occs (a))
    if (match_ternary_clause (d, a, b, c))
      return d;
  return 0;
}

/*------------------------------------------------------------------------*/

// Find if-then-else gate.

void Internal::find_if_then_else (Eliminator & eliminator, ILit pivot) {

  if (!opts.elimites) return;

  assert (opts.elimsubst);

  if (unsat) return;
  if (val (pivot)) return;
  if (!eliminator.gates.empty ()) return;

  const Occs & os = occs (pivot);
  const auto end = os.end ();
  for (auto i = os.begin (); i != end; i++) {
    Clause * di = *i;
    ILit ai, bi, ci;
    if (!get_ternary_clause (di, ai, bi, ci)) continue;
    if (bi == pivot) swap (ai, bi);
    if (ci == pivot) swap (ai, ci);
    assert (ai == pivot);
    for (auto j = i + 1; j != end; j++) {
      Clause * dj = *j;
      ILit aj, bj, cj;
      if (!get_ternary_clause (dj, aj, bj, cj)) continue;
      if (bj == pivot) swap (aj, bj);
      if (cj == pivot) swap (aj, cj);
      assert (aj == pivot);
      if (abs (i_val(bi)) == abs (i_val(cj))) swap (bj, cj);
      if (abs (i_val(ci)) == abs (i_val(cj))) continue;
      if (bi != -bj) continue;
      Clause * d1 = find_ternary_clause (-pivot, bi, -ci);
      if (!d1) continue;
      Clause * d2 = find_ternary_clause (-pivot, bj, -cj);
      if (!d2) continue;
      LOG (di, "1st if-then-else");
      LOG (dj, "2nd if-then-else");
      LOG (d1, "3rd if-then-else");
      LOG (d2, "4th if-then-else");
      LOG ("found ITE gate %d == (%d ? %d : %d)", pivot, -bi, -ci, -cj);
      assert (!di->gate);
      assert (!dj->gate);
      assert (!d1->gate);
      assert (!d2->gate);
      di->gate = true;
      dj->gate = true;
      d1->gate = true;
      d2->gate = true;
      eliminator.gates.push_back (di);
      eliminator.gates.push_back (dj);
      eliminator.gates.push_back (d1);
      eliminator.gates.push_back (d2);
      stats.elimgates++;
      stats.elimites++;
      return;
    }
  }
}

/*------------------------------------------------------------------------*/

// Find and extract clause.

bool Internal::get_clause (Clause * c, vector<ILit> & l) {
  if (c->garbage) return false;
  l.clear ();
  for (const auto & lit : *c) {
    if (val (lit)) continue;
    l.push_back (lit);
  }
  return true;
}

// Check whether 'c' contains only the literals in 'l'.

bool Internal::is_clause (Clause * c, const vector<ILit> & lits) {
  if (c->garbage) return false;
  int size = lits.size ();
  if (c->size < size) return false;
  int found = 0;
  for (const auto & lit : *c) {
    if (val (lit)) continue;
    const auto it = find (lits.begin (), lits.end (), lit);
    if (it == lits.end ()) return false;
    if (++found > size) return false;
  }
  return found == size;
}

Clause * Internal::find_clause (const vector<ILit> & lits) {
  ILit best = 0;
  size_t len = 0;
  for (const auto & lit : lits) {
    size_t l = occs (lit).size ();
    if (i_val(best) && l >= len) continue;
    len = l, best = lit;
  }
  for (auto c : occs (best))
    if (is_clause (c, lits))
      return c;
  return 0;
}

void Internal::find_xor_gate (Eliminator & eliminator, ILit pivot) {

  if (!opts.elimxors) return;

  assert (opts.elimsubst);

  if (unsat) return;
  if (val (pivot)) return;
  if (!eliminator.gates.empty ()) return;

  vector<ILit> lits;

  for (auto d : occs (pivot)) {

    if (!get_clause (d, lits)) continue;

    const int size = lits.size ();      // clause size
    const int arity = size - 1;         // arity of XOR

    if (size < 3) continue;
    if (arity > opts.elimxorlim) continue;

    assert (eliminator.gates.empty ());

    unsigned needed = (1u << arity) - 1;        // additional clauses
    unsigned signs = 0;                         // literals to negate

    do {
      const unsigned prev = signs;
      while (parity (++signs))
        ;
      for (int j = 0; j < size; j++) {
        const unsigned bit = 1u << j;
        ILit lit = lits[j];
        if ((prev & bit) != (signs & bit))
          lits[j] = lit = -lit;
      }
      Clause * e = find_clause (lits);
      if (!e) break;
      eliminator.gates.push_back (e);
    } while (--needed);

    if (needed) { eliminator.gates.clear (); continue; }

    eliminator.gates.push_back (d);
    assert (eliminator.gates.size () == (1u << arity));

#ifdef LOGGING
    if (opts.log) {
      Logger::print_log_prefix (this);
      tout.magenta ();
      printf ("found arity %u XOR gate %d = ", arity, -pivot);
      bool first = true;
      for (const auto & lit : *d) {
        if (lit == pivot) continue;
        assert (lit != -pivot);
        if (!first) fputs (" ^ ", stdout);
        printf ("%d", lit);
        first = false;
      }
      fputc ('\n', stdout);
      tout.normal ();
      fflush (stdout);
    }
#endif
    stats.elimgates++;
    stats.elimxors++;
    const auto end = eliminator.gates.end ();
    auto j = eliminator.gates.begin ();
    for (auto i = j; i != end; i++) {
      Clause * e = *i;
      if (e->gate) continue;
      e->gate = true;
      LOG (e, "contributing");
      *j++ = e;
    }
    eliminator.gates.resize (j - eliminator.gates.begin ());

    break;
  }
}

/*------------------------------------------------------------------------*/

// Find a gate for 'pivot'.  If such a gate is found, the gate clauses are
// marked and pushed on the stack of gates.  Further hyper unary resolution
// might detect units, which are propagated.  This might assign the pivot or
// even produce the empty clause.

void Internal::find_gate_clauses (Eliminator & eliminator, ILit pivot)
{
  if (!opts.elimsubst) return;

  if (unsat) return;
  if (val (pivot)) return;

  assert (eliminator.gates.empty ());

  find_equivalence (eliminator, pivot);
  find_and_gate (eliminator, pivot);
  find_and_gate (eliminator, -pivot);
  find_if_then_else (eliminator, pivot);
  find_xor_gate (eliminator, pivot);
}

void Internal::unmark_gate_clauses (Eliminator & eliminator) {
  LOG ("unmarking %zd gate clauses", eliminator.gates.size ());
  for (const auto & c : eliminator.gates) {
    assert (c->gate);
    c->gate = false;
  }
  eliminator.gates.clear ();
}

/*------------------------------------------------------------------------*/

}
