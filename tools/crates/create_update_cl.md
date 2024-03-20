# Updating Rust crates used by Chromium

This document describes how Chromium updates crates.io Rust crates that Chromium
depends on.

We have a weekly rotation (go/chromium-crates-update-rotation) of engineers
responsible for creating and landing CLs that update Rust crates.

## Initial setup

The "Rust: periodic update of 3rd-party crates" rotation requires access to an
up-to-date Chromium repo.  One way to start a shift is to run `git fetch`,
`git checkout origin/main`, and `gclient sync` (but other workflows should also
work - e.g. ones based on `git-new-workdir`).

## Automated step: `create_update_cl.py`

The first actual step of the rotation is manually running the
`tools/crates/create_update_cl.py` script.

`//tools/crates/create_update_cl.py` runs `gnrt update`, `gnrt vendor`, and
`gnrt gen`, and then uploads zero, one, or more resulting CLs to Gerrit.
`create_update_cl.py` has to be invoked from within a Chromium repo.
The script depends on `depot_tools` and `git` being present in the `PATH`.
Depending on how many crates are updated, the script may need 10-15 minutes to
run.

Before the auto-generated CLs can be landed, some additional manual steps need
to be done first - see the sections below.

## Manual step: `run_cargo_vet.py`

The changes in the auto-generated CL need to go through a security audit, which
will ensure that `cargo vet` criteria (e.g. `ub-risk-0`, `safe-to-deploy`,
etc.). still hold for the new versions.  The CL description specifies which
criteria apply to the updated crates.
See the `//docs/rust-unsafe.md` doc for details on how to audit and certify
the new crate versions (this may require looping in `unsafe` Rust experts
and/or cryptography experts).

For each update CL, there is a separate git branch created. An audit will most
likely need to be recorded in
`third_party/rust/chromium_crates_io/supply-chain/audits.toml` and committed to
the git branch for each update CL. There are some known corner cases where
`audits.toml` changes are not needed:

* Updates of crates listed in `remove_crates` in
  `third_party/rust/chromium_crates_io/gnrt_config.toml` (e.g. the `cc` crate
  which is a dependency of `cxx` but is not actually used/needed in Chromium).
  This case should be recognized by the `create_update_cl.py` script and noted
  in the CL description.
* Updates of grand-parented-in crates that are covered by exemptions in
  `third_party/rust/chromium_crates_io/supply-chain/config.toml` instead of
  being covered by real audits from `audits.toml`.  For such crates, skim the
  delta and use your best judgement on whether to bump the crate version that
  the exemption applies to.  Note that `supply-chain/config.toml` is generated
  by `gnrt vendor` and should not be edited directly - please instead edit
  `third_party/rust/chromium_crates_io/vet_config.toml.hbs` and then run
  `tools/crates/run_gnrt.py vendor` to regenerate `supply-chain/config.toml`.
* Update to a crate version that is already covered by `audits.toml` of other
  projects that Chromium's `run_cargo_vet.py` imports.  In such case (but only
  once https://crrev.com/c/5368743 lands) you may
  need to commit changes that `cargo vet` generates in
  `third_party/rust/chromium_crates_io/supply-chain/imports.lock`.

This step may require one or more of the commands below, for each git branch
associated with an update CL (starting with the earliest branches - ones
closest to `origin/main`):

1. `git checkout rust-crates-update--...`
    - If this is the second or subsequent branch, then also run `git rebase` to
      rebase it on top of the manual `audits.toml` changes in the upstream
      branches
1. Check which crate (or crates) and which audit criteria need to be reviewed in
   this branch / CL: `tools/crates/run_cargo_vet.py check`
    - Note that `run_cargo_vet.py check` may list a _subset_ of the criteria
      that the automated script has listed in the CL description (e.g. if some
      of the criteria are already covered by `audits.toml` imported from other
      projects).
    - Install `cargo vet` if it's not yet installed:
        * `tools/crates/run_cargo.py install cargo-vet --locked --version=0.9.1`
        * TODO: Pre-package `cargo-vet` into `rust-toolchain`:
          https://crrev.com/c/5366668
1. Follow the cargo vet instructions to inspect diffs and certify the results
1. `git add third_party/rust/chromium_crates_io/supply-chain`.
   `git commit -m 'cargo vet'`
1. `git cl upload -m 'cargo vet'`

## Potential additional steps

* If updating `cxx`, you may need to also update its version in:
    - `build/rust/BUILD.gn`
    - `third_party/rust/cxx/v1/cxx.h`

* The `create_update_cl.py` script may stop early if it detects that `gnrt
  vendor` or `gnrt gen` have reported any warnings or errors (e.g. a "License
  file not found for crate foo" warning).  In this case, manual intervention is
  needed to finish the update CL.  It's probably best to finish and land the CLs
  created so far before trying to restart the script in order to create the
  remaining CLs.

## Landing the CL

Other than the above, the CL can go through the normal, plain-vanilla, manual
review and landing process.

1. `git cl upload`
1. Get a review from one of `//third_party/rust/OWNERS`
1. Land the CL using CQ+2
