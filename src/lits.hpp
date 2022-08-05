#ifndef _lits_hpp_INCLUDED
#define _lits_hpp_INCLUDED

namespace CaDiCaL {

    //Internal literals
    struct ILit{
        int ilit;

        ILit(int i = 0){
            ilit = i;
        }

        friend bool operator== (const ILit & lhs, const ILit & rhs);
        friend bool operator!= (const ILit & lhs, const ILit & rhs);
        friend ILit operator- (const ILit &i);
        ILit operator++ (int);
    };

    //so we can switch out for integers more easily
    static inline int i_val(const ILit &i){ return i.ilit; }


    //External literals
    struct ELit{
        int elit;

        ELit(int e = 0){
            elit = e;
        }

        friend bool operator== (const ELit & lhs, const ELit & rhs );
        friend bool operator!= (const ELit & lhs, const ELit & rhs );
        friend ELit operator- (const ELit &e);
    };

    //so we can switch out for integers more easily
    static inline int e_val(const ELit &e){ return e.elit; }

}

#endif
