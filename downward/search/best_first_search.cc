#include "best_first_search.h"

#include "globals.h"
#include "heuristic.h"
#include "successor_generator.h"
#include "plannerParameters.h"
#include <time.h>
#include <iomanip>

#include <cassert>
#include <cmath>

using namespace std;

OpenListInfo::OpenListInfo(Heuristic *heur, bool only_pref, OpenListMode _mode)
{
    heuristic = heur;
    only_preferred_operators = only_pref;
    mode = _mode;
    priority = 0;
}

void BestFirstSearchEngine::reset() {
    cout << "RESET!" << endl;
    closed_list.clear();
    numberOfSearchSteps = 0;
    lastProgressAtExpansionNumber = 0;
    assert(activeQueue < open_lists.size());
    for(unsigned int i = 0; i < open_lists.size(); ++i) {
        open_lists[i].priority = 0;
    }
    current_state = *g_initial_state;
    current_predecessor = NULL;
    current_operators.clear();
    for(unsigned int i = 0; i < open_lists.size(); ++i) {
        open_lists[i].open = OpenList();
    }
    for(unsigned int i = 0; i < best_heuristic_values_of_queues.size(); ++i) {
        best_heuristic_values_of_queues[i] = -1;
    }
    queueStartedLastWith = (queueStartedLastWith+1) % open_lists.size();
    assert(queueStartedLastWith < open_lists.size());
    if(queueStartedLastWith == 0) {
        cout << "Switching to round robin mode. " << endl;
        mode = ROUND_ROBIN;
    } else {
        cout << "Giving prior boost to open list " << queueStartedLastWith << endl;
        open_lists[queueStartedLastWith].priority -= 5000;
    }
    cout << "Open list sizes (priorities):";
    for(unsigned int i = 0; i < open_lists.size(); ++i) {
        cout << " " << open_lists[i].open.size() << " (" << open_lists[i].priority << "), ";
    }
    cout << endl;
}

BestFirstSearchEngine::BestFirstSearchEngine(QueueManagementMode _mode) :
        number_of_expanded_nodes(0), current_state(*g_initial_state), mode(_mode)
{
    current_predecessor = 0;
    start_time = time(NULL);
    bestMakespan = HUGE_VAL;
    bestSumOfGoals = HUGE_VAL;
    queueStartedLastWith = 0;
}

BestFirstSearchEngine::~BestFirstSearchEngine()
{
}

void BestFirstSearchEngine::add_heuristic(Heuristic *heuristic,
        bool use_estimates, bool use_preferred_operators,
        bool pref_ops_cheapest_mode, bool pref_ops_most_expensive_mode,
        bool pref_ops_ordered_mode,  bool pref_ops_rand_mode, bool pref_ops_concurrent_mode)
{
    assert(use_estimates || use_preferred_operators);
    if(pref_ops_ordered_mode) {
        best_heuristic_values_of_queues.push_back(-1);
        preferred_operator_heuristics_ordered.push_back(heuristic);
        open_lists.push_back(OpenListInfo(heuristic, true, ORDERED));
    }
    if(pref_ops_cheapest_mode) {
        best_heuristic_values_of_queues.push_back(-1);
        preferred_operator_heuristics_cheapest.push_back(heuristic);
        open_lists.push_back(OpenListInfo(heuristic, true, CHEAPEST));
    }
    if(pref_ops_most_expensive_mode) {
        best_heuristic_values_of_queues.push_back(-1);
        preferred_operator_heuristics_most_expensive.push_back(heuristic);
        open_lists.push_back(OpenListInfo(heuristic, true, MOSTEXPENSIVE));
    }
    if(pref_ops_rand_mode) {
        best_heuristic_values_of_queues.push_back(-1);
        preferred_operator_heuristics_rand.push_back(heuristic);
        open_lists.push_back(OpenListInfo(heuristic, true, RAND));
    }
    if(pref_ops_concurrent_mode) {
        best_heuristic_values_of_queues.push_back(-1);
        preferred_operator_heuristics_concurrent.push_back(heuristic);
        open_lists.push_back(OpenListInfo(heuristic, true, CONCURRENT));
    }
    if(use_preferred_operators) {
        best_heuristic_values_of_queues.push_back(-1);
        preferred_operator_heuristics_reg.push_back(heuristic);
        open_lists.push_back(OpenListInfo(heuristic, true, REGULAR));
    }
    heuristics.push_back(heuristic);
    best_heuristic_values_of_queues.push_back(-1);
    if(use_estimates) {
        open_lists.push_back(OpenListInfo(heuristic, false, ALL));
    }
}

void BestFirstSearchEngine::initialize()
{
    cout << "INIT" << endl;
    assert(!open_lists.empty());
    activeQueue = 0;
    lastProgressAtExpansionNumber = 0;
    numberOfSearchSteps = 0;
    cout << "Open list sizes (priorities):";
    for(unsigned int i = 0; i < open_lists.size(); ++i) {
        cout << " " << open_lists[i].open.size() << " (" << open_lists[i].priority << "), ";
    }
    cout << endl;
}

void BestFirstSearchEngine::statistics(time_t & current_time)
{
    cout << endl;
    cout << "Search Time: " << (current_time - start_time) << " sec." << endl;
    search_statistics.dump(number_of_expanded_nodes, current_time);
    cout << "OpenList sizes:";
    for(unsigned int i = 0; i < open_lists.size(); ++i) {
        cout << " " << open_lists[i].open.size();
    }
    cout << endl;
    cout << "Heuristic Computations (per heuristic):";
    unsigned long totalHeuristicComputations = 0;
    for(unsigned int i = 0; i < heuristics.size(); ++i) {
        Heuristic *heur = heuristics[i];
        totalHeuristicComputations += heur->get_num_computations();
        cout << " " << heur->get_num_computations();
    }
    cout << " Total: " << totalHeuristicComputations << endl;
    cout << "Number of cache hits in heuristic (per heuristic):";
    unsigned long totalHeuristicCacheHits = 0;
    for(unsigned int i = 0; i < heuristics.size(); ++i) {
        Heuristic *heur = heuristics[i];
        totalHeuristicCacheHits += heur->get_num_cache_hits();
        cout << " " << heur->get_num_cache_hits();
    }
    cout << " Total: " << totalHeuristicCacheHits << endl;

    cout << "Best heuristic values of queues:";
    for(unsigned int i = 0; i < best_heuristic_values_of_queues.size(); ++i) {
        cout << " [" << i << ": " << best_heuristic_values_of_queues[i] << "]";
    }
    cout << endl << endl;

}

void BestFirstSearchEngine::dump_transition() const
{
    cout << endl;
    if(current_predecessor != 0) {
        cout << "DEBUG: In step(), current predecessor is: " << endl;
        current_predecessor->dump(g_parameters.verbose);
    }
    cout << "DEBUG: In step(), current operators are: ";
    for(unsigned int i = 0; i < current_operators.size(); ++i) {
        cout << "  " << current_operators[i]->get_name();
    }
    cout << endl;
    cout << "DEBUG: In step(), current state is: " << endl;
    current_state.dump(g_parameters.verbose);
    cout << endl;
}

void BestFirstSearchEngine::dump_everything() const
{
    dump_transition();
    cout << endl << endl;
    for (std::vector<OpenListInfo>::const_iterator it = open_lists.begin(); it
            != open_lists.end(); it++) {
        cout << "DEBUG: an open list is:" << endl;
        // huge dangerous hack...
        OpenListEntry const* begin = &(it->open.top());
        OpenListEntry const* end = &(it->open.top()) + it->open.size();
        for (const OpenListEntry* it2 = begin; it2 != end; it2++) {
            cout << "OpenListEntry" << endl;
            cout << "state" << endl;
            std::tr1::get<0>(*it2)->dump(true);
            cout << "ops: ";
            const vector<const Operator*>& ops = std::tr1::get<1>(*it2);
            for(unsigned int i = 0; i < ops.size(); ++i) {
                cout << "  " << ops[i]->get_name();
            }
            cout << endl;
            double h = std::tr1::get<2>(*it2);
            cout << "Value: " << h << endl;
            cout << "end OpenListEntry" << endl;
        }
    }
}

SearchEngine::status BestFirstSearchEngine::step()
{
    // Invariants:
    // - current_state is the next state for which we want to compute the heuristic.
    // - current_predecessor is a permanent pointer to the predecessor of that state.
    // - current_operator is the operator which leads to current_state from predecessor.


    numberOfSearchSteps++;
    if(g_parameters.reset_after_solution_was_found && (mode == PRIORITY_BASED)
            && (numberOfSearchSteps - lastProgressAtExpansionNumber) > (5000-1)) {
        cout << "No progress since " << lastProgressAtExpansionNumber << ", now at " << numberOfSearchSteps << endl;
        reset();
    }

    bool discard = true;

    double maxTimeIncrement = 0.0;
    for(int k = 0; k < current_state.operators.size(); ++k) {
        maxTimeIncrement = max(maxTimeIncrement, current_state.operators[k].time_increment);
    }
    double makeSpan = maxTimeIncrement + current_state.timestamp;

    // when using subgoals we want to keep states with the same makespan as they might have better subgoals
    if(g_parameters.use_subgoals_to_break_makespan_ties) {
        if (makeSpan <= bestMakespan && !closed_list.contains(current_state)) {
            discard = false;
        }
    } else {
        if (makeSpan < bestMakespan && !closed_list.contains(current_state)) {
            discard = false;
        }
    }

    // throw away any states resulting from zero cost actions (can't handle)
    for(unsigned int i = 0; i < current_operators.size(); ++i) {
        if(current_predecessor && current_operators[i] &&
                current_operators[i] != g_let_time_pass &&
                current_operators[i]->get_duration(current_predecessor) <= 0.0) {
            discard = true;
            break;
        }
    }

    if(!discard) {
        const TimeStampedState* parent_ptr = NULL;
        if(current_operators.size() == 0) {
            assert(current_predecessor != & current_state);
            number_of_expanded_nodes++;
            parent_ptr = closed_list.insert(current_state,
                    current_predecessor, NULL);
        } else {
            for(unsigned int i = 0; i < current_operators.size(); ++i) {
                assert(current_state.is_consistent_when_progressed());
                assert(current_operators[i]->get_name().compare("wait") != 0);
                assert(current_predecessor != &current_state);
                if(i>0) { //first operator has been applied in fetch_next_state()
                	assert(activeQueue < open_lists.size());
                    assert(g_parameters.pref_ops_concurrent_mode && open_lists[activeQueue].mode == CONCURRENT);
                    if(!current_operators[i]->is_applicable(current_state)) {
                        continue;
                    }
                    current_predecessor = new TimeStampedState(current_state);

                    current_state = TimeStampedState(current_state, *current_operators[i]);
                    assert(current_state.is_consistent_when_progressed());
                }
                assert(current_predecessor != &current_state);
                number_of_expanded_nodes++;
                parent_ptr = closed_list.insert(current_state, current_predecessor, current_operators[i]);
            }
        }

        // evaluate the current/parent state
        // also do this for non/lazy-evaluation as the heuristics
        // provide preferred operators!
        for(int i = 0; i < heuristics.size(); i++) {
            heuristics[i]->evaluate(current_state);
        }
        if(!is_dead_end()) {
            if(check_progress()) {
                // current_state.dump();
                report_progress();
                reward_progress();
            }
            if(check_goal())
                return SOLVED;
            generate_successors(parent_ptr);
        }
    } else if ((current_operators.size() == 1) && (current_operators[0] == g_let_time_pass) &&
            current_state.operators.empty() &&
            makeSpan < bestMakespan) {
        // arrived at same state by letting time pass
        // e.g. when having an action with duration and only start effect
        // the result would be discarded, check if we are at goal    
        for(int i = 0; i < heuristics.size(); i++) {
            heuristics[i]->evaluate(current_state);
        }
        if(!is_dead_end()) {
            if(check_goal())
                return SOLVED;
        }
    }

    time_t current_time = time(NULL);
    static time_t last_stat_time = current_time;
    if(g_parameters.verbose && current_time - last_stat_time >= 10) {
        statistics(current_time);
        last_stat_time = current_time;
    }

    // use different timeouts depending if we found a plan or not.
    if(found_solution()) {
        if (g_parameters.timeout_if_plan_found > 0 
                && current_time - start_time > g_parameters.timeout_if_plan_found) {
            if(g_parameters.verbose)
                statistics(current_time);
            return SOLVED_TIMEOUT;
        }
    } else {
        if (g_parameters.timeout_while_no_plan_found > 0 
                && current_time - start_time > g_parameters.timeout_while_no_plan_found) {
            if(g_parameters.verbose)
                statistics(current_time);
            return FAILED_TIMEOUT;
        }
    }

    return fetch_next_state();
}

bool BestFirstSearchEngine::is_dead_end()
{
    // If a reliable heuristic reports a dead end, we trust it.
    // Otherwise, all heuristics must agree on dead-end-ness.
    int dead_end_counter = 0;
    for(int i = 0; i < heuristics.size(); i++) {
        if(heuristics[i]->is_dead_end()) {
            if(heuristics[i]->dead_ends_are_reliable())
                return true;
            else
                dead_end_counter++;
        }
    }
    return dead_end_counter == heuristics.size();
}

bool BestFirstSearchEngine::check_goal()
{
    // Any heuristic reports 0 iff this is a goal state, so we can
    // pick an arbitrary one. Heuristics are assumed to not miss a goal
    // state and especially not to report a goal as dead end.
    Heuristic *heur = open_lists[0].heuristic;
 
    // We actually need this silly !heur->is_dead_end() check because
    // this state *might* be considered a non-dead end by the
    // overall search even though heur considers it a dead end
    // (e.g. if heur is the CG heuristic, but the FF heuristic is
    // also computed and doesn't consider this state a dead end.
    // If heur considers the state a dead end, it cannot be a goal
    // state (heur will not be *that* stupid). We may not call
    // get_heuristic() in such cases because it will barf.
    if(!heur->is_dead_end() && heur->get_heuristic() == 0) {
        if(current_state.operators.size() > 0) {
            return false;
        }
        // found goal

        if(!current_state.satisfies(g_goal)) {  // will assert...
            dump_everything();
        }
        assert(current_state.operators.empty() && current_state.satisfies(g_goal));

        Plan plan;
        PlanTrace path;
        closed_list.trace_path(current_state, plan, path);
        set_plan(plan);
        set_path(path);
        return true;
    } else {
        return false;
    }
}

void BestFirstSearchEngine::dump_plan_prefix_for_current_state() const
{
    dump_plan_prefix_for__state(current_state);
}

void BestFirstSearchEngine::dump_plan_prefix_for__state(const TimeStampedState &state) const
{
    Plan plan;
    PlanTrace path;
    closed_list.trace_path(state, plan, path);
    for(int i = 0; i < plan.size(); i++) {
        const PlanStep& step = plan[i];
        cout << step.start_time << ": " << "(" << step.op->get_name() << ")"
            << " [" << step.duration << "]" << endl;
    }
}

bool BestFirstSearchEngine::check_progress()
{
    if(g_parameters.reward_only_pref_op_queue) {
        bool progress = false;
        for(int i = 0; i < heuristics.size(); i++) {
            if(heuristics[i]->is_dead_end())
                continue;
            double h = heuristics[i]->get_heuristic();
            assert(best_heuristic_values_of_queues.size() >= 1);
            double &best_h = best_heuristic_values_of_queues[0];
            if(best_h == -1 || h < best_h) {
                best_h = h;
                progress = true;
            }
        }
        return progress;
    } else {
        assert(activeQueue < best_heuristic_values_of_queues.size());
        assert(activeQueue < open_lists.size());
        Heuristic* heuristic = open_lists[activeQueue].heuristic;
        if(heuristic->is_dead_end())
            return false;
        double h = heuristic->get_heuristic();
        double &best_h = best_heuristic_values_of_queues[activeQueue];
        if(best_h == -1 || h < best_h) {
            best_h = h;
            return true;
        }
        return false;
    }
}

void BestFirstSearchEngine::report_progress()
{
    cout << "Best heuristic values of queues: ";
    for(int i = 0; i < best_heuristic_values_of_queues.size(); i++) {
        cout << best_heuristic_values_of_queues[i];
        if(i != best_heuristic_values_of_queues.size() - 1)
            cout << "/";
    }
    cout << " [expanded " << closed_list.size() << " state(s)]" << endl;
}

void BestFirstSearchEngine::reward_progress()
{
    // Boost the "preferred operator" open lists somewhat whenever
    // progress is made. This used to be used in multi-heuristic mode
    // only, but it is also useful in single-heuristic mode, at least
    // in Schedule.
    //
    // Future Work: Test the impact of this, and find a better way of rewarding
    // successful exploration. For example, reward only the open queue
    // from which the good state was extracted and/or the open queues
    // for the heuristic for which a new best value was found.

    if(g_parameters.reward_only_pref_op_queue) {
        for(int i = 0; i < open_lists.size(); i++)
            if(open_lists[i].mode == REGULAR)
                open_lists[i].priority -= 1000;
    } else {
        assert(activeQueue < open_lists.size());
        open_lists[activeQueue].priority -= 1000;
        lastProgressAtExpansionNumber = numberOfSearchSteps;
    }
}

bool knownByLogicalStateOnly(LogicalStateClosedList& scl, const TimedSymbolicStates& timedSymbolicStates)
{
    // feature disabled -> return false = state unkown -> insert
    if(!g_parameters.use_known_by_logical_state_only)
       return false;
    assert(timedSymbolicStates.size() > 0);
    bool ret = true;
    for (int i = 0; i < timedSymbolicStates.size(); ++i) {
        if (scl.count(timedSymbolicStates[i].first) > 0) {
            double currentBestMakespan = scl[timedSymbolicStates[i].first];
            if (timedSymbolicStates[i].second + EPSILON < currentBestMakespan) {
                ret = false;
                scl[timedSymbolicStates[i].first] = timedSymbolicStates[i].second;
            }
        } else {
            ret = false;
            scl[timedSymbolicStates[i].first] = timedSymbolicStates[i].second;
        }
    }
    return ret;
}

void BestFirstSearchEngine::generate_successors(const TimeStampedState *parent_ptr)
{
    vector<const Operator *> all_operators;
    g_successor_generator->generate_applicable_ops(*parent_ptr, all_operators);
    // Filter ops that cannot be applicable just from the preprocess data (doesn't guarantee full applicability)

    vector<const Operator *> preferred_operators_reg;
    for(int i = 0; i < preferred_operator_heuristics_reg.size(); i++) {
        Heuristic *heur = preferred_operator_heuristics_reg[i];
        if(!heur->is_dead_end()) {
            heur->get_preferred_operators(preferred_operators_reg,REGULAR);
        }
    }

    vector<const Operator *> preferred_operators_ordered;
    for(int i = 0; i < preferred_operator_heuristics_ordered.size(); i++) {
        Heuristic *heur = preferred_operator_heuristics_ordered[i];
        if(!heur->is_dead_end()) {
            heur->get_preferred_operators(preferred_operators_ordered,ORDERED);
        }
    }

    vector<const Operator *> preferred_operators_cheapest;
    for(int i = 0; i < preferred_operator_heuristics_cheapest.size(); i++) {
        Heuristic *heur = preferred_operator_heuristics_cheapest[i];
        if(!heur->is_dead_end()) {
            heur->get_preferred_operators(preferred_operators_cheapest,CHEAPEST);
        }
    }

    vector<const Operator *> preferred_operators_most_expensive;
    for(int i = 0; i < preferred_operator_heuristics_most_expensive.size(); i++) {
        Heuristic *heur = preferred_operator_heuristics_most_expensive[i];
        if(!heur->is_dead_end()) {
            heur->get_preferred_operators(preferred_operators_most_expensive,MOSTEXPENSIVE);
        }
    }

    vector<const Operator *> preferred_operators_rand;
    for(int i = 0; i < preferred_operator_heuristics_rand.size(); i++) {
        Heuristic *heur = preferred_operator_heuristics_rand[i];
        if(!heur->is_dead_end()) {
            heur->get_preferred_operators(preferred_operators_rand,RAND);
        }
    }

    vector<const Operator *> preferred_operators_concurrent;
    //    cout << "preferred_operator_heuristics_concurrent.size(): " << preferred_operator_heuristics_concurrent.size() << endl;
    for(int i = 0; i < preferred_operator_heuristics_concurrent.size(); i++) {
        Heuristic *heur = preferred_operator_heuristics_concurrent[i];
        if(!heur->is_dead_end()) {
            heur->get_preferred_operators(preferred_operators_concurrent,CONCURRENT);
        }
    }

    vector<const Operator *> preferred_applicable_operators_reg;
    vector<const Operator *> preferred_applicable_operators_ordererd;
    vector<const Operator *> preferred_applicable_operators_cheapest;
    vector<const Operator *> preferred_applicable_operators_most_expensive;
    vector<const Operator *> preferred_applicable_operators_rand;
    vector<const Operator *> preferred_applicable_operators_concurrent;
    // all_operators contains a superset of applicable operators based on preprocess data
    // this in not necessarily all operators in the task, but also doesn't guarantee applicablity, yet.
    // Here we split this superset of operators in two:
    // - preferred_applicable_operators will be those operators from all_operators that are preferred_operators
    // - all_operators will be the rest (i.e. the non-preferred operators)
    // preferred_applicable_operators thus contains all operators that are preferred and might be applicable,
    // i.e. a subset of preferred_operators
    // The preferred_operators to be used will thus be only the preferred_applicable_operators
    for(int k = 0; k < preferred_operators_reg.size(); ++k) {
        for(int l = 0; l < all_operators.size(); ++l) {
            if(all_operators[l] == preferred_operators_reg[k]) {
                preferred_applicable_operators_reg.push_back(preferred_operators_reg[k]);
                break;
            }
        }
    }
    preferred_operators_reg = preferred_applicable_operators_reg;
    for(int k = 0; k < preferred_operators_ordered.size(); ++k) {
        for(int l = 0; l < all_operators.size(); ++l) {
            if(all_operators[l] == preferred_operators_ordered[k]) {
                preferred_applicable_operators_ordererd.push_back(preferred_operators_ordered[k]);
                break;
            }
        }
    }
    preferred_operators_ordered = preferred_applicable_operators_ordererd;

    for(int k = 0; k < preferred_operators_cheapest.size(); ++k) {
        for(int l = 0; l < all_operators.size(); ++l) {
            if(all_operators[l] == preferred_operators_cheapest[k]) {
                preferred_applicable_operators_cheapest.push_back(preferred_operators_cheapest[k]);
                break;
            }
        }
    }
    preferred_operators_cheapest = preferred_applicable_operators_cheapest;

    for(int k = 0; k < preferred_operators_most_expensive.size(); ++k) {
        for(int l = 0; l < all_operators.size(); ++l) {
            if(all_operators[l] == preferred_operators_most_expensive[k]) {
                preferred_applicable_operators_most_expensive.push_back(preferred_operators_most_expensive[k]);
                break;
            }
        }
    }
    preferred_operators_most_expensive = preferred_applicable_operators_most_expensive;

    for(int k = 0; k < preferred_operators_rand.size(); ++k) {
        for(int l = 0; l < all_operators.size(); ++l) {
            if(all_operators[l] == preferred_operators_rand[k]) {
                preferred_applicable_operators_rand.push_back(preferred_operators_rand[k]);
                break;
            }
        }
    }
    preferred_operators_rand = preferred_applicable_operators_rand;

    for(int k = 0; k < preferred_operators_concurrent.size(); ++k) {
        for(int l = 0; l < all_operators.size(); ++l) {
            if(all_operators[l] == preferred_operators_concurrent[k]) {
                preferred_applicable_operators_concurrent.push_back(preferred_operators_concurrent[k]);
                break;
            }
        }
    }
    preferred_operators_concurrent = preferred_applicable_operators_concurrent;

    for(int l = 0; l < all_operators.size(); ++l) {
    	bool deleted = false;
    	for(int k = 0; k < preferred_operators_ordered.size(); ++k) {
    		if(all_operators[l] == preferred_operators_ordered[k]) {
    			all_operators[l] = all_operators[all_operators.size() - 1];
    			all_operators.pop_back();
    			deleted = true;
    			break;
    		}
        }
    	if(deleted) {
    		continue;
    	}
    	for(int k = 0; k < preferred_operators_reg.size(); ++k) {
    		if(all_operators[l] == preferred_operators_reg[k]) {
    			all_operators[l] = all_operators[all_operators.size() - 1];
    			all_operators.pop_back();
    			deleted = true;
    			break;
    		}
        }
    	if(deleted) {
    		continue;
    	}
    	for(int k = 0; k < preferred_operators_concurrent.size(); ++k) {
    		if(all_operators[l] == preferred_operators_concurrent[k]) {
    			all_operators[l] = all_operators[all_operators.size() - 1];
    			all_operators.pop_back();
    			deleted = true;
    			break;
    		}
    	}
        if(deleted) {
            continue;
        }
        for(int k = 0; k < preferred_operators_cheapest.size(); ++k) {
            if(all_operators[l] == preferred_operators_cheapest[k]) {
                all_operators[l] = all_operators[all_operators.size() - 1];
                all_operators.pop_back();
                break;
            }
        }
        if(deleted) {
            continue;
        }
        for(int k = 0; k < preferred_operators_most_expensive.size(); ++k) {
            if(all_operators[l] == preferred_operators_most_expensive[k]) {
                all_operators[l] = all_operators[all_operators.size() - 1];
                all_operators.pop_back();
                break;
            }
        }
        if(deleted) {
            continue;
        }
        for(int k = 0; k < preferred_operators_rand.size(); ++k) {
            if(all_operators[l] == preferred_operators_rand[k]) {
                all_operators[l] = all_operators[all_operators.size() - 1];
                all_operators.pop_back();
                break;
            }
        }
    }

    for(int i = 0; i < open_lists.size(); i++) {
        Heuristic *heur = open_lists[i].heuristic;

        double priority = -1;   // invalid
        // lazy eval = compute priority by parent
        if(g_parameters.lazy_evaluation) {
            double parentG = getG(parent_ptr, parent_ptr, NULL);
            double parentH = heur->get_heuristic();
            assert(!heur->is_dead_end());
            double parentF = parentG + parentH;
            if(g_parameters.greedy)
                priority = parentH;
            else
                priority = parentF;
        }

        OpenList &open = open_lists[i].open;
        vector<const Operator *>* ops = NULL;
        OpenListMode& mode = open_lists[i].mode;
        switch (mode) {
        case ALL:
            assert(!(open_lists[i].only_preferred_operators));
            ops = &all_operators;
            break;
        case REGULAR:
            assert(open_lists[i].only_preferred_operators);
            ops = &preferred_operators_reg;
            break;
        case CHEAPEST:
            ops = &preferred_operators_cheapest;
            break;
        case MOSTEXPENSIVE:
            ops = &preferred_operators_most_expensive;
            break;
        case RAND:
            ops = &preferred_operators_rand;
            break;
        case ORDERED:
            ops = &preferred_operators_ordered;
            break;
        case CONCURRENT:
            ops = &preferred_operators_concurrent;
            break;
        default:
            assert(false);
            break;
        }

        assert(ops);

        // push successors from applicable ops
        if(open_lists[i].mode == CONCURRENT) {
        	assert(g_parameters.pref_ops_concurrent_mode);
        	vector<const Operator*> newOps;
        	TimeStampedState tss(*parent_ptr);
			for(int j = 0; j < ops->size(); j++) {
				assert((*ops)[j]->get_name().compare("wait") != 0);

				// compute expected min makespan of this op
				double maxTimeIncrement = 0.0;
				for(int k = 0; k < parent_ptr->operators.size(); ++k) {
					maxTimeIncrement = max(maxTimeIncrement, parent_ptr->operators[k].time_increment);
				}
				double duration = (*ops)[j]->get_duration(parent_ptr);
				maxTimeIncrement = max(maxTimeIncrement, duration);
				double makespan = maxTimeIncrement + parent_ptr->timestamp;
				bool betterMakespan = makespan < bestMakespan;
				if(g_parameters.use_subgoals_to_break_makespan_ties && makespan == bestMakespan)
					betterMakespan = true;

				// Generate a child/Use an operator if
				// - it is applicable
				// - its minimum makespan is better than the best we had so far
				// - if knownByLogicalStateOnly hasn't closed this state (when feature enabled)

				// only compute tss if needed
				TimedSymbolicStates timedSymbolicStates;
				TimedSymbolicStates* tssPtr = NULL;
				if(g_parameters.use_known_by_logical_state_only)
					tssPtr = &timedSymbolicStates;
				if(betterMakespan && (*ops)[j]->is_applicable(*parent_ptr, tssPtr) &&
						(!knownByLogicalStateOnly(logical_state_closed_list, timedSymbolicStates))) {
					// non lazy eval = compute priority by child
					if(!g_parameters.lazy_evaluation) {
						// need to compute the child to evaluate it
						tss = TimeStampedState(tss, *(*ops)[j]);
						double childG = getG(&tss, parent_ptr, (*ops)[j]);
						double childH = heur->evaluate(tss);
						if(heur->is_dead_end())
							assert(false);
						double childF = childG + childH;
						if(g_parameters.greedy)
							priority = childH;
						else
							priority = childF;
					}
					newOps.push_back((*ops)[j]);
					assert(newOps.size() > 0);
				}
			}
			if(newOps.size() > 0) {
			    open.push(std::tr1::make_tuple(parent_ptr, newOps, priority));
			    search_statistics.countChild(i);
			}
        } else {
			for(int j = 0; j < ops->size(); j++) {
				assert((*ops)[j]->get_name().compare("wait") != 0);

				// compute expected min makespan of this op
				double maxTimeIncrement = 0.0;
				for(int k = 0; k < parent_ptr->operators.size(); ++k) {
					maxTimeIncrement = max(maxTimeIncrement, parent_ptr->operators[k].time_increment);
				}
				double duration = (*ops)[j]->get_duration(parent_ptr);
				maxTimeIncrement = max(maxTimeIncrement, duration);
				double makespan = maxTimeIncrement + parent_ptr->timestamp;
				bool betterMakespan = makespan < bestMakespan;
				if(g_parameters.use_subgoals_to_break_makespan_ties && makespan == bestMakespan)
					betterMakespan = true;

				// Generate a child/Use an operator if
				// - it is applicable
				// - its minimum makespan is better than the best we had so far
				// - if knownByLogicalStateOnly hasn't closed this state (when feature enabled)

				// only compute tss if needed
				TimedSymbolicStates timedSymbolicStates;
				TimedSymbolicStates* tssPtr = NULL;
				if(g_parameters.use_known_by_logical_state_only)
					tssPtr = &timedSymbolicStates;
				if(betterMakespan && (*ops)[j]->is_applicable(*parent_ptr, tssPtr) &&
						(!knownByLogicalStateOnly(logical_state_closed_list, timedSymbolicStates))) {
					// non lazy eval = compute priority by child
					if(!g_parameters.lazy_evaluation) {
						// need to compute the child to evaluate it
						TimeStampedState tss = TimeStampedState(*parent_ptr, *(*ops)[j]);
						double childG = getG(&tss, parent_ptr, (*ops)[j]);
						double childH = heur->evaluate(tss);
						if(heur->is_dead_end())
							continue;
						double childF = childG + childH;
						if(g_parameters.greedy)
							priority = childH;
						else
							priority = childF;
					}

					vector<const Operator*> newOps;
					newOps.push_back((*ops)[j]);
					assert(newOps.size() > 0);
					open.push(std::tr1::make_tuple(parent_ptr, newOps, priority));
					search_statistics.countChild(i);
				}
			}
        }
        // Inserted all children, now insert one more child by letting time pass
        // only allow let_time_pass if there are running operators (i.e. there is time to pass)
        if(!g_parameters.insert_let_time_pass_only_when_running_operators_not_empty || !parent_ptr->operators.empty()) {
            // non lazy eval = compute priority by child
            if(!g_parameters.lazy_evaluation) {
                // compute child
                TimeStampedState tss = parent_ptr->let_time_pass(false,true);
                double childG = getG(&tss, parent_ptr, NULL);
                double childH = heur->evaluate(tss);
                if(heur->is_dead_end()) {
                    continue;
                }

                double childF = childH + childG;
                if(g_parameters.greedy)
                    priority = childH;
                else
                    priority = childF;
            }
            vector<const Operator*> newOps;
            newOps.push_back(g_let_time_pass);
            assert(newOps.size() > 0);
            open.push(std::tr1::make_tuple(parent_ptr, newOps, priority));
            search_statistics.countChild(i);
        }
    }
    search_statistics.finishExpansion();
}

enum SearchEngine::status BestFirstSearchEngine::fetch_next_state()
{
    OpenListInfo *open_info = select_open_queue();
    if(!open_info) {
        if(found_at_least_one_solution()) {
            cout << "Completely explored state space -- best plan found!" << endl;
            return SOLVED_COMPLETE;
        }
        
        if(g_parameters.verbose) {
            time_t current_time = time(NULL);
            statistics(current_time);
        }
        cout << "Completely explored state space -- no solution!" << endl;
        return FAILED;
    }

    std::tr1::tuple<const TimeStampedState *, vector<const Operator *>, double> next =
        open_info->open.top();
    open_info->open.pop();
    open_info->priority++;

    current_predecessor = std::tr1::get<0>(next);
    current_operators = std::tr1::get<1>(next);

    if(current_operators.size() == 1 && current_operators[0] == g_let_time_pass) {
        // do not apply an operator but rather let some time pass until
        // next scheduled happening
        current_state = current_predecessor->let_time_pass(false,true);
    } else {
        //Apply the first operator. Others are applied in step()
        if(current_operators.size() > 0) {
        	assert(current_operators[0]->get_name().compare("wait") != 0);
        	assert(current_operators[0]->is_applicable(*current_predecessor));
        	current_state = TimeStampedState(*current_predecessor, *current_operators[0]);
        }
    }
    assert(&current_state != current_predecessor);
    return IN_PROGRESS;
}

OpenListInfo *BestFirstSearchEngine::select_open_queue()
{
    OpenListInfo *best = 0;

    switch(mode) {
        case PRIORITY_BASED:
            for(int i = 0; i < open_lists.size(); i++) {
                if(!open_lists[i].open.empty() && (best == 0 || open_lists[i].priority < best->priority)) {
                	best = &open_lists[i];
                	activeQueue = i;
                }
            }
            break;
        case ROUND_ROBIN:
            for(int i = 0; i < open_lists.size(); i++) {
//                cout << "  " << "activeQueue: " << activeQueue << endl;
                activeQueue = (activeQueue + 1) % open_lists.size();
                if(!open_lists[activeQueue].open.empty()) {
                    best = &open_lists[activeQueue];
                    break;
                }
            }
            break;
    }

    return best;
}

double BestFirstSearchEngine::getGc(const TimeStampedState *state) const
{
    return closed_list.getCostOfPath(*state);
}

double BestFirstSearchEngine::getGc(const TimeStampedState *state,
        const Operator *op) const
{
    double opCost = 0.0;
    if (op && op != g_let_time_pass) {
        opCost += op->get_duration(state);
    }
    return getGc(state) + opCost;
}

double BestFirstSearchEngine::getGm(const TimeStampedState *state) const
{
    double longestActionDuration = 0.0;
    for (int i = 0; i < state->operators.size(); ++i) {
        const ScheduledOperator* op = &state->operators[i];
        double duration = 0.0;
        if (op && op != g_let_time_pass) {
            duration = op->get_duration(state);
        }
        if (duration > longestActionDuration) {
            longestActionDuration = duration;
        }
    }
    return state->timestamp + longestActionDuration;
}

double BestFirstSearchEngine::getGt(const TimeStampedState *state) const
{
    double ret = state->timestamp - (EPS_TIME * state->numberOfEpsInsertions);
    assert(ret >= 0.0);
    return ret;
}

/**
 * If mode is cost or weighted a parent_ptr and op have to be given.
 *
 * \param [in] state_ptr the state to compute G for
 * \param [in] closed_ptr should be a closed node that op could be applied to, if state is not closed (i.e. a child)
 */
double BestFirstSearchEngine::getG(const TimeStampedState* state_ptr, 
        const TimeStampedState* closed_ptr, const Operator* op) const
{
    double g = HUGE_VAL;
    switch(g_parameters.g_values) {
        case PlannerParameters::GTimestamp:
            g = getGt(state_ptr);
            break;
        case PlannerParameters::GCost:
            if(op == NULL)
                g = getGc(closed_ptr);
            else
                g = getGc(closed_ptr, op);
            break;
        case PlannerParameters::GMakespan:
            g = getGm(state_ptr);
            break;
        case PlannerParameters::GWeighted:
            if(op == NULL)
                g = g_parameters.g_weight * getGm(state_ptr) 
                    + (1.0 - g_parameters.g_weight) * getGc(closed_ptr);
            else
                g = g_parameters.g_weight * getGm(state_ptr) 
                    + (1.0 - g_parameters.g_weight) * getGc(closed_ptr, op);
            break;
        default:
            assert(false);
    }
    return g;
}

