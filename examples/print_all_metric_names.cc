#include <monsoon/history/dir/dirhistory.h>
#include <monsoon/history/collect_history.h>
#include <monsoon/metric_source.h>
#include <monsoon/path_matcher.h>
#include <monsoon/tag_matcher.h>
#include <objpipe/of.h>
#include <iostream>
#include <memory>
#include <tuple>
#include <unordered_set>
#include <instrumentation/visitor.h>
#include <instrumentation/print_visitor.h>

auto open_dir(std::string dir)
-> std::unique_ptr<monsoon::collect_history> {
  return std::make_unique<monsoon::history::dirhistory>(dir, false);
}

using name_type = std::tuple<monsoon::group_name, monsoon::metric_name>;

struct name_hash {
  auto operator()(const name_type& x) const noexcept -> std::size_t {
    return 17u * std::hash<monsoon::group_name>()(std::get<0>(x))
        + std::hash<monsoon::metric_name>()(std::get<1>(x));
  }
};

using name_set = std::unordered_set<name_type, name_hash>;

int main(int argc, char** argv) {
  if (argc <= 1) {
    std::cerr << (argc >= 1 ? argv[0] : "print_all_metrics")
        << " /path/to/history/dir\n";
    return 1;
  }

  instrumentation::visitor::on_destroy_visitor(std::make_unique<instrumentation::print_visitor>(std::cerr));

  auto names = open_dir(argv[1])->emit({}, monsoon::path_matcher().push_back_double_wildcard(), monsoon::tag_matcher(), monsoon::path_matcher().push_back_double_wildcard())
      .filter([](const auto& x) { return std::holds_alternative<monsoon::metric_source::metric_emit>(x); })
      .select<monsoon::metric_source::metric_emit>()
      .select<1>()
      .iterate()
      .select_first()
      .async(objpipe::multithread_unordered_push())
      .reduce(
          []() { return name_set(); },
          [](name_set& set, auto&& nm) -> objpipe::objpipe_errc {
            set.emplace(std::forward<decltype(nm)>(nm));
            return objpipe::objpipe_errc::success;
          },
          [](name_set& r, name_set&& x) {
#if __cpp_lib_node_extract >= 201703
            r.merge(std::move(x));
#else
            r.insert(x.begin(), x.end());
#endif
          },
          [](name_set&& r) -> name_set { return std::move(r); });

  objpipe::of(names.get())
      .iterate()
      .for_each(
          [](const name_type& tuple) {
            std::cout
                << std::get<0>(tuple)
                << "::"
                << std::get<1>(tuple)
                << "\n";
          });
}
