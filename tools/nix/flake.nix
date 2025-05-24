# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  inputs = { };
  outputs =
    { self }:
    let
      devShell = system: {
        "${system}".default = import ./make-shell-for-system.nix {
          inherit system;
        } { };
      };
    in
    {
      devShells = devShell "aarch64-linux" // devShell "x86_64-linux";
    };
}
