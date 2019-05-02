#ifndef DOMAIN_TRANSITION_GRAPH_H
#define DOMAIN_TRANSITION_GRAPH_H

#include <iostream>
#include <map>
#include <vector>
#include <tr1/unordered_map>
//#include "globals.h"
#include "operator.h"
using namespace std;

class CGHeuristic;
class TimeStampedState;
class Operator;

class ValueNode;
class ValueTransition;
class ValueTransitionLabel;
class DomainTransitionGraph;

// Note: We do not use references but pointers to refer to the "parents" of
// transitions and nodes. This is because these structures could not be
// put into vectors otherwise.

typedef multimap<int, ValueNode *> Heap;
typedef std::tr1::unordered_map<int, int> hashmap;
typedef hashmap::value_type ValuePair;

struct LocalAssignment
{
    DomainTransitionGraph *prev_dtg;
    int local_var;
    double value;
    int var;
    assignment_op fop;
    condition_type cond_type;
    LocalAssignment(DomainTransitionGraph *_prev_dtg, int _local_var, double _value, condition_type _cond_type) :
        prev_dtg(_prev_dtg), local_var(_local_var), value(_value), cond_type(_cond_type)
    {
        var = -1;
    }
    LocalAssignment(DomainTransitionGraph *_prev_dtg, int _local_var, int _var, assignment_op _fop, condition_type _cond_type) :
        prev_dtg(_prev_dtg), local_var(_local_var), var(_var), fop(_fop), cond_type(_cond_type)
    {
        value = -1.0;
    }
    LocalAssignment(istream &in);
    void dump() const;
};

class ValueTransitionLabel
{
    public:
        int duration_variable;
        vector<LocalAssignment> precond;
        trans_type type;
        vector<LocalAssignment> effect;
        Operator *op;
        ValueTransitionLabel(int the_duration_variable, const vector<LocalAssignment> &the_precond,
            trans_type the_type, const vector<LocalAssignment> &the_effect = vector<LocalAssignment> (), Operator* theOp = NULL) :
            duration_variable(the_duration_variable), precond(the_precond), type(the_type), effect(the_effect), op(theOp)
        {
        }
        virtual ~ValueTransitionLabel() {}
};

class ValueTransition
{
    public:
        ValueNode *target;
        vector<ValueTransitionLabel*> ccg_labels; // labels for cyclic CG heuristic

        ValueTransition(ValueNode *targ) : target(targ) {}
        void dump() const;
};

struct ValueNode
{
    DomainTransitionGraph *parent_graph;
    int value;
    vector<ValueTransition> transitions;
    vector<ValueTransition> additional_transitions;

    ValueNode(DomainTransitionGraph *parent, int val) :
        parent_graph(parent), value(val)
    {
    }
    void dump() const;
};

struct CompTransition
{
    int left_var;
    int right_var;
    binary_op op;

    void dump() const;
};

class FuncTransitionLabel : public ValueTransitionLabel
{
    public:
        int starting_variable;
        assignment_op a_op;
        int influencing_variable;
        FuncTransitionLabel(int the_starting_variable, vector<LocalAssignment> the_precond,
                vector<LocalAssignment> the_effect, assignment_op the_a_op,
                int the_influencing_variable, int the_duration_variable, trans_type type,
                Operator *the_op) :
            ValueTransitionLabel(the_duration_variable, the_precond, type, the_effect, the_op),
            starting_variable(the_starting_variable), a_op(the_a_op), influencing_variable(the_influencing_variable)
        {
        }
};

class DomainTransitionGraph
{
    public:
        int var;
        bool is_axiom;

        vector<int> ccg_parents;
        // used for mapping variables in conditions to their global index
        // (only needed for initializing child_state for the start node?)
        hashmap global_to_local_ccg_parents;

        virtual ~DomainTransitionGraph() {}
        static void read_all(istream &in);
        virtual void read_data(istream &in) = 0;
        static void compute_causal_graph_parents_comp(int var, map<int, int> &global_to_ccg_parent);
        static void collect_func_transitions(int var, map<int, int> &global_to_ccg_parent);
        virtual void dump() const = 0;
    protected:
        int translate_global_to_local(map<int, int> &global_to_ccg_parent, int global_var);
};

class DomainTransitionGraphSymb : public DomainTransitionGraph
{
    public:
        vector<ValueNode> nodes;

        DomainTransitionGraphSymb(int var_index, int node_count);
        virtual void read_data(istream &in);
        virtual void dump() const;
        virtual void get_successors(int value, vector<int> &result) const;
        // Build vector of values v' such that there is a transition from value to v'.

    private:
        void compress_effects(const Operator* op, vector<PrePost>& pre_post);
        bool add_relevant_functional_vars_to_context(int var_no, map<int, int> &global_to_ccg_parent);
        void extend_cyclic_effect(const PrePost& pre_post, vector<LocalAssignment>& cyclic_effect,
            map<int, int> &global_to_ccg_parent, const vector<pair<int, int> >& precond_pairs);
        DomainTransitionGraphSymb(const DomainTransitionGraphSymb &other); // copying forbidden
};

class DomainTransitionGraphSubterm : public DomainTransitionGraph
{
    public:
        int left_var;
        int right_var;
        binary_op op;

        DomainTransitionGraphSubterm(int var_index);
        virtual void read_data(istream &in);
        virtual void dump() const;

    private:
        DomainTransitionGraphSubterm(const DomainTransitionGraphSubterm &other); // copying forbidden
};

class DomainTransitionGraphFunc : public DomainTransitionGraph
{
    public:
        vector<FuncTransitionLabel> transitions;
        DomainTransitionGraphFunc(int var_index);
        virtual void read_data(istream &in);
        virtual void dump() const;

    private:
        DomainTransitionGraphFunc(const DomainTransitionGraphFunc &other); // copying forbidden
};

class DomainTransitionGraphComp : public DomainTransitionGraph
{
    public:
        //all transitions which modify a primitve functional variable which is a child
        //in the causal graph of this variable
        // in contrast to DomainTransitionGraphFunc, here the variables are local
        vector<FuncTransitionLabel> transitions;

        std::pair<CompTransition, CompTransition> nodes; //first has start value false; second has start value true
        DomainTransitionGraphComp(int var_index);
        virtual void read_data(istream &in);
        void compute_recursively_parents(int var, map<int, int> &global_to_ccg_parent);
        void collect_recursively_func_transitions(int var, map<int, int> &global_to_ccg_parent);
        virtual void dump() const;
    private:
        DomainTransitionGraphComp(const DomainTransitionGraphComp &other); // copying forbidden
};

#endif
