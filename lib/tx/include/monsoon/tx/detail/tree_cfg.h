#ifndef MONSOON_TX_DETAIL_TREE_CFG_H
#define MONSOON_TX_DETAIL_TREE_CFG_H

#include <cstdint>
#include <boost/asio/buffer.hpp>

namespace monsoon::tx::detail {


struct tree_cfg {
  static constexpr std::size_t SIZE = 20u;

  std::uint32_t items_per_leaf_page, items_per_node_page;
  std::uint32_t key_bytes, val_bytes, augment_bytes;

  void encode(boost::asio::mutable_buffer buf) const;
  void decode(boost::asio::const_buffer buf);
};


} /* namespace monsoon::tx::detail */

#endif /* MONSOON_TX_DETAIL_TREE_CFG_H */
