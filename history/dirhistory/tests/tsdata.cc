#include "UnitTest++/UnitTest++.h"
#include <monsoon/history/dir/tsdata.h>
#include <monsoon/metric_source.h>
#include <monsoon/time_series.h>
#include <iostream>
#include "tsdata_cpp.h"
#include "tsdata_print.h"

using monsoon::history::tsdata;

std::string SAMPLE_DATA_DIR;

inline auto expect_version(std::uint16_t maj, std::uint16_t min)
-> std::tuple<std::uint16_t, std::uint16_t> {
  return { maj, min };
}

auto tsdata_to_metric_emit(const monsoon::time_series& ts)
-> monsoon::metric_source::metric_emit {
  monsoon::metric_source::metric_emit result{ ts.get_time(), {} };
  std::for_each(
      ts.get_data().begin(),
      ts.get_data().end(),
      [&result](const auto& tsv) {
        std::for_each(
            tsv.get_metrics().begin(),
            tsv.get_metrics().end(),
            [&result, &tsv](const auto& metric_entry) {
              std::get<1>(result)[std::make_tuple(tsv.get_name(), metric_entry.first)] = metric_entry.second;
            });
      });
  return result;
}

TEST(read_tsdata_v0) {
  auto tsd = tsdata::open(SAMPLE_DATA_DIR + "/tsdata_v0.tsd");
  REQUIRE CHECK_EQUAL(true, tsd != nullptr);

  CHECK_EQUAL(expect_version(0u, 1u), tsd->version());
  CHECK_EQUAL(tsdata_expected(), tsd->read_all());
  CHECK_EQUAL(tsdata_expected_time, tsd->time());
}

TEST(push_back_tsdata_v0) {
  auto tsd = tsdata::new_file(monsoon::io::fd::tmpfile("monsoon_tsdata_test"), 0u);
  REQUIRE CHECK_EQUAL(true, tsd != nullptr);

  for (const auto& x : tsdata_expected()) tsd->push_back(tsdata_to_metric_emit(x));

  CHECK_EQUAL(expect_version(0u, 1u), tsd->version());
  CHECK_EQUAL(tsdata_expected(), tsd->read_all());
}

TEST(read_tsdata_v1) {
  auto tsd = tsdata::open(SAMPLE_DATA_DIR + "/tsdata_v1.tsd");
  REQUIRE CHECK_EQUAL(true, tsd != nullptr);

  CHECK_EQUAL(expect_version(1u, 0u), tsd->version());
  CHECK_EQUAL(tsdata_expected(), tsd->read_all());
  CHECK_EQUAL(tsdata_expected_time, tsd->time());
}

TEST(push_back_tsdata_v1) {
  auto tsd = tsdata::new_file(monsoon::io::fd::tmpfile("monsoon_tsdata_test"), 1u);
  REQUIRE CHECK_EQUAL(true, tsd != nullptr);

  for (const auto& x : tsdata_expected()) tsd->push_back(tsdata_to_metric_emit(x));

  CHECK_EQUAL(expect_version(1u, 0u), tsd->version());
  CHECK_EQUAL(tsdata_expected(), tsd->read_all());
}

TEST(read_tsdata_v2_tables) {
  auto tsd = tsdata::open(SAMPLE_DATA_DIR + "/tsdata_v2_tables.tsd");
  REQUIRE CHECK_EQUAL(true, tsd != nullptr);

  CHECK_EQUAL(expect_version(2u, 0u), tsd->version());
  CHECK_EQUAL(tsdata_expected(), tsd->read_all());
  CHECK_EQUAL(tsdata_expected_time, tsd->time());
}

TEST(read_tsdata_v2_list) {
  auto tsd = tsdata::open(SAMPLE_DATA_DIR + "/tsdata_v2_list.tsd");
  REQUIRE CHECK_EQUAL(true, tsd != nullptr);

  CHECK_EQUAL(expect_version(2u, 0u), tsd->version());
  CHECK_EQUAL(tsdata_expected(), tsd->read_all());
  CHECK_EQUAL(tsdata_expected_time, tsd->time());
}

TEST(push_back_tsdata_v2) {
  auto tsd = tsdata::new_file(monsoon::io::fd::tmpfile("monsoon_tsdata_test"), 2u);
  REQUIRE CHECK_EQUAL(true, tsd != nullptr);

  for (const auto& x : tsdata_expected()) tsd->push_back(tsdata_to_metric_emit(x));

  CHECK_EQUAL(expect_version(2u, 0u), tsd->version());
  CHECK_EQUAL(tsdata_expected(), tsd->read_all());
}

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Require argument: path to sample data directory.\n";
    return 1;
  }
  SAMPLE_DATA_DIR = argv[1];

  return UnitTest::RunAllTests();
}
