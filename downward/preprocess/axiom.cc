#include "helper_functions.h"
#include "axiom.h"
#include "variable.h"

#include <iostream>
#include <fstream>
#include <cassert>
using namespace std;

Axiom_relational::Axiom_relational(istream &in,
    const vector<Variable *> &variables) {
  check_magic(in, "begin_rule");
  int count; // number of conditions
  in >> count;
  for(int i = 0; i < count; i++) {
    int varNo, val;
    in >> varNo >> val;
    conditions.push_back(Condition(variables[varNo], val));
  }
  int varNo, oldVal, newVal;
  in >> varNo >> oldVal >> newVal;
  effect_var = variables[varNo];
  old_val = oldVal;
  effect_val = newVal;
  check_magic(in, "end_rule");
}

bool Axiom_relational::is_redundant() const {
  return effect_var->get_level() == -1;
}

void strip_Axiom_relationals(vector<Axiom_relational> &axiom_relationals) {
  int old_count = axiom_relationals.size();
  int new_index = 0;
  for(int i = 0; i < axiom_relationals.size(); i++)
    if(!axiom_relationals[i].is_redundant())
      axiom_relationals[new_index++] = axiom_relationals[i];
  axiom_relationals.erase(axiom_relationals.begin() + new_index,
      axiom_relationals.end());
  cout << axiom_relationals.size() << " of " << old_count
      << " Axiom_relational rules necessary." << endl;
}

void strip_Axiom_functionals(vector<Axiom_functional> &axiom_functionals) {
  int old_count = axiom_functionals.size();
  int new_index = 0;
  for(int i = 0; i < axiom_functionals.size(); i++)
    if(!axiom_functionals[i].is_redundant())
      axiom_functionals[new_index++] = axiom_functionals[i];
  axiom_functionals.erase(axiom_functionals.begin() + new_index,
      axiom_functionals.end());
  cout << axiom_functionals.size() << " of " << old_count
      << " Axiom_functional rules necessary." << endl;
}

void Axiom_relational::dump() const {
  cout << "relational axiom:" << endl;
  cout << "conditions:";
  for(int i = 0; i < conditions.size(); i++)
    cout << "  " << conditions[i].var->get_name() << " := "
	<< conditions[i].cond;
  cout << endl;
  cout << "derived:" << endl;
  cout << effect_var->get_name() << " -> " << effect_val << endl;
  cout << endl;
}

void Axiom_relational::generate_cpp_input(ofstream &outfile) const {
  assert(effect_var->get_level() != -1);
  outfile << "begin_rule" << endl;
  outfile << conditions.size() << endl;
  for(int i = 0; i < conditions.size(); i++) {
    assert(conditions[i].var->get_level() != -1);
    outfile << conditions[i].var->get_level() << " "<< conditions[i].cond
	<< endl;
  }
  outfile << effect_var->get_level() << " " << old_val << " " << effect_val
      << endl;
  outfile << "end_rule" << endl;
}

Axiom_functional::Axiom_functional(istream &in,
    const vector<Variable *> &variables, bool comparison) {
  this->comparison = false;
  int varNo, varNo1, varNo2;
  in >> varNo;
  if(comparison) {
      in >> cop;
  } else {
      in >> fop;
  }
  in >> varNo1 >> varNo2;
  effect_var = variables[varNo];
  if(comparison == true) {
    effect_var->set_comparison();
    this->set_comparison();
  } else {
    effect_var->set_subterm();
  }
  left_var = variables[varNo1];
  right_var = variables[varNo2];
}

bool Axiom_functional::is_redundant() const {
  return effect_var->get_level() == -1;
}

void Axiom_functional::dump() const {
  cout << "functional ";
  if(this->is_comparison()) {
    cout << "(comparison) ";
  } else {
    cout << "(subterm) ";
  }
  cout << "axiom" << endl;

  cout << effect_var->get_name() << " contains the value of "
      << left_var->get_name() << " ";
  if(is_comparison()) {
      cout << cop;
  } else {
      cout << fop;
  }
  cout << " " << right_var->get_name() << endl;
  cout << endl;
}

void Axiom_functional::generate_cpp_input(ofstream &outfile) const {
  assert(effect_var->get_level() != -1);
  outfile << effect_var->get_level() << " ";
  if(is_comparison()) {
      outfile << cop;
  } else {
      outfile << fop;
  }
  outfile << " " << left_var->get_level() << " " << right_var->get_level() << endl;
}
