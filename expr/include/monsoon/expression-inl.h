#ifndef MONSOON_EXPRESSION_INL_H
#define MONSOON_EXPRESSION_INL_H

namespace monsoon {


template<typename Expr, typename... Args>
expression_ptr expression::make_ptr(Args&&... args) {
  return std::make_shared<Expr>(std::forward<Args>(args)...);
}


} /* namespace monsoon */

#endif /* MONSOON_EXPRESSION_INL_H */
