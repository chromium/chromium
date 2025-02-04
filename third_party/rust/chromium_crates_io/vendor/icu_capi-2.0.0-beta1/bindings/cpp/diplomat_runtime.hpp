#ifndef DIPLOMAT_RUNTIME_CPP_H
#define DIPLOMAT_RUNTIME_CPP_H

#include <optional>
#include <string>
#include <type_traits>
#include <variant>

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
    typedef struct Option##name {union { c_ty ok; }; bool is_ok; } Option##name;

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
MAKE_SLICES(String, char)
MAKE_SLICES(String16, char16_t)
MAKE_SLICES(Strings, DiplomatStringView)
MAKE_SLICES(Strings16, DiplomatString16View)

} // extern "C"
} // namespace capi

extern "C" inline void _flush(capi::DiplomatWrite* w) {
  std::string* string = reinterpret_cast<std::string*>(w->context);
  string->resize(w->len);
};

extern "C" inline bool _grow(capi::DiplomatWrite* w, uintptr_t requested) {
  std::string* string = reinterpret_cast<std::string*>(w->context);
  string->resize(requested);
  w->cap = string->length();
  w->buf = &(*string)[0];
  return true;
};

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
};

template<class T> struct Ok {
  T inner;
  Ok(T&& i): inner(std::move(i)) {}
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
  Err(T&& i): inner(std::move(i)) {}
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
  };
  bool is_err() const {
    return std::holds_alternative<Err<E>>(this->val);
  };

  std::optional<T> ok() && {
    if (!this->is_ok()) {
      return std::nullopt;
    }
    return std::make_optional(std::move(std::get<Ok<T>>(std::move(this->val)).inner));
  };
  std::optional<E> err() && {
    if (!this->is_err()) {
      return std::nullopt;
    }
    return std::make_optional(std::move(std::get<Err<E>>(std::move(this->val)).inner));
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

template<class T> using span = std::span<T>;

#else // __cplusplus < 202002L

// C++-17-compatible std::span
template<class T>
class span {

public:
  constexpr span(T* data, size_t size)
    : data_(data), size_(size) {}
  template<size_t N>
  constexpr span(std::array<typename std::remove_const<T>::type, N>& arr)
    : data_(const_cast<T*>(arr.data())), size_(N) {}
  constexpr T* data() const noexcept {
    return this->data_;
  }
  constexpr size_t size() const noexcept {
    return this->size_;
  }
private:
  T* data_;
  size_t size_;
};

#endif // __cplusplus >= 202002L

} // namespace diplomat

#endif
