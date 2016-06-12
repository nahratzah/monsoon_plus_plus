#ifndef _ILIAS_OPTIONAL_INL_H_
#define _ILIAS_OPTIONAL_INL_H_

#include "optional.h"
#include <utility>

namespace monsoon {


inline optional_error::optional_error(_namespace(std)::string_ref msg)
: _namespace(std)::runtime_error(msg)
{}

inline optional_error::optional_error(const _namespace(std)::string& msg)
: _namespace(std)::runtime_error(msg)
{}

inline optional_error::optional_error(const char* msg)
: _namespace(std)::runtime_error(msg)
{}


template<typename T>
constexpr optional<T>::optional() noexcept
: data_(false, storage_t())
{}

template<typename T>
optional<T>::optional(const optional& o) noexcept(nothrow_copy_())
: optional()
{
  if (o) assign(o.get());
}

template<typename T>
optional<T>::optional(optional&& o) noexcept(nothrow_move_())
: optional()
{
  if (o) assign(o.release());
}

template<typename T>
optional<T>::optional(const value_type& v) noexcept(nothrow_copy_())
: optional()
{
  assign(v);
}

template<typename T>
optional<T>::optional(value_type&& v) noexcept(nothrow_move_())
: optional()
{
  using _namespace(std)::move;

  assign(move(v));
}

template<typename T>
optional<T>::~optional() noexcept {
  if (is_present()) release();
}

template<typename T>
auto optional<T>::operator=(const optional& o)
    noexcept(nothrow_copy_() && nothrow_assign_()) -> optional& {
  if (o.is_present())
    assign(o.get());
  else if (is_present())
    release();
  return *this;
}

template<typename T>
auto optional<T>::operator=(optional&& o)
    noexcept(nothrow_move_() && nothrow_move_assign_()) -> optional& {
  if (o.is_present())
    assign(o.release());
  else if (is_present())
    release();
  return *this;
}

template<typename T>
auto optional<T>::is_present() const noexcept -> bool {
  using _namespace(std)::get;

  return get<0>(data_);
}

template<typename T>
auto optional<T>::operator*() -> reference {
  return get();
}

template<typename T>
auto optional<T>::operator*() const -> const_reference {
  return get();
}

template<typename T>
auto optional<T>::operator->() -> pointer {
  return &get();
}

template<typename T>
auto optional<T>::operator->() const -> const_pointer {
  return &get();
}

template<typename T>
auto optional<T>::assign(const value_type& v)
    noexcept(nothrow_copy_() && nothrow_assign_()) ->
    void {
  using _namespace(std)::get;

  storage_t& storage = get<1>(data_);
  void*const vptr = &storage;

  if (is_present()) {
    *static_cast<pointer>(vptr) = v;
  } else {
    new (vptr) value_type(v);
    get<0>(data_) = true;
  }
}

template<typename T>
auto optional<T>::assign(value_type&& v)
    noexcept(nothrow_move_() && nothrow_move_assign_()) ->
    void {
  using _namespace(std)::get;
  using _namespace(std)::move;

  storage_t& storage = get<1>(data_);
  void*const vptr = &storage;

  if (is_present()) {
    *static_cast<pointer>(vptr) = move(v);
  } else {
    new (vptr) value_type(move(v));
    get<0>(data_) = true;
  }
}

template<typename T>
auto optional<T>::release() -> value_type {
  using _namespace(std)::get;
  using _namespace(std)::move;

  ensure_present_();

  storage_t& storage = get<1>(data_);
  void*const vptr = &storage;
  value_type rv = move(*static_cast<pointer>(vptr));
  static_cast<pointer>(vptr)->~value_type();
  get<0>(data_) = false;
  return rv;
}

template<typename T>
auto optional<T>::release(const value_type& dfl) -> value_type {
  if (is_present()) return release();
  return dfl;
}

template<typename T>
auto optional<T>::release(value_type&& dfl) -> value_type {
  if (is_present()) return release();
  value_type rv = move(dfl);
  return rv;
}

template<typename T>
auto optional<T>::get() const -> const value_type& {
  using _namespace(std)::get;

  ensure_present_();

  const storage_t& storage = get<1>(data_);
  const void*const vptr = &storage;
  return *static_cast<const_pointer>(vptr);
}

template<typename T>
auto optional<T>::get() -> value_type& {
  using _namespace(std)::get;

  ensure_present_();

  storage_t& storage = get<1>(data_);
  void*const vptr = &storage;
  return *static_cast<pointer>(vptr);
}

template<typename T>
auto optional<T>::get(const value_type& dfl) -> value_type {
  if (is_present()) return get();
  return dfl;
}

template<typename T>
auto optional<T>::get(value_type&& dfl) -> value_type {
  if (is_present()) return get();
  value_type rv = move(dfl);
  return rv;
}

template<typename T>
auto optional<T>::ensure_present_() const -> void {
  if (_predict_false(!is_present()))
    optional_error::__throw();
}


template<typename T>
optional<T&>::optional(const optional& o) noexcept
: optional()
{
  if (o) assign(o.get());
}

template<typename T>
optional<T&>::optional(optional&& o) noexcept
: optional()
{
  if (o) assign(o.release());
}

template<typename T>
optional<T&>::optional(value_type& v) noexcept
: optional()
{
  assign(v);
}

template<typename T>
auto optional<T&>::operator=(const optional& o) noexcept -> optional& {
  if (o.is_present())
    assign(o.get());
  else if (is_present())
    release();
  return *this;
}

template<typename T>
auto optional<T&>::operator=(optional&& o) noexcept -> optional& {
  if (o.is_present())
    assign(o.release());
  else if (is_present())
    release();
  return *this;
}

template<typename T>
auto optional<T&>::is_present() const noexcept -> bool {
  return data_ != nullptr;
}

template<typename T>
auto optional<T&>::operator*() const -> reference {
  return get();
}

template<typename T>
auto optional<T&>::operator->() const -> pointer {
  return &get();
}

template<typename T>
auto optional<T&>::assign(value_type& v) noexcept -> void {
  data_ = &v;
}

template<typename T>
auto optional<T&>::release() -> value_type& {
  ensure_present_();
  return *_namespace(std)::exchange(data_, nullptr);
}

template<typename T>
auto optional<T&>::release(value_type& dfl) -> value_type& {
  if (is_present()) return release();
  return dfl;
}

template<typename T>
auto optional<T&>::get() const -> value_type& {
  ensure_present_();
  return *data_;
}

template<typename T>
auto optional<T&>::get(value_type& dfl) const -> value_type& {
  if (is_present()) return get();
  return dfl;
}

template<typename T>
auto optional<T&>::ensure_present_() const -> void {
  if (_predict_false(!is_present()))
    optional_error::__throw();
}


template<typename T, typename Fn>
auto map(const optional<T>& o, Fn fn) ->
    decltype(_namespace(std)::declval<Fn>()(
                 _namespace(std)::declval<const optional<T>&>().get())) {
  using type =
      decltype(_namespace(std)::declval<Fn>()(
                   _namespace(std)::declval<const optional<T>&>().get()));

  if (!o) return type();
  return fn(o.get());
}

template<typename T, typename Fn>
auto map(optional<T>& o, Fn fn) ->
    decltype(_namespace(std)::declval<Fn>()(
                 _namespace(std)::declval<optional<T>&>().get())) {
  using type =
      decltype(_namespace(std)::declval<Fn>()(
                   _namespace(std)::declval<optional<T>&>().get()));

  if (!o) return type();
  return fn(o.get());
}

template<typename T, typename Fn>
auto map(optional<T>&& o, Fn fn) ->
    decltype(_namespace(std)::declval<Fn>()(
                 _namespace(std)::declval<optional<T>>().release())) {
  using type =
      decltype(_namespace(std)::declval<Fn>()(
                   _namespace(std)::declval<optional<T>>().release()));

  if (!o) return type();
  return fn(o.release());
}


template<typename T, typename Fn>
auto visit(const optional<T>& o, Fn fn) -> bool {
  if (!o) return false;
  fn(o.get());
  return true;
}

template<typename T, typename Fn>
auto visit(optional<T>& o, Fn fn) -> bool {
  if (!o) return false;
  fn(o.get());
  return true;
}

template<typename T, typename Fn>
auto visit(optional<T>&& o, Fn fn) -> bool {
  if (!o) return false;
  fn(o.release());
  return true;
}


} /* namespace monsoon */

#endif /* _ILIAS_OPTIONAL_INL_H_ */
