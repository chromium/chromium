// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// TODO(firsching): as soon as "Vec::try_with_capacity" is available from the
// standard library use this instead of the functions here.
pub trait NewWithCapacity {
    type Output;
    type Error;
    fn new_with_capacity(capacity: usize) -> Result<Self::Output, Self::Error>;
}

impl<T> NewWithCapacity for Vec<T> {
    type Output = Vec<T>;
    type Error = std::collections::TryReserveError;

    fn new_with_capacity(capacity: usize) -> Result<Self::Output, Self::Error> {
        let mut vec = Vec::new();
        vec.try_reserve(capacity)?;
        Ok(vec)
    }
}

impl NewWithCapacity for String {
    type Output = String;
    type Error = std::collections::TryReserveError;
    fn new_with_capacity(capacity: usize) -> Result<Self::Output, Self::Error> {
        let mut s = String::new();
        s.try_reserve(capacity)?;
        Ok(s)
    }
}
