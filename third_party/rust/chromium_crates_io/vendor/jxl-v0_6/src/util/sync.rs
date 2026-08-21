// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#[cfg(feature = "shuttle")]
pub use shuttle::sync::*;

// TODO(veluca): replace this with a shuttle type once
// shuttle supports OnceLock.
#[cfg(feature = "shuttle")]
pub use std::sync::OnceLock;

#[cfg(not(feature = "shuttle"))]
pub use std::sync::*;
