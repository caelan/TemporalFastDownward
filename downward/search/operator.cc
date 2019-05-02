#include "globals.h"
#include "operator.h"
#include "plannerParameters.h"

#include <iostream>
using namespace std;

Prevail::Prevail(istream &in)
{
    in >> var >> prev;
}

bool Prevail::is_applicable(const TimeStampedState &state) const
{
    assert(var >= 0 && var < g_variable_name.size());
    assert(prev >= 0 && prev < g_variable_domain[var]);
    return double_equals(state[var], prev);
}

PrePost::PrePost(istream &in)
{
    int cond_count;
    in >> cond_count;
    for(int i = 0; i < cond_count; i++)
        cond_start.push_back(Prevail(in));
    in >> cond_count;
    for(int i = 0; i < cond_count; i++)
        cond_overall.push_back(Prevail(in));
    in >> cond_count;
    for(int i = 0; i < cond_count; i++)
        cond_end.push_back(Prevail(in));
    in >> var;
    if(is_functional(var)) {
        in >> fop >> var_post;
        // HACK: just use some arbitrary values for pre and post
        // s.t. they do not remain uninitialized
        pre = post = -1;
    } else {
        in >> pre >> post;
        // HACK: just use some arbitrary values for var_post and fop
        // s.t. they do not remain uninitialized
        var_post = -1;
        fop = assign;
    }
}

bool PrePost::is_applicable(const TimeStampedState &state) const
{
    assert(var >= 0 && var < g_variable_name.size());
    assert(pre == -1 || (pre >= 0 && pre < g_variable_domain[var]));
    return pre == -1 || (double_equals(state[var], pre));
}

Operator::Operator(istream &in)
{
    check_magic(in, "begin_operator");
    in >> ws;
    getline(in, name);
    int count;
    binary_op bop;
    in >> bop >> duration_var;
    if(bop != eq) {
        cout << "Error: The duration constraint must be of the form\n";
        cout << "       (= ?duration (arithmetic_term))" << endl;
        exit(1);
    }

    in >> count; //number of prevail at-start conditions
    for(int i = 0; i < count; i++)
        prevail_start.push_back(Prevail(in));
    in >> count; //number of prevail overall conditions
    for(int i = 0; i < count; i++)
        prevail_overall.push_back(Prevail(in));
    in >> count; //number of prevail at-end conditions
    for(int i = 0; i < count; i++)
        prevail_end.push_back(Prevail(in));
    in >> count; //number of pre_post_start conditions (symbolical)
    for(int i = 0; i < count; i++)
        pre_post_start.push_back(PrePost(in));
    in >> count; //number of pre_post_end conditions (symbolical)
    for(int i = 0; i < count; i++)
        pre_post_end.push_back(PrePost(in));
    in >> count; //number of pre_post_start conditions (functional)
    for(int i = 0; i < count; i++)
        pre_post_start.push_back(PrePost(in));
    in >> count; //number of pre_post_end conditions (functional)
    for(int i = 0; i < count; i++)
        pre_post_end.push_back(PrePost(in));
    check_magic(in, "end_operator");
}

Operator::Operator(bool uses_concrete_time_information)
{
    prevail_start   = vector<Prevail>();
    prevail_overall = vector<Prevail>();
    prevail_end     = vector<Prevail>();
    pre_post_start  = vector<PrePost>();
    pre_post_end    = vector<PrePost>();
    if(!uses_concrete_time_information) {
        name = "let_time_pass";
        duration_var = -1;
    } else {
        name = "wait";
        duration_var = -2;
    }
}

void Prevail::dump() const
{
    cout << g_variable_name[var] << ": " << prev << endl;
}

void PrePost::dump() const
{
    cout << "var: " << g_variable_name[var] << ", pre: " << pre
        << " , var_post: " << var_post << ", post: " << post << endl;
}

void Operator::dump() const
{
    cout << name << endl;
    cout << "Prevails start:" << endl;
    for(int i = 0; i < prevail_start.size(); ++i) {
        prevail_start[i].dump();
    }
    cout << "Prevails overall:" << endl;
    for(int i = 0; i < prevail_overall.size(); ++i) {
        prevail_overall[i].dump();
    }
    cout << "Prevails end:" << endl;
    for(int i = 0; i < prevail_end.size(); ++i) {
        prevail_end[i].dump();
    }
    cout << "Preposts start:" << endl;
    for(int i = 0; i < pre_post_start.size(); ++i) {
        pre_post_start[i].dump();
    }
    cout << "Preposts end:" << endl;
    for(int i = 0; i < pre_post_end.size(); ++i) {
        pre_post_end[i].dump();
    }
    cout << endl;
}

bool Operator::is_applicable(const TimeStampedState & state,
        TimedSymbolicStates* timedSymbolicStates) const
{
    double duration = get_duration(&state);

    if(g_parameters.epsilonize_internally) {
    for(unsigned int i = 0; i < state.operators.size(); ++i) {
        double time_increment = state.operators[i].time_increment;
            if(double_equals(time_increment,EPS_TIME)) {
                return false;
            }
        }
    }

    if(duration < 0)
        return false;


    for(int i = 0; i < prevail_start.size(); i++)
        if(!prevail_start[i].is_applicable(state))
            return false;
    for(int i = 0; i < pre_post_start.size(); i++)
        if(!pre_post_start[i].is_applicable(state))
            return false;

    // There may be no simultaneous applications of two instances of the
    // same ground operator (for technical reasons, to simplify the task
    // of keeping track of durations committed to at the start of the
    // operator application)
    for(int i = 0; i < state.operators.size(); i++)
        if(state.operators[i].get_name() == get_name())
            return false;

    return TimeStampedState(state, *this).is_consistent_when_progressed(timedSymbolicStates);
}

bool Operator::isDisabledBy(const Operator* other) const
{
    if(name.compare(other->name) == 0)
        return false;
    if(deletesPrecond(prevail_start, other->pre_post_start))
        return true;
    if(deletesPrecond(prevail_start, other->pre_post_end))
        return true;
    if(deletesPrecond(prevail_overall, other->pre_post_start))
        return true;
    if(deletesPrecond(prevail_overall, other->pre_post_end))
        return true;
    if(deletesPrecond(prevail_end, other->pre_post_start))
        return true;
    if(deletesPrecond(prevail_end, other->pre_post_end))
        return true;
    if(deletesPrecond(pre_post_start, other->pre_post_start))
        return true;
    if(deletesPrecond(pre_post_start, other->pre_post_end))
        return true;
    if(deletesPrecond(pre_post_end, other->pre_post_start))
        return true;
    if(deletesPrecond(pre_post_end, other->pre_post_end))
        return true;
    if(writesOnSameVar(pre_post_start,other->pre_post_start)) return true;
    if(writesOnSameVar(pre_post_start,other->pre_post_end)) return true;
    if(writesOnSameVar(pre_post_end,other->pre_post_start)) return true;
    if(writesOnSameVar(pre_post_end,other->pre_post_end)) return true;

    return false;
}

bool Operator::enables(const Operator* other) const
{
    if(name.compare(other->name) == 0)
        return false;
    if(achievesPrecond(pre_post_start, other->prevail_start))
        return true;
    if(achievesPrecond(pre_post_start, other->prevail_overall))
        return true;
    if(achievesPrecond(pre_post_start, other->prevail_end))
        return true;
    if(achievesPrecond(pre_post_end, other->prevail_start))
        return true;
    if(achievesPrecond(pre_post_end, other->prevail_overall))
        return true;
    if(achievesPrecond(pre_post_end, other->prevail_end))
        return true;
    if(achievesPrecond(pre_post_start, other->pre_post_start))
        return true;
    if(achievesPrecond(pre_post_start, other->pre_post_end))
        return true;
    if(achievesPrecond(pre_post_end, other->pre_post_start))
        return true;
    if(achievesPrecond(pre_post_end, other->pre_post_end))
        return true;
    return false;
}

bool Operator::achievesPrecond(const vector<PrePost>& effects, const vector<
        Prevail>& conds) const
{
    for(int i = 0; i < effects.size(); ++i) {
        for(int j = 0; j < conds.size(); ++j) {
            if(effects[i].var == conds[j].var && double_equals(
                        effects[i].post, conds[j].prev)) {
                return true;
            }
        }
    }
    return false;
}

bool Operator::achievesPrecond(const vector<PrePost>& effs1, const vector<
        PrePost>& effs2) const
{
    for(int i = 0; i < effs1.size(); ++i) {
        for(int j = 0; j < effs2.size(); ++j) {
            if(effs1[i].var == effs2[j].var && double_equals(effs1[i].post,
                        effs2[j].pre)) {
                return true;
            }
        }
    }
    return false;
}

//FIXME: numerical effects?? conditional effects?? axioms??
bool Operator::deletesPrecond(const vector<Prevail>& conds, const vector<
        PrePost>& effects) const
{
    for(int i = 0; i < conds.size(); ++i) {
        for(int j = 0; j < effects.size(); ++j) {
            if(conds[i].var == effects[j].var && !double_equals(conds[i].prev,
                        effects[j].post)) {
                return true;
            }
        }
    }
    return false;
}

bool Operator::deletesPrecond(const vector<PrePost>& effs1, const vector<
        PrePost>& effs2) const
{
    for(int i = 0; i < effs1.size(); ++i) {
        for(int j = 0; j < effs2.size(); ++j) {
            if(effs1[i].var == effs2[j].var && !double_equals(effs1[i].pre,
                        effs2[j].post)) {
                //	  if(effs1[i].var == effs2[j].var &&
                //	     !double_equals(effs1[i].pre,effs2[j].post) &&
                //	     !double_equals(effs1[i].pre,-1.0)) 
                return true;
            }
        }
    }
    return false;
}

bool Operator::writesOnSameVar(const vector<PrePost>& effs1, const vector<
        PrePost>& effs2) const
{
    for(int i = 0; i < effs1.size(); ++i) {
        for(int j = 0; j < effs2.size(); ++j) {
            if(effs1[i].var == effs2[j].var/* && effs1[i].post != effs2[j].post*/) {
                return true;
            }
        }
    }
    return false;
}

double Operator::get_duration(const TimeStampedState* state) const
{
    assert(duration_var >= 0);
    assert(state != NULL);

    // default behaviour: duration is defined by duration_var
    return (*state)[duration_var];
}

bool Operator::operator<(const Operator &other) const
{
    return name < other.name;
}
