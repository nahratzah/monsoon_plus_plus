#ifndef MONSOON_TX_DETAIL_TREE_SPEC_H
#define MONSOON_TX_DETAIL_TREE_SPEC_H

#include <cstddef>
#include <type_traits>
#include <boost/asio/buffer.hpp>
#include <boost/integer/common_factor_rt.hpp>

namespace monsoon::tx::detail {


///\brief Requirements that must be met by all tree types.
template<typename T>
struct tree_spec
: std::true_type
{
  static_assert(std::is_convertible_v<decltype(T::SIZE), std::uint32_t>,
      "type must have a numeric constant 'SIZE' indicating the encoded size in bytes");
  static_assert((std::declval<const T&>().encode(std::declval<boost::asio::mutable_buffer&>()), true),
      "must have encode method: void T::encode(boost::asio::mutable_buffer&) const");
  static_assert((std::declval<T&>().decode(std::declval<boost::asio::const_buffer>()), true),
      "must have decode method: void T::decode(boost::asio::const_buffer) const");
  static_assert(std::is_constructible_v<T, boost::asio::const_buffer>,
      "must be constructible from a buffer");
};

///\brief Requirements that must be met by tree keys.
template<typename T>
struct tree_key_spec
: tree_spec<T>
{
  static_assert(std::is_convertible_v<decltype(std::declval<const T&>() == std::declval<const T&>()), bool>,
      "type must be equal-comparable");
};

///\brief Requirements that must be met by tree values.
template<typename T>
struct tree_val_spec
: tree_spec<T>
{};

///\brief Requirements that must be met by tree augmentations.
///\tparam Key the key type of the tree.
///\tparam Val the mapped type of the tree.
///\tparam T the augmentation.
template<typename Key, typename Val, typename T>
struct tree_augment_spec
: tree_spec<T>
{
  static_assert(std::is_constructible_v<T, const Key&, const Val&>,
      "augmentation must be constructible from a key-value pair");
  static_assert(std::is_default_constructible_v<T>,
      "augmentation must be default constructible");
  static_assert(std::is_convertible_v<T, decltype(T::merge(std::declval<const T&>(), std::declval<const T&>()))>,
      "augmentation must implement a reducer function: static T T::merge(const T&, const T&)");
};

/**
 * \brief Prefer 2 megabyte pages for trees.
 * \details
 * IO throughput tends to increase with larger reads/writes, as it reduces
 * the number of IO operations.
 *
 * However a large page also puts pressure on the memory system, in that we
 * need to keep a representative number of objects in ram while the page is
 * active.
 */
constexpr std::size_t tree_max_page_size = 2 * 1024 * 1024;

/**
 * \brief Try to align page objects to not lose (much) space.
 * \details
 * Try to reduce the align pages to not lose much bytes.
 */
constexpr std::size_t tree_prefer_byte_loss_lcm = 4 * 1024;

/**
 * \brief Minimum number of elements per page.
 * \details
 * Pages should autoconf to hold at least this many elements.
 */
constexpr std::size_t tree_min_elems_per_page = 4 * 1024;

///\brief Trivial compile-time summation function.
template<typename Integer>
constexpr auto sum_(const Integer& v) noexcept -> Integer {
  return v;
}

///\brief Trivial compile-time summation function.
template<typename Integer>
constexpr auto sum_(const Integer& x, const Integer& y, const Integer&... tail) noexcept -> Integer {
  return sum_(x + y, tail...);
}

///\brief Automatically decide on a page size for leaf pages.
///\details This yields a suggestion.
constexpr auto autoconf_tree_leaf_size_suggestion_(std::size_t key_size, std::size_t val_size) -> std::size_t {
  std::size_t suggested_size = boost::integer::lcm(key_size + val_size, tree_prefer_byte_loss_lcm);
  return (suggested_size > tree_max_page_size ? tree_max_page_size : suggested_size);
}

///\brief Compute elements per page.
constexpr auto autoconf_tree_leaf_elems(std::size_t key_size, std::size_t val_size) -> std::size_t {
  std::size_t n_elems = autoconf_tree_leaf_size_suggestion_(key_size, val_size) / (key_size + val_size);
  return (n_elems < tree_min_elems_per_page ? tree_min_elems_per_page : n_elems);
}

///\brief Compute elements per page.
constexpr auto autoconf_tree_page_elems(std::size_t key_size, std::size_t val_size, std::size_t... augment_sizes) -> std::size_t {
  return autoconf_tree_leaf_elems(key_size, sum_(val_size, augment_sizes...));
}


} /* namespace monsoon::tx::detail */

#endif /* MONSOON_TX_DETAIL_TREE_SPEC_H */
