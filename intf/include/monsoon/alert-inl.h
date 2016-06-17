#ifndef MONSOON_ALERT_INL_H
#define MONSOON_ALERT_INL_H

#include <utility>

namespace monsoon {


inline alert::alert(alert&& a) noexcept
: name_(std::move(a.name_)),
  value_(std::move(a.value_)),
  message_(std::move(a.message_)),
  state_(std::move(a.state_)),
  since_(std::move(a.since_)),
  trigger_duration_(std::move(a.trigger_duration_)),
  attributes_(std::move(a.attributes_))
{}

inline auto alert::operator=(alert&& a) noexcept -> alert& {
  name_ = std::move(a.name_);
  value_ = std::move(a.value_);
  message_ = std::move(a.message_);
  state_ = std::move(a.state_);
  since_ = std::move(a.since_);
  trigger_duration_ = std::move(a.trigger_duration_);
  attributes_ = std::move(a.attributes_);
  return *this;
}

inline auto alert::get_name() const noexcept -> const group_name& {
  return name_;
}

inline auto alert::value_is_ok() const noexcept -> bool {
  return !value_.get(true);
}

inline auto alert::value_is_triggering() const noexcept -> bool {
  return value_.get(false);
}

inline auto alert::value_is_unknown() const noexcept -> bool {
  return !value_.is_present();
}

inline auto alert::get_message() const noexcept -> const std::string& {
  return message_;
}

inline auto alert::get_state() const noexcept -> alert_state {
  return state_;
}

inline auto alert::get_since() const noexcept
->  const time_series_value::time_point& {
  return since_;
}

inline auto alert::get_duration() const noexcept
->  const time_series_value::time_point::duration& {
  return trigger_duration_;
}

inline auto alert::get_attributes() const noexcept -> const attributes_map& {
  return attributes_;
}


} /* namespace monsoon */


namespace std {


inline auto hash<monsoon::alert>::operator()(const monsoon::alert& a)
    const noexcept
->  size_t {
  return hash<monsoon::group_name>()(a.get_name());
}


} /* namespace std */

#endif /* MONSOON_ALERT_INL_H */
