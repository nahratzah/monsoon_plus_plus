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


enum class match_clause_keep {
  none,
  left,
  right,
  common
};

/**
 * \brief A match clause.
 * \ingroup expr
 *
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

class monsoon_expr_export_ by_match_clause
: public match_clause
{
 public:
  template<typename Iter>
  by_match_clause(
      Iter b, Iter e,
      match_clause_keep keep = match_clause_keep::none)
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
  match_clause_keep keep_ = match_clause_keep::none;
};

class monsoon_expr_export_ without_match_clause
: public match_clause
{
 public:
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
  std::unordered_set<std::string> tag_names_;
};

class monsoon_expr_export_ default_match_clause
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
