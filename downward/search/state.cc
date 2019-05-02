#include "state.h"

#include "axioms.h"
#include "globals.h"
#include "operator.h"
#include "causal_graph.h"
#include "plannerParameters.h"

#include <algorithm>
#include <iostream>
#include <cassert>
using namespace std;

TimeStampedState::TimeStampedState(istream &in)
{
    check_magic(in, "begin_state");
    for(int i = 0; i < g_variable_domain.size(); i++) {
        double var;
        cin >> var;
        state.push_back(var);
    }
    check_magic(in, "end_state");

    g_default_axiom_values = state;
    timestamp = 0.0; // + EPS_TIME;

    numberOfEpsInsertions = 0;

    initialize();
}

TimeStampedState::TimeStampedState(const TimeStampedState &other) :
        state(other.state), scheduled_effects(other.scheduled_effects),
        conds_over_all(other.conds_over_all), conds_at_end(other.conds_at_end),
        timestamp(other.timestamp), operators(other.operators), numberOfEpsInsertions(other.numberOfEpsInsertions)
{
    initialize();
}

TimeStampedState::TimeStampedState(const TimeStampedState &predecessor,
    const Operator &op) :
        state(predecessor.state),
        scheduled_effects(predecessor.scheduled_effects),
        conds_over_all(predecessor.conds_over_all),
        conds_at_end(predecessor.conds_at_end), 
        operators(predecessor.operators)
{
	// FIXME: The effects between now and now + sep can 
    // happen at different timestamps. So, to implement this method 
    // correctly, we have to apply and check the effects in the correct 
    // order (and also check effect conditions in the intermediate steps).
    // This is analogous to the problem in let_time_pass.

    double sep = (g_parameters.epsilonize_internally ? EPS_TIME : 0.0);

    numberOfEpsInsertions = predecessor.numberOfEpsInsertions;
    if(g_parameters.epsilonize_internally) {
        numberOfEpsInsertions++;
    }

    timestamp = predecessor.timestamp + sep;

    // compute duration
    double duration = op.get_duration(&predecessor);

    // The scheduled effects of the new state are precisely the
    // scheduled effects of the predecessor state plus those at-end
    // effects of the given operator whose at-start conditions are
    // satisfied.
    for(int i = 0; i < op.get_pre_post_end().size(); i++) {
        const PrePost &eff = op.get_pre_post_end()[i];
        if(eff.does_fire(predecessor)) {
            scheduled_effects.push_back(ScheduledEffect(duration, eff));
        }
    }

    // The values of the new state are obtained by applying all
    // effects scheduled in the predecessor state until the new time
    // stamp and subsequently applying axioms
    for(int i = 0; i < scheduled_effects.size(); i++) {
        ScheduledEffect &eff = scheduled_effects[i];
        if((eff.time_increment + EPSILON < sep) &&
                         satisfies(eff.cond_end)) {
            apply_effect(eff.var, eff.fop, eff.var_post, eff.post);
        }
        if(eff.time_increment + EPSILON < sep) {
            scheduled_effects.erase(scheduled_effects.begin() + i);
            i--;
        } else {
            eff.time_increment -= sep;
        }
    }
    // Update values affected by an at-start effect of the operator.
    for(int i = 0; i < op.get_pre_post_start().size(); i++) {
        const PrePost &pre_post = op.get_pre_post_start()[i];

        // at-start effects may not have any at-end conditions
        assert(pre_post.cond_end.size() == 0);

        if(pre_post.does_fire(predecessor)) {
            apply_effect(pre_post.var, pre_post.fop, pre_post.var_post,
                    pre_post.post);
        }
    }

    g_axiom_evaluator->evaluate(*this);

    // The persistent over-all conditions of the new state are
    // precisely the persistent over-all conditions of the predecessor
    // state plus the over-all conditions of the newly added operator
    for(int i = 0; i < op.get_prevail_overall().size(); i++) {
        conds_over_all.push_back(ScheduledCondition(duration,
                    op.get_prevail_overall()[i]));
    }
    for(int i = 0; i < conds_over_all.size(); i++) {
        ScheduledCondition &cond = conds_over_all[i];
        double decrement = min(cond.time_increment,sep);
        cond.time_increment -= decrement;
        if(double_equals(cond.time_increment, 0.0)) {
            conds_over_all.erase(conds_over_all.begin() + i);
            i--;
        }
    }

    // The persistent at-end conditions of the new state are
    // precisely the persistent at-end conditions of the predecessor
    // state plus the at-end conditions of the newly added operator
    for(int i = 0; i < op.get_prevail_end().size(); i++) {
        conds_at_end.push_back(ScheduledCondition(duration, op.get_prevail_end()[i]));
    }
    for(int i = 0; i < conds_at_end.size(); i++) {
        ScheduledCondition &cond = conds_at_end[i];
        double decrement = min(cond.time_increment,sep);
        cond.time_increment -= decrement;
        if(cond.time_increment < 0) {
            conds_at_end.erase(conds_at_end.begin() + i);
            i--;
        }
    }

    // The running operators of the new state are precisely
    // the running operators of the predecessor state plus the newly
    // added operator
    operators.push_back(ScheduledOperator(duration, op));
    for(int i = 0; i < operators.size(); i++) {
        ScheduledOperator &op = operators[i];
        double decrement = min(op.time_increment,sep);
        op.time_increment -= decrement;
        if(double_equals(op.time_increment, 0) || op.time_increment <= 0) {
            operators.erase(operators.begin() + i);
            i--;
        }
    }

    // timestamp += EPS_TIME;
    // FIXME: time increments aller Komponenten des Zustands anpassen
    // assert(!double_equals(timestamp, next_happening()));

    initialize();
}

int TimeStampedState::getNumberOfEpsTimeSteps(double offset) const {
    bool recheck = true;
    int ret = 0;
    while(recheck) {
        recheck = false;
        for(unsigned int i = 0; i < operators.size(); ++i) {
            double increment = operators[i].time_increment-offset;
            if(double_equals(increment,ret*EPS_TIME+EPS_TIME)) {
                ret++;
                recheck = true;
                break;
            }
        }
    }
    return ret;
}


TimeStampedState TimeStampedState::let_time_pass(
                                                 bool go_to_intermediate_between_now_and_next_happening,
    bool skip_eps_steps) const {

	// FIXME: If we do not go to the intermediate between now and the 
    // next happening but epsilonize internally, the effects can 
    // happen at different timestamps. So, to implement this method 
    // correctly, we have to apply and check the effects in the correct 
    // order (and also check effect conditions in the intermediate steps).

    // Copy this state
    TimeStampedState succ(*this);

    // The time stamp of the new state is the minimum of the end
    // time points of scheduled effects in the predecessor state
    // and the end time points associated with persistent at-end
    // conditions of the predecessor state. If the flag "go to
    // intermediate between now and next happening" is set, it is
    // the intermediate time point between new and the next happening
    // (this is needed to safely test all persistent over-all
    // conditions -- otherwise we might fail to ever test some of them).
    double nh = next_happening();
    if(double_equals(nh, timestamp) ||
        !go_to_intermediate_between_now_and_next_happening) {
        succ.timestamp = nh;
    } else {
        succ.timestamp = timestamp + 0.5 * (nh - timestamp);
    }

    double time_diff = succ.timestamp - timestamp;

    if(skip_eps_steps && g_parameters.epsilonize_internally && !go_to_intermediate_between_now_and_next_happening) {
        double additional_time_diff = (getNumberOfEpsTimeSteps(nh-timestamp)+1) * EPS_TIME;
        time_diff += additional_time_diff;
        succ.timestamp += additional_time_diff;
    }

    if(!go_to_intermediate_between_now_and_next_happening) {
        // The values of the new state are obtained by applying all
        // effects scheduled in the predecessor state for the new time
        // stamp and subsequently applying axioms
        for(int i = 0; i < scheduled_effects.size(); i++) {
            const ScheduledEffect &eff = scheduled_effects[i];
            if((eff.time_increment < time_diff+EPSILON) &&
                succ.satisfies(eff.cond_end)) {
                succ.apply_effect(eff.var, eff.fop, eff.var_post, eff.post);
            }
        }
        g_axiom_evaluator->evaluate(succ);
    }

    // The scheduled effects of the new state are precisely the
    // scheduled effects of the predecessor state minus those
    // whose scheduled time point has been reached and minus those
    // whose over-all condition is violated.
    for(int i = 0; i < succ.scheduled_effects.size(); i++) {
        succ.scheduled_effects[i].time_increment -= time_diff;
    }
    if(!go_to_intermediate_between_now_and_next_happening) {
        for(int i = 0; i < succ.scheduled_effects.size(); i++) {
            const ScheduledEffect &eff = succ.scheduled_effects[i];
            if((eff.time_increment < EPSILON) ||
                    !succ.satisfies(eff.cond_overall)) {
                succ.scheduled_effects.erase(succ.scheduled_effects.begin() + i);
                i--;
            }
        }
    }

    // The persistent over-all conditions of the new state are
    // precisely those persistent over-all conditions of the predecessor
    // state whose end time-point is properly in the future (not now)
    for(int i = 0; i < succ.conds_over_all.size(); i++) {
        succ.conds_over_all[i].time_increment -= time_diff;
    }
    if(!go_to_intermediate_between_now_and_next_happening) {
        for(int i = 0; i < succ.conds_over_all.size(); i++) {
            const ScheduledCondition &cond = succ.conds_over_all[i];
            if((cond.time_increment < EPSILON)) {
                succ.conds_over_all.erase(succ.conds_over_all.begin() + i);
                i--;
            }
        }
    }

    // The persistent at-end conditions of the new state are
    // precisely those persistent at-end conditions of the predecessor
    // state whose end time-point is in the future
    for(int i = 0; i < succ.conds_at_end.size(); i++) {
        succ.conds_at_end[i].time_increment -= time_diff;
    }
    if(!go_to_intermediate_between_now_and_next_happening) {
        for(int i = 0; i < succ.conds_at_end.size(); i++) {
            const ScheduledCondition &cond = succ.conds_at_end[i];
            if(cond.time_increment < EPSILON) {
                succ.conds_at_end.erase(succ.conds_at_end.begin() + i);
                i--;
            }
        }
    }

    // The running operators of the new state are precisely those
    // running operators of the predecessor state whose end time-point
    // is in the future
    for(int i = 0; i < succ.operators.size(); i++) {
        succ.operators[i].time_increment -= time_diff;
    }
    if(!go_to_intermediate_between_now_and_next_happening) {
        for(int i = 0; i < succ.operators.size(); i++) {
            const ScheduledOperator &op = succ.operators[i];
            if((op.time_increment < EPSILON) || op.time_increment <= 0) {
                succ.operators.erase(succ.operators.begin() + i);
                i--;
            }
            }
    }

    succ.initialize();

    return succ;
}

double TimeStampedState::next_happening() const
{
    double result = REALLYBIG;
    for(int i = 0; i < operators.size(); i++)
        if(operators[i].time_increment > 0)
            result = min(result, operators[i].time_increment);
    if(double_equals(result, REALLYBIG))
        result = 0.0;
    return result + timestamp;
}

void TimeStampedState::dump(bool verbose) const
{
    cout << "State (Timestamp: " << timestamp << ")" << endl;
    if(verbose) {
        cout << " logical state:" << endl;
        for(int i = 0; i < state.size(); i++)
            cout << "  " << g_variable_name[i] << ": " << state[i] << "    " << endl;
        cout << " scheduled effects:" << endl;
        for(int i = 0; i < scheduled_effects.size(); i++) {
            cout << "  <" << (scheduled_effects[i].time_increment + timestamp) << ",<";
            for(int j = 0; j < scheduled_effects[i].cond_overall.size(); j++) {
                cout << g_variable_name[scheduled_effects[i].cond_overall[j].var]
                    << ": " << scheduled_effects[i].cond_overall[j].prev;
            }
            cout << ">,<";
            for(int j = 0; j < scheduled_effects[i].cond_end.size(); j++) {
                cout << g_variable_name[scheduled_effects[i].cond_end[j].var]
                    << ": " << scheduled_effects[i].cond_end[j].prev;
            }
            cout << ">,<";
            cout << g_variable_name[scheduled_effects[i].var] << " ";
            if(is_functional(scheduled_effects[i].var)) {
                cout << scheduled_effects[i].fop << " ";
                cout << g_variable_name[scheduled_effects[i].var_post] << ">>" << endl;
            } else {
                cout << ":= ";
                cout << scheduled_effects[i].post << ">>" << endl;
            }
        }
        cout << " persistent over-all conditions:" << endl;
        for(int i = 0; i < conds_over_all.size(); i++) {
            cout << "  <" << (conds_over_all[i].time_increment + timestamp) << ",<";
            cout << g_variable_name[conds_over_all[i].var] << ":"
                << conds_over_all[i].prev << ">>" << endl;
        }
        cout << " persistent at-end conditions:" << endl;
        for(int i = 0; i < conds_at_end.size(); i++) {
            cout << "  <" << (conds_at_end[i].time_increment + timestamp) << ",<";
            cout << g_variable_name[conds_at_end[i].var] << ":"
                << conds_at_end[i].prev << ">>" << endl;
        }
        cout << " running operators:" << endl;
        for(int i = 0; i < operators.size(); i++) {
            cout << "  <" << (operators[i].time_increment + timestamp) << ",<";
            cout << operators[i].get_name() << ">>" << endl;
        }
    }
}

void TimeStampedState::scheduleEffect(ScheduledEffect effect)
{
    scheduled_effects.push_back(effect);
    sort(scheduled_effects.begin(), scheduled_effects.end());
}

bool TimeStampedState::is_consistent_now() const
{
    // Persistent over-all conditions must be satisfied
    for(int i = 0; i < conds_over_all.size(); i++)
        if(!satisfies(conds_over_all[i]))
            return false;

    // Persistent at-end conditions must be satisfied
    // if their end time point is now
    for(int i = 0; i < conds_at_end.size(); i++)
        if(double_equals(conds_at_end[i].time_increment, 0) &&
            !satisfies(conds_at_end[i]))
            return false;

    // No further conditions (?)
    return true;
}

bool TimeStampedState::is_consistent_when_progressed(TimedSymbolicStates* timedSymbolicStates) const
{
    double last_time = -1.0;
    double current_time = timestamp;
    TimeStampedState current_progression(*this);

    bool go_to_intermediate = true;
    while(!double_equals(current_time, last_time)) {
        if(!current_progression.is_consistent_now()) {
            return false;
        }

        current_progression = current_progression.let_time_pass(go_to_intermediate,false);
        go_to_intermediate = !go_to_intermediate;
        last_time = current_time;
        current_time = current_progression.timestamp;

        if(!go_to_intermediate && timedSymbolicStates != NULL) {
            timedSymbolicStates->push_back(make_pair(vector<double> (), current_progression.timestamp));
            for(int i = 0; i < current_progression.state.size(); ++i) {
                if(g_variable_types[i] == primitive_functional || g_variable_types[i] == logical) {
                    timedSymbolicStates->back().first.push_back(current_progression.state[i]);
                }
            }
        }
    }

    return true;
}

TimeStampedState &buildTestState(TimeStampedState &state)
{
    vector<Prevail> cas;
    vector<Prevail> coa;
    vector<Prevail> cae;
    state.scheduleEffect(ScheduledEffect(1.0, cas, coa, cae, 11, 0, increase));
    state.scheduleEffect(ScheduledEffect(1.0, cas, coa, cae, 9, 0, assign));
    return state;
}
