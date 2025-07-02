// Copyright 2015 The Crashpad Authors
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

#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <wchar.h>
#include <winhttp.h>

#include <iterator>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/scoped_generic.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "package.h"
#include "util/file/file_io.h"
#include "util/net/http_body.h"
#include "util/net/http_transport.h"
#include "util/numeric/safe_assignment.h"
#include "util/win/module_version.h"

namespace crashpad {

namespace {

constexpr wchar_t kWinHttpDll[] = L"winhttp.dll";

std::string UserAgent() {
  std::string user_agent =
      base::StringPrintf("%s/%s WinHTTP", PACKAGE_NAME, PACKAGE_VERSION);

  VS_FIXEDFILEINFO version;
  if (GetModuleVersionAndType(base::FilePath(kWinHttpDll), &version)) {
    user_agent.append(base::StringPrintf("/%lu.%lu.%lu.%lu",
                                         version.dwFileVersionMS >> 16,
                                         version.dwFileVersionMS & 0xffff,
                                         version.dwFileVersionLS >> 16,
                                         version.dwFileVersionLS & 0xffff));
  }

  if (GetModuleVersionAndType(base::FilePath(L"kernel32.dll"), &version) &&
      (version.dwFileOS & VOS_NT_WINDOWS32) == VOS_NT_WINDOWS32) {
    user_agent.append(base::StringPrintf(" Windows_NT/%lu.%lu.%lu.%lu (",
                                         version.dwFileVersionMS >> 16,
                                         version.dwFileVersionMS & 0xffff,
                                         version.dwFileVersionLS >> 16,
                                         version.dwFileVersionLS & 0xffff));
#if defined(ARCH_CPU_X86)
    user_agent.append("x86");
#elif defined(ARCH_CPU_X86_64)
    user_agent.append("x64");
#elif defined(ARCH_CPU_ARM64)
    user_agent.append("arm64");
#else
#error Port
#endif

    BOOL is_wow64;
    if (!IsWow64Process(GetCurrentProcess(), &is_wow64)) {
      PLOG(WARNING) << "IsWow64Process";
    } else if (is_wow64) {
      user_agent.append("; WoW64");
    }
    user_agent.append(1, ')');
  }

  return user_agent;
}

// PLOG doesn't work for messages from WinHTTP, so we need to use
// FORMAT_MESSAGE_FROM_HMODULE + the dll name manually here.
std::string WinHttpMessage(const char* extra) {
  DWORD error_code = GetLastError();
  char msgbuf[256];
  DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
                FORMAT_MESSAGE_MAX_WIDTH_MASK | FORMAT_MESSAGE_FROM_HMODULE;
  DWORD len = FormatMessageA(flags,
                             GetModuleHandle(kWinHttpDll),
                             error_code,
                             0,
                             msgbuf,
                             static_cast<DWORD>(std::size(msgbuf)),
                             nullptr);
  if (!len) {
    return base::StringPrintf("%s: error 0x%lx while retrieving error 0x%lx",
                              extra,
                              GetLastError(),
                              error_code);
  }

  // Most system messages end in a space. Remove the space if it’s there,
  // because the StringPrintf() below includes one.
  if (len >= 1 && msgbuf[len - 1] == ' ') {
    msgbuf[len - 1] = '\0';
  }
  return base::StringPrintf("%s: %s (0x%lx)", extra, msgbuf, error_code);
}

struct ScopedHINTERNETTraits {
  static HINTERNET InvalidValue() {
    return nullptr;
  }
  static void Free(HINTERNET handle) {
    if (handle) {
      if (!WinHttpCloseHandle(handle)) {
        LOG(ERROR) << WinHttpMessage("WinHttpCloseHandle");
      }
    }
  }
};

using ScopedHINTERNET = base::ScopedGeneric<HINTERNET, ScopedHINTERNETTraits>;

class HTTPTransportWin final : public HTTPTransport {
 public:
  HTTPTransportWin();

  HTTPTransportWin(const HTTPTransportWin&) = delete;
  HTTPTransportWin& operator=(const HTTPTransportWin&) = delete;

  ~HTTPTransportWin() override;

  bool ExecuteSynchronously(std::string* response_body) override;
};

HTTPTransportWin::HTTPTransportWin() : HTTPTransport() {
}

HTTPTransportWin::~HTTPTransportWin() {
}

bool HTTPTransportWin::ExecuteSynchronously(std::string* response_body) {
  ScopedHINTERNET session(WinHttpOpen(base::UTF8ToWide(UserAgent()).c_str(),
                                      WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS,
                                      0));
  if (!session.get()) {
    LOG(ERROR) << WinHttpMessage("WinHttpOpen");
    return false;
  }

  int timeout_in_ms = static_cast<int>(timeout() * 1000);
  if (!WinHttpSetTimeouts(session.get(),
                          timeout_in_ms,
                          timeout_in_ms,
                          timeout_in_ms,
                          timeout_in_ms)) {
    LOG(ERROR) << WinHttpMessage("WinHttpSetTimeouts");
    return false;
  }

  URL_COMPONENTS url_components = {0};
  url_components.dwStructSize = sizeof(URL_COMPONENTS);
  url_components.dwHostNameLength = 1;
  url_components.dwUrlPathLength = 1;
  url_components.dwExtraInfoLength = 1;
  std::wstring url_wide(base::UTF8ToWide(url()));
  // dwFlags = ICU_REJECT_USERPWD fails on XP.
  if (!WinHttpCrackUrl(
          url_wide.c_str(), 0, 0, &url_components)) {
    LOG(ERROR) << WinHttpMessage("WinHttpCrackUrl");
    return false;
  }
  DCHECK(url_components.nScheme == INTERNET_SCHEME_HTTP ||
         url_components.nScheme == INTERNET_SCHEME_HTTPS);
  std::wstring host_name(url_components.lpszHostName,
                         url_components.dwHostNameLength);
  std::wstring url_path(url_components.lpszUrlPath,
                        url_components.dwUrlPathLength);
  std::wstring extra_info(url_components.lpszExtraInfo,
                          url_components.dwExtraInfoLength);

  // Use url_path, and get the query parameter from extra_info, up to the first
  // #, if any. See RFC 7230 §5.3.1 and RFC 3986 §3.4. Beware that when this is
  // used to POST data, the query parameters generally belong in the request
  // body and not in the URL request target. It’s legal for them to be in both
  // places, but the interpretation is subject to whatever the client and server
  // agree on. This honors whatever was passed in, matching other platforms, but
  // you’ve been warned!
  std::wstring request_target(
      url_path.append(extra_info.substr(0, extra_info.find(L'#'))));

  ScopedHINTERNET connect(WinHttpConnect(
      session.get(), host_name.c_str(), url_components.nPort, 0));
  if (!connect.get()) {
    LOG(ERROR) << WinHttpMessage("WinHttpConnect");
    return false;
  }

  ScopedHINTERNET request(WinHttpOpenRequest(
      connect.get(),
      base::UTF8ToWide(method()).c_str(),
      request_target.c_str(),
      nullptr,
      WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES,
      url_components.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE
                                                      : 0));
  if (!request.get()) {
    LOG(ERROR) << WinHttpMessage("WinHttpOpenRequest");
    return false;
  }

  // Add headers to the request.
  //
  // If Content-Length is not provided, implement chunked mode per RFC 7230
  // §4.1.
  //
  // Note that chunked mode can only be used on Vista and later. Otherwise,
  // WinHttpSendRequest() requires a real value for dwTotalLength, used for the
  // Content-Length header. Determining that in the absence of a provided
  // Content-Length would require reading the entire request body before calling
  // WinHttpSendRequest().
  bool chunked = true;
  size_t content_length = 0;
  for (const auto& pair : headers()) {
    if (pair.first == kContentLength) {
      chunked = !base::StringToSizeT(pair.second, &content_length);
      DCHECK(!chunked);
    } else {
      std::wstring header_string = base::UTF8ToWide(pair.first) + L": " +
                                   base::UTF8ToWide(pair.second) + L"\r\n";
      if (!WinHttpAddRequestHeaders(
              request.get(),
              header_string.c_str(),
              base::checked_cast<DWORD>(header_string.size()),
              WINHTTP_ADDREQ_FLAG_ADD)) {
        LOG(ERROR) << WinHttpMessage("WinHttpAddRequestHeaders");
        return false;
      }
    }
  }

  DWORD content_length_dword;
  if (chunked) {
    static constexpr wchar_t kTransferEncodingHeader[] =
        L"Transfer-Encoding: chunked\r\n";
    if (!WinHttpAddRequestHeaders(
            request.get(),
            kTransferEncodingHeader,
            base::checked_cast<DWORD>(wcslen(kTransferEncodingHeader)),
            WINHTTP_ADDREQ_FLAG_ADD)) {
      LOG(ERROR) << WinHttpMessage("WinHttpAddRequestHeaders");
      return false;
    }

    content_length_dword = WINHTTP_IGNORE_REQUEST_TOTAL_LENGTH;
  } else if (!AssignIfInRange(&content_length_dword, content_length)) {
    content_length_dword = WINHTTP_IGNORE_REQUEST_TOTAL_LENGTH;
  }

  if (!WinHttpSendRequest(request.get(),
                          WINHTTP_NO_ADDITIONAL_HEADERS,
                          0,
                          WINHTTP_NO_REQUEST_DATA,
                          0,
                          content_length_dword,
                          0)) {
    LOG(ERROR) << WinHttpMessage("WinHttpSendRequest");
    return false;
  }

  size_t total_written = 0;
  FileOperationResult data_bytes;
  do {
    DataBuffer buf;
    // Read a block of data.
    static_assert(buf.kDataBytes < sizeof(buf.data));
    data_bytes =
        body_stream()->GetBytesBuffer(buf.data_span().data(), buf.kDataBytes);
    if (data_bytes == -1) {
      return false;
    }
    DCHECK_GE(data_bytes, 0);
    DCHECK_LE(static_cast<size_t>(data_bytes), buf.kDataBytes);

    void* write_start;
    DWORD write_size;

    if (chunked) {
      // Chunked encoding uses the entirety of buf. buf.size is presented in
      // hexadecimal without any leading "0x". The terminating CR and LF will be
      // placed immediately following the used portion of buf.data.

      unsigned int data_bytes_ui = base::checked_cast<unsigned int>(data_bytes);

      // snprintf() would NUL-terminate, but _snprintf() won’t.
      static_assert(buf.kSizeBytes < sizeof(buf.size));
      int rv = _snprintf(
          buf.size_span().data(), buf.kSizeBytes, "%08x", data_bytes_ui);
      DCHECK_GE(rv, 0);
      DCHECK_EQ(static_cast<size_t>(rv), buf.kSizeBytes);
      DCHECK_NE(buf.size_span()[buf.kSizeBytes - 1], '\0');

      base::span<char> size_crlf =
          buf.size_span().subspan(buf.kSizeBytes, buf.kCRLFBytes);
      size_crlf[0] = '\r';
      size_crlf[1] = '\n';
      base::span<uint8_t> data_crlf =
          buf.data_span().subspan(data_bytes_ui, buf.kCRLFBytes);
      data_crlf[0] = '\r';
      data_crlf[1] = '\n';

      // Move the left-hand edge of `buf.size` rightward to drop
      // leading zeroes.
      size_t to_skip = 0u;
      for (const char c : buf.size_span().first(buf.kSizeBytes - 1)) {
        if (c != '0') {
          break;
        }
        ++to_skip;
      }

      write_start = buf.size_span().subspan(to_skip).data();
      write_size = base::checked_cast<DWORD>(sizeof(buf.size) - to_skip +
                                             data_bytes + buf.kCRLFBytes);
    } else {
      // When not using chunked encoding, only use buf.data.
      write_start = buf.data.data();
      write_size = base::checked_cast<DWORD>(data_bytes);
    }

    // write_size will be 0 at EOF in non-chunked mode. Skip the write in that
    // case. In contrast, at EOF in chunked mode, a zero-length chunk must be
    // sent to signal EOF. This will happen when processing the EOF indicated by
    // a 0 return from body_stream()->GetBytesBuffer() above.
    if (write_size != 0) {
      DWORD written;
      if (!WinHttpWriteData(request.get(), write_start, write_size, &written)) {
        LOG(ERROR) << WinHttpMessage("WinHttpWriteData");
        return false;
      }

      DCHECK_EQ(written, write_size);
      total_written += written;
    }
  } while (data_bytes > 0);

  if (!chunked) {
    DCHECK_EQ(total_written, content_length);
  }

  if (!WinHttpReceiveResponse(request.get(), nullptr)) {
    LOG(ERROR) << WinHttpMessage("WinHttpReceiveResponse");
    return false;
  }

  DWORD status_code = 0;
  DWORD sizeof_status_code = sizeof(status_code);

  if (!WinHttpQueryHeaders(
          request.get(),
          WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
          WINHTTP_HEADER_NAME_BY_INDEX,
          &status_code,
          &sizeof_status_code,
          WINHTTP_NO_HEADER_INDEX)) {
    LOG(ERROR) << WinHttpMessage("WinHttpQueryHeaders");
    return false;
  }

  if (status_code < 200 || status_code > 203) {
    LOG(ERROR) << base::StringPrintf("HTTP status %lu", status_code);
    return false;
  }

  if (response_body) {
    response_body->clear();

    // There isn’t any reason to call WinHttpQueryDataAvailable(), because it
    // returns the number of bytes available to be read without blocking at the
    // time of the call, not the number of bytes until end-of-file. This method,
    // which executes synchronously, is only concerned with reading until EOF.
    DWORD bytes_read = 0;
    do {
      char read_buffer[4096];
      if (!WinHttpReadData(
              request.get(), read_buffer, sizeof(read_buffer), &bytes_read)) {
        LOG(ERROR) << WinHttpMessage("WinHttpReadData");
        return false;
      }

      response_body->append(read_buffer, bytes_read);
    } while (bytes_read > 0);
  }

  return true;
}

}  // namespace

// static
std::unique_ptr<HTTPTransport> HTTPTransport::Create() {
  return std::unique_ptr<HTTPTransportWin>(new HTTPTransportWin);
}

}  // namespace crashpad
