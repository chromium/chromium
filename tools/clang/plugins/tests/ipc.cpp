// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Blocklisted typedefs
typedef __INTMAX_TYPE__ intmax_t;
typedef __UINTMAX_TYPE__ uintmax_t;
typedef int intptr_t;
typedef unsigned int uintptr_t;
typedef __WINT_TYPE__ wint_t;
typedef __SIZE_TYPE__ size_t;
typedef __SIZE_TYPE__ rsize_t;
typedef long ssize_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;
typedef unsigned int dev_t;
typedef int off_t;
typedef long clock_t;
typedef int time_t;
typedef long suseconds_t;

// Other typedefs
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long int64_t;
typedef unsigned long uint64_t;

namespace std {

template <class T>
struct allocator {};

template <class T, class A = allocator<T>>
struct vector {};

template <class F, class S>
struct pair {};

}  // namespace std

namespace base {

class Pickle {};

template <class T, class... Ts>
struct Tuple {
  T value;
};

}  // namespace base

namespace IPC {

template <class... T>
struct CheckedTuple {
  typedef base::Tuple<T...> Tuple;
};

template <class T>
struct ParamTraits {
  static void Write(base::Pickle*, const T&) {}
};

template <class T>
void WriteParam(base::Pickle* pickle, const T& value) {
  ParamTraits<T>::Write(pickle, value);
}

}  // namespace IPC


/* Test IPC::WriteParam() usage in templates. ERRORS: 6 */

struct Data {
  uint32_t value;
  size_t size;
};

template <>
struct IPC::ParamTraits<Data> {
  static void Write(base::Pickle* pickle, const Data& p) {
    // OK: WriteParam() called in explicit specialization
    WriteParam(pickle, p.value); // OK
    WriteParam(pickle, p.size); // ERROR
  }
};

template <class T>
struct Container {
  T value;
};

template <class T>
struct IPC::ParamTraits<Container<T>> {
  static void Write(base::Pickle* pickle, const Container<T>& container) {
    // NOT CHECKED: T is not explicitly referenced
    IPC::WriteParam(pickle, container.value); // NOT CHECKED
    WriteParam(pickle, container.value); // NOT CHECKED

    // NOT CHECKED: T explicitly referenced
    IPC::WriteParam<T>(pickle, container.value); // NOT CHECKED
    WriteParam<T>(pickle, container.value); // NOT CHECKED

    // OK: explicit cast to non-dependent allowed type
    WriteParam(pickle, static_cast<uint32_t>(container.value)); // OK

    // ERROR: explicit cast to non-dependent banned type
    WriteParam(pickle, static_cast<long>(container.value)); // ERROR
  }
};

template <class T, class... Ts>
struct MultiContainer {
  T value;
};

template <class T, class... Ts>
struct IPC::ParamTraits<MultiContainer<T, Ts...>> {
  static void Write(base::Pickle* pickle,
                    const MultiContainer<T, Ts...>& container) {
    // NOT CHECKED: template argument explicitly referenced
    bool helper[] = {
        (WriteParam<Ts>(pickle, container.value), true)... // NOT CHECKED
    };
    (void)helper;
  }
};

template <class T>
struct SomeClass {
  static void Write(base::Pickle* pickle) {
    // NOT CHECKED: WriteParam() calls on dependent types
    IPC::WriteParam(pickle, T(0)); // NOT CHECKED

    // Non-dependent types are checked
    IPC::WriteParam(pickle, size_t(0)); // ERROR
    IPC::WriteParam(pickle, uint64_t(0)); // OK
  }

  template <class U>
  static void WriteEx(base::Pickle* pickle) {
    // NOT CHECKED: WriteParam() calls on dependent types
    IPC::WriteParam(pickle, U(0)); // NOT CHECKED

    // Non-dependent types are checked
    IPC::WriteParam(pickle, time_t(0)); // ERROR
    IPC::WriteParam(pickle, uint32_t(0)); // OK
  }
};

template <class T>
void SomeWriteFunction(base::Pickle* pickle) {
  // NOT CHECKED: WriteParam() calls on dependent types
  IPC::WriteParam(pickle, T(0)); // NOT CHECKED

  // Non-dependent types are checked
  IPC::WriteParam(pickle, long(0)); // ERROR
  IPC::WriteParam(pickle, char(0)); // OK

  [&](){
    IPC::WriteParam(pickle, T(0)); // NOT CHECKED

    IPC::WriteParam(pickle, clock_t(0)); // ERROR
    IPC::WriteParam(pickle, int64_t(0)); // OK
  }();
}

void TestWriteParamInTemplates() {
  // These specializations call WriteParam() on various banned types, either
  // because they were specified directly (long) or because non-blocklisted
  // typedef (uint64_t) was stripped down to its underlying type, which is
  // blocklisted when used as is (unsigned long).
  // However, since it's hard (if not impossible) to check specializations
  // properly, we're simply not checking them.
  SomeClass<long>::Write(nullptr);
  SomeClass<long>::WriteEx<uint64_t>(nullptr);
  SomeWriteFunction<uint64_t>(nullptr);
}


/* Test IPC::CheckedTuple. ERRORS: 5 */

#define IPC_TUPLE(...) IPC::CheckedTuple<__VA_ARGS__>::Tuple

#define IPC_MESSAGE_DECL(name, id, in_tuple) \
  struct name ## Meta_ ## id { \
    using InTuple = in_tuple; \
  };

#define IPC_TEST_MESSAGE(id, in) \
  IPC_MESSAGE_DECL(TestMessage, id, IPC_TUPLE in)

struct Empty {};

IPC_TEST_MESSAGE(__COUNTER__, (bool, size_t, Empty, long)) // 2 ERRORs

typedef std::vector<long> long1D;
typedef std::vector<long1D> long2D;
IPC_TEST_MESSAGE(__COUNTER__, (bool, long2D)) // ERROR

IPC_TEST_MESSAGE(__COUNTER__, (char, short, std::pair<size_t, bool>)) // ERROR

IPC_TEST_MESSAGE(__COUNTER__, (std::vector<std::vector<long&>&>&)) // ERROR


/* Check IPC::WriteParam() arguments. ERRORS: 30 */

// ERRORS: 21
void TestWriteParamArgument() {
  #define CALL_WRITEPARAM(Type) \
    { \
      Type p; \
      IPC::WriteParam(nullptr, p); \
    }

  // ERROR: blocklisted types / typedefs
  CALL_WRITEPARAM(long) // ERROR
  CALL_WRITEPARAM(unsigned long) // ERROR
  CALL_WRITEPARAM(intmax_t) // ERROR
  CALL_WRITEPARAM(uintmax_t) // ERROR
  CALL_WRITEPARAM(intptr_t) // ERROR
  CALL_WRITEPARAM(uintptr_t) // ERROR
  CALL_WRITEPARAM(wint_t) // ERROR
  CALL_WRITEPARAM(size_t) // ERROR
  CALL_WRITEPARAM(rsize_t) // ERROR
  CALL_WRITEPARAM(ssize_t) // ERROR
  CALL_WRITEPARAM(ptrdiff_t) // ERROR
  CALL_WRITEPARAM(dev_t) // ERROR
  CALL_WRITEPARAM(off_t) // ERROR
  CALL_WRITEPARAM(clock_t) // ERROR
  CALL_WRITEPARAM(time_t) // ERROR
  CALL_WRITEPARAM(suseconds_t) // ERROR

  // ERROR: typedef to blocklisted typedef
  typedef size_t my_size;
  CALL_WRITEPARAM(my_size) // ERROR

  // ERROR: expression ends up with type "unsigned long"
  {
    uint64_t p = 0;
    IPC::WriteParam(nullptr, p + 1); // ERROR
  }

  // ERROR: long chain of typedefs, ends up with blocklisted typedef
  {
    typedef size_t my_size_base;
    typedef const my_size_base my_size;
    typedef my_size& my_size_ref;
    my_size_ref p = 0;
    IPC::WriteParam(nullptr, p); // ERROR
  }

  // ERROR: template specialization references blocklisted type
  CALL_WRITEPARAM(std::vector<long>) // ERROR
  CALL_WRITEPARAM(std::vector<size_t>) // ERROR

  // OK: typedef to blocklisted type
  typedef long my_long;
  CALL_WRITEPARAM(my_long) // OK

  // OK: other types / typedefs
  CALL_WRITEPARAM(char) // OK
  CALL_WRITEPARAM(int) // OK
  CALL_WRITEPARAM(uint32_t) // OK
  CALL_WRITEPARAM(int64_t)  // OK

  // OK: long chain of typedefs, ends up with non-blocklisted typedef
  {
    typedef uint32_t my_int_base;
    typedef const my_int_base my_int;
    typedef my_int& my_int_ref;
    my_int_ref p = 0;
    IPC::WriteParam(nullptr, p); // OK
  }

  // OK: template specialization references non-blocklisted type
  CALL_WRITEPARAM(std::vector<char>) // OK
  CALL_WRITEPARAM(std::vector<my_long>) // OK

  #undef CALL_WRITEPARAM
}

struct Provider {
  typedef unsigned int flags;

  short get_short() const { return 0; }
  uint64_t get_uint64() const { return 0; }
  long get_long() const { return 0; }
  unsigned int get_uint() const { return 0; }
  flags get_flags() const { return 0; }
  size_t get_size() const { return 0; }

  const std::vector<size_t>& get_sizes() const { return sizes_data; }
  const std::vector<uint64_t>& get_uint64s() const { return uint64s_data; }

  template <class T>
  T get() const { return T(); }

  short short_data;
  unsigned int uint_data;
  flags flags_data;
  long long_data;
  size_t size_data;
  uint64_t uint64_data;
  std::vector<size_t> sizes_data;
  std::vector<uint64_t> uint64s_data;
};

// ERRORS: 9
void TestWriteParamMemberArgument() {
  Provider p;

  IPC::WriteParam(nullptr, p.get<short>()); // OK
  IPC::WriteParam(nullptr, p.get_short()); // OK
  IPC::WriteParam(nullptr, p.short_data); // OK

  IPC::WriteParam(nullptr, p.get<unsigned int>()); // OK
  IPC::WriteParam(nullptr, p.get_uint()); // OK
  IPC::WriteParam(nullptr, p.uint_data); // OK

  IPC::WriteParam(nullptr, p.get<Provider::flags>()); // OK
  IPC::WriteParam(nullptr, p.get_flags()); // OK
  IPC::WriteParam(nullptr, p.flags_data); // OK

  IPC::WriteParam(nullptr, p.get<long>()); // ERROR
  IPC::WriteParam(nullptr, p.get_long()); // ERROR
  IPC::WriteParam(nullptr, p.long_data); // ERROR

  // This one is flaky and depends on whether size_t is typedefed to a
  // blocklisted type (unsigned long).
  // IPC::WriteParam(nullptr, p.get<size_t>()); // ERROR
  IPC::WriteParam(nullptr, p.get_size()); // ERROR
  IPC::WriteParam(nullptr, p.size_data); // ERROR

  // Information about uint64_t gets lost, and plugin sees WriteParam()
  // call on unsigned long, which is blocklisted.
  IPC::WriteParam(nullptr, p.get<uint64_t>()); // ERROR
  IPC::WriteParam(nullptr, p.get_uint64()); // OK
  IPC::WriteParam(nullptr, p.uint64_data); // OK

  // Same thing here, WriteParam() sees vector<unsigned long>, and denies it.
  IPC::WriteParam(nullptr, p.get<std::vector<uint64_t>>()); // ERROR
  IPC::WriteParam(nullptr, p.get_uint64s()); // OK
  IPC::WriteParam(nullptr, p.uint64s_data); // OK

  // This one is flaky and depends on whether size_t is typedefed to a
  // blocklisted type (unsigned long).
  // IPC::WriteParam(nullptr, p.get<std::vector<size_t>>());
  IPC::WriteParam(nullptr, p.get_sizes()); // ERROR
  IPC::WriteParam(nullptr, p.sizes_data); // ERROR
}


/* ERRORS: 41 */
