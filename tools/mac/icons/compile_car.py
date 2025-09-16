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


# A minimum `actool` version. It's likely that versions from earlier releases of
# Xcode 26 would work, but they are untested. This is the last version (as of
# writing, mid-August 2025) that is capable of emitting a compatibility `.icns`
# file.
_ACTOOL_26B4_VERSION = _split_version('24112')
# The `actool` version of Xcode 26.0.
_ACTOOL_26_VERSION = _split_version('24127')


class CompatibilityExpectations:
    __slots__ = [
        # Through Xcode 26b6, the `--enable-icon-stack-fallback-generation` +
        # `--include-all-app-icons` workaround can be used to include existing
        # app icon bitmaps for use on macOS <26 while including the macOS 26
        # icon. Xcode 26b4 also correctly includes a backwards-compatibility
        # `.icns` file, but Xcode 26b5 and later no longer include that file and
        # instead log an error. See FB19772616 for a general filing of
        # grievances about being unable to specify bitmaps, though (for obvious
        # reasons) it doesn't go into detail about undocumented command-line
        # flags.
        #
        # This slot is set to `True` if `actool` is expected to create an
        # `.icns` file, `False` if it is not expected to create the file, and
        # `None` if it is unknown whether or not the file will be created.
        'icns_expected'
    ]


def _verify_actool_version() -> CompatibilityExpectations:
    """Verifies that the `actool` being used is suitably recent.

    Returns:
        A CompatibilityExpectations object

    Raises:
        AssetCatalogException: If `actool` is too old
    """
    command = ['xcrun', 'actool', '--output-format=xml1', '--version']
    process = subprocess.check_output(command)
    output_dict = plistlib.loads(process)

    version = _split_version(
        output_dict['com.apple.actool.version']['bundle-version'])

    if version < _ACTOOL_26B4_VERSION:
        raise AssetCatalogException(
            f'actool is too old; it is version {_unsplit_version(version)} but '
            f'at least version {_unsplit_version(_ACTOOL_26B4_VERSION)} is '
            'required. Install at least Xcode 26b4.')

    compatibility_expectations = CompatibilityExpectations()

    if version == _ACTOOL_26B4_VERSION:
        compatibility_expectations.icns_expected = True
    elif version <= _ACTOOL_26_VERSION:
        print(
            '⚠️  Compatibility warning: the active version of `actool` will '
            'not emit an\n`.icns` file. If an `.icns` file is required, use '
            'Xcode 26b4.',
            file=sys.stderr)
        compatibility_expectations.icns_expected = False
    else:
        print(
            '⚠️  Compatibility warning: it is unknown if the active version '
            'of `actool`\nwill emit an `.icns` file. If an `.icns` file is '
            'required, use Xcode 26b4.',
            file=sys.stderr)
        compatibility_expectations.icns_expected = None

    return compatibility_expectations


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
                  compatibility_expectations: CompatibilityExpectations,
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

            # Correctness. The `--enable-icon-stack-fallback-generation`
            # command-line argument is undocumented. By default, if an `.icon`
            # file is provided to `actool`, then `actool` will ignore any
            # corresponding fallback bitmaps in the provided `.xcassets`
            # directory, and generate its own. With that command-line argument
            # specified, `actool` will use the ones in the provided `.xcassets`
            # directory instead.
            #
            # With Xcode 26b4, the first command-line argument is all that is
            # needed, and a backward-compatibility `.icns` file will be
            # generated. With 26b5 and 26b6, adding the
            # `--include-all-app-icons` argument is required to include the
            # provided fallback bitmaps, and even then there will be no `.icns`
            # file generated. (Adding that argument is harmless on 26b4.)
            #
            # See the discussion at
            # https://mjtsai.com/blog/2025/08/08/separate-icons-for-macos-tahoe-vs-earlier/
            # and specifically the toots at
            # https://mas.to/@avidrissman/114989207727177911 and
            # https://mastodon.social/@vslavik/115016258774715162 .
            '--enable-icon-stack-fallback-generation=disabled',
            '--include-all-app-icons',

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
        #
        # If an `.icns` file is expected, then raise on errors to create it. If
        # the file is not expected, swallow the error that is generated when
        # trying to create it, as that error is expected.
        if compatibility_expectations.icns_expected:
            filter = lambda warnings: [
                warning for warning in warnings
                if warning['type'] != 'Ambiguous Content'
            ]
        else:
            filter = lambda warnings: [
                warning for warning in warnings
                if warning['type'] != 'Ambiguous Content' and not (warning[
                    'type'] == 'Unsupported Configuration' and warning[
                        'message'].startswith(
                            'Failed to generate flattened icon stack'))
            ]
        collect_failures(output_dict, 'com.apple.actool.document.warnings',
                         failures, 'document warnings', filter)
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
        if compatibility_expectations.icns_expected is not None:
            expected_file_count = (3 if compatibility_expectations.icns_expected
                                   else 2)
            if len(output_files) != expected_file_count:
                raise AssetCatalogException(
                    f'expected actool to output {expected_file_count} files, '
                    f'but it instead output {len(output_files)} files, namely '
                    f'{output_files}')

        # Exactly three output files are expected; handle them each
        # appropriately.
        for output_file in output_files:
            output_file = pathlib.Path(output_file)
            if output_file.name == 'partial.plist':
                # Ignore the partial plist, as the Chromium plist already has
                # the required information.
                pass
            elif output_file.name == 'AppIcon.icns':
                if compatibility_expectations.icns_expected is None:
                    print(
                        '⚠️ `.icns` file generated; FB19772616 addressed?',
                        file=sys.stderr)
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
    compatibility_expectations = _verify_actool_version()

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
        _process_path(
            pathlib.Path(path), min_deployment_target,
            compatibility_expectations, parsed.verbose)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
