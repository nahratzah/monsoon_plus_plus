#ifndef MONSOON_TX_TREE_ERROR_H
#define MONSOON_TX_TREE_ERROR_H

#include <monsoon/tx/detail/export_.h>
#include <stdexcept>

namespace monsoon::tx {


class monsoon_tx_export_ tree_error
: public std::runtime_error {
  public:
  using std::runtime_error::runtime_error;
  ~tree_error() noexcept override;
};


} /* namespace monsoon::tx */

#endif /* MONSOON_TX_TREE_ERROR_H */
