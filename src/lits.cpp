
#include "lits.hpp"

namespace CaDiCaL{
    bool operator== (const ILit & lhs, const ILit & rhs){ return lhs.ilit == rhs.ilit; }
    bool operator!= (const ILit & lhs, const ILit & rhs){ return !(lhs == rhs); }
    ILit operator- (const ILit &i){ return ILit(-i.ilit); }

    ILit ILit::operator++(int){
        ilit++;
        return *this;
    }

    bool operator== (const ELit & lhs, const ELit & rhs ){ return lhs.elit == rhs.elit; }
    bool operator!= (const ELit & lhs, const ELit & rhs){ return !(lhs == rhs); }
    ELit operator- (const ELit &e){ return ELit(-e.elit); }
}
