#include <monsoon/time_point.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

namespace monsoon {

using namespace boost::posix_time;
using namespace boost::gregorian;


time_point time_point::now() {
  const ptime unix_epoch = ptime(date(1970, 1, 1), time_duration(0, 0, 0));
  const auto time_since_unix_epoch =
      second_clock::universal_time() - unix_epoch;
  return time_point(time_since_unix_epoch.total_milliseconds());
}


} /* namespace monsoon */
