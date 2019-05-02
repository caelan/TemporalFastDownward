#ifndef STATE_H
#define STATE_H

#include <iostream>
#include <map>
#include <vector>
using namespace std;

class Variable;

class State {
  map<Variable *, double> values;
public:
  State() {} // TODO: Entfernen (erfordert kleines Redesign)
  State(istream &in, const vector<Variable *> &variables);

  const double operator[](Variable *var) const;
  void dump() const;
};

#endif
