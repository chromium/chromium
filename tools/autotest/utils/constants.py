# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import re
import sys

from enum import Enum
from pathlib import Path

# Don't write pyc files to the src tree, which show up in version control
# in some environments.
sys.dont_write_bytecode = True

# Output more verbose logging to aid in debugging autotest.
DEBUG: bool = os.environ.get('AUTOTEST_DEBUG') == '1'

SRC_DIR: Path = Path(__file__).parent.parent.parent.parent.resolve()

DEPOT_TOOLS_DIR: Path = SRC_DIR / 'third_party' / 'depot_tools'

## CONSTANTS ##

# Some test suites use suffixes that would also match non-test-suite targets.
# Those test suites should be manually added here.
TEST_TARGET_ALLOWLIST: list[str] = [

    # The tests below this line were output from the ripgrep command just below:
    '//ash:ash_pixeltests',
    '//build/rust/tests/test_serde_json_lenient:test_serde_json_lenient',
    '//chrome/browser/apps/app_service/app_install:app_install_fuzztests',
    '//chrome/browser/glic/e2e_test:glic_internal_e2e_interactive_ui_tests',
    '//chrome/browser/mac:install_sh_test',
    '//chrome/browser/metrics/perf:profile_provider_unittest',
    '//chrome/browser/privacy_sandbox/notice:fuzz_tests',
    '//chrome/browser/web_applications:web_application_fuzztests',
    '//chromecast/media/base:video_plane_controller_test',
    '//chromecast/metrics:cast_metrics_unittest',
    '//chrome/enterprise_companion:enterprise_companion_integration_tests',
    '//chrome/enterprise_companion:enterprise_companion_tests',
    '//chrome/installer/gcapi:gcapi_test',
    '//chrome/installer/test:upgrade_test',
    '//chromeos/ash/components/kiosk/vision:kiosk_vision_unit_tests',
    '//chrome/test/android:chrome_public_apk_baseline_profile_generator',
    '//chrome/test:browser_tests',
    '//chrome/test:interactive_ui_tests',
    '//chrome/test:unit_tests',
    '//clank/javatests:chrome_apk_baseline_profile_generator',
    '//clank/javatests:chrome_smoke_test',
    '//clank/javatests:trichrome_chrome_google_bundle_smoke_test',
    '//components/chromeos_camera:jpeg_decode_accelerator_unittest',
    '//components/exo/wayland:wayland_client_compatibility_tests',
    '//components/exo/wayland:wayland_client_tests',
    '//components/facilitated_payments/core/validation:pix_code_validator_fuzzer',
    '//components/minidump_uploader:minidump_uploader_test',
    '//components/paint_preview/browser:paint_preview_browser_unit_tests',
    '//components/paint_preview/common:paint_preview_common_unit_tests',
    '//components/paint_preview/renderer:paint_preview_renderer_unit_tests',
    '//components/services/paint_preview_compositor:paint_preview_compositor_unit_tests',
    '//components/translate/core/language_detection:language_detection_util_fuzztest',
    '//components/webcrypto:webcrypto_testing_fuzzer',
    '//components/zucchini:zucchini_integration_test',
    '//content/test/fuzzer:devtools_protocol_encoding_json_fuzzer',
    '//fuchsia_web/runners:cast_runner_integration_tests',
    '//fuchsia_web/webengine:web_engine_integration_tests',
    '//google_apis/gcm:gcm_unit_tests',
    '//gpu:gl_tests',
    '//gpu:gpu_benchmark',
    '//gpu/vulkan/android:vk_tests',
    '//ios/web:ios_web_inttests',
    '//ios/web_view/test:ios_web_view_inttests',
    '//media/cdm:aes_decryptor_fuzztests',
    '//media/formats:ac3_util_fuzzer',
    '//media/gpu/chromeos:image_processor_test',
    '//media/gpu/v4l2:v4l2_unittest',
    '//media/gpu/vaapi/test/fake_libva_driver:fake_libva_driver_unittest',
    '//media/gpu/vaapi:vaapi_unittest',
    '//native_client/tests:large_tests',
    '//native_client/tests:medium_tests',
    '//native_client/tests:small_tests',
    '//sandbox/mac:sandbox_mac_fuzztests',
    '//sandbox/win:sbox_integration_tests',
    '//sandbox/win:sbox_validation_tests',
    '//testing/libfuzzer/fuzzers:libyuv_scale_fuzztest',
    '//testing/libfuzzer/fuzzers:paint_vector_icon_fuzztest',
    '//third_party/blink/renderer/controller:blink_perf_tests',
    '//third_party/blink/renderer/core:css_parser_fuzzer',
    '//third_party/blink/renderer/core:inspector_ghost_rules_fuzzer',
    '//third_party/blink/renderer/platform/loader:unencoded_digest_fuzzer',
    '//third_party/crc32c:crc32c_benchmark',
    '//third_party/crc32c:crc32c_tests',
    '//third_party/dawn/src/dawn/tests/benchmarks:dawn_benchmarks',
    '//third_party/federated_compute:federated_compute_tests',
    '//third_party/highway:highway_tests',
    '//third_party/ipcz/src:ipcz_tests',
    '//third_party/libaom:av1_encoder_fuzz_test',
    '//third_party/libaom:test_libaom',
    '//third_party/libvpx:test_libvpx',
    '//third_party/libvpx:vp8_encoder_fuzz_test',
    '//third_party/libvpx:vp9_encoder_fuzz_test',
    '//third_party/libwebp:libwebp_advanced_api_fuzzer',
    '//third_party/libwebp:libwebp_animation_api_fuzzer',
    '//third_party/libwebp:libwebp_animencoder_fuzzer',
    '//third_party/libwebp:libwebp_enc_dec_api_fuzzer',
    '//third_party/libwebp:libwebp_huffman_fuzzer',
    '//third_party/libwebp:libwebp_mux_demux_api_fuzzer',
    '//third_party/libwebp:libwebp_simple_api_fuzzer',
    '//third_party/opus:test_opus_api',
    '//third_party/opus:test_opus_decode',
    '//third_party/opus:test_opus_encode',
    '//third_party/opus:test_opus_padding',
    '//third_party/pdfium:pdfium_embeddertests',
    '//third_party/pffft:pffft_unittest',
    '//third_party/rapidhash:rapidhash_fuzztests',
    '//ui/ozone:ozone_integration_tests',
]
r"""
 You can run this command to find test targets that do not match _TEST_TARGET_SUFFIXES,
 and use it to update _TEST_TARGET_ALLOWLIST.
rg '^(instrumentation_test_runner|test)\("([^"]*)' -o -g'BUILD.gn' -r'$2' -N \
  | rg -v '(_browsertests|_perftests|_wpr_tests|_unittests)$' \
  | rg '^(.*)/BUILD.gn(.*)$' -r'\'//$1$2\',' \
  | sort

 And you can use a command like this to find source_set targets that do match
 _TEST_TARGET_SUFFIXES (ideally this is minimal).
rg '^source_set\("([^"]*)' -o -g'BUILD.gn' -r'$1' -N | \
  rg '(_browsertests|_perftests|_wpr_tests|_unittests)$'
"""
TEST_TARGET_SUFFIXES: list[str] = ('_browsertests', '_perftests', '_wpr_tests',
                                   '_unittests')

PREF_MAPPING_FILE_PATTERN: str = re.escape(
    str(Path('components') / 'policy' / 'test' / 'data' / 'pref_mapping') +
    r'/') + r'.*\.json'

# `rg` always uses forward slashes for globs, even on Windows.
PREF_MAPPING_FILE_NAME_GLOB: str = '*components/policy/test/data/pref_mapping/*.json'
GTEST_FILE_NAME_GLOB: str = '*{test,tests}*.{cc,mm,java}'

# Regex version of `(PREF_MAPPING_FILE_GLOB) | (GTEST_FILE_GLOB)`
TEST_FILE_NAME_REGEX: re.Pattern[str] = re.compile(
    r'(.*tests?.*\.(cc|mm|java)$)' + r'|(' + PREF_MAPPING_FILE_PATTERN + r')',
    flags=re.IGNORECASE)

_PREF_MAPPING_GTEST_FILTER: str = '*PolicyPrefsTest.PolicyToPrefsMapping*'

PREF_MAPPING_FILE_REGEX: re.Pattern[str] = re.compile(PREF_MAPPING_FILE_PATTERN)

SPECIAL_TEST_FILTERS: list[tuple[re.Pattern[str], str]] = [
    (PREF_MAPPING_FILE_REGEX, _PREF_MAPPING_GTEST_FILTER)
]

# If these test definition macros appear as the first thing on a line of a C++
# file, we are certain that the file contains GTests.
GTEST_TEST_DEFINITION_MACRO_REGEX = re.compile(
    r'^(TEST|TEST_F|TEST_P|INSTANTIATE_TEST_SUITE_P|TYPED_TEST|TYPED_TEST_P|'
    r'INSTANTIATE_TYPED_TEST_SUITE_P)\(',
    flags=re.MULTILINE)

JUNIT_TEST_ANNOTATION_REGEX = re.compile(r'^\s*@Test', flags=re.MULTILINE)

## ENUMS ##


class TestValidity(Enum):
  NOT_A_TEST = 0  # Does not match test file regex.
  MAYBE_A_TEST = 1  # Matches test file regex, but doesn't include gtest files.
  VALID_TEST = 2  # Matches test file regex and includes gtest files.
