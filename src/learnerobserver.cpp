#include "internal.hpp"
#include "external.hpp"
#include "learnerobserver.hpp"
#include <sstream>

namespace CaDiCaL {

    LearnerObserver::LearnerObserver (External *e) :
        external(e)
    {
        LOG ("LEARNEROBSERVER new");
    }

    LearnerObserver::~LearnerObserver (){
        LOG ("LEARNEROBSERVER delete");
    }

    void LearnerObserver::add_original_clause (clause_id_t, const vector<ELit> &){ }

    void LearnerObserver::add_derived_clause (clause_id_t id, const vector<clause_id_t> *,
                                              const vector<ELit> &lits, bool is_imported, int glue){
        if (is_imported){ //only export if not imported
            return;
        }
        if (!external->learner){ //only learn if a learner exists
            return;
        }
        if (glue == -1){
            throw std::invalid_argument("Invalid glue value:  -1");
        }
        if (lits.size() == 1){
            external->export_learned_unit_clause(id, lits[0]);
        }
        else if (lits.size() > 1){
            external->export_learned_large_clause(id, lits, glue);
        }
        //ignore empty clause
    }

    void LearnerObserver::delete_clause (clause_id_t, const vector<ELit> &){ }

    void LearnerObserver::finalize_clause (clause_id_t, const vector<ELit> &){ }

    void LearnerObserver::add_todo (const vector<int64_t> &){ }

    bool LearnerObserver::closed (){
        return !external->learner;
    }

    void LearnerObserver::close (){
        assert (!closed ());
    }

    void LearnerObserver::flush (){
        assert (!closed ());
    }

}

