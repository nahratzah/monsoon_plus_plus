#ifndef MONSOON_CONFIG_SUPPORT_H
#define MONSOON_CONFIG_SUPPORT_H

///\file
///\ingroup intf

#include <monsoon/intf_export_.h>
#include <string>
#include <string_view>

namespace monsoon {


/**
 * \brief Put a string between quotes and apply the correct escape sequences.
 * \ingroup intf
 *
 * This function is the inverse of a string parse operation.
 *
 * \param s The string to quote.
 * \return A properly escaped, quoted representation of the string.
 */
monsoon_intf_export_
std::string quoted_string(std::string_view s);

/**
 * \brief Put single quotes and apply correct escaping for an identifier,
 *   unless the identifier is a valid identifier.
 * \ingroup intf
 *
 * This function is the inverse of an identifier parse operation.
 *
 * \note Path segments, as used by \ref simple_group, \ref group_name and
 *   \ref metric_name,
 *   are also identifiers in need of quoting.
 *
 * \param s A string holding the identifier.
 * \return \p s unchanged, if \p s is valid as an unquoted identifier.
 *   Otherwise, a quoted and appropriately escaped representation of the identifier.
 */
monsoon_intf_export_
std::string maybe_quote_identifier(std::string_view);


} /* namespace monsoon */

#endif /* MONSOON_CONFIG_SUPPORT_H */
