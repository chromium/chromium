// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::{
    fmt::Debug,
    ops::{Deref, DerefMut},
};

use crate::util::sync::Mutex;

// Note: this is meant to be used only in low-contention
// scenarios.
pub struct PerThreadStorage<T> {
    storage: Mutex<Vec<T>>,
    init: fn() -> T,
}

impl<T> Debug for PerThreadStorage<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "<thread storage>")
    }
}

pub struct PerThreadStorageRef<'a, T> {
    r: &'a PerThreadStorage<T>,
    val: Option<T>,
}

impl<T> PerThreadStorage<T> {
    pub fn new(init: fn() -> T) -> Self {
        Self {
            storage: Mutex::new(vec![]),
            init,
        }
    }

    pub fn get(&self) -> PerThreadStorageRef<'_, T> {
        let t = {
            let mut a = self.storage.lock().unwrap();
            if let Some(x) = a.pop() {
                x
            } else {
                drop(a);
                (self.init)()
            }
        };
        PerThreadStorageRef {
            r: self,
            val: Some(t),
        }
    }
}

impl<'a, T> Drop for PerThreadStorageRef<'a, T> {
    fn drop(&mut self) {
        let v = self.val.take().unwrap();
        let mut a = self.r.storage.lock().unwrap();
        a.push(v);
    }
}

impl<'a, T> Deref for PerThreadStorageRef<'a, T> {
    type Target = T;
    fn deref(&self) -> &Self::Target {
        self.val.as_ref().unwrap()
    }
}

impl<'a, T> DerefMut for PerThreadStorageRef<'a, T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.val.as_mut().unwrap()
    }
}
