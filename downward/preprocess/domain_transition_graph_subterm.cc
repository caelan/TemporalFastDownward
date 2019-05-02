#include "domain_transition_graph_subterm.h"
#include "operator.h"
#include "axiom.h"
#include "variable.h"
#include "scc.h"
#include "helper_functions.h"

#include <algorithm>
#include <cassert>
#include <iostream>
using namespace std;

DomainTransitionGraphSubterm::DomainTransitionGraphSubterm(const Variable &var) {
 // cout << "creating subterm DTG for " << var.get_name() << endl;
  level = var.get_level();
  if(var.is_comparison()) {
    is_comparison = true;
//    cout << "Comparison!" << endl;
  }
  else {
    is_comparison = false;
//    cout << "not Comparison!" << endl;
  }
  assert(level != -1);
}

void DomainTransitionGraphSubterm::addTransition(int from, int to,
    const Operator &op, int op_index, trans_type type,
    vector<Variable *> variables) {
}

void DomainTransitionGraphSubterm::addAxRelTransition(int from, int to,
    const Axiom_relational &ax, int ax_index) {
}

void DomainTransitionGraphSubterm::setRelation(Variable* left_var,
    compoperator op, Variable* right_var) {
  this->left_var = left_var;
  this->cop = op;
  this->right_var = right_var;
}

void DomainTransitionGraphSubterm::setRelation(Variable* left_var,
    foperator op, Variable* right_var) {
  this->left_var = left_var;
  this->fop = op;
  this->right_var = right_var;
}

void DomainTransitionGraphSubterm::finalize() {

}

bool DomainTransitionGraphSubterm::is_strongly_connected() const {
  return false;
}

void DomainTransitionGraphSubterm::dump() const {
  if(!is_comparison)
    cout << "Subterm DTG!" << endl;
  else
    cout << "Comparison DTG!" << endl;
  cout << " Expression : " << left_var->get_name() << " ";
  if(is_comparison) {
      cout << cop;
  } else {
      cout << fop;
  }
  cout << " " << right_var->get_name() << endl;
}

void DomainTransitionGraphSubterm::generate_cpp_input(ofstream &outfile) const {
  if(!is_comparison) {
    outfile << left_var->get_level() << " " << fop << " "
	<< right_var->get_level() << endl;
  } else {
    outfile << "1" << endl;
    outfile << left_var->get_level() << " " << get_inverse_op(cop) << " "
	<< right_var->get_level() << endl;
    outfile << "0" << endl;
    outfile << left_var->get_level() << " " << cop << " "
	<< right_var->get_level() << endl;
  }
}

