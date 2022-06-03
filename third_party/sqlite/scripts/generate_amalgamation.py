#!/usr/bin/env python3
"""Creates the Chromium SQLite amalgamation.

The amalgamation is a single large source file (sqlite3.c) containing all
of the SQLite code. More at https://www.sqlite.org/amalgamation.html.

Usage:
    generate_amalgamation.py
"""

import argparse
import os
import stat
import subprocess
import sys
import tempfile
from shutil import copyfile, rmtree
from extract_sqlite_api import ProcessSourceFile, header_line, footer_line

# The Chromium SQLite third party directory (i.e. //third_party/sqlite).
_SQLITE_ROOT_DIR = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))

# The Chromium SQLite source directory (i.e. //third_party/sqlite/src).
_SQLITE_SRC_DIR = os.path.join(_SQLITE_ROOT_DIR, 'src')

# The .gni file (also used by BUILD.gn when building) which contains all
# flags passed to the `configuration` script and also used for the compile.
_COMMON_CONFIGURATION_FLAGS_GNI_FILE = os.path.join(
    _SQLITE_ROOT_DIR, 'sqlite_common_configuration_flags.gni')

_CHROMIUM_CONFIGURATION_FLAGS_GNI_FILE = os.path.join(
    _SQLITE_ROOT_DIR, 'sqlite_chromium_configuration_flags.gni')

_DEV_CONFIGURATION_FLAGS_GNI_FILE = os.path.join(
    _SQLITE_ROOT_DIR, 'sqlite_dev_configuration_flags.gni')

# The temporary directory where `make configure` and the amalgamation
# is temporarily created.
_TEMP_CONFIG_DIR = tempfile.mkdtemp()

# Set to True to generate a configuration which is compatible for
# running the SQLite tests.
_CONFIGURE_FOR_TESTING = False


def get_amalgamation_dir(config_name):
    if config_name == 'chromium':
        return os.path.join(_SQLITE_SRC_DIR, 'amalgamation')
    elif config_name == 'dev':
        return os.path.join(_SQLITE_SRC_DIR, 'amalgamation_dev')
    else:
        assert False


def _icu_cpp_flags():
    """Return the libicu C++ flags."""
    cmd = ['icu-config', '--cppflags']
    try:
        return subprocess.check_output(cmd)
    except Exception:
        return ''


def _icu_ld_flags():
    """Return the libicu linker flags."""
    cmd = ['icu-config', '--ldflags']
    try:
        return subprocess.check_output(cmd)
    except Exception:
        return ''


def _strip_flags_for_testing(flags):
    """Accepts the default configure/build flags and strips out those

    incompatible with the SQLite tests.

    When configuring SQLite to run tests this script uses a configuration
    as close to what Chromium ships as possible. Some flags need to be
    omitted for the tests to link and run correct. See comments below.
    """
    test_flags = []
    for flag in flags:
        # Omitting features can cause tests to hang/crash/fail because the
        # SQLite tests don't seem to detect feature omission. Keep them enabled.
        if flag.startswith('SQLITE_OMIT_'):
            continue

        # Some tests compile with specific SQLITE_DEFAULT_PAGE_SIZE so do
        # not hard-code.
        if flag.startswith('SQLITE_DEFAULT_PAGE_SIZE='):
            continue

        # Some tests compile with specific SQLITE_DEFAULT_MEMSTATUS so do
        # not hard-code.
        if flag.startswith('SQLITE_DEFAULT_MEMSTATUS='):
            continue

        # If enabled then get undefined reference to `uregex_open_63' and
        # other *_64 functions.
        if flag == 'SQLITE_ENABLE_ICU':
            continue

        # If defined then the fts4umlaut tests fail with the following error:
        #
        # Error: unknown tokenizer: unicode61
        if flag == 'SQLITE_DISABLE_FTS3_UNICODE':
            continue

        test_flags.append(flag)
    return test_flags


def _read_flags(file_name, param_name):
    config_globals = dict()
    with open(file_name) as input_file:
        code = compile(input_file.read(), file_name, 'exec')
        exec (code, config_globals)
    return config_globals[param_name]


def _read_configuration_values(config_name):
    """Read the configuration flags and return them in an array.


    |config_name| is one of "chromium" or "dev".
    """
    common_flags = _read_flags(_COMMON_CONFIGURATION_FLAGS_GNI_FILE,
                               'sqlite_common_configuration_flags')
    chromium_flags = _read_flags(_CHROMIUM_CONFIGURATION_FLAGS_GNI_FILE,
                                 'sqlite_chromium_configuration_flags')
    dev_flags = _read_flags(_DEV_CONFIGURATION_FLAGS_GNI_FILE,
                            'sqlite_dev_configuration_flags')

    if config_name == 'chromium':
        flags = common_flags + chromium_flags
    elif config_name == 'dev':
        flags = common_flags + dev_flags
    else:
        print('Incorrect config "%s"' % config_name, file=sys.stderr)
        sys.exit(1)

    if _CONFIGURE_FOR_TESTING:
        flags = _strip_flags_for_testing(flags)
    return flags


def _do_configure(config_name):
    """Run the configure script for the SQLite source."""
    configure = os.path.join(_SQLITE_SRC_DIR, 'configure')
    build_flags = ' '.join(
        ['-D' + f for f in _read_configuration_values(config_name)])
    cflags = '-Os {} {}'.format(build_flags, _icu_cpp_flags())
    ldflags = _icu_ld_flags()

    cmd = [
        configure,
        'CFLAGS={}'.format(cflags),
        'LDFLAGS={}'.format(ldflags),
        '--disable-load-extension',
        '--enable-amalgamation',
        '--enable-threadsafe',
    ]
    subprocess.check_call(cmd)

    if _CONFIGURE_FOR_TESTING:
        # Copy the files necessary for building/running tests back
        #into the source directory.
        files = ['Makefile', 'config.h', 'libtool']
        for file_name in files:
            copyfile(
                os.path.join(_TEMP_CONFIG_DIR, file_name),
                os.path.join(_SQLITE_SRC_DIR, file_name))
        file_name = os.path.join(_SQLITE_SRC_DIR, 'libtool')
        st = os.stat(file_name)
        os.chmod(file_name, st.st_mode | stat.S_IEXEC)


def make_aggregate(config_name):
    """Generate the aggregate source files."""
    if not os.path.exists(_TEMP_CONFIG_DIR):
        os.mkdir(_TEMP_CONFIG_DIR)
    try:
        os.chdir(_TEMP_CONFIG_DIR)
        _do_configure(config_name)

        cmd = ['make', 'shell.c', 'sqlite3.h', 'sqlite3.c']
        subprocess.check_call(cmd)

        amalgamation_dir = get_amalgamation_dir(config_name)
        if not os.path.exists(amalgamation_dir):
            os.mkdir(amalgamation_dir)

        readme_dst = os.path.join(amalgamation_dir, 'README.md')
        if not os.path.exists(readme_dst):
            readme_src = os.path.join(_SQLITE_ROOT_DIR, 'scripts',
                                      'README_amalgamation.md')
            copyfile(readme_src, readme_dst)

        copyfile(
            os.path.join(_TEMP_CONFIG_DIR, 'sqlite3.c'),
            os.path.join(amalgamation_dir, 'sqlite3.c'))
        copyfile(
            os.path.join(_TEMP_CONFIG_DIR, 'sqlite3.h'),
            os.path.join(amalgamation_dir, 'sqlite3.h'))

        # shell.c must be placed in a different directory from sqlite3.h,
        # because it contains an '#include "sqlite3.h"' that we want to resolve
        # to our custom //third_party/sqlite/sqlite3.h, not to the sqlite3.h
        # produced here.
        shell_dir = os.path.join(amalgamation_dir, 'shell')
        if not os.path.exists(shell_dir):
            os.mkdir(shell_dir)
        copyfile(
            os.path.join(_TEMP_CONFIG_DIR, 'shell.c'),
            os.path.join(shell_dir, 'shell.c'))
    finally:
        rmtree(_TEMP_CONFIG_DIR)


def extract_sqlite_api(config_name):
    amalgamation_dir = get_amalgamation_dir(config_name)
    input_file = os.path.join(amalgamation_dir, 'sqlite3.h')
    output_file = os.path.join(amalgamation_dir, 'rename_exports.h')
    ProcessSourceFile(
        api_export_macro='SQLITE_API',
        symbol_prefix='chrome_',
        header_line=header_line,
        footer_line=footer_line,
        input_file=input_file,
        output_file=output_file)


if __name__ == '__main__':
    desc = \
    ('Create the SQLite amalgamation. The SQLite amalgamation is documented at '
     'https://www.sqlite.org/amalgamation.html and is a single large file '
     'containing the SQLite source code. Chromium generates the amalgamation with'
     ' this script to ensure that the configuration parameters are identical to '
     'those in the Ninja build file.')

    parser = argparse.ArgumentParser(description=desc)
    parser.add_argument(
        '-t',
        '--testing',
        action='store_true',
        help='Generate an amalgamation for testing (default: false)')
    namespace = parser.parse_args()
    if namespace.testing:
        _CONFIGURE_FOR_TESTING = True
        print('Running configure for testing.')

    for config_name in ['chromium', 'dev']:
        make_aggregate(config_name)
        extract_sqlite_api(config_name)
