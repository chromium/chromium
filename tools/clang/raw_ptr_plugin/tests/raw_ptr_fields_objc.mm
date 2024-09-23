// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

__attribute__((objc_root_class))
@interface MyInterface {}
@property(readonly) int* p;
@end

@implementation MyInterface
// @synthesize will automatically declares `int* _p`.
// The plugin should explicitly exclude a synthesized field from the check.
@synthesize p;
@end
