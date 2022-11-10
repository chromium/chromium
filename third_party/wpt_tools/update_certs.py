#!/usr/bin/env vpython3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess
import sys

_THIS_DIR = os.path.dirname(__file__)
sys.path.append(os.path.join(_THIS_DIR, 'wpt', 'tools', 'wptserve', 'wptserve'))
from sslutils.openssl import OpenSSLEnvironment


_DOMAIN = '127.0.0.1'

def main():
    cert_dir = os.path.join(_THIS_DIR, 'certs')

    print('===> Removing old files...')
    old_files = filter(lambda filename: '.sxg.' not in filename,
                       os.listdir(cert_dir))
    old_files = [os.path.join(cert_dir, fn) for fn in old_files]
    if subprocess.call(['git', 'rm'] + old_files) != 0:
        sys.exit(1)

    print('\n===> Regenerating keys and certificates...')
    env = OpenSSLEnvironment(logging.getLogger(__name__),
                             base_path=cert_dir,
                             force_regenerate=True,
                             duration=3000)
    with env:
        key_path, pem_path = env.host_cert_path(
            [_DOMAIN,
             # See '_subdomains' in wpt/tools/serve/serve.py.
             'www.' + _DOMAIN,
             'www1.' + _DOMAIN,
             'www2.' + _DOMAIN,
             'xn--n8j6ds53lwwkrqhv28a.' + _DOMAIN,
             'xn--lve-6lad.' + _DOMAIN])
        if subprocess.call('git add -v ' + os.path.join(cert_dir, '*'), shell=True) != 0:
            sys.exit(1)

        print('\n===> Updating config.json and base.py...')
        key_basename = os.path.basename(key_path)
        pem_basename = os.path.basename(pem_path)
        config_path = os.path.join(_THIS_DIR, os.pardir, 'blink', 'web_tests',
                                   'external', 'wpt', 'config.json')
        if subprocess.call(['sed', '-i', '-E',
                            's%/[^/]+[.]key%/{key}%g;s%/[^/]+[.]pem%/{pem}%g'.format(
                                key=key_basename, pem=pem_basename),
                            config_path]) != 0:
            sys.exit(1)
        base_py_path = os.path.join(_THIS_DIR, os.pardir, 'blink', 'tools',
                                    'blinkpy', 'web_tests', 'port', 'base.py')
        proc = subprocess.Popen('openssl x509 -noout -pubkey -in ' + pem_path +
                                ' | openssl pkey -pubin -outform der'
                                ' | openssl dgst -sha256 -binary'
                                ' | base64', shell=True, stdout=subprocess.PIPE)
        base64, _ = proc.communicate()
        assert base64.isascii()
        if subprocess.call(['sed', '-i', '-E',
                            's%WPT_FINGERPRINT = \'.*\'%WPT_FINGERPRINT = \'' +
                            base64.decode().strip() + '\'%', base_py_path]) != 0:
            sys.exit(1)
        if subprocess.call(['git', 'add', '-v', config_path, base_py_path]) != 0:
            sys.exit(1)

        print('\n===> Certificate validity:')
        subprocess.call(['grep', 'Not After', pem_path])


if __name__ == "__main__":
    main()
