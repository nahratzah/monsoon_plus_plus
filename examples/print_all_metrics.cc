#include <monsoon/history/dir/dirhistory.h>
#include <monsoon/history/collect_history.h>
#include <monsoon/metric_source.h>
#include <monsoon/path_matcher.h>
#include <monsoon/tag_matcher.h>
#include <objpipe/of.h>
#include <iostream>
#include <memory>
#include <tuple>
#include <instrumentation/visitor.h>
#include <instrumentation/print_visitor.h>

auto open_dir(std::string dir)
-> std::unique_ptr<monsoon::collect_history> {
  return std::make_unique<monsoon::history::dirhistory>(dir, false);
}

int main(int argc, char** argv) {
  if (argc <= 1) {
    std::cerr << (argc >= 1 ? argv[0] : "print_all_metrics")
        << " /path/to/history/dir\n";
    return 1;
  }

  instrumentation::visitor::on_destroy_visitor(std::make_unique<instrumentation::print_visitor>(std::cerr));

  open_dir(argv[1])->emit({}, monsoon::path_matcher().push_back_double_wildcard(), monsoon::tag_matcher(), monsoon::path_matcher().push_back_double_wildcard())
      .filter([](const auto& x) { return std::holds_alternative<monsoon::metric_source::metric_emit>(x); })
      .transform([](const auto& x) -> const auto& { return std::get<monsoon::metric_source::metric_emit>(x); })
      .transform(
          [](const auto& x) {
            const auto& tp = std::get<0>(x);
            const auto& map = std::get<1>(x);
            return objpipe::of(map)
                .iterate()
                .transform(
                    [tp](const auto& metric_entry) {
                      return std::tuple_cat(
                          std::make_tuple(tp),
                          metric_entry.first,
                          std::make_tuple(metric_entry.second));
                    });
          })
      .iterate()
      .for_each(
          [](const auto& tuple) {
            std::cout
                << std::get<0>(tuple)
                << " "
                << std::get<1>(tuple)
                << "::"
                << std::get<2>(tuple)
                << " = "
                << std::get<3>(tuple)
                << "\n";
          });
}
