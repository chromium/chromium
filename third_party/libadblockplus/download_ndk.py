from __future__ import print_function

import os
import urllib
import zipfile
import sys
import shutil


def main(argv):
    # Download
    ndk_src = 'https://dl.google.com/android/repository/android-ndk-r16b-linux-x86_64.zip'

    cwd = os.getcwd()
    libadblockplus_third_party = os.path.join(cwd,
                                              'src', 'third_party',
                                              'libadblockplus', 'src',
                                              'third_party')
    ndk_dst = os.path.join(libadblockplus_third_party,
                           'android-ndk-r16b-linux-x86_64.zip')

    if os.path.exists(ndk_dst):
        os.remove(ndk_dst)

    print('Downloading {} to {}'.format(ndk_src, ndk_dst))
    urllib.urlretrieve(ndk_src, ndk_dst)

    # Delete existing NDK directory
    ndk_dir = os.path.join(libadblockplus_third_party, 'android-ndk-r16b')
    if os.path.exists(ndk_dir):
        print('Deleting {}'.format(ndk_dir))
        shutil.rmtree(ndk_dir)

    # Extract zip (preserving file permissions)
    print('Extracting {} to {}'.format(ndk_dst, libadblockplus_third_party))
    with zipfile.ZipFile(ndk_dst, 'r') as zf:
        for info in zf.infolist():
            zf.extract(info.filename, path=libadblockplus_third_party)
            out_path = os.path.join(libadblockplus_third_party, info.filename)

            perm = info.external_attr >> 16L
            os.chmod(out_path, perm)

    # Delete zip
    os.remove(ndk_dst)


if __name__ == '__main__':
    try:
        main(sys.argv[1:])
    except KeyboardInterrupt:
        sys.exit('interrupted')
