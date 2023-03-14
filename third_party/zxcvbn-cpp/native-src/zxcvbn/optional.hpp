/* A lightweight version of C++1y optional */

#ifndef __ZXCVBN__OPTIONAL_HPP
#define __ZXCVBN__OPTIONAL_HPP

#include <functional>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <cstdint>
#include <cassert>

namespace zxcvbn {

namespace optional {

class nullopt_t {
 public:
  constexpr nullopt_t() {}
};

constexpr nullopt_t nullopt;

class bad_optional_access : public std::logic_error {
 public:
  bad_optional_access(const char *a) : std::logic_error(a) {}
};

template <class T>
class optional {
  static_assert(!std::is_reference<T>::value, "cannot use optional with a reference type!");

  union {
    char _null_state;
    T _val;
  };
  bool _engaged;

  template<class Optional>
  void _init(Optional && o) {
    if (o) {
      new (this) optional(*std::forward<Optional>(o));
    }
    else {
      new (this) optional();
    }
  }

  template<class T2>
  optional & _assign(T2 && val) {
    this->~optional();
    new (this) optional(std::forward<T2>(val));
    return *this;
  }

 public:
  constexpr optional() : _null_state('\0'), _engaged(false) {}

  constexpr optional(nullopt_t) : optional() {}

  constexpr optional(const T & val) : _val(val), _engaged(true) {}
  constexpr optional(T && val) : _val(std::move(val)), _engaged(true) {}

  optional(const optional & val) {
    _init(val);
  }

  optional(optional && val) {
    _init(std::move(val));
  }

  template<class U,
           std::enable_if_t<std::is_same<std::decay_t<U>, T>::value> * = nullptr>
  optional & operator=(U && val) {
    return _assign(std::forward<U>(val));
  }

  optional & operator=(const optional & val) {
    return _assign(val);
  }

  optional &operator=(optional &&val) {
    return _assign(std::move(val));
  }

  ~optional() {
    if (_engaged) {
      _val.~T();
    }
  }

  constexpr const T *operator->() const {
    return &_val;
  }

  constexpr T *operator->() {
    return &_val;
  }

  constexpr const T & operator*() const & {
    return _val;
  }

  constexpr T & operator*() & {
    return _val;
  }

  constexpr T && operator*() const && {
    return std::move(_val);
  }

  constexpr T && operator*() && {
    return std::move(_val);
  }

  constexpr explicit operator bool() const { return _engaged; }
};

template <class T>
constexpr bool operator==(optional<T> f, nullopt_t) {
  return !f;
}

template <class T>
constexpr bool operator!=(optional<T> f, nullopt_t n) {
  return !(f == n);
}

template <class T>
constexpr bool operator==(nullopt_t, optional<T> f) {
  return !f;
}

template <class T>
constexpr bool operator!=(nullopt_t n, optional<T> f) {
  return !(n == f);
}

template <class T>
constexpr bool operator==(optional<T> a, optional<T> b) {
  return a && b ? *a == *b : !a && !b;
}

template <class T>
constexpr bool operator!=(optional<T> a, optional<T> b) {
  return !(a == b);
}

template <class T>
constexpr optional<typename std::decay<T>::type> make_optional(T &&value) {
  return optional<typename std::decay<T>::type>(std::forward<T>(value));
}

}

}

namespace std {

template<class T>
struct hash<zxcvbn::optional::optional<T>> {
  std::size_t operator()(const zxcvbn::optional::optional<T> & v) const {
    if (!v) return 0;
    return std::hash<T>{}(*v);
  }
};

}

#endif
