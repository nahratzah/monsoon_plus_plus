#include <monsoon/any.h>

namespace monsoon {


any_error::~any_error() noexcept {}

auto any_error::__throw() -> void {
  __throw(std::string("ilias::any: no value"));
}

auto any_error::__throw(const std::string& msg) -> void {
  throw any_error(msg);
}

auto any_error::__throw(const char* msg) -> void {
  throw any_error(msg);
}


} /* namespace monsoon */
