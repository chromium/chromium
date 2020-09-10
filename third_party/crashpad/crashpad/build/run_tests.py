#!/usr/bin/env python
# coding: utf-8

# Copyright 2014 The Crashpad Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from __future__ import print_function

import argparse
import os
import pipes
import posixpath
import re
import subprocess
import sys
import tempfile
import uuid

CRASHPAD_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                            os.pardir)
IS_WINDOWS_HOST = sys.platform.startswith('win')


def _FindGNFromBinaryDir(binary_dir):
    """Attempts to determine the path to a GN binary used to generate the build
    files in the given binary_dir. This is necessary because `gn` might not be
    in the path or might be in a non-standard location, particularly on build
    machines."""

    build_ninja = os.path.join(binary_dir, 'build.ninja')
    if os.path.isfile(build_ninja):
        with open(build_ninja, 'rb') as f:
            # Look for the always-generated regeneration rule of the form:
            #
            # rule gn
            #   command = <gn binary> ... arguments ...
            #
            # to extract the gn binary's full path.
            found_rule_gn = False
            for line in f:
                if line.strip() == 'rule gn':
                    found_rule_gn = True
                    continue
                if found_rule_gn:
                    if len(line) == 0 or line[0] != ' ':
                        return None
                    if line.startswith('  command = '):
                        gn_command_line_parts = line.strip().split(' ')
                        if len(gn_command_line_parts) > 2:
                            return os.path.join(binary_dir,
                                                gn_command_line_parts[2])

    return None


def _BinaryDirTargetOS(binary_dir):
    """Returns the apparent target OS of binary_dir, or None if none appear to
    be explicitly specified."""

    gn_path = _FindGNFromBinaryDir(binary_dir)

    if gn_path:
        # Look for a GN “target_os”.
        popen = subprocess.Popen([
            gn_path, '--root=' + CRASHPAD_DIR, 'args', binary_dir,
            '--list=target_os', '--short'
        ],
                                 shell=IS_WINDOWS_HOST,
                                 stdout=subprocess.PIPE,
                                 stderr=open(os.devnull))
        value = popen.communicate()[0]
        if popen.returncode == 0:
            match = re.match('target_os = "(.*)"$', value.decode('utf-8'))
            if match:
                return match.group(1)

    # For GYP with Ninja, look for the appearance of “linux-android” in the path
    # to ar. This path is configured by gyp_crashpad_android.py.
    build_ninja_path = os.path.join(binary_dir, 'build.ninja')
    if os.path.exists(build_ninja_path):
        with open(build_ninja_path) as build_ninja_file:
            build_ninja_content = build_ninja_file.read()
            match = re.search('-linux-android(eabi)?-ar$', build_ninja_content,
                              re.MULTILINE)
            if match:
                return 'android'

    return None


def _EnableVTProcessingOnWindowsConsole():
    """Enables virtual terminal processing for ANSI/VT100-style escape sequences
    on a Windows console attached to standard output. Returns True on success.
    Returns False if standard output is not a console or if virtual terminal
    processing is not supported. The feature was introduced in Windows 10.
    """

    import pywintypes
    import win32console
    import winerror

    stdout_console = win32console.GetStdHandle(win32console.STD_OUTPUT_HANDLE)
    try:
        console_mode = stdout_console.GetConsoleMode()
    except pywintypes.error as e:
        if e.winerror == winerror.ERROR_INVALID_HANDLE:
            # Standard output is not a console.
            return False
        raise

    try:
        # From <wincon.h>. This would be
        # win32console.ENABLE_VIRTUAL_TERMINAL_PROCESSING, but it’s too new to
        # be defined there.
        ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004

        stdout_console.SetConsoleMode(console_mode |
                                      ENABLE_VIRTUAL_TERMINAL_PROCESSING)
    except pywintypes.error as e:
        if e.winerror == winerror.ERROR_INVALID_PARAMETER:
            # ANSI/VT100-style escape sequence processing isn’t supported before
            # Windows 10.
            return False
        raise

    return True


def _RunOnAndroidTarget(binary_dir, test, android_device, extra_command_line):
    local_test_path = os.path.join(binary_dir, test)
    MAYBE_UNSUPPORTED_TESTS = (
        'crashpad_client_test',
        'crashpad_handler_test',
        'crashpad_minidump_test',
        'crashpad_snapshot_test',
    )
    if not os.path.exists(local_test_path) and test in MAYBE_UNSUPPORTED_TESTS:
        print('This test is not present and may not be supported, skipping')
        return

    def _adb(*args):
        # Flush all of this script’s own buffered stdout output before running
        # adb, which will likely produce its own output on stdout.
        sys.stdout.flush()

        adb_command = ['adb', '-s', android_device]
        adb_command.extend(args)
        subprocess.check_call(adb_command, shell=IS_WINDOWS_HOST)

    def _adb_push(sources, destination):
        args = list(sources)
        args.append(destination)
        _adb('push', *args)

    def _adb_shell(command_args, env={}):
        # Build a command to execute via “sh -c” instead of invoking it
        # directly. Here’s why:
        #
        # /system/bin/env isn’t normally present prior to Android 6.0 (M), where
        # toybox was introduced (Android platform/manifest 9a2c01e8450b).
        # Instead, set environment variables by using the shell’s internal
        # “export” command.
        #
        # adbd prior to Android 7.0 (N), and the adb client prior to SDK
        # platform-tools version 24, don’t know how to communicate a shell
        # command’s exit status. This was added in Android platform/system/core
        # 606835ae5c4b). With older adb servers and clients, adb will “exit 0”
        # indicating success even if the command failed on the device. This
        # makes subprocess.check_call() semantics difficult to implement
        # directly. As a workaround, have the device send the command’s exit
        # status over stdout and pick it back up in this function.
        #
        # Both workarounds are implemented by giving the device a simple script,
        # which adbd will run as an “sh -c” argument.
        adb_command = ['adb', '-s', android_device, 'shell']
        script_commands = []
        for k, v in env.items():
            script_commands.append('export %s=%s' %
                                   (pipes.quote(k), pipes.quote(v)))
        script_commands.extend([
            ' '.join(pipes.quote(x) for x in command_args), 'status=${?}',
            'echo "status=${status}"', 'exit ${status}'
        ])
        adb_command.append('; '.join(script_commands))
        child = subprocess.Popen(adb_command,
                                 shell=IS_WINDOWS_HOST,
                                 stdin=open(os.devnull),
                                 stdout=subprocess.PIPE)

        FINAL_LINE_RE = re.compile('status=(\d+)$')
        final_line = None
        while True:
            # Use readline so that the test output appears “live” when running.
            data = child.stdout.readline().decode('utf-8')
            if data == '':
                break
            if final_line is not None:
                # It wasn’t really the final line.
                print(final_line, end='')
                final_line = None
            if FINAL_LINE_RE.match(data.rstrip()):
                final_line = data
            else:
                print(data, end='')

        if final_line is None:
            # Maybe there was some stderr output after the end of stdout. Old
            # versions of adb, prior to when the exit status could be
            # communicated, smush the two together.
            raise subprocess.CalledProcessError(-1, adb_command)
        status = int(FINAL_LINE_RE.match(final_line.rstrip()).group(1))
        if status != 0:
            raise subprocess.CalledProcessError(status, adb_command)

        child.wait()
        if child.returncode != 0:
            raise subprocess.CalledProcessError(subprocess.returncode,
                                                adb_command)

    # /system/bin/mktemp isn’t normally present prior to Android 6.0 (M), where
    # toybox was introduced (Android platform/manifest 9a2c01e8450b). Fake it
    # with a host-generated name. This won’t retry if the name is in use, but
    # with 122 bits of randomness, it should be OK. This uses “mkdir” instead of
    # “mkdir -p”because the latter will not indicate failure if the directory
    # already exists.
    device_temp_dir = '/data/local/tmp/%s.%s' % (test, uuid.uuid4().hex)
    _adb_shell(['mkdir', device_temp_dir])

    try:
        # Specify test dependencies that must be pushed to the device. This
        # could be determined automatically in a GN build, following the example
        # used for Fuchsia. Since nothing like that exists for GYP, hard-code it
        # for supported tests.
        test_build_artifacts = [test, 'crashpad_handler']
        test_data = ['test/test_paths_test_data_root.txt']

        if test == 'crashpad_test_test':
            test_build_artifacts.append(
                'crashpad_test_test_multiprocess_exec_test_child')
        elif test == 'crashpad_util_test':
            test_data.append('util/net/testdata/')

        # Establish the directory structure on the device.
        device_out_dir = posixpath.join(device_temp_dir, 'out')
        device_mkdirs = [device_out_dir]
        for source_path in test_data:
            # A trailing slash could reasonably mean to copy an entire
            # directory, but will interfere with what’s needed from the path
            # split. All parent directories of any source_path need to be be
            # represented in device_mkdirs, but it’s important that no
            # source_path itself wind up in device_mkdirs, even if source_path
            # names a directory, because that would cause the “adb push” of the
            # directory below to behave incorrectly.
            if source_path.endswith(posixpath.sep):
                source_path = source_path[:-1]

            device_source_path = posixpath.join(device_temp_dir, source_path)
            device_mkdir = posixpath.split(device_source_path)[0]
            if device_mkdir not in device_mkdirs:
                device_mkdirs.append(device_mkdir)
        adb_mkdir_command = ['mkdir', '-p']
        adb_mkdir_command.extend(device_mkdirs)
        _adb_shell(adb_mkdir_command)

        # Push the test binary and any other build output to the device.
        local_test_build_artifacts = []
        for artifact in test_build_artifacts:
            local_test_build_artifacts.append(os.path.join(
                binary_dir, artifact))
        _adb_push(local_test_build_artifacts, device_out_dir)

        # Push test data to the device.
        for source_path in test_data:
            _adb_push([os.path.join(CRASHPAD_DIR, source_path)],
                      posixpath.join(device_temp_dir, source_path))

        # Run the test on the device. Pass the test data root in the
        # environment.
        #
        # Because the test will not run with its standard output attached to a
        # pseudo-terminal device, Google Test will not normally enable colored
        # output, so mimic Google Test’s own logic for deciding whether to
        # enable color by checking this script’s own standard output connection.
        # The list of TERM values comes from Google Test’s
        # googletest/src/gtest.cc testing::internal::ShouldUseColor().
        env = {'CRASHPAD_TEST_DATA_ROOT': device_temp_dir}
        gtest_color = os.environ.get('GTEST_COLOR')
        if gtest_color in ('auto', None):
            if (sys.stdout.isatty() and
                (os.environ.get('TERM')
                 in ('xterm', 'xterm-color', 'xterm-256color', 'screen',
                     'screen-256color', 'tmux', 'tmux-256color', 'rxvt-unicode',
                     'rxvt-unicode-256color', 'linux', 'cygwin') or
                 (IS_WINDOWS_HOST and _EnableVTProcessingOnWindowsConsole()))):
                gtest_color = 'yes'
            else:
                gtest_color = 'no'
        env['GTEST_COLOR'] = gtest_color
        _adb_shell([posixpath.join(device_out_dir, test)] + extra_command_line,
                   env)
    finally:
        _adb_shell(['rm', '-rf', device_temp_dir])


def _RunOnIOSTarget(binary_dir, test, is_xcuitest=False):
    """Runs the given iOS |test| app on iPhone 8 with the default OS version."""

    def xctest(binary_dir, test):
        """Returns a dict containing the xctestrun data needed to run an
        XCTest-based test app."""
        test_path = os.path.join(CRASHPAD_DIR, binary_dir)
        module_data = {
            'TestBundlePath': os.path.join(test_path, test + '_module.xctest'),
            'TestHostPath': os.path.join(test_path, test + '.app'),
            'TestingEnvironmentVariables': {
                'DYLD_FRAMEWORK_PATH': '__TESTROOT__/Debug-iphonesimulator:',
                'DYLD_INSERT_LIBRARIES':
                    ('__PLATFORMS__/iPhoneSimulator.platform/Developer/'
                     'usr/lib/libXCTestBundleInject.dylib'),
                'DYLD_LIBRARY_PATH': '__TESTROOT__/Debug-iphonesimulator',
                'IDEiPhoneInternalTestBundleName': test + '.app',
                'XCInjectBundleInto': '__TESTHOST__/' + test,
            }
        }
        return {test: module_data}

    def xcuitest(binary_dir, test):
        """Returns a dict containing the xctestrun data needed to run an
        XCUITest-based test app."""

        test_path = os.path.join(CRASHPAD_DIR, binary_dir)
        runner_path = os.path.join(test_path, test + '_module-Runner.app')
        bundle_path = os.path.join(runner_path, 'PlugIns',
                                   test + '_module.xctest')
        target_app_path = os.path.join(test_path, test + '.app')
        module_data = {
            'IsUITestBundle': True,
            'IsXCTRunnerHostedTestBundle': True,
            'TestBundlePath': bundle_path,
            'TestHostPath': runner_path,
            'UITargetAppPath': target_app_path,
            'DependentProductPaths': [
                bundle_path, runner_path, target_app_path
            ],
            'TestingEnvironmentVariables': {
                'DYLD_FRAMEWORK_PATH': '__TESTROOT__/Debug-iphonesimulator:',
                'DYLD_INSERT_LIBRARIES':
                    ('__PLATFORMS__/iPhoneSimulator.platform/Developer/'
                     'usr/lib/libXCTestBundleInject.dylib'),
                'DYLD_LIBRARY_PATH': '__TESTROOT__/Debug-iphonesimulator',
                'XCInjectBundleInto': '__TESTHOST__/' + test + '_module-Runner',
            },
        }
        return {test: module_data}

    with tempfile.NamedTemporaryFile() as f:
        import plistlib

        xctestrun_path = f.name
        print(xctestrun_path)
        if is_xcuitest:
            plistlib.writePlist(xcuitest(binary_dir, test), xctestrun_path)
        else:
            plistlib.writePlist(xctest(binary_dir, test), xctestrun_path)

        subprocess.check_call([
            'xcodebuild', 'test-without-building', '-xctestrun', xctestrun_path,
            '-destination', 'platform=iOS Simulator,name=iPhone 8'
        ])


# This script is primarily used from the waterfall so that the list of tests
# that are run is maintained in-tree, rather than in a separate infrastructure
# location in the recipe.
def main(args):
    parser = argparse.ArgumentParser(description='Run Crashpad unittests.')
    parser.add_argument('binary_dir', help='Root of build dir')
    parser.add_argument('test', nargs='*', help='Specific test(s) to run.')
    parser.add_argument(
        '--gtest_filter',
        help='Google Test filter applied to Google Test binary runs.')
    args = parser.parse_args()

    # Tell 64-bit Windows tests where to find 32-bit test executables, for
    # cross-bitted testing. This relies on the fact that the GYP build by
    # default uses {Debug,Release} for the 32-bit build and {Debug,Release}_x64
    # for the 64-bit build. This is not a universally valid assumption, and if
    # it’s not met, 64-bit tests that require 32-bit build output will disable
    # themselves dynamically.
    if (sys.platform == 'win32' and args.binary_dir.endswith('_x64') and
            'CRASHPAD_TEST_32_BIT_OUTPUT' not in os.environ):
        binary_dir_32 = args.binary_dir[:-4]
        if os.path.isdir(binary_dir_32):
            os.environ['CRASHPAD_TEST_32_BIT_OUTPUT'] = binary_dir_32

    target_os = _BinaryDirTargetOS(args.binary_dir)
    is_android = target_os == 'android'
    is_ios = target_os == 'ios'

    tests = [
        'crashpad_client_test',
        'crashpad_handler_test',
        'crashpad_minidump_test',
        'crashpad_snapshot_test',
        'crashpad_test_test',
        'crashpad_util_test',
    ]

    if is_android:
        android_device = os.environ.get('ANDROID_DEVICE')
        if not android_device:
            adb_devices = subprocess.check_output(['adb', 'devices'],
                                                  shell=IS_WINDOWS_HOST)
            devices = []
            for line in adb_devices.splitlines():
                line = line.decode('utf-8')
                if (line == 'List of devices attached' or
                        re.match('^\* daemon .+ \*$', line) or line == ''):
                    continue
                (device, ignore) = line.split('\t')
                devices.append(device)
            if len(devices) != 1:
                print("Please set ANDROID_DEVICE to your device's id",
                      file=sys.stderr)
                return 2
            android_device = devices[0]
            print('Using autodetected Android device:', android_device)
    elif is_ios:
        tests.append('ios_crash_xcuitests')
    elif IS_WINDOWS_HOST:
        tests.append('snapshot/win/end_to_end_test.py')

    if args.test:
        for t in args.test:
            if t not in tests:
                print('Unrecognized test:', t, file=sys.stderr)
                return 3
        tests = args.test

    for test in tests:
        print('-' * 80)
        print(test)
        print('-' * 80)
        if test.endswith('.py'):
            subprocess.check_call([
                sys.executable,
                os.path.join(CRASHPAD_DIR, test), args.binary_dir
            ])
        else:
            extra_command_line = []
            if args.gtest_filter:
                extra_command_line.append('--gtest_filter=' + args.gtest_filter)
            if is_android:
                _RunOnAndroidTarget(args.binary_dir, test, android_device,
                                    extra_command_line)
            elif is_ios:
                _RunOnIOSTarget(args.binary_dir,
                                test,
                                is_xcuitest=test.startswith('ios'))
            else:
                subprocess.check_call([os.path.join(args.binary_dir, test)] +
                                      extra_command_line)

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
