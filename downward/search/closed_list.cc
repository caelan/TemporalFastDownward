#include "closed_list.h"

// #include "state.h"

#include <algorithm>
#include <cassert>
using namespace std;

/**
   Map-based implementation of a closed list.

   The closed list has two purposes:
   1. It stores which nodes have been expanded or scheduled to expand
   already to avoid duplicates (i.e., it is used like a set).
   These states "live" in the closed list -- in particular,
   permanently valid pointers to these states are obtained upon
   insertion.
   2. It can trace back a path from the initial state to a given state
   in the list.

   The datatypes used for the closed list could easily be
   parameterized, but there is no such need presently.
*/

ClosedList::ClosedList()
{
}

ClosedList::~ClosedList()
{
}

std::size_t TssHash::operator()(const TimeStampedState &tss) const
{
    std::size_t ret = 0;
    for(int i = 0; i < tss.state.size(); ++i) {
        ret += tss.state[i] * (i + 1);
    }
    ret += tss.scheduled_effects.size() * (tss.state.size() + 1);
    ret += tss.conds_over_all.size() * (tss.state.size() + 2);
    ret += tss.conds_at_end.size() * (tss.state.size() + 3);

    return ret;
}

bool prevailEquals(const Prevail &prev1, const Prevail &prev2)
{
    if(prev1.var != prev2.var)
        return false;
    if(!double_equals(prev1.prev, prev2.prev))
        return false;
    return true;
}

bool scheduledConditionEquals(const ScheduledCondition &cond1, const ScheduledCondition &cond2)
{
    if(!double_equals(cond1.time_increment, cond2.time_increment))
        return false;
    if(cond1.var != cond2.var)
        return false;
    if(!double_equals(cond1.prev, cond2.prev))
        return false;
    return true;
}

bool scheduledEffectEquals(const ScheduledEffect &eff1, const ScheduledEffect &eff2)
{
    if(!double_equals(eff1.time_increment, eff2.time_increment))
        return false;
    if(eff1.var != eff2.var)
        return false;
    if(!double_equals(eff1.pre, eff2.pre))
        return false;
    if(eff1.var_post != eff2.var_post)
        return false;
    if(!double_equals(eff1.post, eff2.post))
        return false;
    if(eff1.fop != eff2.fop)
        return false;
    if(eff1.cond_start.size() != eff2.cond_start.size())
        return false;
    if(eff1.cond_overall.size() != eff2.cond_overall.size())
        return false;
    if(eff1.cond_end.size() != eff2.cond_end.size())
        return false;
    if(!equal(eff1.cond_start.begin(), eff1.cond_start.end(),
                eff2.cond_start.begin(), prevailEquals))
        return false;
    if(!equal(eff1.cond_overall.begin(), eff1.cond_overall.end(),
                eff2.cond_overall.begin(), prevailEquals))
        return false;
    if(!equal(eff1.cond_end.begin(), eff1.cond_end.end(),
                eff2.cond_end.begin(), prevailEquals))
        return false;
    return true;
}

bool TssEquals::operator()(const TimeStampedState &tss1, const TimeStampedState &tss2) const
{
    assert(tss1.state.size() == tss2.state.size());
    if(tss1.scheduled_effects.size() != tss2.scheduled_effects.size())
        return false;
    if(tss1.conds_over_all.size() != tss2.conds_over_all.size())
        return false;
    if(tss1.conds_at_end.size() != tss2.conds_at_end.size())
        return false;
    if(!equal(tss1.scheduled_effects.begin(), tss1.scheduled_effects.end(),
                tss2.scheduled_effects.begin(), scheduledEffectEquals))
        return false;
    if(!equal(tss1.conds_over_all.begin(), tss1.conds_over_all.end(),
                tss2.conds_over_all.begin(), scheduledConditionEquals))
        return false;
    if(!equal(tss1.conds_at_end.begin(), tss1.conds_at_end.end(),
                tss2.conds_at_end.begin(), scheduledConditionEquals))
        return false;

    for(int i = 0; i < tss1.state.size(); ++i) {
        if (!(g_variable_types[i] == primitive_functional
                    || g_variable_types[i] == logical)) {
            continue;
        }

        if (!double_equals(tss1.state[i], tss2.state[i]))
            return false;
    }
    return true;
}

//bool ClosedList::TssCompareIgnoreTimestamps::operator()(const TimeStampedState &tss1, const TimeStampedState &tss2) const {
//    if(tss1.timestamp < tss2.timestamp)
//	return true;
//    if(tss1.timestamp > tss2.timestamp)
//	return false;
//    if(lexicographical_compare(tss1.state.begin(), tss1.state.end(),
//	    tss2.state.begin(), tss2.state.end()))
//	return true;
//    if(lexicographical_compare(tss2.state.begin(), tss2.state.end(),
//	    tss1.state.begin(), tss1.state.end()))
//	return false;
//    if(tss1.scheduled_effects.size() < tss2.scheduled_effects.size())
//	return true;
//    if(tss1.scheduled_effects.size() > tss2.scheduled_effects.size())
//	return false;
//    if(tss1.conds_over_all.size() < tss2.conds_over_all.size())
//	return true;
//    if(tss1.conds_over_all.size() > tss2.conds_over_all.size())
//	return false;
//    if(tss1.conds_at_end.size() < tss2.conds_at_end.size())
//	return true;
//    if(tss1.conds_at_end.size() > tss2.conds_at_end.size())
//	return false;
//    if(lexicographical_compare(tss1.scheduled_effects.begin(),
//	    tss1.scheduled_effects.end(), tss2.scheduled_effects.begin(),
//	    tss2.scheduled_effects.end()))
//	return true;
//    if(lexicographical_compare(tss2.scheduled_effects.begin(),
//	    tss2.scheduled_effects.end(), tss1.scheduled_effects.begin(),
//	    tss1.scheduled_effects.end()))
//	return false;
//    if(lexicographical_compare(tss1.conds_over_all.begin(),
//	    tss1.conds_over_all.end(), tss2.conds_over_all.begin(),
//	    tss2.conds_over_all.end()))
//	return true;
//    if(lexicographical_compare(tss2.conds_over_all.begin(),
//	    tss2.conds_over_all.end(), tss1.conds_over_all.begin(),
//	    tss1.conds_over_all.end()))
//	return false;
//    if(lexicographical_compare(tss1.conds_at_end.begin(),
//	    tss1.conds_at_end.end(), tss2.conds_at_end.begin(),
//	    tss2.conds_at_end.end()))
//	return true;
//    if(lexicographical_compare(tss2.conds_at_end.begin(),
//	    tss2.conds_at_end.end(), tss1.conds_at_end.begin(),
//	    tss1.conds_at_end.end()))
//	return false;
//    return false;
//}


const TimeStampedState *ClosedList::insert(
        TimeStampedState &entry, const TimeStampedState *predecessor,
        const Operator *annotation)
{
    ClosedListMap::iterator ret =
        closed.insert(ValuePair(entry, PredecessorInfo(predecessor, annotation)));
    //    assert(ret.second);
    return &ret->first;
}

void ClosedList::clear()
{
    closed.clear();
}

bool ClosedList::contains(const TimeStampedState &entry) const
{
    double min_so_far = get_min_ts_of_key(entry);
    double diff = entry.timestamp - min_so_far;
    return (!(diff + EPSILON < 0));
}

const TimeStampedState& ClosedList::get(const TimeStampedState &state) const
{
    std::pair<ClosedListMap::const_iterator, ClosedListMap::const_iterator>
        entries = closed.equal_range(state);
    const TimeStampedState *ret = &(closed.find(state)->first);
    ClosedListMap::const_iterator it = entries.first;
    for (; it != entries.second; ++it) {
        if (it->first.timestamp + EPSILON < ret->timestamp) {
            ret = &(it->first);
        }
    }
    return *ret;
}

double ClosedList::get_min_ts_of_key(const TimeStampedState &state) const
{
    double ret = REALLYBIG;
    std::pair<ClosedListMap::const_iterator, ClosedListMap::const_iterator>
        entries = closed.equal_range(state);
    ClosedListMap::const_iterator it = entries.first;
    for(; it != entries.second; ++it) {
        ret = min(ret, it->first.timestamp);
    }
    return ret;
}

int ClosedList::size() const
{
    return closed.size();
}

double ClosedList::getCostOfPath(const TimeStampedState &entry) const
{
    double ret = 0.0;
    TimeStampedState current_entry = entry;
    for (;;) {
        double min_timestamp = current_entry.timestamp;
        std::pair<ClosedListMap::const_iterator, ClosedListMap::const_iterator>
            entries = closed.equal_range(current_entry);
        ClosedListMap::const_iterator it = entries.first;
        const PredecessorInfo* info_helper = NULL;
        for (; it != entries.second; ++it) {
            if (it->first.timestamp + EPSILON < min_timestamp || !info_helper) {
                info_helper = &(it->second);
                min_timestamp = it->first.timestamp;
            }
        }
        if (!info_helper || info_helper->predecessor == 0)
            break;
        const PredecessorInfo &info = *info_helper;
        const TimeStampedState* pred = info.predecessor;
        if (info.annotation != g_let_time_pass
                && info.annotation->get_name().compare("wait")) {
            const Operator* op = info.annotation;

            double duration = op->get_duration(pred);
            ret += duration;
        }
        current_entry = *(info.predecessor);
    }
    return ret;
}

double ClosedList::trace_path(const TimeStampedState &entry,
        vector<PlanStep> &path, PlanTrace &states) const
{
    double ret = 0.0;
    assert(path.empty());
    TimeStampedState current_entry = entry;
    states.push_back(new TimeStampedState(entry));
    for(;;) {
        double min_timestamp = current_entry.timestamp;
        double timestamp = min_timestamp;
        std::pair<ClosedListMap::const_iterator, ClosedListMap::const_iterator>
            entries = closed.equal_range(current_entry);
        ClosedListMap::const_iterator it = entries.first;
        const PredecessorInfo* info_helper = NULL;
        for(; it != entries.second; ++it) {
            if(it->first.timestamp + EPSILON < min_timestamp || !info_helper) {
                info_helper = &(it->second);
                min_timestamp = it->first.timestamp;
            }
        }
        double diff = timestamp - min_timestamp;
        if(!info_helper || info_helper->predecessor == 0)
            break;
        const PredecessorInfo &info = *info_helper;
        if(diff > EPSILON && states.size() > 1) {
            for(int i = 0; i < path.size(); i++) {
                path[i].start_time -= diff;
            }
            for(int i = 0; i < states.size(); i++) {
                states[i]->timestamp -= diff;
            }
        }
        const TimeStampedState* pred = info.predecessor;
        if(info.annotation != g_let_time_pass
                && info.annotation->get_name().compare("wait")) {
            const Operator* op = info.annotation;
            double duration = op->get_duration(pred);
            path.push_back(PlanStep(pred->get_timestamp(), duration, op, pred));
            ret += duration;
        }
        states.push_back(new TimeStampedState(*pred));
        current_entry = *pred;
    }
    reverse(path.begin(), path.end());
    reverse(states.begin(), states.end());
    return ret;
}

double getSumOfSubgoals(const vector<PlanStep> &plan)
{
    double ret = 0.0;
    for (int i = 0; i < plan.size(); ++i) {
        ret += plan[i].duration;
    }
    return ret;
}

double getSumOfSubgoals(const PlanTrace &path)
{
    double ret = 0.0;
    for (int i = 0; i < g_goal.size(); ++i) {
        assert(g_variable_types[g_goal[i].first] == logical || g_variable_types[g_goal[i].first] == comparison);
        //        cout << "Goal " << i << ": " << g_variable_name[g_goal[i].first] << " has to be " << g_goal[i].second << endl;
        double actualIncrement = 0.0;
        for (int j = path.size() - 1; j >= 0; --j) {
            //            cout << "At timstamp " << path[j]->timestamp << " it is " << path[j]->state[g_goal[i].first] << endl;
            if (double_equals(path[j]->state[g_goal[i].first], g_goal[i].second)) {
                actualIncrement = path[j]->timestamp;
            } else {
                //                cout << "Goal " << i << " is satiesfied at timestamp " << actualIncrement << endl;
                break;
            }
        }
        ret += actualIncrement;
    }
    return ret;
}

