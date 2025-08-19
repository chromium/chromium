// Copyright 2014 The Crashpad Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "util/mac/mac_util.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <sys/types.h>

#include <string_view>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/mac/scoped_ioobject.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "build/build_config.h"
#include "util/mac/sysctl.h"

extern "C" {
// Private CoreFoundation internals. See 10.9.2 CF-855.14/CFPriv.h and
// CF-855.14/CFUtilities.c. These are marked for weak import because they’re
// private and subject to change.

#define WEAK_IMPORT __attribute__((weak_import))

// Don’t call these functions directly, call them through the
// TryCFCopy*VersionDictionary() helpers to account for the possibility that
// they may not be present at runtime.
CFDictionaryRef _CFCopySystemVersionDictionary() WEAK_IMPORT;

// Don’t use these constants with CFDictionaryGetValue() directly, use them with
// the TryCFDictionaryGetValue() wrapper to account for the possibility that
// they may not be present at runtime.
extern const CFStringRef _kCFSystemVersionProductNameKey WEAK_IMPORT;
extern const CFStringRef _kCFSystemVersionProductVersionKey WEAK_IMPORT;
extern const CFStringRef _kCFSystemVersionProductVersionExtraKey WEAK_IMPORT;
extern const CFStringRef _kCFSystemVersionBuildVersionKey WEAK_IMPORT;

#undef WEAK_IMPORT

}  // extern "C"

namespace {

// Helpers for the weak-imported private CoreFoundation internals.

CFDictionaryRef TryCFCopySystemVersionDictionary() {
  if (_CFCopySystemVersionDictionary) {
    return _CFCopySystemVersionDictionary();
  }
  return nullptr;
}

const void* TryCFDictionaryGetValue(CFDictionaryRef dictionary,
                                    const void* value) {
  if (value) {
    return CFDictionaryGetValue(dictionary, value);
  }
  return nullptr;
}

// Converts |version| to a triplet of version numbers on behalf of
// MacOSVersionNumber() and MacOSVersionComponents(). Returns true on success.
// If |version| does not have the expected format, returns false. |version| must
// be in the form "10.9.2" or just "10.9". In the latter case, |bugfix| will be
// set to 0.
bool StringToVersionNumbers(std::string_view version,
                            int* major,
                            int* minor,
                            int* bugfix) {
  size_t first_dot = version.find_first_of('.');
  if (first_dot == 0 || first_dot == std::string::npos ||
      first_dot == version.length() - 1) {
    LOG(ERROR) << "version has unexpected format";
    return false;
  }
  if (!base::StringToInt(std::string_view(&version[0], first_dot), major)) {
    LOG(ERROR) << "version has unexpected format";
    return false;
  }
  size_t second_dot = version.find_first_of('.', first_dot + 1);
  if (second_dot == version.length() - 1) {
    LOG(ERROR) << "version has unexpected format";
    return false;
  }
  if (second_dot == std::string::npos) {
    second_dot = version.length();
  }

  if (!base::StringToInt(
          std::string_view(&version[first_dot + 1], second_dot - first_dot - 1),
          minor)) {
    LOG(ERROR) << "version has unexpected format";
    return false;
  }
  if (second_dot == version.length()) {
    *bugfix = 0;
  } else if (!base::StringToInt(
                 std::string_view(&version[second_dot + 1],
                                  version.length() - second_dot - 1),
                 bugfix)) {
    LOG(ERROR) << "version has unexpected format";
    return false;
  }
  return true;
}

std::string IORegistryEntryDataPropertyAsString(io_registry_entry_t entry,
                                                CFStringRef key) {
  base::apple::ScopedCFTypeRef<CFTypeRef> property(
      IORegistryEntryCreateCFProperty(entry, key, kCFAllocatorDefault, 0));
  CFDataRef data = base::apple::CFCast<CFDataRef>(property.get());
  if (data && CFDataGetLength(data) > 0) {
    return reinterpret_cast<const char*>(CFDataGetBytePtr(data));
  }

  return std::string();
}

}  // namespace

namespace crashpad {

int MacOSVersionNumber() {
  static int macos_version_number = []() {
    // kern.osproductversion is a lightweight way to get the operating system
    // version from the kernel without having to open any files or spin up any
    // threads, but it’s only available in macOS 10.13.4 and later.
    std::string macos_version_number_string = ReadStringSysctlByName(
        "kern.osproductversion", true);
    DCHECK(!macos_version_number_string.empty());

    int major;
    int minor;
    int bugfix;
    bool success = StringToVersionNumbers(
            macos_version_number_string, &major, &minor, &bugfix);
    DCHECK(success);

    DCHECK_GE(major, 10);
    DCHECK_LE(major, 99);
    DCHECK_GE(minor, 0);
    DCHECK_LE(minor, 99);
    DCHECK_GE(bugfix, 0);
    DCHECK_LE(bugfix, 99);

    return major * 1'00'00 + minor * 1'00 + bugfix;
  }();

  return macos_version_number;
}

bool MacOSVersionComponents(int* major,
                            int* minor,
                            int* bugfix,
                            std::string* build,
                            std::string* version_string) {
  base::apple::ScopedCFTypeRef<CFDictionaryRef> dictionary(
      TryCFCopySystemVersionDictionary());
  if (!dictionary) {
    LOG(ERROR) << "_CFCopySystemVersionDictionary failed";
    return false;
  }

  bool success = true;

  CFStringRef version_cf =
      base::apple::CFCast<CFStringRef>(TryCFDictionaryGetValue(
          dictionary.get(), _kCFSystemVersionProductVersionKey));
  std::string version;
  if (!version_cf) {
    LOG(ERROR) << "version_cf not found";
    success = false;
  } else {
    version = base::SysCFStringRefToUTF8(version_cf);
    if (!StringToVersionNumbers(version, major, minor, bugfix)) {
      success = false;
    } else {
      DCHECK_GE(*major, 10);
      DCHECK_LE(*major, 99);
      DCHECK_GE(*minor, 0);
      DCHECK_LE(*minor, 99);
      DCHECK_GE(*bugfix, 0);
      DCHECK_LE(*bugfix, 99);
    }
  }

  CFStringRef build_cf =
      base::apple::CFCast<CFStringRef>(TryCFDictionaryGetValue(
          dictionary.get(), _kCFSystemVersionBuildVersionKey));
  if (!build_cf) {
    LOG(ERROR) << "build_cf not found";
    success = false;
  } else {
    build->assign(base::SysCFStringRefToUTF8(build_cf));
  }

  CFStringRef product_cf =
      base::apple::CFCast<CFStringRef>(TryCFDictionaryGetValue(
          dictionary.get(), _kCFSystemVersionProductNameKey));
  std::string product;
  if (!product_cf) {
    LOG(ERROR) << "product_cf not found";
    success = false;
  } else {
    product = base::SysCFStringRefToUTF8(product_cf);
  }

  // This key is not required, and in fact is normally not present.
  CFStringRef extra_cf =
      base::apple::CFCast<CFStringRef>(TryCFDictionaryGetValue(
          dictionary.get(), _kCFSystemVersionProductVersionExtraKey));
  std::string extra;
  if (extra_cf) {
    extra = base::SysCFStringRefToUTF8(extra_cf);
  }

  if (!product.empty() || !version.empty() || !build->empty()) {
    if (!extra.empty()) {
      version_string->assign(base::StringPrintf("%s %s %s (%s)",
                                                product.c_str(),
                                                version.c_str(),
                                                extra.c_str(),
                                                build->c_str()));
    } else {
      version_string->assign(base::StringPrintf(
          "%s %s (%s)", product.c_str(), version.c_str(), build->c_str()));
    }
  }

  return success;
}

void MacModelAndBoard(std::string* model, std::string* board_id) {
  base::mac::ScopedIOObject<io_service_t> platform_expert(
      IOServiceGetMatchingService(kIOMainPortDefault,
                                  IOServiceMatching("IOPlatformExpertDevice")));
  if (platform_expert) {
    model->assign(IORegistryEntryDataPropertyAsString(platform_expert.get(),
                                                      CFSTR("model")));
#if defined(ARCH_CPU_X86_FAMILY)
    board_id->assign(IORegistryEntryDataPropertyAsString(platform_expert.get(),
                                                         CFSTR("board-id")));
#elif defined(ARCH_CPU_ARM64)
    board_id->assign(IORegistryEntryDataPropertyAsString(
        platform_expert.get(), CFSTR("target-sub-type")));
    if (board_id->empty()) {
      board_id->assign(IORegistryEntryDataPropertyAsString(
          platform_expert.get(), CFSTR("target-type")));
    }
#else
#error Port.
#endif
  } else {
    model->clear();
    board_id->clear();
  }
}

}  // namespace crashpad
