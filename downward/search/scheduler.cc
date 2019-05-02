#include "scheduler.h"

#include <cassert>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

SimpleTemporalProblem::SimpleTemporalProblem(
        std::vector<std::string> _variable_names) :
    variable_names(_variable_names)
{
    variable_names.push_back("X0");
    number_of_nodes = variable_names.size();
    m_minimalDistances.resize(number_of_nodes);
    for(int i = 0; i < number_of_nodes; ++i) {
      matrix.push_back(MatrixLine(number_of_nodes, INF));
      m_minimalDistances[i] = INF;
      m_connections.push_back(vector<short>(number_of_nodes, 0));
      m_orderingIndexIsNode.push_back(-1);
      m_orderingIndexIsOrder.push_back(-1);
    }
    m_minimalDistances[number_of_nodes-1] = 0.0;
    for(int i = 0; i < number_of_nodes; i++) {
      matrix[i][i] = 0.0;
    }
}

// set the maximum distance between from and to.
void SimpleTemporalProblem::setDistance(int from, int to, double distance)
{
//    cout << "from:" << from << ", to:" << to << endl;
  assert(0 <= from && from < number_of_nodes);
  assert(0 <= to && to < number_of_nodes);
  assert(from != to);
  double& value = matrix[from][to];
  if(double_equals(value, INF)) { //uninitialized
      value = distance;
  } else {
      if(value < 0.0) {
          assert(distance < 0.0);
          value = min(value,distance);
      } else {
          assert((value > 0.0 && distance > 0.0) || double_equals(value,distance));
          value = max(value,distance);
      }
  }
  m_connections[from][to] = 1;
}

// set the interval into which value(to)-value(from) has to fall
void SimpleTemporalProblem::setInterval(int from, int to, double lower,
        double upper)
{
  setDistance(from,to,upper);
  setDistance(to,from,-lower);
}

// set the exact value which value(to)-value(from) must have
void SimpleTemporalProblem::setSingletonInterval(int from, int to,
        double lowerAndUpper)
{
  setInterval(from,to,lowerAndUpper,lowerAndUpper);
}

// set the interval into which value(to)-value(from) has to fall,
// with the assumption that the interval is open to the right.
void SimpleTemporalProblem::setUnboundedInterval(int from, int to, double lower)
{
  setInterval(from,to,lower,INF);
}

void SimpleTemporalProblem::setIntervalFromXZero(int to, double lower,
        double upper)
{
  setInterval(number_of_nodes-1,to,lower,upper);
}

void SimpleTemporalProblem::setUnboundedIntervalFromXZero(int to, double lower)
{
  setUnboundedInterval(number_of_nodes-1,to,lower);
}

// solve the STP using the Floyd-Warshall algorithm with running time O(n^3)
void SimpleTemporalProblem::solve()
{
    double triangle_length;

    for(size_t k = 0; k < number_of_nodes; k++) {
        vector<double>& matrixK = matrix[k];
        for(size_t i = 0; i < number_of_nodes; i++) {
            vector<double>& matrixI = matrix[i];
            for(size_t j = 0; j < number_of_nodes; j++) {
                triangle_length = matrixI[k];
                if(triangle_length != INF) {
                    triangle_length += matrixK[j];
                }
                double& mij = matrixI[j];
                if(triangle_length < INF && triangle_length < mij) {
                    // update
                    mij = triangle_length;
                }
            }
        }
    }
}

bool SimpleTemporalProblem::solveWithP3C()
{
  makeGraphChordal();
  initializeArcVectors();
    if (!performDPC()) {
    return false;
  }
  assert(m_orderingIndexIsNode.size() == number_of_nodes);
    for (int k = 0; k < number_of_nodes; ++k) {
    int kNode = m_orderingIndexIsOrder[k];
        for (int i = 0; i < m_arcsLarger[kNode].size(); ++i) {
      int iNode = m_arcsLarger[kNode][i];
            for (int j = 0; j < m_arcsLarger[kNode].size(); ++j) {
                if (i == j) {
          continue;
        }
        int jNode = m_arcsLarger[kNode][j];
        assert(arcExists(iNode, kNode) && arcExists(jNode, kNode));
        assert(arcExists(kNode, jNode));
        assert(arcExists(iNode, kNode));
                double triangleLength = add(matrix[iNode][jNode],
                        matrix[jNode][kNode]);
        double originalLength = matrix[iNode][kNode];
        if(triangleLength < originalLength) {
          matrix[iNode][kNode] = triangleLength;
        }
                triangleLength
                    = add(matrix[kNode][iNode], matrix[iNode][jNode]);
        originalLength = matrix[kNode][jNode];
        if(triangleLength < originalLength) {
          matrix[kNode][jNode] = triangleLength;
        }
      }
    }
  }
  extractMinimalDistances();
  return true;
}

bool SimpleTemporalProblem::performDPC()
{
  assert(m_orderingIndexIsNode.size() == m_orderingIndexIsOrder.size());
  assert(m_orderingIndexIsNode.size() == number_of_nodes);
    for (int k = number_of_nodes - 1; k >= 0; --k) {
    int kNode = m_orderingIndexIsOrder[k];
        for (int i = 0; i < m_arcsSmaller[kNode].size(); ++i) {
      int iNode = m_arcsSmaller[kNode][i];
            for (int j = 0; j < m_arcsSmaller[kNode].size(); ++j) {
                if (i == j) {
          continue;
        }
        int jNode = m_arcsSmaller[kNode][j];
        assert(arcExists(iNode, kNode) && arcExists(jNode, kNode));
        assert(arcExists(iNode, jNode));
                double triangleLength = add(matrix[iNode][kNode],
                        matrix[kNode][jNode]);
        double originalLength = matrix[iNode][jNode];
        if(triangleLength < originalLength) {
          matrix[iNode][jNode] = triangleLength;
        }
                if (matrix[iNode][jNode] + matrix[jNode][iNode] < 0) {
          return false;
        }
      }
    }
  }
  return true;
}

void SimpleTemporalProblem::makeGraphChordal()
{
  m_numberOfNeighbors.resize(number_of_nodes);
    for (int i = 0; i < number_of_nodes; ++i) {
    m_numberOfNeighbors[i] = calcNumberOfNeighbors(i);
  }

  int currentNode;
  list<int> currentChildren;
  int nextPositionInOrdering = 0;
    while ((currentNode = getNextNodeInOrdering()) != -1) {
        for (int j = 0; j < number_of_nodes; ++j) {
            if (arcExists(currentNode, j)) {
        m_numberOfNeighbors[j]--;
      }
    }

        m_orderingIndexIsNode[m_orderingIndexIsNode.size() - currentNode - 1]
            = nextPositionInOrdering;
        m_orderingIndexIsOrder[m_orderingIndexIsOrder.size()
            - nextPositionInOrdering - 1] = currentNode;
    nextPositionInOrdering++;

    currentChildren.clear();
    collectChildren(currentNode, currentChildren, true);
    createFillEdges(currentChildren);
  }
}

void SimpleTemporalProblem::extractMinimalDistances()
{
  assert(m_minimalDistances[number_of_nodes-1] == 0.0);
  list<int> nodesToUpdate;
  nodesToUpdate.push_front(number_of_nodes-1);
    while (!nodesToUpdate.empty()) {
    int currentNode = nodesToUpdate.back();
    nodesToUpdate.pop_back();
    list<int> currentChilds;
    currentChilds.clear();
    collectChildren(currentNode, currentChilds, false);
    list<int>::const_iterator it;
        for (it = currentChilds.begin(); it != currentChilds.end(); ++it) {
      int currentChild = *it;
      assert(currentNode != currentChild);
            double newDistance = add(m_minimalDistances[currentNode],
                    matrix[currentChild][currentNode]);
            if (newDistance + EPSILON < m_minimalDistances[currentChild]) {
        m_minimalDistances[currentChild] = newDistance;
        nodesToUpdate.push_front(currentChild);
      }
    }
  }
}

void SimpleTemporalProblem::initializeArcVectors()
{
  m_arcsLarger.clear();
  m_arcsSmaller.clear();
    for (int i = 0; i < number_of_nodes; ++i) {
    int iNode = m_orderingIndexIsOrder[i];
    m_arcsLarger.resize(number_of_nodes);
    m_arcsSmaller.resize(number_of_nodes);
        for (int j = i + 1; j < number_of_nodes; ++j) {
      int jNode = m_orderingIndexIsOrder[j];
            if (arcExists(iNode, jNode)) {
        m_arcsLarger[iNode].push_back(jNode);
      }
    }
        for (int j = i - 1; j >= 0; --j) {
      int jNode = m_orderingIndexIsOrder[j];
            if (arcExists(iNode, jNode)) {
        m_arcsSmaller[iNode].push_back((jNode));
      }
    }
  }
}

double SimpleTemporalProblem::add(double a, double b)
{
    if (a == INF || b == INF || a + b >= INF)
        return INF;
  return a + b;
}

void SimpleTemporalProblem::createFillEdges(list<int>& currentChildren)
{
  list<int>::const_iterator it1, it2;
    for (it1 = currentChildren.begin(); it1 != currentChildren.end(); ++it1) {
    it2 = it1;
    it2++;
        for (; it2 != currentChildren.end(); ++it2) {
            if (!arcExists(*it1, *it2)) {
        setInterval(*it1, *it2, -INF, +INF);
        m_numberOfNeighbors[*it1]++;
        m_numberOfNeighbors[*it2]++;
      }
    }
  }
}

void SimpleTemporalProblem::collectChildren(int currentNode,
        list<int>& currentChildren, bool ignoreNodesInOrdering)
{
    for (int i = 0; i < number_of_nodes; ++i) {
        if (arcExists(currentNode, i) && (!ignoreNodesInOrdering
                    || !isNodeAlreadyInOrdering(i))) {
      currentChildren.push_back(i);
    }
  }
}

int SimpleTemporalProblem::getNextNodeInOrdering()
{
  int ret = -1;
  int maxNumberOfNeighbors = 100000;
    for (int i = 0; i < matrix.size(); ++i) {
        if (!isNodeAlreadyInOrdering(i)) {
      int numberOfNeighbors = m_numberOfNeighbors[i];
            if (numberOfNeighbors < maxNumberOfNeighbors) {
        ret = i;
        maxNumberOfNeighbors = numberOfNeighbors;
      }
    }
  }
  return ret;
}

int SimpleTemporalProblem::calcNumberOfNeighbors(int node)
{
  int ret = 0;
    for (int i = 0; i < number_of_nodes; ++i) {
        if (arcExists(node, i) && !isNodeAlreadyInOrdering(i)) {
      ret++;
    }
  }
  return ret;
}

bool SimpleTemporalProblem::isNodeAlreadyInOrdering(int node)
{
    return (m_orderingIndexIsNode[m_orderingIndexIsNode.size() - node - 1]
            != -1);
}

// check if solution is valid
bool SimpleTemporalProblem::solution_is_valid()
{
    for(size_t i = 0; i < number_of_nodes; i++) {
        for(size_t j = i+1; j < number_of_nodes; j++) {
            if(matrix[i][j] < -matrix[j][i]) {
                return false;
            }
        }
    }
    return true;
}

// return the maximal distance from X0 to any node in the network.
double SimpleTemporalProblem::getMaximalTimePointInTightestSchedule(bool useP3C)
{
  double min = 0.0;
  for(size_t i = 0; i < number_of_nodes-1; i++) {
        double time = useP3C ? m_minimalDistances[i]
            : matrix[i][number_of_nodes - 1];
        if (time < min)
            min = time;
  }
  return -min;
}

void SimpleTemporalProblem::setCurrentDistancesAsDefault()
{
  m_defaultDistances = matrix;
}

void SimpleTemporalProblem::reset()
  {
    matrix = m_defaultDistances;
    for (int i = 0; i < number_of_nodes; ++i) {
    m_orderingIndexIsNode[i] = -1;
    m_orderingIndexIsOrder[i] = -1;
    m_minimalDistances[i] = INF;
  }
  m_minimalDistances[number_of_nodes-1] = 0.0;
}

// dump problem
void SimpleTemporalProblem::dumpDistances()
{
  std::cout << std::setw(10) << "";
  for(size_t i = 0; i < number_of_nodes; i++) {
    std::cout << std::setw(10) << i;//variable_names[i];
  }
  std::cout << std::endl;
  for(size_t i = 0; i < number_of_nodes; i++) {
    std::cout << std::setw(10) << i;//variable_names[i];
    for(size_t j = 0; j < number_of_nodes; j++) {
      std::cout << std::setw(10) << matrix[i][j];
    }
    std::cout << std::endl;
  }
  std::cout << std::endl;
}

// dump problem
void SimpleTemporalProblem::dumpConnections()
{
  std::cout << std::setw(10) << "";
  for(size_t i = 0; i < number_of_nodes; i++) {
    std::cout << std::setw(10) << i;//variable_names[i];
  }
  std::cout << std::endl;
  for(size_t i = 0; i < number_of_nodes; i++) {
    std::cout << std::setw(10) << i;//variable_names[i];
    for(size_t j = 0; j < number_of_nodes; j++) {
      std::cout << std::setw(10) << m_connections[i][j];
    }
    std::cout << std::endl;
  }
  std::cout << std::endl;
}

// dump solution
void SimpleTemporalProblem::dumpSolution()
{

  std::vector<Happening> happenings = getHappenings(true);

  for(size_t i = 0; i < happenings.size(); i++) {
    assert(happenings[i].second <= number_of_nodes-1);
    std::string& node = variable_names[happenings[i].second];
    std::cout << "    " << node << "@" << happenings[i].first << std::endl;
  }
  std::cout << std::endl;
}

std::vector<Happening> SimpleTemporalProblem::getHappenings(bool useP3C)
{
    std::vector<Happening> happenings;
    for(size_t i = 0; i < number_of_nodes - 1; i++) {
        double time = useP3C ? -m_minimalDistances[i]
            : -matrix[i][number_of_nodes - 1];
        happenings.push_back(std::make_pair(time, i));
    }

    sort(happenings.begin(), happenings.end(), HappeningComparator());

    return happenings;
}

