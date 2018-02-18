#ifndef MONSOON_OBJPIPE_DETAIL_ADAPT_H
#define MONSOON_OBJPIPE_DETAIL_ADAPT_H

///\file
///\ingroup objpipe_detail

#include <cassert>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>
#include <monsoon/objpipe/errc.h>
#include <monsoon/objpipe/detail/fwd.h>
#include <monsoon/objpipe/detail/peek_op.h>

/**
 * \brief Adapter functions for objpipe source.
 *
 * \details
 * In the following definitions, T refers to the value type of source.
 * T may optionally be const, an lvalue reference, an rvalue reference,
 * or any combination thereof.
 * Also, wether T is const or a reference may vary for different functions.
 *
 * Source must implement:
 * \code
 * auto is_pullable() const noexcept -> bool
 * auto wait() const -> objpipe_errc;
 * auto front() const -> std::variant<T, objpipe_errc>;
 * auto pop_front() -> objpipe_errc;
 * \endcode
 *
 * Source may optionally implement:
 * \code
 * auto try_pull() -> std::variant<T, objpipe_errc>;
 * auto pull() -> std::variant<T, objpipe_errc>;
 * \endcode
 *
 * In addition, source may implement specialization of the mutator functions:
 * \code
 * auto filter(Pred&& pred) && -> ...
 * auto transform(Fn&& fn) && -> ...
 * auto assertion(Fn&& fn) && -> ...
 * auto flatten() && -> ...
 * \endcode
 */
namespace monsoon::objpipe::detail::adapt {


template<typename Source, typename = void>
struct has_try_pull_
: std::false_type
{};
template<typename Source>
struct has_try_pull_<Source, std::void_t<decltype(std::declval<Source&>().try_pull())>>
: std::true_type
{};

template<typename Source, typename = void>
struct has_pull_
: std::false_type
{};
template<typename Source>
struct has_pull_<Source, std::void_t<decltype(std::declval<Source&>().pull())>>
: std::true_type
{};

template<typename Source, typename Fn, typename = void>
struct has_filter_
: std::false_type
{};
template<typename Source, typename Fn>
struct has_filter_<Source, Fn, std::void_t<decltype(std::declval<Source>().filter(std::declval<Fn>()))>>
: std::true_type
{};

template<typename Source, typename Fn, typename = void>
struct has_transform_
: std::false_type
{};
template<typename Source, typename Fn>
struct has_transform_<Source, Fn, std::void_t<decltype(std::declval<Source>().transform(std::declval<Fn>()))>>
: std::true_type
{};

template<typename Source, typename Fn, typename = void>
struct has_assertion_
: std::false_type
{};
template<typename Source, typename Fn>
struct has_assertion_<Source, Fn, std::void_t<decltype(std::declval<Source>().assertion(std::declval<Fn>()))>>
: std::true_type
{};

template<typename Source, typename = void>
struct has_flatten_
: std::false_type
{};
template<typename Source>
struct has_flatten_<Source, std::void_t<decltype(std::declval<Source>().flatten())>>
: std::true_type
{};

///\brief Trait testing if try_pull() is implemented.
template<typename Source>
constexpr bool has_try_pull = has_try_pull_<Source>::value;

///\brief Trait testing if pull() is implemented.
template<typename Source>
constexpr bool has_pull = has_pull_<Source>::value;

///\brief Trait testing if filter() is specialized.
template<typename Source, typename Fn>
constexpr bool has_filter = has_filter_<Source, Fn>::value;

///\brief Trait testing if transform() is specialized.
template<typename Source, typename Fn>
constexpr bool has_transform = has_transform_<Source, Fn>::value;

///\brief Trait testing if assertion() is specialized.
template<typename Source, typename Fn>
constexpr bool has_assertion = has_assertion_<Source, Fn>::value;

///\brief Trait testing if flatten() is specialized.
template<typename Source>
constexpr bool has_flatten = has_flatten_<Source>::value;

///\brief Trait containing the value type of Source::front() const.
template<typename Source>
using front_type = std::variant_alternative_t<
    0,
    decltype(std::declval<const Source&>().front())>;

template<typename Source, bool = has_try_pull<Source>>
struct try_pull_type_ {
  using type = std::variant_alternative_t<
      0,
      decltype(std::declval<Source&>().try_pull())>;
};
template<typename Source>
struct try_pull_type_<Source, false> {
  using type = std::decay_t<front_type<Source>>;
};
///\brief Trait containing the value type of Source::try_pull().
template<typename Source>
using try_pull_type = typename try_pull_type_<Source>::type;

template<typename Source, bool = has_pull<Source>>
struct pull_type_ {
  using type = std::variant_alternative_t<
      0,
      decltype(std::declval<Source&>().pull())>;
};
template<typename Source>
struct pull_type_<Source, false> {
  using type = try_pull_type<Source>;
};
///\brief Trait containing the value type of Source::pull().
template<typename Source>
using pull_type = typename pull_type_<Source>::type;

///\brief Trait containing the value type of Source.
template<typename Source>
using value_type =
    std::remove_cv_t<std::remove_reference_t<front_type<Source>>>;


/**
 * \brief Adapter for the is_pullable function.
 * \ingroup objpipe_detail
 *
 * \details Adapts a call to the is_pullable() function.
 * \tparam Source Type of the objpipe source.
 */
template<typename Source>
auto is_pullable(
    const Source& src ///< [in] Object pipe source that is to be adapted.
    )
noexcept(noexcept(std::declval<const Source&>().is_pullable()))
-> bool {
  return src.is_pullable();
}

/**
 * \brief Adapter for the wait function.
 * \ingroup objpipe_detail
 *
 * \details Adapts a call to the wait() function.
 * \tparam Source Type of the objpipe source.
 */
template<typename Source>
auto wait(
    const Source& src ///< [in] Object pipe source that is to be adapted.
    )
noexcept(noexcept(std::declval<const Source&>().wait()))
-> objpipe_errc {
  return src.wait();
}

/**
 * \brief Adapter for the wait function.
 * \ingroup objpipe_detail
 *
 * \details Adapts a call to the wait() function.
 * \tparam Source Type of the objpipe source.
 * \return A variant containing the next value or an error code.
 * \throw std::system_error if the front() call fails.
 */
template<typename Source>
auto front(
    const Source& src ///< [in] Object pipe source that is to be adapted.
    )
-> front_type<Source> {
  std::variant<front_type<Source>, objpipe_errc> v = src.front();
  if (v.index() == 0)
    return std::get<0>(std::move(v));

  assert(std::get<1>(v) != objpipe_errc::success);
  throw std::system_error(
      static_cast<int>(std::get<1>(v)),
      objpipe_category());
}

/**
 * \brief Adapter for the pop_front function.
 * \ingroup objpipe_detail
 *
 * \details Adapts a call to the pop_front() function.
 * \tparam Source Type of the objpipe source.
 * \throw std::system_error if the pop_front() call fails.
 */
template<typename Source>
auto pop_front(
    Source& src ///< [in] Object pipe source that is to be adapted.
    )
-> std::enable_if_t<!std::is_const_v<Source>, void> {
  const objpipe_errc e = src.pop_front();
  if (e != objpipe_errc::success) {
    throw std::system_error(
        static_cast<int>(e),
        objpipe_category());
  }
}

/**
 * \brief raw try_pull adapter.
 * \ingroup objpipe_detail
 *
 * \details Wraps a call to the try_pull method.
 *
 * \tparam Source Type of the objpipe source.
 * \return A variant containing the pulled value or an error code.
 */
template<typename Source>
auto raw_try_pull(
    Source& src ///< [in] Object pipe source that is to be adapted.
    )
noexcept(noexcept(std::declval<Source&>().try_pull()))
-> std::enable_if_t<!std::is_const_v<Source> && has_try_pull<Source>,
    std::variant<try_pull_type<Source>, objpipe_errc>> {
  return src.try_pull();
}

/**
 * \brief raw try_pull adapter.
 * \ingroup objpipe_detail
 *
 * \details Adapts a call to try_pull method.
 *
 * Handles the case of unimplemented try_pull method.
 *
 * \note emulated using front() and pop_front().
 *
 * \tparam Source Type of the objpipe source.
 * \return A variant containing the pulled value or an error code.
 * \throw std::system_error if the try_pull() call fails.
 */
template<typename Source>
auto raw_try_pull(
    Source& src ///< [in] Object pipe source that is to be adapted.
    )
noexcept(noexcept(std::declval<const Source&>().front())
    && noexcept(std::declval<Source&>().pop_front())
    && (std::is_reference_v<try_pull_type<Source>>
        || (std::is_nothrow_move_constructible_v<std::decay_t<try_pull_type<Source>>>
            && std::is_nothrow_destructible_v<std::decay_t<try_pull_type<Source>>>)))
-> std::enable_if_t<!std::is_const_v<Source> && !has_try_pull<Source>,
    std::variant<try_pull_type<Source>, objpipe_errc>> {
  std::variant<try_pull_type<Source>, objpipe_errc> v = src.front();
  if (v.index() == 0) {
    objpipe_errc e = src.pop_front();
    if (e != objpipe_errc::success)
      v.emplace(std::in_place_index<1>, e);
  }
  return v;
}

/**
 * \brief try_pull adapter.
 * \ingroup objpipe_detail
 *
 * \details Adapts a call to try_pull method.
 *
 * Handles the case of implemented try_pull method.
 *
 * \tparam Source Type of the objpipe source.
 * \return A optional containing the pulled value, if one was available.
 * \throw std::system_error if the try_pull() call fails.
 */
template<typename Source>
auto try_pull(
    Source& src ///< [in] Object pipe source that is to be adapted.
    )
-> std::optional<std::decay_t<try_pull_type<Source>>> {
  std::variant<try_pull_type<Source>, objpipe_errc> v = raw_try_pull(src);
  if (v.index() == 0)
    return std::make_optional(std::get<0>(std::move(v)));

  if (std::get<1>(v) != objpipe_errc::success) {
    throw std::system_error(
        static_cast<int>(std::get<1>(v)),
        objpipe_category());
  }
  return {};
}

/**
 * \brief raw pull adapter.
 * \ingroup objpipe_detail
 *
 * \details Adapts a call to the pull method.
 *
 * \return A variant containing the pulled value or an error code.
 * \tparam Source Type of the objpipe source.
 */
template<typename Source>
auto raw_pull(
    Source& src ///< [in] Object pipe source that is to be adapted.
    )
noexcept(noexcept(std::declval<Source&>().pull()))
-> std::enable_if_t<!std::is_const_v<Source> && has_pull<Source>,
    std::variant<pull_type<Source>, objpipe_errc>> {
  return src.pull();
}

/**
 * \brief raw pull adapter.
 * \ingroup objpipe_detail
 *
 * \details Adapts a call to the pull method.
 *
 * \note emulated using wait() and Source::try_pull().
 *
 * \return A variant containing the pulled value or an error code.
 * \tparam Source Type of the objpipe source.
 */
template<typename Source>
auto raw_pull(
    Source& src ///< [in] Object pipe source that is to be adapted.
    )
noexcept(noexcept(raw_try_pull(std::declval<Source&>()))
    && noexcept(std::declval<const Source&>().wait()))
-> std::enable_if_t<!std::is_const_v<Source> && !has_pull<Source>,
    std::variant<pull_type<Source>, objpipe_errc>> {
  for (;;) {
    std::variant<pull_type<Source>, objpipe_errc> v = raw_try_pull(src);
    if (v.index() == 0)
      return std::get<0>(std::move(v));

    if (std::get<1>(v) == objpipe_errc::success) std::get<1>(v) = wait(src);
    if (std::get<1>(v) != objpipe_errc::success)
      return v;
  }
}

/**
 * \brief Adapter that maps the internal pull method to throw on error.
 * \ingroup objpipe_detail
 *
 * \details Adapts a call to Source pull() method, to throw an exception if
 * it fails.
 *
 * \tparam Source Type of the objpipe source.
 * \return The pulled value.
 * \throw std::system_error if the pull() call fails.
 */
template<typename Source>
auto pull(
    Source& src ///< [in] Object pipe source that is to be adapted.
    )
-> std::enable_if_t<!std::is_const_v<Source> && has_pull<Source>,
    pull_type<Source>> {
  std::variant<pull_type<Source>, objpipe_errc> v = raw_pull(src);
  if (v.index() == 0) return std::get<0>(std::move(v));

  assert(std::get<1>(v) != objpipe_errc::success);
  throw std::system_error(static_cast<int>(std::get<1>(v)), objpipe_category());
}

/**
 * \brief Adapter that maps the internal pull method to throw on error.
 * \ingroup objpipe_detail
 *
 * \details Emulates a pull(objpipe_errc&) call on source.
 *
 * \tparam Source Type of the objpipe source.
 * \return Optional with the pulled value, iff e == objpipe_errc::success.
 * \throw std::system_error if the pull() call fails.
 */
template<typename Source>
auto pull(
    Source& src, ///< [in] Object pipe source that is to be adapted.
    objpipe_errc& e ///< [out] Error code.
    )
noexcept(std::is_nothrow_move_constructible_v<std::decay_t<pull_type<Source>>>
    && noexcept(raw_pull(std::declval<Source&>())))
-> std::enable_if_t<!std::is_const_v<Source> && has_pull<Source>,
    std::optional<std::decay_t<pull_type<Source>>>> {
  std::variant<pull_type<Source>, objpipe_errc> v = raw_pull(src);
  if (v.index() == 0) {
    e = objpipe_errc::success;
    return std::make_optional(std::get<0>(std::move(v)));
  }

  assert(std::get<1>(v) != objpipe_errc::success);
  e = std::get<1>(v);
  return {};
}

/**
 * \brief Adapter for the filter function.
 * \ingroup objpipe_detail
 *
 * \return A new source, filtering the values in the argument source
 * using the specified functor.
 */
template<typename Source, typename Fn>
constexpr auto filter(
    Source&& src, ///< [in] Object pipe source that is to be adapted.
    Fn&& fn ///< [in] A one-argument invocable, returning the filtered value.
    )
noexcept(std::is_nothrow_constructible_v<filter_op<Source, std::decay_t<Fn>>, Source, Fn>)
-> std::enable_if_t<!has_filter<Source, std::decay_t<Fn>>,
    filter_op<Source, std::decay_t<Fn>>> {
  static_assert(!std::is_reference_v<Source> && !std::is_const_v<Source>,
      "Source must be an rvalue reference");
  return filter_op<Source, std::decay_t<Fn>>(std::move(src), std::forward<Fn>(fn));
}

/**
 * \brief Adapter for the filter function.
 * \ingroup objpipe_detail
 *
 * \note Uses the specialization on the source.
 *
 * \return A new source, filtering the values in the argument source
 * using the specified functor.
 */
template<typename Source, typename Fn>
constexpr auto filter(
    Source&& src, ///< [in] Object pipe source that is to be adapted.
    Fn&& fn ///< [in] A one-argument invocable, returning the filtered value.
    )
noexcept(std::is_nothrow_move_constructible_v<Source>
    && std::is_nothrow_constructible_v<std::decay_t<Fn>, Fn>)
-> std::enable_if_t<has_filter<Source, std::decay_t<Fn>>,
    decltype(std::declval<Source>().filter(std::declval<Fn>()))> {
  static_assert(!std::is_reference_v<Source> && !std::is_const_v<Source>,
      "Source must be an rvalue reference");
  return std::move(src).filter(std::forward<Fn>(fn));
}

/**
 * \brief Adapter for the transform function.
 * \ingroup objpipe_detail
 *
 * \return A new source, transforming the values in the argument source
 * using the specified functor.
 */
template<typename Source, typename Fn>
constexpr auto transform(
    Source&& src, ///< [in] Object pipe source that is to be adapted.
    Fn&& fn ///< [in] A one-argument invocable, returning the transformed value.
    )
#if 0
noexcept(std::is_nothrow_constructible_v<transform_op<Source, std::decay_t<Fn>>, Source, Fn>)
#endif
-> std::enable_if_t<!has_transform<Source, std::decay_t<Fn>>,
    transform_op<Source, std::decay_t<Fn>>> {
  static_assert(!std::is_reference_v<Source> && !std::is_const_v<Source>,
      "Source must be an rvalue reference");
  return transform_op<Source, std::decay_t<Fn>>(std::move(src), std::forward<Fn>(fn));
}

/**
 * \brief Adapter for the transform function.
 * \ingroup objpipe_detail
 *
 * \note Uses the specialization on the source.
 *
 * \return A new source, transforming the values in the argument source
 * using the specified functor.
 */
template<typename Source, typename Fn>
constexpr auto transform(
    Source&& src, ///< [in] Object pipe source that is to be adapted.
    Fn&& fn ///< [in] A one-argument invocable, returning the transformed value.
    )
noexcept(noexcept(std::declval<Source>().transform(std::declval<Fn>())))
-> std::enable_if_t<has_transform<Source, std::decay_t<Fn>>,
    decltype(std::declval<Source>().transform(std::declval<Fn>()))> {
  static_assert(!std::is_reference_v<Source> && !std::is_const_v<Source>,
      "Source must be an rvalue reference");
  return std::move(src).transform(std::forward<Fn>(fn));
}

/**
 * \brief Adapter for the peek function.
 * \ingroup objpipe_detail
 *
 * \return A new source, in place modifying the values in the argument source
 * using the specified functor.
 */
template<typename Source, typename Fn>
constexpr auto peek(
    Source&& src, ///< [in] Object pipe source that is to be adapted.
    Fn&& fn ///< [in] A one-argument invocable.
    )
noexcept(noexcept(transform(
            std::move(src),
            peek_adapter<std::decay_t<Fn>>(std::forward<Fn>(fn)))))
-> decltype(transform(
        std::move(src),
        peek_adapter<std::decay_t<Fn>>(std::forward<Fn>(fn)))) {
  static_assert(!std::is_reference_v<Source> && !std::is_const_v<Source>,
      "Source must be an rvalue reference");
  return transform(
      std::move(src),
      peek_adapter<std::decay_t<Fn>>(std::forward<Fn>(fn)));
}

/**
 * \brief Adapter for the assertion function.
 * \ingroup objpipe_detail
 *
 * \return A new source, which asserts the given functor for each element.
 */
template<typename Source, typename Fn>
constexpr auto assertion(
    Source&& src, ///< [in] Object pipe source that is to be adapted.
    Fn&& fn ///< [in] A one-argument invocable predicate.
    )
noexcept(std::is_nothrow_constructible_v<assertion_op<Source, std::decay_t<Fn>>, Source, Fn>)
-> std::enable_if_t<!has_assertion<Source, std::decay_t<Fn>>,
    assertion_op<Source, std::decay_t<Fn>>> {
  static_assert(!std::is_reference_v<Source> && !std::is_const_v<Source>,
      "Source must be an rvalue reference");
  return assertion_op<Source, std::decay_t<Fn>>(std::move(src), std::forward<Fn>(fn));
}

/**
 * \brief Adapter for the assertion function.
 * \ingroup objpipe_detail
 *
 * \note Uses the specialization on the source.
 *
 * \return A new source, which asserts the given functor for each element.
 */
template<typename Source, typename Fn>
constexpr auto assertion(
    Source&& src, ///< [in] Object pipe source that is to be adapted.
    Fn&& fn ///< [in] A one-argument invocable predicate.
    )
noexcept(noexcept(std::declval<Source>().assertion(std::declval<Fn>())))
-> std::enable_if_t<has_assertion<Source, std::decay_t<Fn>>,
    decltype(std::declval<Source>().assertion(std::declval<Fn>()))> {
  static_assert(!std::is_reference_v<Source> && !std::is_const_v<Source>,
      "Source must be an rvalue reference");
  return std::move(src).assertion(std::forward<Fn>(fn));
}

/**
 * \brief Adapter for the flatten function.
 * \ingroup objpipe_detail
 *
 * \return A new source, iterating over all the elements of each value
 * in the argumet source.
 */
template<typename Source>
constexpr auto flatten(
    Source&& src ///< [in] Objpipe pipe source that is to be adapted.
    )
noexcept(noexcept(std::is_nothrow_constructible_v<flatten_op<Source>>))
-> std::enable_if_t<!has_flatten<Source>,
    flatten_op<Source>> {
  static_assert(!std::is_reference_v<Source> && !std::is_const_v<Source>,
      "Source must be an rvalue reference");
  return flatten_op<Source>(std::move(src));
}

/**
 * \brief Adapter for the flatten function.
 * \ingroup objpipe_detail
 *
 * \note Uses the specialization on the source.
 *
 * \return A new source, iterating over all the elements of each value
 * in the argumet source.
 */
template<typename Source>
constexpr auto flatten(
    Source&& src ///< [in] Objpipe pipe source that is to be adapted.
    )
noexcept(noexcept(std::is_nothrow_constructible_v<flatten_op<Source>>))
-> std::enable_if_t<has_flatten<Source>,
    decltype(std::declval<Source>().flatten())> {
  static_assert(!std::is_reference_v<Source> && !std::is_const_v<Source>,
      "Source must be an rvalue reference");
  return std::move(src).flatten();
}


} /* namespace monsoon::objpipe::detail::adapt */

#include <monsoon/objpipe/detail/transform_op.h>
#include <monsoon/objpipe/detail/flatten_op.h>
#include <monsoon/objpipe/detail/filter_op.h>

#endif /* MONSOON_OBJPIPE_DETAIL_ADAPT_H */
