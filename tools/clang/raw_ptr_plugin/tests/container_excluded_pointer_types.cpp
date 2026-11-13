// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Pointer types listed in `excluded_pointer_types` (see
// container_excluded_pointer_types.yaml) are not required to be wrapped in
// `raw_ptr`. Only the name spelled in the source is compared.

#include <set>
#include <tuple>
#include <vector>

// Win32 `DECLARE_HANDLE()` handles.
struct HWND__;
typedef struct HWND__* HWND;
struct HKL__;
typedef struct HKL__* HKL;
struct HINSTANCE__ {
  int unused;
};
typedef struct HINSTANCE__* HINSTANCE;
typedef HINSTANCE HMODULE;

// Win32 `void*` handles.
typedef void* LPVOID;
typedef void* HANDLE;
typedef LPVOID HINTERNET;
typedef HANDLE HLOCAL;

// Vulkan handles on 64-bit platforms.
typedef struct VkSemaphore_T* VkSemaphore;
typedef struct VkFence_T* VkFence;

// Objective-C `id`, when <objc/objc.h> is compiled as C++.
typedef struct objc_class* Class;
struct objc_object {
  Class isa;
};
typedef struct objc_object* id;

// Names are matched fully-qualified: `ns::Handle` is listed, `other::Handle`
// is not.
namespace ns {
struct Opaque;
using Handle = Opaque*;
}  // namespace ns
namespace other {
struct Opaque;
using Handle = Opaque*;
}  // namespace other

class SomeClass {};
using SomeClassPtr = SomeClass*;

using HandleList = std::vector<HANDLE>;

template <typename T>
using PtrVector = std::vector<T*>;

template <typename T>
using Set = std::set<T>;

struct MyStruct {
  // No error expected: the spelled type is listed.
  std::vector<HWND> hwnds;
  std::set<HKL> keyboard_layouts;
  std::vector<HINSTANCE> instances;
  std::vector<HANDLE> handles;
  std::set<HINTERNET> internet_handles;
  HandleList aliased_handles;
  std::vector<const HANDLE> const_handles;
  std::vector<VkSemaphore> semaphores;
  std::vector<id> observers;
  std::vector<ns::Handle> ns_handles;
  std::tuple<HANDLE, HWND, int> handle_tuple;
  Set<HANDLE> handle_set;
  // No error expected: any listed argument skips the field, even if another
  // argument is a pointer.
  std::tuple<HANDLE, SomeClass*> mixed_tuple;

  // Error expected: not listed. Aliases of listed types (`HMODULE`, `HLOCAL`)
  // and pointers to the records they point to (`HWND__*`) must be listed
  // separately.
  std::vector<HMODULE> modules;
  std::vector<HLOCAL> locals;
  std::vector<HWND__*> hwnd_ptrs;
  std::vector<void*> void_ptrs;
  std::vector<LPVOID> lpvoids;
  std::vector<VkFence> fences;
  std::vector<other::Handle> other_handles;
  std::vector<SomeClass*> some_class_ptrs;
  std::vector<SomeClassPtr> some_class_alias_ptrs;
  PtrVector<SomeClass> alias_template_ptrs;
};
