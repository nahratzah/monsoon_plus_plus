#ifndef MONSOON_CONFIG_SUPPORT_H
#define MONSOON_CONFIG_SUPPORT_H

#include <string>

namespace monsoon {


std::string quoted_string(const std::string&);
std::string maybe_quote_identifier(const std::string&);


} /* namespace monsoon */

#endif /* MONSOON_CONFIG_SUPPORT_H */