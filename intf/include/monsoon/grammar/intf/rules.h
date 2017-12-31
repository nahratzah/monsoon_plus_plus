#ifndef MONSOON_GRAMMAR_INTF_RULES_H
#define MONSOON_GRAMMAR_INTF_RULES_H

#include <monsoon/intf_export_.h>
#include <monsoon/grammar/intf/ast.h>
#include <monsoon/grammar/intf/ast_adapted.h>
#include <boost/spirit/home/x3.hpp>

namespace monsoon {
namespace grammar {
namespace parser {


namespace x3 = boost::spirit::x3;

struct append_utf8 {
  template<typename Ctx>
  void operator()(Ctx& ctx) {
    using str_iter = std::back_insert_iterator<std::string>;
    auto iter = boost::utf8_output_iterator<str_iter>(str_iter(x3::_val(ctx)));
    *iter++ = x3::_attr(ctx);
  }
};

struct append_esc {
  template<typename Ctx>
  void operator()(Ctx& ctx) {
    auto& s = x3::_val(ctx);
    switch (x3::_attr(ctx)) {
      case 'a':
        s += '\a';
        break;
      case 'b':
        s += '\b';
        break;
      case 't':
        s += '\t';
        break;
      case 'n':
        s += '\n';
        break;
      case 'v':
        s += '\v';
        break;
      case 'f':
        s += '\f';
        break;
      case 'r':
        s += '\r';
        break;
      case '\'':
        s += '\'';
        break;
      case '"':
        s += '"';
        break;
      case '\\':
        s += '\\';
        break;
    }
  }
};

inline const auto string =
    x3::rule<class string_, std::string>("string");
inline const auto identifier =
    x3::rule<class identifier, std::string>("identifier");
inline const auto quoted_identifier =
    x3::rule<class quoted_identifier, std::string>("quoted identifier");
inline const auto histogram_range =
    x3::rule<class histogram_range, ast::histogram_range_expr>("histogram range");
inline const auto histogram =
    x3::rule<class histogram_, ast::histogram_expr>("histogram");
inline const auto value =
    x3::rule<class value, ast::value_expr>("value");

inline const auto reject_chars_ =
      x3::char_('\0') // Separate, since \0 is a string termination char.
    | x3::char_(    "\x01\x02\x03\x04\x05\x06\x07"
                "\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
                "\x10\x11\x12\x13\x14\x15\x16\x17"
                "\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f");
inline const auto string_escape =
      x3::uint_parser<std::uint8_t, 8, 1, 3>()[append_utf8()]
    | 'x' >> x3::uint_parser<std::uint8_t, 16, 2, 2>()[append_utf8()]
    | 'u' >> x3::uint_parser<std::uint16_t, 16, 4, 4>()[append_utf8()]
    | 'U' >> x3::uint_parser<std::uint32_t, 16, 8, 8>()[append_utf8()]
    | x3::char_(R"(abtnvfr'"\)")[append_esc()];
inline const auto string_def = x3::lexeme[
    '"' >>
        *( '\\' >> string_escape
         | (~x3::char_(R"(\")") - reject_chars_)[([](auto& ctx){ x3::_val(ctx) += x3::_attr(ctx); })]
         ) >>
    '"'
    ];
inline const auto quoted_identifier_def = x3::lexeme[
    '\'' >>
        *( '\\' >> string_escape
         | (~x3::char_(R"(\')") - reject_chars_)[([](auto& ctx){ x3::_val(ctx) += x3::_attr(ctx); })]
         ) >>
    '\''
    ];
inline const auto identifier_def = x3::lexeme[
    x3::char_("_abcdefghijklmnopqrstuvwxyz") >>
    *x3::char_("_abcdefghijklmnopqrstuvwxyz0123456789")
    ];
inline const auto histogram_range_def =
    x3::double_ >> x3::lit("..") >> x3::double_ >> x3::lit('=') >> x3::double_;
inline const auto histogram_def =
    x3::lit('[') >> -(histogram_range % ',') >> x3::lit(']');
inline const auto value_def =
      x3::bool_
    | x3::uint_parser<metric_value::unsigned_type>()
    | x3::int_parser<metric_value::signed_type>()
    | x3::double_
    | string
    | histogram;

BOOST_SPIRIT_DEFINE(
    string,
    identifier,
    quoted_identifier,
    histogram_range,
    histogram,
    value);


}}} /* namespace monsoon::grammar::parser */

#endif /* MONSOON_GRAMMAR_INTF_RULES_H */
