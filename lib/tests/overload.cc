#include <monsoon/overload.h>
#include "UnitTest++/UnitTest++.h"
#include <variant>

using namespace monsoon;

std::string int_as_str(int i) { return std::to_string(i); }
std::string str_as_str(const std::string& s) { return s; }

struct struct_a {
  std::string a = "a";

  std::string a_fun() { return "a"; }
  std::string a_fun_const() const { return "a"; }
  std::string a_ref_only() & { return "a"; }
  std::string a_rval_only() && { return "a"; }
  std::string a_constref_only() const & { return "a"; }
};

struct struct_b {
  std::string b = "b";

  std::string b_fun() { return "b"; }
  std::string b_fun_const() { return "b"; }
  std::string b_ref_only() & { return "b"; }
  std::string b_rval_only() && { return "b"; }
  std::string b_constref_only() const & { return "b"; }
};

TEST(lambda_visit) {
  CHECK_EQUAL("14",
      std::visit(
          overload(
              [](const int& i) { return std::to_string(i); },
              [](const std::string& s) { return s; }),
          std::variant<int, std::string>(14)));

  CHECK_EQUAL("foo",
      std::visit(
          overload(
              [](const int& i) { return std::to_string(i); },
              [](const std::string& s) { return s; }),
          std::variant<int, std::string>("foo")));
}

TEST(function_ptr) {
  CHECK_EQUAL("14",
      std::visit(
          overload(
              &int_as_str,
              &str_as_str),
          std::variant<int, std::string>(14)));

  CHECK_EQUAL("foo",
      std::visit(
          overload(
              &int_as_str,
              &str_as_str),
          std::variant<int, std::string>("foo")));
}

TEST(functor_class) {
  struct int_functor {
    std::string operator()(int i) { return std::to_string(i); }
  };
  struct str_functor {
    str_functor() {}
    str_functor(const str_functor&) = delete;
    str_functor(str_functor&&) = delete;
    str_functor& operator=(const str_functor&) = delete;
    str_functor& operator=(str_functor&&) = delete;

    std::string operator()(std::string s) { return s; } // non const only
  };
  struct str_functor_const {
    str_functor_const() {}
    str_functor_const(const str_functor_const&) = delete;
    str_functor_const(str_functor_const&&) = delete;
    str_functor_const& operator=(const str_functor_const&) = delete;
    str_functor_const& operator=(str_functor_const&&) = delete;

    std::string operator()(std::string s) const { return s; } // const only
  };
  str_functor str_functor_ref;
  const str_functor_const str_functor_const_ref;

  CHECK_EQUAL("14",
      std::visit(
          overload(
              int_functor(), // rvalue
              str_functor_ref), // lvalue reference
          std::variant<int, std::string>(14)));

  CHECK_EQUAL("foo",
      std::visit(
          overload(
              int_functor(), // rvalue
              str_functor_ref), // lvalue reference
          std::variant<int, std::string>("foo")));

  CHECK_EQUAL("14",
      std::visit(
          overload(
              int_functor(), // rvalue
              str_functor_const_ref), // lvalue const reference
          std::variant<int, std::string>(14)));

  CHECK_EQUAL("foo",
      std::visit(
          overload(
              int_functor(), // rvalue
              str_functor_const_ref), // lvalue const reference
          std::variant<int, std::string>("foo")));
}

TEST(member_variable) {
  // lvalue reference
  auto tmp_a = std::variant<struct_a, struct_b>(struct_a());
  auto tmp_b = std::variant<struct_a, struct_b>(struct_b());
  CHECK_EQUAL("a",
      std::visit(
          overload(
              &struct_a::a,
              &struct_b::b),
          tmp_a));
  CHECK_EQUAL("b",
      std::visit(
          overload(
              &struct_a::a,
              &struct_b::b),
          tmp_b));

  // lvalue const reference
  const auto ctmp_a = std::variant<struct_a, struct_b>(struct_a());
  const auto ctmp_b = std::variant<struct_a, struct_b>(struct_b());
  CHECK_EQUAL("a",
      std::visit(
          overload(
              &struct_a::a,
              &struct_b::b),
          ctmp_a));
  CHECK_EQUAL("b",
      std::visit(
          overload(
              &struct_a::a,
              &struct_b::b),
          ctmp_b));

  // rvalue reference
  CHECK_EQUAL("a",
      std::visit(
          overload(
              &struct_a::a,
              &struct_b::b),
          std::variant<struct_a, struct_b>(struct_a())));
  CHECK_EQUAL("b",
      std::visit(
          overload(
              &struct_a::a,
              &struct_b::b),
          std::variant<struct_a, struct_b>(struct_b())));
}

TEST(member_function_on_nonconst) {
  CHECK_EQUAL("a",
      std::visit(
          overload(
              &struct_a::a_fun,
              &struct_b::b_fun),
          std::variant<struct_a, struct_b>(struct_a())));
  CHECK_EQUAL("b",
      std::visit(
          overload(
              &struct_a::a_fun,
              &struct_b::b_fun),
          std::variant<struct_a, struct_b>(struct_b())));

  CHECK_EQUAL("a",
      std::visit(
          overload(
              &struct_a::a_fun_const,
              &struct_b::b_fun_const),
          std::variant<struct_a, struct_b>(struct_a())));
  CHECK_EQUAL("b",
      std::visit(
          overload(
              &struct_a::a_fun_const,
              &struct_b::b_fun_const),
          std::variant<struct_a, struct_b>(struct_b())));

  auto tmp_a = std::variant<struct_a, struct_b>(struct_a());
  auto tmp_b = std::variant<struct_a, struct_b>(struct_b());
  CHECK_EQUAL("a",
      std::visit(
          overload(
              &struct_a::a_ref_only,
              &struct_b::b_ref_only),
          tmp_a));
  CHECK_EQUAL("b",
      std::visit(
          overload(
              &struct_a::a_ref_only,
              &struct_b::b_ref_only),
          tmp_b));

  CHECK_EQUAL("a",
      std::visit(
          overload(
              &struct_a::a_rval_only,
              &struct_b::b_rval_only),
          std::variant<struct_a, struct_b>(struct_a())));
  CHECK_EQUAL("b",
      std::visit(
          overload(
              &struct_a::a_rval_only,
              &struct_b::b_rval_only),
          std::variant<struct_a, struct_b>(struct_b())));
}

TEST(member_function_on_const) {
  CHECK_EQUAL("a",
      std::visit(
          overload(
              &struct_a::a_fun_const,
              &struct_b::b_fun_const),
          std::variant<struct_a, struct_b>(struct_a())));
  CHECK_EQUAL("b",
      std::visit(
          overload(
              &struct_a::a_fun_const,
              &struct_b::b_fun_const),
          std::variant<struct_a, struct_b>(struct_b())));

  auto tmp_a = std::variant<struct_a, struct_b>(struct_a());
  auto tmp_b = std::variant<struct_a, struct_b>(struct_b());
  CHECK_EQUAL("a",
      std::visit(
          overload(
              &struct_a::a_constref_only,
              &struct_b::b_constref_only),
          tmp_a));
  CHECK_EQUAL("b",
      std::visit(
          overload(
              &struct_a::a_constref_only,
              &struct_b::b_constref_only),
          tmp_b));
}

int main() {
  return UnitTest::RunAllTests();
}
