#include <monsoon/alert.h>

namespace monsoon {


alert::alert()
: state_(alert_state::UNKNOWN)
{}

alert::alert(const alert& a)
: name_(a.name_),
  value_(a.value_),
  message_(a.message_),
  state_(a.state_),
  since_(a.since_),
  trigger_duration_(a.trigger_duration_),
  attributes_(a.attributes_)
{}

alert::alert(alert&& a) noexcept
: name_(std::move(a.name_)),
  value_(std::move(a.value_)),
  message_(std::move(a.message_)),
  state_(std::move(a.state_)),
  since_(std::move(a.since_)),
  trigger_duration_(std::move(a.trigger_duration_)),
  attributes_(std::move(a.attributes_))
{}

auto alert::operator=(const alert& a) -> alert& {
  name_ = a.name_;
  value_ = a.value_;
  message_ = a.message_;
  state_ = a.state_;
  since_ = a.since_;
  trigger_duration_ = a.trigger_duration_;
  attributes_ = a.attributes_;
  return *this;
}

auto alert::operator=(alert&& a) noexcept -> alert& {
  name_ = std::move(a.name_);
  value_ = std::move(a.value_);
  message_ = std::move(a.message_);
  state_ = std::move(a.state_);
  since_ = std::move(a.since_);
  trigger_duration_ = std::move(a.trigger_duration_);
  attributes_ = std::move(a.attributes_);
  return *this;
}

alert::~alert() noexcept {}

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
