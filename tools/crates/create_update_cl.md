# Updating Rust crates used by Chromium

This document describes how Chromium updates crates.io Rust crates that Chromium
depends on.

## Automated steps

`//tools/crates/create_update_cl.py` runs `gnrt update`, `gnrt vendor`, and
`gnrt gen`, and then uploads zero, one, or more resulting CLs to Gerrit.
`create_update_cl.py` has to be invoked from within a Chromium repo.
The script depends on `depot_tools` and `git` being present in the `PATH`.

Before the auto-generated CLs can be landed, some additional manual steps need
to be done first - see the section below.

## Manual steps

### `cargo vet`

The changes in the auto-generated CL need to go through a security audit, which
will ensure that `cargo vet` criteria (e.g. `ub-risk-0`, `safe-to-deploy`,
etc.). still hold for the new versions.  The CL description specifies which
criteria apply to the updated crate (the same criteria should also apply to
other, transitively-updated crates).

Review the changes to identify:

* Impact on memory safety:
    - Identify new places where `unsafe` blocks are used.  Add
      "TODO: unsafe review" code review comments to all such places.
    - Identify crates that have been modified and where `unsafe` blocks are used (the
      modifications may impact the safety preconditions of the existing `unsafe`
      blocks; and therefore the other changes may impact memory safety even if
      there are no *new* `unsafe` blocks).
* Impact on `safe-to-run`
    - Identify new places where unexpected side-effects may take place (e.g.
      unexpected uses of `std::fs` and/or `std::net`)

If your review reveals that the changes may hypotheticaly impact memory safety,
then please open a bug to track `unsafe` review and follow the review process
described in TODO.

Once the steps above are done, use `cargo vet` to record the audit results:

1. Invoke `tools/crates/run_cargo_vet.py` (TODO: provide
   more details and/or links to more details)
1. `git cl upload -m 'cargo vet'`

### Potential additional steps

* If updating `cxx`, you may need to also update its version in:
    - `build/rust/BUILD.gn`
    - `third_party/rust/cxx/v1/cxx.h`

* The script may stop early if it detects that `gnrt vendor` or `gnrt gen` have
  reported any warnings or errors (e.g. a "License file not found for crate foo"
  warning).  In this case, manual intervention is needed to finish the update
  CL.  It's probably best to finish and land the CLs created so far before
  trying to restart the script in order to create the remaining CLs.

### Landing the CL

Other than the above, the CL can go through the normal, plain-vanilla, manual
review and landing process.

1. `git cl upload`
1. Get a review from one of `//third_party/rust/OWNERS`
1. Land the CL using CQ+2
