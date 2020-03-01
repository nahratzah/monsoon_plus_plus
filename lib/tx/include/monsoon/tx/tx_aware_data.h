#ifndef MONSOON_TX_TX_AWARE_DATA_H
#define MONSOON_TX_TX_AWARE_DATA_H

#include <monsoon/tx/detail/export_.h>
#include <monsoon/tx/detail/commit_id.h>
#include <monsoon/tx/db.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
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
  friend db::transaction;

  private:
  static constexpr std::size_t always_flag_size = 1u;
  static constexpr std::size_t presence_size = sizeof(detail::commit_manager::type) + 1u;

  public:
  static constexpr std::size_t CREATION_OFFSET = 0;
  static constexpr std::size_t CREATION_ALWAYS_OFFSET = CREATION_OFFSET + presence_size;
  static constexpr std::size_t DELETION_ALWAYS_OFFSET = CREATION_ALWAYS_OFFSET + always_flag_size;
  static constexpr std::size_t DELETION_OFFSET = DELETION_ALWAYS_OFFSET + always_flag_size;
  static constexpr std::size_t TX_AWARE_SIZE = DELETION_OFFSET + presence_size;

  protected:
  tx_aware_data() noexcept;
  ~tx_aware_data() noexcept = default;

  void encode_tx_aware(boost::asio::mutable_buffer buf) const;
  void decode_tx_aware(boost::asio::const_buffer buf);

  public:
  ///\brief Test if the datum is visible in this commit ID.
  auto visible_in_tx(const detail::commit_manager::commit_id& tx_id) const noexcept -> bool;

  private:
  ///\brief Find the offset of this datum.
  ///\note May only be called with the relevant layout lock held.
  virtual auto offset() const -> std::uint64_t = 0;

  void set_created(detail::commit_manager::type id);
  void set_created_always();
  void set_deleted(detail::commit_manager::type id);
  void set_deleted_always();

  static auto make_creation_buffer(detail::commit_manager::type id) noexcept -> std::array<std::uint8_t, presence_size>;
  static auto make_deletion_buffer(detail::commit_manager::type id) noexcept -> std::array<std::uint8_t, presence_size>;
  static const std::array<std::uint8_t, always_flag_size> always_buffer;

  protected:
  mutable std::shared_mutex mtx_;

  private:
  detail::commit_manager::type creation_ = 0;
  detail::commit_manager::type deletion_ = 0;
  bool creation_present_ : 1;
  bool creation_always_  : 1;
  bool deletion_present_ : 1;
  bool deletion_always_  : 1;
};


inline tx_aware_data::tx_aware_data() noexcept
: creation_present_(false),
  creation_always_(false),
  deletion_present_(false),
  deletion_always_(false)
{}

inline void tx_aware_data::set_created(detail::commit_manager::type id) {
  std::lock_guard<std::shared_mutex> lck{ mtx_ };
  creation_ = id;
  creation_present_ = true;
  creation_always_ = false;
}

inline void tx_aware_data::set_created_always() {
  std::lock_guard<std::shared_mutex> lck{ mtx_ };
  creation_always_ = true;
}

inline void tx_aware_data::set_deleted(detail::commit_manager::type id) {
  std::lock_guard<std::shared_mutex> lck{ mtx_ };
  deletion_ = id;
  deletion_present_ = true;
  deletion_always_ = false;
}

inline void tx_aware_data::set_deleted_always() {
  std::lock_guard<std::shared_mutex> lck{ mtx_ };
  deletion_always_ = true;
}


} /* namespace monsoon::tx */

#endif /* MONSOON_TX_TX_AWARE_DATA_H */
