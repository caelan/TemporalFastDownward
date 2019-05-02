#ifndef DOMAIN_TRANSITION_GRAPH_H
#define DOMAIN_TRANSITION_GRAPH_H

#include <set>
#include "operator.h"
#include <tr1/tuple>

using namespace std;

class Axiom_relational;
class Variable;

class DomainTransitionGraph {
public:
  typedef std::tr1::tuple<const Variable*, double, condition_type> Edge;

  struct CompareCondsIgnoringType {
    bool operator() (const Edge& lhs, const Edge& rhs) const {
      if(std::tr1::get<0>(lhs) < std::tr1::get<0>(rhs))
        return true;
      else if(std::tr1::get<0>(lhs) > std::tr1::get<0>(rhs))
        return false;
      if(std::tr1::get<1>(lhs) < std::tr1::get<1>(rhs))
        return true;
      else
        return false;
    }
  };

  typedef set<Edge, CompareCondsIgnoringType > SetEdgeCondition;
  typedef vector<std::tr1::tuple<const Variable *, double, condition_type> > EdgeCondition;

  virtual ~DomainTransitionGraph() {};

  virtual void addTransition(int from, int to, const Operator &op,
      int op_index, trans_type type, vector<Variable *> variables) = 0;
  virtual void addAxRelTransition(int from, int to, const Axiom_relational &ax,
      int ax_index) = 0;
  virtual void finalize() = 0;
  virtual void dump() const = 0;
  virtual void generate_cpp_input(ofstream &outfile) const = 0;
  virtual bool is_strongly_connected() const = 0;

};

extern void build_DTGs(const vector<Variable *> &varOrder,
    const vector<Operator> &operators, const vector<Axiom_relational> &axioms,
    const vector<Axiom_functional> &axioms_func,
    vector<DomainTransitionGraph*> &transition_graphs);
extern bool are_DTGs_strongly_connected(
    const vector<DomainTransitionGraph*> &transition_graphs);

#endif
