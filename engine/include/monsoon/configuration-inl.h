#ifndef MONSOON_CONFIGURATION_INL_H
#define MONSOON_CONFIGURATION_INL_H

#include <utility>

namespace monsoon {


inline configuration::configuration(configuration&& other) noexcept
: collectors_(std::move(other.collectors_)),
  rules_(std::move(other.rules_))
{}

inline auto configuration::operator=(configuration&& other) noexcept
-> configuration& {
  collectors_ = std::move(other.collectors_);
  rules_ = std::move(other.rules_);
  return *this;
}

inline auto configuration::collectors() const noexcept
->  const std::vector<std::unique_ptr<collector>>& {
  return collectors_;
}

inline auto configuration::rules() const noexcept
->  const std::vector<std::unique_ptr<rule>>& {
  return rules_;
}


} /* namespace monsoon */

#endif /* MONSOON_CONFIGURATION_INL_H */
