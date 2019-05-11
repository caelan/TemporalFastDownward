#include "monitoring.h"
#include "state.h"
#include "operator.h"
#include "globals.h"
#include "axioms.h"
#include "plannerParameters.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <deque>
#include <algorithm>
#include <fstream>

using namespace std;

MonitorEngine* MonitorEngine::instance = 0;

MonitorEngine* MonitorEngine::getInstance()
{
    if (instance == 0)
        instance = new MonitorEngine();

    return instance;
}

void MonitorEngine::readPlanFromFile(const string& filename, vector<string>& plan)
{
    ifstream fin(filename.c_str(), ifstream::in);
    string buffer;
    while(fin.good()) {
        getline(fin, buffer, '\n');
        if(!buffer.empty())
            plan.push_back(buffer);
    }
    fin.close();
}

bool MonitorEngine::validatePlan(const string & filename)
{
    vector<string> plan;
    MonitorEngine::readPlanFromFile(filename, plan);

    MonitorEngine* mon = MonitorEngine::getInstance();
    bool monitor = mon->validatePlan(plan);
    return monitor;
}


MonitorEngine::MonitorEngine()
{
}

MonitorEngine::~MonitorEngine()
{
}

int lower_case(int c)
{
    return tolower(c);
}

bool MonitorEngine::validatePlan(vector<string>& plan)
{
    vector<PlanStep> p;
    for (unsigned int i = 0; i < plan.size(); i++) {
        if(plan[i].length() == 0)   // empty line
            continue;
        if((plan[i])[0] == ';')   // comment
            continue;
        // start
        if(plan[i].find(":") == string::npos) {
            return false;
        }
        const char* startStr = plan[i].substr(0, plan[i].find(":")).c_str();
        char* check = NULL;
        double start = strtod(startStr, &check);
        if(check == startStr) {
            return false;
        }

        // op
        if(plan[i].find("(") == string::npos || plan[i].find(")") == string::npos) {
            return false;
        }
        string name = plan[i].substr(plan[i].find("(") + 1, plan[i].find(")")
                - plan[i].find("(") - 1);
        transform(name.begin(), name.end(), name.begin(), lower_case);

        // duration
        if(plan[i].find("[") == string::npos || plan[i].find("]") == string::npos) {
            return false;
        }
        const char* durStr = plan[i].substr(plan[i].find("[") + 1, plan[i].length() - plan[i].find("[")).c_str();
        check = NULL;
        double duration = strtod(durStr, &check);
        if(check == durStr) {
            return false;
        }

        // lookup op
        bool opFound = false;
        for (unsigned int j = 0; j < g_operators.size(); j++) {
            if (g_operators[j].get_name() == name) {
                // forward state? FIXME no state here, determined by next step
                p.push_back(PlanStep(start,duration,&g_operators[j], NULL));
                opFound = true;
                break;
            }
        }
        if(!opFound) {
            return false;
        }

    }

    // sort the PlanSteps according to their start time (usually they are already).
    stable_sort(p.begin(), p.end(), PlanStepCompareStartTime());

    unsigned int count = 0;
    for(vector<PlanStep>::iterator it = p.begin(); it != p.end(); it++) {
        count++;
    }

    return validatePlan(p);
}

/**
 * Verify a plan by simulating an execution.
 * It can't be guaranteed that timestamps/durations are exactly matched in a real-world system.
 * So the simulation will try to apply operators in the order induced by the start timestamps and
 * an arbitrary number of let_time_passes. This multiple let_time_pass thus generate multiple traces/plans
 * that only differ from the timestamps but have the same order.
 * The queue in step i always stores a collection of the states from applying operators until step i and
 * all combinations of let_time_pass used.
 * After the last operator is applied the queue should contain at least one state that satisfies the goal check,
 * i.e. the goal_state is satisfied and no running operators are left.
 *
 * When applying same-time operators the plans order is used. If those operators were in conflict with each
 * other (violating no-moving-targets-rule) they should have been epsilonized before (i.e. not the same timestamp).
 * This means that any order should be applicable, just using the original one should thus always work.
 */
bool MonitorEngine::validatePlan(std::vector<PlanStep> & plan)
{
    if(plan.empty()) {
        return g_initial_state->satisfies(g_goal);
    }

    deque< FullPlanTrace > currentTraces;
    // seed with init.
    currentTraces.push_back(FullPlanTrace(*g_initial_state));

    for(int i = 0; i < plan.size(); i++) {
        //printf("CURRENT STATES ARE:\n");
        /*for(deque<FullPlanTrace>::iterator it = currentTraces.begin(); it != currentTraces.end(); it++) {
        // it->dumpLastState();
        }*/

        deque< FullPlanTrace > nextTraces;

        for(deque<FullPlanTrace>::iterator it = currentTraces.begin(); it != currentTraces.end(); it++) {
            //printf("Checking STATE:\n");
            //it->dumpLastState();
            // if applicable apply the operator to every state in the queue
            FullPlanTrace curTrace = *it;

            if(g_parameters.monitoring_verify_timestamps) {
                // this plan trace should now be at the start time of plan[i]
                double dt = fabs(plan[i].start_time - curTrace.lastTimestamp());
                if(dt > EPS_TIME) {     // timestamp doesn't match -> stop this trace
                    continue;
                }
            }

            if(curTrace.isApplicable(plan[i].op)) {
                FullPlanTrace nextTrace = curTrace.applyOperator(plan[i].op);
                nextTraces.push_back(nextTrace);

                // for every state in the queue produce as many versions of let_time_pass
                // as possible and construct the next queue from those
                while(nextTraces.back().canLetTimePass()) {
                    nextTraces.push_back(nextTraces.back().letTimePass());
                }
            }
        }

        currentTraces = nextTraces;
        if(currentTraces.empty()) {
            return false;
        }
    }

    // final step: Finalize all ops in queue by letting time pass to finish all ops
    deque<FullPlanTrace> final_traces;
    for(deque<FullPlanTrace>::iterator it = currentTraces.begin(); it != currentTraces.end(); it++) {
        FullPlanTrace finalTrace = *it;
        while(finalTrace.canLetTimePass())
            finalTrace = finalTrace.letTimePass();
        final_traces.push_back(finalTrace);
    }
    // final_traces contains no final state with running ops

    const FullPlanTrace* best = NULL;
    double bestMakespan = HUGE_VAL;
    bool ret = false;
    for(deque<FullPlanTrace>::iterator it = final_traces.begin(); it != final_traces.end(); it++) {
        const FullPlanTrace & t = *it;
        if(t.satisfiesGoal()) {
            ret = true;
            if(t.getLastTimestamp() < bestMakespan) {
                bestMakespan = t.getLastTimestamp();
                best = &t;
            }
        }
    }

    if(!ret)

    if(best != NULL) {
        stringstream os;
        best->outputPlan(os);
        if(!g_parameters.plan_name.empty()) {
            string monitorPlanName = g_parameters.plan_name + ".monitored";
            ofstream of(monitorPlanName.c_str());
            if(!of.good()) {
            } else {
                best->outputPlan(of);
                of.close();
            }
        }
    }

    return ret;
}

// TODO check this and use as verify-timestamps version if we can produce the plan for output
bool MonitorEngine::validatePlanOld(const vector<PlanStep>& plan)
{
    TimeStampedState current = *g_initial_state;

    for (int i = 0; i < plan.size(); i++) {
        while (plan[i].start_time > current.timestamp) {
            double curren_time = current.timestamp;
            if (plan[i].start_time - 2 * EPS_TIME - EPSILON
                    <= current.timestamp) {
                current.timestamp += EPS_TIME;
            } else {
                current = current.let_time_pass();
            }
            if (double_equals(current.timestamp, curren_time)) {
                current.timestamp += EPS_TIME;
            }
        }
        cout << "Current time_stamp: " << current.timestamp << endl;
        cout << "Next op: " << plan[i].op->get_name() << " ";
        // replace with correct function
        if(!plan[i].op->is_applicable(current)) {
            cout << "is not applicable!" << endl;
            return false;
        } else {
            cout << "is applicable!" << endl;
            current = TimeStampedState(current,(*plan[i].op));
        }
    }

    while (!current.operators.empty())
        current = current.let_time_pass();

    return current.satisfies(g_goal);
}


FullPlanTrace::FullPlanTrace(const TimeStampedState & s)
{
    // pred s is only dummy
    FullPlanStep fps(s, NULL, s);
    plan.push_back(fps);
}

bool FullPlanTrace::isApplicable(const Operator* op) const
{
    if(plan.empty())
        return false;

    return op->is_applicable(plan.back().state, 0);
}

FullPlanTrace FullPlanTrace::applyOperator(const Operator* op) const
{
    // copy
    FullPlanTrace ret = *this;
    if(ret.plan.empty())
        return ret;

    FullPlanStep fps(plan.back().state, op, TimeStampedState(plan.back().state, *op));
    ret.plan.push_back(fps);
    return ret;
}

bool FullPlanTrace::canLetTimePass() const
{
    if(plan.empty())
        return false;

    return !plan.back().state.operators.empty();    // are there running ops in the last state
}

FullPlanTrace FullPlanTrace::letTimePass() const
{
    // copy
    FullPlanTrace ret = *this;
    if(ret.plan.empty())
        return ret;

    ret.plan.back().state = ret.plan.back().state.let_time_pass();
    return ret;
}

bool FullPlanTrace::satisfiesGoal() const
{
    if(plan.empty())
        return false;

    return plan.back().state.satisfies(g_goal);
}

double FullPlanTrace::lastTimestamp() const
{
    if(plan.empty())
        return -1;
    return plan.back().state.timestamp;
}

void FullPlanTrace::outputPlan(ostream & os) const
{
    if(plan.empty())
        return;
    os << fixed << setprecision(8);
    //    bool first = true;
    for(deque<FullPlanStep>::const_iterator it = plan.begin(); it != plan.end(); it++) {
        if(it->op == NULL) {
            //            first = false;
            continue;
        }
        //        first = false;
        //printf("DV: %d\n", it->op->get_duration_var());
        os << it->predecessor.timestamp << ": (" << it->op->get_name() << ") [" 
            << it->op->get_duration(&(it->predecessor)) << "]" << endl;
    }
}

void FullPlanTrace::dumpLastState() const
{
    if(plan.empty()) {
        return;
    }
    plan.back().state.dump(true);
}

double FullPlanTrace::getLastTimestamp() const
{
    if(plan.empty())
        return HUGE_VAL;

    return plan.back().state.timestamp;
}

