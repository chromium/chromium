#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Updates the PipeWire source checkout and regenerates all committed generated
files (include/config.h, include/pipewire/version.h, conf/pipewire.conf,
conf/client.conf).

Requires meson and ninja to be installed (pip install meson ninja, or system
packages).

Usage:
  ./on_update.py <new-version>

Example:
  ./on_update.py 1.6.8

After running this script:
  1. Review the upstream meson.build diff for new source files and update
     BUILD.gn accordingly.
  2. Commit: DEPS  include/  conf/  README.chromium  (BUILD.gn if changed)
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.join(SCRIPT_DIR, 'src')
INCLUDE_DIR = os.path.join(SCRIPT_DIR, 'include')
CONF_DIR = os.path.join(SCRIPT_DIR, 'conf')

MESON_ARGS = [
    '--buildtype=plain',
    '-Dsession-managers=[]',
    '-Djack-devel=false',
]

MESON_DISABLED = [
    'docs', 'man', 'gstreamer', 'gstreamer-device-provider',
    'sdl2', 'audiotestsrc', 'videotestsrc', 'volume',
    'bluez5-codec-aptx', 'roc', 'bluez5-codec-lc3plus',
    'vulkan', 'libcamera', 'libcanberra',
    'pipewire-alsa', 'alsa', 'avb',
    'pipewire-v4l2', 'v4l2', 'pipewire-jack',
    'examples', 'libsystemd', 'systemd-system-service',
    'systemd-user-service', 'dbus', 'flatpak', 'selinux',
    'opus', 'sndfile', 'readline', 'installed_tests',
]

# Install-time paths replaced with "." — overridden at runtime by env vars.
PATH_DEFINES = [
    'PREFIX', 'PIPEWIRE_CONFDATADIR', 'LOCALEDIR', 'LIBDIR',
    'MODULEDIR', 'PIPEWIRE_CONFIG_DIR', 'PLUGINDIR', 'SPADATADIR',
    'PA_ALSA_DATA_DIR',
]

# Symbols meson detects on the build host but absent from the sysroot.
UNSET_DEFINES = ['HAVE_GETTID']

# Modules we don't build; mark ifexists+nofail so the daemon skips them.
OPTIONAL_MODULES_PIPEWIRE_CONF = [
    'libpipewire-module-profiler',
    'libpipewire-module-session-manager',
]
OPTIONAL_MODULES_CLIENT_CONF = [
    'libpipewire-module-session-manager',
]

# meson.build files that directly correspond to our BUILD.gn targets.
MESON_BUILD_FILES = [
    'src/pipewire/meson.build',
    'src/modules/meson.build',
    'src/daemon/meson.build',
    'spa/plugins/support/meson.build',
    'spa/plugins/videoconvert/meson.build',
]


def run(cmd, **kwargs):
    """Run a command, printing it first for visibility."""
    print('\n-------------------------------------------------')
    print('Running %s' % ' '.join(str(c) for c in cmd))
    subprocess.run(cmd, check=True, **kwargs)


def rewrite_file(path, search_replace, flags=0):
    """Apply a list of (search_regex, replacement) pairs to a file."""
    with open(path) as f:
        contents = f.read()
    for search, replace in search_replace:
        contents = re.sub(search, replace, contents, flags=flags)
    with open(path, 'w') as f:
        f.write(contents.strip() + '\n')


def fetch_and_checkout(version):
    """Fetch the release tag and check it out. Returns the commit hash.
    Tries refs/tags/<version> first (upstream GitLab), then falls back to
    refs/tags/upstream/<version> (chromium.googlesource.com mirror)."""
    for tag in [f'refs/tags/{version}', f'refs/tags/upstream/{version}']:
        result = subprocess.run(
            ['git', '-C', SRC_DIR, 'fetch', '--depth=1', 'origin', tag],
            capture_output=True)
        if result.returncode == 0:
            break
    else:
        sys.exit(f'ERROR: Could not find tag {version} or upstream/{version}')
    run(['git', '-C', SRC_DIR, 'checkout', 'FETCH_HEAD'])
    result = subprocess.run(
        ['git', '-C', SRC_DIR, 'rev-parse', 'HEAD'],
        capture_output=True, text=True, check=True)
    return result.stdout.strip()


def generate_config_h(tmp_dir):
    """Run meson to generate config.h, then rewrite paths to '.' and unset
    symbols that are absent from the build sysroot."""
    args = ['meson', 'setup'] + MESON_ARGS
    for flag in MESON_DISABLED:
        args.append(f'-D{flag}=disabled')
    args.append(tmp_dir)
    run(args, cwd=SRC_DIR)

    os.makedirs(INCLUDE_DIR, exist_ok=True)
    config_h = os.path.join(INCLUDE_DIR, 'config.h')
    shutil.copy(os.path.join(tmp_dir, 'config.h'), config_h)

    rewrites = []
    for define in PATH_DEFINES:
        rewrites.append((rf'#define {define} ".*"', f'#define {define} "."'))
    for define in UNSET_DEFINES:
        rewrites.append((rf'#define {define}\b', f'/* #undef {define} */'))
    rewrite_file(config_h, rewrites)
    print('Generated include/config.h')


def generate_version_h(version):
    """Generate include/pipewire/version.h from the upstream template."""
    parts = version.split('.')
    major, minor, micro = parts[0], parts[1], parts[2]
    nano = parts[3] if len(parts) > 3 else '0'

    template = os.path.join(SRC_DIR, 'src', 'pipewire', 'version.h.in')
    dst = os.path.join(INCLUDE_DIR, 'pipewire', 'version.h')
    os.makedirs(os.path.dirname(dst), exist_ok=True)

    with open(template) as f:
        content = f.read()

    for placeholder, value in [
        ('@PIPEWIRE_VERSION_MAJOR@', major),
        ('@PIPEWIRE_VERSION_MINOR@', minor),
        ('@PIPEWIRE_VERSION_MICRO@', micro),
        ('@PIPEWIRE_VERSION_NANO@', nano),
        ('@PIPEWIRE_API_VERSION@', '"0.3"'),
    ]:
        content = content.replace(placeholder, value)

    with open(dst, 'w') as f:
        f.write('/* Auto-generated by on_update.py — do not edit manually. */\n')
        f.write(content)
    print('Generated include/pipewire/version.h')


def _patch_optional_modules(path, modules):
    """Insert 'flags = [ ifexists nofail ]' before 'condition' lines for
    modules we don't build, so the daemon starts without them."""
    with open(path) as f:
        lines = f.readlines()

    result = []
    in_target = False
    for line in lines:
        if any(f'name = {m}' in line for m in modules):
            in_target = True
        if in_target and 'flags = [' in line:
            in_target = False  # already has flags
        if in_target and 'condition = [' in line:
            indent = len(line) - len(line.lstrip())
            result.append(' ' * indent + 'flags = [ ifexists nofail ]\n')
            in_target = False
        result.append(line)

    with open(path, 'w') as f:
        f.writelines(result)


def generate_conf_files(version):
    """Generate pipewire.conf and client.conf from upstream .in templates,
    then patch modules we don't build to be optional."""
    os.makedirs(CONF_DIR, exist_ok=True)

    replacements = {
        '@VERSION@': version,
        '@PIPEWIRE_CONFIG_DIR@': '.',
        '@rtprio_server@': '88',
        '@rtprio_client@': '83',
        '@sm_comment@': '#',
        '@pulse_comment@': '#',
        '@session_manager_path@': '/bin/false',
        '@session_manager_args@': '',
        '@pipewire_path@': '/bin/false',
        '@pipewire_pulse_path@': '/bin/false',
    }

    for conf in ('pipewire.conf', 'client.conf'):
        src = os.path.join(SRC_DIR, 'src', 'daemon', f'{conf}.in')
        dst = os.path.join(CONF_DIR, conf)
        with open(src) as f:
            content = f.read()
        for k, v in replacements.items():
            content = content.replace(k, v)
        with open(dst, 'w') as f:
            f.write('# Auto-generated by on_update.py — do not edit manually.\n')
            f.write(content)
        print(f'Generated conf/{conf}')

    _patch_optional_modules(os.path.join(CONF_DIR, 'pipewire.conf'),
                            OPTIONAL_MODULES_PIPEWIRE_CONF)
    _patch_optional_modules(os.path.join(CONF_DIR, 'client.conf'),
                            OPTIONAL_MODULES_CLIENT_CONF)


def _find_repo_root():
    """Walk up from SCRIPT_DIR to find the directory containing .gclient.
    gclient setdep must run from the directory with .gclient, which is
    typically one level above the src/ directory that contains DEPS."""
    d = SCRIPT_DIR
    while True:
        if os.path.isfile(os.path.join(d, '.gclient')):
            return d
        parent = os.path.dirname(d)
        if parent == d:
            return None
        d = parent


def update_deps(revision):
    """Update the DEPS entry via gclient setdep. Falls back to printing
    the manual command if gclient is unavailable or fails."""
    gclient = shutil.which('gclient')
    if not gclient:
        print(f'\nWARNING: gclient not found. Run manually from the repo root:')
        print(f'  gclient setdep -r src/third_party/pipewire/src@{revision}')
        return
    repo_root = _find_repo_root()
    if not repo_root:
        print(f'\nWARNING: Could not find DEPS file. Run manually from the repo root:')
        print(f'  gclient setdep -r src/third_party/pipewire/src@{revision}')
        return
    # Find the DEPS file relative to the .gclient directory.
    deps_file = None
    for candidate in ['DEPS', 'src/DEPS']:
        if os.path.isfile(os.path.join(repo_root, candidate)):
            deps_file = candidate
            break
    if not deps_file:
        print(f'\nWARNING: Could not find DEPS relative to {repo_root}.')
        print(f'  gclient setdep --deps-file=<path> -r src/third_party/pipewire/src@{revision}')
        return

    cmd = ['gclient', 'setdep', f'--deps-file={deps_file}',
           '-r', f'src/third_party/pipewire/src@{revision}']
    try:
        run(cmd, cwd=repo_root)
        print(f'Updated DEPS to {revision}')
    except subprocess.CalledProcessError:
        print(f'\nWARNING: gclient setdep failed. Run manually from {repo_root}:')
        print(f'  {" ".join(cmd)}')


def get_old_revision():
    """Read the current Revision from README.chromium before we update it."""
    path = os.path.join(SCRIPT_DIR, 'README.chromium')
    with open(path) as f:
        for line in f:
            m = re.match(r'^Revision: (\S+)', line)
            if m:
                return m.group(1)
    return None


def review_meson_changes(old_revision, new_revision):
    """Diff the meson.build files that correspond to our BUILD.gn targets
    between the old and new revision, so the user can spot added/removed
    source files."""
    if not old_revision:
        print('\nSkipping meson.build diff: no previous revision found.')
        return

    result = subprocess.run(
        ['git', 'diff', '--no-ext-diff', f'{old_revision}..{new_revision}',
         '--'] + MESON_BUILD_FILES,
        cwd=SRC_DIR, capture_output=True, text=True)

    if not result.stdout.strip():
        print('\nNo changes in relevant meson.build files.')
        return

    print('\n' + '=' * 72)
    print('meson.build diff (check for new source files to add to BUILD.gn):')
    print('=' * 72)
    print(result.stdout)


def update_readme(version, revision):
    """Update Version, Revision, and CPEPrefix in README.chromium."""
    path = os.path.join(SCRIPT_DIR, 'README.chromium')
    rewrite_file(path, [
        (r'^Version: .*', f'Version: {version}'),
        (r'^Revision: .*', f'Revision: {revision}'),
        (r'^CPEPrefix: .*', f'CPEPrefix: cpe:/a:pipewire:pipewire:{version}'),
    ], flags=re.MULTILINE)
    print('Updated README.chromium')


def show_diff():
    """Show a git diff --stat of the files this script modifies."""
    result = subprocess.run(
        ['git', 'diff', '--stat', 'include/', 'conf/', 'README.chromium'],
        cwd=SCRIPT_DIR, capture_output=True, text=True)
    if result.stdout.strip():
        print('\nChanged files:')
        print(result.stdout)


def main():
    parser = argparse.ArgumentParser(
        description='Update PipeWire source and regenerate committed files.')
    parser.add_argument('version', help='New PipeWire version (e.g. 1.6.8)')
    args = parser.parse_args()

    if not os.path.isdir(os.path.join(SRC_DIR, '.git')):
        sys.exit('ERROR: src/ is not a git checkout. Run gclient sync first.')

    for tool in ('meson', 'ninja', 'git'):
        if not shutil.which(tool):
            sys.exit(f'ERROR: {tool} not found. Install it first.')

    version = args.version
    old_revision = get_old_revision()
    revision = fetch_and_checkout(version)
    print(f'Checked out PipeWire {version} at {revision}')

    with tempfile.TemporaryDirectory() as tmp_dir:
        generate_config_h(tmp_dir)

    generate_version_h(version)
    generate_conf_files(version)
    update_deps(revision)
    update_readme(version, revision)
    review_meson_changes(old_revision, revision)
    show_diff()

    print('\nNext steps:')
    print('  1. Review upstream meson.build for new source files; update BUILD.gn.')
    print('  2. Commit: DEPS  include/  conf/  README.chromium  (BUILD.gn if changed)')


if __name__ == '__main__':
    main()
