#include "internal.hpp"
#include <sstream>

namespace CaDiCaL {

/*------------------------------------------------------------------------*/

Tracer::Tracer (Internal * i, File * f, bool b, bool l, bool fr, bool d) :
  internal (i),
  file (f), binary (b), lrat (l), frat (fr), should_delete_clauses(d),
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

void Tracer::add_original_clause (clause_id_t id, const vector<int> & clause) {
  if (!frat) return; //only FRAT files contain original clauses
  if (file->closed ()) return;
  LOG ("TRACER tracing addition of original clause");
  //output o
  if (binary) file->put ('o');
  else file->put ("o ");
  //output id
  if (binary) put_binary_unsigned (id);
  else file->put (id), file->put ("  ");
  //output literals
  for (const auto & external_lit : clause)
    if (binary) put_binary_lit (external_lit);
    else file->put (external_lit), file->put (' ');
  //output end line
  if (binary) put_binary_zero ();
  else file->put ("0\n");
}

void Tracer::add_derived_clause (clause_id_t id, const vector<int64_t> * chain, const vector<int> & clause, bool is_imported) {
  LOG("At Tracer::add_derived_clause");
  if (is_imported) { return; } //don't put imported clauses in proof file
  if (file->closed ()) return;

  if (!chain) {
     LOG("TRACER: add_derived_clause: no chain; skipping");
     return;
  }

  LOG ("TRACER tracing addition of derived clause");
  //only FRAT files start lines with a
  if ((lrat || frat) && binary) file->put ('a');
  else if (frat) file->put ("a ");
  //clause ID for FRAT or LRAT files
  if (binary){
      if (lrat){
          put_binary_signed(id);
      }
      else if (frat){
          put_binary_unsigned(id);
      }
  }
  else if (lrat || frat) {
      file->put (id), file->put (" ");
  }
  //output literals for anything
  for (const auto & external_lit : clause){
    if (binary) put_binary_lit (external_lit);
    else file->put (external_lit), file->put (' ');
  }
  //check if we have a chain, which is required for LRAT
  if (lrat && !chain){ //error if no proof for LRAT
      throw std::invalid_argument("Tracer for LRAT file add_derived_clauses must have a non-null chain");
  }
  //output the proof for FRAT or LRAT files
  if ((frat || lrat) && chain) {
      //output end of literals before proof
      if (binary) put_binary_zero();
      else file->put("0 ");
      //output FRAT proof hint marker
      if (frat){
          if (binary) file->put ('l');
          else file->put ("  l ");
      }
    for (const auto & c : *chain){
      if (binary) put_binary_signed (c);
      else file->put (c), file->put (' ');
    }
  }
  //end line:  0 ends proof (LRAT/FRAT) or literals (DRAT)
  if (binary) put_binary_zero ();
  else file->put ("0\n");
  added++;
  //make sure the empty clause gets fully output here
  if (clause.size() == 0){
      flush();
  }
  LOG("Ending Tracer::add_derived_clause");
  
}

void Tracer::delete_clause (clause_id_t id, const vector<int> & clause) {
  if (!should_delete_clauses) return;
  if (file->closed ()) return;
  LOG ("TRACER tracing deletion of clause");
  //output a leading, ignored clause ID for LRAT
  if (lrat && !binary){
      file->put(id), file->put(" ");
  }
  //output the delete d for any format
  if (binary) file->put ('d');
  else file->put ("d ");
  //output clause ID being deleted for LRAT or FRAT
  if (binary){
      if (lrat) put_binary_signed(id);
      else if (frat) put_binary_unsigned(id);
  }
  else if (lrat || frat) {
    file->put (id), file->put (" ");
  }
  //output literals for FRAT or DRAT
  if (frat || !lrat){
      for (const auto & external_lit : clause)
          if (binary) put_binary_lit (external_lit);
          else file->put (external_lit), file->put (' ');
  }
  //end the line with a zero
  if (binary) put_binary_zero ();
  else file->put ("0\n");
  deleted++;
}

void Tracer::finalize_clause (clause_id_t id, const vector<int> & clause) {
  if (!frat) return; //only FRAT files contain finalize clauses
  if (file->closed ()) return;
  LOG ("TRACER tracing finalized clause");
  if (binary) file->put ('f');
  else file->put ("f ");
  if (binary) put_binary_unsigned (id);
  else file->put (id), file->put ("  ");
  for (const auto & external_lit : clause)
    if (binary) put_binary_lit (external_lit);
    else file->put (external_lit), file->put (' ');
  if (binary) put_binary_zero ();
  else file->put ("0\n");
}

void Tracer::add_todo (const vector<int64_t> & vals) {
  if (!frat && !lrat) return; //only FRAT files contain todo lines
  if (file->closed ()) return;
  //LRAT doesn't have todo lines, but it does have comments, so we can put todos there
  if (lrat && !frat && !binary){
      file->put("c ");
  }
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
