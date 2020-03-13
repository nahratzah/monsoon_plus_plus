#include <monsoon/tx/detail/tree_page.h>
#include <monsoon/tx/tree_error.h>
#include <monsoon/io/rw.h>
#include <algorithm>
#include <boost/endian/conversion.hpp>

namespace monsoon::tx::detail {


void abstract_tree_page_leaf::header::native_to_big_endian() noexcept {
  static_assert(sizeof(header) == header::SIZE);

  boost::endian::native_to_big_inplace(magic);
  boost::endian::native_to_big_inplace(reserved);
  boost::endian::native_to_big_inplace(parent_off);
  boost::endian::native_to_big_inplace(next_sibling_off);
  boost::endian::native_to_big_inplace(prev_sibling_off);
}

void abstract_tree_page_leaf::header::big_to_native_endian() noexcept {
  static_assert(sizeof(header) == header::SIZE);

  boost::endian::big_to_native_inplace(magic);
  boost::endian::big_to_native_inplace(reserved);
  boost::endian::big_to_native_inplace(parent_off);
  boost::endian::big_to_native_inplace(next_sibling_off);
  boost::endian::big_to_native_inplace(prev_sibling_off);
}

void abstract_tree_page_leaf::header::encode(boost::asio::mutable_buffer buf) const {
  assert(buf.size() >= SIZE);
  assert(magic == abstract_tree_page_leaf::magic);

  header tmp = *this;
  tmp.native_to_big_endian();
  boost::asio::buffer_copy(buf, boost::asio::const_buffer(&tmp, sizeof(tmp)));
}

void abstract_tree_page_leaf::header::decode(boost::asio::const_buffer buf) {
  assert(buf.size() >= SIZE);

  boost::asio::buffer_copy(boost::asio::buffer(this, sizeof(*this)), buf);
  big_to_native_endian();
  if (magic != abstract_tree_page_leaf::magic) throw tree_error("bad tree page magic");
}


void abstract_tree_page_branch::header::native_to_big_endian() noexcept {
  static_assert(sizeof(header) == header::SIZE);

  boost::endian::native_to_big_inplace(magic);
  boost::endian::native_to_big_inplace(size);
  boost::endian::native_to_big_inplace(parent_off);
}

void abstract_tree_page_branch::header::big_to_native_endian() noexcept {
  static_assert(sizeof(header) == header::SIZE);

  boost::endian::big_to_native_inplace(magic);
  boost::endian::big_to_native_inplace(size);
  boost::endian::big_to_native_inplace(parent_off);
}

void abstract_tree_page_branch::header::encode(boost::asio::mutable_buffer buf) const {
  assert(buf.size() >= SIZE);
  assert(magic == abstract_tree_page_branch::magic);

  header tmp = *this;
  tmp.native_to_big_endian();
  boost::asio::buffer_copy(buf, boost::asio::const_buffer(&tmp, sizeof(tmp)));
}

void abstract_tree_page_branch::header::decode(boost::asio::const_buffer buf) {
  assert(buf.size() >= SIZE);

  boost::asio::buffer_copy(boost::asio::buffer(this, sizeof(*this)), buf);
  big_to_native_endian();
  if (magic != abstract_tree_page_branch::magic) throw tree_error("bad tree page magic");
}


abstract_tree::~abstract_tree() noexcept = default;


abstract_tree_page::~abstract_tree_page() noexcept = default;

auto abstract_tree_page::tree() const -> cycle_ptr::cycle_gptr<abstract_tree> {
  return cycle_ptr::cycle_gptr<abstract_tree>(tree_);
}

auto abstract_tree_page::decode(
    const txfile::transaction& tx, std::uint64_t off,
    cheap_fn_ref<cycle_ptr::cycle_gptr<abstract_tree_page_leaf>> leaf_constructor,
    cheap_fn_ref<cycle_ptr::cycle_gptr<abstract_tree_page_branch>> branch_constructor)
-> cycle_ptr::cycle_gptr<abstract_tree_page> {
  std::uint32_t magic;
  monsoon::io::read_at(tx, off, &magic, sizeof(magic));
  boost::endian::big_to_native_inplace(magic);

  cycle_ptr::cycle_gptr<abstract_tree_page> page;
  switch (magic) {
    default:
      throw tree_error("bad tree page magic");
    case abstract_tree_page_leaf::magic:
      page = leaf_constructor();
      break;
    case abstract_tree_page_branch::magic:
      page = branch_constructor();
      break;
  }
  page->decode(tx, off);
  return page;
}


abstract_tree_page_leaf::abstract_tree_page_leaf(cycle_ptr::cycle_gptr<abstract_tree> tree)
: abstract_tree_page(std::move(tree)),
  elems_(elems_vector::allocator_type(*this, this->tree()->allocator))
{
  elems_.reserve(cfg->items_per_leaf_page);
}

abstract_tree_page_leaf::abstract_tree_page_leaf(cycle_ptr::cycle_gptr<abstract_tree_page_branch> parent)
: abstract_tree_page(std::move(parent)),
  elems_(elems_vector::allocator_type(*this, this->tree()->allocator))
{
  elems_.reserve(cfg->items_per_leaf_page);
}

abstract_tree_page_leaf::~abstract_tree_page_leaf() noexcept = default;

void abstract_tree_page_leaf::init_empty(std::uint64_t off) {
  assert(elems_.empty());

  off_ = off;
  elems_.resize(cfg->items_per_leaf_page);
}

void abstract_tree_page_leaf::decode(const txfile::transaction& tx, std::uint64_t off) {
  assert(elems_.empty());

  const std::size_t bytes_per_val = tx_aware_data::TX_AWARE_SIZE + cfg->key_bytes + cfg->val_bytes;

  auto buf_storage = std::vector<std::uint8_t>(header::SIZE + cfg->items_per_leaf_page * bytes_per_val);
  monsoon::io::read_at(tx, off, buf_storage.data(), buf_storage.size());
  boost::asio::const_buffer buf = boost::asio::buffer(buf_storage);

  header h;
  h.decode(buf);
  buf += header::SIZE;

  off_ = off;
  parent_off_ = h.parent_off;
  next_sibling_off_ = h.next_sibling_off;
  prev_sibling_off_ = h.prev_sibling_off;
  std::generate_n(
      std::back_inserter(elems_),
      cfg->items_per_leaf_page,
      [&buf, bytes_per_val, this, t = tree()]() -> cycle_ptr::cycle_gptr<abstract_tree_elem> {
        assert(buf.size() >= bytes_per_val);

        auto e = allocate_elem_(t->allocator);
        e->decode(boost::asio::buffer(buf.data(), bytes_per_val));
        buf += bytes_per_val;
        return (e->is_never_visible() ? nullptr : e);
      });

  assert(buf.size() == 0); // Buffer should have been fully consumed.
}

void abstract_tree_page_leaf::encode(txfile::transaction& tx) const {
  assert(!elems_.empty());
  assert(elems_.size() == cfg->items_per_leaf_page);

  const std::size_t bytes_per_val = tx_aware_data::TX_AWARE_SIZE + cfg->key_bytes + cfg->val_bytes;

  auto buf_storage = std::vector<std::uint8_t>(header::SIZE + cfg->items_per_leaf_page * bytes_per_val);
  boost::asio::mutable_buffer buf = boost::asio::buffer(buf_storage);

  header h;
  h.magic = magic;
  h.reserved = 0;
  h.parent_off = parent_off_;
  h.next_sibling_off = next_sibling_off_;
  h.prev_sibling_off = prev_sibling_off_;
  h.encode(buf);
  buf += header::SIZE;

  std::for_each(
      elems_.begin(), elems_.end(),
      [&buf, bytes_per_val](const auto& elem_ptr) {
        assert(buf.size() >= bytes_per_val);

        boost::asio::mutable_buffer elem_buf = boost::asio::buffer(buf.data(), bytes_per_val);
        buf += bytes_per_val;

        if (elem_ptr == nullptr)
          std::memset(elem_buf.data(), 0, elem_buf.size());
        else
          elem_ptr->encode(elem_buf);
      });

  assert(buf.size() == 0); // Buffer should have been fully filled.
  monsoon::io::write_at(tx, off_, buf_storage.data(), buf_storage.size());
}


abstract_tree_page_branch::abstract_tree_page_branch(cycle_ptr::cycle_gptr<abstract_tree> tree)
: abstract_tree_page(std::move(tree)),
  elems_(elems_vector::allocator_type(*this, this->tree()->allocator)),
  keys_(keys_vector::allocator_type(*this, this->tree()->allocator))
{
  elems_.reserve(cfg->items_per_node_page);
}

abstract_tree_page_branch::abstract_tree_page_branch(cycle_ptr::cycle_gptr<abstract_tree_page_branch> parent)
: abstract_tree_page(std::move(parent)),
  elems_(elems_vector::allocator_type(*this, this->tree()->allocator)),
  keys_(keys_vector::allocator_type(*this, this->tree()->allocator))
{
  elems_.reserve(cfg->items_per_node_page);
}

abstract_tree_page_branch::~abstract_tree_page_branch() noexcept = default;

void abstract_tree_page_branch::decode(const txfile::transaction& tx, std::uint64_t off) {
  assert(elems_.empty());

  const std::size_t bytes_per_elem = elem::offset_size + cfg->augment_bytes;
  const std::size_t bytes_per_key = cfg->key_bytes;
  const std::size_t page_bytes = header::SIZE
      + (cfg->items_per_node_page - 1u) * bytes_per_key
      + cfg->items_per_node_page * bytes_per_elem;

  auto buf_storage = std::vector<std::uint8_t>(page_bytes);
  monsoon::io::read_at(tx, off, buf_storage.data(), buf_storage.size());
  boost::asio::const_buffer buf = boost::asio::buffer(buf_storage);

  header h;
  h.decode(buf);
  buf += header::SIZE;
  if (h.size > cfg->items_per_node_page) throw tree_error("too many items in tree branch");

  off_ = off;
  parent_off_ = h.parent_off;

  if (h.size == 0) return;
  const auto t = tree();

  // Decode elems_[0].
  cycle_ptr::cycle_gptr<elem> e = allocate_elem_(t->allocator);
  assert(buf.size() >= bytes_per_elem);
  e->decode(boost::asio::buffer(buf.data(), bytes_per_elem));
  buf += bytes_per_elem;
  elems_.emplace_back(std::move(e));

  cycle_ptr::cycle_gptr<key> k;
  for (std::uint32_t i = 1; i < h.size; ++i) {
    // Decode key separating elems_[i-1] and elems_[i].
    k = allocate_key_(t->allocator);
    assert(buf.size() >= bytes_per_key);
    k->decode(boost::asio::buffer(buf.data(), bytes_per_key));
    buf += bytes_per_key;
    keys_.emplace_back(std::move(k));

    // Decode elems_[i].
    e = allocate_elem_(t->allocator);
    assert(buf.size() >= bytes_per_elem);
    e->decode(boost::asio::buffer(buf.data(), bytes_per_elem));
    buf += bytes_per_elem;
    elems_.emplace_back(std::move(e));
  }

  // Check that the expected not-used bytes is correct.
  assert(buf.size() == (cfg->items_per_node_page - h.size) * (bytes_per_key + bytes_per_elem));
}

void abstract_tree_page_branch::encode(txfile::transaction& tx) const {
  if (elems_.size() > cfg->items_per_node_page) throw tree_error("too many items in tree branch");

  const std::size_t bytes_per_elem = elem::offset_size + cfg->augment_bytes;
  const std::size_t bytes_per_key = cfg->key_bytes;
  const std::size_t page_bytes = header::SIZE
      + (cfg->items_per_node_page - 1u) * bytes_per_key
      + cfg->items_per_node_page * bytes_per_elem;

  auto buf_storage = std::vector<std::uint8_t>(page_bytes);
  boost::asio::mutable_buffer buf = boost::asio::buffer(buf_storage);

  header h;
  h.magic = magic;
  h.size = elems_.size();
  h.parent_off = parent_off_;
  h.encode(buf);
  buf += header::SIZE;

  // Encode elems_[0].
  assert(buf.size() >= bytes_per_elem);
  elems_[0]->encode(boost::asio::buffer(buf.data(), bytes_per_elem));
  buf += bytes_per_elem;

  for (std::uint32_t i = 1; i < elems_.size(); ++i) {
    // Encode key separating elems_[i-1] and elems_[i].
    assert(buf.size() >= bytes_per_key);
    keys_[i - 1]->encode(boost::asio::buffer(buf.data(), bytes_per_key));
    buf += bytes_per_key;

    // Encode elems_[i].
    assert(buf.size() >= bytes_per_elem);
    elems_[1]->encode(boost::asio::buffer(buf.data(), bytes_per_elem));
    buf += bytes_per_elem;
  }

  assert(buf.size() == (cfg->items_per_node_page - h.size) * (bytes_per_key + bytes_per_elem));
  std::memset(buf.data(), 0, buf.size()); // Zero out unused data.
  monsoon::io::write_at(tx, off_, buf_storage.data(), buf_storage.size());
}


abstract_tree_elem::~abstract_tree_elem() noexcept = default;

auto abstract_tree_elem::lock_parent_for_read() const
-> std::tuple<cycle_ptr::cycle_gptr<abstract_tree_page_leaf>, std::shared_lock<std::shared_mutex>> {
  for (;;) {
    std::shared_lock<std::shared_mutex> self_lck{ mtx_ };
    cycle_ptr::cycle_gptr<abstract_tree_page_leaf> p = parent_;
    std::shared_lock<std::shared_mutex> p_lck(p->mtx_, std::try_to_lock);
    if (p_lck.owns_lock()) return std::make_tuple(std::move(p), std::move(p_lck));

    self_lck.unlock();
    p_lck.lock();
    self_lck.lock();
    if (p == parent_) return std::make_tuple(std::move(p), std::move(p_lck));
  }
}

auto abstract_tree_elem::lock_parent_for_write() const
-> std::tuple<cycle_ptr::cycle_gptr<abstract_tree_page_leaf>, std::unique_lock<std::shared_mutex>> {
  for (;;) {
    std::shared_lock<std::shared_mutex> self_lck{ mtx_ };
    cycle_ptr::cycle_gptr<abstract_tree_page_leaf> p = parent_;
    std::unique_lock<std::shared_mutex> p_lck(p->mtx_, std::try_to_lock);
    if (p_lck.owns_lock()) return std::make_tuple(std::move(p), std::move(p_lck));

    self_lck.unlock();
    p_lck.lock();
    self_lck.lock();
    if (p == parent_) return std::make_tuple(std::move(p), std::move(p_lck));
  }
}


} /* namespace monsoon::tx::detail */
