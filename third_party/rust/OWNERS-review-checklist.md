# Chromium Review Checklist for `//third_party/rust`

`//third_party/rust/OWNERS` review crates in this directory
(e.g. when importing a new crate,
or when updating an existing crate to a new version).

This document provides a checklist that

1) helps the reviewers remember what to look for, and
2) helps to set the expectations
   so that the reviews provide the right level of assurance and
   take the right amount of reviewer time
   (not too little, but also not too much).

These are very high level guidelines that explicitly avoid
trying to codify what exactly to look for and how to do the reviews,
because ultimately we trust the reviewer’s judgement here.

## Review checklist

* **ATL approval**: Is it an import of a new crate? If so, then check if 1) ATL
  approval has been obtained, and 2) that an `OWNERS` file for the new crate is
  present (unless it is a foundational, shared-ownership crate).
    - High-level goal: Ensure that an LGTM from `//third_party/rust/OWNERS`
      doesn’t accidentally bypass the requirement for a generic third-party
      review by `//third_party/OWNERS`.
    - If the new crate is a new transitive dependency of an already approved
      crate, then we should send an FYI email to ATLs (as discussed
      [here](https://groups.google.com/a/google.com/g/chrome-atls-discuss/c/xa-tFeJ6BnE/m/vkg89NC1AQAJ)).

* **`unsafe` code**: Does the crate use `unsafe` Rust?  If so, then check that
  the `unsafe` code is small, encapsulated, and documented as explained in
  [the `rule-of-2.md` doc](https://chromium.googlesource.com/chromium/src/+/main/docs/security/rule-of-2.md#unsafe-code-in-safe-languages).
    - High-level goals:
        - Ensure that one of `//third_party/rust/OWNERS` looks
          at each `unsafe` block of code.
        - Introduce some friction for importing libraries with worrying
          or unnecessary use of `unsafe`.
    - Not a goal:
        - Leaving a written trail that for each `unsafe` block explains
          reviewer's thinking why the safety requirements are met.
        - Spending more than a minute or two per `unsafe` block

* **Proc macros and `build.rs`**: Does the crate execute any code at build time?
  If so, then check if the build is still deterministic and hermetic.
    - High-level goal: Avoid surprises and/or identify extra complexity early.
      We want to preemptively review this aspect even though in theory
      non-hermetic or non-deterministic builds should be caught by Chromium CI
      at a later point, and depending on crates like `cc` wouldn’t work with
      Chromium’s build system at all.

* **Other concerns**: Do you have any (generic or specific) concerns or doubts
  about some aspects of the crate?  If so, please don’t hesitate to escalate and
  discuss with others, and then LGTM (or not) based on the discussion.
    - Possible escalation routes:
        - For any question:
          [Cr2O3 chatroom](https://chat.google.com/room/AAAAk1UCFGg?cls=7)
          [Google-internal]
        - For `unsafe` questions:
          [Unsafe Rust Crabal chatroom](https://chat.google.com/room/AAAAhLsgrQ4?cls=7)
          [Google-internal]

## Other notes

* The review can be quite minimal or lightweight if Chromium already has a trust
  relationship with the crate authors.  For example, we already trust:
    - Rust maintainers - e.g. we trust `libc`, `hashbrown`, and similar crates
      authored by https://github.com/rust-lang
    - OS SDKs - e.g. we trust `windows-sys`, `windows_aarch64_msvc` and similar
      crates that are authored by Microsoft and expose OS APIs to Rust
    - Google or Chromium engineers - e.g. Fontations, ICU4x, etc.

* If needed the review can be scoped down by using `ban_features` in
  `gnrt_config.toml` to ensure that a given crate feature is disabled.

* There is no need to review tests, benchmarks, nor examples.

* To quickly check if a crate uses `unsafe` Rust, one can look at the
  value of `allow_unsafe` in the crate's `BUILD.gn` file
  (see [an example here](https://source.chromium.org/chromium/chromium/src/+/main:third_party/rust/png/v0_18/BUILD.gn;l=47;drc=6b4b18e214c4a226ce7ed37a9faeebee2e628daf).

* Tools that may be helpful during a review:
    - `tools/crates/grep_for_vet_relevant_keywords.sh`
    - Tools for looking at a diff when updating a crate to a new version
        - <https://diff.rs/>
        - In Gerrit, you can use patchset 2
          (results of `git mv old_dir new_dir`)
          as the baseline.
          (This should become easier once we fix https://crbug.com/396397336.)

## Explicitly *not* part of the review checklist

* **Licensing**: `gnrt` already checks for known/approved licenses when
  generating `README.chromium`.  If an import requires updating
  `gnrt/lib/readme.rs` to account for new license kinds, then
  `//tools/crates/gnrt/lib/readme.rs-third-party-license-review.md`
  can say how to review the changes.

## Other related docs

* Corresponding checklist used by Android:
  [go/android-rust-reviewing-crates](https://goto2.corp.google.com/android-rust-reviewing-crates)
  [Google-internal]
* [Initial draft of this checklist](https://docs.google.com/document/d/1WIwaifxnNK2slmIDoADJX8yeVtoW23yIIxe5F2s0lEg/edit?usp=sharing&resourcekey=0-NMY_YlQzZiN-trOQuVYFOw)
  [Google internal]
