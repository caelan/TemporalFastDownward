#ifndef SEARCH_ENGINE_H
#define SEARCH_ENGINE_H

#include "globals.h"

class SearchEngine
{
    private:
        bool solved;
        bool solved_at_least_once;
        Plan plan;
        PlanTrace path;
    public:
        enum status
        {
            SOLVED,                 ///< Found a plan
            FAILED,                 ///< No plan could be found at all, explored search space
            SOLVED_COMPLETE,        ///< explored search space, but found a plan earlier
            IN_PROGRESS,            ///< Still searching
            FAILED_TIMEOUT,         ///< No plan found, ran into timeout
            SOLVED_TIMEOUT          ///< Found a plan, but not explored search space
        };
    protected:
        virtual enum status step() = 0;

        void set_plan(const Plan &plan);
        void set_path(const PlanTrace &states);
    public:
        SearchEngine();
        virtual ~SearchEngine();
        virtual void statistics(time_t & current_time) const;
        virtual void initialize() {}
        virtual void dump_everything() const = 0;
        bool found_solution() const;
        bool found_at_least_one_solution() const;
        const Plan &get_plan() const;
        const PlanTrace& get_path() const;
        enum status search();
};

#endif
