#include "file_data_tables.h"
#include "file_data_tables_block.h"

namespace monsoon::history::v2 {


file_data_tables::~file_data_tables() noexcept {}

auto file_data_tables::decode(xdr::xdr_istream& in)
-> void {
  blocks_.clear();
  in.get_collection(
      [this](xdr::xdr_istream& in) {
        file_data_tables_block fdt(shared_from_this());
        fdt.decode(in);
        return fdt;
      },
      blocks_);
}

auto file_data_tables::encode(xdr::xdr_ostream& out) const
-> void {
  out.put_collection(
      [](xdr::xdr_ostream& out, const file_data_tables_block& block) {
        block.encode(out);
      },
      blocks_.begin(), blocks_.end());
}


} /* namespace monsoon::history::v2 */
