#ifndef NDEBUG
#include <ext/algorithm>
#endif
#include <iostream>
#include <map>
#include <cassert>
using namespace std;
using namespace __gnu_cxx;

#include "domain_transition_graph.h"
#include "globals.h"

void DomainTransitionGraph::read_all(istream &in)
{
    int var_count = g_variable_domain.size();

    // First step: Allocate graphs and nodes.
    g_transition_graphs.reserve(var_count);
    for(int var = 0; var < var_count; var++) {
        switch (g_variable_types[var]) {
            case logical: 
                {
                    int range = g_variable_domain[var];
                    assert(range> 0);
                    DomainTransitionGraphSymb *dtg = new DomainTransitionGraphSymb(var, range);
                    g_transition_graphs.push_back(dtg);
                    break;
                }
            case primitive_functional: 
                {
                    DomainTransitionGraphFunc *dtg = new DomainTransitionGraphFunc(var);
                    g_transition_graphs.push_back(dtg);
                    break;
                }
            case subterm_functional: 
                {
                    DomainTransitionGraphSubterm *dtg =
                        new DomainTransitionGraphSubterm(var);
                    g_transition_graphs.push_back(dtg);
                    break;
                }
            case comparison: 
                {
                    DomainTransitionGraphComp *dtg = new DomainTransitionGraphComp(var);
                    g_transition_graphs.push_back(dtg);
                    break;
                }
            default:
                assert(false);
                break;
        }
    }

    // Second step: Read transitions from file.
    for(int var = 0; var < var_count; var++) {
        g_transition_graphs[var]->read_data(in);
    }

    // Third step: Each primitive and subterm variable connected by a direct path to a comparison variable
    // has to be a direct parent.
    // Furthermore, collect all FuncTransitions of each subterm and primitive variable of a comparison
    // variable directly in the corresponding dtg.
    for(int var = 0; var < var_count; var++) {
        if(g_variable_types[var] == comparison) {
            map<int, int> global_to_ccg_parent;
            DomainTransitionGraph::compute_causal_graph_parents_comp(var,
                    global_to_ccg_parent);
            DomainTransitionGraph::collect_func_transitions(var,
                    global_to_ccg_parent);
        }
    }
}

void DomainTransitionGraph::compute_causal_graph_parents_comp(int var, map<int, int> &global_to_ccg_parent)
{
    DomainTransitionGraph *dtg = g_transition_graphs[var];
    DomainTransitionGraphComp *cdtg =
        dynamic_cast<DomainTransitionGraphComp*>(dtg);
    assert(cdtg);
    cdtg->compute_recursively_parents(cdtg->nodes.first.left_var,
            global_to_ccg_parent);
    cdtg->compute_recursively_parents(cdtg->nodes.first.right_var,
            global_to_ccg_parent);
}

void DomainTransitionGraph::collect_func_transitions(int var, map<int, int> &global_to_ccg_parent)
{
    DomainTransitionGraph *dtg = g_transition_graphs[var];
    DomainTransitionGraphComp *cdtg =
        dynamic_cast<DomainTransitionGraphComp*>(dtg);
    assert(cdtg);
    cdtg->collect_recursively_func_transitions(cdtg->nodes.first.left_var,
            global_to_ccg_parent);
    cdtg->collect_recursively_func_transitions(cdtg->nodes.first.right_var,
            global_to_ccg_parent);
}

DomainTransitionGraphSymb::DomainTransitionGraphSymb(int var_index, int node_count)
{
    is_axiom = g_axiom_layers[var_index] != -1;
    var = var_index;
    nodes.reserve(node_count);
    for(int value = 0; value < node_count; value++)
        nodes.push_back(ValueNode(this, value));
}

void DomainTransitionGraphSymb::read_data(istream &in)
{
    check_magic(in, "begin_DTG");

    map<int, int> global_to_ccg_parent;
    map<pair<int, int>, int> transition_index;
    // FIXME: This transition index business is caused by the fact
    //       that transitions in the input are not grouped by target
    //       like they should be. Change this.

    for(int origin = 0; origin < nodes.size(); origin++) {
        int trans_count;
        in >> trans_count;
        for(int i = 0; i < trans_count; i++) {
            trans_type tt;
            int target, operator_index;
            in >> target;
            in >> operator_index;
            in >> tt;

            pair<int, int> arc = make_pair(origin, target);
            if(!transition_index.count(arc)) {
                transition_index[arc] = nodes[origin].transitions.size();
                nodes[origin].transitions.push_back(ValueTransition(&nodes[target]));
                //if assertions fails, range is to small.
                assert(double_equals(
                            target,
                            nodes[origin].transitions[transition_index[arc]].target->value));
            }

            assert(transition_index.count(arc));
            ValueTransition *transition =
                &nodes[origin].transitions[transition_index[arc]];

            binary_op op;
            int duration_variable;

            if(tt == start || tt == end || tt == compressed)
                in >> op >> duration_variable;
            else {
                // axioms
                op = eq;
                duration_variable = -1;
            }

            if(duration_variable != -1) {
                if(!global_to_ccg_parent.count(duration_variable)) {
                    int duration_variable_local = ccg_parents.size();
                    global_to_ccg_parent[duration_variable] = ccg_parents.size();
                    ccg_parents.push_back(duration_variable);
                    global_to_local_ccg_parents.insert(ValuePair(duration_variable,
                        duration_variable_local));
                }
            }

            if(op != eq) {
                cout << "Error: The duration constraint must be of the form\n";
                cout << "       (= ?duration (arithmetic_term))" << endl;
                exit(1);
            }

            vector<LocalAssignment> all_prevails;
            vector<LocalAssignment> cyclic_effect;
            int prevail_count;
            in >> prevail_count;

            vector<pair<int, int> > precond_pairs; // Needed to build up cyclic_effect.
            for(int j = 0; j < prevail_count; j++) {
                int global_var, val;
                condition_type cond_type;
                in >> global_var >> val >> cond_type;
                precond_pairs.push_back(make_pair(global_var, val));

                // Processing for full DTG (cyclic CG).
                translate_global_to_local(global_to_ccg_parent, global_var);
                int ccg_parent = global_to_ccg_parent[global_var];
                DomainTransitionGraph *prev_dtg =
                    g_transition_graphs[global_var];
                all_prevails.push_back(LocalAssignment(prev_dtg, ccg_parent,
                    val, cond_type));
            }
            Operator *the_operator = NULL;

            // FIXME: Skipping axioms for now, just to make the code compile.
            //        Take axioms into account again asap.

            if(is_axiom) {
                // assert(operator_index >= 0 && operator_index < g_axioms.size());
                // the_operator = &g_axioms[operator_index];
            } else {
                assert(operator_index >= 0 && operator_index
                        < g_operators.size());
                the_operator = &g_operators[operator_index];
            }

            // Build up cyclic_effect. This is messy because this isn't
            // really the place to do this, and it was added very much as an
            // afterthought.
            if(the_operator) {
                //	       cout << "Processing operator " << the_operator->get_name() << endl;

                vector<PrePost> pre_post;
                compress_effects(the_operator, pre_post);

                sort(precond_pairs.begin(), precond_pairs.end());

                for(int j = 0; j < pre_post.size(); j++) {
                    int var_no = pre_post[j].var;
                    if(var_no != var) {
                        bool already_contained_in_global = global_to_ccg_parent.count(var_no);
                        bool var_influences_important_comp_var = false;
                        if(!already_contained_in_global &&
                            g_variable_types[var_no] == primitive_functional) {
                            var_influences_important_comp_var =
                                add_relevant_functional_vars_to_context(var_no, global_to_ccg_parent);
                        }
                        if(already_contained_in_global || var_influences_important_comp_var) {
                            extend_cyclic_effect(pre_post[j], cyclic_effect, global_to_ccg_parent,
                                precond_pairs);
                        }
                    }
                }
            }

            transition->ccg_labels.push_back(new ValueTransitionLabel(
                        duration_variable, all_prevails, tt, cyclic_effect,
                        the_operator));
        }
    }
    check_magic(in, "end_DTG");
}

void DomainTransitionGraphSymb::extend_cyclic_effect(const PrePost& pre_post, vector<LocalAssignment>& cyclic_effect,
        map<int, int> &global_to_ccg_parent, const vector<pair<int, int> >& precond_pairs)
{
    int var_no = pre_post.var;
    double pre = pre_post.pre;
    double post = pre_post.post;
    int var_post = pre_post.var_post;
    assignment_op fop = pre_post.fop;

    vector<pair<int, int> > triggercond_pairs;
    if(pre != -1)
        triggercond_pairs.push_back(make_pair(var_no, static_cast<int>(pre)));

    vector<Prevail> cond;
    for(int f = 0; f < pre_post.cond_start.size(); ++f) {
        cond.push_back(pre_post.cond_start[f]);
    }
    for(int f = 0; f < pre_post.cond_end.size(); ++f) {
        cond.push_back(pre_post.cond_end[f]);
    }
    for(int k = 0; k < cond.size(); k++)
        triggercond_pairs.push_back(make_pair(cond[k].var,
                    static_cast<int>(cond[k].prev)));
    sort(triggercond_pairs.begin(), triggercond_pairs.end());
    //    assert(is_sorted(precond_pairs.begin(), precond_pairs.end()));

    if(includes(precond_pairs.begin(), precond_pairs.end(),
                triggercond_pairs.begin(), triggercond_pairs.end())) {
        // if each condition (of the conditional effect) is implied by
        // an operator condition, we can safely add the effects to
        // the cyclic effects
        assert(global_to_ccg_parent.count(var_no));
        int ccg_parent = global_to_ccg_parent[var_no];
        assert(g_variable_types[var_no] == primitive_functional || g_variable_types[var_no] == logical);
        if(g_variable_types[var_no] == logical) {
            if(!double_equals(pre, post)) {
                // FIXME: condition_type is nonsense at this point (will not be needed later!)
                cyclic_effect.push_back(LocalAssignment(
                    g_transition_graphs[var_no], ccg_parent,
                    post, end_cond));
            }
        } else {
            assert(g_variable_types[var_no] == primitive_functional);
            translate_global_to_local(global_to_ccg_parent, var_post);
            // FIXME: condition_type is nonsense at this point (will not be needed later!)
            cyclic_effect.push_back(LocalAssignment(
                        g_transition_graphs[var_no], ccg_parent,
                        global_to_ccg_parent[var_post], fop, end_cond));
        }
    }
}

bool DomainTransitionGraphSymb::add_relevant_functional_vars_to_context(int var_no,
        map<int, int> &global_to_ccg_parent)
{
    // insert all variables on direct paths in the causal graph from var_no
    // (primitive functional) to a relevant comparison
    vector<int> comp_vars;
    g_causal_graph->get_comp_vars_for_func_var(var_no, comp_vars);
    bool var_influences_important_comp_var = false;
    for(int s = 0; s < comp_vars.size(); ++s) {
        if(global_to_ccg_parent.count(comp_vars[s])) {
            var_influences_important_comp_var = true;
            set<int> intermediate_vars;
            g_causal_graph->get_functional_vars_in_unrolled_term(comp_vars[s], intermediate_vars);
            set<int>::iterator it;
            for(it = intermediate_vars.begin(); it != intermediate_vars.end(); ++it) {
                translate_global_to_local(global_to_ccg_parent, *it);
            }
        }
    }
    return var_influences_important_comp_var;
}

void DomainTransitionGraphSymb::compress_effects(const Operator* op, vector<PrePost>& pre_post)
{
    const vector<PrePost> &pre_post_start = op->get_pre_post_start();
    const vector<PrePost> &pre_post_end = op->get_pre_post_end();
    for(int k = 0; k < pre_post_start.size(); ++k) {
        pre_post.push_back(pre_post_start[k]);
    }
    bool pre_post_overwritten = false;
    for(int k = 0; k < pre_post_end.size(); ++k) {
        for(int l = 0; l < pre_post.size(); l++) {
            if(pre_post_end[k].var == pre_post[l].var) {
                pre_post_overwritten = true;
                pre_post[l].post = pre_post_end[k].post;
                pre_post[l].var_post = pre_post_end[k].var_post;
                break;
            }
        }
        if(!pre_post_overwritten) {
            pre_post.push_back(pre_post_end[k]);
        }
        pre_post_overwritten = false;
    }
}

void DomainTransitionGraphSymb::dump() const
{
    cout << " (logical)" << endl;
    for(int i = 0; i < nodes.size(); i++) {
        cout << nodes[i].value << endl;
        vector<ValueTransition> transitions = nodes[i].transitions;
        for(int j = 0; j < transitions.size(); j++) {
            cout << " " << transitions[j].target->value << endl;
            vector<ValueTransitionLabel*> labels = transitions[j].ccg_labels;
            for(int k = 0; k < labels.size(); k++) {
                cout << "  at-start-durations:" << endl;
                cout << "   " << labels[k]->duration_variable << endl;
                cout << "  prevails:" << endl;
                vector<LocalAssignment> prevail = labels[k]->precond;
                for(int l = 0; l < prevail.size(); l++) {
                    cout << "   " << prevail[l].local_var << ":"
                        << prevail[l].value << endl;
                }
            }
        }
        cout << " global parents:" << endl;
        for(int i = 0; i < ccg_parents.size(); i++)
            cout << ccg_parents[i] << endl;
    }
}

void DomainTransitionGraphSymb::get_successors(int value, vector<int> &result) const
{
    assert(result.empty());
    assert(value >= 0 && value < nodes.size());
    const vector<ValueTransition> &transitions = nodes[value].transitions;
    result.reserve(transitions.size());
    for(int i = 0; i < transitions.size(); i++)
        result.push_back(transitions[i].target->value);
}

DomainTransitionGraphSubterm::DomainTransitionGraphSubterm(int var_index)
{
    is_axiom = g_axiom_layers[var_index] != -1;
    var = var_index;
}

void DomainTransitionGraphSubterm::read_data(istream &in)
{
    check_magic(in, "begin_DTG");
    in >> left_var >> op >> right_var;
    check_magic(in, "end_DTG");
}

void DomainTransitionGraphSubterm::dump() const
{
    cout << " (subterm)" << endl;
    cout << left_var << " " << op << " " << right_var << endl;
}

DomainTransitionGraphFunc::DomainTransitionGraphFunc(int var_index)
{
    is_axiom = g_axiom_layers[var_index] != -1;
    var = var_index;
}

void DomainTransitionGraphFunc::read_data(istream &in)
{
    check_magic(in, "begin_DTG");
    int number_of_transitions;
    in >> number_of_transitions;
    for(int i = 0; i < number_of_transitions; i++) {
        int op_index;
        in >> op_index;
        Operator *op = &g_operators[op_index];

        trans_type tt;
        in >> tt;

        assignment_op a_op;
        int influencing_variable;
        in >> a_op >> influencing_variable;
        int duration_variable;
        vector<LocalAssignment> prevails;
        vector<LocalAssignment> effects;

        binary_op bop;
        in >> bop >> duration_variable;

        if(bop != eq) {
            cout << "Error: The duration constraint must be of the form\n";
            cout << "       (= ?duration (arithmetic_term))" << endl;
            exit(1);
        }

        int count;

        in >> count; //number of  Conditions
        for(int i = 0; i < count; i++) {
            LocalAssignment prev_cond = LocalAssignment(in);
            prev_cond.prev_dtg = g_transition_graphs[prev_cond.local_var];
            prevails.push_back(prev_cond);
        }
        transitions.push_back(FuncTransitionLabel(var, prevails, effects, a_op,
                    influencing_variable, duration_variable, tt, op));
    }

    check_magic(in, "end_DTG");
}

LocalAssignment::LocalAssignment(istream &in)
{
    in >> local_var >> value >> cond_type;
}

void LocalAssignment::dump() const
{
    cout << local_var << ": " << value;
}

void DomainTransitionGraphFunc::dump() const
{
    cout << " (functional)" << endl;
    for(int i = 0; i < transitions.size(); i++) {
        cout << " durations_at_start:" << endl;
        cout << "  " << transitions[i].duration_variable << endl;
        cout << " conds:" << endl;
        for(int j = 0; j < transitions[i].precond.size(); j++) {
            cout << "  ";
            transitions[i].precond[j].dump();
        }
        cout << endl;
        cout << " effect: ";
        cout << transitions[i].a_op << " "
            << transitions[i].influencing_variable << endl;
    }
}

DomainTransitionGraphComp::DomainTransitionGraphComp(int var_index)
{
    is_axiom = g_axiom_layers[var_index] != -1;
    var = var_index;
}

void DomainTransitionGraphComp::read_data(istream &in)
{
    check_magic(in, "begin_DTG");
    check_magic(in, "1");
    in >> nodes.second.left_var >> nodes.second.op >> nodes.second.right_var;
    check_magic(in, "0");
    in >> nodes.first.left_var >> nodes.first.op >> nodes.first.right_var;
    check_magic(in, "end_DTG");
    assert(nodes.first.op != nodes.second.op || nodes.first.op == eq);
}

void DomainTransitionGraphComp::compute_recursively_parents(int var,
        map<int, int> &global_to_ccg_parent)
{
    if(g_variable_types[var] == primitive_functional) {
        // Processing for full DTG (cyclic CG).
        translate_global_to_local(global_to_ccg_parent, var);
        return;
    }
    if(g_variable_types[var] == subterm_functional) {
        translate_global_to_local(global_to_ccg_parent, var);
        DomainTransitionGraph* dtg = g_transition_graphs[var];
        DomainTransitionGraphSubterm* sdtg =
            dynamic_cast<DomainTransitionGraphSubterm*>(dtg);
        assert(sdtg);
        compute_recursively_parents(sdtg->left_var, global_to_ccg_parent);
        compute_recursively_parents(sdtg->right_var, global_to_ccg_parent);
        return;
    }
    assert(false);
}

int DomainTransitionGraph::translate_global_to_local(map<int, int> &global_to_ccg_parent, int global_var)
{
    int local_var;
    if(!global_to_ccg_parent.count(global_var)) {
        local_var = ccg_parents.size();
        global_to_ccg_parent[global_var] = local_var;
        ccg_parents.push_back(global_var);
        global_to_local_ccg_parents.insert(ValuePair(global_var, local_var));
    } else {
        local_var = global_to_ccg_parent[global_var];
    }
    return local_var;
}

void DomainTransitionGraphComp::collect_recursively_func_transitions(int var,
        map<int, int> &global_to_ccg_parent)
{
    DomainTransitionGraph* dtg = g_transition_graphs[var];
    if(g_variable_types[var] == primitive_functional) {
        DomainTransitionGraphFunc* fdtg =
            dynamic_cast<DomainTransitionGraphFunc*>(dtg);
        assert(fdtg);
        for(int i = 0; i < fdtg->transitions.size(); i++) {
            transitions.push_back(fdtg->transitions[i]);
            FuncTransitionLabel &trans = transitions.back();
            int starting_variable_global =
                trans.starting_variable;
            int starting_variable_local =
                translate_global_to_local(global_to_ccg_parent, starting_variable_global);
            trans.starting_variable = starting_variable_local;

            int influencing_variable_global =
                trans.influencing_variable;
            int influencing_variable_local =
                translate_global_to_local(global_to_ccg_parent, influencing_variable_global);
            trans.influencing_variable = influencing_variable_local;

            if(trans.duration_variable != -1) {
                int duration_variable_global = trans.duration_variable;
                int duration_variable_local = translate_global_to_local(
                    global_to_ccg_parent, duration_variable_global);
                trans.duration_variable = duration_variable_local;
            }
            vector<LocalAssignment> *prevails = &(trans.precond);
            for(int k = 0; k < prevails->size(); k++) {
                int global_var = (*prevails)[k].local_var;
                int local_var = translate_global_to_local(global_to_ccg_parent, global_var);
                (*prevails)[k].local_var = local_var;
            }
        }
        return;
    }
    if(g_variable_types[var] == subterm_functional) {
        DomainTransitionGraphSubterm* sdtg =
            dynamic_cast<DomainTransitionGraphSubterm*>(dtg);
        assert(sdtg);
        collect_recursively_func_transitions(sdtg->left_var,
                global_to_ccg_parent);
        collect_recursively_func_transitions(sdtg->right_var,
                global_to_ccg_parent);
        return;
    }
}

void DomainTransitionGraphComp::dump() const
{
    cout << " (comparison)" << endl;
    cout << "1: " << nodes.first.left_var << " " << nodes.first.op << " "
        << nodes.first.right_var << endl;
    cout << "0: " << nodes.second.left_var << " " << nodes.second.op << " "
        << nodes.second.right_var << endl;
    cout << "transitions: " << endl;
    for(int i = 0; i < transitions.size(); i++) {
        cout << " duration variable: ";
        cout << "  " << transitions[i].duration_variable;
        cout << endl;
        cout << " conds:" << endl;
        for(int j = 0; j < transitions[i].precond.size(); j++) {
            cout << "  ";
            transitions[i].precond[j].dump();
        }
        cout << endl;
        cout << " effect: ";
        cout << transitions[i].a_op << " "
            << transitions[i].influencing_variable << endl;
    }
    cout << "Context: ";
    for(int i = 0; i < ccg_parents.size(); i++) {
        cout << ccg_parents[i] << " ";
    }
    cout << endl;
}

class hash_pair_vector
{
    public:
        size_t operator()(const vector<pair<int, int> > &vec) const
        {
            unsigned long hash_value = 0;
            for(int i = 0; i < vec.size(); i++) {
                hash_value = 17 * hash_value + vec[i].first;
                hash_value = 17 * hash_value + vec[i].second;
            }
            return size_t(hash_value);
        }
};

