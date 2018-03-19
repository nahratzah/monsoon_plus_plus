#ifndef MONSOON_OBJPIPE_DETAIL_FLATTEN_OP_H
#define MONSOON_OBJPIPE_DETAIL_FLATTEN_OP_H

///\file
///\ingroup objpipe_detail

#include <iterator>
#include <optional>
#include <type_traits>
#include <utility>
#include <monsoon/objpipe/detail/fwd.h>
#include <monsoon/objpipe/detail/transport.h>
#include <monsoon/objpipe/detail/adapt.h>

namespace monsoon::objpipe::detail {


using std::make_move_iterator;
using std::begin;
using std::end;

template<typename Collection, typename = std::void_t<decltype(begin(std::declval<Collection&>()))>>
constexpr auto flatten_op_begin_(Collection& c)
noexcept(noexcept(begin(std::declval<Collection&>())))
-> decltype(begin(c)) {
  return begin(c); // ADL, with fallback to std::begin
}

template<typename Collection, typename = std::void_t<decltype(end(std::declval<Collection&>()))>>
constexpr auto flatten_op_end_(Collection& c)
noexcept(noexcept(end(std::declval<Collection&>())))
-> decltype(end(c)) {
  return end(c); // ADL, with fallback to std::end
}

template<typename Source, typename = void>
struct can_flatten_
: std::false_type
{};

template<typename Source>
struct can_flatten_<Source,
    std::void_t<decltype(flatten_op_begin_(std::declval<adapt::value_type<Source>&>())),
        decltype(flatten_op_end_(std::declval<adapt::value_type<Source>&>()))>>
: std::true_type
{};

template<typename Source>
constexpr bool can_flatten = can_flatten_<Source>::value;

template<typename Collection>
class flatten_op_store_copy_ {
 public:
  using collection = Collection;
  using begin_iterator =
      std::decay_t<decltype(make_move_iterator(flatten_op_begin_(std::declval<collection&>())))>;
  using end_iterator =
      std::decay_t<decltype(make_move_iterator(flatten_op_end_(std::declval<collection&>())))>;

  static_assert(
      std::is_base_of_v<std::input_iterator_tag, typename std::iterator_traits<begin_iterator>::iterator_category>,
      "Collection iterator must be an input iterator");

  constexpr flatten_op_store_copy_(collection&& c)
  noexcept(std::is_nothrow_move_constructible_v<collection>
      && noexcept(begin_iterator(make_move_iterator(flatten_op_begin_(std::declval<collection&>()))))
      && noexcept(end_iterator(make_move_iterator(flatten_op_end_(std::declval<collection&>())))))
  : c_(std::move(c)),
    begin_(make_move_iterator(flatten_op_begin_(c_))),
    end_(make_move_iterator(flatten_op_end_(c_)))
  {}

  constexpr flatten_op_store_copy_(const collection& c)
  noexcept(std::is_nothrow_copy_constructible_v<collection>
      && noexcept(begin_iterator(make_move_iterator(flatten_op_begin_(std::declval<const collection&>()))))
      && noexcept(end_iterator(make_move_iterator(flatten_op_end_(std::declval<const collection&>())))))
  : c_(c),
    begin_(make_move_iterator(flatten_op_begin_(c_))),
    end_(make_move_iterator(flatten_op_end_(c_)))
  {}

  constexpr auto empty() const
  noexcept(noexcept(std::declval<const begin_iterator&>() == std::declval<const end_iterator&>()))
  -> bool {
    return begin_ == end_;
  }

  auto deref() const
  noexcept(noexcept(*std::declval<const begin_iterator&>()))
  -> decltype(*std::declval<const begin_iterator&>()) {
    assert(begin_ != end_);
    return *begin_;
  }

  auto advance()
  noexcept(noexcept(++std::declval<begin_iterator&>()))
  -> void {
    assert(begin_ != end_);
    ++begin_;
  }

 private:
  collection c_;
  begin_iterator begin_;
  end_iterator end_;
};

template<typename Collection>
class flatten_op_store_ref_ {
 public:
  using collection = Collection;
  using begin_iterator =
      std::decay_t<decltype(flatten_op_begin_(std::declval<collection&>()))>;
  using end_iterator =
      std::decay_t<decltype(flatten_op_end_(std::declval<collection&>()))>;

  static_assert(
      std::is_base_of_v<std::input_iterator_tag, typename std::iterator_traits<begin_iterator>::iterator_category>,
      "Collection iterator must be an input iterator");

  constexpr flatten_op_store_ref_(collection&& c)
  noexcept(noexcept(begin_iterator(flatten_op_begin_(std::declval<collection&>())))
      && noexcept(end_iterator(flatten_op_end_(std::declval<collection&>()))))
  : begin_(flatten_op_begin_(c)),
    end_(flatten_op_end_(c))
  {}

  constexpr flatten_op_store_ref_(collection& c)
  noexcept(noexcept(begin_iterator(flatten_op_begin_(std::declval<collection&>())))
      && noexcept(end_iterator(flatten_op_end_(std::declval<collection&>()))))
  : begin_(flatten_op_begin_(c)),
    end_(flatten_op_end_(c))
  {}

  constexpr auto empty() const
  noexcept(noexcept(std::declval<const begin_iterator&>() == std::declval<const end_iterator&>()))
  -> bool {
    return begin_ == end_;
  }

  auto deref() const
  noexcept(noexcept(*std::declval<const begin_iterator&>()))
  -> decltype(*std::declval<const begin_iterator&>()) {
    assert(begin_ != end_);
    return *begin_;
  }

  auto advance()
  noexcept(noexcept(++std::declval<begin_iterator&>()))
  -> void {
    assert(begin_ != end_);
    ++begin_;
  }

 private:
  begin_iterator begin_;
  end_iterator end_;
};

template<typename Collection>
class flatten_op_store_rref_ {
 public:
  using collection = Collection;
  using begin_iterator =
      std::decay_t<decltype(make_move_iterator(flatten_op_begin_(std::declval<collection&>())))>;
  using end_iterator =
      std::decay_t<decltype(make_move_iterator(flatten_op_end_(std::declval<collection&>())))>;

  static_assert(
      std::is_base_of_v<std::input_iterator_tag, typename std::iterator_traits<begin_iterator>::iterator_category>,
      "Collection iterator must be an input iterator");

  constexpr flatten_op_store_rref_(collection&& c)
  noexcept(noexcept(begin_iterator(make_move_iterator(flatten_op_begin_(std::declval<collection&>()))))
      && noexcept(end_iterator(make_move_iterator(flatten_op_end_(std::declval<collection&>())))))
  : begin_(make_move_iterator(flatten_op_begin_(c))),
    end_(make_move_iterator(flatten_op_end_(c)))
  {}

  constexpr auto empty() const
  noexcept(noexcept(std::declval<const begin_iterator&>() == std::declval<const end_iterator&>()))
  -> bool {
    return begin_ == end_;
  }

  auto deref() const
  noexcept(noexcept(*std::declval<const begin_iterator&>()))
  -> decltype(*std::declval<const begin_iterator&>()) {
    assert(begin_ != end_);
    return *begin_;
  }

  auto advance()
  noexcept(noexcept(++std::declval<begin_iterator&>()))
  -> void {
    assert(begin_ != end_);
    ++begin_;
  }

 private:
  begin_iterator begin_;
  end_iterator end_;
};

template<typename Collection>
using flatten_op_store = std::conditional_t<
    std::is_volatile_v<Collection> || !std::is_reference_v<Collection>,
    flatten_op_store_copy_<std::decay_t<Collection>>,
    std::conditional_t<
        std::is_lvalue_reference_v<Collection> || std::is_const_v<Collection>,
        flatten_op_store_ref_<std::remove_reference_t<Collection>>,
        flatten_op_store_rref_<std::remove_reference_t<Collection>>>>;


/**
 * \brief Implements the flatten operation, that iterates over each element of a collection value.
 * \ingroup objpipe_detail
 *
 * \details
 * Replaces each collection element in the nested objpipe by the sequence of its elements.
 *
 * Requires that std::begin() and std::end() are valid for the given collection type.
 *
 * \tparam Source The nested source.
 * \sa \ref monsoon::objpipe::detail::adapter::flatten
 */
template<typename Source>
class flatten_op {
 private:
  using raw_collection_type = adapt::front_type<Source>;
  using store_type = flatten_op_store<raw_collection_type>;
  using item_type = decltype(std::declval<const store_type&>().deref());

  static constexpr bool ensure_avail_noexcept =
      noexcept(std::declval<Source&>().front())
      && noexcept(std::declval<Source&>().pop_front())
      && noexcept(std::declval<store_type>().empty())
      && std::is_nothrow_constructible_v<store_type, raw_collection_type>
      && std::is_nothrow_destructible_v<store_type>
      && (std::is_lvalue_reference_v<raw_collection_type>
          || std::is_rvalue_reference_v<raw_collection_type>
          || std::is_nothrow_move_constructible_v<raw_collection_type>);

 public:
  constexpr flatten_op(Source&& src)
  noexcept(std::is_nothrow_move_constructible_v<Source>)
  : src_(std::move(src)),
    active_()
  {}

  auto is_pullable()
  noexcept(noexcept(std::declval<Source&>().is_pullable())
      && ensure_avail_noexcept)
  -> bool {
    return src_.is_pullable() || ensure_avail_() == objpipe_errc::success;
  }

  auto wait()
  noexcept(ensure_avail_noexcept)
  -> objpipe_errc {
    return ensure_avail_();
  }

  auto front()
  noexcept(ensure_avail_noexcept
      && noexcept(std::declval<store_type&>().deref())
      && (std::is_lvalue_reference_v<item_type>
          || std::is_rvalue_reference_v<item_type>
          || std::is_nothrow_move_constructible_v<item_type>))
  -> transport<item_type> {
    const objpipe_errc e = ensure_avail_();
    if (e == objpipe_errc::success)
      return transport<item_type>(std::in_place_index<0>, active_->deref());
    return transport<item_type>(std::in_place_index<1>, e);
  }

  auto pop_front()
  noexcept(ensure_avail_noexcept
      && noexcept(std::declval<store_type&>().advance()))
  -> objpipe_errc {
    const objpipe_errc e = ensure_avail_();
    if (e == objpipe_errc::success) active_->advance();
    return e;
  }

  auto pull()
  noexcept(ensure_avail_noexcept
      && noexcept(std::declval<store_type&>().deref())
      && (std::is_lvalue_reference_v<item_type>
          || std::is_rvalue_reference_v<item_type>
          || std::is_nothrow_move_constructible_v<item_type>)
      && noexcept(std::declval<store_type&>().advance()))
  -> transport<item_type> {
    const objpipe_errc e = ensure_avail_();
    if (e == objpipe_errc::success) {
      auto result = transport<item_type>(std::in_place_index<0>, active_->deref());
      active_->advance();
      return result;
    }
    return transport<item_type>(std::in_place_index<1>, e);
  }

  auto try_pull()
  noexcept(ensure_avail_noexcept
      && noexcept(std::declval<store_type&>().deref())
      && (std::is_lvalue_reference_v<item_type>
          || std::is_rvalue_reference_v<item_type>
          || std::is_nothrow_move_constructible_v<item_type>)
      && noexcept(std::declval<store_type&>().advance()))
  -> transport<item_type> {
    return pull();
  }

 private:
  auto ensure_avail_() const
  noexcept(ensure_avail_noexcept)
  -> objpipe_errc {
    while (!active_.has_value() || active_->empty()) {
      if (active_.has_value() && active_->empty()) {
        objpipe_errc e = src_.pop_front();
        if (e != objpipe_errc::success)
          return e;
      }

      transport<raw_collection_type> front_val = src_.front();
      if (!front_val.has_value()) {
        assert(front_val.errc() != objpipe_errc::success);
        return front_val.errc();
      }
      active_.emplace(std::move(front_val).value());
    }
    return objpipe_errc::success;
  }

  mutable Source src_;
  mutable std::optional<store_type> active_;
};


} /* namespace monsoon::objpipe::detail */

#endif /* MONSOON_OBJPIPE_DETAIL_FLATTEN_OP_H */
