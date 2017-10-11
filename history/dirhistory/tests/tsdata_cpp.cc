#include "tsdata_cpp.h"
#include <monsoon/group_name.h>
#include <monsoon/histogram.h>
#include <monsoon/metric_name.h>
#include <monsoon/metric_value.h>
#include <monsoon/tags.h>
#include <monsoon/time_point.h>
#include <monsoon/time_series.h>
#include <monsoon/time_series_value.h>
#include <vector>

const std::tuple<monsoon::time_point, monsoon::time_point> tsdata_expected_time =
    std::make_tuple(
        monsoon::time_point("1980-01-01T08:00:00.000Z"),
        monsoon::time_point("1990-01-01T09:00:00.000Z"));

std::vector<monsoon::time_series> tsdata_expected() {
  using namespace std::literals;
  using monsoon::time_series;
  using monsoon::time_series_value;
  using monsoon::metric_name;
  using monsoon::metric_value;
  using monsoon::group_name;
  using monsoon::tags;
  using monsoon::histogram;
  using monsoon::time_point;

  return {
    time_series(time_point("1980-01-01T08:00:00.000Z"), {
        time_series_value(
            group_name({ "test", "histogram" }, tags({ {"true", metric_value(true)} })),
            { { metric_name({ "hist", "o", "gram" }),
                metric_value(histogram({ {{0, 1}, 2}, {{3, 4}, 5} })) }
            }),
        time_series_value(
            group_name({ "test", "int" }, tags({ {"false", metric_value(false)} })),
            { { metric_name({ "i", "n", "t" }),
                metric_value(42) }
            })
        }),
    time_series(time_point("1990-01-01T09:00:00.000Z"), {
        time_series_value(
            group_name({ "test", "histogram" }),
            { { metric_name({ "hist", "o", "gram" }),
                metric_value(histogram({ {{0, 1}, 2}, {{3, 4}, 5} })) }
            }),
        time_series_value(
            group_name({ "test", "flt" }),
            { { metric_name({ "f", "l", "o", "a", "t" }),
                metric_value(2.7182818284590452354)
              }
            }),
        time_series_value(
            group_name({ "test", "empty" }),
            { { metric_name({ "value" }),
                metric_value()
              }
            }),
        time_series_value(
            group_name({ "test", "string" }),
            { { metric_name({ "value" }),
                metric_value("a string")
              },
              { metric_name({ "another" }),
                metric_value("string")
              }
            })
        })
  };
}

std::unordered_set<monsoon::group_name> expected_groups() {
  std::unordered_set<monsoon::group_name> result;

  for (const auto& ts : tsdata_expected()) {
    for (const auto& tsv : ts.get_data())
      result.insert(tsv.get_name());
  }
  return result;
}

std::unordered_set<monsoon::simple_group> expected_simple_groups() {
  std::unordered_set<monsoon::simple_group> result;

  for (const auto& g : expected_groups())
    result.insert(g.get_path());
  return result;
}

auto expected_tagged_metrics()
-> std::unordered_set<
    std::tuple<monsoon::group_name, monsoon::metric_name>,
    monsoon::history::tsdata::metrics_hash> {
  auto result = std::unordered_set<
      std::tuple<monsoon::group_name, monsoon::metric_name>,
      monsoon::history::tsdata::metrics_hash>();

  for (const auto& ts : tsdata_expected()) {
    for (const auto& tsv : ts.get_data())
      for (const auto& mm : tsv.get_metrics()) {
        result.emplace(tsv.get_name(), mm.first);
      }
  }
  return result;
}

auto expected_untagged_metrics()
-> std::unordered_set<
    std::tuple<monsoon::simple_group, monsoon::metric_name>,
    monsoon::history::tsdata::metrics_hash> {
  auto result = std::unordered_set<
      std::tuple<monsoon::simple_group, monsoon::metric_name>,
      monsoon::history::tsdata::metrics_hash>();

  for (const auto& entry : expected_tagged_metrics())
    result.emplace(std::get<0>(entry).get_path(), std::get<1>(entry));
  return result;
}

/*
 *        final List<TimeSeriesCollection> tsdata = Stream.<TimeSeriesCollection>builder()
 *                .add(new SimpleTimeSeriesCollection(
 *                        DateTime.parse("1980-01-01T08:00:00.000Z"),
 *                        Stream.of(
 *                                new ImmutableTimeSeriesValue(
 *                                        GroupName.valueOf(
 *                                                SimpleGroupPath.valueOf("test", "histogram"),
 *                                                Tags.valueOf(singletonMap("true", MetricValue.TRUE))),
 *                                        singletonMap(
 *                                                MetricName.valueOf("hist", "o", "gram"),
 *                                                MetricValue.fromHistValue(
 *                                                        new Histogram(
 *                                                                new Histogram.RangeWithCount(0, 1, 2),
 *                                                                new Histogram.RangeWithCount(3, 4, 5))))),
 *                                new ImmutableTimeSeriesValue(
 *                                        GroupName.valueOf(
 *                                                SimpleGroupPath.valueOf("test", "int"),
 *                                                Tags.valueOf(singletonMap("false", MetricValue.FALSE))),
 *                                        singletonMap(
 *                                                MetricName.valueOf("i", "n", "t"),
 *                                                MetricValue.fromIntValue(42)))
 *                        )))
 *                .add(new SimpleTimeSeriesCollection(
 *                        DateTime.parse("1990-01-01T09:00:00.000Z"),
 *                        Stream.of(
 *                                new ImmutableTimeSeriesValue(
 *                                        GroupName.valueOf(
 *                                                SimpleGroupPath.valueOf("test", "histogram"),
 *                                                Tags.EMPTY),
 *                                        singletonMap(
 *                                                MetricName.valueOf("hist", "o", "gram"),
 *                                                MetricValue.fromHistValue(
 *                                                        new Histogram(
 *                                                                new Histogram.RangeWithCount(0, 1, 2),
 *                                                                new Histogram.RangeWithCount(3, 4, 5))))),
 *                                new ImmutableTimeSeriesValue(
 *                                        GroupName.valueOf(
 *                                                SimpleGroupPath.valueOf("test", "flt"),
 *                                                Tags.EMPTY),
 *                                        singletonMap(
 *                                                MetricName.valueOf("f", "l", "o", "a", "t"),
 *                                                MetricValue.fromDblValue(Math.E))),
 *                                new ImmutableTimeSeriesValue(
 *                                        GroupName.valueOf(
 *                                                SimpleGroupPath.valueOf("test", "empty"),
 *                                                Tags.EMPTY),
 *                                        singletonMap(
 *                                                MetricName.valueOf("value"),
 *                                                MetricValue.EMPTY)),
 *                                new ImmutableTimeSeriesValue(
 *                                        GroupName.valueOf(
 *                                                SimpleGroupPath.valueOf("test", "string"),
 *                                                Tags.EMPTY),
 *                                        ImmutableMap.<MetricName, MetricValue>builder()
 *                                        .put(
 *                                                MetricName.valueOf("value"),
 *                                                MetricValue.fromStrValue("a string"))
 *                                        .put(
 *                                                MetricName.valueOf("another"),
 *                                                MetricValue.fromStrValue("string"))
 *                                        .build())
 *                        )))
 *                .build()
 *                .collect(Collectors.toList());
 *
 *        new FileSupport(new FileSupport0(), true)
 *                .create_file(new File("/tmp/tsdata_v0.tsd").toPath(), tsdata);
 *        new FileSupport(new FileSupport1(), true)
 *                .create_file(new File("/tmp/tsdata_v1_list.tsd").toPath(), tsdata);
 *        new FileSupport(new FileTableFileSupport(), true)
 *                .create_file(new File("/tmp/tsdata_v2_table.tsd").toPath(), tsdata);
 *        new FileSupport(new FileListFileSupport(), true)
 *                .create_file(new File("/tmp/tsdata_v2_list.tsd").toPath(), tsdata);
 *
 */
