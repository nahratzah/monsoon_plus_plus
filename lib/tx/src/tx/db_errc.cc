#include <monsoon/tx/db_errc.h>
#include <string>

namespace monsoon::tx {
namespace {


class db_error_category_impl
: public std::error_category
{
  public:
  auto name() const noexcept -> const char* override;
  auto message(int ev) const -> std::string override;
};

auto db_error_category_impl::name() const noexcept -> const char* {
  return "monsoon_tx_db";
}

auto db_error_category_impl::message(int ev) const -> std::string {
  switch (static_cast<db_errc>(ev)) {
    default:
      return "(unrecognized error)";
    case db_errc::gone_away:
      return "database has shut down";
    case db_errc::deleted_required_object_in_tx:
      return "required object was deleted in the same transaction";
    case db_errc::deleted_required_object:
      return "required object was deleted in another transaction";
    case db_errc::double_delete:
      return "deleted object was deleted in another transaction";
  }
}


} /* namespace monsoon::tx::<unnamed> */


auto db_error_category() -> const std::error_category& {
  static const db_error_category_impl impl;
  return impl;
}

auto make_error_code(db_errc e) -> std::error_code {
  return { static_cast<int>(e), db_error_category() };
}


} /* namespace monsoon::tx */
