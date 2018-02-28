#ifndef MONSOON_CACHE_KEY_DECORATOR_H
#define MONSOON_CACHE_KEY_DECORATOR_H

#include <memory>
#include <optional>
#include <type_traits>

namespace monsoon::cache {


///\brief Element decorator, for storing a key.
template<typename T>
struct key_decorator() {
  using key_type = T;

  key_decorator() noexcept = default;

  template<typename Alloc, typename Types...,
      typename = std::enable_if_t<
          std::uses_allocator_v<key_type, Alloc>
          && std::is_constructible_v<key_type, const key_type&, Alloc>>>
  key_decorator(
      [[maybe_unused]] std::allocator_arg_t tag,
      Alloc& alloc,
      const std::tuple<Types...>& init)
  noexcept(std::is_nothrow_constructible_v<key_type, const key_type&, Alloc>)
  : key(std::in_place, std::get<key_type>(init), alloc)
  {}

  template<typename Alloc, typename Types...,
      typename = std::enable_if_t<
          std::uses_allocator_v<key_type, Alloc>
          && !std::is_constructible_v<key_type, const key_type&, Alloc>>>
  key_decorator(std::allocator_arg_t tag, Alloc& alloc, const std::tuple<Types...>& init)
  noexcept(std::is_nothrow_constructible_v<key_type, std::allocator_tag_t, Alloc, const key_type&>)
  : key(std::in_place, tag, alloc, std::get<key_type>(init))
  {}

  template<typename Alloc, typename Types...,
      typename = std::enable_if_t<
          !std::uses_allocator_v<std::optional<key_type>, Alloc>>>
  key_decorator(
      [[maybe_unused]] std::allocator_arg_t tag,
      [[maybe_unused]] Alloc& alloc,
      const std::tuple<Types...>& init)
  noexcept(std::is_nothrow_copy_constructible_v<key_type>)
  : key(std::get<key_type>(init))
  {}

  std::optional<key_type> key;
};


} /* namespace monsoon::cache */

#endif /* MONSOON_CACHE_KEY_DECORATOR_H */
