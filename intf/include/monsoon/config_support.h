#ifndef MONSOON_CONFIG_SUPPORT_H
#define MONSOON_CONFIG_SUPPORT_H

#include <monsoon/intf_export_.h>
#include <string>

namespace monsoon {


std::string monsoon_intf_export_ quoted_string(const std::string&);
std::string monsoon_intf_export_ maybe_quote_identifier(const std::string&);


} /* namespace monsoon */

#endif /* MONSOON_CONFIG_SUPPORT_H */
