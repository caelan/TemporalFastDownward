#ifndef DOMAIN_TRANSITION_GRAPH_SUBTERM_H
#define DOMAIN_TRANSITION_GRAPH_SUBTERM_H

#include <vector>
#include "domain_transition_graph.h"
#include "helper_functions.h"
#include "operator.h"
using namespace std;

class Axiom_relational;
class Variable;

class DomainTransitionGraphSubterm : public DomainTransitionGraph {

private:
  int level;
  Variable* left_var;
  Variable* right_var;
  compoperator cop;
  foperator fop;
  bool is_comparison;

public:
  DomainTransitionGraphSubterm(const Variable &var);
  void addTransition(int from, int to, const Operator &op, int op_index,
      trans_type type, vector<Variable *> variables);
  void addAxRelTransition(int from, int to, const Axiom_relational &ax,
      int ax_index);
  void setRelation(Variable* left_var,
      compoperator op, Variable* right_var);
  void setRelation(Variable* left_var,
      foperator op, Variable* right_var);
  void finalize();
  void dump() const;
  void generate_cpp_input(ofstream &outfile) const;
  bool is_strongly_connected() const;
};

#endif
