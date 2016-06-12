#include <monsoon/optional.h>

namespace monsoon {


optional_error::~optional_error() noexcept {}

auto optional_error::__throw() -> void {
  __throw(std::string("ilias::optional: no value"));
}

auto optional_error::__throw(const std::string& msg) -> void {
  throw optional_error(msg);
}

auto optional_error::__throw(const char* msg) -> void {
  throw optional_error(msg);
}


} /* namespace monsoon */
