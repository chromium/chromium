// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef UI_GFX_X_XPROTO_INTERNAL_H_
#define UI_GFX_X_XPROTO_INTERNAL_H_

#include <string_view>

#include "base/memory/raw_ptr.h"

#ifndef IS_X11_IMPL
#error "This file should only be included by //ui/gfx/x"
#endif

#include <bitset>
#include <type_traits>

#include "base/component_export.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_types.h"

namespace x11 {

class Connection;

template <typename T, typename Enable = void>
struct EnumBase {
  using type = T;
};

template <typename T>
struct EnumBase<T, typename std::enable_if_t<std::is_enum<T>::value>> {
  using type = typename std::underlying_type<T>::type;
};

template <typename T>
using EnumBaseType = typename EnumBase<T>::type;

template <typename T>
void ReadError(T* error, ReadBuffer* buf);

// Calls free() on the underlying data when the count drops to 0.
class COMPONENT_EXPORT(X11) MallocedRefCountedMemory
    : public UnsizedRefCountedMemory {
 public:
  explicit MallocedRefCountedMemory(void* data);

  MallocedRefCountedMemory(const MallocedRefCountedMemory&) = delete;
  MallocedRefCountedMemory& operator=(const MallocedRefCountedMemory&) = delete;

 private:
  struct Deleter {
    void operator()(uint8_t* data) {
      if (data) {
        free(data);
      }
    }
  };
  ~MallocedRefCountedMemory() override;

  // UnsizedRefCountedMemory:
  void* data() LIFETIME_BOUND override;
  const void* data() const LIFETIME_BOUND override;

  std::unique_ptr<uint8_t[], Deleter> data_;
};

// Wraps another RefCountedMemory, giving a view into it.  Similar to
// std::string_view, the data is some contiguous subarray, but unlike
// std::string_view, a counted reference is kept on the underlying memory.
class COMPONENT_EXPORT(X11) OffsetRefCountedMemory
    : public UnsizedRefCountedMemory {
 public:
  OffsetRefCountedMemory(scoped_refptr<UnsizedRefCountedMemory> memory,
                         size_t offset,
                         size_t size);

  OffsetRefCountedMemory(const OffsetRefCountedMemory&) = delete;
  OffsetRefCountedMemory& operator=(const OffsetRefCountedMemory&) = delete;

 private:
  ~OffsetRefCountedMemory() override;

  // UnsizedRefCountedMemory:
  void* data() LIFETIME_BOUND override;
  const void* data() const LIFETIME_BOUND override;

  scoped_refptr<UnsizedRefCountedMemory> memory_;
  size_t offset_;
};

// Wraps a bare pointer and does not take any action when the reference count
// reaches 0.  This is used to wrap stack-alloctaed or persistent data so we can
// pass those to Read/ReadEvent/ReadReply which expect RefCountedMemory.
class COMPONENT_EXPORT(X11) UnretainedRefCountedMemory
    : public UnsizedRefCountedMemory {
 public:
  explicit UnretainedRefCountedMemory(void* data);

  UnretainedRefCountedMemory(const UnretainedRefCountedMemory&) = delete;
  UnretainedRefCountedMemory& operator=(const UnretainedRefCountedMemory&) =
      delete;

 private:
  ~UnretainedRefCountedMemory() override;

  // UnsizedRefCountedMemory:
  void* data() LIFETIME_BOUND override;
  const void* data() const LIFETIME_BOUND override;

  raw_ptr<void> data_;
};

template <typename T>
void Read(T* t, ReadBuffer* buf) {
  static_assert(std::is_trivially_copyable<T>::value, "");
  detail::VerifyAlignment(t, buf->offset);
  memcpy(t, buf->data->bytes() + buf->offset, sizeof(*t));
  buf->offset += sizeof(*t);
}

inline void Pad(WriteBuffer* buf, size_t amount) {
  uint8_t zero = 0;
  for (size_t i = 0; i < amount; i++) {
    buf->Write(&zero);
  }
}

inline void Pad(ReadBuffer* buf, size_t amount) {
  buf->offset += amount;
}

inline void Align(WriteBuffer* buf, size_t align) {
  Pad(buf, (align - (buf->offset() % align)) % align);
}

inline void Align(ReadBuffer* buf, size_t align) {
  Pad(buf, (align - (buf->offset % align)) % align);
}

// Helper function for xcbproto popcount.  Given an integral type, returns the
// number of 1 bits present.
template <typename T>
size_t PopCount(T t) {
  return std::bitset<sizeof(T) * 8>(static_cast<EnumBaseType<T>>(t)).count();
}

// Helper function for xcbproto sumof.  Given a function |f| and a container
// |t|, maps the elements uisng |f| and reduces by summing the results.
template <typename F, typename T>
auto SumOf(F&& f, T& t) {
  decltype(f(t[0])) sum = 0;
  for (auto& v : t) {
    sum += f(v);
  }
  return sum;
}

// Helper function for xcbproto case.  Checks for equality between |t| and |s|.
template <typename T, typename S>
bool CaseEq(T t, S s) {
  return t == static_cast<decltype(t)>(s);
}

// Helper function for xcbproto bitcase expressions.  Checks if the bitmasks |t|
// and |s| have any intersection.
template <typename T, typename S>
bool CaseAnd(T t, S s) {
  return static_cast<EnumBaseType<T>>(t) & static_cast<EnumBaseType<T>>(s);
}

// Helper function for xcbproto & expressions.  Computes |t| & |s|.
template <typename T, typename S>
auto BitAnd(T t, S s) {
  return static_cast<EnumBaseType<T>>(t) & static_cast<EnumBaseType<T>>(s);
}

// Helper function for xcbproto ~ expressions.
template <typename T>
auto BitNot(T t) {
  return ~static_cast<EnumBaseType<T>>(t);
}

// Helper function for generating switch values.  |switch_var| is the value to
// modify.  |enum_val| is the value to set |switch_var| to if this is a regular
// case, or the bit to be set in |switch_var| if this is a bit case.  This
// function is a no-op when |condition| is false.
template <typename T>
auto SwitchVar(T enum_val, bool condition, bool is_bitcase, T* switch_var) {
  using EnumInt = EnumBaseType<T>;
  if (!condition) {
    return;
  }
  EnumInt switch_int = static_cast<EnumInt>(*switch_var);
  if (is_bitcase) {
    *switch_var = static_cast<T>(switch_int | static_cast<EnumInt>(enum_val));
  } else {
    CHECK(!switch_int);
    *switch_var = enum_val;
  }
}

template <typename T>
std::unique_ptr<T> MakeExtension(Connection* connection,
                                 Future<QueryExtensionReply> future) {
  auto reply = future.Sync();
  return std::make_unique<T>(connection,
                             reply ? *reply.reply : QueryExtensionReply{});
}

}  // namespace x11

#endif  //  UI_GFX_X_XPROTO_INTERNAL_H_
