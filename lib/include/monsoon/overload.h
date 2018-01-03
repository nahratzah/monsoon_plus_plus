#ifndef MONSOON_OVERLOAD_H
#define MONSOON_OVERLOAD_H

#include <type_traits>
#include <utility>
#include <functional>

namespace monsoon {
namespace support {


template<typename Ptr> class overload_base_fnptr_;

template<typename Result, typename... Args>
class overload_base_fnptr_<Result (*)(Args...)> {
 private:
  using fn_type = Result (*)(Args...);

 public:
  constexpr overload_base_fnptr_(fn_type fn)
  : fn_(fn)
  {}

  constexpr Result operator()(Args... args) const {
    return std::invoke(fn_, std::forward<Args>(args)...);
  }

 private:
  fn_type fn_;
};

template<typename Result, typename Class>
class overload_base_fnptr_<Result (Class::*)> {
 private:
  using fn_type = Result (Class::*);

 public:
  constexpr overload_base_fnptr_(fn_type fn)
  : fn_(fn)
  {}

  constexpr auto operator()(Class& cls) const
  -> std::add_lvalue_reference_t<Result> {
    return std::invoke(fn_, cls);
  }

  constexpr auto operator()(const Class& cls) const
  -> std::add_lvalue_reference_t<std::add_const_t<Result>> {
    return std::invoke(fn_, cls);
  }

  constexpr auto operator()(Class&& cls) const
  -> std::add_rvalue_reference_t<Result> {
    return std::invoke(fn_, std::move(cls));
  }

 private:
  fn_type fn_;
};

template<typename Result, typename Class, typename... Args>
class overload_base_fnptr_<Result (Class::*)(Args...)> {
 private:
  using fn_type = Result (Class::*)(Args...);

 public:
  constexpr overload_base_fnptr_(fn_type fn)
  : fn_(fn)
  {}

  constexpr Result operator()(Class& cls, Args... args) const {
    return std::invoke(fn_, cls, std::forward<Args>(args)...);
  }

  constexpr Result operator()(Class&& cls, Args... args) const {
    return std::invoke(fn_, cls, std::forward<Args>(args)...);
  }

 private:
  fn_type fn_;
};

template<typename Result, typename Class, typename... Args>
class overload_base_fnptr_<Result (Class::*)(Args...) const> {
 private:
  using fn_type = Result (Class::*)(Args...) const;

 public:
  constexpr overload_base_fnptr_(fn_type fn)
  : fn_(fn)
  {}

  constexpr Result operator()(const Class& cls, Args... args) const {
    return std::invoke(fn_, cls, std::forward<Args>(args)...);
  }

 private:
  fn_type fn_;
};

template<typename Result, typename Class, typename... Args>
class overload_base_fnptr_<Result (Class::*)(Args...) const &> {
 private:
  using fn_type = Result (Class::*)(Args...) const &;

 public:
  constexpr overload_base_fnptr_(fn_type fn)
  : fn_(fn)
  {}

  constexpr Result operator()(const Class& cls, Args... args) const {
    return std::invoke(fn_, cls, std::forward<Args>(args)...);
  }

 private:
  fn_type fn_;
};

template<typename Result, typename Class, typename... Args>
class overload_base_fnptr_<Result (Class::*)(Args...) &> {
 private:
  using fn_type = Result (Class::*)(Args...) &;

 public:
  constexpr overload_base_fnptr_(fn_type fn)
  : fn_(fn)
  {}

  constexpr Result operator()(Class& cls, Args... args) const {
    return std::invoke(fn_, cls, std::forward<Args>(args)...);
  }

 private:
  fn_type fn_;
};

template<typename Result, typename Class, typename... Args>
class overload_base_fnptr_<Result (Class::*)(Args...) &&> {
 private:
  using fn_type = Result (Class::*)(Args...) &&;

 public:
  constexpr overload_base_fnptr_(fn_type fn)
  : fn_(fn)
  {}

  constexpr Result operator()(Class&& cls, Args... args) const {
    return std::invoke(fn_, std::move(cls), std::forward<Args>(args)...);
  }

 private:
  fn_type fn_;
};


template<typename Fn>
using overload_base_t = std::conditional_t<
    std::is_pointer_v<std::remove_reference_t<Fn>>
    || std::is_member_pointer_v<std::remove_reference_t<Fn>>,
    overload_base_fnptr_<std::remove_reference_t<Fn>>,
    std::conditional_t<
        std::is_rvalue_reference_v<Fn>,
        std::remove_reference_t<Fn>,
        std::conditional_t<
            std::is_lvalue_reference_v<Fn>,
            std::reference_wrapper<std::remove_reference_t<Fn>>,
            Fn>>>;


template<typename... Fn>
class overload_t
: private overload_base_t<Fn>...
{
 public:
  template<typename... FnInit>
  constexpr overload_t(FnInit&&... fn)
  : overload_base_t<Fn>(std::forward<FnInit>(fn))...
  {}

  using overload_base_t<Fn>::operator()...;
};

template<typename... Fn>
constexpr auto overload(Fn&&... fn)
-> overload_t<Fn...> {
  return overload_t<Fn...>(std::forward<Fn>(fn)...);
}


} /* namespace monsoon::support */


using support::overload;


} /* namespace monsoon */

#endif /* MONSOON_OVERLOAD_H */
