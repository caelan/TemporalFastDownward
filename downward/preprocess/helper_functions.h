#ifndef HELPERS_H
#define HELPERS_H

#include "state.h"
#include "variable.h"
#include "successor_generator.h"
#include "causal_graph.h"

#include <string>
#include <vector>
#include <iostream>

using namespace std;

class State;
class Operator;
class Axiom_relational;
class Axiom_functional;
class DomainTransitionGraph;

//void read_everything
void read_preprocessed_problem_description(istream &in,
    vector<Variable> &internal_variables, vector<Variable *> &variables,
    State &initial_state, vector<pair<Variable*, int> > &goals,
    vector<Operator> &operators, vector<Axiom_relational> &axioms_rel,
    vector<Axiom_functional> &axioms_func, bool& contains_quantified_conditions);

//void dump_everything
void dump_preprocessed_problem_description(const vector<Variable *> &variables,
    const State &initial_state, const vector<pair<Variable*, int> > &goals,
    const vector<Operator> &operators,
    const vector<Axiom_relational> &axioms_rel,
    const vector<Axiom_functional> &axioms_func);

void dump_DTGs(const vector<Variable *> &ordering,
    vector<DomainTransitionGraph*> &transition_graphs);

void generate_cpp_input(bool causal_graph_acyclic,
    const vector<Variable *> & ordered_var, const State &initial_state,
    const vector<pair<Variable*, int> > &goals,
    const vector<Operator> & operators,
    const vector<Axiom_relational> &axioms_rel,
    const vector<Axiom_functional> &axioms_func,
    const SuccessorGenerator &sg,
    const vector<DomainTransitionGraph*> transition_graphs,
    const CausalGraph &cg,
    bool contains_quantified_conditions);

void check_magic(istream &in, string magic);

enum foperator {assign=0, scale_up=1, scale_down=2, increase=3, decrease=4};
enum compoperator {lt=0, le=1, eq=2, ge=3, gt=4, ue=5};

istream& operator>>(istream &is, foperator &fop);

ostream& operator<<(ostream &os, const foperator &fop);

istream& operator>>(istream &is, compoperator &fop);

ostream& operator<<(ostream &os, const compoperator &fop);

enum condition_type {start_cond = 0, overall_cond = 1, end_cond = 2, ax_cond};

istream& operator>>(istream &is, condition_type &fop);

ostream& operator<<(ostream &os, const condition_type &fop);

enum trans_type {start = 0, end = 1, compressed = 2, ax_rel = 3};

istream& operator>>(istream &is, trans_type &fop);

ostream& operator<<(ostream &os, const trans_type &fop);

compoperator get_inverse_op(compoperator op);

struct DurationCond {
  compoperator op;
  Variable *var;
  DurationCond(compoperator o, Variable *v) :
    op(o), var(v) {
  }
  DurationCond() {}
};

template<typename T> vector<T> append(vector<T> &first, vector<T> &sec) {
  for(int i=0; i< sec.size(); i++) {
    first.push_back(sec[i]);
  }
  return first;
}

#endif
