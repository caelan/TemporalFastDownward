#include "domain_transition_graph.h"
#include "domain_transition_graph_symb.h"
#include "domain_transition_graph_func.h"
#include "domain_transition_graph_subterm.h"
#include "operator.h"
#include "axiom.h"
#include "variable.h"

#include <algorithm>
#include <cassert>
#include <iostream>
using namespace std;

void build_DTGs(const vector<Variable *> &var_order,
    const vector<Operator> &operators,
    const vector<Axiom_relational> &axioms_rel,
    const vector<Axiom_functional> &axioms_func,
    vector<DomainTransitionGraph *> &transition_graphs) {

  for(int i = 0; i < var_order.size(); i++) {
    Variable v = *var_order[i];
    if(v.is_subterm()||v.is_comparison()) {
      DomainTransitionGraphSubterm *dtg = new DomainTransitionGraphSubterm(v);
      transition_graphs.push_back(dtg);
    } else if(v.is_functional()) {
      DomainTransitionGraphFunc *dtg = new DomainTransitionGraphFunc(v);
      transition_graphs.push_back(dtg);
    } else {
      DomainTransitionGraphSymb *dtg = new DomainTransitionGraphSymb(v);
      transition_graphs.push_back(dtg);
    }
  }

  for(int i = 0; i < operators.size(); i++) {
    const Operator &op = operators[i];
    const vector<Operator::PrePost> &pre_post_start = op.get_pre_post_start();
    const vector<Operator::PrePost> &pre_post_end = op.get_pre_post_end();
    vector<std::pair<Operator::PrePost,trans_type> > pre_posts_to_add;
    for(int j = 0; j < pre_post_start.size(); ++j) {
      pre_posts_to_add.push_back(make_pair(pre_post_start[j],start));
//      pre_posts_to_add.push_back(make_pair(Operator::PrePost(
//                    pre_post_start[j].var, pre_post_start[j].post,
//                    pre_post_start[j].pre), DomainTransitionGraph::ax_rel));
    }
//    bool pre_post_overwritten = false;
//    for(int j = 0; j < pre_post_end.size(); ++j) {
//      for(int k = 0; k < pre_posts_to_add.size(); ++k) {
//	if(pre_post_end[j].var == pre_posts_to_add[k].first.var) {
////	  // and add additional end transition
////	  pre_posts_to_add.push_back(make_pair(Operator::PrePost(
////              pre_posts_to_add[k].first.var, pre_posts_to_add[k].first.post,
////              pre_post_end[j].post), DomainTransitionGraph::ax_rel));
//          // compress original transition!
//	  pre_posts_to_add[k].first.post = pre_post_end[j].post;
//	  pre_posts_to_add[k].second = end;
//	  pre_post_overwritten = true;
//	  break;
//	}
//    }
//    if(!pre_post_overwritten) {
//	pre_posts_to_add.push_back(make_pair(pre_post_end[j],end));
//      }
//      pre_post_overwritten = false;
//    }

    bool pre_post_overwritten = false;
    for(int j = 0; j < pre_post_end.size(); ++j) {
      for(int k = 0; k < pre_posts_to_add.size(); ++k) {
        if(pre_post_end[j].var == pre_posts_to_add[k].first.var) {
          int intermediate_value = pre_posts_to_add[k].first.post;
          // compress original transition!
          pre_posts_to_add[k].first.post = pre_post_end[j].post;
          pre_posts_to_add[k].second = end;
          pre_post_overwritten = true;
          // If the intermediate value is something meaningful (i.e. unequal to
          // <none_of_those>), we add an addtional start transition with the
          // intermediate value as target.
          if(intermediate_value < pre_posts_to_add[k].first.var->get_range() - 1) {
              pre_posts_to_add.push_back(make_pair(Operator::PrePost(
                      pre_posts_to_add[k].first.var,
                      pre_posts_to_add[k].first.pre,
                      intermediate_value), end));
          }
          break;
        }
    }
    if(!pre_post_overwritten) {
	pre_posts_to_add.push_back(make_pair(pre_post_end[j],end));
      }
      pre_post_overwritten = false;
    }

    for(int j = 0; j < pre_posts_to_add.size(); j++) {
      if(pre_posts_to_add[j].first.pre == pre_posts_to_add[j].first.post)
	continue;
      const Variable *var = pre_posts_to_add[j].first.var;
      assert(var->get_layer() == -1);
      int var_level = var->get_level();
      if(var_level != -1) {
	int pre = pre_posts_to_add[j].first.pre;
	int post = pre_posts_to_add[j].first.post;
	if(pre != -1) {
	  transition_graphs[var_level]->addTransition(pre, post, op, i,
	      pre_posts_to_add[j].second, var_order);
	} else {
	  for(int pre = 0; pre < var->get_range(); pre++)
	    if(pre != post) {
	      transition_graphs[var_level]->addTransition(pre, post, op, i,
	          pre_posts_to_add[j].second, var_order);
	    }
	}
      }
    }
    const vector<Operator::NumericalEffect> &numerical_effs_start =
	op.get_numerical_effs_start();
    for(int j = 0; j < numerical_effs_start.size(); j++) {
      const Variable *var = numerical_effs_start[j].var;
      int var_level = var->get_level();
      if(var_level != -1) {
	int foperator = static_cast<int>(numerical_effs_start[j].fop);
	int right_var = numerical_effs_start[j].foperand->get_level();
	transition_graphs[var_level]->addTransition(foperator, right_var, op,
	    i, start, var_order);
      }
    }
    const vector<Operator::NumericalEffect> &numerical_effs_end =
	op.get_numerical_effs_end();
    for(int j = 0; j < numerical_effs_end.size(); j++) {
      const Variable *var = numerical_effs_end[j].var;
      int var_level = var->get_level();
      if(var_level != -1) {
	int foperator = static_cast<int>(numerical_effs_end[j].fop);
	int right_var = numerical_effs_end[j].foperand->get_level();
	transition_graphs[var_level]->addTransition(foperator, right_var, op,
	    i, end, var_order);
      }
    }
  }

  for(int i = 0; i < axioms_rel.size(); i++) {
    const Axiom_relational &ax = axioms_rel[i];
    Variable *var = ax.get_effect_var();
    int var_level = var->get_level();
    assert(var->get_layer()> -1);
    assert(var_level != -1);
    int old_val = ax.get_old_val();
    int new_val = ax.get_effect_val();
    transition_graphs[var_level]->addAxRelTransition(old_val, new_val, ax, i);
  }

  for(int i = 0; i < axioms_func.size(); i++) {
    const Axiom_functional &ax = axioms_func[i];
    Variable *var = ax.get_effect_var();
    if(!var->is_necessary()&&!var->is_used_in_duration_condition())
      continue;
    int var_level = var->get_level();
    assert(var->get_layer()> -1);
    Variable* left_var = ax.get_left_var();
    Variable* right_var = ax.get_right_var();
    DomainTransitionGraphSubterm *dtgs = dynamic_cast<DomainTransitionGraphSubterm*>(transition_graphs[var_level]);
    assert(dtgs);
    if(var->is_comparison()) {
        dtgs->setRelation(left_var, ax.cop, right_var);
    } else {
        dtgs->setRelation(left_var, ax.fop, right_var);
    }
  }

  for(int i = 0; i < transition_graphs.size(); i++)
    transition_graphs[i]->finalize();
}

bool are_DTGs_strongly_connected(
    const vector<DomainTransitionGraph*> &transition_graphs) {
  bool connected = true;
  // no need to test last variable's dtg (highest level variable)
  for(int i = 0; i < transition_graphs.size() - 1; i++)
    if(!transition_graphs[i]->is_strongly_connected())
      connected = false;
  return connected;
}
