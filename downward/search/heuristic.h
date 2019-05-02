#ifndef HEURISTIC_H
#define HEURISTIC_H

#include <map>
#include <vector>

#include "globals.h"

class Operator;
class TimeStampedState;

class Heuristic
{
    enum
    {
        NOT_INITIALIZED = -2
    };
    double heuristic;
    double waiting_time;
    
    struct EvaluationInfo
    {
        EvaluationInfo() {
            heuristic = NOT_INITIALIZED;
        }
    EvaluationInfo(double heur, const std::vector<const Operator *> &pref) :
        heuristic(heur), preferred_operators(pref)
        {
        }
        double heuristic;
        std::vector<const Operator *> preferred_operators;
    };
    
    
    unsigned long num_computations;     ///< For stats, how often was compute_heuristic called
    
 protected:
    unsigned long num_cache_hits;     ///< For stats, how often was compute_heuristic called again for a same state
    enum
    {
        DEAD_END = -1
    };
    virtual void initialize()
    {
    }
    virtual double compute_heuristic(const TimeStampedState
                                     &state) = 0;
    void clearPreferredOperators();
 public:
    std::vector<const Operator *> preferred_operators_regular;
    std::vector<const Operator *> preferred_operators_ordered;
    std::vector<const Operator *> preferred_operators_cheapest;
    std::vector<const Operator *> preferred_operators_most_expensive;
    std::vector<const Operator *> preferred_operators_rand;
    std::vector<const Operator *> preferred_operators_concurrent;
    inline void set_waiting_time(double time_increment);
    void set_preferred(const Operator *op, OpenListMode mode);
    Heuristic();
    virtual ~Heuristic();
    
    double evaluate(const TimeStampedState &state);
    bool is_dead_end();
    double get_heuristic();
    void get_preferred_operators(std::vector<const Operator *> &result, OpenListMode mode);
    inline double get_waiting_time();
    virtual bool dead_ends_are_reliable()
    {
        return true;
    }
    unsigned long get_num_computations() const { return num_computations; }
    unsigned long get_num_cache_hits() const { return num_cache_hits; }
};

inline void Heuristic::set_waiting_time(double time_increment)
{
    waiting_time = time_increment;
}

inline double Heuristic::get_waiting_time()
{
    return waiting_time;
}

#endif
