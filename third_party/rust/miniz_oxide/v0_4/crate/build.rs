#![forbid(unsafe_code)]
use autocfg;

fn main() {
    autocfg::new().emit_sysroot_crate("alloc");
}
