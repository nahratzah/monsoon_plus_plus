#ifndef MONSOON_TX_DETAIL_LAYOUT_DOMAIN_H
#define MONSOON_TX_DETAIL_LAYOUT_DOMAIN_H

#include <monsoon/tx/detail/export_.h>
#include <shared_mutex>

namespace monsoon::tx {
class db;
} /* namespace monsoon::tx */
namespace monsoon::tx::detail {


class layout_domain;

class monsoon_tx_export_ layout_obj {
  friend monsoon::tx::db;

  public:
  virtual ~layout_obj() noexcept = 0;

  protected:
  virtual auto get_layout_domain() const noexcept -> const layout_domain& = 0;

  ///\brief Mutex that controls read/write access to the layout.
  ///\details While held, offsets of objects may not be changed.
  ///\note
  ///It's totally fine for the value of data to change,
  ///and for data to be inserted or marked deleted.
  ///Basically anything is allowed, as long as it does not change the offsets
  ///of existing data.
  mutable std::shared_mutex layout_mtx;
};

class monsoon_tx_export_ layout_domain {
  protected:
  constexpr layout_domain() noexcept = default;
  virtual ~layout_domain() noexcept;

  public:
  virtual auto less_compare(const layout_obj& x, const layout_obj& y) const -> bool = 0;
};


} /* namespace monsoon::tx::detail */

#endif /* MONSOON_TX_DETAIL_LAYOUT_DOMAIN_H */
