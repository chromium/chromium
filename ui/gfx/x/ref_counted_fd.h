// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_REF_COUNTED_FD_H_
#define UI_GFX_X_REF_COUNTED_FD_H_

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"

namespace x11 {

// Wraps a native file descriptor and close()s it when there are no more active
// scoped_refptrs.  This class is needed to implement request argument
// forwarding and is probably not useful outside of that context.
class COMPONENT_EXPORT(X11) RefCountedFD {
 public:
  RefCountedFD();
  explicit RefCountedFD(int fd);
  explicit RefCountedFD(base::ScopedFD);

  // All special members are defaulted.
  RefCountedFD(const RefCountedFD&);
  RefCountedFD(RefCountedFD&&);
  RefCountedFD& operator=(const RefCountedFD&);
  RefCountedFD& operator=(RefCountedFD&&);
  ~RefCountedFD();

  int get() const;

 private:
  class Impl : public base::RefCounted<Impl> {
   public:
    explicit Impl(int fd);
    explicit Impl(base::ScopedFD fd);

    base::ScopedFD& fd() { return fd_; }

   private:
    friend class base::RefCounted<Impl>;

    ~Impl();

    base::ScopedFD fd_;
  };

  scoped_refptr<Impl> impl_;
};

}  // namespace x11

#endif  // UI_GFX_X_REF_COUNTED_FD_H_
