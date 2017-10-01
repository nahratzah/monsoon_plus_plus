#ifndef MONSOON_ALERT_H
#define MONSOON_ALERT_H

#include <monsoon/intf_export_.h>
#include <monsoon/group_name.h>
#include <monsoon/metric_value.h>
#include <monsoon/time_point.h>
#include <functional>
#include <string>
#include <optional>
#include <variant>
#include <unordered_map>

namespace monsoon {


enum class alert_state : unsigned char {
  OK,
  TRIGGERING,
  FIRING,
  UNKNOWN
};

class monsoon_intf_export_ alert {
 public:
  using attributes_map =
      std::unordered_map<std::string,
                         std::variant<metric_value, std::vector<metric_value>>>;

  alert();
  alert(const alert&);
  alert(alert&&) noexcept;
  alert& operator=(const alert&);
  alert& operator=(alert&&) noexcept;
  ~alert() noexcept;

  const group_name& get_name() const noexcept;
  bool value_is_ok() const noexcept;
  bool value_is_triggering() const noexcept;
  bool value_is_unknown() const noexcept;
  const std::string& get_message() const noexcept;
  alert_state get_state() const noexcept;
  const time_point& get_since() const noexcept;
  const time_point::duration& get_duration() const noexcept;
  const attributes_map& get_attributes() const noexcept;

  alert& extend_with(alert&&) noexcept;

 private:
  group_name name_;
  std::optional<bool> value_;
  std::string message_;
  alert_state state_;
  time_point since_;
  time_point::duration trigger_duration_;
  attributes_map attributes_;
};


} /* namespace monsoon */


namespace std {


template<>
struct hash<monsoon::alert> {
  using argument_type = const monsoon::alert&;
  using result_type = size_t;

  monsoon_intf_export_
  size_t operator()(const monsoon::alert&) const noexcept;
};


} /* namespace std */

#include "alert-inl.h"

#endif /* MONSOON_ALERT_H */
