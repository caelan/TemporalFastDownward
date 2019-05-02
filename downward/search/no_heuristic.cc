#include "no_heuristic.h"

#include "globals.h"
#include "operator.h"
#include "state.h"

using namespace std;

void NoHeuristic::initialize()
{
    cout << "Initializing no heuristic..." << endl;
}

double NoHeuristic::compute_heuristic(const TimeStampedState &state)
{
    if(state.satisfies(g_goal) && state.scheduled_effects.empty())
        return 0.0;

    return (state.timestamp + 1.0);

    //    return 1;
}
