#include <monsoon/tx/tx_aware_data.h>
#include <cassert>
#include <boost/endian/conversion.hpp>

namespace monsoon::tx {


void tx_aware_data::encode_tx_aware(boost::asio::mutable_buffer buf) const {
  assert(buf.size() >= TX_AWARE_SIZE);

  sequence::type c = creation_, d = deletion_;
  flag_type f = flags_;

  boost::endian::native_to_big_inplace(c);
  boost::endian::native_to_big_inplace(d);
  boost::endian::native_to_big_inplace(f);

  boost::asio::buffer_copy(
      buf,
      std::array<boost::asio::const_buffer, 3>({
            boost::asio::buffer(&c, sizeof(c)),
            boost::asio::buffer(&d, sizeof(d)),
            boost::asio::buffer(&f, sizeof(f))
          }));
}

void tx_aware_data::decode_tx_aware(boost::asio::const_buffer buf) {
  assert(buf.size() >= TX_AWARE_SIZE);

  boost::asio::buffer_copy(
      std::array<boost::asio::mutable_buffer, 3>({
            boost::asio::buffer(&creation_, sizeof(creation_)),
            boost::asio::buffer(&deletion_, sizeof(deletion_)),
            boost::asio::buffer(&flags_, sizeof(flags_))
          }),
      buf);

  boost::endian::big_to_native_inplace(creation_);
  boost::endian::big_to_native_inplace(deletion_);
  boost::endian::big_to_native_inplace(flags_);
}

auto tx_aware_data::visible_in_tx(const monsoon::tx::detail::commit_manager::commit_id& tx_id) const noexcept -> bool {
  // Deleted for all transactions.
  if (flags_ & deletion_always) return false;
  // Deleted at/before this transaction.
  if ((flags_ & deletion_present)
      && deletion_ - tx_id.tx_start() <= tx_id.relative_val())
    return false;

  if (!(flags_ & creation_always)) {
    // Never created.
    if (!(flags_ & creation_present)) return false;
    // Created after this transaction.
    if (creation_ - tx_id.tx_start() > tx_id.relative_val())
      return false;
  }
  // None of invalidation constraints apply.
  return true;
}


} /* namespace monsoon::tx */
