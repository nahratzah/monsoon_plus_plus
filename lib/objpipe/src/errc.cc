#include <monsoon/objpipe/errc.h>
#include <ostream>

namespace monsoon {
namespace objpipe {


class monsoon_objpipe_local_ objpipe_category_t
: public std::error_category
{
 public:
  const char* name() const noexcept override;
  std::error_condition default_error_condition(int) const noexcept override;
  bool equivalent(const std::error_code&, int) const noexcept override;
  std::string message(int) const override;
};


const char* objpipe_category_t::name() const noexcept {
  return "monsoon::objpipe";
}

std::error_condition objpipe_category_t::default_error_condition(int e)
    const noexcept {
  return std::error_condition(objpipe_errc(e));
}

bool objpipe_category_t::equivalent(const std::error_code& ec, int e)
    const noexcept {
  return &ec.category() == this && ec.value() == e;
}

std::string objpipe_category_t::message(int e) const {
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

const std::error_category& objpipe_category() {
  static const objpipe_category_t cat;
  return cat;
}

std::error_condition make_error_condition(objpipe_errc e) {
  return std::error_condition(static_cast<int>(e), objpipe_category());
}

std::ostream& operator<<(std::ostream& out, objpipe_errc e) {
  using namespace std::string_view_literals;

  std::string_view e_txt;
  switch (e) {
    case objpipe_errc::success:
      e_txt = "objpipe_errc[success]"sv;
    case objpipe_errc::closed:
      e_txt = "objpipe_errc[closed]"sv;
  }
  return out << e_txt;
}


}} /* namespace monsoon::objpipe */
