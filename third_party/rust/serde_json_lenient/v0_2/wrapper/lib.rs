// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

mod visitor;

use crate::visitor::ValueVisitor;

use serde::de::Deserializer;
use serde_json_lenient::de::SliceRead;
use std::pin::Pin;

/// UTF8 byte order mark.
const UTF8_BOM: [u8; 3] = [0xef, 0xbb, 0xbf];

/// C++ bindings
#[cxx::bridge(namespace=serde_json_lenient)]
mod ffi {
    // From the `wrapper_functions` target.
    unsafe extern "C++" {
        include!("third_party/rust/serde_json_lenient/v0_2/wrapper/functions.h");

        type Dict;
        type List;

        fn list_append_none(ctx: Pin<&mut List>);
        fn list_append_bool(ctx: Pin<&mut List>, val: bool);
        fn list_append_i32(ctx: Pin<&mut List>, val: i32);
        fn list_append_f64(ctx: Pin<&mut List>, val: f64);
        fn list_append_str(ctx: Pin<&mut List>, val: &str);
        fn list_append_list<'a>(ctx: Pin<&'a mut List>) -> Pin<&'a mut List>;
        fn list_append_dict<'a>(ctx: Pin<&'a mut List>) -> Pin<&'a mut Dict>;

        fn dict_set_none(ctx: Pin<&mut Dict>, key: &str);
        fn dict_set_bool(ctx: Pin<&mut Dict>, key: &str, val: bool);
        fn dict_set_i32(ctx: Pin<&mut Dict>, key: &str, val: i32);
        fn dict_set_f64(ctx: Pin<&mut Dict>, key: &str, val: f64);
        fn dict_set_str(ctx: Pin<&mut Dict>, key: &str, val: &str);
        fn dict_set_list<'f, 'a>(ctx: Pin<&'a mut Dict>, key: &'f str) -> Pin<&'a mut List>;
        fn dict_set_dict<'f, 'a>(ctx: Pin<&'a mut Dict>, key: &'f str) -> Pin<&'a mut Dict>;
    }

    extern "Rust" {
        fn decode_json(
            json: &[u8],
            options: &JsonOptions,
            ctx: Pin<&mut List>,
            error: Pin<&mut DecodeError>,
        ) -> bool;
    }

    struct DecodeError {
        line: i32,
        column: i32,
        message: String,
    }

    /// Options for parsing JSON inputs. A mirror of the C++
    /// `base::JSONParserOptions` bitflags, represented as a friendlier
    /// struct-of-bools instead, and with additional fields
    struct JsonOptions {
        /// Allows commas to exist after the last element in structures.
        allow_trailing_commas: bool,
        /// If set the parser replaces invalid code points (i.e. lone
        /// surrogates) with the Unicode replacement character (U+FFFD).
        /// If not set, invalid code points trigger a hard error and
        /// parsing fails.
        replace_invalid_characters: bool,
        /// Allows both C (/* */) and C++ (//) style comments.
        allow_comments: bool,
        /// Permits unescaped \r and \n in strings. This is a subset of what
        /// allow_control_chars allows.
        allow_newlines: bool,
        /// Permits \\v vertical tab escapes.
        allow_vert_tab: bool,
        /// Permits \\xNN escapes as described above.
        allow_x_escapes: bool,

        /// The maximum recursion depth to walk while parsing nested JSON
        /// objects. JSON beyond the specified depth will be ignored.
        max_depth: usize,
    }
}

pub type DecodeError = ffi::DecodeError;
pub type Dict = ffi::Dict;
pub type JsonOptions = ffi::JsonOptions;
pub type List = ffi::List;

/// Decode a JSON input from `json` and call back out to functions defined in
/// `options` when visiting each node in order for the caller to construct an
/// output.
///
/// The first item visited will be appended to the `ctx`.
///
/// The `error` is only written to when there is an error decoding and `false`
/// is returned.
///
/// # Returns
///
/// Whether the decode succeeded.
pub fn decode_json(
    json: &[u8],
    options: &JsonOptions,
    ctx: Pin<&mut List>,
    // TODO(danakj): Return `Result<(), DecodeError>` once the binding generator supports it.
    mut error: Pin<&mut DecodeError>,
) -> bool {
    let mut deserializer = serde_json_lenient::Deserializer::new(SliceRead::new(
        if json.starts_with(&UTF8_BOM) { &json[3..] } else { json },
        options.replace_invalid_characters,
        options.allow_newlines,
        /*allow_control_chars_in_string=*/false,
        options.allow_vert_tab,
        options.allow_x_escapes,
    ));
    deserializer.set_ignore_trailing_commas(options.allow_trailing_commas);
    deserializer.set_allow_comments(options.allow_comments);

    // We track recursion depth ourselves to limit it to `max_depth` option.
    deserializer.disable_recursion_limit();

    let target = visitor::DeserializationTarget::List { ctx };

    let result = deserializer.deserialize_any(ValueVisitor::new(target, options.max_depth));
    match result.and(deserializer.end()) {
        Ok(()) => true,
        Err(err) => {
            error.as_mut().line = err.line().try_into().unwrap_or(-1);
            error.as_mut().column = err.column().try_into().unwrap_or(-1);
            // The following line pulls in a lot of binary bloat, due to all the formatter
            // implementations required to stringify error messages. This error message is
            // used in only a couple of places outside unit tests so we could
            // consider trying to eliminate.
            error.as_mut().message = err.to_string();
            false
        }
    }
}
