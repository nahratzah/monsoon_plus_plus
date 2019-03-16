#ifndef MONSOON_HISTORY_PRINT_HISTORY_H
#define MONSOON_HISTORY_PRINT_HISTORY_H

#include <monsoon/history/history_export_.h>
#include <monsoon/history/collect_history.h>

namespace monsoon {


///\brief A simple history that prints out all its received values.
class monsoon_history_export_ print_history
: public collect_history
{
 public:
  ~print_history() noexcept override;

  auto time() const -> std::tuple<time_point, time_point> override;

  auto emit(
      time_range tr,
      path_matcher group_filter,
      tag_matcher group_tag_filter,
      path_matcher metric_filter,
      time_point::duration slack) const
      -> objpipe::reader<emit_type> override;

  auto emit_time(
      time_range tr,
      time_point::duration slack) const
      -> objpipe::reader<time_point> override;

 private:
  void do_push_back_(const metric_emit&) override;
};


} /* namespace monsoon */

#endif /* MONSOON_HISTORY_PRINT_HISTORY_H */
