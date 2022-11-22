// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace blink {

// Stuff in blink:: should be renamed.
void foo();

// Stuff in nested namespaces should be renamed.
namespace nested {
void foo();
}  // namespace nested

// blink::protocol namespace is blocklisted.
namespace protocol {
void foo();
}  // namespace protocol

}  // namespace blink

namespace WTF {

// Stuff in WTF:: should be renamed.
void foo();

// Stuff in nested namespaces should be renamed.
namespace nested {
void foo();
}  // namespace nested

}  // namespace WTF

// Stuff outside blink:: and WTF:: should not be.
namespace other {
void foo();
namespace blink {
void foo();
}  // namespace blink
namespace WTF {
void foo();
}  // namespace WTF
}  // namespace other
void foo();

void G() {
  blink::foo();
  blink::nested::foo();
  WTF::foo();
  WTF::nested::foo();
  other::foo();
  foo();
  other::blink::foo();
  other::WTF::foo();
}
