#ifndef TSDATA_CPP_H
#define TSDATA_CPP_H

#include <monsoon/time_series.h>
#include <monsoon/time_point.h>
#include <tuple>
#include <vector>

/**
 * TSData in the sample files.
 */
std::vector<monsoon::time_series> tsdata_expected();

/**
 * Expected time range.
 */
extern const std::tuple<monsoon::time_point, monsoon::time_point> tsdata_expected_time;

#endif /* TSDATA_CPP_H */
