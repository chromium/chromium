#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import glob
import os
import shutil
import subprocess
import sys
import tempfile

BUILD_DIR = 'build'


def run_with_vsvars(cmd, tmpdir=None):
    fd, filename = tempfile.mkstemp('.bat', text=True)
    with os.fdopen(fd, 'w') as f:
        print('@echo off', file=f)
        print(r'call "%VS100COMNTOOLS%\vsvars32.bat"', file=f)
        if tmpdir:
            print(r'cd %s' % tmpdir, file=f)
        print(cmd, file=f)
    try:
        p = subprocess.Popen([filename],
                             shell=True,
                             stdout=subprocess.PIPE,
                             universal_newlines=True)
        out, _ = p.communicate()
        return p.returncode, out
    finally:
        os.unlink(filename)


def get_vc_dir():
    _, out = run_with_vsvars('echo VCINSTALLDIR=%VCINSTALLDIR%')
    for line in out.splitlines():  # pylint: disable-msg=E1103
        if line.startswith('VCINSTALLDIR='):
            return line[len('VCINSTALLDIR='):]
    return None


def build(infile):
    if not os.path.exists(BUILD_DIR):
        os.makedirs(BUILD_DIR)
    outfile = 'limiter.exe'
    outpath = os.path.join(BUILD_DIR, outfile)
    cpptime = os.path.getmtime(infile)
    if not os.path.exists(outpath) or cpptime > os.path.getmtime(outpath):
        print('Building %s...' % outfile)
        rc, out = run_with_vsvars(
            'cl /nologo /Ox /Zi /W4 /WX /D_UNICODE /DUNICODE'
            ' /D_CRT_SECURE_NO_WARNINGS /EHsc %s /link /out:%s' %
            (os.path.join('..', infile), outfile), BUILD_DIR)
        if rc:
            print(out)
            print('Failed to build %s' % outfile)
            sys.exit(1)
    else:
        print('%s already built' % outfile)
    return outpath


def main():
    # Switch to our own dir.
    os.chdir(os.path.dirname(os.path.abspath(__file__)))

    if sys.argv[-1] == 'clean':
        if os.path.exists(BUILD_DIR):
            shutil.rmtree(BUILD_DIR)
        for exe in glob.glob('*.exe'):
            os.unlink(exe)
        return 0

    vcdir = os.environ.get('VCINSTALLDIR')
    if not vcdir:
        vcdir = get_vc_dir()
        if not vcdir:
            print('Could not get VCINSTALLDIR. Run vsvars32.bat?')
            return 1
        os.environ['PATH'] += (';' + os.path.join(vcdir, 'bin') + ';' +
                               os.path.join(vcdir, r'..\Common7\IDE'))

    # Verify that we can find link.exe.
    link = os.path.join(vcdir, 'bin', 'link.exe')
    if not os.path.exists(link):
        print('link.exe not found at %s' % link)
        return 1

    exe_name = build('limiter.cc')
    for shim_exe in ('lib.exe', 'link.exe'):
        newpath = '%s__LIMITER.exe' % shim_exe
        shutil.copyfile(exe_name, newpath)
        print('%s shim built. Use with msbuild like: "/p:LinkToolExe=%s"' \
            % (shim_exe, os.path.abspath(newpath)))

    return 0


if __name__ == '__main__':
    sys.exit(main())
