#ifndef NO_HEURISTIC_H_
#define NO_HEURISTIC_H_

#include "heuristic.h"

class TimeStampedState;

class NoHeuristic : public Heuristic
{
        enum {
            QUITE_A_LOT = 1000000
        };
    protected:
        virtual void initialize();
        virtual double compute_heuristic(const TimeStampedState &TimeStampedState);
    public:
        NoHeuristic() {}
        ~NoHeuristic() {}
        virtual bool dead_ends_are_reliable() {
            return true;
        }
};

#endif /*NO_HEURISTIC_H_*/
