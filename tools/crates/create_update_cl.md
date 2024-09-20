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

## Avoiding Conflicts

Before creating a CL stack, check for open CLs with the [`cratesio-autoupdate`
tag](https://chromium-review.googlesource.com/q/hashtag:%22cratesio-autoupdate%22+(status:open%20OR%20status:merged)).
Such CLs tend to conflict, so coordinate with owners of any open CLs.

## Automated step: `create_update_cl.py`

The first actual step of the rotation is running the script:

```
$ tools/crates/create_update_cl.py auto
```

`create_update_cl.py` has to be invoked from within a Chromium repo.
The script depends on `depot_tools` and `git` being present in the `PATH`.

In `auto` mode `//tools/crates/create_update_cl.py` runs `gnrt update` to
discover all possible minor version updates and then for each update creates a
new local git branch (and a Gerrit CL unless invoked with `--no-upload`).
Each branch contains an update created by `gnrt update <old crate id>`, `gnrt
vendor`, and `gnrt gen`.
Depending on how many crates are updated, the script may need 10-15 minutes to
run.

(Side-note: outside the rotation one may also use the script to update a single
crate - e.g. `tools/crates/create_update_cl.py single bytemuck`.  When working
with multi-epoch/version crates the old version to update can be specified
as follows: `tools/crates/create_update_cl.py single syn@2.0.55`.)

Before the auto-generated CLs can be landed, some additional manual steps need
to be done first - see the sections below.

## Manual step: `run_cargo_vet.py`

The changes in the auto-generated CL need to go through a security audit, which
will ensure that `cargo vet` criteria (e.g. `ub-risk-0`, `safe-to-deploy`,
etc.). still hold for the new versions.  The CL description specifies what are
the _minimum_ criteria required for the updated crates (note that
`supply-chain/audits.toml` can and should record a stricter certification if
possible).
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
  projects that Chromium's `run_cargo_vet.py` imports.  In such case you may
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
    - Also note that `cargo vet` will list the _minimum_ required criteria
      and `audits.toml` can and should record stricter certification if
      possible. In particular:
         - Record `does-not-implement-crypto` instead of `crypto-safe` if the
           crate does not implement crypto.
         - Record a lower-numbered `ub-risk-N` if appropriate.
    - And also note that if the crate is currently covered by an exemption
      in `config.toml`, then we want to bump the exemption instead of providing
      a delta audit that is baselined on an exemption.
      Note that `config.toml` shouldn't be edited manually - please edit
      `vet_config.toml.hbs` and regenerate `config.toml` by running
      `tools/crates/run_gnrt.py vendor`.
1. Follow the cargo vet instructions to inspect diffs and certify the results
    - Note that special guidelines may apply to
      [delta audits](https://github.com/google/rust-crate-audits/blob/main/auditing_standards.md#delta-audits-should-describe-the-final-version)
1. `git add third_party/rust/chromium_crates_io/supply-chain`.
1. `git commit -m 'cargo vet'`
1. `git cl upload -m 'cargo vet'`

## New transitive dependencies

If the roll brings in a new transitive dependency, it will need to be
audited in its entirety and the results recorded in
`third_party/rust/chromium_crates_io/supply-chain/audits.toml`.

The addition of transitive Rust dependencies does not need ATL approval,
but an FYI email should be sent to
[chrome-atls-discuss@google.com](mailto:chrome-atls-discuss@google.com)
in order to record the addition.

### Optional: Adding the transitive dependency in its own CL.

If the crate and/or audit are non-trivial, it's possible to split the
additional crate into its own CL, however then it will default to global
visibility and allowing non-test use.
* `gnrt add` and `gnrt vendor` can add the dependency to a fresh checkout.
* Mark the crate as being for third-party code only by setting
  `allow_first_party_usage` to `false` for the crate in
  `third_party/rust/chromium_crates_io/gnrt_config.toml`.
* If the crates making use of the transitive dependency are only allowed
  in tests, then set `group = 'test'` for the crate in
  `third_party/rust/chromium_crates_io/gnrt_config.toml`. This reduces
  the level of security review required for the library.
* `gnrt gen` will then generate the GN rules.
* Rebase the roll CL on top of the changes to make sure the choices made above
  are correct. `gn gen` will fail in CQ if the crate was placed in the `'test'`
  group but needs to be visible outside of tests.

## Potential additional steps

* If updating `cxx`, you will also need to update its version in
    - `build/rust/cxx_version.gni`

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

## Checking for new major versions

Note that `create_update_cl.py` will only handle minor version changes (e.g.
123.1 => 123.2, or 0.123.1 => 0.123.2).  Major version changes (e.g. 1.0 => 2.0,
which may include breaking API changes and other breaking changes) need to be
handled separately.

As part of the rotation, one should attempt to check for new major versions of
direct Chromium dependencies (i.e. dependencies directly listed in
`third_party/rust/chromium_crates_io/Cargo.toml`).  To discover direct _and_
transitive dependencies with a new major version, you can use the command below
(running it in the final update CL branch - after all the minor version
updates):

```
$ tools/crates/run_cargo.py -Zunstable-options -C third_party/rust/chromium_crates_io -Zbindeps update --dry-run --verbose
...
   Unchanged serde_json_lenient v0.1.8 (latest: v0.2.0)
   Unchanged syn v1.0.109 (latest: v2.0.53)
...
```

### Workflow A: Single update CL

If the updating to a new major version doesn't require lots of Chromium changes,
then it may be possible to land the update in a single CL.  This is typically
possible when the APIs affected by the major version's breaking change either
weren't used by Chromium, or were used only in a handful of places.

**Warning**: Sometimes a new major version may be API compatible, but may
introduce breaking changes in the _behavior_ of the existing APIs.

To update:

1. Prepare `Cargo.toml` change:
    1. `git checkout origin/main`
    1. `git checkout -b major-version-update-of-foo`
    1. Edit `third_party/rust/chromium_crates_io/Cargo.toml` to change the major
       version of the crate (or crates) you want to update.
       **Important**: Do not edit `Cargo.lock` (e.g. don't run `gnrt vendor`
       etc.).
    1. `git add third_party/rust/chromium_crates_io/Cargo.toml`
    1. `git commit -m "Manual edit of Cargo.toml"`
    1. `git cl upload -m "Manual edit of Cargo.toml" --bypass-hooks --skip-title --force`
1. Run the helper script as follows:
   `tools/crates/create_update_cl.py manual
   --title "Roll foo crate to new major version in //third_party/rust."`
    - This will fix up the CL description
    - To make the review easier, one of the patchsets covers just the path
      changes.  For example - see [the delta here](https://crrev.com/c/5445719/2..7).
1. Follow the manual steps from the minor version update rotation:
    1. `cargo vet` audit
    1. Review, landing, etc.

### Workflow B: Incremental transition

When lots of first-party code depends on the old major version, then the
transition to the new major version may need to be done incrementally.  In this
case the transition can be split into the following steps:

1. Open a new bug to track the transition
    - TODO: Figure out how to tag/format the bug to make it easy to discover
      in future rotations
1. Land the new major version, so that the old and the new versions coexist.
   To do this follow the process for importing a new crate as described in
   [`docs/rust.md`](../../docs/rust.md#importing-a-crate-from-crates_io)
   (i.e. edit `Cargo.toml` to add the new version, run `gnrt vendor`, and so
   forth).
1. Incrementally transition first-party code to the new major version
1. Remove the old major version.  To do this follow a similar process as above
   (i.e. edit `Cargo.toml` to remove the old version, run `gnrt vendor`, and so
   forth).  Any leftover files in `//third_party/rust/<crate>/<old epoch>`
   should also be removed.

Note that the following `Cargo.toml` syntax allows two versions of a crate to
coexist:

```
[dependencies.serde_json_lenient_old_epoch]
package = "serde_json_lenient"
version = "0.1"

[dependencies.serde_json_lenient]
version = "0.2"
```
