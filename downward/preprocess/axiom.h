#ifndef AXIOM_H
#define AXIOM_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
using namespace std;

class Variable;

struct Condition {
  Variable *var;
  int cond;
  Condition(Variable *v, int c) :
    var(v), cond(c) {
  }
};

class Axiom_relational {
public:

private:
  Variable *effect_var;
  int old_val;
  int effect_val;
  vector<Condition> conditions; // var, val
public:
  Axiom_relational(istream &in, const vector<Variable *> &variables);

  bool is_redundant() const;
  void dump() const;
  void generate_cpp_input(ofstream &outfile) const;
  const vector<Condition> &get_conditions() const {
    return conditions;
  }
  Variable* get_effect_var() const {
    return effect_var;
  }
  int get_old_val() const {
    return old_val;
  }
  int get_effect_val() const {
    return effect_val;
  }
};

class Axiom_functional {
public:

private:
  Variable *effect_var;
  Variable *left_var;
  Variable *right_var;
  bool comparison;
public:
  foperator fop;
  compoperator cop;
      Axiom_functional(istream &in, const vector<Variable *> &variables,
	  bool comparison);

  bool is_redundant() const;

  void dump() const;
  void generate_cpp_input(ofstream &outfile) const;
  Variable* get_effect_var() const {
    return effect_var;
  }
  Variable* get_left_var() const {
    return left_var;
  }
  Variable* get_right_var() const {
    return right_var;
  }
  foperator get_operator() const {
    return fop;
  }
  void set_operator(foperator _fop) {
      fop = _fop;
  }
  bool is_comparison() const {
    return comparison;
  }
  void set_comparison() {
    comparison = true;
  }
};

extern void strip_Axiom_relationals(vector<Axiom_relational> &axioms_rel);

extern void strip_Axiom_functionals(vector<Axiom_functional> &axioms_func);

#endif
