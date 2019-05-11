#include "best_first_search.h"
#include "cyclic_cg_heuristic.h"
#include "no_heuristic.h"
#include "monitoring.h"

#include "globals.h"
#include "operator.h"
#include "partial_order_lifter.h"

#include "plannerParameters.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>

#include <cstdio>
#include <math.h>

using namespace std;

#include <sys/times.h>
#include <sys/time.h>

double save_plan(BestFirstSearchEngine& engine, double best_makespan, int &plan_number, string &plan_name);
//std::string getTimesName(const string & plan_name);    ///< returns the file name of the .times file for plan_name
double getCurrentTime();            ///< returns the system time in seconds

int main(int argc, char **argv)
{
    srand(1);
    ifstream file("../preprocess/output");
    if(strcmp(argv[argc - 1], "-eclipserun") == 0) {
        cin.rdbuf(file.rdbuf());
        cerr.rdbuf(cout.rdbuf());
        argc--;
    }

    struct tms start, search_start, search_end;
    times(&start);
    double start_walltime, search_start_walltime, search_end_walltime;
    start_walltime = getCurrentTime();

    if(!g_parameters.readParameters(argc, argv)) {
        cerr << "Error in reading parameters.\n";
        return 2;
    }
    g_parameters.dump();

    bool poly_time_method = false;
    cin >> poly_time_method;
    if(poly_time_method) {
        cout << "Poly-time method not implemented in this branch." << endl;
        cout << "Starting normal solver." << endl;
    }

    read_everything(cin);

    cout << "Contains universal conditions: " << g_contains_universal_conditions << endl;
    if(g_parameters.reschedule_plans && g_contains_universal_conditions) {
        cout << "Disabling rescheduling because of universal conditions in original task!" << endl;
    }

    g_let_time_pass = new Operator(false);
    g_wait_operator = new Operator(true);

    //    FILE* timeDebugFile = NULL;
    // if(!getTimesName(g_parameters.plan_name).empty()) {
    //     timeDebugFile = fopen(getTimesName(g_parameters.plan_name).c_str(), "w");
    //     if(!timeDebugFile) {
    //         cout << "WARNING: Could not open time debug file at: " << getTimesName(g_parameters.plan_name) << endl;
    //     } else {
    //         fprintf(timeDebugFile, "# Makespans for created plans and the time it took to create the plan\n");
    //         fprintf(timeDebugFile, "# The special makespan: -1 "
    //                 "indicates the total runtime and not the generation of a plan\n");
    //         fprintf(timeDebugFile, "# makespan search_time(s) total_time(s) search_walltime(s) total_walltime(s)\n");
    //         fflush(timeDebugFile);
    //     }
    // }


    // Monitoring mode
    if (!g_parameters.planMonitorFileName.empty()) {
        bool ret = MonitorEngine::validatePlan(g_parameters.planMonitorFileName);
        if(ret)
            exit(0);
        exit(1);
    }

    // Initialize search engine and heuristics
    BestFirstSearchEngine* engine = new BestFirstSearchEngine(g_parameters.queueManagementMode);

    if(g_parameters.makespan_heuristic || g_parameters.makespan_heuristic_preferred_operators)
        engine->add_heuristic(new CyclicCGHeuristic(CyclicCGHeuristic::REMAINING_MAKESPAN),
            g_parameters.makespan_heuristic, g_parameters.makespan_heuristic_preferred_operators);
    if(g_parameters.cyclic_cg_heuristic || g_parameters.cyclic_cg_preferred_operators)
        engine->add_heuristic(new CyclicCGHeuristic(CyclicCGHeuristic::CEA), g_parameters.cyclic_cg_heuristic,
            g_parameters.cyclic_cg_preferred_operators, g_parameters.pref_ops_cheapest_mode,
            g_parameters.pref_ops_most_expensive_mode, g_parameters.pref_ops_ordered_mode,
            g_parameters.pref_ops_rand_mode, g_parameters.pref_ops_concurrent_mode);
    if(g_parameters.no_heuristic)
        engine->add_heuristic(new NoHeuristic, g_parameters.no_heuristic, false);

    double best_makespan = REALLYBIG;
    times(&search_start);
    search_start_walltime = getCurrentTime();
    int plan_number = 1;

    SearchEngine::status search_result = SearchEngine::IN_PROGRESS;
    if(g_parameters.reset_after_solution_was_found) {
        cout << "Giving prior boost to open list " << engine->queueStartedLastWith << endl;
        engine->open_lists[engine->queueStartedLastWith].priority -= 5000;
    }

    while(true) {
        engine->initialize();
        search_result = engine->search();

        times(&search_end);
        search_end_walltime = getCurrentTime();
        if(engine->found_solution()) {
            cout << "New solution has been found." << endl;
            if(search_result == SearchEngine::SOLVED) {
                // FIXME only save_plan if return value is SOLVED, otherwise no new plan was found
                best_makespan = save_plan(*engine, best_makespan, plan_number, g_parameters.plan_name);
                // write plan length and search time to file
                // if(timeDebugFile && search_result == SearchEngine::SOLVED) {    // don't write info for timeout
                //     int search_ms = (search_end.tms_utime - search_start.tms_utime) * 10;
                //     int total_ms = (search_end.tms_utime - start.tms_utime) * 10;
                //     double search_time = 0.001 * (double)search_ms;
                //     double total_time = 0.001 * (double)total_ms;
                //     double search_time_wall = search_end_walltime - search_start_walltime;
                //     double total_time_wall = search_end_walltime - start_walltime;

                //     fprintf(timeDebugFile, "%f %f %f %f %f\n", best_makespan, search_time, total_time, 
                //             search_time_wall, total_time_wall);
                //     fflush(timeDebugFile);
                // }
                engine->bestMakespan = best_makespan;
            }
            // to continue searching we need to be in anytime search and the ret value is SOLVED
            // all other possibilities are either a timeout or completely explored search space
            if(g_parameters.anytime_search) {
                if (search_result == SearchEngine::SOLVED) {
                    if(g_parameters.reset_after_solution_was_found && engine->mode == BestFirstSearchEngine::PRIORITY_BASED) {
//                        engine->reset();
                    } else {
                        engine->fetch_next_state();
                    }
                } else {
                    break;
                }
            } else {
                break;
            }
        } else {
            break;
        }
    }

    double search_time_wall = search_end_walltime - search_start_walltime;
    double total_time_wall = search_end_walltime - start_walltime;

    int search_ms = (search_end.tms_utime - search_start.tms_utime) * 10;
    int total_ms = (search_end.tms_utime - start.tms_utime) * 10;
    double search_time = 0.001 * (double)search_ms;
    double total_time = 0.001 * (double)total_ms;
    cout << "Search time: " << search_time << " seconds - Walltime: "
        << search_time_wall << " seconds" << endl;
    cout << "Total time: " << total_time << " seconds - Walltime: " 
        << total_time_wall << " seconds" << endl;

    // if(timeDebugFile) {
    //     fprintf(timeDebugFile, "%f %f %f %f %f\n", -1.0, search_time, total_time, 
    //             search_time_wall, total_time_wall);
    //     fclose(timeDebugFile);
    // }

    switch(search_result) {
        case SearchEngine::SOLVED_TIMEOUT:
        case SearchEngine::FAILED_TIMEOUT:
            return 137;
        case SearchEngine::SOLVED:
        case SearchEngine::SOLVED_COMPLETE:
            return 0;
        case SearchEngine::FAILED:
            assert (!engine->found_at_least_one_solution());
            return 1;
        default:
            cerr << "Invalid Search return value: " << search_result << endl;
    }

    return 2;
}

/// Epsilonize the plan at filename
/**
 * Uses a syscall to epsilonize_plan in tfd_modules package.
 *
 * This will result in the file "filename" copied at "filename.orig" and
 * "filename" being the epsilonized version.
 *
 * \returns true, if the syscall was successfull.
 */
bool epsilonize_plan(const std::string & filename, bool keepOriginalPlan = true)
{
    // get a temp file name
    std::string orig_file = filename + ".orig";

    // first move filename to ...orig file, FIXME copy better?
    int retMove = rename(filename.c_str(), orig_file.c_str());
    if(retMove != 0) {
        cerr << __func__ << ": Error moving to orig file: " << orig_file << " from: " << filename << endl;
        return false;
    }

    // create the syscall to write the epsilonized plan
    std::string syscall = "epsilonize_plan.py";
    // read in the orig plan filename and output to filename
    syscall += " < " + orig_file + " > " + filename;  

    // execute epsilonize
    int ret = system(syscall.c_str());
    if(ret != 0) {
        cerr << __func__ << ": Error executing epsilonize_plan as: " << syscall << endl;
        // move back instead of copying/retaining .orig although not epsilonized
        int retMove = rename(orig_file.c_str(), filename.c_str());
        if(retMove != 0) {
            cerr << __func__ << ": Error moving orig file: " << orig_file << " back to: " << filename << endl;
        }
        return false;
    }

    if(!keepOriginalPlan) {
        remove(orig_file.c_str());  // don't care about return value, can't do anything if it fails
    }

    return true;
}

double save_plan(BestFirstSearchEngine& engine, double best_makespan, int &plan_number, string &plan_name)
{
    const vector<PlanStep> &plan = engine.get_plan();
    const PlanTrace &path = engine.get_path();

    PartialOrderLifter partialOrderLifter(plan, path);

    Plan rescheduled_plan = plan;
    if(!g_contains_universal_conditions && g_parameters.reschedule_plans)
        rescheduled_plan = partialOrderLifter.lift();

    double makespan = 0;
    for(int i = 0; i < rescheduled_plan.size(); i++) {
        double end_time = rescheduled_plan[i].start_time + rescheduled_plan[i].duration;
        makespan = max(makespan, end_time);
    }
    double original_makespan = 0;
    for (int i = 0; i < plan.size(); i++) {
        double end_time = plan[i].start_time + rescheduled_plan[i].duration;
        original_makespan = max(original_makespan, end_time);
    }

    if(g_parameters.use_subgoals_to_break_makespan_ties) {
        if(makespan > best_makespan)
            return best_makespan;

        if(double_equals(makespan, best_makespan)) {
            double sumOfSubgoals = getSumOfSubgoals(rescheduled_plan);
            if(sumOfSubgoals < engine.bestSumOfGoals) {
                cout
                    << "Found plan of equal makespan but with faster solved subgoals."
                    << endl;
                cout << "Sum of subgoals = " << sumOfSubgoals << endl;
                engine.bestSumOfGoals = sumOfSubgoals;
            } else {
                return best_makespan;
            }
        } else {
            assert(makespan < best_makespan);
            double sumOfSubgoals = getSumOfSubgoals(rescheduled_plan);
            engine.bestSumOfGoals = sumOfSubgoals;
        }
    } else {
        if(makespan >= best_makespan)
            return best_makespan;
    }

    cout << endl << "Found new plan:" << endl;
    for(int i = 0; i < plan.size(); i++) {
        const PlanStep& step = plan[i];
        printf("%.8f: (%s) [%.8f]\n", step.start_time, step.op->get_name().c_str(), step.duration);
    }
    if(!g_contains_universal_conditions && g_parameters.reschedule_plans) {
        cout << "Rescheduled Plan:" << endl;
        for (int i = 0; i < rescheduled_plan.size(); i++) {
            const PlanStep& step = rescheduled_plan[i];
            printf("%.8f: (%s) [%.8f]\n", step.start_time, step.op->get_name().c_str(), step.duration);
        }
    }
    cout << "Solution with original makespan " << original_makespan
        << " found (ignoring no-moving-targets-rule)." << endl;

    // Determine filenames to write to
    FILE *file = 0;
    //    FILE *unscheduled_plan_file = 0;
    //    FILE *best_file = 0;
    std::string plan_filename;
    //    std::string unscheduled_plan_filename;
    //    std::string best_plan_filename;
    if (plan_name != "-") {
        // Construct planNrStr to get 42 into "42".
        int nrLen = log10(plan_number) + 10;
        char* planNr = (char*)malloc(nrLen * sizeof(char));
        sprintf(planNr, "%d", plan_number);
        std::string planNrStr = planNr;
        free(planNr);

        // Construct filenames
        //        best_plan_filename = plan_name + ".best";
        if (g_parameters.anytime_search)
            plan_filename = plan_name + "." + planNrStr;
        else
            plan_filename = plan_name;
        //        unscheduled_plan_filename = plan_filename + ".unscheduled";

        // open files
        file = fopen(plan_filename.c_str(), "w");
        //        unscheduled_plan_file = fopen(unscheduled_plan_filename.c_str(), "w");
        //        best_file = fopen(best_plan_filename.c_str(), "w");

        if(file == NULL) {
            fprintf(stderr, "%s:\n  Could not open plan file %s for writing.\n", 
                    __PRETTY_FUNCTION__, plan_filename.c_str());
            return makespan;
        }
    } else {
        file = stdout;
    }

    // Actually write the plan
    plan_number++;
    for(int i = 0; i < rescheduled_plan.size(); i++) {
        const PlanStep& step = rescheduled_plan[i];
        fprintf(file, "%.8f: (%s) [%.8f]\n", step.start_time, step.op->get_name().c_str(), step.duration);
        // if (best_file) {
        //     fprintf(best_file, "%.8f: (%s) [%.8f]\n", 
        //         step.start_time, step.op->get_name().c_str(), step.duration);
        // }
    }

    // for(int i = 0; i < plan.size(); i++) {
    //     const PlanStep& step = plan[i];
    //     fprintf(unscheduled_plan_file, "%.8f: (%s) [%.8f]\n", step.start_time, step.op->get_name().c_str(), step.duration);
    // }

    if (plan_name != "-") {
        fclose(file);
        //        fclose(unscheduled_plan_file);
        // if(best_file)
        //     fclose(best_file);
    }

    cout << "Plan length: " << rescheduled_plan.size() << " step(s)." << endl;
    cout << "Makespan   : " << makespan << endl;
    if(!g_contains_universal_conditions && g_parameters.reschedule_plans)
        cout << "Rescheduled Makespan   : " << makespan << endl;
    else
        cout << "Makespan   : " << makespan << endl;

    if(g_parameters.epsilonize_externally) {
    // Perform epsilonize
    if(!plan_filename.empty()) {
        bool ret_of_epsilonize_plan = epsilonize_plan(plan_filename, g_parameters.keep_original_plans);
        if(!ret_of_epsilonize_plan)
            cout << "Error while calling epsilonize plan! File: " << plan_filename << endl;
    }
    // if(!best_plan_filename.empty()) {
    //     bool ret_of_epsilonize_best_plan = epsilonize_plan(best_plan_filename, g_parameters.keep_original_plans);
    //     if(!ret_of_epsilonize_best_plan)
    //         cout << "Error while calling epsilonize best_plan! File: " << best_plan_filename << endl;
    //     }
    }

    return makespan;
}

// std::string getTimesName(const string & plan_name)
// {
//     if(plan_name.empty())
//         return std::string();
//     if(plan_name == "-")
//         return std::string();
//     return plan_name + ".times";
// }

double getCurrentTime()
{
    const double USEC_PER_SEC = 1000000;

    struct timeval tv; 
    gettimeofday(&tv, 0);
    return(tv.tv_sec + (double)tv.tv_usec / USEC_PER_SEC);
}

