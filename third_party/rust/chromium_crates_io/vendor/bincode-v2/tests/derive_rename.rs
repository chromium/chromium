#![cfg(feature = "derive")]

extern crate bincode as bincode_new;

// Make sure that the `bincode` crate exists, just symlink it to `core.
extern crate core as bincode;

#[derive(bincode_new::Encode)]
#[bincode(crate = "bincode_new")]
#[allow(dead_code)]
struct DeriveRenameTest {
    a: u32,
    b: u32,
}
