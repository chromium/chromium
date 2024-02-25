//! WARNING: this is not part of the crate's public API and is subject to change at any time

use crate::{Level, Metadata, Record};
use std::fmt::Arguments;
pub use std::option::Option;
pub use std::{file, format_args, line, module_path, stringify};

#[cfg(not(feature = "kv_unstable"))]
pub fn log(
    args: Arguments,
    level: Level,
    &(target, module_path, file): &(&str, &'static str, &'static str),
    line: u32,
    kvs: Option<&[(&str, &str)]>,
) {
    if kvs.is_some() {
        panic!(
            "key-value support is experimental and must be enabled using the `kv_unstable` feature"
        )
    }

    crate::logger().log(
        &Record::builder()
            .args(args)
            .level(level)
            .target(target)
            .module_path_static(Some(module_path))
            .file_static(Some(file))
            .line(Some(line))
            .build(),
    );
}

#[cfg(feature = "kv_unstable")]
pub fn log(
    args: Arguments,
    level: Level,
    &(target, module_path, file): &(&str, &'static str, &'static str),
    line: u32,
    kvs: Option<&[(&str, &dyn crate::kv::ToValue)]>,
) {
    crate::logger().log(
        &Record::builder()
            .args(args)
            .level(level)
            .target(target)
            .module_path_static(Some(module_path))
            .file_static(Some(file))
            .line(Some(line))
            .key_values(&kvs)
            .build(),
    );
}

pub fn enabled(level: Level, target: &str) -> bool {
    crate::logger().enabled(&Metadata::builder().level(level).target(target).build())
}
