#include <cstdlib>
#include <iostream>
#include <fstream>
#include <cassert>

#include <string>
#include <vector>
using namespace std;

#include "helper_functions.h"
#include "state.h"
#include "operator.h"
#include "axiom.h"
#include "variable.h"
#include "successor_generator.h"
#include "domain_transition_graph.h"

void check_magic(istream &in, string magic) {
  string word;
  in >> word;
  if(word != magic) {
    cout << "Failed to match magic word '" << magic << "'." << endl;
    cout << "Got '" << word << "'." << endl;
    exit(1);
  }
}

void read_variables(istream &in, vector<Variable> &internal_variables,
    vector<Variable *> &variables) {
  check_magic(in, "begin_variables");
  int count;
  in >> count;
  internal_variables.reserve(count);
  // Important so that the iterators stored in variables are valid.
  for(int i = 0; i < count; i++) {
    internal_variables.push_back(Variable(in));
    variables.push_back(&internal_variables.back());
  }
  check_magic(in, "end_variables");
}

void read_goal(istream &in, const vector<Variable *> &variables,
    vector<pair<Variable*, int> > &goals) {
  check_magic(in, "begin_goal");
  int count;
  in >> count;
  for(int i = 0; i < count; i++) {
    int varNo, val;
    in >> varNo >> val;
    goals.push_back(make_pair(variables[varNo], val));
  }
  check_magic(in, "end_goal");
}

void dump_goal(const vector<pair<Variable*, int> > &goals) {
  cout << "Goal Conditions:" << endl;
  for(int i = 0; i < goals.size(); i++)
    cout << "  " << goals[i].first->get_name() << ": " << goals[i].second
	<< endl;
}

void read_operators(istream &in, const vector<Variable *> &variables,
    vector<Operator> &operators) {
  int count;
  in >> count;
  for(int i = 0; i < count; i++)
    operators.push_back(Operator(in, variables));
}

void read_axioms_rel(istream &in, const vector<Variable *> &variables,
    vector<Axiom_relational> &axioms_rel) {
  int count;
  in >> count;
  for(int i = 0; i < count; i++)
    axioms_rel.push_back(Axiom_relational(in, variables));
}

void read_axioms_comp(istream &in, const vector<Variable *> &variables,
    vector<Axiom_functional> &axioms_func) {
  int count;
  in >> count;
  for(int i = 0; i < count; i++)
    axioms_func.push_back(Axiom_functional(in, variables, true));
}

void read_axioms_func(istream &in, const vector<Variable *> &variables,
    vector<Axiom_functional> &axioms_func) {
  int count;
  in >> count;
  for(int i = 0; i < count; i++)
    axioms_func.push_back(Axiom_functional(in, variables, false));
}

void read_contains_quantified_conditions(istream &in, bool& contains_quantified_conditions) {
  in >> contains_quantified_conditions;
}

void read_preprocessed_problem_description(istream &in,
    vector<Variable> &internal_variables, vector<Variable *> &variables,
    State &initial_state, vector<pair<Variable*, int> > &goals,
    vector<Operator> &operators, vector<Axiom_relational> &axioms_rel,
    vector<Axiom_functional> &axioms_func, bool& contains_quantified_conditions) {
  read_variables(in, internal_variables, variables);
  initial_state = State(in, variables);
  read_goal(in, variables, goals);
  read_operators(in, variables, operators);
  read_axioms_rel(in, variables, axioms_rel);
  read_axioms_comp(in, variables, axioms_func);
  read_axioms_func(in, variables, axioms_func);
  read_contains_quantified_conditions(in, contains_quantified_conditions);
}

void dump_preprocessed_problem_description(const vector<Variable *> &variables,
    const State &initial_state, const vector<pair<Variable*, int> > &goals,
    const vector<Operator> &operators,
    const vector<Axiom_relational> &axioms_rel,
    const vector<Axiom_functional> &axioms_func) {

  cout << "Variables (" << variables.size() << "):" << endl;
  for(int i = 0; i < variables.size(); i++)
    variables[i]->dump();

  cout << "Initial State:" << endl;
  initial_state.dump();
  dump_goal(goals);

  for(int i = 0; i < operators.size(); i++)
    operators[i].dump();
  for(int i = 0; i < axioms_rel.size(); i++)
    axioms_rel[i].dump();
  for(int i = 0; i < axioms_func.size(); i++)
    axioms_func[i].dump();
}

void dump_DTGs(const vector<Variable *> &ordering,
    vector<DomainTransitionGraph*> &transition_graphs) {
  for(int i = 0; i < transition_graphs.size(); i++) {
    cout << "Domain transition graph for variable " << ordering[i]->get_level()
	<< " (original name: " << ordering[i]->get_name() << endl;
    transition_graphs[i]->dump();
  }
}

void generate_cpp_input(bool solveable_in_poly_time,
    const vector<Variable *> & ordered_vars, const State &initial_state,
    const vector<pair<Variable*, int> > &goals,
    const vector<Operator> & operators,
    const vector<Axiom_relational> &axioms_rel,
    const vector<Axiom_functional> &axioms_func, const SuccessorGenerator &sg,
    const vector<DomainTransitionGraph*> transition_graphs,
    const CausalGraph &cg,
    bool contains_quantified_conditions) {
  ofstream outfile;
  outfile.open("output", ios::out);
  outfile << solveable_in_poly_time << endl; // 1 if true, else 0
  int var_count = ordered_vars.size();
  outfile << "begin_variables" << endl;
  outfile << var_count << endl;
  for(int i = 0; i < var_count; i++)
    outfile << ordered_vars[i]->get_name() << " "
	<< ordered_vars[i]->get_range() << " " << ordered_vars[i]->get_layer()
	<< endl;
  outfile << "end_variables" << endl;
  outfile << "begin_state" << endl;
  for(int i = 0; i < var_count; i++)
    outfile << initial_state[ordered_vars[i]] << endl; // for axioms default value
  outfile << "end_state" << endl;

  vector<int> ordered_goal_values;
  ordered_goal_values.resize(var_count, -1);
  for(int i = 0; i < goals.size(); i++) {
    int var_index = goals[i].first->get_level();
    ordered_goal_values[var_index] = goals[i].second;
  }
  outfile << "begin_goal" << endl;
  outfile << goals.size() << endl;
  for(int i = 0; i < var_count; i++)
    if(ordered_goal_values[i] != -1)
      outfile << i << " " << ordered_goal_values[i] << endl;
  outfile << "end_goal" << endl;

  outfile << operators.size() << endl;
  for(int i = 0; i < operators.size(); i++)
    operators[i].generate_cpp_input(outfile);

  outfile << axioms_rel.size() << endl;
  for(int i = 0; i < axioms_rel.size(); i++)
    axioms_rel[i].generate_cpp_input(outfile);

  outfile << axioms_func.size() << endl;
  for(int i = 0; i < axioms_func.size(); i++)
    axioms_func[i].generate_cpp_input(outfile);

  outfile << "begin_SG" << endl;
  cout << "printing SG " << endl;
  sg.generate_cpp_input(outfile);
  outfile << "end_SG" << endl;

  outfile << "begin_CG" << endl;
  cg.generate_cpp_input(outfile, ordered_vars);
  outfile << "end_CG" << endl;

  cout <<  var_count << endl;
  for(int i = 0; i < var_count; i++) {
    outfile << "begin_DTG" << endl;
    transition_graphs[i]->generate_cpp_input(outfile);
    outfile << "end_DTG" << endl;
  }

  outfile << contains_quantified_conditions << endl;

  outfile.close();
}

compoperator get_inverse_op(compoperator op) {
  switch (op) {
  case lt:
    return ge;
    break;
  case le:
    return gt;
    break;
  case eq:
    return ue;
    break;
  case ge:
    return lt;
    break;
  case gt:
    return le;
    break;
  case ue:
    return eq;
    break;
  default:
    cout << "inverse requestet for " << op << endl;
    assert(false);
	return eq; // arbitrary foperator
  }
}

istream& operator>>(istream &is, foperator &fop) {
  string strVal;
  is >> strVal;
  if(!strVal.compare("="))
    fop = assign;
  else if(!strVal.compare("+"))
    fop = increase;
  else if(!strVal.compare("-"))
    fop = decrease;
  else if(!strVal.compare("*"))
    fop = scale_up;
  else if(!strVal.compare("/"))
    fop = scale_down;
  else
    assert(false);
  return is;
}

ostream& operator<<(ostream &os, const foperator &fop) {
  switch (fop) {
  case assign:
    os << "=";
    break;
  case scale_up:
    os << "*";
    break;
  case scale_down:
    os << "/";
    break;
  case increase:
    os << "+";
    break;
  case decrease:
    os << "-";
    break;
  default:
    cout << (int)fop << " was read" << endl;
    assert(false);
  }
  return os;
}

istream& operator>>(istream &is, compoperator &fop) {
  string strVal;
  is >> strVal;
  if(!strVal.compare("<"))
    fop = lt;
  else if(!strVal.compare("<="))
    fop = le;
  else if(!strVal.compare("="))
    fop = eq;
  else if(!strVal.compare(">="))
    fop = ge;
  else if(!strVal.compare(">"))
    fop = gt;
  else if(!strVal.compare("!="))
    fop = ue;
  else
    assert(false);
  return is;
}

ostream& operator<<(ostream &os, const compoperator &fop) {
  switch (fop) {
  case lt:
    os << "<";
    break;
  case le:
    os << "<=";
    break;
  case eq:
    os << "=";
    break;
  case ge:
    os << ">=";
    break;
  case gt:
    os << ">";
    break;
  case ue:
    os << "!=";
    break;
  default:
    cout << fop << " WAS READ" << endl;
    assert(false);
  }
  return os;
}

istream& operator>>(istream &is, trans_type &tt) {
    string strVal;
    is >> strVal;
    if(!strVal.compare("s"))
        tt = start;
    else if(!strVal.compare("e"))
        tt = end;
    else if(!strVal.compare("c"))
        tt = compressed;
    else if(!strVal.compare("a"))
        tt = ax_rel;
    else {
        cout << strVal << " was read." << endl;
        assert(false);
    }
    return is;
}

ostream& operator<<(ostream &os, const trans_type &tt) {
    switch (tt) {
    case start:
        os << "s";
        break;
    case end:
        os << "e";
        break;
    case compressed:
        os << "c";
        break;
    case ax_rel:
        os << "a";
    default:
        // cout << "Error: Encountered binary operator " << bop << "." << endl;
        assert(false);
        break;
    }
    return os;
}

istream& operator>>(istream &is, condition_type &ct) {
  string strVal;
  is >> strVal;
  if(!strVal.compare("s"))
    ct = start_cond;
  else if(!strVal.compare("o"))
    ct = overall_cond;
  else if(!strVal.compare("e"))
    ct = end_cond;
  else if(!strVal.compare("a"))
    ct = ax_cond;
  else
    assert(false);
  return is;
}

ostream& operator<<(ostream &os, const condition_type &ct) {
  switch (ct) {
  case start_cond:
    os << "s";
    break;
  case overall_cond:
    os << "o";
    break;
  case end_cond:
    os << "e";
    break;
  case ax_cond:
    os << "a";
    break;
  default:
    assert(false);
  }
  return os;
}
