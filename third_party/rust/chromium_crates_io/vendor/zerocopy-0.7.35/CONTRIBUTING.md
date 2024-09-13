<!-- Copyright 2022 The Fuchsia Authors

Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
<LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
This file may not be copied, modified, or distributed except according to
those terms. -->

# How to Contribute

We'd love to accept your patches and contributions to zerocopy. There are just a
few small guidelines you need to follow.

Once you've read the rest of this doc, check out our [good-first-issue
label][good-first-issue] for some good issues you can use to get your toes wet!

## Contributor License Agreement

Contributions to this project must be accompanied by a Contributor License
Agreement. You (or your employer) retain the copyright to your contribution;
this simply gives us permission to use and redistribute your contributions as
part of the project. Head over to <https://cla.developers.google.com/> to see
your current agreements on file or to sign a new one.

You generally only need to submit a CLA once, so if you've already submitted one
(even if it was for a different project), you probably don't need to do it
again.

## Code Reviews

All submissions, including submissions by project members, require review. We
use GitHub pull requests for this purpose. Consult [GitHub
Help][about_pull_requests] for more information on using pull requests.

## Code Guidelines

### Philosophy

This section is inspired by [Flutter's style guide][flutter_philosophy], which
contains many general principles that you should apply to all your programming
work. Read it. The below calls out specific aspects that we feel are
particularly important.

#### Dogfood Your Features

In non-library code, it's often advised to only implement features you need.
After all, it's hard to correctly design code without a concrete use case to
guide its design. Since zerocopy is a library, this advice is not as applicable;
we want our API surface to be featureful and complete even if not every feature
or method has a known use case. However, the observation that unused code is
hard to design still holds.

Thus, when designing external-facing features, try to make use of them somehow.
This could be by using them to implement other features, or it could be by
writing prototype code which won't actually be checked in anywhere. If you're
feeling ambitious, you could even add (and check in) a [Cargo
example][cargo_example] that exercises the new feature.

#### Go Down the Rabbit Hole

You will occasionally encounter behavior that surprises you or seems wrong. It
probably is! Invest the time to find the root cause - you will either learn
something, or fix something, and both are worth your time. Do not work around
behavior you don't understand.

### Avoid Duplication

Avoid duplicating code whenever possible. In cases where existing code is not
exposed in a manner suitable to your needs, prefer to extract the necessary
parts into a common dependency.

### Comments

When writing comments, take a moment to consider the future reader of your
comment. Ensure that your comments are complete sentences with proper grammar
and punctuation. Note that adding more comments or more verbose comments is not
always better; for example, avoid comments that repeat the code they're anchored
on.

Documentation comments should be self-contained; in other words, do not assume
that the reader is aware of documentation in adjacent files or on adjacent
structures. Avoid documentation comments on types which describe _instances_ of
the type; for example, `AddressSet is a set of client addresses.` is a comment
that describes a field of type `AddressSet`, but the type may be used to hold
any kind of `Address`, not just a client's.

Phrase your comments to avoid references that might become stale; for example:
do not mention a variable or type by name when possible (certain doc comments
are necessary exceptions). Also avoid references to past or future versions of
or past or future work surrounding the item being documented; explain things
from first principles rather than making external references (including past
revisions).

When writing TODOs:

1. Include an issue reference using the format `TODO(#123):`
1. Phrase the text as an action that is to be taken; it should be possible for
   another contributor to pick up the TODO without consulting any external
   sources, including the referenced issue.

### Tests

Much of the code in zerocopy has the property that, if it is buggy, those bugs
may not cause user code to fail. This makes it extra important to write thorough
tests, but it also makes it harder to write those tests correctly. Here are some
guidelines on how to test code in zerocopy:
1. All code added to zerocopy must include tests that exercise it completely.
1. Tests must be deterministic. Threaded or time-dependent code, random number
   generators (RNGs), and communication with external processes are common
   sources of nondeterminism. See [Write reproducible, deterministic
   tests][determinism] for tips.
1. Avoid [change detector tests][change_detector_tests]; tests that are
   unnecessarily sensitive to changes, especially ones external to the code
   under test, can hamper feature development and refactoring.
1. Since we run tests in [Miri][miri], make sure that tests exist which exercise
   any potential [undefined behavior][undefined_behavior] so that Miri can catch
   it.
1. If there's some user code that should be impossible to compile, add a
   [trybuild test][trybuild] to ensure that it's properly rejected.

### Source Control Best Practices

Commits should be arranged for ease of reading; that is, incidental changes
such as code movement or formatting changes should be committed separately from
actual code changes.

Commits should always be focused. For example, a commit could add a feature,
fix a bug, or refactor code, but not a mixture.

Commits should be thoughtfully sized; avoid overly large or complex commits
which can be logically separated, but also avoid overly separated commits that
require code reviews to load multiple commits into their mental working memory
in order to properly understand how the various pieces fit together.

#### Commit Messages

Commit messages should be _concise_ but self-contained (avoid relying on issue
references as explanations for changes) and written such that they are helpful
to people reading in the future (include rationale and any necessary context).

Avoid superfluous details or narrative.

Commit messages should consist of a brief subject line and a separate
explanatory paragraph in accordance with the following:

1. [Separate subject from body with a blank line](https://chris.beams.io/posts/git-commit/#separate)
1. [Limit the subject line to 50 characters](https://chris.beams.io/posts/git-commit/#limit-50)
1. [Capitalize the subject line](https://chris.beams.io/posts/git-commit/#capitalize)
1. [Do not end the subject line with a period](https://chris.beams.io/posts/git-commit/#end)
1. [Use the imperative mood in the subject line](https://chris.beams.io/posts/git-commit/#imperative)
1. [Wrap the body at 72 characters](https://chris.beams.io/posts/git-commit/#wrap-72)
1. [Use the body to explain what and why vs. how](https://chris.beams.io/posts/git-commit/#why-not-how)

If the code affects a particular subsystem, prefix the subject line with the
name of that subsystem in square brackets, omitting any "zerocopy" prefix
(that's implicit). For example, for a commit adding a feature to the
zerocopy-derive crate:

```text
[derive] Support AsBytes on types with parameters
```

The body may be omitted if the subject is self-explanatory; e.g. when fixing a
typo. The git book contains a [Commit Guidelines][commit_guidelines] section
with much of the same advice, and the list above is part of a [blog
post][beams_git_commit] by [Chris Beams][chris_beams].

Commit messages should make use of issue integration. Including an issue
reference like `#123` will cause the GitHub UI to link the text of that
reference to the referenced issue, and will also make it so that the referenced
issue back-links to the commit. Use "Closes", "Fixes", or "Resolves" on its own
line to automatically close an issue when your commit is merged:

```text
Closes #123
Fixes #123
Resolves #123
```

When using issue integration, don't omit necessary context that may also be
included in the relevant issue (see "Commit messages should be _concise_ but
self-contained" above). Git history is more likely to be retained indefinitely
than issue history (for example, if this repository is migrated away from GitHub
at some point in the future).

Commit messages should never contain references to any of:

1. Relative moments in time
1. Non-public URLs
1. Individuals
1. Hosted code reviews (such as on https://github.com/google/zerocopy/pulls)
    + Refer to commits in this repository by their SHA-1 hash
    + Refer to commits in other repositories by public web address (such as
      https://github.com/google/zerocopy/commit/789b3deb)
1. Other entities which may not make sense to arbitrary future readers

## Community Guidelines

This project follows [Google's Open Source Community
Guidelines][google_open_source_guidelines].

[about_pull_requests]: https://help.github.com/articles/about-pull-requests/
[beams_git_commit]: https://chris.beams.io/posts/git-commit/
[cargo_example]: http://xion.io/post/code/rust-examples.html
[change_detector_tests]: https://testing.googleblog.com/2015/01/testing-on-toilet-change-detector-tests.html
[chris_beams]: https://chris.beams.io/
[commit_guidelines]: https://www.git-scm.com/book/en/v2/Distributed-Git-Contributing-to-a-Project#_commit_guidelines
[determinism]: https://fuchsia.dev/fuchsia-src/contribute/testing/best-practices#write_reproducible_deterministic_tests
[flutter_philosophy]: https://github.com/flutter/flutter/wiki/Style-guide-for-Flutter-repo#philosophy
[good-first-issue]: https://github.com/google/zerocopy/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22
[google_open_source_guidelines]: https://opensource.google/conduct/
[magic_number]: https://en.wikipedia.org/wiki/Magic_number_(programming)
[miri]: https://github.com/rust-lang/miri
[trybuild]: https://crates.io/crates/trybuild
[undefined_behavior]: https://raphlinus.github.io/programming/rust/2018/08/17/undefined-behavior.html
