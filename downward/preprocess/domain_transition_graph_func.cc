#include "domain_transition_graph_func.h"
#include "operator.h"
#include "axiom.h"
#include "variable.h"
#include "scc.h"

#include <algorithm>
#include <cassert>
#include <iostream>
using namespace std;

DomainTransitionGraphFunc::DomainTransitionGraphFunc(const Variable &var) {
  //cout << "creating functional DTG for " << var.get_name() << endl;
  transitions.clear();
  level = var.get_level();
  assert(level != -1);
}
void DomainTransitionGraphFunc::addTransition(int fop, int right_var,
    const Operator &op, int op_index, trans_type type,
    vector<Variable *> variables) {

  class self_fulfilled_ {
public:
    bool operator()(const vector<Operator::PrePost> &pre_post_start, const Variable *var,
	const int &val) {
      for(unsigned int j=0; j<pre_post_start.size(); j++)
	if(pre_post_start[j].var == var && pre_post_start[j].post == val)
	  return true;
      return false;
    }
  } self_fulfilled;

  Transition trans(type, op_index);
  SetEdgeCondition &cond = trans.set_condition;
  const vector<Operator::Prevail> &prevail_start = op.get_prevail_start();
  const vector<Operator::Prevail> &prevail_overall = op.get_prevail_overall();
  const vector<Operator::Prevail> &prevail_end = op.get_prevail_end();
  const vector<Operator::PrePost> &pre_post_start = op.get_pre_post_start();
  const vector<Operator::PrePost> &pre_post_end = op.get_pre_post_end();
  for(int i = 0; i < prevail_start.size(); i++)
    if(true) { // [cycles]
      // if(prevail_start[i].var->get_level() < level) // [no cycles]
      cond.insert(std::tr1::make_tuple(prevail_start[i].var, prevail_start[i].prev, start_cond));
    }
  for(int i = 0; i < prevail_overall.size(); i++)
    if(true) // [cycles]
      // if(prevail_overall[i].var->get_level() < level) // [no cycles]
      if(!self_fulfilled(pre_post_start, prevail_overall[i].var,
	  prevail_overall[i].prev))
      cond.insert(std::tr1::make_tuple(prevail_overall[i].var, prevail_overall[i].prev, overall_cond));
  for(int i = 0; i < prevail_end.size(); i++)
    if(true) // [cycles]
      // if(prevail_end[i].var->get_level() < level) // [no cycles]
      if(!self_fulfilled(pre_post_start, prevail_end[i].var,
	  prevail_end[i].prev))
      cond.insert(std::tr1::make_tuple(prevail_end[i].var, prevail_end[i].prev, end_cond));
  for(int i = 0; i < pre_post_start.size(); i++) {
    if(pre_post_start[i].var->get_level() != level && pre_post_start[i].pre
	!= -1) // [cycles]
      // if(pre_post_start[i].var->get_level() < level && pre_post_start[i].pre != -1) //[no cycles]
      cond.insert(std::tr1::make_tuple(pre_post_start[i].var, pre_post_start[i].pre, start_cond));
    else if(pre_post_start[i].var->get_level() == level
	&& pre_post_start[i].is_conditional_effect) {
      for(int j = 0; j < pre_post_start[i].effect_conds_start.size(); j++)
	cond.insert(std::tr1::make_tuple(pre_post_start[i].effect_conds_start[j].var,
	    pre_post_start[i].effect_conds_start[j].cond, start_cond));
      //      for(int j = 0; j < pre_post_start[i].effect_conds_overall.size(); j++)
      //	cond.insert(make_pair(pre_post_start[i].effect_conds_overall[j].var,
      //	    pre_post_start[i].effect_conds_overall[j].cond));
      //      for(int j = 0; j < pre_post_start[i].effect_conds_end.size(); j++)
      //	cond.insert(make_pair(pre_post_start[i].effect_conds_end[j].var,
      //	    pre_post_start[i].effect_conds_end[j].cond));
    }
  }
  for(int i = 0; i < pre_post_end.size(); i++) {
    if(pre_post_end[i].var->get_level() != level && pre_post_end[i].pre != -1) { // [cycles]
      // if(pre_post_end[i].var->get_level() < level && pre_post_end[i].pre != -1) //[no cycles]
      if(!self_fulfilled(pre_post_start, pre_post_end[i].var,
	  pre_post_end[i].pre))
      cond.insert(std::tr1::make_tuple(pre_post_end[i].var, pre_post_end[i].pre, end_cond));
    } else if(pre_post_end[i].var->get_level() == level
	&& pre_post_end[i].is_conditional_effect) {
      //      for(int j = 0; j < pre_post_end[i].effect_conds_start.size(); j++)
      //	cond.insert(make_pair(pre_post_end[i].effect_conds_start[j].var,
      //	    pre_post_end[i].effect_conds_start[j].cond));
      //      for(int j = 0; j < pre_post_end[i].effect_conds_overall.size(); j++)
      //	cond.insert(make_pair(pre_post_end[i].effect_conds_overall[j].var,
      //	    pre_post_end[i].effect_conds_overall[j].cond));
      for(int j = 0; j < pre_post_end[i].effect_conds_end.size(); j++)
	if(!self_fulfilled(pre_post_start,
	    pre_post_end[i].effect_conds_end[j].var,
	    pre_post_end[i].effect_conds_end[j].cond))
	cond.insert(std::tr1::make_tuple(pre_post_end[i].effect_conds_end[j].var,
	    pre_post_end[i].effect_conds_end[j].cond, end_cond));
    }
  }
  //write from set to vector
  EdgeCondition &cond_vec = trans.condition;
  for(SetEdgeCondition::iterator it = cond.begin(); it != cond.end(); ++it) {
//    cout << "writing [" << (*it).first->get_level() << "," << (*it).second << "]" << endl;
    cond_vec.push_back(*it);
  }

  trans.duration = op.get_duration_cond();
  trans.fop = static_cast<foperator>(fop);
  trans.right_var = variables[right_var];
  transitions.push_back(trans);
}

void DomainTransitionGraphFunc::addAxRelTransition(int from, int to,
    const Axiom_relational &ax, int ax_index) {
}

void DomainTransitionGraphFunc::finalize() {
}

bool DomainTransitionGraphFunc::is_strongly_connected() const {
  return false;
}

void DomainTransitionGraphFunc::dump() const {
  cout << "Functional DTG!" << endl;
  for(int i = 0; i < transitions.size(); i++) {
    cout << " Transition:" << endl;
    const Transition &trans = transitions[i];
    cout << "  durations " << trans.duration.op << " "
	<< trans.duration.var->get_name() << endl;
    for(int k = 0; k < trans.condition.size(); k++)
      cout << "  if " << std::tr1::get<0>(trans.condition[k])->get_name()
	  << " = " << std::tr1::get<1>(trans.condition[k]) << " (" << std::tr1::get<2>(trans.condition[k]) << ")" << endl;
    if(trans.type==start)
      cout << "  start effect" << endl;
    else
        if(trans.type==end)
      cout << "  end effect" << endl;
    else
      cout << "   " << "axiom" << endl;
    cout << "  " << trans.fop << " " << trans.right_var->get_name() << endl;

  }
}

void DomainTransitionGraphFunc::generate_cpp_input(ofstream &outfile) const {
  outfile << transitions.size() << endl;
  for(int i = 0; i < transitions.size(); i++) {
    const Transition &trans = transitions[i];
    outfile << trans.op << endl; // operator doing the transition
    outfile << trans.type << endl; //type of transition
    outfile << trans.fop << " " << trans.right_var->get_level() << endl;
    //duration
    outfile << trans.duration.op << " " << trans.duration.var->get_level()
	<< endl;
    //conditions
    outfile << trans.condition.size() << endl;
    for(int k = 0; k < trans.condition.size(); k++)
      outfile << std::tr1::get<0>(trans.condition[k])->get_level() << " "
	  << std::tr1::get<1>(trans.condition[k]) << " " << std::tr1::get<2>(trans.condition[k]) << endl;
  }
}
