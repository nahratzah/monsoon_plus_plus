#include <monsoon/objpipe/errc.h>

namespace monsoon {
namespace objpipe {


const char* objpipe_category::name() const {
  return "monsoon::objpipe";
}

std::error_condition objpipe_category::default_error_condition(int e) const {
  return std::error_condition(objpipe_errc(e));
}

bool objpipe_category::equivalent(const std::error_code& ec, int e) const {
  return ec.category() == this && ec.value() == e;
}

std::string objpipe_category::message(int e) const {
  using std::to_string;

  switch (objpipe_errc(e)) {
    default:
      return "objpipe unknown error " + to_string(e);
    case objpipe_errc::success:
      return "success";
    case objpipe_errc::closed:
      return "objpipe closed";
  }
}

const objpipe_category_t objpipe_category;

std::error_condition make_error_condition(objpipe_errc e) {
  return std::error_condition(static_cast<int>(e), objpipe_category);
}


}} /* namespace monsoon::objpipe */
