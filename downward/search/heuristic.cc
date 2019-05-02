#include "heuristic.h"
#include "operator.h"
#include "plannerParameters.h"

using namespace std;

Heuristic::Heuristic()
{
    heuristic = NOT_INITIALIZED;
    num_computations = 0;
    num_cache_hits = 0;
}

Heuristic::~Heuristic()
{
}

void Heuristic::set_preferred(const Operator *newOp, OpenListMode mode)
{
    std::vector<const Operator *>* pref_ops = &preferred_operators_regular;
    switch (mode) {
    case REGULAR:
        break;
    case ORDERED:
        pref_ops = &preferred_operators_ordered;
        break;
    case CHEAPEST:
        pref_ops = &preferred_operators_cheapest;
        break;
    case MOSTEXPENSIVE:
        pref_ops = &preferred_operators_most_expensive;
        break;
    case RAND:
        pref_ops = &preferred_operators_rand;
        break;
    case CONCURRENT:
        pref_ops = &preferred_operators_concurrent;
        break;
    default:
        assert(false);
        break;
    }
    if(newOp->get_name().compare("wait") == 0) {
        const ScheduledOperator *s_newOp = dynamic_cast<const ScheduledOperator*>(newOp);
        assert(s_newOp);
        set_waiting_time(min(waiting_time, s_newOp->time_increment));
        return;
    }
    for(int i = 0; i < pref_ops->size(); i++) {
    	const Operator* prefOp = (*pref_ops)[i];
        if(newOp == prefOp) {
            return;
        }
    }
    pref_ops->push_back(newOp);
}

void Heuristic::clearPreferredOperators()
{
    preferred_operators_regular.clear();
    preferred_operators_ordered.clear();
    preferred_operators_cheapest.clear();
    preferred_operators_most_expensive.clear();
    preferred_operators_rand.clear();
    preferred_operators_concurrent.clear();
}

double Heuristic::evaluate(const TimeStampedState &state)
{
    
    if(heuristic == NOT_INITIALIZED)
        initialize();
    
    clearPreferredOperators();
    heuristic = compute_heuristic(state);
    num_computations++;
    assert(heuristic == DEAD_END || heuristic >= 0);
    
    if(heuristic == DEAD_END) {
        // It is ok to have preferred operators in dead-end states.
        // This allows a heuristic to mark preferred operators on-the-fly,
        // selecting the first ones before it is clear that all goals
        // can be reached.
        clearPreferredOperators();
    }
    
    
#ifndef NDEBUG
    if(heuristic != DEAD_END) {
        //	cout << "preferred operators:" << endl;
        //	for(int i = 0; i < preferred_operators.size(); i++) {
        //	    //assert(preferred_operators[i]->is_applicable(state));
        //	    cout << "  " << preferred_operators[i]->get_name() << endl;
        //	}
    }
#endif
    return heuristic;
}

bool Heuristic::is_dead_end()
{
    return heuristic == DEAD_END;
}

double Heuristic::get_heuristic()
{
    // The -1 value for dead ends is an implementation detail which is
    // not supposed to leak. Thus, calling this for dead ends is an
    // error. Call "is_dead_end()" first.
    assert(heuristic >= 0);
    return heuristic;
}

void Heuristic::get_preferred_operators(std::vector<const Operator *> &result, OpenListMode mode)
{
    assert(heuristic >= 0);
    switch (mode) {
    case REGULAR:
        result.insert(result.end(), preferred_operators_regular.begin(), preferred_operators_regular.end());
        break;
    case ORDERED:
        result.insert(result.end(), preferred_operators_ordered.begin(), preferred_operators_ordered.end());
        break;
    case CHEAPEST:
        result.insert(result.end(), preferred_operators_cheapest.begin(), preferred_operators_cheapest.end());
        break;
    case MOSTEXPENSIVE:
        result.insert(result.end(), preferred_operators_most_expensive.begin(), preferred_operators_most_expensive.end());
        break;
    case RAND:
        result.insert(result.end(), preferred_operators_rand.begin(), preferred_operators_rand.end());
        break;
    case CONCURRENT:
        result.insert(result.end(), preferred_operators_concurrent.begin(), preferred_operators_concurrent.end());
        break;
    default:
        break;
    }
}
