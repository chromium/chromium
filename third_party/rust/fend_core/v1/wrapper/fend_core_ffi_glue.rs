// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::time::Instant;

const TIMEOUT_IN_MS: u128 = 100;

#[cxx::bridge(namespace = "fend_core")]
mod ffi {
    extern "Rust" {
        fn evaluate_using_rust(query: &[u8], out_result: &mut String) -> bool;
    }
}

struct TimeoutInterrupt {
    start: Instant,
    timeout: u128,
}

impl TimeoutInterrupt {
    fn new_with_timeout(timeout: u128) -> Self {
        Self { start: Instant::now(), timeout }
    }
}

impl fend_core::Interrupt for TimeoutInterrupt {
    fn should_interrupt(&self) -> bool {
        Instant::now().duration_since(self.start).as_millis() > self.timeout
    }
}

pub fn evaluate_using_rust(query: &[u8], out_result: &mut String) -> bool {
    let Ok(query_str) = std::str::from_utf8(query) else {
        return false;
    };
    let mut context = fend_core::Context::new();
    let interrupt = TimeoutInterrupt::new_with_timeout(TIMEOUT_IN_MS);
    match fend_core::evaluate_with_interrupt(query_str, &mut context, &interrupt) {
        Err(_) => false,
        Ok(result) => {
            *out_result = result.get_main_result().to_string();
            true
        }
    }
}
