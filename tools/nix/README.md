NixOS dev environment
=====================

**Note:** this is not used in production.

To get a shell with this env:

```sh
$ nix-shell
```

To run a command in this env, you can’t use `nix-shell --run`, but instead:

```sh
$ NIX_SHELL_RUN='...' nix-shell
```

To set up clangd with remote indexing support:

1. `$ NIX_SHELL_RUN='readlink /usr/bin/clangd' nix-shell`
2. Copy the path into your editor config
