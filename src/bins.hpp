#ifndef _bins_hpp_INCLUDED
#define _bins_hpp_INCLUDED

#include "util.hpp"     // Alphabetically after 'bins'.
#include "lits.hpp"

namespace CaDiCaL {

using namespace std;

struct Bin {
  ILit lit;
  clause_id_t clause_id;
};

typedef vector<Bin> Bins;

inline void shrink_bins (Bins & bs) { shrink_vector (bs); }
inline void erase_bins (Bins & bs) { erase_vector (bs); }

}

#endif
