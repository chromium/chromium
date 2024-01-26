# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A tool for updating IDL COM headers/TLB after updating IDL template.

This tool must be run from a Windows machine at the source root directory.

Example:
    python3 tools/win/update_idl.py
"""

import os
import platform
import subprocess

class IDLUpdateError(Exception):
    """Module exception class."""


class IDLUpdater:
    """A class to update IDL COM headers/TLB files based on config."""

    def __init__(self, idl_gn_target: str, target_cpu: str,
                 is_chrome_branded: bool):
        self.idl_gn_target = idl_gn_target
        self.target_cpu = target_cpu
        self.is_chrome_branded = str(is_chrome_branded).lower()
        self.output_dir = r'out\idl_update'

    def update(self) -> None:
        print('Updating', self.idl_gn_target, 'IDL files for', self.target_cpu,
              'CPU, chrome_branded:', self.is_chrome_branded, '...')
        self._make_output_dir()
        self._gen_gn_args()
        self._autoninja_and_update()

    def _make_output_dir(self) -> None:
        if not os.path.exists(self.output_dir):
            os.makedirs(self.output_dir)

    def _gen_gn_args(self) -> None:
        # If the gn_args file already exists and has the desired values then
        # don't touch it - this avoids unnecessary and expensive gn gen
        # invocations.
        gn_args_path = os.path.join(self.output_dir, 'args.gn')
        contents = (f'target_cpu="{self.target_cpu}"\n'
                    f'is_chrome_branded={self.is_chrome_branded}\n'
                    f'is_debug=true\n'
                    f'enable_nacl=false\n'
                    f'blink_symbol_level=0\n'
                    f'v8_symbol_level=0\n').format()
        if os.path.exists(gn_args_path):
            with open(gn_args_path, 'rt', newline='') as f:
                new_contents = f.read()
                if new_contents == contents:
                    return

        # `subprocess` may interpret the complex config values passed via
        # `--args` differently than intended. Generate the default gn.args first
        # and then update it by writing directly.

        # gen args with default values.
        print('Generating', gn_args_path, 'with default values.')
        subprocess.run(['gn.bat', 'gen', self.output_dir], check=True)

        # Manually update args.gn
        print('Write', gn_args_path, 'with desired config.')
        with open(gn_args_path, 'wt', newline='') as f:
            f.write(contents)
        print('Done.')

    def _autoninja_and_update(self) -> None:
        print('Check if update is needed by building the target...')
        # Use -j 1 since otherwise the exact build output is not deterministic.
        proc = subprocess.run([
            'autoninja.bat', '-j', '1', '-C', self.output_dir,
            self.idl_gn_target
        ],
                              capture_output=True,
                              check=False,
                              universal_newlines=True)
        if proc.returncode == 0:
            print('No update is needed.\n')
            return

        cmd = self._extract_update_command(proc.stdout)
        print('Updating IDL COM headers/TLB by running: [', cmd, ']...')
        subprocess.run(cmd, shell=True, capture_output=True, check=True)
        print('Done.\n')

    def _extract_update_command(self, stdout: str) -> str:
        # Exclude blank lines.
        lines = list(filter(None, stdout.splitlines()))

        if (len(lines) < 3
                or 'ninja: build stopped: subcommand failed.' not in lines[-1]
                or 'copy /y' not in lines[-2]
                or 'To rebaseline:' not in lines[-3]):
            print('-' * 80)
            print('STDOUT:')
            print(stdout)
            print('-' * 80)

            raise IDLUpdateError(
                'Unexpected autoninja error, or update this tool if the output '
                'format is changed.')

        return lines[-2].strip().replace('..\\..\\', '')


def check_running_environment() -> None:
    if 'Windows' not in platform.system():
        raise IDLUpdateError('This tool must run from Windows platform.')

    proc = subprocess.run(['git.bat', 'rev-parse', '--show-toplevel'],
                          capture_output=True,
                          check=True)

    if proc.returncode != 0:
        raise IDLUpdateError(
            'Failed to run git for finding source root directory.')

    source_root = os.path.abspath(proc.stdout.decode('utf-8').strip()).lower()
    if not os.path.exists(source_root):
        raise IDLUpdateError('Unexpected failure to get source root directory')

    cwd = os.getcwd().lower()
    if cwd != source_root:
        raise IDLUpdateError(f'This tool must run from project root folder. '
                             f'CWD: [{cwd}] vs ACTUAL:[{source_root}]')

    # Build performance output interferes with error parsing. Silence it.
    os.environ['NINJA_SUMMARIZE_BUILD'] = '0'


def main():
    check_running_environment()

    for target_cpu in ['arm64', 'x64', 'x86']:
        for idl_target in [
                'updater_idl',
                'updater_idl_user',
                'updater_idl_system',
                'updater_internal_idl',
                'updater_internal_idl_user',
                'updater_internal_idl_system',
                'updater_legacy_idl',
                'updater_legacy_idl_user',
                'updater_legacy_idl_system',
                'google_update',
                'elevation_service_idl',
                'gaia_credential_provider_idl',
                'iaccessible2',
                'ichromeaccessible',
                'isimpledom',
                'remoting_lib_idl',
        ]:
            IDLUpdater(idl_target + '_idl_action', target_cpu, False).update()


if __name__ == '__main__':
    main()
