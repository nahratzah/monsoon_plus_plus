#ifndef MONSOON_HASH_SUPPORT_H
#define MONSOON_HASH_SUPPORT_H

///\file

#include <type_traits>

namespace monsoon {


/**
 * \brief Compute a hash over a map.
 *
 * The hash function will not change if the map is reordered.
 *
 * \tparam An iterator over std::pair.
 * \param[in] b,e An iteration over std::pair.
 */
template<typename Iter>
size_t map_to_hash(Iter b, Iter e) noexcept {
  size_t cumulative = 0;

  while (b != e) {
    const auto& first = b->first;
    const auto& second = b->second;
    using first_type =
        std::remove_const_t<std::remove_reference_t<decltype(first)>>;
    using second_type =
        std::remove_const_t<std::remove_reference_t<decltype(second)>>;
    const size_t h = 23u * std::hash<first_type>()(first) +
                     std::hash<second_type>()(second);
    cumulative ^= h;

    ++b;
  }

  return cumulative;
}

/**
 * \brief Compute a hash over a map.
 *
 * The hash function will not change if the map is reordered.
 *
 * \tparam Map The map type.
 * \param m The map for which to compute a hash value.
 */
template<typename Map>
size_t map_to_hash(const Map& m) noexcept {
  return map_to_hash(m.begin(), m.end());
}


} /* namespace monsoon */

#endif /* MONSOON_HASH_SUPPORT_H */
