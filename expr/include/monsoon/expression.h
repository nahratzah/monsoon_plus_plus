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
 * \details
 * Since \ref monsoon::expression "expression" is an interface, it can only be held in a pointer.
 * This type represents the default pointer for an expression.
 */
using expression_ptr = std::unique_ptr<const expression>;

/**
 * \brief Expressions represent a computation on zero or more metrics.
 * \ingroup expr
 *
 * \details
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
   * \brief A speculative scalar.
   *
   * \details
   * Speculative emitted values are emitted as early as possible.
   * Being speculative, they may be overriden or invalidated by later emitions.
   */
  using speculative_scalar = metric_value;
  /**
   * \brief A speculative vector.
   *
   * \details
   * Speculative emitted values are emitted as early as possible.
   * Being speculative, they may be overriden or invalidated by later emitions.
   */
  using speculative_vector = std::tuple<tags, metric_value>;
  /**
   * \brief A factual scalar.
   *
   * \details
   * Factual emitions are known correct and will never be overriden, nor invalidated.
   * A factual emition will always contain all data for a given timestamp.
   * Speculative emitions shall never have a timestamp at/before the most recent
   * factual emition.
   */
  using factual_scalar = metric_value;
  /**
   * \brief A factual vector.
   *
   * \details
   * Factual emitions are known correct and will never be overriden, nor invalidated.
   * A factual emition will always contain all data for a given timestamp.
   * Speculative emitions shall never have a timestamp at/before the most recent
   * factual emition.
   *
   * \todo Supply an implementation of hash and equal functors, that uses a
   * tag name set for their computation. Thus allowing fast merging of by-clauses.
   */
  using factual_vector = std::unordered_map<tags, metric_value>;
  /**
   * \brief Emitted scalar values.
   *
   * \details
   * Scalars are untagged values.
   */
  struct scalar_emit_type {
    using data_type = std::variant<speculative_scalar, factual_scalar>;

    time_point tp;
    data_type data;

    template<typename... Args>
    scalar_emit_type(time_point tp, Args&&... args)
    : tp(std::move(tp)),
      data(std::forward<Args>(args)...)
    {}
  };
  /**
   * \brief Emitted vectors.
   *
   * \details
   * Vectors are tagged values.
   */
  struct vector_emit_type {
    using data_type = std::variant<speculative_vector, factual_vector>;

    time_point tp;
    data_type data;

    template<typename... Args>
    vector_emit_type(time_point tp, Args&&... args)
    : tp(std::move(tp)),
      data(std::forward<Args>(args)...)
    {}
  };
  /**
   * \brief An objpipe of scalars.
   */
  using scalar_objpipe = objpipe::reader<scalar_emit_type>;
  /**
   * \brief An objpipe of vectors.
   */
  using vector_objpipe = objpipe::reader<vector_emit_type>;

  /**
   * \brief Create an expression pointer in place.
   *
   * \details
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
      -> std::variant<scalar_objpipe, vector_objpipe> = 0;

  /**
   * \brief Test if the expression is a scalar expression.
   *
   * \note Exactly one of #is_scalar() and #is_vector() will return true.
   */
  virtual bool is_scalar() const noexcept = 0;
  /**
   * \brief Test if the expression is a vector expression.
   *
   * \note Exactly one of #is_scalar() and #is_vector() will return true.
   */
  virtual bool is_vector() const noexcept = 0;

 private:
  virtual void do_ostream(std::ostream&) const = 0;
};

/**
 * \brief Convert an expression to its text representation.
 * \ingroup expr_io
 * \relates expression
 *
 * \details
 * The yielded expression is parsable into a new instance of expression.
 *
 * \return The text representation of the expression.
 */
monsoon_expr_export_
std::string to_string(const expression&);

monsoon_expr_export_
bool operator==(
    const expression::scalar_emit_type& x,
    const expression::scalar_emit_type& y) noexcept;
bool operator!=(
    const expression::scalar_emit_type& x,
    const expression::scalar_emit_type& y) noexcept;

monsoon_expr_export_
bool operator==(
    const expression::vector_emit_type& x,
    const expression::vector_emit_type& y) noexcept;
bool operator!=(
    const expression::vector_emit_type& x,
    const expression::vector_emit_type& y) noexcept;

///\brief Write scalar emit type to output.
///\ingroup expr_io
monsoon_expr_export_
auto operator<<(std::ostream& out, const expression::scalar_emit_type& v)
    -> std::ostream&;
///\brief Write vector emit type to output.
///\ingroup expr_io
monsoon_expr_export_
auto operator<<(std::ostream& out, const expression::vector_emit_type& v)
    -> std::ostream&;


} /* namespace monsoon */

#include "expression-inl.h"

#endif /* MONSOON_EXPRESSION_H */
