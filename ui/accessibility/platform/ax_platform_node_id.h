// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_ID_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_ID_H_

#include <stdint.h>

#include "base/types/pass_key.h"
#include "base/types/strong_alias.h"

namespace ui {

class AXNodeIdDelegate;

// The underlying type of a uniquely-generated accessibility node identifier for
// use with the platform. The actual id store can only a accommodate 32 bit
// integers on Windows, because IAccessible2 uses LONG for get_uniqueID() and
// for event target ids. In addition, the signed values have special meaning, so
// the id must be between 1 and 2^31-1.
//
// These ids must not be conflated with the int id or AXNodeId, that comes with
// web node data, which are only unique within their source frame.
//
// Instances may only be created by AXUniqueId. Take care when storing an
// instance, as its value is not guaranteed to be unique once the AXUniqueId
// from whence it came is destroyed.
//
// Instances are implicitly convertible to (but not from) the platform's node
// identifier type so that they may be used directly when calling the
// platform's accessibility APIs.
class AXPlatformNodeId
    : public base::StrongAlias<class AXPlatformNodeIdTag, int32_t> {
 public:
  // An invalid id -- the value of which matches that of kInvalidAXNodeID (0).
  constexpr AXPlatformNodeId()
      : base::StrongAlias<class AXPlatformNodeIdTag, int32_t>::StrongAlias(0) {}

  // Allow implementations of AXNodeIdDelegate to create instances, as they are
  // responsible for per-window allocation of unique identifiers.
  constexpr explicit AXPlatformNodeId(base::PassKey<AXNodeIdDelegate>,
                                      int32_t v)
      : base::StrongAlias<class AXPlatformNodeIdTag, int32_t>::StrongAlias(v) {}

  // Allow implicit conversion to the platform's node id type.
  constexpr operator const int32_t&() const { return value_; }

 private:
  friend class AXUniqueId;

  constexpr explicit AXPlatformNodeId(int32_t v)
      : base::StrongAlias<class AXPlatformNodeIdTag, int32_t>::StrongAlias(v) {}
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_ID_H_
