NixOS dev environment
=====================

**Note:** this is not used in production.

## Getting and using a shell

To get a shell with this env:

```sh
$ nix-shell
```

or

```sh
$ nix develop
```

To run a command in this env, you can’t use `nix-shell --run`, but instead:

```sh
$ NIX_SHELL_RUN='...' nix-shell
```

To set up clangd with remote indexing support:

1. `$ NIX_SHELL_RUN='readlink /usr/bin/clangd' nix-shell`
2. Copy the path into your editor config

## Updating dependencies

Periodically, we'll want to update dependencies to keep them relatively fresh.
To do that:

```sh
$ nix run github:NixOS/nixpkgs\#npins -- update
```

Then commit the new `sources.json`.

## Rolling back dependencies

If something breaks due to a bad deps update, rolling back is as simple as
reverting to an earlier version of `sources.json`.
