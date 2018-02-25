#ifndef TSDATA_CPP_H
#define TSDATA_CPP_H

#include <monsoon/time_series.h>
#include <monsoon/time_point.h>
#include <monsoon/metric_source.h>
#include <monsoon/history/dir/tsdata.h>
#include <tuple>
#include <vector>

/**
 * TSData in the sample files.
 */
std::vector<monsoon::time_series> tsdata_expected();
std::unordered_set<monsoon::group_name> expected_groups();
std::unordered_set<monsoon::simple_group> expected_simple_groups();
auto expected_tagged_metrics()
-> std::unordered_set<
    std::tuple<monsoon::group_name, monsoon::metric_name>,
    monsoon::metric_source::metrics_hash>;
auto expected_untagged_metrics()
-> std::unordered_set<
    std::tuple<monsoon::simple_group, monsoon::metric_name>,
    monsoon::metric_source::metrics_hash>;

/**
 * Expected time range.
 */
extern const std::tuple<monsoon::time_point, monsoon::time_point> tsdata_expected_time;

#endif /* TSDATA_CPP_H */
