#ifndef MONITORING_H
#define MONITORING_H

#include <deque>
#include <vector>
#include <string>
#include <iostream>

#include "search_engine.h"
#include "globals.h"
#include "state.h"
#include "operator.h"

/// A full plan trace is a list of FullPlanSteps.
class FullPlanTrace
{
   public:
      /// initializing constructor
      explicit FullPlanTrace(const TimeStampedState & s);

      /// A detailed (inefficient) plan step used in monitoring.
      class FullPlanStep
      {
         public:
            FullPlanStep(const TimeStampedState & p, const Operator* o, const TimeStampedState & s)
               : predecessor(p), op(o), state(s) {}
            
            TimeStampedState predecessor;
            const Operator* op;
            TimeStampedState state;
      };

      /// is op applicable to the last state in plan.
      bool isApplicable(const Operator* op) const;

      /// return a new FullPlanTrace that has one more step, where op is applied.
      /**
       * \returns the correct result if isApplicable(op) is true, the result is undefined otherwise.
       */
      FullPlanTrace applyOperator(const Operator* op) const;

      /// can the last state let_time_pass, i.e. are there running operators.
      bool canLetTimePass() const;

      /// return a new FullPlanTrace where the last operator did let_time_pass.
      /**
       * \returns the correct result, if canLetTimePass is true, the result is undefined otherwise.
       */
      FullPlanTrace letTimePass() const;

      /// \returns true, if the last state satisfies g_goal.
      bool satisfiesGoal() const;

      /// \returns the timestamp of the last state in the trace
      double lastTimestamp() const;

      /// print plan in output format into os
      void outputPlan(ostream & os) const;

      void dumpLastState() const;

      /// the timestamp of the last state.
      double getLastTimestamp() const;

   protected:
      /// A plan is a list of FullPlanStep where each steps state equals the next steps predecessor.
      std::deque<FullPlanStep> plan;

};

class MonitorEngine
{
    protected:
        MonitorEngine();
        static MonitorEngine* instance;

        static void readPlanFromFile(const string & filename, vector<string> & plan);

    public:
        ~MonitorEngine();

        static MonitorEngine* getInstance();

        /// Static convenience function to validate the plan from this file.
        static bool validatePlan(const string & filename);

        bool validatePlan(std::vector<std::string> & plan);

        /// Validate a plan.
        bool validatePlan(std::vector<PlanStep> & plan);
        /// Old version - no idea how that works
        bool validatePlanOld(const std::vector<PlanStep> & plan) __attribute__((deprecated));
};

#endif

