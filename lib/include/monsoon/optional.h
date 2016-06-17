#ifndef _ILIAS_OPTIONAL_H_
#define _ILIAS_OPTIONAL_H_

#include <type_traits>
#include <stdexcept>
#include <string>
#include <tuple>

namespace monsoon {


class optional_error
: public std::runtime_error
{
 public:
  explicit optional_error(const std::string&);
  explicit optional_error(const char*);

  ~optional_error() noexcept override;

  static void __throw()
      __attribute__((__noreturn__));
  static void __throw(const std::string&)
      __attribute__((__noreturn__));
  static void __throw(const char*)
      __attribute__((__noreturn__));
};


template<typename T>
class optional {
 private:
  using storage_t = std::aligned_union_t<0, T>;

  static_assert(!std::is_reference<T>::value,
                "Optional may not be a reference type.");

  static constexpr bool nothrow_copy_() {
    return std::is_nothrow_copy_constructible<T>::value;
  }
  static constexpr bool nothrow_move_() {
    return std::is_nothrow_copy_constructible<T>::value;
  }
  static constexpr bool nothrow_assign_() {
    return std::is_nothrow_assignable<T, const T&>::value;
  }
  static constexpr bool nothrow_move_assign_() {
    return std::is_nothrow_assignable<T, T&&>::value;
  }
  static constexpr bool nothrow_equality_() {
    return noexcept(std::declval<T>() == std::declval<T>());
  }

 public:
  using value_type = T;
  using const_value_type = const T;
  using reference = value_type&;
  using const_reference = const_value_type&;
  using pointer = value_type*;
  using const_pointer = const_value_type*;

  constexpr optional() noexcept;
  optional(const optional&) noexcept(nothrow_copy_());
  optional(optional&&) noexcept(nothrow_move_());
  optional(const value_type&) noexcept(nothrow_copy_());
  optional(value_type&&) noexcept(nothrow_move_());
  ~optional() noexcept;

  optional& operator=(const optional&)
      noexcept(nothrow_copy_() && nothrow_assign_());
  optional& operator=(optional&&)
      noexcept(nothrow_move_() && nothrow_move_assign_());

  explicit operator bool() const noexcept { return is_present(); }
  bool is_present() const noexcept;

  reference operator*();
  const_reference operator*() const;
  pointer operator->();
  const_pointer operator->() const;

  void assign(const value_type&)
      noexcept(nothrow_copy_() && nothrow_assign_());
  void assign(value_type&&)
      noexcept(nothrow_move_() && nothrow_move_assign_());
  value_type release();
  value_type release(const value_type&);
  value_type release(value_type&&);
  const value_type& get() const;
  value_type& get();
  value_type get(const value_type&);
  value_type get(value_type&&);
  value_type get(const value_type&) const;
  value_type get(value_type&&) const;

  bool operator==(const optional&) const noexcept(nothrow_equality_());
  bool operator!=(const optional&) const noexcept(nothrow_equality_());

 private:
  void ensure_present_() const;

  std::tuple<bool, storage_t> data_;
};

template<typename T>
class optional<T&> {
 public:
  using value_type = T;
  using const_value_type = const T;
  using reference = value_type&;
  using const_reference = const_value_type&;
  using pointer = value_type*;
  using const_pointer = const_value_type*;

  constexpr optional() noexcept = default;
  optional(const optional&) noexcept;
  optional(optional&&) noexcept;
  optional(value_type&) noexcept;
  ~optional() noexcept = default;

  optional& operator=(const optional&) noexcept;
  optional& operator=(optional&&) noexcept;

  explicit operator bool() const noexcept { return is_present(); }
  bool is_present() const noexcept;

  reference operator*() const;
  pointer operator->() const;

  void assign(value_type&) noexcept;
  value_type& release();
  value_type& release(value_type&);
  value_type& get() const;
  value_type& get(value_type&) const;

  bool operator==(const optional&) const noexcept;
  bool operator!=(const optional&) const noexcept;

 private:
  void ensure_present_() const;

  T* data_ = nullptr;
};


template<typename T, typename Fn>
auto map(const optional<T>&, Fn) ->
    decltype(std::declval<Fn>()(
                 std::declval<const optional<T>&>().get()));

template<typename T, typename Fn>
auto map(optional<T>&, Fn) ->
    decltype(std::declval<Fn>()(
                 std::declval<optional<T>&>().get()));

template<typename T, typename Fn>
auto map(optional<T>&&, Fn) ->
    decltype(std::declval<Fn>()(
                 std::declval<optional<T>>().release()));


template<typename T, typename Fn>
bool visit(const optional<T>&, Fn);

template<typename T, typename Fn>
bool visit(optional<T>&, Fn);

template<typename T, typename Fn>
bool visit(optional<T>&&, Fn);


} /* namespace monsoon */

#include "optional-inl.h"

#endif /* _ILIAS_OPTIONAL_H_ */
