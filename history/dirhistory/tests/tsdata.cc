#include "UnitTest++/UnitTest++.h"
#include <monsoon/history/dir/tsdata.h>
#include <iostream>
#include "tsdata_cpp.h"

using monsoon::history::tsdata;

std::string SAMPLE_DATA_DIR;

TEST(read_tsdata_v0) {
  auto tsd = tsdata::open(SAMPLE_DATA_DIR + "/tsdata_v0.tsd");
  REQUIRE CHECK_EQUAL(true, tsd != nullptr);

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
