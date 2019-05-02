#ifndef CLOSED_LIST_H
#define CLOSED_LIST_H

// #include <map>
#include <tr1/unordered_map>
#include <algorithm>
#include <vector>

#include "operator.h"

struct TssHash
{
    std::size_t operator()(const TimeStampedState & tss) const;
};

bool prevailEquals(const Prevail &prev1, const Prevail &prev2);

bool scheduledConditionEquals(const ScheduledCondition &cond1, const ScheduledCondition &cond2);

bool scheduledEffectEquals(const ScheduledEffect &eff1, const ScheduledEffect &eff2);

struct TssEquals
{
    bool operator()(const TimeStampedState &tss1, const TimeStampedState &tss2) const;
};

class ClosedList
{
    struct PredecessorInfo
    {
        const TimeStampedState *predecessor;
        const Operator *annotation;
        PredecessorInfo(const TimeStampedState *pred,
            const Operator *annote) :
                predecessor(pred), annotation(annote)
        {
        }
    };

    typedef tr1::unordered_multimap<TimeStampedState, PredecessorInfo, TssHash, TssEquals> ClosedListMap;
    typedef ClosedListMap::value_type ValuePair;
    ClosedListMap closed;

    public:
        ClosedList();
        ~ClosedList();
        const TimeStampedState *insert(TimeStampedState &entry,
                const TimeStampedState *predecessor,
                const Operator *annotation);
        void clear();

        bool contains(const TimeStampedState &entry) const;

        const TimeStampedState& get(const TimeStampedState &state) const;

        double get_min_ts_of_key(const TimeStampedState &state) const;

        int size() const;
        double trace_path(const TimeStampedState &entry, std::vector<PlanStep> &path, PlanTrace &staes) const;
        double getCostOfPath(const TimeStampedState &entry) const;
};

double getSumOfSubgoals(const vector<PlanStep> &plan);
double getSumOfSubgoals(const PlanTrace &path);

#endif
