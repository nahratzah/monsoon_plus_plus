#ifndef MONSOON_OBJPIPE_DETAIL_TRANSPORT_H
#define MONSOON_OBJPIPE_DETAIL_TRANSPORT_H

#include <memory>
#include <type_traits>
#include <variant>
#include <monsoon/objpipe/errc.h>

namespace monsoon::objpipe::detail {


template<typename T>
class transport {
 public:
  using type = T;

  template<typename U, typename = std::enable_if_t<std::is_constructible_v<T, std::add_rvalue_reference_t<U>>, void>>
  constexpr transport(transport<U>&& other)
  noexcept(std::is_nothrow_constructible_v<T, U>)
  : transport(std::in_place_index<1>, other.errc())
  {
    if (other.has_value())
      emplace<0>(std::move(other).get());
  }

  template<typename U, typename = std::enable_if_t<std::is_constructible_v<T, std::add_const_t<U>>, void>>
  constexpr transport(const transport<U>& other)
  noexcept(std::is_nothrow_constructible_v<T, U>)
  : transport(std::in_place_index<1>, other.errc())
  {
    if (other.has_value())
      emplace<0>(other.get());
  }

  template<typename Arg>
  constexpr transport(std::in_place_index_t<0>, Arg&& arg)
  noexcept(std::is_nothrow_constructible_v<T, Arg>)
  : data_(std::in_place_index<0>, std::forward<Arg>(arg))
  {}

  constexpr transport(std::in_place_index_t<1>, objpipe_errc e)
  noexcept
  : data_(std::in_place_index<1>, e)
  {}

  auto has_value() const
  noexcept
  -> bool {
    return data_.index() == 0;
  }

  auto value() const &
  noexcept
  -> const T& {
    assert(has_value());
    return std::get<0>(data_);
  }

  auto value() const &&
  noexcept
  -> const T& {
    assert(has_value());
    return std::get<0>(data_);
  }

  auto value() &
  noexcept
  -> T& {
    assert(has_value());
    return std::get<0>(data_);
  }

  auto value() &&
  noexcept
  -> T&& {
    assert(has_value());
    return std::get<0>(std::move(data_));
  }

  auto errc() const
  noexcept
  -> objpipe_errc {
    return (has_value() ? objpipe_errc::success : std::get<1>(data_));
  }

  template<typename... Args>
  auto emplace(std::in_place_index_t<0>, Args&&... args)
  noexcept(std::is_nothrow_constructible_v<T, Args...>)
  -> void {
    data_.emplace(std::in_place_index<0>, std::forward<Args>(args)...);
  }

  auto emplace(std::in_place_index_t<1>, objpipe_errc e)
  noexcept
  -> void {
    data_.emplace(std::in_place_index<1>, e);
  }

 private:
  std::variant<T, objpipe_errc> data_;
};

template<typename T>
class transport<T&> {
 public:
  using type = T&;

  template<typename U, typename = std::enable_if_t<std::is_base_of_v<T, std::remove_reference_t<U>> && std::is_reference_v<U>, void>>
  constexpr transport(const transport<U>& other)
  noexcept
  : data_(other.data_)
  {}

  explicit constexpr transport(std::in_place_index_t<0>, T& v)
  noexcept
  : data_(std::in_place_index<0>, std::addressof(v))
  {}

  constexpr transport(std::in_place_index_t<1>, objpipe_errc e)
  noexcept
  : data_(std::in_place_index<1>, e)
  {}

  auto has_value() const
  noexcept
  -> bool {
    return data_.index() == 0;
  }

  auto value() const
  noexcept
  -> T& {
    assert(has_value());
    return *std::get<0>(data_);
  }

  auto errc() const
  noexcept
  -> objpipe_errc {
    return (has_value() ? objpipe_errc::success : std::get<1>(data_));
  }

  auto emplace(std::in_place_index_t<0>, T& v)
  noexcept
  -> void {
    data_.emplace(std::in_place_index<0>, std::addressof(v));
  }

  auto emplace(std::in_place_index_t<1>, objpipe_errc e)
  noexcept
  -> void {
    data_.emplace(std::in_place_index<1>, e);
  }

 private:
  std::variant<T*, objpipe_errc> data_;
};

template<typename T>
class transport<T&&> {
 public:
  using type = T&&;

  template<typename U, typename = std::enable_if_t<std::is_base_of_v<T, U>, void>>
  constexpr transport(transport<U&&>&& other)
  noexcept
  : data_(other.data_)
  {}

  explicit constexpr transport(std::in_place_index_t<0>, T&& v)
  noexcept
  : data_(std::in_place_index<0>, std::addressof(v))
  {}

  constexpr transport(std::in_place_index_t<1>, objpipe_errc e)
  noexcept
  : data_(std::in_place_index<1>, e)
  {}

  auto has_value() const
  noexcept
  -> bool {
    return data_.index() == 0;
  }

  auto value() const
  noexcept
  -> T&& {
    assert(has_value());
    return std::move(*std::get<0>(data_));
  }

  auto errc() const
  noexcept
  -> objpipe_errc {
    return (has_value() ? objpipe_errc::success : std::get<1>(data_));
  }

  auto emplace(std::in_place_index_t<0>, T&& v)
  noexcept
  -> void {
    data_.emplace(std::in_place_index<0>, std::addressof(v));
  }

  auto emplace(std::in_place_index_t<1>, objpipe_errc e)
  noexcept
  -> void {
    data_.emplace(std::in_place_index<1>, e);
  }

 private:
  std::variant<T*, objpipe_errc> data_;
};


} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_TRANSPORT_H */
