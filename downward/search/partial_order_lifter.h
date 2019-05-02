#ifndef PARTIAL_ORDER_LIFTER_H
#define PARTIAL_ORDER_LIFTER_H

#include "state.h"
#include "operator.h"
#include "scheduler.h"
#include <list>


enum ActionType {
    start_action = 0, end_action = 1, dummy_start_action = 2, dummy_end_action = 3
};

struct InstantPlanStep {

    ActionType type;
    int endAction;
    int actionFinishingImmeadatlyAfterThis;
    double timepoint;
    double duration;
    const Operator* op;
    string name;

    int correspondingPlanStep;

    std::set<int> precondition_vars;
    std::set<int> effect_vars;

    std::vector<Prevail> preconditions;
    std::set<int> effect_cond_vars;
    std::vector<PrePost> effects;
    std::vector<Prevail> overall_conds;

    InstantPlanStep(ActionType _type, double _timepoint, double _duration, const Operator* _op) :
        type(_type), timepoint(_timepoint), duration(_duration), op(_op) {
        endAction = -1;
        actionFinishingImmeadatlyAfterThis = -1;
        name = type + "." + op->get_name();
    }

    bool operator<(const InstantPlanStep &other) const {
        return timepoint < other.timepoint;
    }

    void dump();
    void print_name();

};

typedef std::vector<InstantPlanStep> InstantPlan;

typedef std::pair<int, int> Ordering;

class PartialOrderLifter {
    struct doubleEquComp {
      bool operator() (const double& lhs, const double& rhs) const
      {return lhs+EPSILON<rhs;}
    };
private:
    Plan plan;
    const PlanTrace trace;

    InstantPlan instant_plan;
    std::set<Ordering> partial_order;

    void findTriggeringEffects(const TimeStampedState* stateBeforeHappening,
            const TimeStampedState* stateAfterHappening, vector<PrePost>& las);
    void findTriggeringEffectsForInitialState(
            const TimeStampedState* tsstate, vector<PrePost>& effects);
    void findAllEffectCondVars(const ScheduledOperator& new_op, set<
            int>& effect_cond_vars, ActionType type);
    void findPreconditions(const ScheduledOperator& new_op,
            vector<Prevail>& preconditions, ActionType type);
        //      todo();

    int getIndexOfPlanStep(const ScheduledOperator& op, double timestamp);
    void buildInstantPlan();
    void dumpInstantPlan();
    void dumpOrdering();
    void buildTrace();
    void buildPartialOrder();
    Plan createAndSolveSTN();
    bool isMutex(const Operator* op, vector<const Operator*>& otherOps);

public:
    PartialOrderLifter(const Plan &_plan, const PlanTrace &_trace) : plan(_plan), trace(_trace) {
        instant_plan.reserve(sizeof(InstantPlanStep) * (plan.size() * 2 + 2));
    }

    Plan lift();
};

#endif
