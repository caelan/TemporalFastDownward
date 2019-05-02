#ifndef STATE_H
#define STATE_H

#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>
#include <set>
#include "globals.h"
using namespace std;

class Operator;
class TimeStampedState;

struct Prevail
{
    int var;
    double prev;
    Prevail(istream &in);
    Prevail(int v, double p) :
        var(v), prev(p)
    {
    }
    virtual ~Prevail()
    {
    }

    bool is_applicable(const TimeStampedState & state) const;

    void dump() const;

    bool operator<(const Prevail &other) const {
        if(var < other.var)
            return true;
        if(var > other.var)
            return false;
        return prev < other.prev;
    }
};

struct PrePost
{
    int var;
    double pre;
    int var_post;
    double post;
    vector<Prevail> cond_start;
    vector<Prevail> cond_overall;
    vector<Prevail> cond_end;
    assignment_op fop;

    PrePost()
    {
    } // Needed for axiom file-reading constructor, unfortunately.
    PrePost(std::istream &in);
    PrePost(int v, double pr, int vpo, double po, const std::vector<Prevail> &co_start,
            const std::vector<Prevail> &co_oa,
            const std::vector<Prevail> &co_end, assignment_op fo = assign) :
        var(v), pre(pr), var_post(vpo), post(po),
        cond_start(co_start), cond_overall(co_oa), cond_end(co_end), fop(fo)
    {
    }
    PrePost(int v, double p) : var(v), post(p) {}
    virtual ~PrePost() {}

    bool is_applicable(const TimeStampedState &state) const;

    bool does_fire(const TimeStampedState &state) const {
        for(unsigned int i = 0; i < cond_start.size(); i++)
            if(!cond_start[i].is_applicable(state))
                return false;
        return true;
    }

    void dump() const;
};

struct ScheduledEffect : public PrePost
{
    double time_increment;
    ScheduledEffect(double t, vector<Prevail> &cas, vector<Prevail> &coa, vector<Prevail> &cae,
        int va, int vi, assignment_op op) :
        PrePost(va, -1.0, vi, -1.0, cas, coa, cae, op), time_increment(t)
    {
        initialize();
    }
    ScheduledEffect(double t, const PrePost& pp) : PrePost(pp), time_increment(t)
    {
        initialize();
    }
    void initialize()
    {
        sort(cond_start.begin(), cond_start.end());
        sort(cond_overall.begin(), cond_overall.end());
        sort(cond_end.begin(), cond_end.end());
    }
    void dump() const {
        cout << time_increment << ": ";
        PrePost::dump();
    }
    bool operator<(const ScheduledEffect &other) const
    {
        if(time_increment < other.time_increment)
            return true;
        if(time_increment > other.time_increment)
            return false;
        if(var < other.var)
            return true;
        if(var > other.var)
            return false;
        if(pre < other.pre)
            return true;
        if(pre > other.pre)
            return false;
        if(var_post < other.var_post)
            return true;
        if(var_post > other.var_post)
            return false;
        if(post < other.post)
            return true;
        if(post > other.post)
            return false;
        if(fop < other.fop)
            return true;
        if(fop > other.fop)
            return false;
        if(cond_start.size() < other.cond_start.size())
            return true;
        if(cond_start.size() > other.cond_start.size())
            return false;
        if(cond_overall.size() < other.cond_overall.size())
            return true;
        if(cond_overall.size() > other.cond_overall.size())
            return false;
        if(cond_end.size() < other.cond_end.size())
            return true;
        if(cond_end.size() > other.cond_end.size())
            return false;
        if(lexicographical_compare(cond_start.begin(), cond_start.end(),
                    other.cond_start.begin(), other.cond_start.end()))
            return true;
        if(lexicographical_compare(other.cond_start.begin(), other.cond_start.end(),
            cond_start.begin(), cond_start.end()))
            return false;
        if(lexicographical_compare(cond_overall.begin(), cond_overall.end(),
            other.cond_overall.begin(), other.cond_overall.end()))
            return true;
        if(lexicographical_compare(other.cond_overall.begin(), other.cond_overall.end(),
            cond_overall.begin(), cond_overall.end()))
            return false;
        if(lexicographical_compare(cond_end.begin(), cond_end.end(),
                    other.cond_end.begin(), other.cond_end.end()))
            return true;
        if(lexicographical_compare(other.cond_end.begin(), other.cond_end.end(),
            cond_end.begin(), cond_end.end()))
            return false;
        return false;
    }
};

struct ScheduledCondition : public Prevail
{
    double time_increment;
    ScheduledCondition(double t, int v, double p) :
        Prevail(v, p), time_increment(t)
    {
    }
    ScheduledCondition(double t, const Prevail &prev) : Prevail(prev), time_increment(t)
    {
    }
    bool operator<(const ScheduledCondition &other) const
    {
        if(time_increment < other.time_increment)
            return true;
        if(time_increment > other.time_increment)
            return false;
        if(var < other.var)
            return true;
        if(var > other.var)
            return false;
        return prev < other.prev;
    }
};

class ScheduledOperator;

typedef std::pair<std::vector<double>, double> TimedSymbolicState;
typedef std::vector<TimedSymbolicState> TimedSymbolicStates;

class TimeStampedState
{
    friend class RelaxedState;
    friend class AxiomEvaluator;
    friend struct PrePost;
    friend struct Prevail;
    friend class Operator;
    friend class NoHeuristic;
    friend class CyclicCGHeuristic;
    friend class ConsistencyCache;
    friend struct TssCompareIgnoreTimestamp;

    private:
        bool satisfies(const Prevail& cond) const
        {
            return cond.is_applicable(*this);
        }

        bool satisfies(const vector<Prevail>& conds) const
        {
            for(unsigned int i = 0; i < conds.size(); i++)
                if(!satisfies(conds[i]))
                    return false;
            return true;
        }

        bool satisfies(const pair<int, double>& goal) const
        {
            return double_equals(state[goal.first], goal.second);
        }

        void apply_numeric_effect(int lhs, assignment_op op, int rhs)
        {
            switch(op) {
                case assign:
                    state[lhs] = state[rhs];
                    break;
                case scale_up:
                    state[lhs] *= state[rhs];
                    break;
                case scale_down:
                    state[lhs] /= state[rhs];
                    break;
                case increase:
                    state[lhs] += state[rhs];
                    break;
                case decrease:
                    state[lhs] -= state[rhs];
                    break;
                default:
                    assert(false);
                    break;
            }
        }

        void apply_discrete_effect(int var, double post)
        {
            state[var] = post;
        }

        void apply_effect(int lhs, assignment_op op, int rhs, double post)
        {
            if(is_functional(lhs)) {
                apply_numeric_effect(lhs, op, rhs);
            } else {
                apply_discrete_effect(lhs, post);
            }
        }

        void initialize()
        {
            sort(scheduled_effects.begin(), scheduled_effects.end());
            sort(conds_over_all.begin(), conds_over_all.end());
            sort(conds_at_end.begin(), conds_at_end.end());
        }

    public:
        vector<double> state;
        vector<ScheduledEffect> scheduled_effects;
        vector<ScheduledCondition> conds_over_all;
        vector<ScheduledCondition> conds_at_end;

        double timestamp;
        vector<ScheduledOperator> operators;

        int numberOfEpsInsertions;

        TimeStampedState(istream &in);
        // clone a state
        TimeStampedState(const TimeStampedState &other);
        // apply an operator
        TimeStampedState(const TimeStampedState &predecessor, const Operator &op);
        // let time pass without applying an operator
        TimeStampedState let_time_pass(
            bool go_to_intermediate_between_now_and_next_happening = false,
            bool skip_eps_steps = false) const;
        int getNumberOfEpsTimeSteps(double offset) const;

        double &operator[](int index)
        {
            return state[index];
        }
        double operator[](int index) const
        {
            return state[index];
        }
        void dump(bool verbose) const;

        void scheduleEffect(ScheduledEffect effect);

        double next_happening() const;

        bool is_consistent_now() const;
        bool is_consistent_when_progressed(TimedSymbolicStates* timedSymbolicStates = NULL) const;

        const double &get_timestamp() const
        {
            return timestamp;
        }

        bool satisfies(const vector<pair<int, double> >& goal) const
        {
            for(unsigned int i = 0; i < goal.size(); i++)
                if(!satisfies(goal[i]))
                    return false;
            return true;
        }
};

TimeStampedState &buildTestState(TimeStampedState &state);

#endif
