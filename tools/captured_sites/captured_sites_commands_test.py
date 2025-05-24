#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Test the captured sites Commands."""

import os
from pathlib import Path
import unittest

import captured_sites_commands


class UnitTestCapturedSitesCommands(unittest.TestCase):

  def createAndSetEnvDirectory(self, env_name, dir_name):
    full_dir_path = Path(dir_name)
    if not full_dir_path.exists():
      os.makedirs(full_dir_path, exist_ok=True)
    self.local_environ[env_name] = dir_name

  def setUp(self):
    self.local_environ = os.environ.copy()
    self.createAndSetEnvDirectory('CAPTURED_SITES_USER_DATA_DIR',
                                  '/tmp/captured_sites/userdir')
    self.createAndSetEnvDirectory('CAPTURED_SITES_LOG_DATA_DIR',
                                  '/tmp/captured_sites/local_test_results')

  def buildReturnCommandText(self, name, args):
    command = captured_sites_commands.initiate_command(name, self.local_environ)
    command.build(args)
    return command.print()

  def helpCompareInputsToExpected(self, actual_input_and_output):
    for i, [name, args, expected_print] in enumerate(actual_input_and_output):
      identifier = ' '.join([name] + args)
      with self.subTest(command=identifier):
        actual_print = self.buildReturnCommandText(name, args)
        print(name, args)
        self.assertEqual(actual_print, expected_print)

  def testBuildCommand(self):
    actual_input_and_output = [
        [
            'build', [],
            'autoninja -C out/Default captured_sites_interactive_tests'
        ],
        [
            'build', ['-r'],
            'autoninja -C out/Release captured_sites_interactive_tests'
        ],
    ]
    self.helpCompareInputsToExpected(actual_input_and_output)

  def testChromeCommand(self):
    actual_input_and_output = [
        [
            'chrome', [],
            ('/usr/bin/google-chrome --ignore-certificate-errors-spki-list=2HcX'
             'CSKKJS0lEXLQEWhpHUfGuojiU0tiT5gOF9LP6IQ=,PoNnQAwghMiLUPg1YNFtvTfG'
             'reNT8r9oeLEyzgNCJWc= --user-data-dir="/tmp/captured_sites/userdir'
             '" --disable-application-cache --show-autofill-signatures --enable'
             '-features=AutofillShowTypePredictions --disable-features=Autofill'
             'CacheQueryResponses')
        ],
        [
            'chrome', ['-r'],
            ('out/Release/chrome --ignore-certificate-errors-spki-list=2HcXCSKK'
             'JS0lEXLQEWhpHUfGuojiU0tiT5gOF9LP6IQ=,PoNnQAwghMiLUPg1YNFtvTfGreNT'
             '8r9oeLEyzgNCJWc= --user-data-dir="/tmp/captured_sites/userdir" --'
             'disable-application-cache --show-autofill-signatures --enable-fea'
             'tures=AutofillShowTypePredictions --disable-features=AutofillCach'
             'eQueryResponses')
        ],
        [
            'chrome', ['-w'],
            ('/usr/bin/google-chrome --ignore-certificate-errors-spki-list=2HcX'
             'CSKKJS0lEXLQEWhpHUfGuojiU0tiT5gOF9LP6IQ=,PoNnQAwghMiLUPg1YNFtvTfG'
             'reNT8r9oeLEyzgNCJWc= --user-data-dir="/tmp/captured_sites/userdir'
             '" --disable-application-cache --show-autofill-signatures --enable'
             '-features=AutofillShowTypePredictions --disable-features=Autofill'
             'CacheQueryResponses --host-resolver-rules="MAP *:80 127.0.0.1:808'
             '0,MAP *:443 127.0.0.1:8081,EXCLUDE localhost"')
        ],
        [
            'chrome', ['-r', '-w'],
            ('out/Release/chrome --ignore-certificate-errors-spki-list=2HcXCSKK'
             'JS0lEXLQEWhpHUfGuojiU0tiT5gOF9LP6IQ=,PoNnQAwghMiLUPg1YNFtvTfGreNT'
             '8r9oeLEyzgNCJWc= --user-data-dir="/tmp/captured_sites/userdir" --'
             'disable-application-cache --show-autofill-signatures --enable-fea'
             'tures=AutofillShowTypePredictions --disable-features=AutofillCach'
             'eQueryResponses --host-resolver-rules="MAP *:80 127.0.0.1:8080,MA'
             'P *:443 127.0.0.1:8081,EXCLUDE localhost"')
        ],
    ]
    self.helpCompareInputsToExpected(actual_input_and_output)

  def testWprCommand(self):
    actual_input_and_output = [
        [
            'wpr', ['record', 'google'],
            ('third_party/catapult/telemetry/telemetry/bin/linux/x86_64/wpr rec'
             'ord --http_port=8080 --https_port=8081 --inject_scripts=third_par'
             'ty/catapult/web_page_replay_go/deterministic.js,chrome/test/data/'
             'web_page_replay_go_helper_scripts/automation_helper.js --https_ce'
             'rt_file=components/test/data/autofill/web_page_replay_support_fil'
             'es/ecdsa_cert.pem,components/test/data/autofill/web_page_replay_s'
             'upport_files/wpr_cert.pem --https_key_file=components/test/data/a'
             'utofill/web_page_replay_support_files/ecdsa_key.pem,components/te'
             'st/data/autofill/web_page_replay_support_files/wpr_key.pem chrome'
             '/test/data/autofill/captured_sites/artifacts/google.wpr')
        ],
        [
            'wpr', ['record', '-c', 'rsa', 'google'],
            ('third_party/catapult/telemetry/telemetry/bin/linux/x86_64/wpr rec'
             'ord --http_port=8080 --https_port=8081 --inject_scripts=third_par'
             'ty/catapult/web_page_replay_go/deterministic.js,chrome/test/data/'
             'web_page_replay_go_helper_scripts/automation_helper.js --https_ce'
             'rt_file=components/test/data/autofill/web_page_replay_support_fil'
             'es/wpr_cert.pem --https_key_file=components/test/data/autofill/we'
             'b_page_replay_support_files/wpr_key.pem chrome/test/data/autofill'
             '/captured_sites/artifacts/google.wpr')
        ],
        [
            'wpr', ['replay', 'google'],
            ('third_party/catapult/telemetry/telemetry/bin/linux/x86_64/wpr rep'
             'lay --http_port=8080 --https_port=8081 --inject_scripts=third_par'
             'ty/catapult/web_page_replay_go/deterministic.js,chrome/test/data/'
             'web_page_replay_go_helper_scripts/automation_helper.js --serve_re'
             'sponse_in_chronological_sequence --https_cert_file=components/tes'
             't/data/autofill/web_page_replay_support_files/ecdsa_cert.pem,comp'
             'onents/test/data/autofill/web_page_replay_support_files/wpr_cert.'
             'pem --https_key_file=components/test/data/autofill/web_page_repla'
             'y_support_files/ecdsa_key.pem,components/test/data/autofill/web_p'
             'age_replay_support_files/wpr_key.pem chrome/test/data/autofill/ca'
             'ptured_sites/artifacts/google.wpr')
        ],
        [
            'wpr', ['replay', 'sign_in_pass', 'google'],
            ('third_party/catapult/telemetry/telemetry/bin/linux/x86_64/wpr rep'
             'lay --http_port=8080 --https_port=8081 --inject_scripts=third_par'
             'ty/catapult/web_page_replay_go/deterministic.js,chrome/test/data/'
             'web_page_replay_go_helper_scripts/automation_helper.js --serve_re'
             'sponse_in_chronological_sequence --https_cert_file=components/tes'
             't/data/autofill/web_page_replay_support_files/ecdsa_cert.pem,comp'
             'onents/test/data/autofill/web_page_replay_support_files/wpr_cert.'
             'pem --https_key_file=components/test/data/autofill/web_page_repla'
             'y_support_files/ecdsa_key.pem,components/test/data/autofill/web_p'
             'age_replay_support_files/wpr_key.pem chrome/test/data/password/ca'
             'ptured_sites/artifacts/sign_in_pass/google.wpr')
        ],
        [
            'wpr', ['replay', '-c', 'rsa', 'google'],
            ('third_party/catapult/telemetry/telemetry/bin/linux/x86_64/wpr rep'
             'lay --http_port=8080 --https_port=8081 --inject_scripts=third_par'
             'ty/catapult/web_page_replay_go/deterministic.js,chrome/test/data/'
             'web_page_replay_go_helper_scripts/automation_helper.js --serve_re'
             'sponse_in_chronological_sequence --https_cert_file=components/tes'
             't/data/autofill/web_page_replay_support_files/wpr_cert.pem --http'
             's_key_file=components/test/data/autofill/web_page_replay_support_'
             'files/wpr_key.pem chrome/test/data/autofill/captured_sites/artifa'
             'cts/google.wpr')
        ],
    ]
    self.helpCompareInputsToExpected(actual_input_and_output)

  def testRefreshCommand(self):
    actual_input_and_output = [
        [
            'refresh', ['google'],
            ('out/Default/captured_sites_interactive_tests --gtest_filter="*/Au'
             'tofillCapturedSitesRefresh.Recipe/google" --enable-pixel-output-i'
             'n-tests --test-launcher-interactive --vmodule=captured_sites_test'
             '_utils=2,cache_replayer=1,autofill_captured_sites_interactive_uit'
             'est=1')
        ],
        [
            'refresh', ['-r', 'google'],
            ('out/Release/captured_sites_interactive_tests --gtest_filter="*/Au'
             'tofillCapturedSitesRefresh.Recipe/google" --enable-pixel-output-i'
             'n-tests --test-launcher-interactive --vmodule=captured_sites_test'
             '_utils=2,cache_replayer=1,autofill_captured_sites_interactive_uit'
             'est=1')
        ],
        [
            'refresh', ['-b', 'google'],
            ('testing/xvfb.py out/Default/captured_sites_interactive_tests --gt'
             'est_filter="*/AutofillCapturedSitesRefresh.Recipe/google" --enabl'
             'e-pixel-output-in-tests --test-launcher-interactive --vmodule=cap'
             'tured_sites_test_utils=2,cache_replayer=1,autofill_captured_sites'
             '_interactive_uitest=1')
        ],
        [
            'refresh', ['-r', '-s', 'google'],
            ('out/Release/captured_sites_interactive_tests --gtest_filter="*/Au'
             'tofillCapturedSitesRefresh.Recipe/google" --enable-pixel-output-i'
             'n-tests --test-launcher-interactive --vmodule=captured_sites_test'
             '_utils=2,cache_replayer=1,autofill_captured_sites_interactive_uit'
             'est=1 --test-launcher-summary-output=/tmp/captured_sites/local_te'
             'st_results/google_output.json 2>&1 | tee /tmp/captured_sites/loca'
             'l_test_results/google_capture.log')
        ],
        [
            'refresh',
            [
                '-r', '-s', '-b', '-d', '-f', '-v', '-t', '5', '-a', 'c', '-q',
                'pipe', '-w', 'google'
            ],
            ('testing/xvfb.py out/Release/captured_sites_interactive_tests --gt'
             'est_filter="*/AutofillCapturedSitesRefresh.Recipe/google" --enabl'
             'e-pixel-output-in-tests --test-launcher-interactive --vmodule=cap'
             'tured_sites_test_utils=2,autofill_download_manager=1,form_cache=1'
             ',autofill_agent=1,autofill_handler=1,form_structure=1,cache_repla'
             'yer=2,autofill_captured_sites_interactive_uitest=1 --gtest_also_r'
             'un_disabled_tests --gtest_break_on_failure --wpr_verbose --test-l'
             'auncher-retry-limit=5 --autofill-server-type=SavedCache  --comman'
             'd_file=pipe --test-launcher-summary-output=/tmp/captured_sites/lo'
             'cal_test_results/google_output.json 2>&1 | tee /tmp/captured_site'
             's/local_test_results/google_capture.log')
        ],
    ]
    self.helpCompareInputsToExpected(actual_input_and_output)

  def testRunCommand(self):
    actual_input_and_output = [
        [
            'run', ['google'],
            ('out/Default/captured_sites_interactive_tests --gtest_filter="*/Au'
             'tofillCapturedSitesInteractiveTest.Recipe/google" --enable-pixel-'
             'output-in-tests --test-launcher-interactive --vmodule=captured_si'
             'tes_test_utils=2,cache_replayer=1,autofill_captured_sites_interac'
             'tive_uitest=1')
        ],
        [
            'run', ['-r', 'google'],
            ('out/Release/captured_sites_interactive_tests --gtest_filter="*/Au'
             'tofillCapturedSitesInteractiveTest.Recipe/google" --enable-pixel-'
             'output-in-tests --test-launcher-interactive --vmodule=captured_si'
             'tes_test_utils=2,cache_replayer=1,autofill_captured_sites_interac'
             'tive_uitest=1')
        ],
        [
            'run', ['-b', 'google'],
            ('testing/xvfb.py out/Default/captured_sites_interactive_tests --gt'
             'est_filter="*/AutofillCapturedSitesInteractiveTest.Recipe/google"'
             ' --enable-pixel-output-in-tests --test-launcher-interactive --vmo'
             'dule=captured_sites_test_utils=2,cache_replayer=1,autofill_captur'
             'ed_sites_interactive_uitest=1')
        ],
        [
            'run', ['-r', '-s', 'google'],
            ('out/Release/captured_sites_interactive_tests --gtest_filter="*/Au'
             'tofillCapturedSitesInteractiveTest.Recipe/google" --enable-pixel-'
             'output-in-tests --test-launcher-interactive --vmodule=captured_si'
             'tes_test_utils=2,cache_replayer=1,autofill_captured_sites_interac'
             'tive_uitest=1 --test-launcher-summary-output=/tmp/captured_sites/'
             'local_test_results/google_output.json 2>&1 | tee /tmp/captured_si'
             'tes/local_test_results/google_capture.log')
        ],
        [
            'run', ['-r', '-s', '-u', 'google'],
            ('out/Release/captured_sites_interactive_tests --gtest_filter="*/Au'
             'tofillCapturedSitesInteractiveTest.Recipe/google" --enable-pixel-'
             'output-in-tests --ui-test-action-max-timeout=180000 --test-launch'
             'er-timeout=180000 --vmodule=captured_sites_test_utils=2,cache_rep'
             'layer=1,autofill_captured_sites_interactive_uitest=1 --test-launc'
             'her-summary-output=/tmp/captured_sites/local_test_results/google_'
             'output.json 2>&1 | tee /tmp/captured_sites/local_test_results/goo'
             'gle_capture.log')
        ],
        [
            'run',
            [
                '-r', '-s', '-b', '-d', '-f', '-v', '-t', '5', '-a', 'c', '-q',
                'pipe', '-w', 'google'
            ],
            ('testing/xvfb.py out/Release/captured_sites_interactive_tests --gt'
             'est_filter="*/AutofillCapturedSitesInteractiveTest.Recipe/google"'
             ' --enable-pixel-output-in-tests --test-launcher-interactive --vmo'
             'dule=captured_sites_test_utils=2,autofill_download_manager=1,form'
             '_cache=1,autofill_agent=1,autofill_handler=1,form_structure=1,cac'
             'he_replayer=2,autofill_captured_sites_interactive_uitest=1 --gtes'
             't_also_run_disabled_tests --gtest_break_on_failure --wpr_verbose '
             '--test-launcher-retry-limit=5 --autofill-server-type=SavedCache  '
             '--command_file=pipe --test-launcher-summary-output=/tmp/captured_'
             'sites/local_test_results/google_output.json 2>&1 | tee /tmp/captu'
             'red_sites/local_test_results/google_capture.log')
        ],
    ]
    self.helpCompareInputsToExpected(actual_input_and_output)


if __name__ == '__main__':
  unittest.main()
