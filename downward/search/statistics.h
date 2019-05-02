#ifndef STATISTICS_H
#define STATISTICS_H

#include <math.h>
#include <stdio.h>
#include <string.h>

/// This class calculates mean and standard deviation for incremental values.
template <class T>
class Statistics
{
   public:
      Statistics(const char* name = "");
      
      void reset();
      void addMeasurement(T val);

      T getMean() const;
      T getStandardDeviation() const;
      int getNumMeasurements() const;

      T getMin() const { return _min; }
      T getMax() const { return _max; }

      void print(const double factor=1.0) const;

   protected:
      T _sumMeasurements;
      T _sumSquaredMeasurements;
      double _numMeasurements;

      T _min;
      T _max;

      const char* _name;
};

template <class T>
Statistics<T>::Statistics(const char* name) : _name(name)
{
   reset();
}
      
template <class T>
void Statistics<T>::reset()
{
   _sumMeasurements = static_cast<T>(0);
   _sumSquaredMeasurements = static_cast<T>(0);
   _numMeasurements = 0;
   _min = HUGE_VAL;     // this probably only works for doubles
   _max = -HUGE_VAL;
}

template <class T>
void Statistics<T>::addMeasurement(T val)
{
   _sumMeasurements += val;
   _sumSquaredMeasurements += (val * val);
   _numMeasurements += 1.0;
   if(val > _max)
      _max = val;
   if(val < _min)
      _min = val;
}

template <class T>
T Statistics<T>::getMean() const
{
   if(_numMeasurements == 0)
      return static_cast<T>(0);
   return _sumMeasurements/_numMeasurements;
}

template <class T>
T Statistics<T>::getStandardDeviation() const
{
   if(_numMeasurements < 2)
      return static_cast<T>(0);

   return sqrt( (_numMeasurements * _sumSquaredMeasurements - _sumMeasurements * _sumMeasurements) / (_numMeasurements * (_numMeasurements - 1)) );
}

template <class T>
int Statistics<T>::getNumMeasurements() const
{
   return (int)_numMeasurements;
}

template <class T>
void Statistics<T>::print(const double factor) const
{
    if(strlen(_name) == 0)
        printf("Mean %.2f Std: %.2f (Num: %.0f)\n", getMean()*factor, getStandardDeviation()*factor, _numMeasurements);
    else
        printf("%s: Mean %.2f Std: %.2f (Num: %.0f)\n", _name, getMean()*factor, getStandardDeviation()*factor, _numMeasurements);
}

#endif

