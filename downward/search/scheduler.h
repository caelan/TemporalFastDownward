#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <vector>
#include <list>
#include <string>
#include "globals.h"

const double INF = 10000000.0;

typedef std::pair<double,int> Happening;

class SimpleTemporalProblem
{
private:

        struct HappeningComparator
        {
                bool operator()(const Happening& h1, const Happening& h2)
                {
                    if (h1.first < h2.first)
                        return true;
                    if (h2.first < h1.first)
                        return false;
      return h1.second < h2.second;
    }
  };

  typedef std::vector<double> MatrixLine;
  typedef std::vector<MatrixLine> DistanceMatrix;

  int number_of_nodes;
  std::vector<std::string> variable_names;
  DistanceMatrix m_defaultDistances;
  DistanceMatrix matrix;

  void setDistance(int from, int to, double distance);

  void extractMinimalDistances();

  std::vector<std::vector<int> > m_arcsSmaller;
  std::vector<std::vector<int> > m_arcsLarger;
  std::vector<std::vector<short> > m_connections;
  std::vector<int> m_orderingIndexIsNode;
  std::vector<int> m_orderingIndexIsOrder;
  std::vector<int> m_numberOfNeighbors;
  std::vector<double> m_minimalDistances;

public:
    SimpleTemporalProblem(std::vector<std::string> _variable_names);

  void setInterval(int from, int to, double lower, double upper);

  void setSingletonInterval(int from, int to, double lowerAndUpper);
  void setUnboundedInterval(int from, int to, double lower);

  void setIntervalFromXZero(int to, double lower, double upper);
  void setUnboundedIntervalFromXZero(int to, double lower);

  void setCurrentDistancesAsDefault();
  void solve();
  bool solveWithP3C();
  void makeGraphChordal();
  void initializeArcVectors();
  bool performDPC();
  inline bool arcExists(int nodeA, int nodeB)
  {
    return (m_connections[nodeA][nodeB]);
  }
  double add(double a, double b);
  void createFillEdges(std::list<int>& currentChildren);
        void collectChildren(int currentNode, std::list<int>& currentChildren,
                bool ignoreNodesInOrdering);
  int getNextNodeInOrdering();
  int calcNumberOfNeighbors(int node);
  bool isNodeAlreadyInOrdering(int node);

  bool solution_is_valid();
  double getMaximalTimePointInTightestSchedule(bool useP3C);
  void reset();

  void dumpDistances();
  void dumpConnections();
  void dumpSolution();
  std::vector<Happening> getHappenings(bool useP3C);
};

#endif
