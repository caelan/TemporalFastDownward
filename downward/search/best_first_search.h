#ifndef BEST_FIRST_SEARCH_H
#define BEST_FIRST_SEARCH_H

#include <vector>
#include <queue>
#include "closed_list.h"
#include "search_engine.h"
#include "state.h"
#include "operator.h"
#include <tr1/tuple>
#include "search_statistics.h"
#include "globals.h"

class Heuristic;

typedef std::tr1::tuple<const TimeStampedState *, vector<const Operator *>, double> OpenListEntry;

class OpenListEntryCompare
{
    public:
        bool operator()(const OpenListEntry left_entry, const OpenListEntry right_entry) const
        {
            return std::tr1::get<2>(right_entry) < std::tr1::get<2>(left_entry);
        }
};

typedef priority_queue<OpenListEntry, std::vector<OpenListEntry>,OpenListEntryCompare> OpenList;

struct OpenListInfo
{
	OpenListMode mode;
    OpenListInfo(Heuristic *heur, bool only_pref, OpenListMode _mode=REGULAR);
    Heuristic *heuristic;
    bool only_preferred_operators;
    //    OpenList<OpenListEntry> open;
    OpenList open;
    int priority; // low value indicates high priority
};

/// Maps logical state to best timestamp for that state
typedef std::map<std::vector<double>, double> LogicalStateClosedList;
bool knownByLogicalStateOnly(LogicalStateClosedList& scl,
        const TimedSymbolicStates& timedSymbolicStates);

class BestFirstSearchEngine : public SearchEngine
{
    private:
        std::vector<Heuristic *> heuristics;
        std::vector<Heuristic *> preferred_operator_heuristics_reg;
        std::vector<Heuristic *> preferred_operator_heuristics_ordered;
        std::vector<Heuristic *> preferred_operator_heuristics_cheapest;
        std::vector<Heuristic *> preferred_operator_heuristics_most_expensive;
        std::vector<Heuristic *> preferred_operator_heuristics_rand;
        std::vector<Heuristic *> preferred_operator_heuristics_concurrent;
        ClosedList closed_list;
        int number_of_expanded_nodes;
        
        LogicalStateClosedList logical_state_closed_list;

        std::vector<double> best_heuristic_values_of_queues;

        TimeStampedState current_state;
        const TimeStampedState *current_predecessor;
        vector<const Operator*> current_operators;

        time_t start_time;

        SearchStatistics search_statistics;

        int activeQueue;
        int lastProgressAtExpansionNumber;
        int numberOfSearchSteps;

    private:
        bool is_dead_end();
        bool check_goal();
        bool check_progress();
        void report_progress();
        void reward_progress();
        void generate_successors(const TimeStampedState *parent_ptr);
        void dump_transition() const;
        /// Dump the whole knowledge of search engine.
        void dump_everything() const;
        OpenListInfo *select_open_queue();

        void dump_plan_prefix_for_current_state() const;
        void dump_plan_prefix_for__state(const TimeStampedState &state) const;

        /// Compute G depending on the current g mode.
        double getG(const TimeStampedState* state_ptr, const TimeStampedState* closed_ptr, const Operator* op) const;
        double getGc(const TimeStampedState *parent_ptr) const;
        double getGc(const TimeStampedState *parent_ptr, const Operator *op) const;
        double getGm(const TimeStampedState *parent_ptr) const;
        double getGt(const TimeStampedState *parent_ptr) const;

    protected:

    public:
        std::vector<OpenListInfo> open_lists;
        int queueStartedLastWith;
        virtual SearchEngine::status step();
        void reset();
        enum QueueManagementMode
        {
            ROUND_ROBIN, PRIORITY_BASED
        } mode;

        BestFirstSearchEngine(QueueManagementMode _mode);
        ~BestFirstSearchEngine();
        void add_heuristic(Heuristic *heuristic, bool use_estimates,
                bool use_preferred_operators, bool pref_ops_cheapest_mode = false,
                bool pref_ops_most_expensive_mode = false, bool pref_ops_ordered_mode = false,
                bool pref_ops_rand_mode = false, bool pref_ops_concurrent_mode = false);
        virtual void statistics(time_t & current_time);
        virtual void initialize();
        SearchEngine::status fetch_next_state();

    public:
        double bestMakespan;
        double bestSumOfGoals;
};

#endif
