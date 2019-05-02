#ifndef CAUSAL_GRAPH_H
#define CAUSAL_GRAPH_H

#include <iosfwd>
#include <vector>
#include <set>
#include <map>
using namespace std;

class CausalGraph
{
        vector<vector<int> > arcs;
        vector<vector<int> > edges;
    public:
        CausalGraph(istream &in);
        ~CausalGraph()
        {
        }
        const vector<int> &get_successors(int var) const;
        const vector<int> &get_neighbours(int var) const;
        void get_comp_vars_for_func_var(int var, vector<int>& comp_vars);
        void get_functional_vars_in_unrolled_term(int top_var, set<int>& intermediate_vars);
        void dump() const;
};

#endif
