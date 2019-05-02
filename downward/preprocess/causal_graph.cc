/* Computes the levels of the variables and returns an ordering of the variables 
 * with respect to their levels, beginning with the lowest-level variable.
 *
 * The new order does not contain any variables that are unimportant for the 
 * problem, i.e. variables that don't occur in the goal and don't influence 
 * important variables (== there is no path in the causal graph from this variable
 * through an important variable to the goal).
 * 
 * The constructor takes variables and operators and constructs a graph such that
 * there is a weighted edge from a variable A to a variable B with weight n+m if 
 * there are n operators that have A as prevail condition or postcondition or 
 * effect condition for B and that have B as postcondition, and if there are 
 * m axioms that have A as condition and B as effect. 
 * This graph is used for calculating strongly connected components (using "scc")
 * wich are returned in order (lowest-level component first).
 * In each scc with more than one variable cycles are then eliminated by 
 * succesivly taking out edges with minimal weight (in max_dag), returning a 
 * variable order.
 * Last, unimportant variables are identified and eliminated from the new order,
 * which is returned when "get_variable_ordering" is called.
 * Note: The causal graph will still contain the unimportant vars, but they are 
 * suppressed in the input for the search programm.
 */

#include "causal_graph.h"
#include "max_dag.h"
#include "operator.h"
#include "axiom.h"
#include "scc.h"
#include "variable.h"

#include <iostream>
#include <cassert>
using namespace std;

bool g_do_not_prune_variables = false;

void CausalGraph::weigh_graph_from_ops(const vector<Variable *> &,
    const vector<Operator> &operators, const vector<pair<Variable *, int> >&) {
  for(int i = 0; i < operators.size(); i++) {
//    cout << i << "th of " << operators.size()-1 << " operators: "
//	<< operators[i].get_name() << endl;
    const vector<Operator::Prevail> &prevail_start =
	operators[i].get_prevail_start();
    const vector<Operator::Prevail> &prevail_overall =
	operators[i].get_prevail_overall();
    const vector<Operator::Prevail> &prevail_end =
	operators[i].get_prevail_end();
    const vector<Operator::PrePost> &pre_post_start =
	operators[i].get_pre_post_start();
    const vector<Operator::PrePost> &pre_post_end =
	operators[i].get_pre_post_end();
    const vector<Operator::NumericalEffect> &numerical_effs_start =
	operators[i].get_numerical_effs_start();
    const vector<Operator::NumericalEffect> &numerical_effs_end =
	operators[i].get_numerical_effs_end();
    vector<Variable *> source_vars;
    for(int j = 0; j < prevail_start.size(); j++)
      source_vars.push_back(prevail_start[j].var);
    for(int j = 0; j < prevail_overall.size(); j++)
      source_vars.push_back(prevail_overall[j].var);
    for(int j = 0; j < prevail_end.size(); j++)
      source_vars.push_back(prevail_end[j].var);
    for(int j = 0; j < pre_post_start.size(); j++)
      //if(pre_post_start[j].pre != -1)
      source_vars.push_back(pre_post_start[j].var);
    for(int j = 0; j < pre_post_end.size(); j++)
      //if(pre_post_end[j].pre != -1)
      source_vars.push_back(pre_post_end[j].var);

    for(int k = 0; k < pre_post_start.size(); k++) {
      Variable *curr_target = pre_post_start[k].var;
      if(pre_post_start[k].is_conditional_effect) {
	for(int l = 0; l < pre_post_start[k].effect_conds_start.size(); l++)
	  source_vars.push_back(pre_post_start[k].effect_conds_start[l].var);
	for(int l = 0; l < pre_post_start[k].effect_conds_overall.size(); l++)
	  source_vars.push_back(pre_post_start[k].effect_conds_overall[l].var);
	for(int l = 0; l < pre_post_start[k].effect_conds_end.size(); l++)
	  source_vars.push_back(pre_post_start[k].effect_conds_end[l].var);
      }
      for(int j = 0; j < source_vars.size(); j++) {
	Variable *curr_source = source_vars[j];
	count(curr_source, curr_target);
      }
      if(pre_post_start[k].is_conditional_effect) {
	source_vars.erase(source_vars.end()
	    - pre_post_start[k].effect_conds_start.size()
	    - pre_post_start[k].effect_conds_overall.size()
	    - pre_post_start[k].effect_conds_end.size(), source_vars.end());
      }
    }
    for(int k = 0; k < pre_post_end.size(); k++) {
      Variable *curr_target = pre_post_end[k].var;
      if(pre_post_end[k].is_conditional_effect) {
	for(int l = 0; l < pre_post_end[k].effect_conds_start.size(); l++)
	  source_vars.push_back(pre_post_end[k].effect_conds_start[l].var);
	for(int l = 0; l < pre_post_end[k].effect_conds_overall.size(); l++)
	  source_vars.push_back(pre_post_end[k].effect_conds_overall[l].var);
	for(int l = 0; l < pre_post_end[k].effect_conds_end.size(); l++)
	  source_vars.push_back(pre_post_end[k].effect_conds_end[l].var);
      }
      for(int j = 0; j < source_vars.size(); j++) {
	Variable *curr_source = source_vars[j];
	count(curr_source, curr_target);
      }

      if(pre_post_end[k].is_conditional_effect) {
	source_vars.erase(source_vars.end()
	    - pre_post_end[k].effect_conds_start.size()
	    - pre_post_end[k].effect_conds_overall.size()
	    - pre_post_end[k].effect_conds_end.size(), source_vars.end());
      }
    }
    for(int k = 0; k < numerical_effs_start.size(); k++) {
      Variable *curr_target = numerical_effs_start[k].var;
      if(numerical_effs_start[k].is_conditional_effect) {
	for(int l = 0; l < numerical_effs_start[k].effect_conds_start.size(); l++)
	  source_vars.push_back(numerical_effs_start[k].effect_conds_start[l].var);
	for(int l = 0; l < numerical_effs_start[k].effect_conds_overall.size(); l++)
	  source_vars.push_back(numerical_effs_start[k].effect_conds_overall[l].var);
	for(int l = 0; l < numerical_effs_start[k].effect_conds_end.size(); l++)
	  source_vars.push_back(numerical_effs_start[k].effect_conds_end[l].var);
      }
      source_vars.push_back(numerical_effs_start[k].foperand);
      for(int j = 0; j < source_vars.size(); j++) {
	Variable *curr_source = source_vars[j];
	count(curr_source, curr_target);
      }
      source_vars.pop_back();//foperand
      if(numerical_effs_start[k].is_conditional_effect)
	source_vars.erase(source_vars.end()
	    - numerical_effs_start[k].effect_conds_start.size()
	    - numerical_effs_start[k].effect_conds_overall.size()
	    - numerical_effs_start[k].effect_conds_end.size(),
	    source_vars.end());
    }
    for(int k = 0; k < numerical_effs_end.size(); k++) {
      Variable *curr_target = numerical_effs_end[k].var;
      if(numerical_effs_end[k].is_conditional_effect) {
	for(int l = 0; l < numerical_effs_end[k].effect_conds_start.size(); l++)
	  source_vars.push_back(numerical_effs_end[k].effect_conds_start[l].var);
	for(int l = 0; l < numerical_effs_end[k].effect_conds_overall.size(); l++)
	  source_vars.push_back(numerical_effs_end[k].effect_conds_overall[l].var);
	for(int l = 0; l < numerical_effs_end[k].effect_conds_end.size(); l++)
	  source_vars.push_back(numerical_effs_end[k].effect_conds_end[l].var);
      }
      source_vars.push_back(numerical_effs_end[k].foperand);

      for(int j = 0; j < source_vars.size(); j++) {
	Variable *curr_source = source_vars[j];
	count(curr_source, curr_target);
      }
      source_vars.pop_back();//foperand
      if(numerical_effs_end[k].is_conditional_effect)
	source_vars.erase(source_vars.end()
	    - numerical_effs_end[k].effect_conds_start.size()
	    - numerical_effs_end[k].effect_conds_overall.size()
	    - numerical_effs_end[k].effect_conds_end.size(), source_vars.end());
    }
  }
}

void CausalGraph::count(Variable *curr_source, Variable *curr_target) {
  WeightedSuccessors &weighted_succ = weighted_graph[curr_source];
  if(predecessor_graph.count(curr_target) == 0)
    predecessor_graph[curr_target] = Predecessors();
  if(curr_source != curr_target) {
    if(weighted_succ.find(curr_target) != weighted_succ.end()) {
      weighted_succ[curr_target]++;
      predecessor_graph[curr_target][curr_source]++;
    } else {
      weighted_succ[curr_target] = 1;
      predecessor_graph[curr_target][curr_source] = 1;
    }
//    cout << "weighing edge from " << curr_source->get_name() << " to "
//	<< curr_target->get_name() << endl;
  }
}

void CausalGraph::weigh_graph_from_axioms_rel(const vector<Variable *> &,
    const vector<Axiom_relational> &axioms_rel,
    const vector<pair<Variable *, int> >&) {
  for(int i = 0; i < axioms_rel.size(); i++) {
//    cout << i << "th of " << axioms_rel.size()-1 << " relational axioms: "
//	<< endl;
    const vector<Condition> &conds = axioms_rel[i].get_conditions();
    vector<Variable *> source_vars;
    for(int j = 0; j < conds.size(); j++)
      source_vars.push_back(conds[j].var);

    for(int j = 0; j < source_vars.size(); j++) {
      Variable *curr_source = source_vars[j];
      // only one target var: the effect var of axiom[i]
      Variable *curr_target = axioms_rel[i].get_effect_var();
      count(curr_source, curr_target);
    }
  }
}

void CausalGraph::weigh_graph_from_axioms_func(const vector<Variable *> &,
    const vector<Axiom_functional> &axioms_func,
    const vector<pair<Variable *, int> >&) {
  for(int i = 0; i < axioms_func.size(); i++) {
//    cout << i << "th of " << axioms_func.size()-1 << " functional axioms: "
//	<< endl;
    count(axioms_func[i].get_left_var(), axioms_func[i].get_effect_var());
    count(axioms_func[i].get_right_var(), axioms_func[i].get_effect_var());
  }
}

CausalGraph::CausalGraph(const vector<Variable *> &the_variables,
    const vector<Operator> &the_operators,
    const vector<Axiom_relational> &the_axioms_rel,
    const vector<Axiom_functional> &the_axioms_func,
    const vector<pair<Variable *, int> > &the_goals) :
  variables(the_variables), operators(the_operators),
      axioms_rel(the_axioms_rel), axioms_func(the_axioms_func),
      goals(the_goals), acyclic(false) {

  for(int i = 0; i < variables.size(); i++)
    weighted_graph[variables[i]] = WeightedSuccessors();
  weigh_graph_from_ops(variables, operators, goals);
  weigh_graph_from_axioms_rel(variables, axioms_rel, goals);
  weigh_graph_from_axioms_func(variables, axioms_func, goals);
  //dump();

  Partition sccs;
  get_strongly_connected_components(sccs);

  cout << "The causal graph is " << (sccs.size() == variables.size() ? ""
      : "not ") << "acyclic." << endl;
  /*
   if (sccs.size() != variables.size()) {
   cout << "Components: " << endl;
   for(int i = 0; i < sccs.size(); i++) {
   for(int j = 0; j < sccs[i].size(); j++)
   cout << " " << sccs[i][j]->get_name();
   cout << endl;
   }
   }
   */
  calculate_pseudo_sort();
  //calculate_topological_pseudo_sort(sccs);
  calculate_important_vars();

//   cout << "new variable order: ";
//   for(int i = 0; i < ordering.size(); i++)
//     cout << ordering[i]->get_name()<<" - "; 
//   cout << endl;
}

void CausalGraph::calculate_pseudo_sort() {
  ordering = variables;
}

void CausalGraph::calculate_topological_pseudo_sort(const Partition &sccs) {
  map<Variable *, int> goal_map;
  for(int i = 0; i < goals.size(); i++)
    goal_map[goals[i].first] = goals[i].second;
  for(int scc_no = 0; scc_no < sccs.size(); scc_no++) {
    const vector<Variable *> &curr_scc = sccs[scc_no];
    if(curr_scc.size()> 1) {
      // component needs to be turned into acyclic subgraph  

      // Map variables to indices in the strongly connected component.
      map<Variable *, int> variableToIndex;
      for(int i = 0; i < curr_scc.size(); i++)
	variableToIndex[curr_scc[i]] = i;

      // Compute subgraph induced by curr_scc and convert the successor
      // representation from a map to a vector.
      vector<vector<pair<int, int> > > subgraph;
      for(int i = 0; i < curr_scc.size(); i++) {
	// For each variable in component only list edges inside component.
	WeightedSuccessors &all_edges = weighted_graph[curr_scc[i]];
	vector<pair<int, int> > subgraph_edges;
	for(WeightedSuccessors::const_iterator curr = all_edges.begin(); curr
	    != all_edges.end(); ++curr) {
	  Variable *target = curr->first;
	  int cost = curr->second;
	  map<Variable *, int>::iterator index_it = variableToIndex.find(target);
	  if(index_it != variableToIndex.end()) {
	    int new_index = index_it->second;
	    if(goal_map.find(target) != goal_map.end()) {
	      //TODO: soll das so bleiben? (Zahl taucht in max_dag auf)
	      // target is goal
	      subgraph_edges.push_back(make_pair(new_index, 100000 + cost));
	    }
	    subgraph_edges.push_back(make_pair(new_index, cost));
	  }
	}
	subgraph.push_back(subgraph_edges);
      }

      vector<int> order = MaxDAG(subgraph).get_result();
      for(int i = 0; i < order.size(); i++) {
	ordering.push_back(curr_scc[order[i]]);
      }
    } else {
      ordering.push_back(curr_scc[0]);
    }
  }
}

void CausalGraph::get_strongly_connected_components(Partition &result) {
  map<Variable *, int> variableToIndex;
  for(int i = 0; i < variables.size(); i++)
    variableToIndex[variables[i]] = i;

  vector<vector<int> > unweighted_graph;
  unweighted_graph.resize(variables.size());
  for(WeightedGraph::const_iterator it = weighted_graph.begin(); it
      != weighted_graph.end(); ++it) {
    int index = variableToIndex[it->first];
    vector<int> &succ = unweighted_graph[index];
    const WeightedSuccessors &weighted_succ = it->second;
    for(WeightedSuccessors::const_iterator it = weighted_succ.begin(); it
	!= weighted_succ.end(); ++it)
      succ.push_back(variableToIndex[it->first]);
  }

  vector<vector<int> > int_result = SCC(unweighted_graph).get_result();

  result.clear();
  for(int i = 0; i < int_result.size(); i++) {
    vector<Variable *> component;
    for(int j = 0; j < int_result[i].size(); j++)
      component.push_back(variables[int_result[i][j]]);
    result.push_back(component);
  }
}
void CausalGraph::calculate_important_vars() {
  //variables occuring in a duration constrain are necessary!
  for(int i = 0; i < variables.size(); i++) {
    if(variables[i]->is_used_in_duration_condition()) {
      variables[i]->set_necessary();
      dfs(variables[i]);
    }
  }
  for(int i = 0; i < goals.size(); i++) {
    if(!goals[i].first->is_necessary()) {
      //cout << "var " << goals[i].first->get_name() <<" is directly neccessary." 
      // << endl;
      goals[i].first->set_necessary();
      dfs(goals[i].first);
    }
  }
  // change ordering to leave out unimportant vars
  vector<Variable *> new_ordering;
  int old_size = ordering.size();
  for(int i = 0; i < old_size; i++)
    if(ordering[i]->is_necessary() || g_do_not_prune_variables)
      new_ordering.push_back(ordering[i]);
    else {
	cout << "variable " << ordering[i]->get_name() << " is not necessary!" << endl;
    }
  ordering = new_ordering;
  for(int i = 0; i < ordering.size(); i++) {
    ordering[i]->set_level(i);
  }
  cout << ordering.size() << " variables of " << old_size << " necessary"
      << endl;
}

void CausalGraph::dfs(Variable *from) {
  for(Predecessors::iterator pred = predecessor_graph[from].begin(); pred
      != predecessor_graph[from].end(); ++pred) {
    Variable* curr_predecessor = pred->first;
    if(!curr_predecessor->is_necessary()) {
      curr_predecessor->set_necessary();
      //cout << "var " << curr_predecessor->get_name() <<" is neccessary." << endl;
      dfs(curr_predecessor);
    }
  }
}

const vector<Variable *> &CausalGraph::get_variable_ordering() const {
  return ordering;
}

bool CausalGraph::is_acyclic() const {
  return acyclic;
}

void CausalGraph::dump() const {
  for(WeightedGraph::const_iterator source = weighted_graph.begin(); source
      != weighted_graph.end(); ++source) {
    cout << "dependent on var " << source->first->get_name() << ": " << endl;
    const WeightedSuccessors &curr = source->second;
    for(WeightedSuccessors::const_iterator it = curr.begin(); it != curr.end(); ++it) {
      cout << "  [" << it->first->get_name() << ", " << it->second << "]"
	  << endl;
      //assert(predecessor_graph[it->first][source->first] == it->second); 
    }
  }
  for(PredecessorGraph::const_iterator source = predecessor_graph.begin(); source
      != predecessor_graph.end(); ++source) {
    cout << "var " << source->first->get_name() << " is dependent of: " << endl;
    const Predecessors &curr = source->second;
    for(Predecessors::const_iterator it = curr.begin(); it != curr.end(); ++it)
      cout << "  [" << it->first->get_name() << ", " << it->second << "]"
	  << endl;
  }
}
void CausalGraph::generate_cpp_input(ofstream &outfile,
    const vector<Variable *> & ordered_vars) const {
  //TODO: use const iterator!
  vector<WeightedSuccessors *> succs; // will be ordered like ordered_vars
  vector<int> number_of_succ; // will be ordered like ordered_vars
  succs.resize(ordered_vars.size());
  number_of_succ.resize(ordered_vars.size());
  for(WeightedGraph::const_iterator source = weighted_graph.begin(); source
      != weighted_graph.end(); ++source) {
    Variable * source_var = source->first;
    if(source_var->get_level() != -1) {
      // source variable influences others 
      WeightedSuccessors &curr = (WeightedSuccessors &) source->second;
      succs[source_var->get_level()] = &curr;
      // count number of influenced vars
      int num = 0;
      for(WeightedSuccessors::const_iterator it = curr.begin(); it
	  != curr.end(); ++it)
	if(it->first->get_level() != -1
	// && it->first->get_level() > source_var->get_level()
	)
	  num++;
      number_of_succ[source_var->get_level()] = num;
    }
  }
  for(int i = 0; i < ordered_vars.size(); i++) {
    WeightedSuccessors *curr = succs[i];
    // print number of variables influenced by variable i
    outfile << number_of_succ[i] << endl;
    for(WeightedSuccessors::const_iterator it = curr->begin(); it
	!= curr->end(); ++it) {
      if(it->first->get_level() != -1
      // && it->first->get_level() > ordered_vars[i]->get_level()
      )
	// the variable it->first is important and influenced by variable i
	// print level
	outfile << it->first->get_level() << endl;
    }
  }
}

