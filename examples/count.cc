#include <monsoon/history/dir/dirhistory.h>
#include <monsoon/history/collect_history.h>
#include <monsoon/time_point.h>
#include <monsoon/time_range.h>
#include <monsoon/group_name.h>
#include <monsoon/metric_name.h>
#include <monsoon/metric_value.h>
#include <monsoon/acceptor.h>
#include <cstdint>
#include <iostream>

auto open_dir(std::string dir)
-> std::shared_ptr<monsoon::collect_history> {
  return std::make_unique<monsoon::history::dirhistory>(dir, false);
}

class scrape_counter
: public monsoon::acceptor<monsoon::group_name, monsoon::metric_name, monsoon::metric_value>
{
 public:
  void accept(monsoon::time_point, vector_type) {
    ++count;
  }

  std::uint64_t count = 0;
};

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << (argc >= 1 ? argv[0] : "count") << " /path/to/history/dir\n";
    return 1;
  }

  auto history = open_dir(argv[1]);
  const auto tr = monsoon::time_range();

  scrape_counter counter;
  history->emit(counter, tr, history->untagged_metrics(tr));
  std::cout << counter.count << " scrapes\n";
  return 0;
}
