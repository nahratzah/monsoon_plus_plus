#include <monsoon/time_point.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <locale>
#include <sstream>
#include <ostream>

namespace monsoon {

using namespace boost::posix_time;
using namespace boost::gregorian;

inline namespace time_point_internals {


monsoon_intf_local_ auto unix_epoch() -> ptime;
monsoon_intf_local_ void imbue_tp_out_format(std::basic_ios<char>& out);
monsoon_intf_local_ void imbue_tp_in_format(std::basic_ios<char>& out);
monsoon_intf_local_ auto parse_as_msec_since_posix_epoch(const std::string&)
    -> std::int64_t;


} /* namespace monsoon::(inline)time_point_internals */


time_point::time_point(const std::string& s)
: time_point(parse_as_msec_since_posix_epoch(s))
{}

time_point time_point::now() {
  const auto time_since_unix_epoch =
      second_clock::universal_time() - unix_epoch();
  return time_point(time_since_unix_epoch.total_milliseconds());
}

std::string to_string(time_point tp) {
  std::ostringstream oss;
  imbue_tp_out_format(oss);
  oss << (unix_epoch() + millisec(tp.millis_since_posix_epoch()));
  return oss.str();
}

auto operator<<(std::ostream& out, time_point tp) -> std::ostream& {
  out << (unix_epoch() + millisec(tp.millis_since_posix_epoch()));
  return out;
}

int time_point::year() const noexcept {
  return (unix_epoch() + millisec(millis_since_posix_epoch())).date().year();
}

int time_point::month() const noexcept {
  return (unix_epoch() + millisec(millis_since_posix_epoch())).date().month();
}

int time_point::day_of_month() const noexcept {
  return (unix_epoch() + millisec(millis_since_posix_epoch())).date().day();
}

int time_point::hour() const noexcept {
  return (unix_epoch() + millisec(millis_since_posix_epoch())).time_of_day().hours();
}

int time_point::minute() const noexcept {
  return (unix_epoch() + millisec(millis_since_posix_epoch())).time_of_day().minutes();
}

int time_point::second() const noexcept {
  return (unix_epoch() + millisec(millis_since_posix_epoch())).time_of_day().seconds();
}


inline namespace time_point_internals {


const auto FORMAT = "%Y-%m-%dT%H:%M:%S%FZ";

void imbue_tp_out_format(std::basic_ios<char>& out) {
  auto fmt = std::make_unique<time_facet>();
  fmt->format(FORMAT);
  out.imbue(std::locale(std::locale::classic(), fmt.release()));
}

void imbue_tp_in_format(std::basic_ios<char>& out) {
  auto fmt = std::make_unique<time_input_facet>();
  fmt->format(FORMAT);
  out.imbue(std::locale(std::locale::classic(), fmt.release()));
}

auto unix_epoch() -> ptime {
  return ptime(date(1970, 1, 1), time_duration(0, 0, 0));
}

auto parse_as_msec_since_posix_epoch(const std::string& s) -> std::int64_t {
  std::istringstream iss = std::istringstream(s);
  imbue_tp_in_format(iss);

  ptime pval;
  iss >> pval;
  return (pval - unix_epoch()).total_milliseconds();
}


} /* namespace monsoon::time_point_internals */
} /* namespace monsoon */
