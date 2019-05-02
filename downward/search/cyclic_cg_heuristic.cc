#include "cyclic_cg_heuristic.h"
#include "plannerParameters.h"
#include "scheduler.h"
#include <algorithm>
#include <cassert>
#include <vector>
#include <set>
#include <limits>

class CausalGraph;
using namespace std;

LocalProblem::LocalProblem(CyclicCGHeuristic* _owner, int the_var_no,
        int the_start_value) :
    owner(_owner), base_priority(-1.0), var_no(the_var_no),
    causal_graph_parents(NULL), start_value(the_start_value)
{
}

double LocalTransition::get_direct_cost()
{
    double ret = 0.0;
    assert(label);
    if(label->duration_variable != -1) {
        if(label->duration_variable == -2) {
            assert(false);
            ScheduledOperator *s_op = dynamic_cast<ScheduledOperator*>(label->op);
            assert(s_op);
            g_HACK()->set_waiting_time(max(g_HACK()->get_waiting_time(), s_op->time_increment));
        } else {
            LocalProblemNode *source = get_source();
            ret = source->children_state[duration_var_local];
        }
    }
    // HACK for modeltrain, where it appears, that the duration of a transition is negative...WHY???
    if(ret < 0.0)
        ret = 0.0;
    return ret;
}

inline CyclicCGHeuristic* LocalTransition::g_HACK()
{
    return get_source()->owner->owner;
}

void LocalTransitionDiscrete::on_source_expanded(const TimeStampedState &state)
{
    assert(source->cost >= 0);
    assert(source->cost < LocalProblem::QUITE_A_LOT);
    assert(source->owner == target->owner);
    double duration = 0.0;
    unreached_conditions = 0;
    if(g_HACK()->is_running(this, state)) {
        target_cost = 0.0;
    } else {
        duration = get_direct_cost();
        target_cost = source->cost + duration;
        if(target->cost <= target_cost) {
            // Transition cannot find a shorter path to target.
            return;
        }
        const vector<LocalAssignment> &prevail = label->precond;
        for(int i = 0; i < prevail.size(); i++) {
            int local_var = prevail[i].local_var;
            int prev_var_no = prevail[i].prev_dtg->var;
            assert(g_variable_types[prev_var_no]==logical ||
                    g_variable_types[prev_var_no]==comparison);
            double current_val = source->children_state[local_var];
            if(!double_equals(current_val, prevail[i].value)) {
                int current_val_int = static_cast<int>(current_val);
                LocalProblem *child_problem = g_HACK()->get_local_problem(
                    prev_var_no, current_val_int);
                if(!child_problem->is_initialized()) {
                    double base_priority = source->priority();
                    child_problem->initialize(base_priority, current_val_int, state);
                }
                int prev_value = static_cast<int> (prevail[i].value);
                LocalProblemNode *cond_node =
                    child_problem->get_node(prev_value);
                if(!double_equals(cond_node->reached_by_wait_for, -1.0)) {
                    assert(!g_parameters.cg_heuristic_zero_cost_waiting_transitions
                            || cond_node->cost == 0.0);  // If zero cost is on, this should be 0.0
                    assert(cond_node->cost < LocalProblem::QUITE_A_LOT);
                    g_HACK()->set_waiting_time(max(g_HACK()->get_waiting_time(),
                        cond_node->reached_by_wait_for - EPS_TIME));
                } else if(cond_node->expanded) {
                    target_cost = target_cost + cond_node->cost;
                    if(target->cost <= target_cost) {
                        // Transition cannot find a shorter path to target.
                        return;
                    }
                } else {
                    unreached_conditions++;
                    cond_node->add_to_waiting_list(this, i);
                }
            }
        }
    }
    try_to_fire();
}

void LocalTransitionDiscrete::on_condition_reached(int /*cond_no*/, double cost)
{
    assert(unreached_conditions);
    --unreached_conditions;
    target_cost = target_cost + cost;
    try_to_fire();
}

void LocalTransitionDiscrete::try_to_fire()
{
    if(!unreached_conditions && target_cost < target->cost) {
        target->cost = target_cost;
        target->reached_by = this;
        target->pred = this;
        g_HACK()->add_to_queue(target);
    }
}

void LocalTransitionDiscrete::print_description()
{
    cout << "<" << source->owner->var_no << "|" << source->value << ","
        << target->value << "> (" << this << "), prevails: ";
    for(int i = 0; i < label->precond.size(); i++) {
        cout
            << (*source->owner->causal_graph_parents)[label->precond[i].local_var]
            << ": " << label->precond[i].value << " ";
    }
}

LocalProblemNodeDiscrete::LocalProblemNodeDiscrete(
        LocalProblemDiscrete *owner_, int children_state_size, int the_value) :
    LocalProblemNode(owner_, children_state_size, the_value)
{
    expanded = false;
    reached_by_wait_for = -1.0;
    reached_by = 0;
    pred = 0;
}

void LocalProblemNodeDiscrete::on_expand(const TimeStampedState &state)
{
    if(expanded)
        return;
    expanded = true;
    // Set children state unless this was an initial node.
    if(reached_by) {
        LocalProblemNode *parent = reached_by->get_source();
        children_state = parent->children_state;
        const vector<LocalAssignment> &prevail = reached_by->label->precond;
        for(int i = 0; i < prevail.size(); i++) {
            children_state[prevail[i].local_var] = prevail[i].value;
        }
        const vector<LocalAssignment> &cyclic_effects =
            reached_by->label->effect;
        for(int i = 0; i < cyclic_effects.size(); i++) {
            if(g_variable_types[cyclic_effects[i].prev_dtg->var] == logical) {
                children_state[cyclic_effects[i].local_var]
                    = cyclic_effects[i].value;
            } else {
                assert(g_variable_types[cyclic_effects[i].prev_dtg->var] == primitive_functional);
                const LocalAssignment &la = cyclic_effects[i];
                updatePrimitiveNumericVariable(la.fop, la.local_var, la.var,
                        children_state);
            }
        }
        if(parent->reached_by)
            reached_by = parent->reached_by;
    }
    for(const_it_waiting_list it = waiting_list.begin(); it
            != waiting_list.end(); ++it) {
        it->first->on_condition_reached(it->second, cost);
    }
    waiting_list.clear();

    for(int i = 0; i < additional_outgoing_transitions.size(); i++) {
        additional_outgoing_transitions[i].on_source_expanded(state);
    }

    for(int i = 0; i < outgoing_transitions.size(); i++) {
        outgoing_transitions[i].on_source_expanded(state);
    }

    return;
}

//waiting edges are for free....check if all conditions are satiesfied indeed!
bool LocalProblemNode::all_conds_satiesfied(const ValueTransitionLabel *label, const TimeStampedState &state)
{
    for(int i = 0; i < label->precond.size(); ++i) {
        int var = label->precond[i].prev_dtg->var;
        if(!double_equals(label->precond[i].value, state[var])) {
            return false;
        }
    }
    return true;
}

void LocalProblemNode::mark_helpful_transitions(const TimeStampedState &state, int goalNumber)
{
    if(!this->owner->is_initialized()) {
        //condition was already satiesfied!
        return;
    }
    assert(cost >= 0 && cost < LocalProblem::QUITE_A_LOT);
    if(reached_by) {
        double duration = 0.0;
        int duration_variable = reached_by->label->duration_variable;
        if(reached_by->label->duration_variable == -2) {
            ScheduledOperator *s_op =
                dynamic_cast<ScheduledOperator*> (reached_by->label->op);
            assert(s_op);
            duration = s_op->time_increment;
        } else if(!(duration_variable == -1)) {
            duration = reached_by->get_source()->children_state[reached_by->duration_var_local];
        }
        if(double_equals(reached_by->target_cost, duration)) {
            assert(reached_by->label);
            // if reached_by->label->op is NULL this means that the transition corresponds to
            // an axiom. Do not add this axiom (or rather a NULL pointer) to the set of
            // preferred operators.
            if(reached_by->label->op && all_conds_satiesfied(reached_by->label, state) && !g_HACK()->is_running(reached_by, state)) {
                const Operator *op = reached_by->label->op;
                g_HACK()->prefOpsSortedByCorrespondigGoal[goalNumber].insert(op);
            }
        } else {
            // Recursively compute helpful transitions for prevailed variables.
            if(!g_HACK()->is_running(reached_by, state)) {
                const vector<LocalAssignment> &prevail = reached_by->label->precond;
                for(int i = 0; i < prevail.size(); i++) {
                    int prev_var_no = prevail[i].prev_dtg->var;
                    int prev_value = static_cast<int> (prevail[i].value);
                    int value = static_cast<int> (state[prev_var_no]);

                    if(prev_value == value)
                        continue;

                    LocalProblemNode *child_node = g_HACK()->get_local_problem(
                        prev_var_no, value)->get_node(prev_value);
                    assert(child_node);
                    if(child_node->cost < LocalProblem::QUITE_A_LOT &&
                        double_equals(child_node->reached_by_wait_for, -1.0)) {
                        child_node->mark_helpful_transitions(state, (this==g_HACK()->goal_node ? i : goalNumber));
                    }
                }
            }
        }
    }
}

void CyclicCGHeuristic::setCheapestPrefOpsExternally(int number) {
    cheapestGoals.clear();
    for(unsigned int i = 0; i < number; ++i) {
        double cheapestGoal = numeric_limits<double>::max();
        int idx = -1;
        for(unsigned int j = 0; j < costsOfGoals.size(); ++j) {
            assert(double_equals(costsOfGoals[j],-1.0) || costsOfGoals[j] >= 0.0);
            if(!double_equals(costsOfGoals[j],-1.0) && (costsOfGoals[j] < cheapestGoal)) {
                cheapestGoal = costsOfGoals[j];
                idx = j;
            }
        }
        if(idx == -1) {
            return;
        }
        set<const Operator*> prefOps = prefOpsSortedByCorrespondigGoal[idx];
        for(set<const Operator*>::iterator it = prefOps.begin(); it != prefOps.end(); ++it) {
        	set_preferred(*it, CHEAPEST);
        }
        costsOfGoals[idx] = -1.0;
    }
}

void CyclicCGHeuristic::setMostExpensivePrefOpsExternally(int number) {
    mostExpensiveGoals.clear();
    for(unsigned int i = 0; i < number; ++i) {
        double mostExpensiveGoal = -numeric_limits<double>::max();
        int idx = -1;
        for(unsigned int j = 0; j < costsOfGoals.size(); ++j) {
            assert(double_equals(costsOfGoals[j],-1.0) || costsOfGoals[j] >= 0.0);
            if(!double_equals(costsOfGoals[j],-1.0) && (costsOfGoals[j] > mostExpensiveGoal)) {
                mostExpensiveGoal = costsOfGoals[j];
                idx = j;
            }
        }
        if(idx == -1) {
            return;
        }
        set<const Operator*> prefOps = prefOpsSortedByCorrespondigGoal[idx];
        for(set<const Operator*>::iterator it = prefOps.begin(); it != prefOps.end(); ++it) {
            set_preferred(*it, MOSTEXPENSIVE);
        }
        costsOfGoals[idx] = -1.0;
    }
}

void CyclicCGHeuristic::setRandPrefOpsExternally(int number) {
    randGoals.clear();
    vector<int> notYetSatiesfiedGoals;
    for(unsigned int j = 0; j < costsOfGoals.size(); ++j) {
        assert(double_equals(costsOfGoals[j],-1.0) || costsOfGoals[j] >= 0.0);
        if(!double_equals(costsOfGoals[j],-1.0)) {
            notYetSatiesfiedGoals.push_back(j);
        }
    }
    for(unsigned int i = 0; i < number; ++i) {
        if(notYetSatiesfiedGoals.size() == 0) {
            return;
        }
        int idx = rand() % notYetSatiesfiedGoals.size();
        set<const Operator*> prefOps = prefOpsSortedByCorrespondigGoal[notYetSatiesfiedGoals[idx]];
        for(set<const Operator*>::iterator it = prefOps.begin(); it != prefOps.end(); ++it) {
            set_preferred(*it, RAND);
        }
        notYetSatiesfiedGoals[idx] = notYetSatiesfiedGoals[notYetSatiesfiedGoals.size()-1];
        notYetSatiesfiedGoals.pop_back();
    }
}

void CyclicCGHeuristic::setConcurrentPrefOpsExternally(const TimeStampedState &state) {
	for(unsigned int i = 0; i < prefOpsSortedByCorrespondigGoal.size(); ++i) {
		set<const Operator*>& prefOps = prefOpsSortedByCorrespondigGoal[i];
		for(set<const Operator*>::iterator it = prefOps.begin(); it != prefOps.end(); ++it) {
			bool isMutex = false;
			for(int i = 0; i < preferred_operators_concurrent.size(); i++) {
				const Operator* prefOp = preferred_operators_concurrent[i];
				if((*it)->isDisabledBy(prefOp) || prefOp->isDisabledBy(*it)) {
					isMutex = true;
					break;
				}
            }
			if(!isMutex && (*it)->is_applicable(state)) {
				set_preferred(*it, CONCURRENT);
			}
		}
	}
}

void CyclicCGHeuristic::setFirstPrefOpsExternally(int number) {
	for(unsigned int i = 0; i < prefOpsSortedByCorrespondigGoal.size(); ++i) {
		if(number <= 0) {
			return;
		}
		if(!double_equals(costsOfGoals[i],-1.0)) {
			assert(i < prefOpsSortedByCorrespondigGoal.size());
			set<const Operator*>& prefOpsSortedOfThisGoal = prefOpsSortedByCorrespondigGoal[i];
			for(set<const Operator*>::iterator it = prefOpsSortedOfThisGoal.begin(); it != prefOpsSortedOfThisGoal.end(); ++it) {
				set_preferred(*it, ORDERED);
			}
			number--;
		}
	}
}

void CyclicCGHeuristic::setAllPrefOpsExternally() {
	for(unsigned int i = 0; i < prefOpsSortedByCorrespondigGoal.size(); ++i) {
		set<const Operator*>& prefOps = prefOpsSortedByCorrespondigGoal[i];
		for(set<const Operator*>::iterator it = prefOps.begin(); it != prefOps.end(); ++it) {
			set_preferred(*it, REGULAR);
		}
	}
}

//value of -1.0 means that the goal is already reached or already examined.
void LocalProblemNode::setCostsOfGoals(const TimeStampedState &state) {
    assert(this == g_HACK()->goal_node);
    const vector<LocalAssignment> &prevail = reached_by->label->precond;
    assert(prevail.size() > 0);
    vector<double>& costsOfGoals = g_HACK()->costsOfGoals;
    costsOfGoals.clear();
    for(unsigned int i = 0; i < prevail.size(); i++) {
        int prev_var_no = prevail[i].prev_dtg->var;
        int prev_value = static_cast<int> (prevail[i].value);
        int value = static_cast<int> (state[prev_var_no]);
        if(double_equals(value,prev_value)) {
            costsOfGoals.push_back(-1.0);
            continue;
        }
        LocalProblem *child_problem = g_HACK()->get_local_problem(prev_var_no, value);
        assert(child_problem->is_initialized());
        LocalProblemNode *cond_node = child_problem->get_node(prev_value);
        assert(cond_node->expanded);
        costsOfGoals.push_back(cond_node->cost);
    }
}

void LocalProblemNodeDiscrete::dump()
{
    cout << "---------------" << endl;
    print_name();
    cout << "Waiting list:" << endl;
    for(const_it_waiting_list it = waiting_list.begin(); it
            != waiting_list.end(); ++it) {
        cout << " ";
        it->first->print_description();
        cout << "," << it->second << endl;
    }
    cout << "Context:" << endl;
    if(!expanded)
        cout << " not set yet!" << endl;
    else {
        for(int i = 0; i < owner->causal_graph_parents->size(); i++) {
            cout << (*owner->causal_graph_parents)[i] << ": ";
            cout << children_state[i] << endl;
        }
    }
    cout << "cost: " << cost << endl;
    cout << "priority: " << priority() << endl;
    cout << "reached_by: ";
    if(reached_by && reached_by->label && reached_by->label->op) {
        cout << reached_by->label->op->get_name() << endl;
    } else {
        cout << "axiom" << endl;
    }
    cout << "pred: ";
    if(pred && pred->label && pred->label->op) {
        cout << pred->label->op->get_name() << endl;
    } else {
        cout << "axiom" << endl;
    }
    cout << "---------------" << endl;
}

void LocalProblemNodeDiscrete::print_name()
{
    cout << "Local Problem [" << owner->var_no << ","
        << (dynamic_cast<LocalProblemDiscrete*> (owner))->start_value
        << "], node " << value << " (" << this << ")" << endl;
}

void LocalProblemDiscrete::compile_DTG_arcs_to_LTD_objects(
        DomainTransitionGraphSymb *dtgs)
{
    for(int value = 0; value < nodes.size(); value++) {
        LocalProblemNodeDiscrete &node = nodes[value];
        node.outgoing_transitions.clear();
        node.additional_outgoing_transitions.clear();
        ValueNode &dtg_node = dtgs->nodes[value];
        for(int i = 0; i < dtg_node.transitions.size(); i++) {
            ValueTransition &dtg_trans = dtg_node.transitions[i];
            int target_value = dtg_trans.target->value;
            LocalProblemNodeDiscrete &target = nodes[target_value];
            for(int j = 0; j < dtg_trans.ccg_labels.size(); j++) {
                ValueTransitionLabel *label = dtg_trans.ccg_labels[j];
                LocalTransitionDiscrete trans(label, &node, &target);
                trans.duration_var_local = getLocalIndexOfGlobalVariable(
                        label->duration_variable);
                assert(trans.duration_var_local != -1 || label->duration_variable == -1 || label->duration_variable == -2);
                node.outgoing_transitions.push_back(trans);
            }
        }
        for(int i = 0; i < dtg_node.additional_transitions.size(); i++) {
            ValueTransition &dtg_trans = dtg_node.additional_transitions[i];
            int target_value = dtg_trans.target->value;
            LocalProblemNodeDiscrete &target = nodes[target_value];
            for(int j = 0; j < dtg_trans.ccg_labels.size(); j++) {
                ValueTransitionLabel *label = dtg_trans.ccg_labels[j];
                LocalTransitionDiscrete trans(label, &node, &target);
                trans.duration_var_local = getLocalIndexOfGlobalVariable(
                        label->duration_variable);
                assert(trans.duration_var_local != -1 || label->duration_variable == -1 || label->duration_variable == -2);
                node.additional_outgoing_transitions.push_back(trans);
            }
        }
    }
}

void LocalProblemDiscrete::build_nodes_for_variable(int var_no)
{
    if(!(g_variable_types[var_no] == logical)) {
        cout << "variable type: " << g_variable_types[var_no] << endl;
        assert(false);
    }
    DomainTransitionGraph *dtg = g_transition_graphs[var_no];
    DomainTransitionGraphSymb *dtgs =
        dynamic_cast<DomainTransitionGraphSymb *> (dtg);
    assert(dtgs);
    causal_graph_parents = &dtg->ccg_parents;
    global_to_local_parents = &dtg->global_to_local_ccg_parents;
    int num_parents = causal_graph_parents->size();
    for(int value = 0; value < g_variable_domain[var_no]; value++)
        nodes.push_back(LocalProblemNodeDiscrete(this, num_parents, value));
    compile_DTG_arcs_to_LTD_objects(dtgs);
}

void LocalProblemDiscrete::build_nodes_for_goal()
{
    // TODO: We have a small memory leak here. Could be fixed by
    // making two LocalProblem classes with a virtual destructor.
    causal_graph_parents = new vector<int> ;
    for(int i = 0; i < g_goal.size(); i++)
        causal_graph_parents->push_back(g_goal[i].first);

    for(int value = 0; value < 2; value++)
        nodes.push_back(LocalProblemNodeDiscrete(this, g_goal.size(), value));

    vector<LocalAssignment> goals;
    for(int i = 0; i < g_goal.size(); i++) {
        int goal_var = g_goal[i].first;
        double goal_value = g_goal[i].second;
        DomainTransitionGraph *goal_dtg = g_transition_graphs[goal_var];
        goals.push_back(LocalAssignment(goal_dtg, i, goal_value, end_cond));
    }
    ValueTransitionLabel *label = new ValueTransitionLabel(-1, goals, end);
    LocalProblemNodeDiscrete *target = &nodes[1];
    LocalProblemNodeDiscrete *start = &nodes[0];
    assert(label);
    LocalTransitionDiscrete trans(label, start, target);

    nodes[0].outgoing_transitions.push_back(trans);
}

LocalProblemDiscrete::LocalProblemDiscrete(CyclicCGHeuristic* _owner, int the_var_no, int the_start_val) :
    LocalProblem(_owner, the_var_no, the_start_val)
{
    if(var_no == -1)
        build_nodes_for_goal();
    else
        build_nodes_for_variable(var_no);
    int parents_num = causal_graph_parents->size();
    nodes[0].children_state.resize(parents_num, 0.0);
    nodes[1].children_state.resize(parents_num, 0.0);

    if(var_no != -1) {
        depending_vars.resize(parents_num);
        children_in_cg.resize(parents_num);
        buildDependingVars(parents_num);
    }
}

void LocalProblemDiscrete::initialize(double base_priority_, int start_value,
        const TimeStampedState &state)
{
    assert(!is_initialized());
    base_priority = base_priority_;

    for(int to_value = 0; to_value < nodes.size(); to_value++) {
        nodes[to_value].expanded = false;
        nodes[to_value].cost = QUITE_A_LOT;
        nodes[to_value].reached_by = NULL;
        nodes[to_value].pred = NULL;
        nodes[to_value].waiting_list.clear();
        nodes[to_value].reached_by_wait_for = -1.0;
    }
    LocalProblemNodeDiscrete *start = &nodes[start_value];
    start->cost = 0;
    int parents_num = causal_graph_parents->size();
    for(int i = 0; i < parents_num; i++) {
        int var = (*causal_graph_parents)[i];
        start->children_state[i] = state[var];
    }
    owner->add_to_queue(start);
    if(g_parameters.cg_heuristic_fire_waiting_transitions_only_if_local_problems_matches_state) {
        if(!(double_equals(state[var_no], start_value))) {
            return;
        }
    }
    for(int i = 0; i < state.scheduled_effects.size(); i++) {
        const ScheduledEffect &seffect = state.scheduled_effects[i];
        if(seffect.var == var_no) {
            LocalProblemNodeDiscrete& node = nodes[static_cast<int>(seffect.post)];
            if(g_parameters.cg_heuristic_zero_cost_waiting_transitions)
                node.cost = 0.0;
            else
                node.cost = seffect.time_increment;
            assert(seffect.time_increment > 0.0);
            assert(seffect.time_increment < LocalProblem::QUITE_A_LOT);
            node.reached_by_wait_for = seffect.time_increment;
            for(int i = 0; i < parents_num; ++i) {
                int varToSet = (*causal_graph_parents)[i];
                double valueToSet = state[varToSet];
                for(int j = 0; j < state.scheduled_effects.size(); ++j) {
                    if(j == i)
                        continue;
                    const ScheduledEffect &otherSeffect = state.scheduled_effects[j];
                    if(otherSeffect.var == varToSet && 
                       otherSeffect.time_increment == seffect.time_increment) {
                        valueToSet = otherSeffect.post;
                        break;
                    }
                }
                node.children_state[i] = valueToSet;
            }
            node.on_expand(state);
        }
    }
}

vector<TimedOp> LocalProblem::generate_causal_constraints(LocalProblemNode* goalNode, set<
        CausalConstraint>& constraints, vector<TimedOp>& neededOps,
        const TimeStampedState& state, set<const Operator*>& labels)
{
    LocalProblemNode* startNode = get_node(start_value);
    vector<LocalTransition*> path;
    LocalProblemNode* helpNode = goalNode;
    while(helpNode != startNode && helpNode->pred && !owner->is_running(
                helpNode->pred, state)) {
        path.push_back(helpNode->pred);
        helpNode = helpNode->pred->get_source();
    }
    bool transIsOp = false;
    int indexOfMainTrans = -1;
    int indexOfLastMainTrans = -1;
    vector<TimedOp> returnOps = vector<TimedOp> ();
    for(int i = 0; i < path.size(); ++i) {
        LocalTransition* trans = path[i];
        if(!(owner->is_running(trans, state))) {
            if(trans->label->op) {
                Operator *op = trans->label->op;
                if(trans->label && (labels.find(op) != labels.end())) {
                    //                    continue;
                }
                double duration = trans->get_direct_cost();
                indexOfMainTrans = neededOps.size();
                TimedOp newOp = tr1::make_tuple(op, duration, indexOfMainTrans);
                neededOps.push_back(newOp);
                labels.insert(op);
                transIsOp = true;
                for(int l = 0; l < returnOps.size(); ++l) {
                    if(tr1::get<0>(neededOps[indexOfMainTrans])->isDisabledBy(
                                tr1::get<0>(returnOps[l]))) {
                        //zwischen kante und allen kanten und deren cond-kanten, die im gleichen graph danach kommen
                        constraints.insert(make_pair(indexOfMainTrans,
                                    tr1::get<2>(returnOps[l])));
                    }
                }
            } else {
                indexOfMainTrans = -1;
                transIsOp = false;
            }
            const vector<LocalAssignment>& preconds = trans->label->precond;
            vector<vector<TimedOp> > subplans;
            for(int j = 0; j < preconds.size(); ++j) {
                vector<TimedOp> newOps = vector<TimedOp> ();
                double actual_value =
                    trans->get_source()->children_state[preconds[j].local_var];
                if(!(double_equals(actual_value, preconds[j].value))) {
                    int var = preconds[j].prev_dtg->var;
                    LocalProblem* sub_problem = owner->get_local_problem(var,
                            static_cast<int> (actual_value));
                    newOps = sub_problem->generate_causal_constraints(
                            (sub_problem->get_node(
                                                   static_cast<int> (preconds[j].value))),
                            constraints, neededOps, state, labels);
                    if(transIsOp) {
                        //                        cout << "TRANS: " << trans->label->op->get_name() << endl;
                        for(int k = 0; k < newOps.size(); ++k) {
                            //    cout << " NEWOP: " << tr1::get<0>(newOps[k])->get_name() << endl;
                            //zwischen kante und allen aktionen der conditions
                            constraints.insert(make_pair(indexOfMainTrans + 1
                                        + k, indexOfMainTrans));
                            for(int l = 0; l < returnOps.size(); ++l) {
                                //    cout << "  RETOP: " << tr1::get<0>(returnOps[l])->get_name() << endl;
                                if(tr1::get<0>(newOps[k])->isDisabledBy(
                                            tr1::get<0>(returnOps[l]))) {
                                    //    cout << "   yes: " << tr1::get<2>(newOps[k]) << " << " << tr1::get<2>(returnOps[l]) << endl;
                                    //zwischen cond-kanten der einen und cond-kanten aller nachfolgenden
                                    constraints.insert(make_pair(tr1::get<2>(
                                                    newOps[k]), tr1::get<2>(
                                                        returnOps[l])));
                                } else {
                                    //cout << "   no" << endl;
                                }
                            }
                        }
                        //                        cout << "." << endl;
                    }
                }
                subplans.push_back(newOps);
            }
            assert(subplans.size() == preconds.size());

            if(transIsOp) {
                returnOps.push_back(tr1::make_tuple(trans->label->op,
                            trans->get_direct_cost(), indexOfMainTrans));
            }
            for(int k = 0; k < subplans.size(); ++k) {
                returnOps.insert(returnOps.end(), subplans[k].begin(),
                        subplans[k].end());
            }
            //            cout << "subplan sizes:" << endl;
            //            for(int v = 0; v < subplans.size(); ++v) {
            //                cout << v << ":" << subplans[v].size() << endl;
            //            }

            for(int k = 0; k < subplans.size() - 1; ++k) {
                if(subplans.size() == 0)
                    break;
                for(int l = 0; l < subplans[k].size(); ++l) {
                    TimedOp& firstAction = subplans[k][l];
                    for(int s = k + 1; s < subplans.size(); ++s) {
                        for(int m = 0; m < subplans[s].size(); ++m) {
                            TimedOp& secondAction = subplans[s][m];
                            if(tr1::get<0>(firstAction)->isDisabledBy(
                                        tr1::get<0>(secondAction))) {
                                //zwischen allen aktionen der teilpläne
                                constraints.insert(
                                        make_pair(tr1::get<2>(firstAction),
                                            tr1::get<2>(secondAction)));
                            }
                        }
                    }
                }
            }
            if(transIsOp && indexOfLastMainTrans != -1) {
                //                constraints.insert(make_pair(indexOfMainTrans, indexOfLastMainTrans));
            }
            indexOfLastMainTrans = indexOfMainTrans;
        } else {
            cout << "Error: Owner was running" << endl;
        }
    }
    return returnOps;
}

void LocalTransitionComp::print_description()
{
    const FuncTransitionLabel *label_func =
        dynamic_cast<const FuncTransitionLabel*> (label);
    assert(label_func);
    cout << "<" << source->owner->var_no << "|" << source->value << "> ("
        << this << "), prevails: ";
    for(int i = 0; i < label->precond.size(); i++) {
        cout
            << (*source->owner->causal_graph_parents)[label->precond[i].local_var]
            << ": " << label->precond[i].value << " ";
    }
    cout << ";   affecting variable: " << label_func->starting_variable << " ";
}

LocalProblemNodeComp::LocalProblemNodeComp(LocalProblemComp *owner_,
        int children_state_size, int the_value, binary_op the_op) :
    LocalProblemNode(owner_, children_state_size, the_value), op(the_op)
{
    expanded = false;
    opened = false;
    reached_by_wait_for = -1.0;
    reached_by = NULL;
    pred = NULL;
    bestTransition = NULL;
}

void LocalProblemNode::add_to_waiting_list(LocalTransition *transition,
        int prevail_no)
{
    waiting_list.push_front(make_pair(transition, prevail_no));
}

inline CyclicCGHeuristic* LocalProblemNode::g_HACK()
{
    return owner->owner;
}

void LocalProblemNodeComp::expand(LocalTransitionComp* trans)
{
    LocalProblemComp *prob = dynamic_cast<LocalProblemComp*> (owner);
    LocalProblemNodeComp *target_node = &(prob->nodes[1 - prob->start_value]);
    target_node->reached_by = trans;
    target_node->pred = trans;
    target_node->expanded = true;
    expanded = true;

    //call transitions on the own waiting lists
    for(const_it_waiting_list it = target_node->waiting_list.begin(); it
            != target_node->waiting_list.end(); ++it) {
        it->first->on_condition_reached(it->second, target_node->cost);
    }
    target_node->waiting_list.clear();
}

void LocalProblemNodeComp::fire(LocalTransitionComp* trans)
{
    // we have to add the costs of the transition itself!
    trans->target_cost += trans->get_direct_cost();
    if(trans->target_cost < trans->target->cost) {
        trans->target->cost = trans->target_cost;
        trans->target->bestTransition = trans;
        bestTransition = trans;
        trans->target->opened = true;
    }
}

void LocalProblemNodeComp::on_expand(const TimeStampedState &state)
{
    if(opened) {
        if(bestTransition) {
            bestTransition->source->expand(bestTransition);
        }
        return;
    }
    opened = true;

    vector<LocalTransitionComp*> ready_transitions;
    nodes_where_this_subscribe.resize(outgoing_transitions.size());
    vector<double> temp_children_state(children_state.size());
    for(int i = 0; i < outgoing_transitions.size(); i++) {
        LocalTransitionComp *trans = &outgoing_transitions[i];
        temp_children_state = children_state;
        updateNumericVariables((*trans), temp_children_state);
        if(check_progress_of_transition(temp_children_state, trans)) {
            if(is_satiesfied(i, trans, state)) {
                ready_transitions.push_back(trans);
            } else if(g_HACK()->is_running(trans, state)) {
                ready_transitions.push_back(trans);
            }
        }
    }
    if(!ready_transitions.empty()) {
        for(int i = 0; i < ready_transitions.size(); ++i) {
            fire(ready_transitions[i]);
        }
        assert(bestTransition);
        assert(bestTransition->target);
        g_HACK()->add_to_queue(bestTransition->target);
    }
    subscribe_to_waiting_lists();
    return;
}

bool LocalProblemNodeComp::check_progress_of_transition(
        vector<double> &temp_children_state, LocalTransitionComp *trans)
{
    double &old_value = children_state[0];
    double &new_value = temp_children_state[0];
    LocalProblemNodeComp *node = dynamic_cast<LocalProblemNodeComp*>(trans->get_source());
    assert(node);
    binary_op comp_op = node->op;
    switch (comp_op) {
        case lt:
        case le:
            return new_value < old_value;
        case gt:
        case ge:
            return new_value > old_value;
        case eq:
            return abs(new_value) < abs(old_value);
        case ue:
            return !(double_equals(new_value, 0));
        default:
            return false;
    }
}

void LocalTransitionComp::on_condition_reached(int cond_no, double cost)
{
    if(!source->opened || source->expanded || conds_satiesfied[cond_no] == true) {
        return;
    }
    target_cost += cost;
    assert(conds_satiesfied[cond_no] == false);
    conds_satiesfied[cond_no] = true;
    for(int i = 0; i < conds_satiesfied.size(); i++)
        if(conds_satiesfied[i] == false)
            return;
    // we have to add the costs of the transition itself!
    target_cost += get_direct_cost();
    target->cost = target_cost;
    target->bestTransition = this;
    source->bestTransition = this;
    target->opened = true;
    g_HACK()->add_to_queue(target);
}

bool CyclicCGHeuristic::is_running(LocalTransition* trans, const TimeStampedState& state)
{
    if(!trans->label || !trans->label->op)
        return false;
    for(int i = 0; i < state.operators.size(); ++i) {
        if(!(state.operators[i].get_name().compare(trans->label->op->get_name()))) {
            set_waiting_time(max(get_waiting_time(), state.operators[i].time_increment - EPS_TIME));
            return true;
        }
    }
    return false;
}

bool LocalProblemNodeComp::is_satiesfied(int trans_index,
        LocalTransitionComp* trans, const TimeStampedState &state)
{
    bool ret = true;
    for(int i = 0; i < trans->label->precond.size(); i++) {
        const LocalAssignment &pre_cond = trans->label->precond[i];
        // check whether the cost of this prevail has already been computed
        int global_var = (*(owner->causal_graph_parents))[pre_cond.local_var];
        assert(!is_functional(global_var));
        double current_val = children_state[pre_cond.local_var];
        if(double_equals(current_val, pre_cond.value)) {
            // to change nothing costs nothing
            assert(trans->conds_satiesfied[i] == false);
            trans->conds_satiesfied[i] = true;
            continue;
        }
        int current_val_int = static_cast<int>(current_val);
        LocalProblem *child_problem = g_HACK()->get_local_problem(global_var,
                current_val_int);
        if(!child_problem->is_initialized()) {
            double base_priority = priority();
            child_problem->initialize(base_priority, current_val_int, state);
        }
        int prev_value = static_cast<int> (pre_cond.value);
        LocalProblemNode *cond_node = child_problem->get_node(prev_value);
        if(!double_equals(cond_node->reached_by_wait_for, -1.0)) {
            g_HACK()->set_waiting_time(max(g_HACK()->get_waiting_time(),
                cond_node->reached_by_wait_for - EPS_TIME));
            assert(trans->target_cost < LocalProblem::QUITE_A_LOT);
            assert(trans->conds_satiesfied[i] == false);
            trans->conds_satiesfied[i] = true;
        } else if(cond_node->expanded) {
            trans->target_cost = trans->target_cost + cond_node->cost;
            assert(trans->conds_satiesfied[i] == false);
            trans->conds_satiesfied[i] = true;
        } else {
            nodes_where_this_subscribe[trans_index].push_back(make_pair(
                        cond_node, i));
            //the cost of this prevail has not been determined so far...
            ret = false;
        }
    }
    return ret;
}

bool LocalProblemNodeComp::is_directly_satiesfied(
        const LocalAssignment &pre_cond)
{
    if(double_equals(children_state[pre_cond.local_var], pre_cond.value))
        return true;
    return false;
}

void LocalProblemNodeComp::subscribe_to_waiting_lists()
{
    for(int i = 0; i < nodes_where_this_subscribe.size(); i++) {
        for(int j = 0; j < nodes_where_this_subscribe[i].size(); j++) {
            LocalProblemNode *prob = nodes_where_this_subscribe[i][j].first;
            int prevail_no = nodes_where_this_subscribe[i][j].second;
            prob->add_to_waiting_list(&(outgoing_transitions[i]), prevail_no);
        }
    }
}

void LocalProblemNodeComp::updateNumericVariables(LocalTransitionComp &trans,
        vector<double> &temp_children_state)
{
    const FuncTransitionLabel* label_func =
        dynamic_cast<const FuncTransitionLabel*> (trans.label);
    assert(label_func);
    int primitive_var_local = label_func->starting_variable;
    assert(g_variable_types[(*owner->causal_graph_parents)[primitive_var_local]]
            == primitive_functional);
    int influencing_var_local = label_func->influencing_variable;
    assert(is_functional((*owner->causal_graph_parents)[influencing_var_local]));
    assignment_op a_op = label_func->a_op;
    updatePrimitiveNumericVariable(a_op, primitive_var_local,
            influencing_var_local, temp_children_state);
}

void LocalProblemNode::updatePrimitiveNumericVariable(assignment_op a_op,
        int primitive_var_local, int influencing_var_local,
        vector<double> &temp_children_state)
{
    double &new_value = temp_children_state[primitive_var_local];
    double &influencing_value = temp_children_state[influencing_var_local];
    switch (a_op) {
        case assign:
            new_value = influencing_value;
            break;
        case scale_up:
            new_value *= influencing_value;
            break;
        case scale_down:
            if(double_equals(influencing_value, 0.0)) {
                if(new_value < 0)
                    new_value = REALLYBIG;
                else
                    new_value = REALLYSMALL;
            } else
                new_value /= influencing_value;
            break;
        case increase:
            new_value += influencing_value;
            break;
        case decrease:
            new_value -= influencing_value;
            break;
        default:
            assert(false);
            break;
    }
    updateNumericVariablesRec(primitive_var_local, temp_children_state);
}

void LocalProblemNode::updateNumericVariablesRec(int local_var,
        vector<double> &temp_children_state)
{
    for(int i = 0; i < owner->depending_vars[local_var].size(); i++) {
        int var_to_update = owner->depending_vars[local_var][i];
        if(!(owner->children_in_cg[var_to_update].size() == 3)) {
            //	    cout << "owner->children_in_cg[var_to_update].size(): " << owner->children_in_cg[var_to_update].size() << endl;
            //	    assert(false);
            continue;
        }
        binary_op
            bop =
            static_cast<binary_op> (owner->children_in_cg[var_to_update][2]);
        int left_var = owner->children_in_cg[var_to_update][0];
        int right_var = owner->children_in_cg[var_to_update][1];
        if(g_variable_types[(*(owner->causal_graph_parents))[var_to_update]]
                == subterm_functional) {
            updateSubtermNumericVariables(var_to_update, bop, left_var,
                    right_var, temp_children_state);
        } else {
            assert(g_variable_types[(*(owner->causal_graph_parents))[var_to_update]] == comparison);
            updateComparisonVariables(var_to_update, bop, left_var, right_var,
                    temp_children_state);
        }
    }
}

int LocalProblem::getLocalIndexOfGlobalVariable(int global_var)
{
    hashmap::const_iterator iter;
    iter = global_to_local_parents->find(global_var);
    if(iter == global_to_local_parents->end())
        return -1;
    return (*iter).second;
}

void LocalProblemNode::updateComparisonVariables(int var, binary_op op,
        int left_var, int right_var, vector<double> &temp_children_state)
{
    double &left = temp_children_state[left_var];
    double &right = temp_children_state[right_var];
    double &target = temp_children_state[var];
    switch (op) {
        case lt:
            if(left + EPSILON < right) {
                target = 0;
            } else {
                target = 1;
            }
            break;
        case le:
            if(left + EPSILON < right || double_equals(left, right)) {
                target = 0;
            } else {
                target = 1;
            }
            break;
        case eq:
            if(double_equals(left, right)) {
                target = 0;
            } else {
                target = 1;
            }
            break;
        case gt:
            if(left + EPSILON > right) {
                target = 0;
            } else {
                target = 1;
            }
            break;
        case ge:
            if(left + EPSILON > right || !double_equals(left, right)) {
                target = 0;
            } else {
                target = 1;
            }
            break;
        case ue:
            if(!double_equals(left, right)) {
                target = 0;
            } else {
                target = 1;
            }

        default:
            assert(false);
            break;
    }
    updateNumericVariablesRec(var, temp_children_state);
}

void LocalProblemNode::updateSubtermNumericVariables(int var, binary_op op,
        int left_var, int right_var, vector<double> &temp_children_state)
{
    double &left = temp_children_state[left_var];
    double &right = temp_children_state[right_var];
    double &target = temp_children_state[var];
    switch (op) {
        case add:
            target = left + right;
            break;
        case subtract:
            target = left - right;
            break;
        case mult:
            target = left * right;
            break;
        case divis:
            if(double_equals(right, 0.0)) {
                if(left < 0)
                    target = REALLYBIG;
                else
                    target = REALLYSMALL;
            } else
                target = left / right;
            break;
        default:
            assert(false);
            break;
    }
    updateNumericVariablesRec(var, temp_children_state);
}

void LocalProblemNodeComp::dump()
{
    cout << "---------------" << endl;
    print_name();
    cout << "Waiting list:" << endl;
    for(const_it_waiting_list it = waiting_list.begin(); it
            != waiting_list.end(); ++it) {
        cout << " ";
        it->first->print_description();
        cout << "," << it->second << endl;
    }
    cout << "Context:" << endl;
    if(!opened)
        cout << " not set yet!" << endl;
    else {
        for(int i = 0; i < owner->causal_graph_parents->size(); i++) {
            cout << (*owner->causal_graph_parents)[i] << ": ";
            cout << children_state[i] << endl;
        }
    }
    cout << "cost: " << cost << endl;
    cout << "priority: " << priority() << endl;
    cout << "---------------" << endl;
}

void LocalProblemNodeComp::print_name()
{
    cout << "Local Problem [" << owner->var_no << ","
        << (dynamic_cast<LocalProblemComp*> (owner))->start_value
        << "], node " << value << " (" << this << ")" << endl;
}

void LocalProblem::buildDependingVars(int parents_num)
{
    for(int i = 0; i < parents_num; i++) {
        int context_variable = (*causal_graph_parents)[i];
        const vector<int>& current_depending_vars =
            g_causal_graph->get_successors(context_variable);
        for(int j = 0; j < current_depending_vars.size(); j++) {
            int current_depending_var = current_depending_vars[j];
            if(g_variable_types[current_depending_var] == comparison
                    || g_variable_types[current_depending_var]
                    == subterm_functional) {
                int idx = getLocalIndexOfGlobalVariable(current_depending_var);
                if(idx != -1)
                    depending_vars[i].push_back(idx);
            }
        }
        if(g_variable_types[context_variable] == subterm_functional) {
            DomainTransitionGraph *dtg = g_transition_graphs[context_variable];
            DomainTransitionGraphSubterm *dtgs =
                dynamic_cast<DomainTransitionGraphSubterm*> (dtg);
            assert(dtgs);
            int left_var = getLocalIndexOfGlobalVariable(dtgs->left_var);
            int right_var = getLocalIndexOfGlobalVariable(dtgs->right_var);
            if(left_var != -1 && right_var != -1) {
                assert(children_in_cg[i].size() == 0);
                children_in_cg[i].push_back(left_var);
                children_in_cg[i].push_back(right_var);
                children_in_cg[i].push_back(dtgs->op);
            }
        } else if(g_variable_types[context_variable] == comparison) {
            DomainTransitionGraph *dtg = g_transition_graphs[context_variable];
            DomainTransitionGraphComp *dtgc =
                dynamic_cast<DomainTransitionGraphComp*> (dtg);
            assert(dtgc);
            int left_var = getLocalIndexOfGlobalVariable(
                    dtgc->nodes.first.left_var);
            int right_var = getLocalIndexOfGlobalVariable(
                    dtgc->nodes.first.right_var);
            if(left_var != -1 && right_var != -1) {
                assert(children_in_cg[i].size() == 0);
                children_in_cg[i].push_back(left_var);
                children_in_cg[i].push_back(right_var);
                children_in_cg[i].push_back(dtgc->nodes.first.op);
            }
        }
    }
}

LocalProblemComp::LocalProblemComp(CyclicCGHeuristic* _owner, int the_var_no, int the_start_value) :
    LocalProblem(_owner, the_var_no, the_start_value)
{
    assert(var_no >= 0);
    build_nodes_for_variable(var_no, the_start_value);

    int parents_num = causal_graph_parents->size();
    nodes[start_value].children_state.resize(parents_num, 0.0);

    depending_vars.resize(parents_num);
    children_in_cg.resize(parents_num);
    buildDependingVars(parents_num);
}

void LocalProblemComp::build_nodes_for_variable(int var_no, int the_start_value)
{
    if(!(g_variable_types[var_no] == comparison)) {
        cout << "variable type: " << g_variable_types[var_no] << endl;
        assert(false);
    }
    DomainTransitionGraph *dtg = g_transition_graphs[var_no];
    DomainTransitionGraphComp *dtgc =
        dynamic_cast<DomainTransitionGraphComp *> (dtg);
    assert(dtgc);

    causal_graph_parents = &dtgc->ccg_parents;
    global_to_local_parents = &dtgc->global_to_local_ccg_parents;

    int num_parents = causal_graph_parents->size();

    assert(g_variable_domain[var_no] == 3);
    // There are 3 values for a comp variable: false, true and undefined. In the heuristic we only have
    // to deal with the first both of them.
    nodes.push_back(LocalProblemNodeComp(this, num_parents, 0, dtgc->nodes.second.op));
    nodes.push_back(LocalProblemNodeComp(this, num_parents, 1, dtgc->nodes.first.op));

    // Compile the DTG arcs into LocalTransition objects.
    for(int i = 0; i < dtgc->transitions.size(); i++) {
        LocalTransitionComp trans(&(dtgc->transitions[i]),
                &(nodes[the_start_value]), &(nodes[the_start_value - 1]));
        //variables in func_transitions of comp nodes are local!!!!
        trans.duration_var_local = dtgc->transitions[i].duration_variable;
        nodes[the_start_value].outgoing_transitions.push_back(trans);
    }
}

void LocalProblemComp::initialize(double base_priority_, int start_value,
        const TimeStampedState &state)
{
    assert(start_value == this->start_value);

    int target_value = abs(start_value - 1);

    assert(!is_initialized());
    base_priority = base_priority_;

    assert(nodes.size() == 2);

    for(int to_value = 0; to_value < nodes.size(); to_value++) {
        nodes[to_value].expanded = false;
        nodes[to_value].opened = false;
        nodes[to_value].reached_by = NULL;
        nodes[to_value].pred = NULL;
        nodes[to_value].waiting_list.clear();
        nodes[to_value].nodes_where_this_subscribe.clear();
        nodes[to_value].bestTransition = NULL;
        nodes[to_value].reached_by_wait_for = -1.0;
    }

    nodes[start_value].cost = 0;
    nodes[target_value].cost = LocalProblem::QUITE_A_LOT;

    for(int i = 0; i < nodes[start_value].outgoing_transitions.size(); i++) {
        LocalTransitionComp &trans = nodes[start_value].outgoing_transitions[i];
        trans.target_cost = 0;
        for(int j = 0; j < trans.label->precond.size(); j++) {
            trans.conds_satiesfied[j] = false;
        }
    }

    int parents_num = causal_graph_parents->size();
    for(int i = 0; i < parents_num; i++) {
        int var = (*causal_graph_parents)[i];
        nodes[start_value].children_state[i] = state[var];
    }
    owner->add_to_queue(&nodes[start_value]);
}

CyclicCGHeuristic::CyclicCGHeuristic(Mode _mode) : mode(_mode)
{
    goal_problem = 0;
    goal_node = 0;
}

CyclicCGHeuristic::~CyclicCGHeuristic()
{
    delete goal_problem;
    for(int i = 0; i < local_problems.size(); i++)
        delete local_problems[i];
}

void CyclicCGHeuristic::initialize()
{
    assert(goal_problem == 0);
    cout << "Initializing cyclic causal graph heuristic...";

    int num_variables = g_variable_domain.size();

    goal_problem = new LocalProblemDiscrete(this, -1, 0);
    goal_node = &goal_problem->nodes[1];

    local_problem_index.resize(num_variables);
    for(int var_no = 0; var_no < num_variables; var_no++) {
        int num_values = g_variable_domain[var_no];
        if(num_values == -1) {
            //we don't need local problems for functional variables so far....
            assert(g_variable_types[var_no] == subterm_functional || g_variable_types[var_no] == primitive_functional);
        } else {
            local_problem_index[var_no].resize(num_values, NULL);
        }
    }
    prefOpsSortedByCorrespondigGoal.resize(g_goal.size());
    costsOfGoals.resize(g_goal.size());
    cout << "done." << endl;
}

void CyclicCGHeuristic::reset_pref_ops_from_cache(double heuristic, const TimeStampedState &state) {
    if(heuristic != DEAD_END && heuristic != 0) {
        PrefOpsCache::const_iterator it = cachePrefOps.find(state);
        assert(it != cachePrefOps.end());
        prefOpsSortedByCorrespondigGoal = it->second;

        CostsToGoalCache::const_iterator it2 = cacheCostsToGoal.find(state);
        assert(it2 != cacheCostsToGoal.end());
        costsOfGoals = it2->second;

        set_specific_pref_ops(state);
    }
}

void CyclicCGHeuristic::compute_pref_ops(double heuristic, const TimeStampedState &state) {
    if(heuristic != DEAD_END && heuristic != 0) {
        if(g_parameters.pref_ops_cheapest_mode || g_parameters.pref_ops_most_expensive_mode ||
                g_parameters.pref_ops_ordered_mode || g_parameters.pref_ops_rand_mode) {
            assert(g_parameters.number_pref_ops_cheapest_mode +
                    g_parameters.number_pref_ops_most_expensive_mode +
                    g_parameters.number_pref_ops_ordered_mode +
                    g_parameters.number_pref_ops_rand_mode > 0);
            goal_node->setCostsOfGoals(state);
        }
        cacheCostsToGoal[state] = costsOfGoals;
        for(unsigned int i = 0; i < prefOpsSortedByCorrespondigGoal.size(); ++i) {
            prefOpsSortedByCorrespondigGoal[i].clear();
        }
        goal_node->mark_helpful_transitions(state,0);
        set_specific_pref_ops(state);
    }
}

void CyclicCGHeuristic::set_specific_pref_ops(const TimeStampedState &state) {
    vector<double> temp_costsOfGoals = costsOfGoals;
    if(g_parameters.pref_ops_ordered_mode) {
        setFirstPrefOpsExternally(g_parameters.number_pref_ops_ordered_mode);
    }
    if(g_parameters.pref_ops_cheapest_mode) {
        setCheapestPrefOpsExternally(g_parameters.number_pref_ops_cheapest_mode);
    }
    costsOfGoals = temp_costsOfGoals;
    if(g_parameters.pref_ops_most_expensive_mode) {
        setMostExpensivePrefOpsExternally(g_parameters.number_pref_ops_most_expensive_mode);
    }
    costsOfGoals = temp_costsOfGoals;
    if(g_parameters.pref_ops_rand_mode) {
        setRandPrefOpsExternally(g_parameters.number_pref_ops_rand_mode);
    }
    if(g_parameters.pref_ops_concurrent_mode) {
        setConcurrentPrefOpsExternally(state);
    }
    setAllPrefOpsExternally();
}

double CyclicCGHeuristic::compute_heuristic(const TimeStampedState &state)
{
    if(state.satisfies(g_goal) && state.operators.empty()) {
        return 0.0;
    }

    double heuristic = DEAD_END;

    if(g_parameters.use_caching_in_heuristic) {
        HeuristicValueCache::const_iterator it = cacheHeuristicValue.find(state);
        if(it != cacheHeuristicValue.end()) {
            num_cache_hits++;
            heuristic = cacheHeuristicValue[state];
            reset_pref_ops_from_cache(heuristic, state);
        } else {
            heuristic = compute_costs(state);
            compute_pref_ops(heuristic, state);
            cacheHeuristicValue[state] = heuristic;
            cachePrefOps[state] = prefOpsSortedByCorrespondigGoal;
        }
    } else {
        heuristic = compute_costs(state);
        compute_pref_ops(heuristic, state);
    }

    double scheduledPlanMakespan = 0.0;
    if(heuristic != DEAD_END && !(mode == COST || mode == CEA)) {
        scheduledPlanMakespan = computeScheduledPlanMakespan(state);
    }

    if(heuristic == DEAD_END) {
        return DEAD_END;
    }

    if(mode == COST) {
        return heuristic;
    } else if(mode == REMAINING_MAKESPAN) {
        return scheduledPlanMakespan;
    } else if(mode == SUFFIX_MAKESPAN) {
        double longestRunningAction = 0.0;
        for(int i = 0; i < state.operators.size(); ++i) {
            if(state.operators[i].time_increment > longestRunningAction) {
                longestRunningAction = state.operators[i].time_increment;
            }
        }

        assert(longestRunningAction <= scheduledPlanMakespan);
        scheduledPlanMakespan -= longestRunningAction;
        if(!state.operators.empty() && (double_equals(scheduledPlanMakespan, 0.0))) {
            scheduledPlanMakespan = 0.9;
        }
        return scheduledPlanMakespan;
    } else if(mode == WEIGHTED) {
        assert(heuristic != DEAD_END);
        if(get_waiting_time() - EPSILON > 0.0) {
            heuristic += get_waiting_time();
        }
        double longestRunningAction = 0.0;
        for(int i = 0; i < state.operators.size(); ++i) {
            if(state.operators[i].time_increment > longestRunningAction) {
                longestRunningAction = state.operators[i].time_increment;
            }
        }
        assert(longestRunningAction <= scheduledPlanMakespan);
        scheduledPlanMakespan -= longestRunningAction;
        return scheduledPlanMakespan + 0.2 * heuristic;
    }

    assert(mode == CEA);
    assert(heuristic != DEAD_END);
    if(!state.operators.empty() && (heuristic == 0.0)) {
        heuristic += 0.9;
    }
    if(get_waiting_time() - EPSILON > 0.0) {
        heuristic += get_waiting_time();
    }
    return heuristic;
}
        
double CyclicCGHeuristic::computeScheduledPlanMakespan(const TimeStampedState & state) const
{
    set<CausalConstraint> constraints;
    vector<TimedOp> needed_ops;
    for(int i = 0; i < state.operators.size(); ++i) {
        needed_ops.push_back(tr1::make_tuple(&state.operators[i],
                    state.operators[i].time_increment, i));
    }
    set<const Operator*> labels;
    goal_problem->generate_causal_constraints(goal_node, constraints, needed_ops,
            state, labels);

    // constraints for running actions
    for(int i = 0; i < state.operators.size(); ++i) {
        for(int j = state.operators.size(); j < needed_ops.size(); ++j) {
            if(tr1::get<0>(needed_ops[i])->isDisabledBy(tr1::get<0>(
                            needed_ops[j])) || tr1::get<0>(needed_ops[i])->enables(
                            tr1::get<0>(needed_ops[j]))) {
                constraints.insert(make_pair(i, j));
            }
        }
    }

    // build and solve STN...
    vector<string> variable_names;//(needed_ops.size()*2);
    for(int i = 0; i < needed_ops.size(); ++i) {
        variable_names.push_back(tr1::get<0>(needed_ops[i])->get_name()
                + "s");
    }
    for(int i = 0; i < needed_ops.size(); ++i) {
        variable_names.push_back(tr1::get<0>(needed_ops[i])->get_name()
                + "e");
    }
    SimpleTemporalProblem stn(variable_names);

    // assert that start time point of actions are non-negative
    for(int i = 0; i < needed_ops.size(); ++i) {
        stn.setUnboundedIntervalFromXZero(i, 0.0);
        // assert that differences between start and end time points are exactly
        // the durations of the actions
        stn.setSingletonInterval(i, i + needed_ops.size(), tr1::get<1>(
                    needed_ops[i]));
    }

    // assert that causal relationships are preserved
    for(set<CausalConstraint>::iterator it = constraints.begin(); it
            != constraints.end(); ++it) {
        stn.setUnboundedInterval(needed_ops.size() + it->first, it->second,
                EPSILON);
    }

    stn.setCurrentDistancesAsDefault();
    bool isValid = stn.solveWithP3C();
    assert(isValid);
    vector<Happening> happenings = stn.getHappenings(true);
    if(!stn.solution_is_valid()) {
        cout << "Ops:" << endl;
        for(int i = 0; i < needed_ops.size(); ++i) {
            cout << i << ": " << tr1::get<0>(needed_ops[i])->get_name()
                << ", duration: " << tr1::get<1>(needed_ops[i]) << endl;
        }
        cout << "Constraints:" << endl;
        set<CausalConstraint>::iterator it;
        for(it = constraints.begin(); it != constraints.end(); ++it) {
            cout << it->first << " <<< " << it->second << endl;
        }
    }

    assert(stn.solution_is_valid());

    double heuristicNew = stn.getMaximalTimePointInTightestSchedule(true);
    assert(heuristicNew >= 0.0);
    return heuristicNew;
}

void CyclicCGHeuristic::initialize_queue()
{
    open_nodes = node_queue();
}

void CyclicCGHeuristic::add_to_queue(LocalProblemNode *node)
{
    open_nodes.push(node);
}

LocalProblemNode* CyclicCGHeuristic::remove_from_queue()
{
    LocalProblemNode* ret = open_nodes.top();
    open_nodes.pop();
    return ret;
}

double CyclicCGHeuristic::compute_costs(const TimeStampedState &state)
{
    initialize_queue();
    set_waiting_time(REALLYSMALL);
    goal_problem->base_priority = -1;
    for(int i = 0; i < local_problems.size(); i++)
        local_problems[i]->base_priority = -1;

    goal_problem->initialize(0.0, 0, state);
    while(!open_nodes.empty()) {
        LocalProblemNode *node = remove_from_queue();
        if(node == goal_node)
            return node->cost;
        if(!(node->expanded)) {
            node->on_expand(state);
        }
    }
    return DEAD_END;
}
