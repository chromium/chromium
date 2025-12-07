#!/usr/bin/env python3
#
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This file contains a couple dictionaries of useful GN args we need in various
# scripts. Providing a central location to get all gn args, or only the
# platforms you are interested in, and to configure them as required.

import os


class GnConfigsImpl:

    # TODO: Consider switching to enum.Flag rather than a bool.
    def __init__(self, use_remoteexec) -> None:
        self.remoteexec_args = [
            'use_reclient=false',
            'use_remoteexec=true',
            'use_siso=true',
        ]
        self.localexec_args = [
            'use_reclient=false',
            'use_remoteexec=false',
            'use_siso=true',
        ]
        current_exec = (self.remoteexec_args
                        if use_remoteexec else self.localexec_args)

        # TODO: Consider switching to straight defining a YAML file or json
        # struct to separate code from data more completely.
        self.linux_configs = {}
        # CQ bots:
        # https://ci.chromium.org/ui/p/chromium/builders/try/linux-rel
        # Note: For other CQ bots simply replace "linux-rel" with the name.
        self.linux_configs["linux-rel"] = [
            'dcheck_always_on=true',
            'devtools_skip_typecheck=false',
            'ffmpeg_branding="Chrome"',
            'is_component_build=false',
            'is_debug=false',
            'proprietary_codecs=true',
            'symbol_level=0',
            'target_cpu="x64"',
            'target_os="linux"',
            'use_clang_coverage=true',
        ] + current_exec
        self.linux_configs["linux-chromium-asan"] = [
            'dcheck_always_on=true',
            'fail_on_san_warnings=true',
            'is_asan=true',
            'is_component_build=false',
            'is_debug=false',
            'is_lsan=true',
            'symbol_level=1',
            'target_cpu="x64"',
            'target_os="linux"',
        ] + current_exec
        self.linux_configs["linux-libfuzzer-asan-rel"] = [
            'dcheck_always_on=true',
            'enable_mojom_fuzzer=true',
            'ffmpeg_branding="ChromeOS"',
            'generate_fuzzer_owners=false',
            'is_asan=true',
            'is_component_build=true',
            'is_debug=false',
            'optimize_for_fuzzing=true',
            'pdf_enable_xfa=true',
            'proprietary_codecs=true',
            'symbol_level=0',
            'target_cpu="x64"',
            'target_os="linux"',
            'use_libfuzzer=true',
        ] + current_exec
        self.linux_configs["linux-chromium-tsan-rel-ng"] = [
            'dcheck_always_on=true',
            'fail_on_san_warnings=true',
            'is_component_build=false',
            'is_debug=false',
            'is_tsan=true',
            'symbol_level=1',
            'target_cpu="x64"',
            'target_os="linux"',
        ] + current_exec
        # Finally Official linux builder args
        # See https://ci.chromium.org/ui/p/chrome/builders/official/linux64
        self.linux_configs["linux-official"] = [
            'dcheck_always_on=true',
            'is_chrome_branded=true',
            'is_debug=false',
            'is_official_build=true',
            'rtc_use_pipewire=true',
            # Added to make it explicit.
            'target_os="linux"',
        ] + current_exec

        self.mac_configs = {}
        # CQ bots
        # https://ci.chromium.org/ui/p/chromium/builders/try/mac-rel
        # Note: For other CQ bots simply replace "mac-rel" with the name.
        # Note: you can get to the args from there but it is a bit difficult you
        # can also look at:
        # infra/config/generated/builders/try/mac-rel/gn-args.json
        # To save compile time we've merged mac-rel with mac-official
        self.mac_configs["mac-rel"] = [
            # mac-rel
            # "coverage_instrumentation_input_file" removed didn't exist locally
            'dcheck_always_on=true',
            'enable_backup_ref_ptr_feature_flag=true',
            'enable_dangling_raw_ptr_checks=true',
            'enable_dangling_raw_ptr_feature_flag=true',
            'ffmpeg_branding="Chrome"',
            'is_component_build=false',
            'is_debug=false',
            'proprietary_codecs=true',
            'symbol_level=0',
            'target_cpu="x64"',
            'target_os="mac"',
            'use_clang_coverage=true',
            # Below are args needed by mac-official
            # 'is_chrome_branded=true', Doesn't build locally.
            # 'is_official_build=true', Only needed for optimizations
            'save_reproducers_on_lld_crash=true',
            # Set to false, although it is True on official bots this requires
            # extra set up so remove here.
            'ignore_missing_widevine_signing_cert=true',
            # See crbug.com/40249290 needed for cross compiling
            'enable_dsyms=false',
            # See (internal only): yaqs/4233495799914299392 also needed
            'enable_stripping=false',
        ] + current_exec
        self.win_configs = {}
        # CQ bot
        # See infra/config/generated/builders/try/win-rel/gn-args.json
        self.win_configs['win-rel'] = [
            # Coverage file is removed.
            'dcheck_always_on=true',
            'enable_dangling_raw_ptr_checks=true',
            'enable_dangling_raw_ptr_feature_flag=true',
            'enable_resource_allowlist_generation=false',
            'ffmpeg_branding="Chrome"',
            'is_component_build=false',
            'is_debug=false',
            'proprietary_codecs=true',
            'symbol_level=0',
            'target_cpu="x64"',
            'target_os="win"',
            'use_clang_coverage=true',
        ] + current_exec
        self.win_configs['win_chromium_compile_dbg_ng'] = [
            'ffmpeg_branding="Chrome"',
            'is_component_build=true',
            'is_debug=true',
            'proprietary_codecs=true',
            'symbol_level=0',
            'target_cpu="x86"',
            'target_os="win"',
        ] + current_exec

        self.android_configs = {}
        # CQ bots
        # https://ci.chromium.org/ui/p/chromium/builders/try/android-arm64-rel
        # Note: For other CQ bots simply replace "android-arm64-rel" with the
        # name.
        # Note: again you can also use
        # infra/config/generated/builders/try/android-arm64-rel/gn-args.json
        self.android_configs["android-arm64-rel"] = [
            #'android_static_analysis="off"', gn warns this has no effect.
            # Coverage file is removed
            'dcheck_always_on=true',
            'debuggable_apks=false',
            'enable_android_secondary_abi=true',
            'fail_on_android_expectations=true',
            'ffmpeg_branding="Chrome"',
            'is_component_build=false',
            'is_debug=false',
            'proprietary_codecs=true',
            'strip_debug_info=true',
            'symbol_level=0',
            'system_webview_package_name="com.google.android.chrome"',
            'target_cpu="arm64"',
            'use_clang_coverage=true',
        ] + current_exec
        self.android_configs['android-binary-size'] = [
            'android_channel="stable"',
            'debuggable_apks=false',
            'ffmpeg_branding="Chrome"',
            'is_high_end_android_secondary_toolchain=false',
            'proprietary_codecs=true',
            'symbol_level=1',
            'target_cpu="arm64"',
            'target_os="android"',
            'v8_is_on_release_branch=true',
        ] + current_exec
        self.android_configs["android_compile_dbg"] = [
            'android_static_analysis="on"',
            'debuggable_apks=false',
            'enable_android_secondary_abi=true',
            'ffmpeg_branding="Chrome"',
            'is_component_build=true',
            'is_debug=true',
            'proprietary_codecs=true',
            'symbol_level=0',
            'target_cpu="arm64"',
            'target_os="android"',
        ] + current_exec
        self.android_configs["android-x86-rel"] = [
            'android_static_analysis="off"',
            # coverage file is removed
            'dcheck_always_on=true',
            'debuggable_apks=false',
            'ffmpeg_branding="Chrome"',
            'is_component_build=false',
            'is_debug=false',
            'proprietary_codecs=true',
            'strip_debug_info=true',
            'symbol_level=0',
            'system_webview_package_name="com.google.android.apps.chrome"',
            'system_webview_shell_package_name="org.chromium.my_webview_shell"',
            'target_cpu="x86"',
            'target_os="android"',
            'use_clang_coverage=true',
            'use_jacoco_coverage=true',
        ] + current_exec
        # Finally official build (arm64 in this case)
        # See https://ci.chromium.org/ui/p/chrome/g/official.android/builders
        self.android_configs["android-arm64-official"] = [
            'dcheck_always_on=false',
            'enable_remoting=true',
            'ffmpeg_branding="Chrome"',
            'is_chrome_branded=true',
            'is_component_build=false',
            'is_debug=false',
            'is_high_end_android=false',
            'is_official_build=true',
            'proprietary_codecs=true',
            # locally 2 generates to many debug symbols.
            'symbol_level=0',
            # We don't have the official signing keys which causes unresolved
            # dependencies.
            #'system_webview_apk_target=
            #    "//clank/android_webview:system_webview_google_apk"',
            'target_cpu="arm64"',
            'target_os="android"',
            'thin_lto_enable_cache=false',
            'use_signing_keys=true',
        ] + current_exec

        self.fuchsia_configs = {}
        # Finally official build (arm64 in this case)
        # See https://ci.chromium.org/ui/p/chrome/g/official.desktop/builders
        self.fuchsia_configs['fuchsia-arm64-official'] = [
            'dcheck_always_on=true',
            'is_chrome_branded=false',
            'is_debug=false',
            'target_cpu="arm64"',
            'target_os="fuchsia"',
            'test_host_cpu="arm64"',
            'use_official_google_api_keys=false',
            # Needed to prevent debug info from being to large.
            'symbol_level=1',
        ] + current_exec

        self.chromeos_configs = {}
        # Finally official build
        # See https://ci.chromium.org/ui/p/chrome/g/official.desktop/builders
        # Note that a lot of the args are removed but they are operating system
        # related args.
        self.chromeos_configs['chromeos-official'] = [
            'target_os="chromeos"',
            'dcheck_always_on=true',
            'is_chrome_branded=true',
            #'is_chromeos_device=true', Not building on a chromeos device.
            'is_debug=false',
            'is_official_build=true',
            'use_cfi_cast=true',
            'enable_pseudolocales=true',
            'enable_remoting=true',
            # Encountered to many open fds without while compiling.
            'symbol_level=1',
        ] + current_exec

        # A set of platforms that gives you minimum coverage.
        self.min_all_platforms = {}
        self.min_all_platforms['linux'] = self.linux_configs['linux-rel']
        self.min_all_platforms['android'] = self.android_configs[
            'android-arm64-rel']
        self.min_all_platforms['mac'] = self.mac_configs['mac-rel']
        self.min_all_platforms['win'] = self.win_configs['win-rel']
        self.min_all_platforms['fuchsia'] = self.fuchsia_configs[
            'fuchsia-arm64-official']
        self.min_all_platforms['chromeos'] = self.chromeos_configs[
            'chromeos-official']

        # A set of platforms that should give you reasonable CQ coverage.
        self.all_platforms_and_configs = (self.linux_configs
                                          | self.android_configs
                                          | self.mac_configs | self.win_configs
                                          | self.fuchsia_configs
                                          | self.chromeos_configs)

    # Make it simple to get a certain config or all platform configs if you only
    # care about a single one.
    def __getitem__(self, key):
        potential_platform_key = '%s_configs' % key
        if hasattr(self, potential_platform_key):
            return getattr(self, potential_platform_key)
        for name, config in self.all_platforms_and_configs.items():
            print(name)
            if key == name:
                return config
        return None


# To prevent initializing it repeatedly if people call it as a simple accessor.
# TODO: Consider just switching to a global object already initialized.
_remote_exec_config = None
_local_exec_config = None


# This function returns the config and ensures it only gets initialized once per
# python execution.
#
# It declares the following member variable dicts:
#  * linux_configs (accessible through ['linux']
#  * android_configs (accessible through ['android']
#  * mac_configs (again accessible)
#  * win_configs (again accessible)
#  * fuchsia_configs (again)
#  * chromeos_configs (again).
#  * all_platforms_and_configs -> This is all of the configs defined above.
#  * min_all_platforms -> This is a single representative config from each
#    platform listed above.
#
# In addition if know the name of the config you want you can access it directly
# through GnConfigs()['linux-rel'] for example. This returns an array of args
# used by the 'linux-rel' CQ bot.
#
# In general all dicts are a mapping of CQ (or official builder) to an array of
# GN args. You can provide this key, args list to GenerateGnTarget(key, args)
# Which will create the directory and ensure the args are properly updated.
def GnConfigs(use_remoteexec) -> GnConfigsImpl:
    # Needed to not create a local variable in the assignment.
    global _remote_exec_config, _local_exec_config
    if use_remoteexec:
        if _remote_exec_config is None:
            _remote_exec_config = GnConfigsImpl(True)
        return _remote_exec_config

    if _local_exec_config is None:
        _local_exec_config = GnConfigsImpl(False)
    return _local_exec_config


# This function will generate out/<target> to use <args> this is the same as
# running `gn args` yourself and manually entering <args>.
def GenerateGnTarget(target, args):
    # Configure the target.
    ret = os.system("gn gen out/%s --args='%s'" % (target, "\n".join(args)))
    if os.WIFEXITED(ret) and os.WEXITSTATUS(ret) == 0:
        return True
    return False
