#ifndef TIME_MEASURE_HPP
#define TIME_MEASURE_HPP

#include <ctime>

class TimeMeasure {
public:
  static timespec diff(timespec start, timespec end) {
    timespec result;
    if ((end.tv_nsec - start.tv_nsec) < 0) {
      result.tv_sec = end.tv_sec - start.tv_sec - 1;
      result.tv_nsec = 1000000000L + end.tv_nsec - start.tv_nsec;
    } else {
      result.tv_sec = end.tv_sec - start.tv_sec;
      result.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return result;
  }

  static timespec sum(timespec t0, timespec t1) {
    timespec result;
    result.tv_sec = t0.tv_sec + t1.tv_sec ;
    result.tv_nsec = t0.tv_nsec + t1.tv_nsec ;

    if (result.tv_nsec >= 1000000000L) {
      result.tv_sec++;  
      result.tv_nsec = result.tv_nsec - 1000000000L;
    }

    return result;
  }

  static double percentage(timespec numerator, timespec denominator) {
    return double(asNanoSeconds(numerator)) / double(asNanoSeconds(denominator));
  }

  static long long asNanoSeconds(timespec t) {
    return 1000000000LL * t.tv_sec + t.tv_nsec;
  }
};

#endif
