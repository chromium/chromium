// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_WEAK_PTR_NSOBJECT_H_
#define UI_BASE_COCOA_WEAK_PTR_NSOBJECT_H_

#include "base/component_export.h"

#if defined(__OBJC__)
@class WeakPtrNSObject;
#else
class WeakPtrNSObject;
#endif

namespace ui {
namespace internal {

// Non-templatized base for WeakPtrNSObjectFactory with utility functions. This
// mainly serves to hide the objective-C code from the header, so it can be
// included in cc files.
class COMPONENT_EXPORT(UI_BASE) WeakPtrNSObjectFactoryBase {
 protected:
  static WeakPtrNSObject* Create(void* owner);
  static void* UnWrap(WeakPtrNSObject* handle);
  static void InvalidateAndRelease(WeakPtrNSObject* handle);
};

}  // namespace internal

// Class that wraps a single weak pointer in an NSObject, which can be ref
// counted. This works a bit like base::WeakPtrFactory, except the WeakPtr is
// an NSObject owned both by the factory and anything that retains it via
// handle(). When the factory is destroyed (and releases its ownership), the
// weak pointer is invalidated, and Get() will return null. This is primarily
// used for passing a weak pointer into an Objective-C block, which will
// automatically retain NSObjects on the stack that are captured in the block.
template <class T>
class WeakPtrNSObjectFactory : public internal::WeakPtrNSObjectFactoryBase {
 public:
  explicit WeakPtrNSObjectFactory(T* owner) : handle_(Create(owner)) {}

  WeakPtrNSObjectFactory(const WeakPtrNSObjectFactory&) = delete;
  WeakPtrNSObjectFactory& operator=(const WeakPtrNSObjectFactory&) = delete;

  ~WeakPtrNSObjectFactory() { InvalidateAndRelease(handle_); }

  // Gets the original owner, if it hasn't been destroyed.
  static T* Get(WeakPtrNSObject* p) { return static_cast<T*>(UnWrap(p)); }

  // Gets the NSObject, which can then be retained. The result should always be
  // assigned to a local variable outside the block to ensure it is retained.
  WeakPtrNSObject* handle() { return handle_; }

 private:
  WeakPtrNSObject* handle_;
};

}  // namespace ui

#endif  // UI_BASE_COCOA_WEAK_PTR_NSOBJECT_H_
