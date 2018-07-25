from __future__ import print_function

import os
import sys
import subprocess
import shutil


def get_dst_path():
    cwd = os.getcwd()
    return os.path.join(cwd, 'src', 'third_party',
                        'libadblockplus_android', 'src',
                        'third_party', 'android_sdk')


def duplicate_sdk():
    cwd = os.getcwd()
    sdk_src = os.path.join(cwd, 'src', 'third_party', 'android_tools', 'sdk')
    sdk_dst = get_dst_path()

    if os.path.exists(sdk_dst):
        print('Deleting {}'.format(sdk_dst))
        shutil.rmtree(sdk_dst)
    else:
        dst_parent = os.path.abspath(os.path.join(sdk_dst, os.pardir))
        print('Creating {}'.format(dst_parent))
        os.makedirs(dst_parent)

    print('Copying Android SDK from {} to {}'.format(sdk_src, sdk_dst))
    shutil.copytree(sdk_src, sdk_dst)


def install(title, package, version, verbose=False):
    print('Installing {} ...'.format(title))

    sdk_root = get_dst_path()
    sdkmanager = os.path.join(sdk_root, 'tools', 'bin', 'sdkmanager')

    args = [
        sdkmanager,
        '--sdk_root={}'.format(sdk_root),
        '{};{}'.format(package, version)
    ]

    if verbose:
        args += ['--verbose']

    process = subprocess.Popen(args, stdin=subprocess.PIPE,
                               stdout=sys.stdout, stderr=sys.stderr)

    # Agree to License
    process.stdin.write('y')
    process.communicate()
    process.stdin.close()

    if process.returncode != 0:
        print('{} finished with error code {}'.format(
            args, process.returncode))

    return process.returncode


def main(argv):
    duplicate_sdk()

    # TODO: update when different version of built-tools
    # and platform are required
    bt25 = install('Build tools', 'build-tools', '25.0.0', True)
    if bt25 != 0:
        return bt25

    a16 = install('Platform 16', 'platforms', 'android-16', True)
    if a16 != 0:
        return a16

    a21 = install('Platform 21', 'platforms', 'android-21', True)
    if a21 != 0:
        return a21

    return 0


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv[1:]))
    except KeyboardInterrupt:
        sys.exit('interrupted')
