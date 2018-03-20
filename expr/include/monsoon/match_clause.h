#ifndef MONSOON_MATCH_CLAUSE_H
#define MONSOON_MATCH_CLAUSE_H

///\file
///\brief A match clause for vector operations.
///\ingroup expr

#include <monsoon/expr_export_.h>
#include <monsoon/tags.h>
#include <string>
#include <vector>
#include <unordered_set>
#include <utility>
#include <type_traits>
#include <cassert>

namespace monsoon {


/**
 * \brief Describe which tags to keep on an expression with multiple tagged values.
 * \ingroup intf
 *
 * \details This is used to decide behaviour when two or more tagged values
 * are present in an expression.
 * \relates by_match_clause
 */
enum class match_clause_keep {
  ///\brief Only keep selected tags.
  selected,
  ///\brief Use tags from left most argument.
  ///\note Only for binary operators.
  left,
  ///\brief Use tags from right most argument.
  ///\note Only for binary operators.
  right,
  ///\brief Use all tags that are the same across arguments.
  common
};

/**
 * \brief A match clause.
 * \ingroup expr
 *
 * \details
 * Match clauses test if a pair of \ref monsoon::tags "tag sets" match.
 * Matched tag sets are used to match values together in binary operations.
 */
class monsoon_expr_export_ match_clause {
 public:
  ///\brief Destructor.
  virtual ~match_clause() noexcept;

  /**
   * \brief Check if the \ref monsoon::tags "tag set" has correct tags.
   * This predicate checks if the tag names in \p x
   * are the ones this match clause requires.
   *
   * If this function returns false, the tags shall not be passed
   * to other match clause functions.
   * \return True if the tag names in \p x are sufficient
   * for the match clause to operate on.
   */
  virtual bool pass(const tags& x) const noexcept = 0;

  /**
   * \brief Less comparison on two tags.
   * Yields true if x comes before y,
   * taking into account the configuration of the match clause.
   * \return True if x is ordered before y.
   */
  virtual bool less_cmp(const tags& x, const tags& y) const noexcept = 0;

  /**
   * \brief Reduces tag set.
   * \return A merged tag set, constructed from the two input tag sets.
   */
  virtual tags reduce(const tags& x, const tags& y) const = 0;

  /**
   * \brief Computes a hash value for tag set.
   * \return A hash value for the tag set.
   */
  virtual std::size_t hash(const tags& x) const noexcept = 0;

  /**
   * \brief Compare two tag sets for equality.
   * \return True if the tag sets are equal,
   * taking into account the configuration of the match clause.
   */
  virtual bool eq_cmp(const tags& x, const tags& y) const noexcept = 0;

  class hash {
   public:
    hash(std::shared_ptr<const match_clause> mc) noexcept
    : mc(std::move(mc))
    {
      assert(this->mc != nullptr);
    }

    std::size_t operator()(const tags& x) const noexcept {
      assert(this->mc != nullptr);
      return mc->hash(x);
    }

    bool operator==(const class hash& o) const noexcept {
      return mc == o.mc;
    }

    bool operator!=(const class hash& o) const noexcept {
      return !(*this == o);
    }

    std::shared_ptr<const match_clause> mc;
  };

  class equal_to {
   public:
    equal_to(std::shared_ptr<const match_clause> mc) noexcept
    : mc(std::move(mc))
    {
      assert(this->mc != nullptr);
    }

    bool operator()(const tags& x, const tags& y) const noexcept {
      assert(this->mc != nullptr);
      return mc->eq_cmp(x, y);
    }

    bool operator==(const equal_to& o) const noexcept {
      return mc == o.mc;
    }

    bool operator!=(const equal_to& o) const noexcept {
      return !(*this == o);
    }

    std::shared_ptr<const match_clause> mc;
  };
};

/**
 * \brief A by match clause.
 * \ingroup expr
 *
 * \details A by match clause if of the form:
 * \code
 * by (list, of, tag, names)
 * by (list, of, tag, names) keep selected
 * by (list, of, tag, names) keep left
 * by (list, of, tag, names) keep right
 * by (list, of, tag, names) keep common
 * \endcode
 *
 * The tag names are used to join two or more series together for an operation.
 * The keep specifier decides which tag names are kept.
 */
class monsoon_expr_export_ by_match_clause final
: public match_clause
{
 public:
  /**
   * \brief Constructor for by match clause.
   *
   * \details Constructs a by match clause, using the supplied name list
   * and a \ref match_clause_keep "keep specification".
   * \tparam Iter An input iterator, containing elements convertible to std::string.
   * \param [b,e] Begin and end iterators on a range of names.
   * \param keep Decide in how the argument tags are reduced.
   */
  template<typename Iter>
  by_match_clause(
      Iter b, Iter e,
      match_clause_keep keep = match_clause_keep::selected)
  : tag_names_(b, e),
    keep_(keep)
  {
    fixup_();
  }

  ~by_match_clause() noexcept override;

  ///\copydoc match_clause::pass(const tags&);
  bool pass(const tags& x) const noexcept override;
  ///\copydoc match_clause::less_cmp(const tags&, const tags&);
  bool less_cmp(const tags& x, const tags& y) const noexcept override;
  ///\copydoc match_clause::reduce(const tags&, const tags&);
  tags reduce(const tags& x, const tags& y) const override;
  ///\copydoc match_clause::hash(const tags&);
  virtual std::size_t hash(const tags& x) const noexcept override;
  ///\copydoc match_clause::eq_cmp(const tags&, const tags&);
  virtual bool eq_cmp(const tags& x, const tags& y) const noexcept override;

 private:
  void fixup_() noexcept;

  std::vector<std::string> tag_names_; // Sorted vector.
  match_clause_keep keep_ = match_clause_keep::selected;
};

/**
 * \brief A without match clause.
 * \ingroup expr
 *
 * \details A without match clause if of the form:
 * \code
 * without (list, of, tag, names)
 * \endcode
 *
 * The tag names are excluded during comparison, for joining two or more series together.
 * The kept tags are all those that are not excluded.
 */
class monsoon_expr_export_ without_match_clause final
: public match_clause
{
 public:
  /**
   * \brief Constructor.
   * \ingroup expr
   *
   * \details Constructs a without match clause, using the specified name list.
   * \tparam Iter An input iterator, containing elements convertible to std::string.
   * \param [b,e] Begin and end iterators on a range of names.
   */
  template<typename Iter>
  without_match_clause(Iter b, Iter e)
  : tag_names_(b, e)
  {}

  ~without_match_clause() noexcept override;

  ///\copydoc match_clause::pass(const tags&);
  bool pass(const tags& x) const noexcept override;
  ///\copydoc match_clause::less_cmp(const tags&, const tags&);
  bool less_cmp(const tags& x, const tags& y) const noexcept override;
  ///\copydoc match_clause::reduce(const tags&, const tags&);
  tags reduce(const tags& x, const tags& y) const override;
  ///\copydoc match_clause::hash(const tags&);
  virtual std::size_t hash(const tags& x) const noexcept override;
  ///\copydoc match_clause::eq_cmp(const tags&, const tags&);
  virtual bool eq_cmp(const tags& x, const tags& y) const noexcept override;

 private:
  std::unordered_set<std::string, std::hash<std::string_view>, std::equal_to<>> tag_names_;
};

/**
 * \brief Default match clause.
 * \ingroup expr
 *
 * \details The default match clause.
 *
 * It joins together time series where both sides have the same \ref tags "tag set".
 *
 * Note that there is no \ref match_clause_keep "keep specification":
 * there's no need, as both sides are the same tag sets, so all cases of
 * the \ref match_clause_keep "keep specification" would yield
 * the same \ref tags "tag set".
 */
class monsoon_expr_export_ default_match_clause final
: public match_clause
{
 public:
  ~default_match_clause() noexcept override;

  ///\copydoc match_clause::pass(const tags&);
  bool pass(const tags& x) const noexcept override;
  ///\copydoc match_clause::less_cmp(const tags&, const tags&);
  bool less_cmp(const tags& x, const tags& y) const noexcept override;
  ///\copydoc match_clause::reduce(const tags&, const tags&);
  tags reduce(const tags& x, const tags& y) const override;
  ///\copydoc match_clause::hash(const tags&);
  virtual std::size_t hash(const tags& x) const noexcept override;
  ///\copydoc match_clause::eq_cmp(const tags&, const tags&);
  virtual bool eq_cmp(const tags& x, const tags& y) const noexcept override;
};


} /* namespace monsoon */

#endif /* MONSOON_MATCH_CLAUSE_H */
