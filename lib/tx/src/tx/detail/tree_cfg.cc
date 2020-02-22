#include <monsoon/tx/detail/tree_cfg.h>
#include <cassert>
#include <boost/endian/conversion.hpp>

namespace monsoon::tx::detail {


void tree_cfg::encode(boost::asio::mutable_buffer buf) const {
  assert(buf.size() >= SIZE);

  tree_cfg tmp = *this;
  boost::endian::native_to_big_inplace(tmp.items_per_leaf_page);
  boost::endian::native_to_big_inplace(tmp.items_per_node_page);
  boost::endian::native_to_big_inplace(tmp.key_bytes);
  boost::endian::native_to_big_inplace(tmp.val_bytes);
  boost::endian::native_to_big_inplace(tmp.augment_bytes);
  boost::asio::buffer_copy(buf, boost::asio::const_buffer(&tmp, sizeof(tmp)));
}

void tree_cfg::decode(boost::asio::const_buffer buf) {
  assert(buf.size() >= SIZE);

  boost::asio::buffer_copy(boost::asio::buffer(this, sizeof(*this)), buf);
  boost::endian::big_to_native_inplace(items_per_leaf_page);
  boost::endian::big_to_native_inplace(items_per_node_page);
  boost::endian::big_to_native_inplace(key_bytes);
  boost::endian::big_to_native_inplace(val_bytes);
  boost::endian::big_to_native_inplace(augment_bytes);
}


} /* namespace monsoon::tx::detail */
