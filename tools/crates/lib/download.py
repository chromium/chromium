# python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""The download module implements the download action.

The download action will download a crate from crates.io and unpack it into
`third_party/rust/`."""

from __future__ import annotations

from lib import cargo
from lib import common
from lib import consts

import argparse
import certifi
from functools import partial
import io
import os
import re
import shutil
import sys
import tarfile
import tempfile
import toml
import urllib3


class UntarAbsolutePathError(Exception):
    def __init__(self, path: str):
        self.path = path


class DownloadError(Exception):
    pass


class NeedLicenseError(Exception):
    def __init__(self, crate_license: str):
        self.crate_license = crate_license


def run(args: argparse.Namespace):
    """Entry point for the the 'download' action."""
    if _check_if_crate_is_blocked(args.crate_name):
        exit(1)

    full_version = _find_crate_full_version(args.crate_name, args.crate_version,
                                            args.verbose)

    crate_tarball = _download_crate(args.crate_name, full_version)
    if not crate_tarball:
        exit(1)

    try:
        try:
            _make_dirs_for_crate(args.crate_name, full_version)
        except FileExistsError as e:
            print("Unable to make directory {} as it already exists".format(
                e.filename),
                  file=sys.stderr)
            raise DownloadError

        try:
            _untar_crate(args.crate_name, full_version, crate_tarball)
        except UntarAbsolutePathError as e:
            print("Error: Crate has file at an absolute path!", file=sys.stderr)
            print("    " + e.path, file=sys.stderr)
            raise DownloadError

        # This expects to find the untar'd crate with its Cargo.toml present to
        # read.
        try:
            readme_contents = _gen_readme(args, args.crate_name, full_version)
        except NeedLicenseError as e:
            print("Error: --license is required to override Cargo.toml "
                  "value of \"{}\" (or add this to "
                  "lib.consts.ALLOWED_LICENSES)".format(e.crate_license),
                  file=sys.stderr)
            raise DownloadError

        readme_path = common.os_crate_version_dir(args.crate_name,
                                                  full_version,
                                                  rel_path=["README.chromium"])
        with open(readme_path, "w") as readme_file:
            readme_file.write(readme_contents)
        print("Downloaded {} {} to {}".format(
            args.crate_name, full_version,
            common.os_crate_version_dir(args.crate_name, full_version)))

    except DownloadError:
        # Remove the crate-name/vX/crate/ dir which we have downloaded, but
        # nothing else, in case there's patches/ or something there.
        shutil.rmtree(common.os_crate_cargo_dir(args.crate_name, full_version))
        # Try remove the crate-name/vX/ dir if it's empty, but there may be
        # patches/ or other stuff there if we're updating an existing crate to a
        # new vers so it may fail.
        try:
            shutil.rmtree(
                common.os_crate_version_dir(args.crate_name, full_version))
        except:
            pass
        # Also try remove the crate-name/ dir, but there might be other versions
        # present so it can fail.
        try:
            os.rmdir(common.os_crate_name_dir(args.crate_name))
        except:
            pass


def _gen_readme(args: argparse.Namespace, crate_name: str, version: str) -> str:
    """Generate the contents of a README.chromium file for a crate."""
    cargo = common.load_toml(
        common.os_crate_cargo_dir(crate_name, version, rel_path=["Cargo.toml"]))

    if args.license:
        license = args.license
    else:
        crate_license = cargo["package"]["license"]
        license = None
        for allow in consts.ALLOWED_LICENSES:
            if allow[0] == crate_license:
                license = allow[1]
                break
    if not license:
        raise NeedLicenseError(crate_license)

    return consts.README_CHROMIUM.format(
        crate_name=cargo["package"]["name"],
        url=common.crate_view_url(cargo["package"]["name"]),
        description=cargo["package"]["description"].rstrip(),
        version=cargo["package"]["version"],
        security=args.security_critical,
        license=license,
    )


def _check_if_crate_is_blocked(crate_name: str) -> bool:
    """Checks whether a crate is considered blocked (and should not be used).

    Prints a message and returns True if it is.
    """
    if crate_name in consts.BLOCKED_CRATES:
        reason = consts.BLOCKED_CRATES[crate_name]
        print("The crate \"{}\" is blocked and should not be downloaded: {}".
              format(crate_name, reason),
              file=sys.stderr)
        return True
    return False


def _find_crate_full_version(crate_name: str, partial_version: str,
                             verbose: bool) -> str:
    """Look up the latest matching version from crates.io.

    Returns:
        If `partial_version` is a full semver (1.2.3), then that is returned
        immediately. Always returns a full version with 3 components, which
        will be determined from crates.io."""
    # Find the version we want to download from crates.io.
    if common.version_is_complete(partial_version):
        return partial_version

    # Go to crates.io through `cargo tree`.
    with tempfile.TemporaryDirectory() as workdir:
        cargo_toml_path = os.path.join(workdir, "Cargo.toml")

        # Generate a fake Cargo.toml which depends on the crate and version.
        toml_version = {"dependencies": {crate_name: partial_version}}
        cargo.write_cargo_toml_in_tempdir(
            workdir,
            None,
            orig_toml_parsed=cargo.add_required_cargo_fields(toml_version),
            verbose=verbose)
        # `cargo tree` will tell us the actual version number of the dependency,
        # finding the latest matching version on crates.io.
        out = cargo.run_cargo_tree(cargo_toml_path,
                                   cargo.CrateBuildOutput.NORMAL, None, 1, [])
        # Depth 1 should give only two output lines.
        assert len(out) == 2

    m = re.search(consts.CARGO_DEPS_REGEX, out[1])
    # If these fail, we have invalid output from `cargo tree`?
    assert m
    assert m.group("version")
    return m.group("version")


def _download_crate(crate_name: str, version: str) -> bytes:
    """Downloads a crate from crates.io and returns it as `bytes`.

    Returns:
        The `bytes` of the downloaded crate tarball, or None if the download
        fails.
    """
    url = common.crate_download_url(crate_name, version)
    http = urllib3.PoolManager(cert_reqs="CERT_REQUIRED",
                               ca_certs=certifi.where())
    resp = http.request("GET", url)
    if resp.status != 200:
        print("Unable to download {}, status {}".format(url, resp.status),
              file=sys.stderr)
        return None
    return resp.data


def _make_dirs_for_crate(crate_name: str, version: str) -> bool:
    """Recursively make directories to hold a downloaded crate."""
    # This is the crate-name/vX/ directory, where the BUILD.gn lives and any
    # patches/ directory that are locally applied to the crate.
    ver_dir = common.os_crate_version_dir(crate_name, version)
    # This is the dir inside the crate-name/vX/ directory where the crate's
    # contents will be extracted. If it already exists, we can't download and
    # extract the crate, as we'd end up with a mixture of files.
    cargo_dir = common.os_crate_cargo_dir(crate_name, version)
    if ver_dir != cargo_dir:
        try:
            os.makedirs(ver_dir)
        except FileExistsError:
            pass
    os.mkdir(cargo_dir)


def _untar_crate(crate_name: str, version: str, crate_tarball: bytes):
    """Untar a downloaded crate tarball."""
    with tarfile.open(mode="r", fileobj=io.BytesIO(crate_tarball)) as contents:
        for m in contents.getmembers():
            # Tar files always have "/" as a path separator.
            if m.name.startswith("/") or m.name.startswith(".."):
                raise UntarAbsolutePathError(m.name)
            # Drop the first path component, which is the crate's name-version.
            m.name = re.sub("^.+?/", "", m.name)
        contents.extractall(path=common.os_crate_cargo_dir(crate_name, version))
