#ifndef DOMAIN_TRANSITION_GRAPH_FUNC_H
#define DOMAIN_TRANSITION_GRAPH_FUNC_H

#include <vector>
#include "domain_transition_graph.h"
using namespace std;

class Axiom_relational;
class Variable;

class DomainTransitionGraphFunc : public DomainTransitionGraph {
private:
  struct Transition {
    Transition(trans_type theType, int theOp) :
      type(theType), op(theOp) {
    }
    SetEdgeCondition set_condition;
    EdgeCondition condition;
    trans_type type;
    foperator fop;
    DurationCond duration;
    Variable *right_var;
    int op;
  };
  int level;
  vector<Transition> transitions;

public:
  DomainTransitionGraphFunc(const Variable &var);
  void addTransition(int foperator, int right_var, const Operator &op,
      int op_index, trans_type type, vector<Variable *> variables);
  void addAxRelTransition(int from, int to, const Axiom_relational &ax,
      int ax_index);
  void finalize();
  void dump() const;
  void generate_cpp_input(ofstream &outfile) const;
  bool is_strongly_connected() const;
};

#endif
