#include "causal_graph.h"
#include "globals.h"
#include "domain_transition_graph.h"

#include <algorithm>
#include <iostream>
#include <cassert>
using namespace std;

CausalGraph::CausalGraph(istream &in)
{
    check_magic(in, "begin_CG");
    int var_count = g_variable_domain.size();
    arcs.resize(var_count);
    edges.resize(var_count);
    for(int from_node = 0; from_node < var_count; from_node++) {
        int num_succ;
        in >> num_succ;
        arcs[from_node].reserve(num_succ);
        for(int j = 0; j < num_succ; j++) {
            int to_node;
            in >> to_node;
            arcs[from_node].push_back(to_node);
            edges[from_node].push_back(to_node);
            edges[to_node].push_back(from_node);
        }
    }
    check_magic(in, "end_CG");

    for(int i = 0; i < var_count; i++) {
        sort(edges[i].begin(), edges[i].end());
        edges[i].erase(unique(edges[i].begin(), edges[i].end()), edges[i].end());
    }
}

const vector<int> &CausalGraph::get_successors(int var) const
{
    return arcs[var];
}

const vector<int> &CausalGraph::get_neighbours(int var) const
{
    return edges[var];
}

void CausalGraph::get_comp_vars_for_func_var(int var, vector<int>& comp_vars)
{
    for(int i = 0; i < arcs[var].size(); ++i) {
        if(g_variable_types[arcs[var][i]] == comparison) {
            comp_vars.push_back(arcs[var][i]);
        } else if(g_variable_types[arcs[var][i]] == subterm_functional) {
            get_comp_vars_for_func_var(arcs[var][i], comp_vars);
        }
    }
}

// return functional vars in unrolled comparison term or functional subterm
// (represented by top_var)
void CausalGraph::get_functional_vars_in_unrolled_term(int top_var,
        set<int>& intermediate_vars)
{
    intermediate_vars.insert(top_var);
    for(int i = 0; i < edges[top_var].size(); ++i) {
        int current_var = edges[top_var][i];
        bool current_is_predecessor_of_top_var = false;
        for(int j = 0; j < arcs[current_var].size(); ++j) {
            if(arcs[current_var][j] == top_var) {
                current_is_predecessor_of_top_var = true;
                break;
            }
        }
        if(!current_is_predecessor_of_top_var) {
            continue;
        }
        if(g_variable_types[current_var] == primitive_functional) {
            intermediate_vars.insert(current_var);
        } else {
            assert (g_variable_types[current_var] != comparison);
            if(g_variable_types[current_var] == subterm_functional) {
                get_functional_vars_in_unrolled_term(current_var, intermediate_vars);
            }
        }
    }
}

void CausalGraph::dump() const
{
    cout << "Causal graph: " << endl;
    for(int i = 0; i < arcs.size(); i++) {
        cout << "dependent on var " << g_variable_name[i] << ": " << endl;
        for(int j = 0; j < arcs[i].size(); j++)
            cout << "  " << g_variable_name[arcs[i][j]] << ",";
        cout << endl;
    }
}

