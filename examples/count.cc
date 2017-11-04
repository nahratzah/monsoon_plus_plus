#include <monsoon/history/dir/dirhistory.h>
#include <monsoon/history/collect_history.h>
#include <monsoon/time_point.h>
#include <monsoon/time_range.h>
#include <monsoon/group_name.h>
#include <monsoon/metric_name.h>
#include <monsoon/metric_value.h>
#include <cstdint>
#include <iostream>

auto open_dir(std::string dir)
-> std::shared_ptr<monsoon::collect_history> {
  return std::make_unique<monsoon::history::dirhistory>(dir, false);
}

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << (argc >= 1 ? argv[0] : "count") << " /path/to/history/dir\n";
    return 1;
  }

  auto history = open_dir(argv[1]);
  std::uintmax_t counter = 0u;
  for (auto e : history->emit_time(monsoon::time_range()))
    ++counter;
  std::cout << counter << " scrapes\n";
  return 0;
}
