// TODO: document further

/* unary operators are constructed in "build_DTGs" by calling
 * "addTransition" with the appropriate arguments: for every
 * pre-postcondition pair of an operator, an edge in the
 * DTG is built between the corresponding values in the dtg of the
 * variable, annotated with all prevail conditions and preconditions and
 * effect conditions of that postcondition.
 */

#include "domain_transition_graph_symb.h"
#include "operator.h"
#include "axiom.h"
#include "variable.h"
#include "scc.h"

#include <algorithm>
#include <cassert>
#include <iostream>
using namespace std;

DomainTransitionGraphSymb::DomainTransitionGraphSymb(const Variable &var) {
  //cout << "creating symbolical DTG for " << var.get_name() << endl;
  int range = var.get_range();
  if(range == -1) {
    range = 0;
  }
  vertices.clear();
  vertices.resize(range);
  level = var.get_level();
  assert(level != -1);
}

void DomainTransitionGraphSymb::addTransition(int from, int to,
    const Operator &op, int op_index, trans_type type,
    vector<Variable *> variables) {

  class self_fulfilled_ {
  public:
    bool operator()(const vector<Operator::PrePost> &pre_post_start, const Variable *var,
        const int &val) {
      for(unsigned int j = 0; j < pre_post_start.size(); j++)
        if(pre_post_start[j].var == var && pre_post_start[j].post == val)
          return true;
      return false;
    }
  } self_fulfilled;

  Transition trans(to, op_index, type);
  SetEdgeCondition &cond = trans.set_condition;
  const vector<Operator::Prevail> &prevail_start = op.get_prevail_start();
  const vector<Operator::Prevail> &prevail_overall = op.get_prevail_overall();
  const vector<Operator::Prevail> &prevail_end = op.get_prevail_end();
  const vector<Operator::PrePost> &pre_post_start = op.get_pre_post_start();
  const vector<Operator::PrePost> &pre_post_end = op.get_pre_post_end();

  assert(type == compressed || type == end || type == start);

//  if(type == compressed) {
    // insert all at-start conditions
    for(int i = 0; i < prevail_start.size(); i++) {
      cond.insert(std::tr1::make_tuple(prevail_start[i].var, prevail_start[i].prev, start_cond));
    }
    // insert all overall conditions if not satiesfied by an at-start effect of the same operator
    for(int i = 0; i < prevail_overall.size(); i++) {
      if(!self_fulfilled(pre_post_start, prevail_overall[i].var,
          prevail_overall[i].prev)) {
        cond.insert(std::tr1::make_tuple(prevail_overall[i].var, prevail_overall[i].prev, overall_cond));
      }
    }

  // insert all end conditions if not satiesfied by an at-start effect of the same operator
  for(int i = 0; i < prevail_end.size(); i++) {
    if(!self_fulfilled(pre_post_start, prevail_end[i].var, prevail_end[i].prev)) {
      cond.insert(std::tr1::make_tuple(prevail_end[i].var, prevail_end[i].prev, end_cond));
    }
  }
//  }
//  if(type == compressed) {
    // for each start effect, if the effect affects the Variable of this DTG, insert its SAS-Precondition,
    // if it affects another variable, add its start effect conditions.
    for(int i = 0; i < pre_post_start.size(); i++) {
      if(pre_post_start[i].var->get_level() != level && pre_post_start[i].pre
          != -1) {
        cond.insert(std::tr1::make_tuple(pre_post_start[i].var, pre_post_start[i].pre, start_cond));
      } else if(pre_post_start[i].var->get_level() == level
          && pre_post_start[i].is_conditional_effect) {
        for(int j = 0; j < pre_post_start[i].effect_conds_start.size(); j++)
          cond.insert(std::tr1::make_tuple(pre_post_start[i].effect_conds_start[j].var,
              pre_post_start[i].effect_conds_start[j].cond, start_cond));
      }
    }

  // for each end effect, if the effect affects the Variable of this DTG, insert its SAS-Precondition,
  // if it affects another variable, add its end effect conditions (if they are not satiesfied by an
  // start effect of the same operator).
  for(int i = 0; i < pre_post_end.size(); i++) {
    if(pre_post_end[i].var->get_level() != level && pre_post_end[i].pre != -1) {
      if(!self_fulfilled(pre_post_start, pre_post_end[i].var,
          pre_post_end[i].pre)) {
        cond.insert(std::tr1::make_tuple(pre_post_end[i].var, pre_post_end[i].pre, end_cond));
      }
    } else if(pre_post_end[i].var->get_level() == level
        && pre_post_end[i].is_conditional_effect) {
      for(int j = 0; j < pre_post_end[i].effect_conds_end.size(); j++) {
        if(!self_fulfilled(pre_post_start,
            pre_post_end[i].effect_conds_end[j].var,
            pre_post_end[i].effect_conds_end[j].cond)) {
          cond.insert(std::tr1::make_tuple(pre_post_end[i].effect_conds_end[j].var,
              pre_post_end[i].effect_conds_end[j].cond, end_cond));
        }
      }
    }
//  }
  }
  //write from set to vector
  EdgeCondition &cond_vec = trans.condition;
  for(SetEdgeCondition::iterator it = cond.begin(); it != cond.end(); ++it) {
    //cout << "writing [" << (*it).first->get_level() << "," << (*it).second << "]" << endl;
    cond_vec.push_back(*it);
  }

  trans.duration = op.get_duration_cond();
  vertices[from].push_back(trans);
}

void DomainTransitionGraphSymb::addAxRelTransition(int from, int to,
    const Axiom_relational &ax, int ax_index) {
  Transition trans(to, ax_index, ax_rel);
  EdgeCondition &cond = trans.condition;
  const vector<Condition> &ax_conds = ax.get_conditions();
  for(int i = 0; i < ax_conds.size(); i++)
    if(true) // [cycles]
      // if(prevail[i].var->get_level() < level) // [no cycles]
      cond.push_back(std::tr1::make_tuple(ax_conds[i].var, ax_conds[i].cond, ax_cond));
  vertices[from].push_back(trans);
}

bool DomainTransitionGraphSymb::Transition::operator<(const Transition &other) const {
  if (target != other.target)
  return target < other.target;
  else if (duration.var != other.duration.var)
  return duration.var < other.duration.var;
  return (condition.size() < other.condition.size());
}

void DomainTransitionGraphSymb::Transition::dump() {
  cout << "target: " << target << ", dur_var: " << duration.var->get_level()
      << endl;
  for(int k = 0; k < condition.size(); k++) {
    cout << " " << std::tr1::get<0>(condition[k])->get_level() << " : "
	<< std::tr1::get<1>(condition[k]) << " (" << std::tr1::get<2>(condition[k]) << ")" << endl;
  }
}

void DomainTransitionGraphSymb::finalize() {
  for(int i = 0; i < vertices.size(); i++) {

//    if((level == 6) && (i == 0)) {
//      cout << "before sorting transitions: " << endl;
//      for(int j = 0; j < vertices[i].size(); j++)
//	vertices[i][j].dump();
//    }

//    for(int j = 0; j < vertices[i].size(); j++) {
//	cout << " " << j << endl;
//	vertices[i][j].dump();
//    }
    // For all sources, sort transitions according to targets and condition length
    sort(vertices[i].begin(), vertices[i].end());

    vertices[i].erase(unique(vertices[i].begin(), vertices[i].end()),
	vertices[i].end());

//    if((level == 6) && (i == 0)) {
//      cout << "after sorting transitions: " << endl;
//      for(int j = 0; j < vertices[i].size(); j++)
//	vertices[i][j].dump();
//    }

    // For all transitions, sort conditions (acc. to pointer addresses)
    for(int j = 0; j < vertices[i].size(); j++) {
      Transition &trans = vertices[i][j];
      sort(trans.condition.begin(), trans.condition.end());
    }

//    if((level == 6) && (i == 0)) {
//      cout << "after sorting conditions: " << endl;
//      for(int j = 0; j < vertices[i].size(); j++)
//	vertices[i][j].dump();
//    }

    // Look for dominated transitions
    vector<Transition> undominated_trans;
    vector<bool> is_dominated;
    is_dominated.resize(vertices[i].size(), false);
    for(int j = 0; j < vertices[i].size(); j++) {
      if(!is_dominated[j]) {
	Transition &trans = vertices[i][j];
	undominated_trans.push_back(trans);
	EdgeCondition &cond = trans.condition;
	int comp = j + 1; // compare transition no. j to no. comp
	// comp is dominated if it has same target and same duration and more conditions
	while(comp < vertices[i].size()) {
	  if(is_dominated[comp]) {
	    comp++;
	    continue;
	  }
	  Transition &other_trans = vertices[i][comp];
	  assert(other_trans.target >= trans.target);
	  if(other_trans.target != trans.target)
	    break; // transition and all after it have different targets
	  if(other_trans.duration.var->get_level()
	      != trans.duration.var->get_level())
	    break; // transition and all after it have different durations
	  else { //domination possible
	    if(other_trans.condition.size() < cond.size()) {
	      cout << "level: " << level << ", i: " << i << ", j: " << j;
	      assert(false);
	    }
	    if(cond.size() == 0) {
	      is_dominated[comp] = true; // comp is dominated
	      comp++;
	    } else {
	      bool same_conditions = true;
	      for(int k = 0; k < cond.size(); k++) {
		bool cond_k = false;
		for(int l = 0; l < other_trans.condition.size(); l++) {
		  if(std::tr1::get<0>(other_trans.condition[l]) > std::tr1::get<0>(cond[k])) {
		    break; // comp doesn't have this condition, not dominated
		  }
		  if(std::tr1::get<0>(other_trans.condition[l]) == std::tr1::get<0>(cond[k])
		      && std::tr1::get<1>(other_trans.condition[l]) == std::tr1::get<1>(cond[k])) {
		      // TODO: should we also check the type of the condition at this point?
		    cond_k = true;

		    break; // comp has this condition, look for next cond
		  }
		}
		if(!cond_k) { // comp is not dominated
		  same_conditions = false;
		  break;
		}
	      }
	      is_dominated[comp] = same_conditions;
	      comp++;
	    }
	  }
	}
      }
    }

    vertices[i].swap(undominated_trans);

//    if((level == 6) && (i == 0)) {
//      cout << "after searching for dominated transitions: " << endl;
//      for(int j = 0; j < vertices[i].size(); j++)
//	vertices[i][j].dump();
//    }

  }
}

bool DomainTransitionGraphSymb::is_strongly_connected() const {
  vector<vector<int> > easy_graph;
  for(int i = 0; i < vertices.size(); i++) {
    vector<int> empty_vector;
    easy_graph.push_back(empty_vector);
    for(int j = 0; j < vertices[i].size(); j++) {
      const Transition &trans = vertices[i][j];
      easy_graph[i].push_back(trans.target);
    }
  }
  vector<vector<int> > sccs = SCC(easy_graph).get_result();
  //  cout << "easy graph sccs for var " << level << endl;
  //   for(int i = 0; i < sccs.size(); i++) {
  //     for(int j = 0; j < sccs[i].size(); j++)
  //       cout << " " << sccs[i][j];
  //     cout << endl;
  //   }
  bool connected = false;
  if(sccs.size() == 1) {
    connected = true;
    //cout <<"is strongly connected" << endl;
  }
  //else cout << "not strongly connected" << endl;
  return connected;
}

void DomainTransitionGraphSymb::dump() const {
  cout << "Symbolical DTG!" << endl;
  for(int i = 0; i < vertices.size(); i++) {
    cout << "  From value " << i << ":" << endl;
    for(int j = 0; j < vertices[i].size(); j++) {
      const Transition &trans = vertices[i][j];
      cout << "    " << "To value " << trans.target << endl;
      if(trans.type == start || trans.type == end || trans.type == compressed)
	cout << "     duration " << trans.duration.op << " "
	    << trans.duration.var->get_level() << endl;
      else
	cout << "     " << "axiom" << endl;
      for(int k = 0; k < trans.condition.size(); k++)
	cout << "      if " << std::tr1::get<0>(trans.condition[k])->get_level() << " = "
	    << std::tr1::get<1>(trans.condition[k]) <<  " (" << std::tr1::get<2>(trans.condition[k]) << ")" << endl;
    }
  }
}

void DomainTransitionGraphSymb::generate_cpp_input(ofstream &outfile) const {
  //outfile << vertices.size() << endl; // the variable's range
  for(int i = 0; i < vertices.size(); i++) {
    outfile << vertices[i].size() << endl; // number of transitions from this value
    for(int j = 0; j < vertices[i].size(); j++) {
      const Transition &trans = vertices[i][j];
      outfile << trans.target << endl; // target of transition
      outfile << trans.op << endl; // operator doing the transition
      //duration:
      if(trans.type==compressed)
          outfile << "c" << endl;
      else if(trans.type ==start)
	outfile << "s" << endl;
      else if(trans.type==end)
	outfile << "e" << endl;
      else {
        assert(trans.type==ax_rel);
	outfile << "a" << endl;
      }
      if(trans.type == start || trans.type == end || trans.type == compressed)
	outfile << trans.duration.op << " " << trans.duration.var->get_level()
	    << endl;
      // calculate number of important prevail conditions
      int number = 0;
      for(int k = 0; k < trans.condition.size(); k++)
	if(std::tr1::get<0>(trans.condition[k])->get_level() != -1)
	  number++;
      outfile << number << endl;
      for(int k = 0; k < trans.condition.size(); k++)
	if(std::tr1::get<0>(trans.condition[k])->get_level() != -1)
	  outfile << std::tr1::get<0>(trans.condition[k])->get_level() << " "
	      << std::tr1::get<1>(trans.condition[k]) << " " << std::tr1::get<2>(trans.condition[k]) << endl; // condition: var, val
    }
  }
}

