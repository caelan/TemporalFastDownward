#ifndef DOMAIN_TRANSITION_GRAPH_SYMB_H
#define DOMAIN_TRANSITION_GRAPH_SYMB_H

#include <vector>
#include "domain_transition_graph.h"
#include "helper_functions.h"
#include "operator.h"
using namespace std;

class Axiom_relational;
class Variable;

class DomainTransitionGraphSymb : public DomainTransitionGraph {

private:
  struct Transition {
    Transition(int theTarget, int theOp, trans_type theType) :
      target(theTarget), op(theOp), type(theType) {
    }
    bool operator==(const Transition &other) const {
      return target == other.target && op == other.op && condition
	  == other.condition && type == other.type;
    }
    bool operator<(const Transition &other) const;
    int target;
    int op;
    SetEdgeCondition set_condition;
    EdgeCondition condition;
    trans_type type;
    DurationCond duration;
    void dump();
  };
  typedef vector<Transition> Vertex;
  vector<Vertex> vertices;
  int level;

public:
  DomainTransitionGraphSymb(const Variable &var);
  void addTransition(int from, int to, const Operator &op, int op_index,
      trans_type type, vector<Variable *> variables);
  void addAxRelTransition(int from, int to, const Axiom_relational &ax,
      int ax_index);
  void finalize();
  void dump() const;
  void generate_cpp_input(ofstream &outfile) const;
  bool is_strongly_connected() const;
};

#endif
