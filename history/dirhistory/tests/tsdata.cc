#include "UnitTest++/UnitTest++.h"
#include <monsoon/history/dir/tsdata.h>
#include <iostream>
#include "tsdata_cpp.h"
#include "tsdata_print.h"

using monsoon::history::tsdata;

std::string SAMPLE_DATA_DIR;

inline auto expect_version(std::uint16_t maj, std::uint16_t min)
-> std::tuple<std::uint16_t, std::uint16_t> {
  return { maj, min };
}

TEST(read_tsdata_v0) {
  auto tsd = tsdata::open(SAMPLE_DATA_DIR + "/tsdata_v0.tsd");
  REQUIRE CHECK_EQUAL(true, tsd != nullptr);

  CHECK_EQUAL(expect_version(0u, 1u), tsd->version());
  CHECK_EQUAL(tsdata_expected(), tsd->read_all());
}

TEST(read_tsdata_v1) {
  auto tsd = tsdata::open(SAMPLE_DATA_DIR + "/tsdata_v1.tsd");
  REQUIRE CHECK_EQUAL(true, tsd != nullptr);

  CHECK_EQUAL(expect_version(1u, 0u), tsd->version());
  CHECK_EQUAL(tsdata_expected(), tsd->read_all());
}

TEST(read_tsdata_v2_tables) {
  auto tsd = tsdata::open(SAMPLE_DATA_DIR + "/tsdata_v2_tables.tsd");
  REQUIRE CHECK_EQUAL(true, tsd != nullptr);

  CHECK_EQUAL(expect_version(2u, 0u), tsd->version());
  CHECK_EQUAL(tsdata_expected(), tsd->read_all());
}

TEST(read_tsdata_v2_list) {
  auto tsd = tsdata::open(SAMPLE_DATA_DIR + "/tsdata_v2_list.tsd");
  REQUIRE CHECK_EQUAL(true, tsd != nullptr);

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
