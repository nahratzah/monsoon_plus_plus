#ifndef MONSOON_TX_TX_AWARE_DATA_H
#define MONSOON_TX_TX_AWARE_DATA_H

#include <monsoon/tx/detail/export_.h>
#include <monsoon/tx/detail/commit_id.h>
#include <monsoon/tx/sequence.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <boost/asio/buffer.hpp>

namespace monsoon::tx {


/**
 * \brief Transaction aware data.
 * \details
 * Represents (the aspect of) data that is aware of transactions.
 *
 * Such data won't ever be modified, but instead will be a write-once
 * data element.
 *
 * Creation and deletion are controlled using transaction indices.
 * (Those indices are mutable on the data.)
 */
class monsoon_tx_export_ tx_aware_data {
  private:
  using flag_type = std::uint32_t;

  public:
  static constexpr std::size_t TX_AWARE_CREATION_OFFSET = 0;
  static constexpr std::size_t TX_AWARE_DELETION_OFFSET = TX_AWARE_CREATION_OFFSET + sizeof(sequence::type);
  static constexpr std::size_t TX_AWARE_FLAGS_OFFSET = TX_AWARE_DELETION_OFFSET + sizeof(sequence::type);
  static constexpr std::size_t TX_AWARE_SIZE = TX_AWARE_FLAGS_OFFSET + sizeof(flag_type);

  private:
  ///\brief Indicates the creation commit ID holds a value.
  static constexpr flag_type creation_present = 0x1u;
  ///\brief Indicates the creation is visible to all current transactions.
  static constexpr flag_type creation_always  = 0x2u;
  ///\brief Indicates the deletion commit ID holds a value.
  static constexpr flag_type deletion_present = 0x4u;
  ///\brief Indicates the deletion is visible to all current transactions.
  static constexpr flag_type deletion_always  = 0x8u;

  protected:
  tx_aware_data() = default;
  ~tx_aware_data() noexcept = default;

  void encode_tx_aware(boost::asio::mutable_buffer buf) const;
  void decode_tx_aware(boost::asio::const_buffer buf);

  public:
  auto visible_in_tx(const monsoon::tx::detail::commit_manager::commit_id& tx_id) const noexcept -> bool;

  protected:
  mutable std::shared_mutex mtx_;

  private:
  sequence::type creation_ = 0;
  sequence::type deletion_ = 0;
  flag_type flags_ = 0u;
};


} /* namespace monsoon::tx */

#endif /* MONSOON_TX_TX_AWARE_DATA_H */
