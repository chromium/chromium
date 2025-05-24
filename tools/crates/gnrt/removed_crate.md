# Placeholder crates

In some circumstances `tools/crates/run_gnrt.py vendor` will *not* vendor an
actual crate.  When Chromium doesn't really depend on a given crate, then a
placeholder crate will be generated instead.  This typically happens when
either:

* The crate is a conditional dependency that is:
     - Only present on certain target platforms (ones not supported by
       Chromium build system as determined by
       `tools/crates/gnrt/condition.rs`)
     - Only present if certain crate features are enabled (ones determined to
       be disabled in Chromium after crate feature resolution performed by
       `gnrt`, `guppy`, and `cargo`)
* or `gnrt_config.toml` asked to remove the crate (e.g. via
  `remove_crates` entry of `gnrt_config.toml`)

`gnrt` generates a placeholder crate, because otherwise `cargo metadata`,
`cargo tree`, and other `cargo` commands wouldn't work.
