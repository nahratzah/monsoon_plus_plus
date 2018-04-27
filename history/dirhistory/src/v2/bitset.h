#ifndef MONSOON_HISTORY_V2_BITSET_H
#define MONSOON_HISTORY_V2_BITSET_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <vector>
#include <monsoon/xdr/xdr.h>

namespace monsoon::history::v2 {


class monsoon_dirhistory_local_ bitset
: public std::vector<bool>
{
 public:
  using std::vector<bool>::vector;
  using std::vector<bool>::operator=;

  void decode(xdr::xdr_istream& in);
  void encode(xdr::xdr_ostream& out) const;
};


} /* namespace monsoon::history::v2 */

#endif /* MONSOON_HISTORY_V2_BITSET_H */
