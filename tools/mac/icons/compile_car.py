#!/usr/bin/env python3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script to compile asset catalogs into .car files for Chromium Mac.

The Chromium build system has the ability to automatically compile `.xcassets`
and `.icon` files into `.car` files and include them into outputs; see
//build/toolchain/apple/compile_xcassets.py and
https://gn.googlesource.com/gn/+/main/docs/reference.md#func_bundle_data.
However, `actool` is Mac-only, and has a wild amount of dependencies on the rest
of the Xcode package (`ibtoold` specifically, which has 70+ different
dependencies, mostly frameworks), so pre-compiled `.car` files are checked in
instead.

Full documentation can be found at //docs/mac/icons.md but briefly:

$ python3 compile_car.py chrome/app/theme/chromium/mac/Assets.xcasset
"""

import argparse
import os
import pathlib
import platform
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile
import typing


class AssetCatalogException(Exception):
    pass


def _split_version(version: str) -> tuple[int, ...]:
    return tuple(int(x) for x in version.split('.'))


def _unsplit_version(version: tuple[int, ...]) -> str:
    return '.'.join((str(x) for x in version))


_REQUIRED_ACTOOL_VERSION = _split_version('26.0')


def _verify_actool_version() -> None:
    """Verifies that the `actool` being used is suitably recent.

    Raises:
        AssetCatalogException: If `actool` is too old
    """
    command = ['xcrun', 'actool', '--output-format=xml1', '--version']
    process = subprocess.check_output(command)
    output_dict = plistlib.loads(process)

    version = _split_version(
        output_dict['com.apple.actool.version']['short-bundle-version'])

    if version < _REQUIRED_ACTOOL_VERSION:
        raise AssetCatalogException(
            'actool is too old; it is version '
            f'{_unsplit_version(version)} but at least version '
            f'{_unsplit_version(_REQUIRED_ACTOOL_VERSION)} is '
            'required')


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
    """Compiles a single `.xcassets` directory and `.icon` into a .car file.

    This function invokes `actool` to compile the given `.xcassets` directory
    and parallel .icon file. It handles the output from `actool`, checks for
    errors and unexpected warnings/notices, and copies the resulting `app.icns`
    and `Assets.car` files to the same directory as the input `.xcassets`
    directory, with names derived from the input path.

    Args:
        path: A pathlib.Path object to the .xcassets directory to process.
        min_deployment_target: The minimum macOS deployment target string to
            pass to `actool`.

    Raises:
        ValueError: If the asset catalog's path is incorrect in format.
        AssetCatalogException: If `actool` reported errors, or behaved in a way
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
        source_dir = path.joinpath(os.pardir)

        # The app icon is copied into the .car file under the name that it has
        # when given to actool, so make a copy in the temp directory to ensure
        # it has the correct name. `shutil.copytree` is used as .icon "files"
        # are really packages.
        appicon_original_path = source_dir.joinpath(f'AppIcon{name_tag}.icon')
        appicon_tmp_path = tmp_dir.joinpath('AppIcon.icon')
        shutil.copytree(appicon_original_path, appicon_tmp_path)

        command = [
            # The binary.
            'xcrun',
            'actool',

            # Output and error handling.
            '--output-format=xml1',
            '--notices',
            '--warnings',
            '--errors',

            # Platform.
            '--platform=macosx',
            '--target-device=mac',

            # Correctness. This command-line argument is undocumented. It forces
            # `actool` aka `ibtool` to use bundled versions of the asset catalog
            # frameworks so that it generates consistent results no matter what
            # OS release it is run on. Xcode 26+ includes this when invoking
            # `actool`; see various copies of the `AssetCatalogCompiler.xcspec`
            # file found in various places inside the Xcode package.
            '--lightweight-asset-runtime-mode=enabled',

            # Correctness. This command-line argument is undocumented. By
            # default, if an `.icon` file is provided to `actool`, then `actool`
            # will ignore any corresponding fallback bitmaps in the provided
            # `.xcassets` directory, and generate its own. However, `actool`
            # only generates 1x fallback bitmaps, which causes blurry icons to
            # appear on 2x screens on macOS releases prior to macOS 26, which is
            # undesirable (FB19028379). Given that there already are hand-
            # crafted icon bitmaps available in the `.xcassets` directory, stop
            # `actool` from generating its own, and have it use the available
            # ones instead.
            '--enable-icon-stack-fallback-generation=disabled',

            # Target information.
            '--app-icon=AppIcon',
            f'--minimum-deployment-target={min_deployment_target}',

            # Where to place the outputs.
            f'--output-partial-info-plist={tmp_plist}',
            f'--compile={tmp_dir}',

            # What to compile.
            path,
            appicon_tmp_path,
        ]
        if verbose:
            print(f'  Invoking: {" ".join((str(item) for item in command))}')

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
        # switching to `actool`-generated fallback bitmaps.
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
            if output_file.name == 'partial.plist':
                # Ignore the partial plist, as the Chromium plist already has
                # the required information.
                pass
            elif output_file.name == 'AppIcon.icns':
                destination_path = source_dir.joinpath(f'app{name_tag}.icns')
                if verbose:
                    print(f'  Copying output to: {destination_path}')
                shutil.copyfile(output_file, destination_path)
                pass
            elif output_file.name == 'Assets.car':
                destination_path = source_dir.joinpath(f'Assets{name_tag}.car')
                if verbose:
                    print(f'  Copying output to: {destination_path}')
                shutil.copyfile(output_file, destination_path)
            else:
                raise AssetCatalogException(
                    f'Unexpected output file: {output_file}')


def main(args: list[str]):
    _verify_actool_version()

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
            print(f'Processing: {path}')
        _process_path(pathlib.Path(path), min_deployment_target, parsed.verbose)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
