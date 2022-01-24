# Rust third-party code

Use of Rust is being experimented with. As such, code outside of that experiment
should not depend on targets in this directory.

## Directory structure

We store each third-party crate in a directory of the same name. Under
that directory a folder named based on the crate epoch version is created.
If the crate's version has a major version greater-than 0, then that is used
as its version folder, such as `v1`. Otherwise, the name includes all
leading zeros in the version, such as `v0_3` or `v0_0_6`.

For example, the `tutelage` crate at version **1.4.3** would be stored at
```sh
//third_party/rust/tutelage/v1
```

Whereas the verion **0.2.8** version would be stored at
```sh
//third_party/rust/tutelage/v0_2
```

## OWNERS

We do not require OWNERS in each crate's directory at this time, however this
will be revisted when Rust goes to production.

## Vendoring vs DEPS

In order to move quickly and reduce our processes, we will vendor third-party
code directly into Chromium src.git under this directory.

## Testing

All third-party crates should have their tests added to a Chromium test suite.
These tests will be run on the Rust FYI bots, and may move to a blocking bot
when Rust goes to production.
