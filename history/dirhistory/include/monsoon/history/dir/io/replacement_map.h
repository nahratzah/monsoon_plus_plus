#ifndef MONSOON_HISTORY_DIR_IO_REPLACEMENT_MAP_H
#define MONSOON_HISTORY_DIR_IO_REPLACEMENT_MAP_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <memory>
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
  public:
  class entry_type
  : public boost::intrusive::set_base_hook<boost::intrusive::optimize_size<true>>
  {
    public:
    using size_type = std::size_t;

    entry_type() = default;

#if __cpp_lib_shared_ptr_arrays >= 201611
    public:
    entry_type(monsoon::io::fd::offset_type first, std::unique_ptr<std::uint8_t[]>&& data, size_type size)
    : first(first),
      data_(std::move(data)),
      size_(size)
    {}
#else
    public:
    entry_type(monsoon::io::fd::offset_type first, std::unique_ptr<std::uint8_t[]>&& data, size_type size)
    : first(first),
      data_(data.get(), data.get_deleter()),
      size_(size)
    {
      data.release();
    }
#endif

    public:
    auto begin_offset() const noexcept -> monsoon::io::fd::offset_type {
      return first;
    }

    auto end_offset() const noexcept -> monsoon::io::fd::offset_type {
      return first + size();
    }

    auto size() const noexcept -> size_type {
      return size_;
    }

    auto data() const noexcept -> const void* {
      return data_.get();
    }

    ///\brief Remove bytes from the front of the entry.
    ///\param[in] n Number of bytes to remove.
    ///\return This entry.
    ///\throw std::overflow_error if \p n is larger than the size of this entry.
    auto pop_front(size_type n = 1) -> entry_type&;

    ///\brief Remove bytes from the rear of the entry.
    ///\param[in] n Number of bytes to remove.
    ///\return This entry.
    ///\throw std::overflow_error if \p n is larger than the size of this entry.
    auto pop_back(size_type n = 1) -> entry_type&;

    ///\brief Remove bytes from the rear of the entry, until the entry is exactly \p n bytes.
    ///\param[in] n Number of bytes to keep.
    ///\return This entry.
    ///\throw std::overflow_error if \p n is larger than the size of this entry.
    auto keep_front(size_type n) -> entry_type&;

    ///\brief Remove bytes from the front of the entry, until the entry is exactly \p n bytes.
    ///\param[in] n Number of bytes to keep.
    ///\return This entry.
    ///\throw std::overflow_error if \p n is larger than the size of this entry.
    auto keep_back(size_type n) -> entry_type&;

    ///\brief Test if this entry is empty.
    ///\return True if this entry holds zero bytes. False otherwise.
    auto empty() const noexcept -> bool {
      return size() == 0u;
    }

    private:
    monsoon::io::fd::offset_type first = 0;
#if __cpp_lib_shared_ptr_arrays >= 201611
    std::shared_ptr<const std::uint8_t[]> data_;
#else
    std::shared_ptr<const std::uint8_t> data_;
#endif
    size_type size_ = 0;
  };

  private:
  struct entry_key_extractor_ {
    using type = monsoon::io::fd::offset_type;

    auto operator()(const entry_type& e) const noexcept -> type {
      return e.begin_offset();
    }
  };

  using map_type = boost::intrusive::set<
      entry_type,
      boost::intrusive::key_of_value<entry_key_extractor_>,
      boost::intrusive::constant_time_size<false>>;

  public:
  using const_iterator = map_type::const_iterator;
  using iterator = const_iterator;
  class tx;

  ~replacement_map() noexcept;

  auto begin() const -> const_iterator {
    return map_.begin();
  }

  auto cbegin() const -> const_iterator {
    return begin();
  }

  auto end() const -> const_iterator {
    return map_.end();
  }

  auto cend() const -> const_iterator {
    return end();
  }

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
   * The returned transaction is applied only if the commit method is invoked.
   *
   * The replacement_map only allows for a single transaction at a time.
   * The exception begin that multiple transaction are allowed if all transactions
   * are non-replacing and do not overlap.
   * \param[in] off The offset at which the write takes place.
   * \param[in] buf The buffer holding the bytes to be written.
   * \param[in] nbytes Number of bytes that is to be written.
   * \param[in] overwrite If set, allow this write to overwrite previous writes on this replacement_map.
   * \throw std::overflow_error if \p buf + \p nbytes exceed the range of an offset.
   * \throw std::bad_alloc if insufficient memory is available to complete the operation.
   */
  auto write_at(monsoon::io::fd::offset_type off, const void* buf, std::size_t nbytes, bool overwrite = true) -> tx;

  /**
   * \brief Write data into the replacement map, from a file.
   * \details
   * Prepares a transaction that writes \p nbytes into the replacement map.
   * The bytes written are sourced from \p fd at offset \p fd_off.
   *
   * The returned transaction is applied only if the commit method is invoked.
   *
   * The replacement_map only allows for a single transaction at a time.
   * The exception begin that multiple transaction are allowed if all transactions
   * are non-replacing and do not overlap.
   * \param[in] off The offset at which the write takes place.
   * \param[in] fd The file holding the bytes to be written.
   * \param[in] fd_off The position in the file at which the bytes to be written reside.
   * \param[in] nbytes Number of bytes that is to be written.
   * \param[in] overwrite If set, allow this write to overwrite previous writes on this replacement_map.
   * \throw std::overflow_error if \p buf + \p nbytes exceed the range of an offset.
   * \throw std::bad_alloc if insufficient memory is available to complete the operation.
   */
  auto write_at_from_file(monsoon::io::fd::offset_type off, const monsoon::io::fd& fd, monsoon::io::fd::offset_type fd_off, std::size_t nbytes, bool overwrite = true) -> tx;

  private:
  auto write_at_with_overwrite_(monsoon::io::fd::offset_type off, const void* buf, std::size_t nbytes) -> tx;
  auto write_at_without_overwrite_(monsoon::io::fd::offset_type off, const void* buf, std::size_t nbytes) -> tx;

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
