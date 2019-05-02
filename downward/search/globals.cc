#include "globals.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cassert>
#include <cmath>
using namespace std;

#include "axioms.h"
//#include "causal_graph.h"
#include "domain_transition_graph.h"
#include "operator.h"
#include "state.h"
#include "successor_generator.h"
#include "plannerParameters.h"

void PlanStep::dump() const
{
    cout << start_time << ": " << op->get_name() << "[" << duration << "]"
        << endl;
}

void check_magic(istream &in, string magic)
{
    string word;
    in >> word;
    if(word != magic) {
        cout << "Failed to match magic word '" << magic << "'." << endl;
        cout << "Got '" << word << "'." << endl;
        exit(1);
    }
}

void read_variables(istream &in)
{
    check_magic(in, "begin_variables");
    int count;
    in >> count;
    for(int i = 0; i < count; i++) {
        string name;
        in >> name;
        g_variable_name.push_back(name);
        int range;
        in >> range;
        g_variable_domain.push_back(range);
        int layer;
        in >> layer;
        g_axiom_layers.push_back(layer);
        //identify variable type
        if(range != -1) {
            g_variable_types.push_back(logical);
            //changes to comparison if a comparison axiom is detected
        } else {
            g_variable_types.push_back(primitive_functional);
            //changes to subterm_functional if a numeric axiom is detected
        }
    }
    check_magic(in, "end_variables");
}

void read_goal(istream &in)
{
    check_magic(in, "begin_goal");
    int count;
    in >> count;
    for(int i = 0; i < count; i++) {
        int var;
        double val;
        in >> var >> val;
        g_goal.push_back(make_pair(var, val));
    }
    check_magic(in, "end_goal");
}

void dump_goal()
{
    cout << "Goal Conditions:" << endl;
    for(int i = 0; i < g_goal.size(); i++)
        cout << "  " << g_variable_name[g_goal[i].first] << ": "
            << g_goal[i].second << endl;
}

void read_operators(istream &in)
{
    int count;
    in >> count;
    for(int i = 0; i < count; i++)
        g_operators.push_back(Operator(in));
}

void read_logic_axioms(istream &in)
{
    int count;
    in >> count;
    for(int i = 0; i < count; i++) {
        LogicAxiom *ax = new LogicAxiom(in);
        g_axioms.push_back(ax);
    }
}

void read_numeric_axioms(istream &in)
{
    int count;
    in >> count;
    for(int i = 0; i < count; i++) {
        NumericAxiom *ax = new NumericAxiom(in);
        g_axioms.push_back(ax);
        // ax->dump();
    }
}

void read_contains_universal_conditions(istream &in)
{
    in >> g_contains_universal_conditions;
}

void evaluate_axioms_in_init()
{
    g_axiom_evaluator = new AxiomEvaluator;
    g_axiom_evaluator->evaluate(*g_initial_state);
}

void read_everything(istream &in)
{
    read_variables(in);
    g_initial_state = new TimeStampedState(in);
    //g_initial_state->dump();
    read_goal(in);
    read_operators(in);
    read_logic_axioms(in);
    read_numeric_axioms(in);
    evaluate_axioms_in_init();
    check_magic(in, "begin_SG");
    g_successor_generator = read_successor_generator(in);
    check_magic(in, "end_SG");
    g_causal_graph = new CausalGraph(in);
    DomainTransitionGraph::read_all(in);
    read_contains_universal_conditions(in);
}

void dump_everything()
{
    cout << "Variables (" << g_variable_name.size() << "):" << endl;
    for(int i = 0; i < g_variable_name.size(); i++)
        cout << "  " << g_variable_name[i] << " (range "
            << g_variable_domain[i] << ")" << endl;
    cout << "Initial State:" << endl;
    g_initial_state->dump(true);
    dump_goal();
    cout << "Successor Generator:" << endl;
    g_successor_generator->dump();
    for(int i = 0; i < g_variable_domain.size(); i++)
        g_transition_graphs[i]->dump();
}

void dump_DTGs()
{
    for(int i = 0; i < g_variable_domain.size(); i++) {
        cout << "DTG of variable " << i;
        g_transition_graphs[i]->dump();
    }
}

int g_last_arithmetic_axiom_layer;
int g_comparison_axiom_layer;
int g_first_logic_axiom_layer;
int g_last_logic_axiom_layer;
vector<string> g_variable_name;
vector<int> g_variable_domain;
vector<int> g_axiom_layers;
vector<double> g_default_axiom_values;
vector<variable_type> g_variable_types;
TimeStampedState *g_initial_state;
vector<pair<int, double> > g_goal;
vector<Operator> g_operators;
vector<Axiom*> g_axioms;
AxiomEvaluator *g_axiom_evaluator;
SuccessorGenerator *g_successor_generator;
vector<DomainTransitionGraph *> g_transition_graphs;
CausalGraph *g_causal_graph;

PlannerParameters g_parameters;

Operator *g_let_time_pass;
Operator *g_wait_operator;

bool g_contains_universal_conditions;

istream& operator>>(istream &is, assignment_op &aop)
{
    string strVal;
    is >> strVal;
    if(!strVal.compare("="))
        aop = assign;
    else if(!strVal.compare("+"))
        aop = increase;
    else if(!strVal.compare("-"))
        aop = decrease;
    else if(!strVal.compare("*"))
        aop = scale_up;
    else if(!strVal.compare("/"))
        aop = scale_down;
    else {
        cout << "SEVERE ERROR: expected assignment operator, read in " << strVal << endl;
        assert(false);
    }
    return is;
}

ostream& operator<<(ostream &os, const assignment_op &aop)
{
    switch (aop) {
        case assign:
            os << ":=";
            break;
        case scale_up:
            os << "*=";
            break;
        case scale_down:
            os << "/=";
            break;
        case increase:
            os << "+=";
            break;
        case decrease:
            os << "-=";
            break;
        default:
            cout << "Error: aop has value " << (int)aop << endl;

            assert(false);
            break;
    }
    return os;
}

istream& operator>>(istream &is, binary_op &bop)
{
    string strVal;
    is >> strVal;
    if(!strVal.compare("+"))
        bop = add;
    else if(!strVal.compare("-"))
        bop = subtract;
    else if(!strVal.compare("*"))
        bop = mult;
    else if(!strVal.compare("/"))
        bop = divis;
    else if(!strVal.compare("<"))
        bop = lt;
    else if(!strVal.compare("<="))
        bop = le;
    else if(!strVal.compare("="))
        bop = eq;
    else if(!strVal.compare(">="))
        bop = ge;
    else if(!strVal.compare(">"))
        bop = gt;
    else if(!strVal.compare("!="))
        bop = ue;
    else {
        cout << strVal << " was read" << endl;
        assert(false);
    }
    return is;
}

ostream& operator<<(ostream &os, const binary_op &bop)
{
    switch (bop) {
        case mult:
            os << "*";
            break;
        case divis:
            os << "/";
            break;
        case add:
            os << "+";
            break;
        case subtract:
            os << "-";
            break;
        case lt:
            os << "<";
            break;
        case le:
            os << "<=";
            break;
        case eq:
            os << "=";
            break;
        case ge:
            os << ">=";
            break;
        case gt:
            os << ">";
            break;
        case ue:
            os << "!=";
            break;
        default:
            assert(false);
            break;
    }
    return os;
}

istream& operator>>(istream &is, trans_type &tt)
{
    string strVal;
    is >> strVal;
    if(!strVal.compare("s"))
        tt = start;
    else if(!strVal.compare("e"))
        tt = end;
    else if(!strVal.compare("c"))
        tt = compressed;
    else if(!strVal.compare("a"))
        tt = ax;
    else {
        cout << strVal << " was read." << endl;
        assert(false);
    }
    return is;
}

ostream& operator<<(ostream &os, const trans_type &tt)
{
    switch (tt) {
        case start:
            os << "s";
            break;
        case end:
            os << "e";
            break;
        case compressed:
            os << "c";
            break;
        case ax:
            os << "a";
        default:
            // cout << "Error: Encountered binary operator " << bop << "." << endl;
            assert(false);
            break;
    }
    return os;
}

istream& operator>>(istream &is, condition_type &ct)
{
    string strVal;
    is >> strVal;
    if(!strVal.compare("s"))
        ct = start_cond;
    else if(!strVal.compare("o"))
        ct = overall_cond;
    else if(!strVal.compare("e"))
        ct = end_cond;
    else if(!strVal.compare("a"))
        ct = ax_cond;
    else
        assert(false);
    return is;
}

ostream& operator<<(ostream &os, const condition_type &ct)
{
    switch (ct) {
        case start_cond:
            os << "s";
            break;
        case overall_cond:
            os << "o";
            break;
        case end_cond:
            os << "e";
            break;
        case ax_cond:
            os << "a";
            break;
        default:
            assert(false);
    }
    return os;
}

void printSet(const set<int> s)
{
    set<int>::const_iterator it;
    for(it = s.begin(); it != s.end(); ++it)
        cout << *it << ",";
    cout << endl;
}
