// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::time::Instant;

#[cxx::bridge(namespace = "fend_core")]
mod ffi {
    extern "Rust" {
        // If `timeout_in_ms` = 0, there is no timeout.
        fn evaluate_using_rust(query: &[u8], out_result: &mut String, timeout_in_ms: u32) -> bool;
    }
}

const FULL_QUERY: &str = "@noapprox ans in 2dp";

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

fn is_allowed_byte(byte: u8) -> bool {
    // All bytes of UTF-8 non-ASCII codepoints have an MSB of 1, which should
    // never match any ASCII character.
    matches!(byte, b' ' | b'0'..=b'9' | b'+' | b'-' | b'*' | b'/' | b'(' | b')')
}

pub fn evaluate_using_rust(query: &[u8], out_result: &mut String, timeout_in_ms: u32) -> bool {
    if !query.iter().any(|c| is_allowed_byte(*c)) {
        return false;
    }
    let Ok(query) = std::str::from_utf8(query) else {
        return false;
    };

    let mut context = fend_core::Context::new();

    let query_with_preamble = format!("f = °F; c = °C; {query}");
    let original_result = if timeout_in_ms > 0 {
        let interrupt = TimeoutInterrupt::new_with_timeout(timeout_in_ms.into());
        fend_core::evaluate_with_interrupt(&query_with_preamble, &mut context, &interrupt)
    } else {
        fend_core::evaluate(&query_with_preamble, &mut context)
    };
    let Ok(original_result) = original_result else {
        return false;
    };
    if original_result.get_main_result().starts_with('\\')
        || original_result.get_main_result().ends_with(query)
    {
        return false;
    }

    // `ans` should be set to the value calculated above.
    let final_result = if timeout_in_ms > 0 {
        let interrupt = TimeoutInterrupt::new_with_timeout(timeout_in_ms.into());
        fend_core::evaluate_with_interrupt(FULL_QUERY, &mut context, &interrupt)
    } else {
        fend_core::evaluate(FULL_QUERY, &mut context)
    };
    let Ok(final_result) = final_result else {
        return false;
    };

    *out_result = final_result.get_main_result().to_string();
    true
}
