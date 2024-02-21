# Updating Rust crates used by Chromium

This document describes how Chromium updates crates.io Rust crates that Chromium
depends on.

## Automated steps

`//tools/crates/create_update_cl.sh` runs `gnrt update` and then
uploads the resulting CL to Gerrit.
`create_update_cl.sh` has to be invoked from within a Chromium repo.  This
script takes less than 3 minutes to run.
The script depends on `depot_tools` and `git` being present in the `PATH`.

Before the auto-generated CL can be landed, some additional manual steps need
to be done first - see the section below.

## Manual steps

### `gnrt vendor` and `gnrt gen`

The automated steps only run `gnrt update` and require manual invocation of
`gnrt vendor` and `gnrt gen` steps.  This approach is motivated by the risk
that `vendor` and `gen` may require a manual intervention (e.g. to work
around "License file not found for crate foo" and similar warnings).

1. Invoke `tools/crates/run_gnrt.py vendor` and address any warnings.
1. `git cl upload -m 'gnrt vendor'`
1. Invoke `tools/crates/run_gnrt.py gen` and address any warnings.
1. `git cl upload -m 'gnrt gen'`
1. Update inclusive language exceptions:
   ```
   infra/update_inclusive_language_presubmit_exempt_dirs.sh > \
     infra/inclusive_language_presubmit_exempt_dirs.txt
   ```
1. `git cl upload -m 'inclusive language exemptions'`
1. Optional: run a smoke test that verifies that the new crates build fine.
   For example - try building `chrome`.

### `cargo vet`

The changes in the auto-generated CL need to go through a security audit, which
will ensure that `cargo vet` criteria (e.g. `ub-risk-0`, `safe-to-deploy`,
etc.). still hold for the new versions.

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

### Landing the CL

Other than the above, the CL can go through the normal, plain-vanilla, manual
review and landing process.

1. `git cl upload`
1. Get a review from one of `//third_party/rust/OWNERS`
1. Land the CL using CQ+2
