#include "file_data_tables_block.h"
#include "dictionary.h"
#include "tables.h"
#include <monsoon/history/dir/hdir_exception.h>

namespace monsoon::history::v2 {


file_data_tables_block::~file_data_tables_block() noexcept {}

auto file_data_tables_block::get_dictionary()
-> std::shared_ptr<dictionary> {
  auto xdr = get_ctx().new_reader(dict_);
  std::shared_ptr<dictionary> dict = std::make_shared<dictionary>();
  dict->decode_update(xdr);
  if (!xdr.at_end()) throw dirhistory_exception("xdr data remaining");
  xdr.close();
  return dict;
}

auto file_data_tables_block::get_dictionary() const
-> std::shared_ptr<const dictionary> {
  return const_cast<file_data_tables_block&>(*this).get_dictionary();
}

auto file_data_tables_block::get() const
-> std::shared_ptr<const tables> {
  auto xdr = get_ctx().new_reader(tables_);
  std::shared_ptr<tables> tbl = tables::from_xdr(std::const_pointer_cast<file_data_tables_block>(shared_from_this()), xdr);
  if (!xdr.at_end()) throw dirhistory_exception("xdr data remaining");
  xdr.close();
  return tbl;
}

auto file_data_tables_block::from_xdr(
    std::shared_ptr<void> parent,
    xdr::xdr_istream& in,
    encdec_ctx ctx)
-> std::shared_ptr<file_data_tables_block> {
  auto result = std::make_shared<file_data_tables_block>(std::move(parent), std::move(ctx));
  result->decode(in);
  return result;
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


} /* namespace monsoon::history::v2 */
