#include "UnitTest++/UnitTest++.h"
#include <monsoon/history/dir/tsdata.h>
#include <iostream>

using monsoon::history::tsdata;

std::string SAMPLE_DATA;

TEST(read_tsdata_v0) {
  auto tsd = tsdata::open(SAMPLE_DATA + "/tsdata_v0.tsd");
}

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Require argument: path to sample data directory.\n";
    return 1;
  }
  SAMPLE_DATA = argv[1];

  return UnitTest::RunAllTests();
}
