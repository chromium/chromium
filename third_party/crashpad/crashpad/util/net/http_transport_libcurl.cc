// Copyright 2017 The Crashpad Authors. All rights reserved.
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

#include "util/net/http_transport.h"

#include <curl/curl.h>
#include <dlfcn.h>
#include <string.h>
#include <sys/utsname.h>

#include <algorithm>
#include <limits>

#include "base/check.h"
#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/scoped_generic.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "package.h"
#include "util/misc/no_cfi_icall.h"
#include "util/net/http_body.h"
#include "util/numeric/safe_assignment.h"

namespace crashpad {

namespace {

// Crashpad depends on libcurl via dlopen() in order to maximize its tolerance
// of various libcurl flavors and installation configurations. This class serves
// as a linkage table for libcurl procedures.
class Libcurl {
 public:
  static bool Initialized() {
    static bool initialized = Get()->Initialize();
    return initialized;
  }

  static void CurlEasyCleanup(CURL* curl) {
    return Get()->curl_easy_cleanup_(curl);
  }

  static CURL* CurlEasyInit() { return Get()->curl_easy_init_(); }

  static CURLcode CurlEasyPerform(CURL* curl) {
    return Get()->curl_easy_perform_(curl);
  }

  static const char* CurlEasyStrError(CURLcode code) {
    return Get()->curl_easy_strerror_(code);
  }

  template <typename Pointer>
  static CURLcode CurlEasyGetInfo(CURL* curl, CURLINFO info, Pointer out) {
    return Get()->curl_easy_getinfo_(curl, info, out);
  }

  template <typename Pointer>
  static CURLcode CurlEasySetOpt(CURL* curl, CURLoption option, Pointer param) {
    return Get()->curl_easy_setopt_(curl, option, param);
  }

  static CURLcode CurlGlobalInit(long flags) {
    return Get()->curl_global_init_(flags);
  }

  static void CurlSlistFreeAll(struct curl_slist* slist) {
    return Get()->curl_slist_free_all_(slist);
  }

  static struct curl_slist* CurlSlistAppend(struct curl_slist* slist,
                                            const char* data) {
    return Get()->curl_slist_append_(slist, data);
  }

  static char* CurlVersion() { return Get()->curl_version_(); }

 private:
  Libcurl() = default;
  ~Libcurl() = delete;

  static Libcurl* Get() {
    static Libcurl* instance = new Libcurl();
    return instance;
  }

  bool Initialize() {
    void* libcurl = []() {
      std::vector<std::string> errors;
      for (const auto& lib : {
               "libcurl.so",
               "libcurl-gnutls.so.4",
               "libcurl-nss.so.4",
               "libcurl.so.4",
           }) {
        void* libcurl = dlopen(lib, RTLD_LAZY | RTLD_LOCAL);
        if (libcurl) {
          return libcurl;
        }
        errors.emplace_back(dlerror());
      }
      for (const auto& message : errors) {
        LOG(ERROR) << "dlopen:" << message;
      }
      return static_cast<void*>(nullptr);
    }();
    if (!libcurl) {
      return false;
    }

#define LINK_OR_RETURN_FALSE(symbol)               \
  do {                                             \
    symbol##_.SetPointer(dlsym(libcurl, #symbol)); \
    if (!symbol##_) {                              \
      LOG(ERROR) << "dlsym:" << dlerror();         \
      return false;                                \
    }                                              \
  } while (0);

    LINK_OR_RETURN_FALSE(curl_easy_cleanup);
    LINK_OR_RETURN_FALSE(curl_easy_init);
    LINK_OR_RETURN_FALSE(curl_easy_perform);
    LINK_OR_RETURN_FALSE(curl_easy_strerror);
    LINK_OR_RETURN_FALSE(curl_easy_getinfo);
    LINK_OR_RETURN_FALSE(curl_easy_setopt);
    LINK_OR_RETURN_FALSE(curl_global_init);
    LINK_OR_RETURN_FALSE(curl_slist_free_all);
    LINK_OR_RETURN_FALSE(curl_slist_append);
    LINK_OR_RETURN_FALSE(curl_version);

#undef LINK_OR_RETURN_FALSE

    return true;
  }

  NoCfiIcall<decltype(curl_easy_cleanup)*> curl_easy_cleanup_;
  NoCfiIcall<decltype(curl_easy_init)*> curl_easy_init_;
  NoCfiIcall<decltype(curl_easy_perform)*> curl_easy_perform_;
  NoCfiIcall<decltype(curl_easy_strerror)*> curl_easy_strerror_;
  NoCfiIcall<decltype(curl_easy_getinfo)*> curl_easy_getinfo_;
  NoCfiIcall<decltype(curl_easy_setopt)*> curl_easy_setopt_;
  NoCfiIcall<decltype(curl_global_init)*> curl_global_init_;
  NoCfiIcall<decltype(curl_slist_free_all)*> curl_slist_free_all_;
  NoCfiIcall<decltype(curl_slist_append)*> curl_slist_append_;
  NoCfiIcall<decltype(curl_version)*> curl_version_;

  DISALLOW_COPY_AND_ASSIGN(Libcurl);
};

std::string UserAgent() {
  std::string user_agent = base::StringPrintf(
      "%s/%s %s", PACKAGE_NAME, PACKAGE_VERSION, Libcurl::CurlVersion());

  utsname os;
  if (uname(&os) != 0) {
    PLOG(WARNING) << "uname";
  } else {
    // Match the architecture name that would be used by the kernel, so that the
    // strcmp() below can omit the kernel’s architecture name if it’s the same
    // as the user process’ architecture. On Linux, these names are normally
    // defined in each architecture’s Makefile as UTS_MACHINE, but can be
    // overridden in architecture-specific configuration as COMPAT_UTS_MACHINE.
    // See linux-4.9.17/arch/*/Makefile and
    // linux-4.9.17/arch/*/include/asm/compat.h. In turn, on some systems, these
    // names are further overridden or refined in early kernel startup code by
    // modifying the string returned by linux-4.9.17/include/linux/utsname.h
    // init_utsname() as noted.
#if defined(ARCH_CPU_X86)
    // linux-4.9.17/arch/x86/kernel/cpu/bugs.c check_bugs() sets the first digit
    // to 4, 5, or 6, but no higher.
#if defined(__i686__)
    static constexpr char arch[] = "i686";
#elif defined(__i586__)
    static constexpr char arch[] = "i586";
#elif defined(__i486__)
    static constexpr char arch[] = "i486";
#else
    static constexpr char arch[] = "i386";
#endif
#elif defined(ARCH_CPU_X86_64)
    static constexpr char arch[] = "x86_64";
#elif defined(ARCH_CPU_ARMEL)
    // linux-4.9.17/arch/arm/kernel/setup.c setup_processor() bases the string
    // on the ARM processor name and a character identifying little- or
    // big-endian. The processor name comes from a definition in
    // arch/arm/mm/proc-*.S.
#if defined(__ARM_ARCH_4T__)
    static constexpr char arch[] =
        "armv4t"
#elif defined(__ARM_ARCH_5TEJ__)
    static constexpr char arch[] =
        "armv5tej"
#elif defined(__ARM_ARCH_5TE__)
    static constexpr char arch[] =
        "armv5te"
#elif defined(__ARM_ARCH_5T__)
    static constexpr char arch[] =
        "armv5t"
#elif defined(__ARM_ARCH_7M__)
    static constexpr char arch[] =
        "armv7m"
#else
    // Most ARM architectures fall into here, including all profile variants of
    // armv6, armv7, armv8, with one exception, armv7m, handled above.
    // xstr(__ARM_ARCH) will be the architecture revision number, such as 6, 7,
    // or 8.
#define xstr(s) str(s)
#define str(s) #s
    static constexpr char arch[] =
        "armv" xstr(__ARM_ARCH)
#undef str
#undef xstr
#endif
#if defined(ARCH_CPU_LITTLE_ENDIAN)
        "l";
#elif defined(ARCH_CPU_BIG_ENDIAN)
        "b";
#endif
#elif defined(ARCH_CPU_ARM64)
    // ARM64 uses aarch64 or aarch64_be as directed by ELF_PLATFORM. See
    // linux-4.9.17/arch/arm64/kernel/setup.c setup_arch().
#if defined(ARCH_CPU_LITTLE_ENDIAN)
    static constexpr char arch[] = "aarch64";
#elif defined(ARCH_CPU_BIG_ENDIAN)
    static constexpr char arch[] = "aarch64_be";
#endif
#else
#error Port
#endif

    user_agent.append(
        base::StringPrintf(" %s/%s (%s", os.sysname, os.release, arch));
    if (strcmp(arch, os.machine) != 0) {
      user_agent.append(base::StringPrintf("; %s", os.machine));
    }
    user_agent.append(1, ')');
  }

  return user_agent;
}

std::string CurlErrorMessage(CURLcode curl_err, const std::string& base) {
  return base::StringPrintf("%s: %s (%d)",
                            base.c_str(),
                            Libcurl::CurlEasyStrError(curl_err),
                            curl_err);
}

struct ScopedCURLTraits {
  static CURL* InvalidValue() { return nullptr; }
  static void Free(CURL* curl) {
    if (curl) {
      Libcurl::CurlEasyCleanup(curl);
    }
  }
};
using ScopedCURL = base::ScopedGeneric<CURL*, ScopedCURLTraits>;

class CurlSList {
 public:
  CurlSList() : list_(nullptr) {}
  ~CurlSList() {
    if (list_) {
      Libcurl::CurlSlistFreeAll(list_);
    }
  }

  curl_slist* get() const { return list_; }

  bool Append(const char* data) {
    curl_slist* list = Libcurl::CurlSlistAppend(list_, data);
    if (!list_) {
      list_ = list;
    }
    return list != nullptr;
  }

 private:
  curl_slist* list_;

  DISALLOW_COPY_AND_ASSIGN(CurlSList);
};

class ScopedClearString {
 public:
  explicit ScopedClearString(std::string* string) : string_(string) {}

  ~ScopedClearString() {
    if (string_) {
      string_->clear();
    }
  }

  void Disarm() { string_ = nullptr; }

 private:
  std::string* string_;

  DISALLOW_COPY_AND_ASSIGN(ScopedClearString);
};

class HTTPTransportLibcurl final : public HTTPTransport {
 public:
  HTTPTransportLibcurl();
  ~HTTPTransportLibcurl() override;

  // HTTPTransport:
  bool ExecuteSynchronously(std::string* response_body) override;

 private:
  static size_t ReadRequestBody(char* buffer,
                                size_t size,
                                size_t nitems,
                                void* userdata);
  static size_t WriteResponseBody(char* buffer,
                                  size_t size,
                                  size_t nitems,
                                  void* userdata);

  DISALLOW_COPY_AND_ASSIGN(HTTPTransportLibcurl);
};

HTTPTransportLibcurl::HTTPTransportLibcurl() : HTTPTransport() {}

HTTPTransportLibcurl::~HTTPTransportLibcurl() {}

bool HTTPTransportLibcurl::ExecuteSynchronously(std::string* response_body) {
  DCHECK(body_stream());

  response_body->clear();

  // curl_easy_init() will do this on the first call if it hasn’t been done yet,
  // but not in a thread-safe way as is done here.
  static CURLcode curl_global_init_err = []() {
    return Libcurl::CurlGlobalInit(CURL_GLOBAL_DEFAULT);
  }();
  if (curl_global_init_err != CURLE_OK) {
    LOG(ERROR) << CurlErrorMessage(curl_global_init_err, "curl_global_init");
    return false;
  }

  CurlSList curl_headers;
  ScopedCURL curl(Libcurl::CurlEasyInit());
  if (!curl.get()) {
    LOG(ERROR) << "curl_easy_init";
    return false;
  }

// These macros wrap the repetitive “try something, log an error and return
// false on failure” pattern. Macros are convenient because the log messages
// will point to the correct line number, which can help pinpoint a problem when
// there are as many calls to these functions as there are here.
#define TRY_CURL_EASY_SETOPT(curl, option, parameter)               \
  do {                                                              \
    CURLcode curl_err =                                             \
        Libcurl::CurlEasySetOpt((curl), (option), (parameter));     \
    if (curl_err != CURLE_OK) {                                     \
      LOG(ERROR) << CurlErrorMessage(curl_err, "curl_easy_setopt"); \
      return false;                                                 \
    }                                                               \
  } while (false)
#define TRY_CURL_SLIST_APPEND(slist, data) \
  do {                                     \
    if (!(slist).Append(data)) {           \
      LOG(ERROR) << "curl_slist_append";   \
      return false;                        \
    }                                      \
  } while (false)

  TRY_CURL_EASY_SETOPT(curl.get(), CURLOPT_USERAGENT, UserAgent().c_str());

  // Accept and automatically decode any encoding that libcurl understands.
  TRY_CURL_EASY_SETOPT(curl.get(), CURLOPT_ACCEPT_ENCODING, "");

  TRY_CURL_EASY_SETOPT(curl.get(), CURLOPT_URL, url().c_str());

  if (!root_ca_certificate_path().empty()) {
    TRY_CURL_EASY_SETOPT(
        curl.get(), CURLOPT_CAINFO, root_ca_certificate_path().value().c_str());
  }

  constexpr int kMillisecondsPerSecond = 1E3;
  TRY_CURL_EASY_SETOPT(curl.get(),
                       CURLOPT_TIMEOUT_MS,
                       static_cast<long>(timeout() * kMillisecondsPerSecond));

  // If the request body size is known ahead of time, a Content-Length header
  // field will be present. Store that to use as CURLOPT_POSTFIELDSIZE_LARGE,
  // which will both set the Content-Length field in the request header and
  // inform libcurl of the request body size. Otherwise, use Transfer-Encoding:
  // chunked, which does not require advance knowledge of the request body size.
  bool chunked = true;
  size_t content_length;
  for (const auto& pair : headers()) {
    if (pair.first == kContentLength) {
      chunked = !base::StringToSizeT(pair.second, &content_length);
      DCHECK(!chunked);
    } else {
      TRY_CURL_SLIST_APPEND(curl_headers,
                            (pair.first + ": " + pair.second).c_str());
    }
  }

  if (method() == "POST") {
    TRY_CURL_EASY_SETOPT(curl.get(), CURLOPT_POST, 1l);

    // By default when sending a POST request, libcurl includes an “Expect:
    // 100-continue” header field. Althogh this header is specified in HTTP/1.1
    // (RFC 2616 §8.2.3, RFC 7231 §5.1.1), even collection servers that claim to
    // speak HTTP/1.1 may not respond to it. When sending this header field,
    // libcurl will wait for one second for the server to respond with a “100
    // Continue” status before continuing to transmit the request body. This
    // delay is avoided by telling libcurl not to send this header field at all.
    // The drawback is that certain HTTP error statuses may not be received
    // until after substantial amounts of data have been sent to the server.
    TRY_CURL_SLIST_APPEND(curl_headers, "Expect:");

    if (chunked) {
      TRY_CURL_SLIST_APPEND(curl_headers, "Transfer-Encoding: chunked");
    } else {
      curl_off_t content_length_curl;
      if (!AssignIfInRange(&content_length_curl, content_length)) {
        LOG(ERROR) << base::StringPrintf("Content-Length %zu too large",
                                         content_length);
        return false;
      }
      TRY_CURL_EASY_SETOPT(
          curl.get(), CURLOPT_POSTFIELDSIZE_LARGE, content_length_curl);
    }
  } else if (method() != "GET") {
    // Untested.
    TRY_CURL_EASY_SETOPT(curl.get(), CURLOPT_CUSTOMREQUEST, method().c_str());
  }

  TRY_CURL_EASY_SETOPT(curl.get(), CURLOPT_HTTPHEADER, curl_headers.get());

  TRY_CURL_EASY_SETOPT(curl.get(), CURLOPT_READFUNCTION, ReadRequestBody);
  TRY_CURL_EASY_SETOPT(curl.get(), CURLOPT_READDATA, this);
  TRY_CURL_EASY_SETOPT(curl.get(), CURLOPT_WRITEFUNCTION, WriteResponseBody);
  TRY_CURL_EASY_SETOPT(curl.get(), CURLOPT_WRITEDATA, response_body);

#undef TRY_CURL_EASY_SETOPT
#undef TRY_CURL_SLIST_APPEND

  // If a partial response body is received and then a failure occurs, ensure
  // that response_body is cleared.
  ScopedClearString clear_response_body(response_body);

  // Do it.
  CURLcode curl_err = Libcurl::CurlEasyPerform(curl.get());
  if (curl_err != CURLE_OK) {
    LOG(ERROR) << CurlErrorMessage(curl_err, "curl_easy_perform");
    return false;
  }

  long status;
  curl_err =
      Libcurl::CurlEasyGetInfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
  if (curl_err != CURLE_OK) {
    LOG(ERROR) << CurlErrorMessage(curl_err, "curl_easy_getinfo");
    return false;
  }

  if (status != 200) {
    LOG(ERROR) << base::StringPrintf("HTTP status %ld", status);
    return false;
  }

  // The response body is complete. Don’t clear it.
  clear_response_body.Disarm();

  return true;
}

// static
size_t HTTPTransportLibcurl::ReadRequestBody(char* buffer,
                                             size_t size,
                                             size_t nitems,
                                             void* userdata) {
  HTTPTransportLibcurl* self =
      reinterpret_cast<HTTPTransportLibcurl*>(userdata);

  // This libcurl callback mimics the silly stdio-style fread() interface: size
  // and nitems have been separated and must be multiplied.
  base::CheckedNumeric<size_t> checked_len = base::CheckMul(size, nitems);
  size_t len = checked_len.ValueOrDefault(std::numeric_limits<size_t>::max());

  // Limit the read to what can be expressed in a FileOperationResult.
  len = std::min(
      len,
      static_cast<size_t>(std::numeric_limits<FileOperationResult>::max()));

  FileOperationResult bytes_read = self->body_stream()->GetBytesBuffer(
      reinterpret_cast<uint8_t*>(buffer), len);
  if (bytes_read < 0) {
    return CURL_READFUNC_ABORT;
  }

  return bytes_read;
}

// static
size_t HTTPTransportLibcurl::WriteResponseBody(char* buffer,
                                               size_t size,
                                               size_t nitems,
                                               void* userdata) {
  std::string* response_body = reinterpret_cast<std::string*>(userdata);

  // This libcurl callback mimics the silly stdio-style fread() interface: size
  // and nitems have been separated and must be multiplied.
  base::CheckedNumeric<size_t> checked_len = base::CheckMul(size, nitems);
  size_t len = checked_len.ValueOrDefault(std::numeric_limits<size_t>::max());

  response_body->append(buffer, len);
  return len;
}

}  // namespace

// static
std::unique_ptr<HTTPTransport> HTTPTransport::Create() {
  return std::unique_ptr<HTTPTransport>(
      Libcurl::Initialized() ? new HTTPTransportLibcurl() : nullptr);
}

}  // namespace crashpad
