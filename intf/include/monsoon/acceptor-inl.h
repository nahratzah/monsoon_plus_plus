#ifndef MONSOON_ACCEPTOR_INL_H
#define MONSOON_ACCEPTOR_INL_H

namespace monsoon {


template<typename... Types>
acceptor<Types...>::~acceptor() noexcept {}

template<typename... Types>
void acceptor<Types...>::accept_speculative(time_point,
    std::add_lvalue_reference_t<std::add_const_t<Types>>...) {
  /* SKIP */
}


} /* namespace monsoon */

#endif /* MONSOON_ACCEPTOR_INL_H */
