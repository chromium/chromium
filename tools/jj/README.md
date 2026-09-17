# jj for chromium
jj is a Git-compatible version control system. It is not yet officially
supported by ChOps, but may be in the future.

This directory contains documentation and configuration to support jj in
chromium.

## Getting started
1.  Join [the mailing list](https://groups.google.com/g/chromium-jj-users). This
    is very important, as this is how we will communicate important information.
    Note that this is not an official group.
2.  Follow go/jj-in-chromium (only currently open to googlers). Further
    instructions will be added later which also support external contributors.
3.  Optional: Try the
    [official jj tutorial](https://jj-vcs.github.io/jj/prerelease/tutorial/) or
    [the unofficial one](https://steveklabnik.github.io/jujutsu-tutorial/)

## Using chromium with jj
### Working with commits
Most commands for working locally with commits work just fine. The one
exception to this is that Git submodules are not yet supported by jj.

This means whenever you see jj say "ignoring git submodule" (generally only
when you switch between submitted commits), you will need to run `gclient sync`.

### Syncing code
`jj sync` (using the config in `tools/jj/config.toml`)

Fetches `origin/main`, rebases the current change's branch onto `trunk()` (or
all mutable changes with `-a`), and runs `gclient sync -D`.

If a sync (or any other operation) results in a conflict, you can resolve the
conflict by directly editing the conflicted file(s) and/or using `jj resolve`.

If you would like to look at the pre-conflict state of a change (e.g. for a
pre-jj dormant branch) you can use `jj evolog -r <revision>` to find its
last unconflicted revision (e.g. `<change_id>/10`).

### Uploading code
`jj upload` (using the config in `tools/jj/config.toml`)

Runs `jj fix` and `git cl presubmit` before pushing changes to Gerrit.
Presubmits are skipped when uploading multiple revisions at once.

### Managing bugs
`jj bug add`

Attaches `Bug:` or `Fixed:` trailers to commits, with support for inheriting
from parent commits or searching Buganizer via `bugged`.

### Creating secondary workspaces
`jj new-chromium-workspace <path>`

Creates a new workspace directory with a linked `.gclient` and a `jj` workspace
at `<path>/src`. Run `gclient sync` inside the new workspace afterward to sync
submodules.

### Gerrit review URL
`jj gerrit-url`

Prints Gerrit review URLs for revisions based on their `Change-Id`.

### Running formatters
`jj fix` (using the config in `tools/jj/config.toml`)
