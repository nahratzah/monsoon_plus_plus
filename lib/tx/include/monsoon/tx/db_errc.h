#ifndef MONSOON_TX_DB_ERRC_H
#define MONSOON_TX_DB_ERRC_H

#include <monsoon/tx/detail/export_.h>
#include <system_error>

namespace monsoon::tx {


enum class db_errc {
  ///\brief Indicates the database (or part of it) has been destroyed.
  gone_away = 10,
  ///\brief Indicates the transaction requires an object to be present, but it was deleted in this same transaction.
  deleted_required_object_in_tx = 20,
  ///\brief Indicates another transaction deleted a required object.
  deleted_required_object,
  ///\brief Indicates an object that was deleted in the current transaction, has already been deleted in another transaction.
  double_delete,
};

monsoon_tx_export_ auto make_error_code(db_errc e) -> std::error_code;
monsoon_tx_export_ auto db_error_category() -> const std::error_category&;


} /* namespace monsoon::tx */

namespace std {
template<> struct is_error_code_enum<monsoon::tx::db_errc> : true_type {};
} /* namespace std */


#endif /* MONSOON_TX_DB_ERRC_H */
