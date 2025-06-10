#ifndef DIPLOMAT_RUNTIME_CPP_H
#define DIPLOMAT_RUNTIME_CPP_H

#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <cstdint>
#include <functional>
#include <memory>
#include <limits>


#if __cplusplus >= 202002L
#include <span>
#else
#include <array>
#endif

namespace diplomat {

namespace capi {
extern "C" {

static_assert(sizeof(char) == sizeof(uint8_t), "your architecture's `char` is not 8 bits");
static_assert(sizeof(char16_t) == sizeof(uint16_t), "your architecture's `char16_t` is not 16 bits");
static_assert(sizeof(char32_t) == sizeof(uint32_t), "your architecture's `char32_t` is not 32 bits");

typedef struct DiplomatWrite {
    void* context;
    char* buf;
    size_t len;
    size_t cap;
    bool grow_failed;
    void (*flush)(struct DiplomatWrite*);
    bool (*grow)(struct DiplomatWrite*, size_t);
} DiplomatWrite;

bool diplomat_is_str(const char* buf, size_t len);

#define MAKE_SLICES(name, c_ty) \
    typedef struct Diplomat##name##View { \
        const c_ty* data; \
        size_t len; \
    } Diplomat##name##View; \
    typedef struct Diplomat##name##ViewMut { \
        c_ty* data; \
        size_t len; \
    } Diplomat##name##ViewMut; \
    typedef struct Diplomat##name##Array { \
        const c_ty* data; \
        size_t len; \
    } Diplomat##name##Array;

#define MAKE_SLICES_AND_OPTIONS(name, c_ty) \
    MAKE_SLICES(name, c_ty) \
    typedef struct Option##name {union { c_ty ok; }; bool is_ok; } Option##name; \
    typedef struct Option##name##View {union { Diplomat##name##View ok; }; bool is_ok; } Option##name##View; \
    typedef struct Option##name##ViewMut {union { Diplomat##name##ViewMut ok; }; bool is_ok; } Option##name##ViewMut; \
    typedef struct Option##name##Array {union { Diplomat##name##Array ok; }; bool is_ok; } Option##name##Array; \

MAKE_SLICES_AND_OPTIONS(I8, int8_t)
MAKE_SLICES_AND_OPTIONS(U8, uint8_t)
MAKE_SLICES_AND_OPTIONS(I16, int16_t)
MAKE_SLICES_AND_OPTIONS(U16, uint16_t)
MAKE_SLICES_AND_OPTIONS(I32, int32_t)
MAKE_SLICES_AND_OPTIONS(U32, uint32_t)
MAKE_SLICES_AND_OPTIONS(I64, int64_t)
MAKE_SLICES_AND_OPTIONS(U64, uint64_t)
MAKE_SLICES_AND_OPTIONS(Isize, intptr_t)
MAKE_SLICES_AND_OPTIONS(Usize, size_t)
MAKE_SLICES_AND_OPTIONS(F32, float)
MAKE_SLICES_AND_OPTIONS(F64, double)
MAKE_SLICES_AND_OPTIONS(Bool, bool)
MAKE_SLICES_AND_OPTIONS(Char, char32_t)
MAKE_SLICES_AND_OPTIONS(String, char)
MAKE_SLICES_AND_OPTIONS(String16, char16_t)
MAKE_SLICES_AND_OPTIONS(Strings, DiplomatStringView)
MAKE_SLICES_AND_OPTIONS(Strings16, DiplomatString16View)

} // extern "C"
} // namespace capi

extern "C" inline void _flush(capi::DiplomatWrite* w) {
  std::string* string = reinterpret_cast<std::string*>(w->context);
  string->resize(w->len);
}

extern "C" inline bool _grow(capi::DiplomatWrite* w, uintptr_t requested) {
  std::string* string = reinterpret_cast<std::string*>(w->context);
  string->resize(requested);
  w->cap = string->length();
  w->buf = &(*string)[0];
  return true;
}

inline capi::DiplomatWrite WriteFromString(std::string& string) {
  capi::DiplomatWrite w;
  w.context = &string;
  w.buf = &string[0];
  w.len = string.length();
  w.cap = string.length();
  // Will never become true, as _grow is infallible.
  w.grow_failed = false;
  w.flush = _flush;
  w.grow = _grow;
  return w;
}

template<class T> struct Ok {
  T inner;
  Ok(T&& i): inner(std::forward<T>(i)) {}
  // We don't want to expose an lvalue-capable constructor in general
  // however there is no problem doing this for trivially copyable types
  template<typename X = T, typename = typename std::enable_if<std::is_trivially_copyable<X>::value>::type>
  Ok(T i): inner(i) {}
  Ok() = default;
  Ok(Ok&&) noexcept = default;
  Ok(const Ok &) = default;
  Ok& operator=(const Ok&) = default;
  Ok& operator=(Ok&&) noexcept = default;
};

template<class T> struct Err {
  T inner;
  Err(T&& i): inner(std::forward<T>(i)) {}
  // We don't want to expose an lvalue-capable constructor in general
  // however there is no problem doing this for trivially copyable types
  template<typename X = T, typename = typename std::enable_if<std::is_trivially_copyable<X>::value>::type>
  Err(T i): inner(i) {}
  Err() = default;
  Err(Err&&) noexcept = default;
  Err(const Err &) = default;
  Err& operator=(const Err&) = default;
  Err& operator=(Err&&) noexcept = default;
};

template<class T, class E>
class result {
private:
    std::variant<Ok<T>, Err<E>> val;
public:
  result(Ok<T>&& v): val(std::move(v)) {}
  result(Err<E>&& v): val(std::move(v)) {}
  result() = default;
  result(const result &) = default;
  result& operator=(const result&) = default;
  result& operator=(result&&) noexcept = default;
  result(result &&) noexcept = default;
  ~result() = default;
  bool is_ok() const {
    return std::holds_alternative<Ok<T>>(this->val);
  }
  bool is_err() const {
    return std::holds_alternative<Err<E>>(this->val);
  }

  template<typename U = T, typename std::enable_if_t<!std::is_reference_v<U>, std::nullptr_t> = nullptr>
  std::optional<T> ok() && {
    if (!this->is_ok()) {
      return std::nullopt;
    }
    return std::make_optional(std::move(std::get<Ok<T>>(std::move(this->val)).inner));
  }

  template<typename U = E, typename std::enable_if_t<!std::is_reference_v<U>, std::nullptr_t> = nullptr>
  std::optional<E> err() && {
    if (!this->is_err()) {
      return std::nullopt;
    }
    return std::make_optional(std::move(std::get<Err<E>>(std::move(this->val)).inner));
  }

  // std::optional does not work with reference types directly, so wrap them if present
  template<typename U = T, typename std::enable_if_t<std::is_reference_v<U>, std::nullptr_t> = nullptr>
  std::optional<std::reference_wrapper<std::remove_reference_t<T>>> ok() && {
    if (!this->is_ok()) {
      return std::nullopt;
    }
    return std::make_optional(std::reference_wrapper(std::forward<T>(std::get<Ok<T>>(std::move(this->val)).inner)));
  }

  template<typename U = E, typename std::enable_if_t<std::is_reference_v<U>, std::nullptr_t> = nullptr>
  std::optional<std::reference_wrapper<std::remove_reference_t<E>>> err() && {
    if (!this->is_err()) {
      return std::nullopt;
    }
    return std::make_optional(std::reference_wrapper(std::forward<E>(std::get<Err<E>>(std::move(this->val)).inner)));
  }

  void set_ok(T&& t) {
    this->val = Ok<T>(std::move(t));
  }

  void set_err(E&& e) {
    this->val = Err<E>(std::move(e));
  }

  template<typename T2>
  result<T2, E> replace_ok(T2&& t) {
    if (this->is_err()) {
      return result<T2, E>(Err<E>(std::get<Err<E>>(std::move(this->val))));
    } else {
      return result<T2, E>(Ok<T2>(std::move(t)));
    }
  }
};

class Utf8Error {};

// Use custom std::span on C++17, otherwise use std::span
#if __cplusplus >= 202002L

constexpr std::size_t dynamic_extent = std::dynamic_extent;
template<class T, std::size_t E = dynamic_extent> using span = std::span<T, E>;

#else // __cplusplus < 202002L

// C++-17-compatible-ish std::span
constexpr size_t dynamic_extent = std::numeric_limits<std::size_t>::max();
template <class T, std::size_t Extent = dynamic_extent>
class span {
public:
  constexpr span(T *data = nullptr, size_t size = Extent)
    : data_(data), size_(size) {}

  constexpr span(const span<T> &o)
    : data_(o.data_), size_(o.size_) {}
  template <size_t N>
  constexpr span(std::array<typename std::remove_const_t<T>, N> &arr)
    : data_(const_cast<T *>(arr.data())), size_(N) {}

  constexpr T* data() const noexcept {
    return this->data_;
  }
  constexpr size_t size() const noexcept {
    return this->size_;
  }

  constexpr T *begin() const noexcept { return data(); }
  constexpr T *end() const noexcept { return data() + size(); }

  void operator=(span<T> o) {
    data_ = o.data_;
    size_ = o.size_;
  }

private:
  T* data_;
  size_t size_;
};

#endif // __cplusplus >= 202002L

// Interop between std::function & our C Callback wrapper type

template <typename T, typename = void>
struct as_ffi {
  using type = T;
};

template <typename T>
struct as_ffi<T, std::void_t<decltype(std::declval<std::remove_pointer_t<T>>().AsFFI())>> {
  using type = decltype(std::declval<std::remove_pointer_t<T>>().AsFFI());
};

template<typename T>
using as_ffi_t = typename as_ffi<T>::type;

template<typename T>
using replace_string_view_t = std::conditional_t<std::is_same_v<T, std::string_view>, capi::DiplomatStringView, T>;

template<typename T>
using replace_ref_with_ptr_t = std::conditional_t<std::is_reference_v<T>, std::add_pointer_t<std::remove_reference_t<T>>, T>;

/// Replace the argument types from the std::function with the argument types for th function pointer
template<typename T>
using replace_fn_t = replace_string_view_t<as_ffi_t<T>>;

template <typename T> struct fn_traits;
template <typename Ret, typename... Args> struct fn_traits<std::function<Ret(Args...)>> {
    using fn_ptr_t = Ret(Args...);
    using function_t = std::function<fn_ptr_t>;
    using ret = Ret;

    // For a given T, creates a function that take in the C ABI version & return the C++ type.
    template<typename T>
    static T replace(replace_fn_t<T> val) {
      if constexpr(std::is_same_v<T, std::string_view>)   {
          return std::string_view{val.data, val.len};
      } else if constexpr (!std::is_same_v<T, as_ffi_t<T>>) {
        if constexpr (std::is_lvalue_reference_v<T>) {
          return *std::remove_reference_t<T>::FromFFI(val);
        }
        else {
          return T::FromFFI(val);
        }
      }
      else {
          return val;
      }
    }

    static Ret c_run_callback(const void *cb, replace_fn_t<Args>... args) {
        return (*reinterpret_cast<const function_t *>(cb))(replace<Args>(args)...);
    }

    static void c_delete(const void *cb) {
        delete reinterpret_cast<const function_t *>(cb);
    }

    fn_traits(function_t) {} // Allows less clunky construction (avoids decltype)
};

// additional deduction guide required
template<class T>
fn_traits(T) -> fn_traits<T>;

// Trait for extracting inner types from either std::optional or std::unique_ptr.
// These are the two potential types returned by next() functions
template<typename T> struct inner { using type = T; };
template<typename T> struct inner<std::unique_ptr<T>> { using type = T; };
template<typename T> struct inner<std::optional<T>>{ using type = T; };

template<typename T, typename U = typename inner<T>::type>
inline const U get_inner_if_present(T v) {
  if constexpr(std::is_same_v<T,U>) {
    return std::move(v);
  } else {
    return *std::move(v);
  }
}

// Adapter for iterator types
template<typename T, typename U = void> struct has_next : std::false_type {};
template<typename T> struct has_next < T, std::void_t<decltype(std::declval<T>().next())>> : std::true_type {};
template<typename T> constexpr bool has_next_v = has_next<T>::value;

/// Helper template enabling native iteration over unique ptrs to objects which implement next()
template<typename T>
struct next_to_iter_helper {
  static_assert(has_next_v<T>, "next_to_iter_helper may only be used with types implementing next()");
  using next_type = decltype(std::declval<T>().next());

  // STL Iterator trait definitions
  using value_type = typename inner<next_type>::type;
  using difference_type = void;
  using reference = std::add_lvalue_reference_t<value_type>;
  using iterator_category = std::input_iterator_tag;

  next_to_iter_helper(std::unique_ptr<T>&& ptr) : _ptr(std::move(ptr)), _curr(_ptr->next()) {}

  // https://en.cppreference.com/w/cpp/named_req/InputIterator requires that the type be copyable
  next_to_iter_helper(const next_to_iter_helper& o) : _ptr(o._ptr), _curr(o._curr) {}

  void operator++() { _curr = _ptr->next(); }
  void operator++(int) { ++(*this); }
  const value_type& operator*() const { return *_curr; }

  bool operator!=(std::nullopt_t) {
    return (bool)_curr;
  }

  std::shared_ptr<T> _ptr; // shared to satisfy the copyable requirement
  next_type _curr;
};

} // namespace diplomat

#endif