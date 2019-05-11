#include "plannerParameters.h"
#include <iostream>
#include <stdio.h>

PlannerParameters::PlannerParameters()
{
    // set defaults
    anytime_search = false;
    timeout_while_no_plan_found = 0;
    timeout_if_plan_found = 0;

    greedy = false;
    lazy_evaluation = true;
    verbose = true;

    insert_let_time_pass_only_when_running_operators_not_empty = false;

    cyclic_cg_heuristic = false;
    cyclic_cg_preferred_operators = false;
    makespan_heuristic = false;
    makespan_heuristic_preferred_operators = false;
    no_heuristic = false;

    cg_heuristic_zero_cost_waiting_transitions = true;
    cg_heuristic_fire_waiting_transitions_only_if_local_problems_matches_state = false;

    use_caching_in_heuristic = true;

    g_values = GTimestamp;
    g_weight = 0.5;

    queueManagementMode = BestFirstSearchEngine::PRIORITY_BASED;

    use_known_by_logical_state_only = false;

    use_subgoals_to_break_makespan_ties = false;

    reschedule_plans = false;
    epsilonize_internally = false;
    epsilonize_externally = false;
    keep_original_plans = true;

    pref_ops_ordered_mode = false;
    pref_ops_cheapest_mode = false;
    pref_ops_most_expensive_mode = false;
    pref_ops_rand_mode = false;
    pref_ops_concurrent_mode = false;
    number_pref_ops_ordered_mode = 1000;
    number_pref_ops_cheapest_mode = 1000;
    number_pref_ops_most_expensive_mode = 1000;
    number_pref_ops_rand_mode = 1000;
    reset_after_solution_was_found = false;

    reward_only_pref_op_queue = false;

    plan_name = "sas_plan";
    planMonitorFileName = "";

    monitoring_verify_timestamps = false;
}

PlannerParameters::~PlannerParameters()
{
}

bool PlannerParameters::readParameters(int argc, char** argv)
{
    bool ret = true;
    ret &= readCmdLineParameters(argc, argv);

    if(!cyclic_cg_heuristic && !makespan_heuristic && !no_heuristic) {
        if(planMonitorFileName.empty()) {   // for monitoring this is irrelevant
            cerr << "Error: you must select at least one heuristic!" << endl
                << "If you are unsure, choose options \"yY\" / cyclic_cg_heuristic." << endl;
            ret = false;
        }
    }
    if(timeout_if_plan_found < 0) {
        cerr << "Error: timeout_if_plan_found < 0, have: " << timeout_if_plan_found << endl;
        timeout_if_plan_found = 0;
        ret = false;
    }
    if(timeout_while_no_plan_found < 0) {
        cerr << "Error: timeout_while_no_plan_found < 0, have: " << timeout_while_no_plan_found << endl;
        timeout_while_no_plan_found = 0;
        ret = false;
    }
    if(use_known_by_logical_state_only) {
        cerr << "WARNING: known by logical state only is experimental and might lead to incompleteness!" << endl;
    }

    return ret;
}

void PlannerParameters::dump() const
{
    cout << endl << "Planner Paramters:" << endl;
    cout << "Anytime Search: " << (anytime_search ? "Enabled" : "Disabled") << endl;
    cout << "Timeout if plan was found: " << timeout_if_plan_found << " seconds";
    if(timeout_if_plan_found == 0)
        cout << " (no timeout)";
    cout << endl;
    cout << "Timeout while no plan was found: " << timeout_while_no_plan_found << " seconds";
    if(timeout_while_no_plan_found == 0)
        cout << " (no timeout)";
    cout << endl;
    cout << "Greedy Search: " << (greedy ? "Enabled" : "Disabled") << endl;
    cout << "Verbose: " << (verbose ? "Enabled" : "Disabled") << endl;
    cout << "Lazy Heuristic Evaluation: " << (lazy_evaluation ? "Enabled" : "Disabled") << endl;

    cout << (use_caching_in_heuristic ? "U" : "Don't u") << "se caching in heuristic." << endl;

    cout << "Cyclic CG heuristic: " << (cyclic_cg_heuristic ? "Enabled" : "Disabled")
        << " \tPreferred Operators: " << (cyclic_cg_preferred_operators ? "Enabled" : "Disabled") << endl;
    cout << "Makespan heuristic: " << (makespan_heuristic ? "Enabled" : "Disabled")
        << " \tPreferred Operators: " << (makespan_heuristic_preferred_operators ? "Enabled" : "Disabled") << endl;
    cout << "No Heuristic: " << (no_heuristic ? "Enabled" : "Disabled") << endl;
    cout << "Cg Heuristic Zero Cost Waiting Transitions: "
        << (cg_heuristic_zero_cost_waiting_transitions ? "Enabled" : "Disabled") << endl;
    cout << "Cg Heuristic Fire Waiting Transitions Only If Local Problems Matches State: "
        << (cg_heuristic_fire_waiting_transitions_only_if_local_problems_matches_state ? "Enabled" : "Disabled") << endl;

    cout << "PrefOpsOrderedMode: " << (pref_ops_ordered_mode ? "Enabled" : "Disabled")
         << " with " << number_pref_ops_ordered_mode << " goals" << endl;
    cout << "PrefOpsCheapestMode: " << (pref_ops_cheapest_mode ? "Enabled" : "Disabled")
         << " with " << number_pref_ops_cheapest_mode << " goals" << endl;
    cout << "PrefOpsMostExpensiveMode: " << (pref_ops_most_expensive_mode ? "Enabled" : "Disabled")
         << " with " << number_pref_ops_most_expensive_mode << " goals" << endl;
    cout << "PrefOpsRandMode: " << (pref_ops_rand_mode ? "Enabled" : "Disabled")
         << " with " << number_pref_ops_rand_mode << " goals" << endl;
    cout << "PrefOpsConcurrentMode: " << (pref_ops_concurrent_mode ? "Enabled" : "Disabled") << endl;
    cout << "Reset after solution was found: " << (reset_after_solution_was_found ? "Enabled" : "Disabled") << endl;

    cout << "Reward only preferred operators queue: " << (reward_only_pref_op_queue ? "Enabled" : "Disabled") << endl;

    cout << "GValues by: ";
    switch(g_values) {
        case GMakespan:
            cout << "Makespan";
            break;
        case GCost:
            cout << "Cost";
            break;
        case GTimestamp:
            cout << "Timestamp";
            break;
        case GWeighted:
            cout << "Weighted (" << g_weight << ")";
            break;
    }
    cout << endl;

    cout << "Queue management mode: ";
    switch(queueManagementMode) {
        case BestFirstSearchEngine::PRIORITY_BASED:
            cout << "Priority based";
            break;
        case BestFirstSearchEngine::ROUND_ROBIN:
            cout << "Round Robin";
            break;
    }
    cout << endl;

    cout << "Known by logical state only filtering: "
        << (use_known_by_logical_state_only ? "Enabled" : "Disabled") << endl;

    cout << "use_subgoals_to_break_makespan_ties: "
        << (use_subgoals_to_break_makespan_ties ? "Enabled" : "Disabled") << endl;

    cout << "Reschedule plans: " << (reschedule_plans ? "Enabled" : "Disabled") << endl;
    cout << "Epsilonize internally: " << (epsilonize_internally ? "Enabled" : "Disabled") << endl;
    cout << "Epsilonize externally: " << (epsilonize_externally ? "Enabled" : "Disabled") << endl;
    cout << "Keep original plans: " << (keep_original_plans ? "Enabled" : "Disabled") << endl;

    cout << "Plan name: \"" << plan_name << "\"" << endl;
    cout << "Plan monitor file: \"" << planMonitorFileName << "\"";
    if(planMonitorFileName.empty()) {
        cout << " (no monitoring)";
    }
    cout << endl;

    cout << "Monitoring verify timestamps: " << (monitoring_verify_timestamps ? "Enabled" : "Disabled") << endl;

    cout << endl;
}

void PlannerParameters::printUsage() const
{
    printf("Usage: search <option characters>  (input read from stdin)\n");
    printf("Options are:\n");
    printf("  a - enable anytime search (otherwise finish on first plan found)\n");
    printf("  t <timeout secs> - total timeout in seconds for anytime search (when plan found)\n");
    printf("  T <timeout secs> - total timeout in seconds for anytime search (when no plan found)\n");
    printf("  m <monitor file> - monitor plan, validate a given plan\n");
    printf("  g - perform greedy search (follow heuristic)\n");
    printf("  l - disable lazy evaluation (Lazy = use parent's f instead of child's)\n");
    printf("  v - disable verbose printouts\n");
    printf("  y - cyclic cg CEA heuristic\n");
    printf("  Y - cyclic cg CEA heuristic - preferred operators\n");
    printf("  x - cyclic cg makespan heuristic \n");
    printf("  X - cyclic cg makespan heuristic - preferred operators\n");
    printf("  G [m|c|t|w] - G value evaluation, one of m - makespan, c - pathcost, t - timestamp, w [weight] - weighted / Note: One of those has to be set!\n");
    printf("  Q [r|p|h] - queue mode, one of r - round robin, p - priority, h - hierarchical\n");
    printf("  K - use tss known filtering (might crop search space)!\n");
    printf("  n - no_heuristic\n");
    printf("  r - reschedule_plans\n");
    printf("  O [n] - prefOpsOrderedMode, with n being the number of pref ops used\n");
    printf("  C [n] - prefOpsCheapestMode, with n being the number of pref ops used\n");
    printf("  E [n] - prefOpsMostExpensiveMode, with n being the number of pref ops used\n");
    printf("  e - epsilonize internally\n");
    printf("  f - epsilonize externally\n");
    printf("  p <plan file> - plan filename prefix\n");
    printf("  M v - monitoring: verify timestamps\n");
    printf("  u - do not use cachin in heuristic\n");
}

bool PlannerParameters::readCmdLineParameters(int argc, char** argv)
{
    for (int i = 1; i < argc; i++) {
        for (const char *c = argv[i]; *c != 0; c++) {
            if (*c == 'a') {
                anytime_search = true;
            } else if (*c == 't') {
                assert(i + 1 < argc);
                timeout_if_plan_found = atoi(string(argv[++i]).c_str());
            } else if (*c == 'T') {
                assert(i + 1 < argc);
                timeout_while_no_plan_found = atoi(string(argv[++i]).c_str());
            } else if (*c == 'O') {
                pref_ops_ordered_mode = true;
                assert(i + 1 < argc);
                number_pref_ops_ordered_mode = atoi(string(argv[++i]).c_str());
            } else if (*c == 'C') {
                pref_ops_cheapest_mode = true;
                assert(i + 1 < argc);
                number_pref_ops_cheapest_mode = atoi(string(argv[++i]).c_str());
            } else if (*c == 'E') {
                pref_ops_most_expensive_mode = true;
                assert(i + 1 < argc);
                number_pref_ops_most_expensive_mode = atoi(string(argv[++i]).c_str());
            } else if (*c == 'R') {
                pref_ops_rand_mode = true;
                assert(i + 1 < argc);
                number_pref_ops_rand_mode = atoi(string(argv[++i]).c_str());
            } else if (*c == 'S') {
                pref_ops_concurrent_mode = true;
            } else if (*c == 'b') {
                reset_after_solution_was_found = true;
            } else if (*c == 'i') {
                reward_only_pref_op_queue = true;
            } else if (*c == 'm') {
                assert(i + 1 < argc);
                planMonitorFileName = string(argv[++i]);
            } else if (*c == 'g') {
                greedy = true;
            } else if (*c == 'u') {
                use_caching_in_heuristic = false;
            } else if (*c == 'l') {
                lazy_evaluation = false;
            } else if (*c == 'v') {
                verbose = false;
            } else if (*c == 'y') {
                cyclic_cg_heuristic = true;
            } else if (*c == 'Y') {
                cyclic_cg_preferred_operators = true;
            } else if (*c == 'x') {
                makespan_heuristic = true;
            } else if (*c == 'X') {
                makespan_heuristic_preferred_operators = true;
            } else if (*c == 'n') {
                no_heuristic = true;
            } else if (*c == 'G') {
                assert(i + 1 < argc);
                const char *g = argv[++i];
                if (*g == 'm') {
                    g_values = GMakespan;
                } else if (*g == 'c') {
                    g_values = GCost;
                } else if (*g == 't') {
                    g_values = GTimestamp;
                } else {
                    assert(*g == 'w');
                    g_values = GWeighted;
                    assert(i + 1 < argc);
                    g_weight = strtod(argv[++i], NULL);
                    assert(g_weight > 0 && g_weight < 1);  // for 0, 1 use G m or G c, others invalid.
                }
            } else if (*c == 'Q') {
                assert(i + 1 < argc);
                const char *g = argv[++i];
                if (*g == 'r') {
                    queueManagementMode = BestFirstSearchEngine::ROUND_ROBIN;
                } else {
                    assert(*g == 'p');
                    queueManagementMode = BestFirstSearchEngine::PRIORITY_BASED;
                }
            } else if (*c == 'K') {
                use_known_by_logical_state_only = true;
            } else if (*c == 'p') {
                assert(i + 1 < argc);
                plan_name = string(argv[++i]);
            } else if (*c == 'r') {
                reschedule_plans = true;
            } else if (*c == 'e') {
                epsilonize_internally = true;
            } else if (*c == 'f') {
                epsilonize_externally = true;
            } else if (*c == 'M') {
                assert(i + 1 < argc);
                const char *g = argv[++i];
                assert(*g == 'v');
                if (*g == 'v') {
                    monitoring_verify_timestamps = true;
                }
            } else {
                cerr << "Unknown option: " << *c << endl;
                printUsage();
                return false;
            }
        }
    }

    return true;
}

