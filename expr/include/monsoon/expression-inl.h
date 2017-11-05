#ifndef MONSOON_EXPRESSION_INL_H
#define MONSOON_EXPRESSION_INL_H

namespace monsoon {


template<typename Expr, typename... Args>
expression_ptr expression::make_ptr(Args&&... args) {
  return std::make_unique<Expr>(std::forward<Args>(args)...);
}

inline expression::expression(precedence level) noexcept
: level(level)
{}


inline bool operator!=(
    const expression::scalar_emit_type& x,
    const expression::scalar_emit_type& y) noexcept {
  return !(x == y);
}

inline bool operator!=(
    const expression::vector_emit_type& x,
    const expression::vector_emit_type& y) noexcept {
  return !(x == y);
}


} /* namespace monsoon */

#endif /* MONSOON_EXPRESSION_INL_H */
