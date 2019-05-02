#ifndef OPERATOR_H
#define OPERATOR_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "helper_functions.h"
using namespace std;

class Variable;

class Operator {
public:
  struct Prevail {
    Variable *var;
    int prev;
    Prevail(Variable *v, int p) :
      var(v), prev(p) {
    }
  };
  struct EffCond {
    Variable *var;
    int cond;
    EffCond(Variable *v, int c) :
      var(v), cond(c) {
    }
  };
  struct PrePost {
    Variable *var;
    int pre, post;
    bool is_conditional_effect;
    vector<EffCond> effect_conds_start;
    vector<EffCond> effect_conds_overall;
    vector<EffCond> effect_conds_end;
    PrePost(Variable *v, int pr, int po) :
      var(v), pre(pr), post(po) {
      is_conditional_effect = false;
    }
    PrePost(Variable *v, vector<EffCond> ecs_start, vector<EffCond> ecs_overall,
			vector<EffCond> ecs_end, int pr, int po) :
      var(v), pre(pr), post(po), effect_conds_start(ecs_start),
	  effect_conds_overall(ecs_overall), effect_conds_end(ecs_end) {
      is_conditional_effect = true;
    }
  };

  struct NumericalEffect {
    Variable *var;
    vector<EffCond> effect_conds_start;
    vector<EffCond> effect_conds_overall;
    vector<EffCond> effect_conds_end;
    foperator fop;
    Variable *foperand;
    bool is_conditional_effect;
    NumericalEffect(Variable *v, foperator fotor, Variable *fand) :
      var(v), fop(fotor), foperand(fand) {
      is_conditional_effect = false;
    }
    NumericalEffect(Variable *v, vector<EffCond> ecs_start, vector<EffCond> ecs_overall,
	vector<EffCond> ecs_end, foperator fotor, Variable *fand) :
      var(v), effect_conds_start(ecs_start), effect_conds_overall(ecs_overall), effect_conds_end(ecs_end), 
	  fop(fotor), foperand(fand) {
      is_conditional_effect = true;
    }
  };

private:
  string name;
  vector<Prevail> prevail_start; // var, val
  vector<Prevail> prevail_overall; // var, val
  vector<Prevail> prevail_end; // var, val
  vector<PrePost> pre_post_start; // var, old-val, new-val
  vector<PrePost> pre_post_end; // var, old-val, new-val
  vector<NumericalEffect> numerical_effs_start; // comp, first-op, sec-op
  vector<NumericalEffect> numerical_effs_end; // comp, first-op, sec-op
  DurationCond duration_cond;
public:
  Operator(istream &in, const vector<Variable *> &variables);

  void strip_unimportant_effects();
  bool is_redundant() const;

  void dump() const;
  void generate_cpp_input(ofstream &outfile) const;
  void write_prevails(ofstream &outfile, const vector<Prevail> &prevails) const;
  void write_effect_conds(ofstream &outfile, const vector<EffCond> &conds) const;
  void write_pre_posts(ofstream &outfile, const vector<PrePost> &pre_posts) const;
  void write_num_effect(ofstream &outfile, const vector<NumericalEffect> &num_effs) const;
  string get_name() const {
    return name;
  }
  const DurationCond &get_duration_cond() const {
    return duration_cond;
  }
  //    const vector<Prevail> &get_prevail() const {
  //	return prevail_start;
  //    } //TODO: Succ generator needs this..what should be returned?
  //    const vector<PrePost> &get_pre_post() const {
  //	return pre_post_start;
  //    } //TODO: Succ generator needs this..what should be returned?

  const vector<Prevail> &get_prevail_start() const {
    return prevail_start;
  }
  const vector<Prevail> &get_prevail_overall() const {
    return prevail_overall;
  }
  const vector<Prevail> &get_prevail_end() const {
    return prevail_end;
  }
  const vector<PrePost> &get_pre_post_start() const {
    return pre_post_start;
  }
  const vector<PrePost> &get_pre_post_end() const {
    return pre_post_end;
  }
  const vector<NumericalEffect> &get_numerical_effs_start() const {
    return numerical_effs_start;
  }
  const vector<NumericalEffect> &get_numerical_effs_end() const {
    return numerical_effs_end;
  }
};

extern void strip_operators(vector<Operator> &operators);

#endif
