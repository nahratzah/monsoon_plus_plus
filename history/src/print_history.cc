#include <monsoon/history/print_history.h>
#include <monsoon/time_point.h>
#include <monsoon/group_name.h>
#include <monsoon/metric_name.h>
#include <monsoon/metric_value.h>
#include <iostream>
#include <objpipe/of.h>

namespace monsoon {


print_history::~print_history() noexcept {}

auto print_history::time() const
-> std::tuple<time_point, time_point> {
  time_point tp = time_point::now();
  return { tp, tp };
}

auto print_history::emit(
    time_range tr,
    path_matcher group_filter,
    tag_matcher group_tag_filter,
    path_matcher metric_filter,
    time_point::duration slack) const
-> objpipe::reader<emit_type> {
  return objpipe::of<emit_type>();
}

auto print_history::emit_time(
    time_range tr,
    time_point::duration slack) const
-> objpipe::reader<time_point> {
  return objpipe::of<time_point>();
}

void print_history::do_push_back_(const metric_emit& m) {
  const time_point& tp = std::get<0>(m);
  const auto& collection = std::get<1>(m);

  std::for_each(
      collection.cbegin(),
      collection.cend(),
      [tp](const auto& datum) {
        std::cerr
            << tp
            << " "
            << std::get<0>(datum.first)
            << "::"
            << std::get<1>(datum.first)
            << " = "
            << datum.second
            << std::endl;
      });
}


} /* namespace monsoon */
