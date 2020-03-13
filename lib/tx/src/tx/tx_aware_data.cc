#include <monsoon/tx/tx_aware_data.h>
#include <cassert>
#include <boost/endian/conversion.hpp>

namespace monsoon::tx {
namespace {


///\brief On-disk representation.
struct tx_aware_data_record {
  detail::commit_manager::type creation;
  std::uint8_t creation_present;
  std::uint8_t creation_always;
  std::uint8_t deletion_always;
  std::uint8_t deletion_present;
  detail::commit_manager::type deletion;

  void native_to_big_inplace() noexcept {
    boost::endian::native_to_big_inplace(creation);
    boost::endian::native_to_big_inplace(creation_present);
    boost::endian::native_to_big_inplace(creation_always);
    boost::endian::native_to_big_inplace(deletion_always);
    boost::endian::native_to_big_inplace(deletion_present);
    boost::endian::native_to_big_inplace(deletion);
  }

  void big_to_native_inplace() noexcept {
    boost::endian::big_to_native_inplace(creation);
    boost::endian::big_to_native_inplace(creation_present);
    boost::endian::big_to_native_inplace(creation_always);
    boost::endian::big_to_native_inplace(deletion_always);
    boost::endian::big_to_native_inplace(deletion_present);
    boost::endian::big_to_native_inplace(deletion);
  }
};

static_assert(sizeof(tx_aware_data_record) == tx_aware_data::TX_AWARE_SIZE);


} /* namespace monsoon::tx::<unnamed> */


const std::array<std::uint8_t, tx_aware_data::always_flag_size> tx_aware_data::always_buffer{{ 1u }};

void tx_aware_data::encode_tx_aware(boost::asio::mutable_buffer buf) const {
  assert(buf.size() >= TX_AWARE_SIZE);

  std::shared_lock<std::shared_mutex> lck{ mtx_ };

  tx_aware_data_record r;
  r.creation         = creation_;
  r.deletion         = deletion_;
  r.creation_present = creation_present_;
  r.creation_always  = creation_always_;
  r.deletion_present = deletion_present_;
  r.deletion_always  = deletion_always_;
  r.native_to_big_inplace();

  boost::asio::buffer_copy(buf, boost::asio::buffer(&r, sizeof(r)));
}

void tx_aware_data::decode_tx_aware(boost::asio::const_buffer buf) {
  assert(buf.size() >= TX_AWARE_SIZE);

  std::lock_guard<std::shared_mutex> lck{ mtx_ };

  tx_aware_data_record r;
  boost::asio::buffer_copy(boost::asio::buffer(&r, sizeof(r)), buf);
  r.big_to_native_inplace();

  creation_         = r.creation;
  creation_present_ = r.creation_present;
  creation_always_  = r.creation_always;
  deletion_         = r.deletion;
  deletion_present_ = r.deletion_present;
  deletion_always_  = r.deletion_always;
}

auto tx_aware_data::visible_in_tx(const detail::commit_manager::commit_id& tx_id) const noexcept -> bool {
  std::shared_lock<std::shared_mutex> lck{ mtx_ };

  // Deleted for all transactions.
  if (deletion_always_) return false;
  // Deleted at/before this transaction.
  if (deletion_present_
      && deletion_ - tx_id.tx_start() <= tx_id.relative_val())
    return false;

  if (!creation_always_) {
    // Never created.
    if (!creation_present_) return false;
    // Created after this transaction.
    if (creation_ - tx_id.tx_start() > tx_id.relative_val())
      return false;
  }
  // None of invalidation constraints apply.
  return true;
}

auto tx_aware_data::is_never_visible() const noexcept -> bool {
  if (deletion_always_) return true;
  if (!creation_always_ && !creation_present_) return true;
  return false;
}

auto tx_aware_data::make_creation_buffer(detail::commit_manager::type id) noexcept -> std::array<std::uint8_t, presence_size> {
  std::array<std::uint8_t, presence_size> result;

  tx_aware_data_record r;
  r.creation         = id;
  r.creation_present = true;
  r.creation_always  = false;
  r.native_to_big_inplace();

  boost::asio::buffer_copy(
      boost::asio::buffer(result),
      boost::asio::buffer(&r, sizeof(r)) + CREATION_OFFSET,
      presence_size);
  return result;
}

auto tx_aware_data::make_deletion_buffer(detail::commit_manager::type id) noexcept -> std::array<std::uint8_t, presence_size> {
  std::array<std::uint8_t, presence_size> result;

  tx_aware_data_record r;
  r.deletion         = id;
  r.deletion_present = true;
  r.deletion_always  = false;
  r.native_to_big_inplace();

  boost::asio::buffer_copy(
      boost::asio::buffer(result),
      boost::asio::buffer(&r, sizeof(r)) + DELETION_OFFSET,
      presence_size);
  return result;
}


} /* namespace monsoon::tx */
