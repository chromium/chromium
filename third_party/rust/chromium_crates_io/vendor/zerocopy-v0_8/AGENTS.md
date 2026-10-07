<!-- Copyright 2025 The Fuchsia Authors

Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
<LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
This file may not be copied, modified, or distributed except according to
those terms. -->

# Instructions for AI Agents

These instructions apply to work in the `zerocopy/` subtree.

## Standing Rules

- **Cargo:** For zerocopy crate build, test, and lint commands, never run
  `cargo` directly. Use `./cargo.sh`. Repository tools outside this crate may
  have their own invocation commands. See the
  [toolchain reference](../skills/zerocopy-development/references/toolchains.md)
  for toolchain selection and version gating.
- **README generation:** Do not edit `README.md` directly. It is generated from
  the top-level documentation in `src/lib.rs`. Regenerate it with:

  ```bash
  (cd .. && cargo -q run --manifest-path tools/Cargo.toml \
    -p generate-readme) > README.md
  ```

<!-- TODO-check-disable -->
- **TODOs:** Do not use `TODO` unless you intend to block the PR; CI rejects
  `TODO`. Use `FIXME` for non-blocking follow-up work.
<!-- TODO-check-enable -->

- **Documentation:** Keep documentation and instruction references synchronized
  when moving, renaming, or changing the behavior of referenced code or files.
- **File headers:** New files must use the repository copyright header with the
  file's creation year. Preserve an existing file's original creation year.
- **Formatting:** Follow `ci/check_fmt.sh`.
- **Comments and Markdown:** Wrap prose at 80 columns from the left margin when
  practical, including the comment prefix. Do not wrap tables, diagrams, long
  URLs, code blocks, or other content when wrapping would impair readability.
  In Markdown, indent wrapped bullet continuation lines by two spaces and put a
  blank line after each section heading.

## Task-Specific Skills

Use the applicable skills in addition to these standing rules. Skills compose;
when more than one applies, use all of them.

- For authoring, modifying, building, testing, or validating zerocopy code, use
  the [`zerocopy-development`](../skills/zerocopy-development/SKILL.md) skill.
- For reviewing zerocopy changes, use the
  [`zerocopy-review`](../skills/zerocopy-review/SKILL.md) skill.
- For unsafe Rust, unsafe APIs or traits, raw pointers, FFI, layout or validity
  reasoning, safety comments or `# Safety` documentation, soundness analysis,
  or invariant-bearing abstractions, use the
  [`unsafe-rust`](../skills/unsafe-rust/SKILL.md) skill. That skill is the
  source of truth for unsafe-code authoring and review methodology.
