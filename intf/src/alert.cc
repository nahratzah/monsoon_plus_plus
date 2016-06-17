#include <monsoon/alert.h>

namespace monsoon {


auto alert::extend_with(alert&& a) noexcept -> alert& {
  if (state_ == a.state_) {
    trigger_duration_ += std::move(a.trigger_duration_);
  } else {
    trigger_duration_ = std::move(a.trigger_duration_);
    since_ = std::move(a.since_);
  }

  name_ = std::move(a.name_);
  value_ = std::move(a.value_);
  message_ = std::move(a.message_);
  state_ = std::move(a.state_);
  attributes_ = std::move(a.attributes_);
  return *this;
}


} /* namespace monsoon */
