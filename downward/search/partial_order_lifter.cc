#include "partial_order_lifter.h"

void InstantPlanStep::print_name() {
    if(type == dummy_start_action) {
        cout << "dummy_start_action";
    } else if(type == dummy_end_action) {
        cout << "dummy_end_action";
    } else {
        cout << name;
    }
}

void InstantPlanStep::dump() {
    print_name();
    cout << endl;
    cout << "     Timepoint:" << timepoint << endl;
    cout << "     precond vars:" << endl;
    set<int>::iterator it;
    for(it = precondition_vars.begin(); it != precondition_vars.end(); ++it) {
        cout << "      " << *it << endl;
    }
    cout << "     effect vars:" << endl;
    for(it = effect_vars.begin(); it != effect_vars.end(); ++it) {
        cout << "      " << *it << endl;
    }
    cout << "     effect_cond_vars:" << endl;
    for(it = effect_cond_vars.begin(); it != effect_cond_vars.end(); ++it) {
        cout << "      " << *it << endl;
    }
    cout << "     effects:" << endl;
    for(int i = 0; i < effects.size(); ++i) {
        cout << "      " << effects[i].var << ": " << effects[i].post << endl;
    }
    cout << "     preconditions:" << endl;
    for(int i = 0; i < preconditions.size(); ++i) {
        cout << "      " << preconditions[i].var << ": " << preconditions[i].prev << endl;
    }
    cout << "     overall conds:" << endl;
    for(int i = 0; i < overall_conds.size(); ++i) {
        cout << "      " << overall_conds[i].var << ": " << overall_conds[i].prev << endl;
    }
    if(endAction != -1) {
        cout << "     endAction: " << endAction << endl;
    }
    if(actionFinishingImmeadatlyAfterThis) {
        cout << "     actionFinishingImmeadatlyAfterThis: " << actionFinishingImmeadatlyAfterThis << endl;
    }
}

Plan PartialOrderLifter::createAndSolveSTN() {
    vector<string> variable_names;
    for(int i = 0; i < instant_plan.size(); ++i) {
        variable_names.push_back(instant_plan[i].name);
    }

    SimpleTemporalProblem stn(variable_names);

    // assert that causal relationships are preserved
    for(set<Ordering>::iterator it = partial_order.begin(); it != partial_order.end(); ++it) {
        if(partial_order.find(make_pair(it->second,it->first)) != partial_order.end()) {
            stn.setSingletonInterval(it->first,it->second,0.0);
        } else {
            stn.setUnboundedInterval(it->first, it->second, EPS_TIME);
        }
    }

    for(int i = 0; i < instant_plan.size(); ++i) {
        if(instant_plan[i].type == start_action) {
        	assert(instant_plan[i].endAction != -1);
            // assert that start time point of actions are non-negative
            stn.setUnboundedIntervalFromXZero(i, 0.0);
            // assert that differences between start and end time points are exactly
            // the durations of the actions
            stn.setSingletonInterval(i, instant_plan[i].endAction, instant_plan[i].duration);
        } else {
            assert(instant_plan[i].type == end_action);
            // assert that two actions ending at the same time point in the original plan
            // also do so in the scheduled one
//            if(instant_plan[i].actionFinishingImmeadatlyAfterThis != -1) {
//                stn.setSingletonInterval(i,instant_plan[i].actionFinishingImmeadatlyAfterThis,EPS_TIME);
//            }
        }
    }



//    // assert that overall conditions are preserved
//    for(int i = 0; i < plan.size(); ++i) {
//        const vector<Prevail>& overall_conds = plan[i].op->get_prevail_overall();
//        for(int j = 0; j < overall_conds.size(); ++j) {
//            for(int k = 0; k < instant_plan.size(); ++k) {
//                InstantPlanStep& step = instant_plan[k];
//                for(int l = 0; l < step.effects.size(); ++l) {
//                    if(overall_conds[j].var == step.effects[l].var && overall_conds[j].prev != step.effects[l].post) {
//                        if(plan[i].start_time < step.start_time) {
//                            stn.st
//                        }
//                    }
//                }
//            }
//        }
//
//    }

//    std::cout << "Unsolved Simple Temporal Network:" << std::endl;
//
//
//    std::cout << "=================================" << std::endl;
//    stn.dump();

    stn.solve();
    vector<Happening> happenings = stn.getHappenings(false);

    //    bool solvable = stn.solveWithP3C();
    //    assert(solvable);
    //    vector<Happening> happenings = stn.getHappenings(true);



    map<string,int> startedActions;

    double lastTime = 0.0;
    vector<const Operator*> lastOpsWithSameStartingTime;
    int idx = 1;
    for(int i = 0; i < happenings.size(); ++i) {
    	Happening& currentHappening = happenings[i];
    	int currentIndexInHappenings = currentHappening.second;
    	InstantPlanStep& currentStep = instant_plan[currentIndexInHappenings];
    	assert(currentStep.op);
//    	cout << "NEXT ACTION: " << currentStep.op->get_name() << " (" << currentStep.type << ")" << endl;
        if(currentStep.type == start_action) {
            assert(currentStep.correspondingPlanStep < plan.size());
            double newTime = currentHappening.first;
            const Operator* thisOp = currentStep.op;
//            cout << "this op: " << (thisOp ? thisOp->get_name() : "NULL") << ", last ops: ";
//            for(unsigned int j = 0; j < lastOpsWithSameStartingTime.size(); ++j) {
//            	cout << (lastOpsWithSameStartingTime[j] ? lastOpsWithSameStartingTime[j]->get_name() : "NULL");
//            	cout << " ";
//            }
//            cout << ":: i: " << i << ", lastTime: " << lastTime << ", newTime: " << newTime;
//            if(thisOp && lastOpsWithSameStartingTime.size() > 0) {
//            	for(unsigned int j = 0; j < lastOpsWithSameStartingTime.size(); ++j) {
//            		cout << ", dis1: " << thisOp->isDisabledBy(lastOpsWithSameStartingTime[j])
//            				<< ", dis2: " << lastOpsWithSameStartingTime[j]->isDisabledBy(thisOp) << endl;
//            	}
//            }
            if(i>0 && double_equals(lastTime,newTime) && isMutex(thisOp, lastOpsWithSameStartingTime)){
	      //            if(i>0 && double_equals(lastTime,newTime)){
                idx++;
                lastOpsWithSameStartingTime.clear();
            }
//            cout << "idx: " << idx << endl;
            plan[currentStep.correspondingPlanStep].start_time = newTime + (idx * EPSILON * 100.0);
            lastTime=newTime;
            lastOpsWithSameStartingTime.push_back(thisOp);
        }
//        cout << "ferdsch." << endl;
    }

    return plan;
}

bool PartialOrderLifter::isMutex(const Operator* op, vector<const Operator*>& otherOps) {
	for(unsigned int i = 0; i < otherOps.size(); ++i) {
		const Operator* otherOp = otherOps[i];
		if(op->isDisabledBy(otherOp) || otherOp->isDisabledBy(op)) {
			return true;
		}
	}
	return false;
}

Plan PartialOrderLifter::lift() {
    //    instant_plan.clear();
    //    partial_order.clear();
    buildInstantPlan();
//    dumpInstantPlan();
    buildPartialOrder();
//    dumpOrdering();
    return createAndSolveSTN();
}

void PartialOrderLifter::buildPartialOrder() {
    std::vector<std::vector<Prevail> > primary_add;
    primary_add.resize(instant_plan.size());

    for(int i = instant_plan.size() - 1; i >= 0; --i) {
        //        cout << "Looking at op: ";
        //        instant_plan[i].print_name();
        //        cout << endl;

        /*        // step (a) find achievers for all action preconditions and add corresponding ordering constraints
        InstantPlanStep &step = instant_plan[i];
        for(int j = 0; j < step.preconditions.size(); ++j) {
//            cout << "  Search achiever for ";
//            step.preconditions[j].dump();
            double latestTimeStampThatAchievesCond = 0.0;
            int indexOfAchiever = -1;
            for(int k = i-1; k >= 0; --k) {
//                cout << "  " << endl;
                InstantPlanStep &temp_step = instant_plan[k];
//                temp_step.print_name();
//                cout << " has effects:";
//                for(unsigned int v = 0; v < temp_step.effects.size(); ++v) {
//                    cout << " " << g_variable_name[temp_step.effects[v].var] << "->" << temp_step.effects[v].post << endl;
//                }
                double currentTimeStamp = temp_step.duration + temp_step.timepoint;
                for(int l = 0; l < temp_step.effects.size(); ++l) {
                    if(step.preconditions[j].var == temp_step.effects[l].var &&
                            step.preconditions[j].prev == temp_step.effects[l].post &&
                            step.op != temp_step.op) {
//                        temp_step.print_name();
//                        cout << ": " << currentTimeStamp << endl;
//                        cout << "latestTimeStampThatAchievesCond: " << latestTimeStampThatAchievesCond << endl;
                        if(currentTimeStamp - EPSILON > latestTimeStampThatAchievesCond &&
                                currentTimeStamp + EPSILON < step.timepoint) {
                            indexOfAchiever = k;
                            latestTimeStampThatAchievesCond = currentTimeStamp;
                        }
                    }
                }
            }
            if(indexOfAchiever != -1) {
//                cout << "   achiever found: ";
//                instant_plan[indexOfAchiever].print_name();
//                cout << endl;
                partial_order.insert(make_pair(indexOfAchiever,i));
                primary_add[indexOfAchiever].push_back(step.preconditions[j]);
            }
        }
        */
//        // step (a) find achievers for all action preconditions and add corresponding ordering constraints
        InstantPlanStep &step = instant_plan[i];
        for(int j = 0; j < step.preconditions.size(); ++j) {
            //            cout << "  Search achiever for ";
            //            step.preconditions[j].dump();
            bool achiever_found = false;
            for(int k = i-1; k >= 0; --k) {
                if(!achiever_found) {
                    InstantPlanStep &temp_step = instant_plan[k];
                    for(int l = 0; l < temp_step.effects.size(); ++l) {
//                        cout << " " << step.preconditions[j].var << " " << temp_step.effects[l].var << " " << step.preconditions[j].prev << " " << temp_step.effects[l].post << endl;
                        if(step.preconditions[j].var == temp_step.effects[l].var && step.preconditions[j].prev == temp_step.effects[l].post &&
                                step.op != temp_step.op) {
//                             achiever found!
//                            cout << "   achiever found: ";
//                            temp_step.print_name();
//                            cout << endl;
                            partial_order.insert(make_pair(k,i));
                            primary_add[k].push_back(step.preconditions[j]);
                            achiever_found = true;
                            break;
                        }
                    }
                } else {
                    achiever_found = false;
                    break;
                }
            }
        }

        for(int j = 0; j < step.overall_conds.size(); ++j) {
            //            cout << "  Search achiever for ";
            //            step.overall_conds[j].dump();
            bool achiever_found = false;
            for(int k = i-1; k >= 0; --k) {
                if(!achiever_found) {
                    InstantPlanStep &temp_step = instant_plan[k];
                    for(int l = 0; l < temp_step.effects.size(); ++l) {
//                        cout << " " << step.overall_conds[j].var << " " << temp_step.effects[l].var << " " << step.overall_conds[j].prev << " " << temp_step.effects[l].post << endl;
                        if(step.overall_conds[j].var == temp_step.effects[l].var && step.overall_conds[j].prev == temp_step.effects[l].post &&
                                step.op != temp_step.op) {
//                             achiever found!
//                            cout << "   achiever found: ";
//                            temp_step.print_name();
//                            cout << endl;
                            assert(partial_order.find(make_pair(i,k)) == partial_order.end());
                            partial_order.insert(make_pair(k,i));
                            primary_add[k].push_back(step.overall_conds[j]);
                            achiever_found = true;
                            break;
                        }
                    }
                } else {
                    achiever_found = false;
                    break;
                }
            }
        }

        // step (b) demote actions threatening preconditions
//        cout << " i: " << i << endl;
        for(int j = 0; j < step.effects.size(); ++j) {
            for(int k = i-1; k >= 0; --k) {
//                cout << "  k: " << k << endl;
                InstantPlanStep &temp_step = instant_plan[k];
                for(int l = 0; l < temp_step.preconditions.size(); ++l) {
                    if(temp_step.preconditions[l].var == step.effects[j].var && temp_step.preconditions[l].prev != step.effects[j].post &&
                            step.op != temp_step.op) {
                        //                        step.print_name();
                        //                        cout << " threatens ";
                        //                        temp_step.print_name();
                        //                        cout << endl;
                        partial_order.insert(make_pair(k,i));
                    }
                }
            }
        }

        // two instances of the same action may not overlap in time!
        for(int k = i-1; k >= 0; --k) {
            InstantPlanStep &temp_step = instant_plan[k];
            if(step.op->get_name().compare(temp_step.op->get_name())==0) {
//                step.print_name();
//                cout << " (" << i << ")";
//                cout << " is the same as ";
//                temp_step.print_name();
//                cout << " (" << k << ")";
//                cout << endl;
                partial_order.insert(make_pair(k,i));
                break;
//            } else {
//                step.print_name();
//                cout << " (" << i << ")";
//                cout << " is different than ";
//                temp_step.print_name();
//                cout << " (" << k << ")";
//                cout << endl;
            }
        }
        // mutex conditions: no two actions may affect the same variable at the same time!
        for(int j = 0; j < step.effects.size(); ++j) {
            for(int k = i-1; k >= 0; --k) {
                InstantPlanStep &temp_step = instant_plan[k];
                for(int l = 0; l < temp_step.effects.size(); ++l) {
                    if(temp_step.effects[l].var == step.effects[j].var
//                            &&
//                            temp_step.effects[l].post != step.effects[j].post &&
//                            //|| temp_step.effects[l].var_post != step.effects[j].var_post) &&
//                            step.op != temp_step.op
                            ) {
//                        step.print_name();
//                        cout << " writes on same var as ";
//                        temp_step.print_name();
//                        cout << endl;
                        partial_order.insert(make_pair(k,i));
                    }
                }
            }
        }

        // step (c)
        for(int j = 0; j < primary_add[i].size(); ++j) {
            for(int k = i-1; k >= 0; --k) {
                InstantPlanStep &temp_step = instant_plan[k];
                for(int l = 0; l < temp_step.effects.size(); ++l) {
                    if(temp_step.effects[l].var == primary_add[i][j].var && temp_step.effects[l].post != primary_add[i][j].prev &&
                            step.op != temp_step.op) {
//                        step.print_name();
//                        cout << " threatens primary add of ";
//                        temp_step.print_name();
//                        cout << endl;
                        partial_order.insert(make_pair(k,i));
                    }
                }
            }
        }

        // overallconds must be satiesfied!
        for(int j = 0; j < step.overall_conds.size(); ++j) {
            for(int k = 0; k < instant_plan.size(); ++k) {
                InstantPlanStep &temp_step = instant_plan[k];
                for(int l = 0; l < temp_step.effects.size(); ++l) {
                    if(temp_step.effects[l].var == step.overall_conds[j].var && temp_step.effects[l].post != step.overall_conds[j].prev &&
                            step.op != temp_step.op) {
                        if(step.timepoint >= temp_step.timepoint && step.type == start_action) {
//                            temp_step.print_name();
//                            cout << " could break overall cond and has to be scheduled before ";
//                            step.print_name();
//                            cout << endl;
                            partial_order.insert(make_pair(k,i));
                        } else if(step.timepoint <= temp_step.timepoint && step.type == end_action) {
//                            temp_step.print_name();
//                            cout << " could break overall cond and has to be scheduled after ";
//                            step.print_name();
//                            cout << endl;
                            partial_order.insert(make_pair(i,k));
                        }
                    }
                }

            }
        }



        const Operator* op = step.op;
        assert(op);
        int durVar = op->get_duration_var();
        bool achiever_found = false;
        for(int k = i-1; k >= 0; --k) {
            if(!achiever_found) {
                InstantPlanStep &temp_step = instant_plan[k];
                for(int l = 0; l < temp_step.effects.size(); ++l) {
//                            cout << " " << step.overall_conds[j].var << " " << temp_step.effects[l].var << " " << step.overall_conds[j].prev << " " << temp_step.effects[l].post << endl;
                    if(durVar == temp_step.effects[l].var && step.op != temp_step.op) {
//                             achiever found!
//                            cout << "   achiever found: ";
//                            temp_step.print_name();
//                            cout << endl;
                        assert(partial_order.find(make_pair(i,k)) == partial_order.end());
                        partial_order.insert(make_pair(k,i));
                        achiever_found = true;
                        break;
                    }
                }
            } else {
                achiever_found = false;
                break;
            }
        }
        for(int k = i+1; k < instant_plan.size(); ++k) {
        	if(!achiever_found) {
        		InstantPlanStep &temp_step = instant_plan[k];
                for(int l = 0; l < temp_step.effects.size(); ++l) {
//                            cout << " " << step.overall_conds[j].var << " " << temp_step.effects[l].var << " " << step.overall_conds[j].prev << " " << temp_step.effects[l].post << endl;
                    if(durVar == temp_step.effects[l].var && step.op != temp_step.op) {
//                             achiever found!
//                            cout << "   achiever found: ";
//                            temp_step.print_name();
//                            cout << endl;
                        assert(partial_order.find(make_pair(k,i)) == partial_order.end());
                        partial_order.insert(make_pair(i,k));
                        achiever_found = true;
                        break;
                    }
                }
            } else {
                achiever_found = false;
                break;
            }
        }
    }
}

void PartialOrderLifter::dumpInstantPlan() {
    for(int i = 0; i < instant_plan.size(); ++i) {
        cout << i << ": ";
        instant_plan[i].dump();
        cout << "-----------------------------------" << endl;
    }
}

void PartialOrderLifter::dumpOrdering() {
    cout << "numberOfOrderingConstraints: " << partial_order.size() << endl;
    cout << "Ordering:" << endl;
    for(std::set<Ordering>::iterator it = partial_order.begin(); it != partial_order.end(); ++it) {
        cout << " ";
        instant_plan[it->first].print_name();
        cout << " < ";
        instant_plan[it->second].print_name();
        cout << endl;
    }
}

void PartialOrderLifter::findTriggeringEffects(const TimeStampedState* stateBeforeHappening,
        const TimeStampedState* stateAfterHappening, vector<PrePost>& effects) {
    effects.clear();
    assert(stateAfterHappening->state.size() == stateBeforeHappening->state.size());
    for(int i = 0; i < stateAfterHappening->state.size(); ++i) {
        if(!(double_equals(stateBeforeHappening->state[i],stateAfterHappening->state[i]))) {
//            cout << "             eff: " << g_variable_name[i] << "->" << stateAfterHappening->state[i] << endl;
            effects.push_back(PrePost(i,stateAfterHappening->state[i]));
        }
    }
}

//void PartialOrderLifter::findTriggeringEffectsForInitialState(const TimeStampedState* tsstate, vector<PrePost>& effects) {
//    for(int i = 0; i < tsstate->state.size(); ++i) {
////        effects.push_back(PrePost(i,tsstate->state[i]));
//    }
//}

void PartialOrderLifter::findAllEffectCondVars(const ScheduledOperator& new_op, set<
        int>& effect_cond_vars, ActionType type) {
    // TODO: start_type -> start_conds, end_type -> end_conds, overall_conds??
    const vector<PrePost>* pre_posts;
    if(type == start_action) {
        pre_posts = &new_op.get_pre_post_start();
    } else {
        assert(type == end_action);
        pre_posts = &new_op.get_pre_post_end();
    }
    const vector<Prevail>* prevails;
    for(int i = 0; i < pre_posts->size(); ++i) {
        if(type == start_action) {
            prevails = &(*pre_posts)[i].cond_start;
        } else {
            assert(type == end_action);
            prevails = &(*pre_posts)[i].cond_end;
        }
        for(int j = 0; j < prevails->size(); ++j) {
            effect_cond_vars.insert((*prevails)[i].var);
        }
    }
}

void PartialOrderLifter::findPreconditions(const ScheduledOperator& new_op,
        vector<Prevail>& preconditions, ActionType type) {
    // TODO: start_type -> start_conds, end_type -> end_conds, overall_conds??
    const std::vector<Prevail> *prevails;
    const std::vector<PrePost> *pre_posts;
    if(type == start_action) {
        prevails = &new_op.get_prevail_start();
        pre_posts = &new_op.get_pre_post_start();
    } else {
        prevails = &new_op.get_prevail_end();
        pre_posts = &new_op.get_pre_post_end();
    }
    for(size_t i = 0; i < prevails->size(); i++) {
        preconditions.push_back((*prevails)[i]);
    }
    for(int i = 0; i < pre_posts->size(); ++i) {
        if((*pre_posts)[i].pre != -1) {
            preconditions.push_back(Prevail((*pre_posts)[i].var, (*pre_posts)[i].pre));
        }
    }
}

int PartialOrderLifter::getIndexOfPlanStep(const ScheduledOperator& op, double timestamp) {
//    cout << "Searching for " << op.get_name() << ", timestamp: " << timestamp << endl;
    for(int i = 0; i < plan.size(); ++i) {
//        cout << "  This is " << plan[i].op->get_name() << ", plan[i].start_time: " << plan[i].start_time << endl;
        if(plan[i].op->get_name().compare(op.get_name()) == 0 && abs(plan[i].start_time - timestamp)-EPSILON <= EPS_TIME) {
            return i;
        }
    }
    assert(false);
    return -1;
}

//Invariant: Operators are not allowed to run in multiples
void PartialOrderLifter::buildInstantPlan() {
    const TimeStampedState* stateBeforeHappening;
    const TimeStampedState* stateAfterHappening;
    vector<InstantPlanStep*> runningInstantActions;
    vector<pair<const ScheduledOperator*, double> > actualEndingTimeOfRunningActions;
    int startPoints = 0;
    int endPoints = 0;
//    cout << "-------------------" << endl;
//    trace[0]->dump(false);
    for(int i = 0; i < trace.size() - 1; ++i) {
//        cout << "-------------------" << endl;
//        trace[i+1]->dump(false);
        stateBeforeHappening = trace[i];
        vector<PrePost> effects;
        stateAfterHappening = trace[i + 1];
        findTriggeringEffects(stateBeforeHappening,stateAfterHappening,effects);
        double currentTimeStamp = stateAfterHappening->timestamp;
        //first, check whether some action has finished in current state.
        InstantPlanStep *lastEndingAction = NULL;
        double endTime = currentTimeStamp;
        for(int j = 0; j < actualEndingTimeOfRunningActions.size(); ++j) {
            const ScheduledOperator* tmpOp = actualEndingTimeOfRunningActions[j].first;
            double tmpEndTime = actualEndingTimeOfRunningActions[j].second;
//            cout << "tmpOp: " << tmpOp->get_name() << endl;
//            cout << "tmpEndTime: " << tmpEndTime << endl;
            if(tmpEndTime - EPSILON > endTime) {
                continue;
            }
//            cout << "  ENDING:" << tmpOp->get_name() << endl;
            endPoints++;
            instant_plan.push_back(InstantPlanStep(end_action, endTime, -1, tmpOp));
            vector<Prevail> preconditions;
            findPreconditions(*tmpOp, preconditions, end_action);
            set<int> effect_cond_vars;
            findAllEffectCondVars(*tmpOp,effect_cond_vars,end_action);
            instant_plan.back().effects = effects;
            instant_plan.back().effect_cond_vars = effect_cond_vars;
            instant_plan.back().preconditions = preconditions;
            instant_plan.back().overall_conds = tmpOp->get_prevail_overall();
            //            bool bad = true;
            for(unsigned int i = 0; i < runningInstantActions.size(); ++i) {
                if(runningInstantActions[i]->op == instant_plan.back().op) {
                    runningInstantActions[i]->endAction = instant_plan.size()-1;
                    //                    bad = false;
                }
            }
            //            assert(!bad);
            if(lastEndingAction != NULL) {
                lastEndingAction->actionFinishingImmeadatlyAfterThis = instant_plan.size() - 1;
            }
            lastEndingAction = &instant_plan.back();
            actualEndingTimeOfRunningActions.erase(actualEndingTimeOfRunningActions.begin()+j);
            j--;
        }
        //second, check whether a new action has been scheduled in current state
        if(stateAfterHappening->operators.size() == 0) {
            continue;
        }
        //if a new action has been scheduled, it is always the last in state.operators
        const ScheduledOperator& new_op = stateAfterHappening->operators.back();
        bool isAlreadyRunning = false;
        const ScheduledOperator* tmpOp = NULL;
        for(unsigned int j = 0; j < actualEndingTimeOfRunningActions.size(); ++j) {
            tmpOp = actualEndingTimeOfRunningActions[j].first;
            if(tmpOp->get_name().compare(new_op.get_name())==0) {
                isAlreadyRunning = true;
                break;
            }
        }
        if(!isAlreadyRunning) {
//            cout << "OP: " << new_op.get_name() << endl;
            // A new action starts at this happening!
            startPoints++;
//            cout << "  STARTING: " << new_op.get_name() << endl;
            double startTime = currentTimeStamp;
            double time_increment = new_op.time_increment;
            double endTime = startTime + time_increment;
            vector<Prevail> preconditions;
            findPreconditions(new_op, preconditions, start_action);
            set<int> effect_cond_vars;
            findAllEffectCondVars(new_op,effect_cond_vars,start_action);
            actualEndingTimeOfRunningActions.push_back(make_pair(&new_op,endTime));
//            cout << "  will end at " << endTime << endl;
            instant_plan.push_back(InstantPlanStep(start_action,
                                startTime, stateBeforeHappening->state[new_op.get_duration_var()], &new_op));
            instant_plan.back().effects = effects;
            instant_plan.back().effect_cond_vars = effect_cond_vars;
            instant_plan.back().preconditions = preconditions;
            instant_plan.back().overall_conds = new_op.get_prevail_overall();
            instant_plan.back().correspondingPlanStep = getIndexOfPlanStep(new_op,startTime);
            runningInstantActions.push_back(&(instant_plan.back()));
        }
    }
//    cout << "startPoints: " << startPoints << ", endPoints: " << endPoints << endl;
    assert(startPoints == endPoints);
}
