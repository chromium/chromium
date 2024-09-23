# Guide for a Bazel Extension LSP in Chromium

## Pre-requisites

Install the [Bazel][bazel-extension] extension in VS Code.

Clone the [bazel-lsp][bazel-lsp] code to a directory of your choice:

```
export BAZEL_LSP=...
git clone https://github.com/cameron-martin/bazel-lsp.git $BAZEL_LSP
```

The Chromium src path should be exported to the `CHROMIUM_SRC` environment
variable.

**Important:** In order for the LSP to recognize the Chromium repo's root dir,
two files need to be created in the Chromium root dir:

```
export CHROMIUM_SRC=...
touch $CHROMIUM_SRC/MODULE.bazel{,.lock}
```

A working `bazel` command from the command line is required. You can test this
by running (`depot_tools` has a copy of `bazel`, so it should already be on
your `$PATH`):

```
cd $CHROMIUM_SRC
bazel info
```

## Building the LSP binary

```
cd $BAZEL_LSP
git checkout -b my_branch
git am $CHROMIUM_SRC/tools/vscode/bazel_lsp/*.patch
bazel build //:bazel-lsp -c opt
cp -f bazel-bin/bazel-lsp .
```

This produces a LSP binary at `$BAZEL_LSP/bazel-lsp`. It is important to copy
over the binary from the `bazel-bin` since the `bazel-bin` directory is a
symlink to the local Bazel cache directory and binaries there may be cleaned up
or deleted.

## Setting up the LUCI stdlib (optional)

This step is optional but very much recommended. The easiest way is to re-use
an existing `infra` or `infra_internal` directory. For example, you may already
have the stdlib checked out at a path similar to
`infra/go/src/go.chromium.org/luci/lucicfg/starlark/stdlib`. Otherwise you can
clone the `luci-go` repo with
`git clone https://chromium.googlesource.com/infra/luci/luci-go`.

The rest of this guide assumes that the path to the stdlib directory is exported
to the `$LUCI_STDLIB` environment variable.

## Using it

In your VS Code `settings.json` file, add the following with the environment
variables expanded to their full absolute paths (the stdlib path is optional):

```json
  "bazel.lsp.enabled": true,
  "bazel.lsp.command": "$BAZEL_LSP/bazel-lsp",
  "bazel.lsp.args": [
    "--lucicfg-stdlib-path",
    "$LUCI_STDLIB",
  ],
```

That's it! Hopefully now you can use "Go to definition" in VS Code for
`infra/config/*.star` files.

## Updating the patch(es)

It is likely that as the [bazel-lsp][bazel-lsp] project gets updated, there will
be conflicts in the existing patch. You can update it with:

```
cd $BAZEL_LSP
git checkout -b my_branch
git am $CHROMIUM_SRC/tools/vscode/bazel_lsp/*.patch
# Fix patch conflicts, commit new patch(es).
rm $CHROMIUM_SRC/tools/vscode/bazel_lsp/*.patch
git format-patch origin/HEAD..HEAD -o $CHROMIUM_SRC/tools/vscode/bazel_lsp
# Upload a CL to update the patches in Chromium.
```

[bazel-extension]: https://marketplace.visualstudio.com/items?itemName=BazelBuild.vscode-bazel
[bazel-lsp]: https://github.com/cameron-martin/bazel-lsp