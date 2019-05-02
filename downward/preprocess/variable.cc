#include "variable.h"

#include <cassert>
using namespace std;

Variable::Variable(istream &in) {
  in >> name >> range >> layer;
  level = -1;
  necessary = false;
  used_in_duration_condition = false;
  subterm = false;
  comparison = false;
  if (range == -1)
    functional = true;
  else
    functional = false;
}

void Variable::set_level(int theLevel) {
  assert(level == -1);
  level = theLevel;
}

int Variable::get_level() const {
  return level;
}

void Variable::set_necessary() {
  //assert(necessary == false);
  necessary = true;
}

int Variable::get_range() const {
  return range;
}

string Variable::get_name() const {
  return name;
}

bool Variable::is_necessary() const {
  return necessary;
}

bool Variable::is_functional() const {
  return functional;
}

bool Variable::is_used_in_duration_condition() const {
  return used_in_duration_condition;
}

void Variable::set_used_in_duration_condition() {
  used_in_duration_condition = true;
}

bool Variable::is_subterm() const {
  return subterm;
}

void Variable::set_subterm() {
  subterm = true;
}

bool Variable::is_comparison() const {
  return comparison;
}

void Variable::set_comparison() {
  comparison = true;
}

void Variable::dump() const {
  cout << name << " [range " << range;
  if(level != -1)
    cout << "; level " << level;
  if(is_derived())
    cout << "; derived; layer: "<< layer;
  cout << "]" << endl;
}
