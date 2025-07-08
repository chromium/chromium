#!/usr/bin/env python3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script to compile asset catalogs into .car files for Chromium Mac.

The Chromium build system has the ability to automatically compile
`.xcassets` files into `.car` files and include them into outputs; see
//build/toolchain/apple/compile_xcassets.py and
https://gn.googlesource.com/gn/+/main/docs/reference.md#func_bundle_data.
However, `actool` is Mac-only, and has a wild amount of dependencies on the
rest of the Xcode package (`ibtoold` specifically, which has 70+ different
dependencies, mostly frameworks), so pre-compiled `.car` files are checked in
instead.

Full documentation can be found at //docs/mac/icons.md but briefly:

$ python3 compile_car.py chrome/app/theme/chromium/mac/Assets.xcasset
"""

import argparse
import os
import pathlib
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile
import typing


class AssetCatalogException(Exception):
    pass


def _min_deployment_target() -> str:
    """Determines and returns the minimum deployment target, as determined by
    the `mac_deployment_target` value in the //build/config/mac/mac_sdk.gni
    file.

    Returns:
        The minimum deployment target, as a string value.
    """
    src_root = pathlib.Path(__file__).parent.joinpath(*((os.pardir,) * 3))
    mac_sdk_path = src_root.joinpath('build', 'config', 'mac', 'mac_sdk.gni')

    with open(mac_sdk_path, 'r') as mac_sdk_file:
        match, = re.finditer(
            r'^\s*mac_deployment_target\s*=\s*"(.*)"(?:\s*#.*)?$',
            mac_sdk_file.read(), re.MULTILINE)
        return match.group(1)


def _process_path(path: pathlib.Path, min_deployment_target: str,
                  verbose: bool) -> None:
    """Compiles a single .xcassets directory into a .car file.

    This function invokes `actool` to compile the given asset catalog. It
    handles the output from `actool`, checks for errors and unexpected
    warnings/notices, and copies the resulting `Assets.car` file to the same
    directory as the input `.xcassets` directory, with a name derived from the
    input path.

    Args:
        path: A pathlib.Path object to the .xcassets directory to process.
        min_deployment_target: The minimum macOS deployment target string to
            pass to `actool`.

    Raises:
        ValueError: If the asset catalog's path is incorrect in format.
        AssetCatalogException: If `actool` reports errors, or behaved in a way
            that was unexpected.
    """
    with tempfile.TemporaryDirectory() as tmp_dir:
        tmp_dir = pathlib.Path(tmp_dir)
        tmp_plist = tmp_dir.joinpath('partial.plist')

        # The "tag" is the '_beta' etc channel indicator in the file name.
        if not path.suffix == '.xcassets':
            raise ValueError(
                'Asset catalog filename must have .xcassets suffix')
        name_parts = path.stem.split('_')
        if len(name_parts) > 2:
            raise ValueError('Asset catalog filename must have at most one _')
        name_tag = f'_{name_parts[1]}' if len(name_parts) == 2 else ''
        source_dir = path / os.pardir

        command = [
            'xcrun', 'actool', '--output-format=xml1', '--notices',
            '--warnings', '--errors', '--platform=macosx',
            '--target-device=mac', '--app-icon=AppIcon',
            f'--minimum-deployment-target={min_deployment_target}',
            f'--output-partial-info-plist={tmp_plist}', f'--compile={tmp_dir}',
            path
        ]

        process = subprocess.Popen(
            command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        output, stderr = process.communicate()
        output_dict = plistlib.loads(output)

        failures = {}
        if process.returncode != 0:
            failures['return code'] = f'{process.returncode}'

        def collect_failures(output_dict: dict[str, typing.Any],
                             output_dict_key: str,
                             failures: dict[str, list],
                             failures_key: str,
                             filter=None) -> None:
            value = output_dict.get(output_dict_key)
            if value is not None and filter is not None:
                value = filter(value)
            if not value:  # all implicitly false values
                return
            failures[failures_key] = value

        collect_failures(output_dict, 'com.apple.actool.errors', failures,
                         'errors')
        collect_failures(output_dict, 'com.apple.actool.document.errors',
                         failures, 'document errors')
        collect_failures(output_dict, 'com.apple.actool.warnings', failures,
                         'warnings')
        # Some warnings are expected, so swallow those. Raise all others.
        # TODO(avi): Remove the "ambiguous content" warning exception when
        # moving to .icon files.
        collect_failures(
            output_dict, 'com.apple.actool.document.warnings', failures,
            'document warnings', lambda warnings: [
                warning for warning in warnings
                if warning['type'] != 'Ambiguous Content'
            ])
        # Weirdly, actool classifies any missing input files as a "notice", so
        # fail upon any "notices".
        collect_failures(output_dict, 'com.apple.actool.notices', failures,
                         'notices')
        collect_failures(output_dict, 'com.apple.actool.document.notices',
                         failures, 'document notices')

        if failures:
            # stderr is usually ignorable spew, but include it in the exception
            # if something went wrong.
            failures['stderr'] = stderr
            raise AssetCatalogException(f'actool failed; {failures}')

        compilation_results = output_dict.get(
            'com.apple.actool.compilation-results')
        if compilation_results is None:
            raise AssetCatalogException('actool had no compilation results')
        output_files = compilation_results.get('output-files')
        if output_files is None:
            raise AssetCatalogException('actool had no output files')
        if len(output_files) != 3:
            raise AssetCatalogException(
                'expected actool to output 3 files, but it instead output '
                f'{len(output_files)} files, namely {output_files}')

        # Exactly three output files are expected; handle them each
        # appropriately.
        for output_file in output_files:
            output_file = pathlib.Path(output_file)
            match output_file.name:
                case 'partial.plist':
                    # Ignore the partial plist, as the Chromium plist already
                    # has the required information.
                    pass
                case 'AppIcon.icns':
                    # For now, ignore the generated icon file in favor of
                    # keeping the existing hand-crafted icns file.
                    #
                    # TODO(avi): When moving to .icon files, uncomment.
                    #
                    # destination_path = source_dir.joinpath(
                    #     f'app{name_tag}.icns')
                    # if verbose:
                    #     print(f'  Copying output to {destination_path}')
                    # shutil.copyfile(output_file, destination_path)
                    pass
                case 'Assets.car':
                    destination_path = source_dir.joinpath(
                        f'Assets{name_tag}.car')
                    if verbose:
                        print(f'  Copying output to {destination_path}')
                    shutil.copyfile(output_file, destination_path)
                case _:
                    raise AssetCatalogException(
                        f'Unexpected output file: {output_file}')


def main(args: list[str]):
    parser = argparse.ArgumentParser()
    parser.add_argument(
        'paths',
        nargs='+',
        metavar='path',
        help='the path to the .xcassets to process')
    parser.add_argument(
        '-v',
        '--verbose',
        dest='verbose',
        action='store_true',
        help='enable verbose output')
    parsed = parser.parse_args(args)

    min_deployment_target = _min_deployment_target()
    if parsed.verbose:
        print('Determined the minimum deployment target to be '
              f'{min_deployment_target}.')

    for path in parsed.paths:
        if parsed.verbose:
            print(f'Processing {path} ...')
        _process_path(pathlib.Path(path), min_deployment_target, parsed.verbose)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
