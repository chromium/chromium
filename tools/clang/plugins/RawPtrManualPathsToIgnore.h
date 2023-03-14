// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_RAWPTRMANUALPATHSTOIGNORE_H_
#define TOOLS_CLANG_PLUGINS_RAWPTRMANUALPATHSTOIGNORE_H_

// Array listing regular expressions of paths that should be ignored when
// running the rewrite_raw_ptr_fields tool on Chromium sources.
//
// If a source file path contains any of the lines in the filter file below,
// then such source file will not be rewritten.
//
// Lines prefixed with "!" can be used to force include files that matched a
// file path to be ignored.
//
// Note that the rewriter has a hardcoded logic for a handful of path-based
// exclusions that cannot be expressed as substring matches:
// - Excluding paths containing "third_party/", but still covering
//   "third_party/blink/"
//   (see the isInThirdPartyLocation AST matcher in RewriteRawPtrFields.cpp).
// - Excluding paths _starting_ with "gen/" or containing "/gen/"
//   (i.e. hopefully just the paths under out/.../gen/... directory)
//   via the isInGeneratedLocation AST matcher in RewriteRawPtrFields.cpp.
constexpr const char* const kRawPtrManualPathsToIgnore[] = {
    // Exclude to prevent PartitionAlloc<->raw_ptr<T> cyclical dependency.
    "base/allocator/",

    // Exclude dependences of raw_ptr.h
    // TODO(bartekn): Update the list of dependencies.
    "base/logging.h",
    "base/synchronization/lock_impl.h",
    "base/check.h",

    // Exclude - deprecated and contains legacy C++ and pre-C++11 code.
    "ppapi/",

    // Exclude tools that do not ship in the Chrome binary. Can't depend on
    // //base.
    "base/android/linker/",
    "chrome/chrome_cleaner/",
    "tools/",
    "net/tools/",
    "chrome/chrome_elf/",
    "chrome/installer/mini_installer/",

    // DEPS prohibits includes from base/
    "chrome/install_static",
    "net/cert/pki",

    // Exclude pocdll.dll as it doesn't depend on //base and only used for
    // testing.
    "sandbox/win/sandbox_poc/pocdll",

    // Exclude internal definitions of undocumented Windows structures.
    "sandbox/win/src/nt_internals.h",

    // Exclude directories that don't depend on //base, because nothing there
    // uses
    // anything from /base.
    "sandbox/linux/system_headers/",
    "components/history_clusters/core/",
    "ui/qt/",

    // The folder holds headers that are duplicated in the Android source and
    // need to
    // provide a stable C ABI. Can't depend on //base.
    "android_webview/public/",

    // Exclude dir that should hold C headers.
    "mojo/public/c/",

    // Exclude code that only runs inside a renderer process - renderer
    // processes are excluded for now from the MiraclePtr project scope,
    // because they are sensitive to performance regressions (to a much higher
    // degree than, say, the Browser process).
    //
    // Note that some renderer-only directories are already excluded
    // elsewhere - for example "v8/" is excluded in another part of this
    // file.
    //
    // The common/ directories must be included in the rewrite as they contain
    // code
    // that is also used from the browser process.
    //
    // Also, note that isInThirdPartyLocation AST matcher in
    // RewriteRawPtrFields.cpp explicitly includes third_party/blink
    // (because it is in the same git repository as the rest of Chromium),
    // but we go ahead and exclude most of it below (as Renderer-only code).
    "/renderer/",                     // (e.g. //content/renderer/ or
                                      // //components/visitedlink/renderer/
                                      //  or //third_party/blink/renderer)",
    "third_party/blink/public/web/",  // TODO: Consider renaming this directory
                                      // to",
                                      // public/renderer?",

    // Contains sysroot dirs like debian_bullseye_amd64-sysroot/ that are not
    // part of the repository.
    "build/linux/",

    // glslang_tab.cpp.h uses #line directive and modifies the file path to
    // "MachineIndependent/glslang.y" so the isInThirdPartyLocation() filter
    // cannot
    // catch it even though glslang_tab.cpp.h is in third_party/
    "MachineIndependent/",

    // Exclude paths in separate repositories - i.e. in directories that
    // 1. Contain a ".git" subdirectory
    // 2. And hasn't been excluded via "third_party/" substring in their path
    //    (see the isInThirdPartyLocation AST matcher in
    //    RewriteRawPtrFields.cpp).
    //
    // The list below has been generated with:
    //
    //  $ find . -type d -name .git | \
//      sed -e 's/\.git$//g' | \
//      sed -e 's/\.\///g' | \
//      grep -v third_party | \
//      grep -v '^$' | \
//      sort | uniq > ~/scratch/git-paths
    "buildtools/clang_format/script/",
    "chrome/app/theme/default_100_percent/google_chrome/",
    "chrome/app/theme/default_200_percent/google_chrome/",
    "chrome/app/theme/google_chrome/",
    "chrome/app/vector_icons/google_chrome/",
    "chrome/browser/enterprise/connectors/internal/",
    "chrome/browser/google/linkdoctor_internal/",
    "chrome/browser/internal/",
    "chrome/browser/media/engagement_internal/",
    "chrome/browser/resources/chromeos/quickoffice/",
    "chrome/browser/resources/media_router_internal/",
    "chrome/browser/resources/preinstalled_web_apps/internal/",
    "chrome/browser/resources/settings_internal/",
    "chrome/browser/spellchecker/internal/",
    "chrome/browser/ui/media_router/internal/",
    "chrome/installer/mac/internal/",
    "chrome/test/data/firefox3_profile/searchplugins/",
    "chrome/test/data/firefox3_searchplugins/",
    "chrome/test/data/gpu/vt/",
    "chrome/test/data/pdf_private/",
    "chrome/test/data/perf/canvas_bench/",
    "chrome/test/data/perf/frame_rate/content/",
    "chrome/test/data/perf/frame_rate/private/",
    "chrome/test/data/perf/private/",
    "chrome/test/data/xr/webvr_info/",
    "chrome/test/media_router/internal/",
    "chrome/test/python_tests/",
    "chrome/tools/memory/",
    "clank/",
    "components/history_clusters/internal/",
    "components/ntp_tiles/resources/internal/",
    "components/optimization_guide/internal/",
    "components/resources/default_100_percent/google_chrome/",
    "components/resources/default_200_percent/google_chrome/",
    "components/resources/default_300_percent/google_chrome/",
    "components/site_isolation/internal/",
    "content/test/data/plugin/",
    "docs/website/",
    "google_apis/internal/",
    "media/cdm/api/",
    "native_client/",
    "remoting/android/internal/",
    "remoting/host/installer/linux/internal/",
    "remoting/internal/",
    "remoting/test/internal/",
    "remoting/tools/internal/",
    "remoting/webapp/app_remoting/internal/",
    "tools/page_cycler/acid3/",
    "tools/perf/data/",
    "ui/file_manager/internal/",
    "v8/",
    "webkit/data/bmp_decoder/",
    "webkit/data/ico_decoder/",
    "webkit/data/test_shell/plugins/",
};

#endif  // TOOLS_CLANG_PLUGINS_RAWPTRMANUALPATHSTOIGNORE_H_
