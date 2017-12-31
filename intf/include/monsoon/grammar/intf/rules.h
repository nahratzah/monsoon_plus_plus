#ifndef MONSOON_GRAMMAR_INTF_RULES_H
#define MONSOON_GRAMMAR_INTF_RULES_H

#include <monsoon/intf_export_.h>
#include <monsoon/grammar/intf/ast.h>
#include <monsoon/grammar/intf/ast_adapted.h>
#include <boost/spirit/home/x3.hpp>
#include <boost/regex/pending/unicode_iterator.hpp>
#include <cstdint>

namespace monsoon {
namespace grammar {
namespace parser {


constexpr unsigned int MAX_UNICODE_CODEPOINT = 0x10ffff;

namespace x3 = boost::spirit::x3;

struct append_utf8 {
  template<typename Ctx>
  void operator()(Ctx& ctx) const {
    const auto& attr = x3::_attr(ctx);
    if (attr > MAX_UNICODE_CODEPOINT) {
      x3::_pass(ctx) = false;
      return;
    }

    using str_iter = std::back_insert_iterator<std::string>;
    auto iter = boost::utf8_output_iterator<str_iter>(str_iter(x3::_val(ctx)));
    *iter++ = attr;
  }
};

struct append_esc {
  template<typename Ctx>
  void operator()(Ctx& ctx) const {
    auto& s = x3::_val(ctx);
    switch (x3::_attr(ctx)) {
      default:
        x3::_pass(ctx) = false;
        break;
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

struct append_char {
  template<typename Ctx>
  void operator()(Ctx& ctx) const {
    const auto& attr = x3::_attr(ctx);
    if (attr < 0x20 || attr > 0x7f) {
      x3::_pass(ctx) = false;
      return;
    }

    x3::_val(ctx) += attr;
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
inline const auto tag_value =
    x3::rule<class tag_value, ast::value_expr>("value");
inline const auto simple_path_lit =
    x3::rule<class simple_path_lit, ast::simple_path_lit_expr>("path");
inline const auto tags_lit =
    x3::rule<class tags_lit, ast::tags_lit_expr>("tags");

inline const auto string_escape =
      x3::uint_parser<std::uint8_t, 8, 1, 3>()[append_utf8()]
    | 'x' >> x3::uint_parser<std::uint8_t, 16, 2, 2>()[append_utf8()]
    | 'u' >> x3::uint_parser<std::uint16_t, 16, 4, 4>()[append_utf8()]
    | 'U' >> x3::uint_parser<std::uint32_t, 16, 8, 8>()[append_utf8()]
    | x3::char_(R"(abtnvfr'"\)")[append_esc()];
inline const auto string_def = x3::lexeme[
    '"' >>
        *( '\\' >> string_escape
         | (~x3::char_(R"(\")"))[append_char()]
         ) >>
    '"'
    ];
inline const auto quoted_identifier_def = x3::lexeme[
    '\'' >>
        *( '\\' >> string_escape
         | (~x3::char_(R"(\')"))[append_char()]
         ) >>
    '\''
    ];
inline const auto identifier_def = x3::lexeme[
    x3::char_("_abcdefghijklmnopqrstuvwxyz") >>
    *x3::char_("_abcdefghijklmnopqrstuvwxyz0123456789")
    ];
inline const auto histogram_range_def =
    ( x3::int_parser<std::intmax_t>() >> &x3::lit("..")
    | x3::real_parser<std::double_t, x3::strict_real_policies<std::double_t>>()
    ) >>
    x3::lit("..") >>
    x3::real_parser<std::double_t>() >>
    x3::lit('=') >>
    x3::real_parser<std::double_t>();
inline const auto histogram_def =
    x3::lit('[') >> -(histogram_range % ',') >> x3::lit(']');
inline const auto value_def =
      x3::bool_
    | x3::uint_parser<metric_value::unsigned_type>() >> &!(x3::lit('.') | x3::lit('e'))
    | x3::int_parser<metric_value::signed_type>() >> &!(x3::lit('.') | x3::lit('e'))
    | x3::real_parser<metric_value::fp_type>()
    | string
    | histogram;
inline const auto tag_value_def =
      x3::bool_
    | x3::uint_parser<metric_value::unsigned_type>() >> &!(x3::lit('.') | x3::lit('e'))
    | x3::int_parser<metric_value::signed_type>() >> &!(x3::lit('.') | x3::lit('e'))
    | x3::real_parser<metric_value::fp_type>()
    | string;
inline const auto tags_lit_def =
    x3::lit('{') >>
    -(( identifier >> x3::lit('=') >> tag_value
      | quoted_identifier >> x3::lit('=') >> tag_value
      ) % ',') >>
    x3::lit('}');
inline const auto simple_path_lit_def = (identifier | quoted_identifier) % '.';

BOOST_SPIRIT_DEFINE(
    string,
    identifier,
    quoted_identifier,
    histogram_range,
    histogram,
    value,
    simple_path_lit,
    tag_value,
    tags_lit);


}}} /* namespace monsoon::grammar::parser */

#endif /* MONSOON_GRAMMAR_INTF_RULES_H */
