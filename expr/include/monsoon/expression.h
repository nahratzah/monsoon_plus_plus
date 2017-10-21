#ifndef MONSOON_EXPRESSION_H
#define MONSOON_EXPRESSION_H

///\file
///\brief Expression evaluation.
///\ingroup expr

#include <monsoon/expr_export_.h>
#include <monsoon/metric_value.h>
#include <monsoon/tags.h>
#include <monsoon/metric_source.h>
#include <monsoon/time_point.h>
#include <monsoon/time_range.h>
#include <monsoon/objpipe/reader.h>
#include <variant>
#include <memory>
#include <iosfwd>

namespace monsoon {


class expression;

/**
 * \brief A pointer to an expression.
 * \ingroup expr
 *
 * Since \ref monsoon::expression "expression" is an interface, it can only be held in a pointer.
 * This type represents the default pointer for an expression.
 *
 * \todo Figure out if we can simply use std::unique_ptr instead, removing the shared_ptr overhead.
 */
using expression_ptr = std::shared_ptr<const expression>;

/**
 * \brief Expressions represent a computation on zero or more metrics.
 * \ingroup expr
 *
 * Expressions allow for computing over time, using a metric source.
 * @note Expressions are immutable once constructed.
 * @headerfile monsoon/expression.h <monsoon/expression.h>
 */
class monsoon_expr_export_ expression {
  /**
   * \brief Expressions can be textually represented into a stream.
   * \ingroup expr_io
   *
   * \param out The output stream to which to write the textual representation of the expression.
   * \param expr The expression of which the textual representation is to be written.
   * \return out
   */
  friend std::ostream& operator<<(std::ostream& out, const expression& expr);

 public:
  /**
   * \brief The speculative emit type.
   *
   * Speculative emitted values are emitted as early as possible.
   * Being speculative, they may be overriden or invalidated by later emitions.
   */
  using speculative_emit_type =
      std::variant<metric_value, std::tuple<tags, metric_value>>;
  /**
   * \brief A factual emition.
   *
   * Factual emitions are known correct and will never be overriden, nor invalidated.
   * A factual emition will always contain all data for a given timestamp.
   * Speculative emitions shall never have a timestamp at/before the most recent
   * factual emition.
   */
  using factual_emit_type =
      std::variant<metric_value, std::unordered_map<tags, metric_value>>;
  /**
   * \brief The emit type of the evaluation.
   *
   * A mix of speculative and factual emitions are yielded during evaluation.
   */
  using emit_type =
      std::tuple<
          time_point,
          std::variant<speculative_emit_type, factual_emit_type>>;

  /**
   * \brief Create an expression pointer in place.
   *
   * Helper function for expression creation.
   *
   * \tparam Expr The expression type that is to be constructed.
   * \tparam Args The argument types passed to the constructor of Expr.
   * \param args Arguments to be passed to the constructor of the expression type.
   * \return A pointer containing the given expression type,
   *   that was constructed using the argument list.
   */
  template<typename Expr, typename... Args>
      static expression_ptr make_ptr(Args&&... args);

  /**
   * \brief Expressions are destructible.
   */
  virtual ~expression() noexcept;

  /**
   * \brief Evaluate an expression using a \ref metric_source.
   */
  virtual auto operator()(const metric_source&, const time_range&,
      time_point::duration) const
      -> objpipe::reader<emit_type> = 0;

 private:
  virtual void do_ostream(std::ostream&) const = 0;
};

/**
 * \brief Convert an expression to its text representation.
 * \ingroup expr_io
 *
 * The yielded expression is parsable into a new instance of expression.
 *
 * @return The text representation of the expression.
 */
std::string to_string(const expression&);


} /* namespace monsoon */

#include "expression-inl.h"

#endif /* MONSOON_EXPRESSION_H */
