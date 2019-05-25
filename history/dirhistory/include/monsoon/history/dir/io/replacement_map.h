#ifndef MONSOON_HISTORY_DIR_IO_REPLACEMENT_MAP_H
#define MONSOON_HISTORY_DIR_IO_REPLACEMENT_MAP_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <cstdint>
#include <iterator>
#include <cstring>
#include <algorithm>
#include <vector>
#include <boost/intrusive/set.hpp>
#include <boost/intrusive/options.hpp>
#include <monsoon/io/fd.h>

namespace monsoon::history::io {


/**
 * \brief Record file changes.
 * \details
 * A replacement map holds on to file changes in memory and allows for them to
 * be applied during reads.
 */
class monsoon_dirhistory_export_ replacement_map {
  private:
  using vector_type = std::vector<std::uint8_t>;

  struct entry_type
  : public boost::intrusive::set_base_hook<>
  {
    entry_type(monsoon::io::fd::offset_type first, vector_type second) noexcept
    : first(std::move(first)),
      second(std::move(second))
    {}

    auto end_offset() const noexcept -> monsoon::io::fd::offset_type {
      return first + second.size();
    }

    const monsoon::io::fd::offset_type first;
    const vector_type second;
  };

  struct entry_key_extractor_ {
    using type = monsoon::io::fd::offset_type;

    auto operator()(const entry_type& e) const noexcept -> const type& {
      return e.first;
    }
  };

  using map_type = boost::intrusive::set<
      entry_type,
      boost::intrusive::key_of_value<entry_key_extractor_>,
      boost::intrusive::constant_time_size<false>>;

  public:
  class tx;

  ~replacement_map() noexcept;

  /**
   * \brief Read data from the replacement map if applicable.
   * \details
   * Data in the replacement map is sparse.
   * Thus not all reads may succeed.
   * When a read doesn't succeed, we'll indicate that by returning a zero-bytes-read return value.
   *
   * \param[in] off The offset at which the read happens.
   * \param[out] buf Buffer into which to read data.
   * \param[in,out] nbytes Number of bytes to read. If the read happens inside a gap of the replacement map, the value will be clamped to not exceed that gap.
   * \return The number of bytes read from the replacement map, or 0 if no bytes could be read.
   */
  auto read_at(monsoon::io::fd::offset_type off, void* buf, std::size_t& nbytes) const -> std::size_t;
  /**
   * \brief Write data into the replacement map.
   * \details
   * Prepares a transaction that writes \p nbytes from \p buf into the replacement map.
   *
   * If the map already holds data at the given position, it will be replaced.
   *
   * The returned transaction is applied only if the commit method is invoked.
   * The replacement_map only allows for a single transaction at a time.
   * \throw std::overflow_error if \p buf + \p nbytes exceed the range of an offset.
   * \throw std::bad_alloc if insufficient memory is available to complete the operation.
   */
  auto write_at(monsoon::io::fd::offset_type off, const void* buf, std::size_t nbytes) -> tx;

  private:
  map_type map_;
};


///\brief Transactional write operation.
///\details Holds the current write operation in such a way that it can be either committed or rolled back.
///Only one transaction can be active at any moment.
class monsoon_dirhistory_export_ replacement_map::tx {
  friend replacement_map;

  public:
  tx(const tx&) = delete;
  tx& operator=(const tx&) = delete;

  tx() noexcept = default;

  tx(tx&& x) noexcept
  : map_(std::exchange(x.map_, nullptr)),
    to_erase_(std::move(x.to_erase_)),
    to_insert_(std::move(x.to_insert_))
  {}

  tx& operator=(tx&& x) noexcept {
    map_ = std::exchange(x.map_, nullptr);
    to_erase_ = std::move(x.to_erase_);
    to_insert_ = std::move(x.to_insert_);
    return *this;
  }

  ~tx() noexcept;

  void commit() noexcept;

  private:
  map_type* map_ = nullptr; // No ownership.
  std::vector<map_type::iterator> to_erase_;
  std::vector<std::unique_ptr<entry_type>> to_insert_;
};


} /* namespace monsoon::history::io */

#endif /* MONSOON_HISTORY_DIR_IO_REPLACEMENT_MAP_H */
