#include <monsoon/tx/detail/txfile_allocator.h>
#include <array>
#include <mutex>
#include <boost/endian/conversion.hpp>
#include <boost/polymorphic_pointer_cast.hpp>
#include <boost/polymorphic_cast.hpp>
#include <objpipe/of.h>

namespace monsoon::tx::detail {


void txfile_allocator::key::decode(boost::asio::const_buffer buf) {
  boost::asio::buffer_copy(boost::asio::buffer(&addr, sizeof(addr)), buf);
  boost::endian::big_to_native_inplace(addr);
}

void txfile_allocator::key::encode(boost::asio::mutable_buffer buf) const {
  const auto be_addr = boost::endian::native_to_big(addr);
  boost::asio::buffer_copy(buf, boost::asio::buffer(&be_addr, sizeof(be_addr)));
}


txfile_allocator::element::~element() noexcept = default;

void txfile_allocator::element::decode(boost::asio::const_buffer buf) {
  boost::asio::buffer_copy(
      std::array<boost::asio::mutable_buffer, 3> {{
        boost::asio::buffer(&key.addr, sizeof(key.addr)),
        boost::asio::buffer(&used, sizeof(used)),
        boost::asio::buffer(&free, sizeof(free)),
      }},
      buf);

  boost::endian::big_to_native_inplace(key.addr);
  boost::endian::big_to_native_inplace(used);
  boost::endian::big_to_native_inplace(free);
}

void txfile_allocator::element::encode(boost::asio::mutable_buffer buf) const {
  const auto be_addr = boost::endian::native_to_big(key.addr);
  const auto be_used = boost::endian::native_to_big(used);
  const auto be_free = boost::endian::native_to_big(free);

  boost::asio::buffer_copy(
      buf,
      std::array<boost::asio::const_buffer, 3>{{
        boost::asio::buffer(&be_addr, sizeof(be_addr)),
        boost::asio::buffer(&be_used, sizeof(be_used)),
        boost::asio::buffer(&be_free, sizeof(be_free))
      }});
}

auto txfile_allocator::element::is_never_visible() const noexcept -> bool {
  return used == 0 && free == 0;
}

auto txfile_allocator::element::mtx_ref_() const noexcept -> std::shared_mutex& {
  return mtx_;
}

auto txfile_allocator::element::branch_key_(allocator_type alloc) const -> std::shared_ptr<abstract_tree_page_branch_key> {
  return std::allocate_shared<tree_page_branch_key<class key>>(alloc, key);
}


void txfile_allocator::max_free_space_augment::decode(boost::asio::const_buffer buf) {
  boost::asio::buffer_copy(boost::asio::buffer(&free, sizeof(free)), buf);
  boost::endian::big_to_native_inplace(free);
}

void txfile_allocator::max_free_space_augment::encode(boost::asio::mutable_buffer buf) const {
  const auto be_free = boost::endian::native_to_big(free);
  boost::asio::buffer_copy(buf, boost::asio::buffer(&be_free, sizeof(be_free)));
}


txfile_allocator::txfile_allocator(std::shared_ptr<monsoon::tx::db> db, std::uint64_t off)
: abstract_tree(),
  db_obj(db)
{}

txfile_allocator::~txfile_allocator() noexcept = default;

auto txfile_allocator::compute_augment_(std::uint64_t off, const std::vector<cycle_ptr::cycle_gptr<const abstract_tree_elem>>& elems, allocator_type allocator) const -> std::shared_ptr<abstract_tree_page_branch_elem> {
  auto augments = objpipe::of(std::cref(elems))
      .iterate()
      .filter([](const auto& ptr) noexcept -> bool { return ptr != nullptr; })
      .transform(
          [](const auto& ptr) noexcept -> cycle_ptr::cycle_gptr<const element> {
            return boost::polymorphic_pointer_downcast<const element>(ptr);
          })
      .deref()
      .transform(
          [](const auto& ref) noexcept {
            return std::make_tuple(
                max_free_space_augment(ref));
          })
      .reduce(&txfile_allocator::augment_merge_)
      .value_or(std::tuple<max_free_space_augment>());

  return std::allocate_shared<branch_elem>(allocator, off, std::move(augments));
}

auto txfile_allocator::compute_augment_(std::uint64_t off, const std::vector<std::shared_ptr<const abstract_tree_page_branch_elem>>& elems, allocator_type allocator) const -> std::shared_ptr<abstract_tree_page_branch_elem> {
  auto augments = objpipe::of(std::cref(elems))
      .iterate()
      .filter([](const auto& ptr) noexcept -> bool { return ptr != nullptr; })
      .transform(
          [](const auto& ptr) noexcept -> std::shared_ptr<const branch_elem> {
            return boost::polymorphic_pointer_downcast<const branch_elem>(ptr);
          })
      .transform(
          [](const auto& ptr) noexcept {
            return ptr->augments;
          })
      .reduce(&txfile_allocator::augment_merge_)
      .value_or(std::tuple<max_free_space_augment>());

  return std::allocate_shared<branch_elem>(allocator, off, std::move(augments));
}

auto txfile_allocator::get_if_present(std::uint64_t off) const noexcept -> cycle_ptr::cycle_gptr<abstract_tree_page> {
  return boost::polymorphic_pointer_downcast<abstract_tree_page>(
      obj_cache->get_if_present(off, this->shared_from_this(this)));
}

auto txfile_allocator::get(std::uint64_t off) const -> cycle_ptr::cycle_gptr<abstract_tree_page> {
  return boost::polymorphic_pointer_downcast<abstract_tree_page>(
      obj_cache->get(
          off, this->shared_from_this(this),
          [this](auto&& alloc, auto&& off) {
            return abstract_tree_page::decode(
                const_cast<txfile_allocator&>(*this),
                this->txfile_begin(),
                std::forward<decltype(off)>(off),
                std::forward<decltype(alloc)>(alloc));
          }));
}

void txfile_allocator::invalidate(std::uint64_t off) const noexcept {
  obj_cache->invalidate(off, this->shared_from_this(this));
}

auto txfile_allocator::allocate_elem_(cycle_ptr::cycle_gptr<tree_page_leaf> parent, allocator_type allocator) const -> cycle_ptr::cycle_gptr<abstract_tree_elem> {
  return cycle_ptr::allocate_cycle<element>(allocator, parent);
}

auto txfile_allocator::allocate_branch_elem_(allocator_type allocator) const -> std::shared_ptr<abstract_tree_page_branch_elem> {
  return std::allocate_shared<branch_elem>(allocator);
}

auto txfile_allocator::allocate_branch_key_(allocator_type allocator) const -> std::shared_ptr<abstract_tree_page_branch_key> {
  return std::allocate_shared<tree_page_branch_key<class key>>(allocator);
}

auto txfile_allocator::less_cb(const abstract_tree_page_branch_key&  x, const abstract_tree_page_branch_key&  y) const -> bool {
  auto* cx = boost::polymorphic_cast<const tree_page_branch_key<class key>*>(&x);
  auto* cy = boost::polymorphic_cast<const tree_page_branch_key<class key>*>(&y);
  return cx->key < cy->key;
}

auto txfile_allocator::less_cb(const abstract_tree_elem&             x, const abstract_tree_elem&             y) const -> bool {
  auto* cx = boost::polymorphic_cast<const element*>(&x);
  auto* cy = boost::polymorphic_cast<const element*>(&y);
  return cx->key < cy->key;
}

auto txfile_allocator::less_cb(const abstract_tree_page_branch_key&  x, const abstract_tree_elem&             y) const -> bool {
  auto* cx = boost::polymorphic_cast<const tree_page_branch_key<class key>*>(&y);
  auto* cy = boost::polymorphic_cast<const element*>(&x);
  return cx->key < cy->key;
}

auto txfile_allocator::less_cb(const abstract_tree_elem&             x, const abstract_tree_page_branch_key&  y) const -> bool {
  auto* cx = boost::polymorphic_cast<const element*>(&x);
  auto* cy = boost::polymorphic_cast<const tree_page_branch_key<class key>*>(&y);
  return cx->key < cy->key;
}

auto txfile_allocator::augment_merge_(const std::tuple<max_free_space_augment>& x, const std::tuple<max_free_space_augment>& y) -> std::tuple<max_free_space_augment> {
  return std::make_tuple(
      max_free_space_augment::merge(std::get<0>(x), std::get<0>(y)));
}

void txfile_allocator::decorate_root_page_(const cycle_ptr::cycle_gptr<tree_page_leaf>& root_page, allocator_type tx_allocator) const {
  auto elem = cycle_ptr::allocate_cycle<element>(tx_allocator, root_page);
  elem->key.addr = root_page->offset();
  elem->used = tree_page_leaf::encoded_size(*cfg);
  root_page->elems_.push_back(std::move(elem));
}


} /* namespace monsoon::tx::detail */
