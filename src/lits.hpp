#ifndef _lits_hpp_INCLUDED
#define _lits_hpp_INCLUDED

namespace CaDiCaL {

    //Internal literals
    struct ILit{
        int ilit;

        ILit(int i){
            ilit = i;
        }
        /*ILit(const ILit &i){
            ilit = i.ilit;
        }*/
    };

    //so we can switch out for integers more easily
    static inline int i_val(const ILit &i){ return i.ilit; }


    //External literals
    struct ELit{
        int elit;

        ELit(int e){
            elit = e;
        }
        /*ELit(const ELit &e){
            elit = e.elit;
        }*/
    };

    //so we can switch out for integers more easily
    static inline int e_val(const ELit &e){ return e.elit; }

}

#endif
