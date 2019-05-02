#ifndef AXIOMS_H
#define AXIOMS_H

#include <cassert>
#include <string>
#include <vector>

#include "globals.h"
#include "state.h"
#include "operator.h"

class TimeStampedState;

struct Prevail;

class Axiom
{
    public:
        int affected_variable;
};

class LogicAxiom : public Axiom
{
    public:
        std::vector<Prevail> prevail; // var, val
        double old_value, new_value;

        LogicAxiom(std::istream &in);

        void dump()
        {
            for (int i = 0; i < prevail.size(); i++) {
                cout << "[";
                prevail[i].dump();
                cout << "]";
            }
            cout << "[" << g_variable_name[affected_variable] << ": "
                << old_value << "]";
            cout << " => " << g_variable_name[affected_variable] << ": "
                << new_value << endl;
        }

        bool is_applicable(const TimeStampedState &state) const
        {
            for(int i = 0; i < prevail.size(); i++)
                if(!prevail[i].is_applicable(state))
                    return false;
            return true;
        }
};

class NumericAxiom : public Axiom
{
    public:
        int var_lhs;
        int var_rhs;
        binary_op op;

        NumericAxiom(std::istream &in);

        void dump()
        {
            cout << g_variable_name[affected_variable] << " = ("
                << g_variable_name[var_lhs];
            cout << " " << op << " ";
            cout << g_variable_name[var_rhs] << ")" << endl;
        }
};

class AxiomEvaluator
{
        struct LogicAxiomRule;
        struct LogicAxiomLiteral
        {
            std::vector<LogicAxiomRule *> condition_of;
        };
        struct LogicAxiomRule
        {
            int condition_count;
            int unsatisfied_conditions;
            int effect_var;
            int effect_val;
            LogicAxiomLiteral *effect_literal;
            LogicAxiomRule(int cond_count, int eff_var, int eff_val,
                    LogicAxiomLiteral *eff_literal) :
                condition_count(cond_count), unsatisfied_conditions(cond_count),
                effect_var(eff_var), effect_val(eff_val),
                effect_literal(eff_literal)
                {
                }
        };
        struct NegationByFailureInfo
        {
            int var_no;
            LogicAxiomLiteral *literal;
            NegationByFailureInfo(int var, LogicAxiomLiteral *lit) :
                var_no(var), literal(lit)
                {
                }
        };

        std::vector<std::vector<LogicAxiomLiteral> > axiom_literals;
        std::vector<LogicAxiomRule> rules;
        std::vector<std::vector<NegationByFailureInfo> > nbf_info_by_layer;
        std::vector<std::vector<Axiom*> > axioms_by_layer;

    private:
        void evaluate_arithmetic_axioms(TimeStampedState &state);
        void evaluate_comparison_axioms(TimeStampedState &state);
        void evaluate_logic_axioms(TimeStampedState &state);

    public:
        AxiomEvaluator();
        void evaluate(TimeStampedState &state);
};

#endif
