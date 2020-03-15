#ifndef MONSOON_TX_DETAIL_TXFILE_ALLOCATOR_INL_H
#define MONSOON_TX_DETAIL_TXFILE_ALLOCATOR_INL_H

namespace monsoon::tx::detail {


inline auto txfile_allocator::key::operator==(const key& y) const noexcept -> bool {
  return addr == y.addr;
}

inline auto txfile_allocator::key::operator!=(const key& y) const noexcept -> bool {
  return !(*this == y);
}

inline auto txfile_allocator::key::operator<(const key& y) const noexcept -> bool {
  return addr < y.addr;
}

inline auto txfile_allocator::key::operator>(const key& y) const noexcept -> bool {
  return y < *this;
}

inline auto txfile_allocator::key::operator<=(const key& y) const noexcept -> bool {
  return !(*this > y);
}

inline auto txfile_allocator::key::operator>=(const key& y) const noexcept -> bool {
  return !(*this < y);
}


inline auto txfile_allocator::max_free_space_augment::merge(
    const max_free_space_augment& x, const max_free_space_augment& y) noexcept
-> const max_free_space_augment& {
  return (x.free > y.free ? x : y);
}


} /* namespace monsoon::tx::detail */

#endif /* MONSOON_TX_DETAIL_TXFILE_ALLOCATOR_INL_H */
