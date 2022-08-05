#include "internal.hpp"
#include <sstream>

namespace CaDiCaL {

/*------------------------------------------------------------------------*/

Tracer::Tracer (Internal * i, File * f, bool b, bool l) :
  internal (i),
  file (f), binary (b), lrat (l),
  added (0), deleted (0)
{
  (void) internal;
  LOG ("TRACER new");
}

Tracer::~Tracer () {
  LOG ("TRACER delete");
  delete file;
}

/*------------------------------------------------------------------------*/

// Support for binary DRAT format.

inline void Tracer::put_binary_zero () {
  assert (binary);
  assert (file);
  file->put ((unsigned char) 0);
}

inline void Tracer::put_binary_lit (int lit) {
  assert (binary);
  assert (file);
  assert (lit != INT_MIN);
  put_binary_signed (lit);
}

inline void Tracer::put_binary_signed (int64_t n) {
  put_binary_unsigned (2*abs (n) + (n < 0));
}

inline void Tracer::put_binary_unsigned (int64_t n) {
  assert (binary);
  assert (file);
  assert (n > 0);
  unsigned char ch;
  while (n & ~0x7f) {
    ch = (n & 0x7f) | 0x80;
    file->put (ch);
    n >>= 7;
  }
  ch = n;
  file->put (ch);
}

/*------------------------------------------------------------------------*/

void Tracer::add_original_clause (clause_id_t id, const vector<ELit> & clause) {
  if (!lrat) return;
  if (file->closed ()) return;
  LOG ("TRACER tracing addition of original clause");
  if (binary) file->put ('o');
  else if (lrat) file->put ("o ");
  if (lrat) {
    if (binary) put_binary_unsigned (id);
    else file->put (id), file->put ("  ");
  }
  for (const auto & external_lit : clause)
    if (binary) put_binary_lit (e_val(external_lit));
    else file->put (e_val(external_lit)), file->put (' ');
  if (binary) put_binary_zero ();
  else file->put ("0\n");
}

void Tracer::add_derived_clause (clause_id_t id, const vector<int64_t> * chain, const vector<ELit> & clause, bool is_imported) {
  if (is_imported) { return; } //don't put imported clauses in proof file
  if (file->closed ()) return;
  LOG ("TRACER tracing addition of derived clause");
  if (binary) file->put ('a');
  else if (lrat) file->put ("a ");
  if (lrat) {
    if (binary) put_binary_unsigned (id);
    else file->put (id), file->put ("  ");
  }
  for (const auto & external_lit : clause)
    if (binary) put_binary_lit (e_val(external_lit));
    else file->put (e_val(external_lit)), file->put (' ');
  if (lrat && chain) {
    if (binary) put_binary_zero (), file->put ('l');
    else file->put ("0  l ");
    for (const auto & c : *chain)
      if (binary) put_binary_signed (c);
      else file->put (c), file->put (' ');
  }
  if (binary) put_binary_zero ();
  else file->put ("0\n");
  added++;
  //make sure the empty clause gets fully output here
  if (clause.size() == 0){
      flush();
  }
}

void Tracer::delete_clause (clause_id_t id, const vector<ELit> & clause) {
  if (file->closed ()) return;
  LOG ("TRACER tracing deletion of clause");
  if (binary) file->put ('d');
  else file->put ("d ");
  if (lrat) {
    if (binary) put_binary_unsigned (id);
    else file->put (id), file->put ("  ");
  }
  for (const auto & external_lit : clause)
    if (binary) put_binary_lit (e_val(external_lit));
    else file->put (e_val(external_lit)), file->put (' ');
  if (binary) put_binary_zero ();
  else file->put ("0\n");
  deleted++;
}

void Tracer::finalize_clause (clause_id_t id, const vector<ELit> & clause) {
  if (!lrat) return;
  if (file->closed ()) return;
  LOG ("TRACER tracing finalized clause");
  if (binary) file->put ('f');
  else file->put ("f ");
  if (binary) put_binary_unsigned (id);
  else file->put (id), file->put ("  ");
  for (const auto & external_lit : clause)
    if (binary) put_binary_lit (e_val(external_lit));
    else file->put (e_val(external_lit)), file->put (' ');
  if (binary) put_binary_zero ();
  else file->put ("0\n");
}

void Tracer::add_todo (const vector<int64_t> & vals) {
  if (!lrat) return;
  if (file->closed ()) return;
  ostringstream ss;
  for (auto c : vals) ss << " " << c;
  LOG ("TRACER tracing TODO%s", ss.str ().c_str ());
  if (binary) file->put ('t');
  else file->put ("t ");
  for (const auto & val : vals)
    if (binary) put_binary_unsigned (val);
    else file->put (val), file->put (' ');
  if (binary) put_binary_zero ();
  else file->put ("0\n");
}

/*------------------------------------------------------------------------*/

bool Tracer::closed () { return file->closed (); }

void Tracer::close () { assert (!closed ()); file->close (); }

void Tracer::flush () {
  assert (!closed ());
  file->flush ();
  MSG ("traced %" PRId64 " added and %" PRId64 " deleted clauses",
    added, deleted);
}

}
