#include "axioms.h"
#include "globals.h"
#include "operator.h"
#include "state.h"

#include <deque>
#include <iostream>
using namespace std;

LogicAxiom::LogicAxiom(istream &in)
{
    check_magic(in, "begin_rule");
    int cond_count;
    in >> cond_count;
    for(int i = 0; i < cond_count; i++)
        prevail.push_back(Prevail(in));
    in >> affected_variable >> old_value >> new_value;
    check_magic(in, "end_rule");
}

NumericAxiom::NumericAxiom(istream &in)
{
    in >> affected_variable >> op >> var_lhs >> var_rhs;
    if(op == lt || op == eq || op == gt || op == ge || op == le || op == ue)
        g_variable_types[affected_variable] = comparison;
    else
        g_variable_types[affected_variable] = subterm_functional;
}

AxiomEvaluator::AxiomEvaluator()
{
    // Handle axioms in the following order:
    // 1) Arithmetic axioms (layers 0 through k-1)
    // 2) Comparison axioms (layer k)
    // 3) Logic axioms (layers k+1 through n)

    // determine layer where arithmetic axioms end and comparison
    // axioms and logic axioms start 	
    g_last_arithmetic_axiom_layer = -1;
    g_comparison_axiom_layer = -1;
    g_first_logic_axiom_layer = -1;
    g_last_logic_axiom_layer = -1;
    for(int i = 0; i < g_axiom_layers.size(); i++) {
        int layer = g_axiom_layers[i];
        if(layer == -1)
            continue;
        if(g_variable_types[i] == logical) {
            g_last_logic_axiom_layer = max(g_last_logic_axiom_layer, layer);
            if(layer < g_first_logic_axiom_layer || g_first_logic_axiom_layer == -1)
                g_first_logic_axiom_layer = layer;
        } else if(g_variable_types[i] == comparison) {
            assert(g_comparison_axiom_layer == -1 || g_comparison_axiom_layer == layer);
            g_comparison_axiom_layer = layer;
        } else { //if(is_functional(i))
            g_last_arithmetic_axiom_layer = max(g_last_arithmetic_axiom_layer,
                    layer);
        }
    }

    int max_axiom_layer = max(g_last_logic_axiom_layer, g_comparison_axiom_layer);
    max_axiom_layer = max(max_axiom_layer, g_last_arithmetic_axiom_layer);
    assert(g_last_arithmetic_axiom_layer < g_comparison_axiom_layer || g_comparison_axiom_layer == -1);
    assert(g_comparison_axiom_layer < g_first_logic_axiom_layer || g_first_logic_axiom_layer == -1);
    assert(g_last_arithmetic_axiom_layer < g_first_logic_axiom_layer || g_first_logic_axiom_layer == -1);
    assert(g_first_logic_axiom_layer <= g_last_logic_axiom_layer);
    assert(g_first_logic_axiom_layer > -1 || g_last_logic_axiom_layer == -1);
    assert(g_first_logic_axiom_layer == -1 || g_last_logic_axiom_layer > -1);

    // Initialize axioms by layer
    axioms_by_layer.resize(g_last_logic_axiom_layer + 2);
    for(int i = 0; i < max_axiom_layer + 2; i++)
        axioms_by_layer.push_back(vector<Axiom*>());

    for(int i = 0; i < g_axioms.size(); i++) {
        int layer = g_axiom_layers[g_axioms[i]->affected_variable];
        axioms_by_layer[layer].push_back(g_axioms[i]);
    }

    // Initialize literals
    for(int i = 0; i < g_variable_domain.size(); i++)
        axiom_literals.push_back(vector<LogicAxiomLiteral>(
            max(g_variable_domain[i], 0))
        );

    // Initialize rules
    if(g_first_logic_axiom_layer != -1) {
        for(int layer = g_first_logic_axiom_layer; 
            layer <= g_last_logic_axiom_layer; layer++) {
            for(int i = 0; i < axioms_by_layer[layer].size(); i++) {
                LogicAxiom *axiom =
                    static_cast<LogicAxiom*>(axioms_by_layer[layer][i]);
                int cond_count = axiom->prevail.size();
                int eff_var = axiom->affected_variable;
                int eff_val = static_cast<int>(axiom->new_value);
                LogicAxiomLiteral *eff_literal =
                    &axiom_literals[eff_var][eff_val];
                rules.push_back(LogicAxiomRule(cond_count, eff_var, eff_val, eff_literal));
            }
        }

        // Cross-reference rules and literals
        int sum_of_indices = 0;
        for(int layer = g_first_logic_axiom_layer; 
            layer <= g_last_logic_axiom_layer; layer++) {
            for(int i = 0; i < axioms_by_layer[layer].size(); i++, sum_of_indices++) {
                LogicAxiom *axiom =
                    static_cast<LogicAxiom*>(axioms_by_layer[layer][i]);
                const vector<Prevail> &conditions = axiom->prevail;
                for(int j = 0; j < conditions.size(); j++) {
                    const Prevail &cond = conditions[j];
                    axiom_literals[cond.var][static_cast<int>(cond.prev)].condition_of.push_back(
                        &rules[sum_of_indices]);
                }
            }
        }
    }

    // Initialize negation-by-failure information
    nbf_info_by_layer.resize(max_axiom_layer + 2);

    for(int var_no = 0; var_no < g_axiom_layers.size(); var_no++) {
        int layer = g_axiom_layers[var_no];
        if(layer > -1 && layer >= g_first_logic_axiom_layer &&
            layer <= g_last_logic_axiom_layer) {
            int nbf_value = static_cast<int>(g_default_axiom_values[var_no]);
            LogicAxiomLiteral *nbf_literal = &axiom_literals[var_no][nbf_value];
            NegationByFailureInfo nbf_info(var_no, nbf_literal);
            nbf_info_by_layer[layer].push_back(nbf_info);
        }
    }

}

void AxiomEvaluator::evaluate(TimeStampedState &state)
{
    evaluate_arithmetic_axioms(state);
    evaluate_comparison_axioms(state);
    evaluate_logic_axioms(state);
    // state.dump();
}

void AxiomEvaluator::evaluate_arithmetic_axioms(TimeStampedState &state)
{
    for(int layer_no = 0; layer_no <= g_last_arithmetic_axiom_layer; layer_no++) {
        for(int i = 0; i < axioms_by_layer[layer_no].size(); i++) {
            NumericAxiom* ax =
                static_cast<NumericAxiom*>(axioms_by_layer[layer_no][i]);

            int var = ax->affected_variable;
            int lhs = ax->var_lhs;
            int rhs = ax->var_rhs;

            switch(ax->op) {
                case add:
                    state[var] = state[lhs] + state[rhs];
                    break;
                case subtract:
                    state[var] = state[lhs] - state[rhs];
                    break;
                case mult:
                    state[var] = state[lhs] * state[rhs];
                    break;
                case divis:
                    state[var] = state[lhs] / state[rhs];
                    break;
                default:
                    cout << "Error: No comparison operators are allowed here." << endl;
                    assert(false);
                    break;
            }

        }
    }
}

void AxiomEvaluator::evaluate_comparison_axioms(TimeStampedState &state)
{
    // There must be exactly one axiom layer containing all the 
    // comparison axioms. This is the one between the layers containing
    // numeric axioms and logic axioms.
    if(g_comparison_axiom_layer == -1)
        return;
    for(int i = 0; i < axioms_by_layer[g_comparison_axiom_layer].size(); i++) {
        NumericAxiom* ax =
            static_cast<NumericAxiom*>(axioms_by_layer[g_comparison_axiom_layer][i]);

        // ax->dump();

        int var = ax->affected_variable;
        int lhs = ax->var_lhs;
        int rhs = ax->var_rhs;
        switch(ax->op) {
            case lt:
                state[var] = (state[lhs] < state[rhs]) ? 0.0 : 1.0;
                break;
            case le:
                state[var] = (state[lhs] <= state[rhs]) ? 0.0 : 1.0;
                break;
            case eq:
                state[var] = double_equals(state[lhs], state[rhs]) ? 0.0 : 1.0;
                break;
            case ge:
                state[var] = (state[lhs] >= state[rhs]) ? 0.0 : 1.0;
                break;
            case gt:
                state[var] = (state[lhs] > state[rhs]) ? 0.0 : 1.0;
                break;
            case ue:
                state[var] = !double_equals(state[lhs], state[rhs]) ? 0.0 : 1.0;
                break;
            default:
                cout << "Error: ax->op is " << ax->op << "." << endl;
                cout << "Error: No arithmetic operators are allowed here." << endl;
                assert(false);
                break;
        }

    }
}

void AxiomEvaluator::evaluate_logic_axioms(TimeStampedState &state)
{
    // cout << "Evaluating axioms..." << endl;
    deque<LogicAxiomLiteral *> queue;
    for(int i = 0; i < g_axiom_layers.size(); i++) {
        if(g_axiom_layers[i] == -1) {
            // non-derived variable
            const variable_type& vt = g_variable_types[i];
            if (vt != comparison && vt != logical) {
                // variable is functional
                // do nothing (should have been handled by
                // arithmetic/comparison axioms)
            } else {
                // variable is a logic variable
                queue.push_back(&axiom_literals[i][static_cast<int>(state[i])]);
            }
        } else if(g_axiom_layers[i] <= g_last_arithmetic_axiom_layer) {
            // derived variable corresponding to an arithmetic (sub)term.
            // do nothing (should have been handled by arithmetic/comparison axioms)
        } else if(g_axiom_layers[i] == g_comparison_axiom_layer) {
            // derived variable corresponding to a comparison.
            // can be handled like a non-derived discrete variable
            queue.push_back(&axiom_literals[i][static_cast<int>(state[i])]);
        } else if(g_axiom_layers[i] >= g_first_logic_axiom_layer) {
            // derived discrete variable -> use default value first
            state[i] = g_default_axiom_values[i];
        } else {
            // cannot happen
            cout << "Error: Encountered a variable with an axiom layer exceeding " 
                << "the maximal computed axiom layer." << endl;
            exit(1);
        }
    }

    for(int i = 0; i < rules.size(); i++) {
        rules[i].unsatisfied_conditions = rules[i].condition_count;

        // In a perfect world, trivial axioms would have been
        // compiled away, and we could have the following assertion
        // instead of the following block.
        // assert(rules[i].condition_counter != 0);
        if(rules[i].condition_count == 0) {
            // NOTE: This duplicates code from the main loop below.
            // I don't mind because this is (hopefully!) going away
            // some time.
            int var_no = rules[i].effect_var;
            int val = rules[i].effect_val;
            if(!double_equals(state[var_no], val)) {
                // cout << "  -> deduced " << var_no << " = " << val << endl;
                state[var_no] = val;
                queue.push_back(rules[i].effect_literal);
            }
        }
    }

    if(g_first_logic_axiom_layer != -1) {
        for(int layer_no = g_first_logic_axiom_layer;
            layer_no < g_last_logic_axiom_layer + 1; layer_no++) {
            // Apply Horn rules.
            while(!queue.empty()) {
                LogicAxiomLiteral *curr_literal = queue.front();
                queue.pop_front();
                for(int i = 0; i < curr_literal->condition_of.size(); i++) {
                    LogicAxiomRule *rule = curr_literal->condition_of[i];
                    if(--(rule->unsatisfied_conditions) == 0) {
                        int var_no = rule->effect_var;
                        int val = rule->effect_val;
                        if(!double_equals(state[var_no], val)) {
                            // cout << "  -> deduced " << var_no << " = " << val << endl;
                            state[var_no] = val;
                            queue.push_back(rule->effect_literal);
                        }
                    }
                }
            }

            // Apply negation by failure rules.
            const vector<NegationByFailureInfo> &nbf_info = nbf_info_by_layer[layer_no];
            for(int i = 0; i < nbf_info.size(); i++) {
                int var_no = nbf_info[i].var_no;
                if(double_equals(state[var_no], g_default_axiom_values[var_no]))
                    queue.push_back(nbf_info[i].literal);
            }
        }
    }
}
