#ifndef MONSOON_TX_TREE_H
#define MONSOON_TX_TREE_H

#include <monsoon/tx/detail/tree_spec.h>

namespace monsoon::tx {


template<
    typename Key, typename Val, typename Less = std::less<Key>,
    typename Augments...>
class tree {
  static_assert(monsoon::tx::detail::tree_key_spec<Key>::value);
  static_assert(monsoon::tx::detail::tree_val_spec<Val>::value);
  static_assert(std::is_invocable_r_v<bool, const Less&, const Key&, const Key&>,
      "less operation must be invocable on the key type");
  static_assert(std::conjunction_v<monsoon::tx::detail::tree_augment_spec<Key, Val, Augments>...>);

  public:
  ///\brief Number of bytes taken up by the tree.
  static constexpr std::size_t SIZE = 0;

  tree() = default;

  /**
   * \brief Open an existing tree.
   * \details
   * Reads \p SIZE bytes at offset \p off to figure out the configuration parameters for the tree.
   *
   * \param[in] tx A readable transaction in the file.
   * \param[in] off The offset of the tree.
   */
  tree(const txfile::transaction& t, monsoon::io::fd::offset_type off);

  /**
   * \brief Initialize a new tree.
   * \details
   * Overwrites \p SIZE bytes at offset \p off initializing an empty tree.
   *
   * \note The tree is unusable until the transaction has been committed.
   * \param[in,out] tx A writeable transaction in the file.
   * \param[in] off The offset at which the tree will exist.
   */
  static auto init(
      txfile::transaction& t, monsoon::io::fd::offset_type off,
      bool compressed_pages = false,
      leaf_elems = monsoon::tx::detail::autoconf_tree_leaf_elems(Key::SIZE, Val::SIZE),
      node_elems = monsoon::tx::defail::autoconf_tree_leaf_elems(Key::SIZE, sizeof(std::uint64_t), Augments::SIZE...);
};


} /* namespace monsoon::tx */

#endif /* MONSOON_TX_TREE_H */
