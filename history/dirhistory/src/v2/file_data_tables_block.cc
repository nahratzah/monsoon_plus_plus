#include "file_data_tables_block.h"
#include "file_data_tables.h"
#include "tables.h"
#include <monsoon/history/dir/hdir_exception.h>

namespace monsoon::history::v2 {


auto decode(const cache_search_type<dictionary, file_data_tables_block>& cst, dictionary::allocator_type alloc)
-> std::shared_ptr<dictionary> {
  auto result = std::allocate_shared<dictionary>(alloc, alloc);
  auto xdr = cst.parent()->get_ctx().new_reader(cst.fptr(), dictionary::is_compressed);
  result->decode_update(xdr);
  if (!xdr.at_end()) throw dirhistory_exception("xdr data remaining");
  xdr.close();
  return result;
}


file_data_tables_block::~file_data_tables_block() noexcept {}

auto file_data_tables_block::get_dictionary()
-> std::shared_ptr<dictionary> {
  return get_dynamics_cache<dictionary>(shared_from_this(), dict_);
}

auto file_data_tables_block::get_dictionary() const
-> std::shared_ptr<const dictionary> {
  return get_dynamics_cache<dictionary>(shared_from_this(), dict_);
}

auto file_data_tables_block::get_ctx() const
-> const encdec_ctx& {
  return std::shared_ptr<const file_data_tables>(owner_)->get_ctx();
}

auto file_data_tables_block::get() const
-> std::shared_ptr<const tables> {
  return get_dynamics_cache<tables>(std::const_pointer_cast<file_data_tables_block>(shared_from_this()), tables_);
}

auto file_data_tables_block::decode(xdr::xdr_istream& in)
-> void {
  timestamps_.decode(in);
  dict_.decode(in);
  tables_.decode(in);
}

auto file_data_tables_block::encode(xdr::xdr_ostream& in) const
-> void {
  timestamps_.encode(in);
  dict_.encode(in);
  tables_.encode(in);
}

auto file_data_tables_block::shared_from_this()
-> std::shared_ptr<file_data_tables_block> {
  return std::shared_ptr<file_data_tables_block>(
      std::shared_ptr<file_data_tables>(owner_),
      this);
}

auto file_data_tables_block::shared_from_this() const
-> std::shared_ptr<const file_data_tables_block> {
  return std::shared_ptr<const file_data_tables_block>(
      std::shared_ptr<file_data_tables>(owner_),
      this);
}


} /* namespace monsoon::history::v2 */
