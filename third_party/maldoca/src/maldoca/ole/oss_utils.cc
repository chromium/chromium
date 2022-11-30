// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "maldoca/ole/oss_utils.h"

#include <errno.h>

#include <algorithm>
#include <cstring>
#if defined(_WIN32)
#include <windows.h>

#include "absl/container/flat_hash_map.h"
#endif  // _WIN32

#include "absl/base/call_once.h"
#ifndef MALDOCA_IN_CHROMIUM
#include "absl/flags/flag.h"  // nogncheck
#endif
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#ifndef MALDOCA_IN_CHROMIUM
#include "google/protobuf/text_format.h"  // nogncheck
#endif
#include "libxml/SAX2.h"
#include "libxml/parserInternals.h"
#include "maldoca/base/encoding_error.h"

#ifdef MALDOCA_CHROME
const static int32_t default_max_proto_recursion = 400;
#else
ABSL_FLAG(int32_t, default_max_proto_recursion, 400,
          "Default max allowed recursion in proto parsing from text.");
#endif
#if defined(_WIN32)
// UTF-16 little endian code page on Windows:
// https://docs.microsoft.com/en-us/windows/win32/intl/code-page-identifiers
constexpr int32_t kUtf16LECodePage = 1200;
#endif  // _WIN32

namespace maldoca {
namespace utils {
namespace {
xmlSAXHandler sax_handler;
#if defined (_WIN32)
std::once_flag once_init;
#else
absl::once_flag once_init;
#endif

void InitSAXHandler() {
  xmlSAXVersion(&sax_handler, 2);
  // Disable the default SAX callbacks for dangerous operations.
  // Disable entityDecl - this seems to stop all trouble related to external
  // entities, as proved by the unit tests.
  sax_handler.entityDecl = nullptr;
  // Disable resolveEntity - this seems to stop all trouble related to
  // externally located DTD definitions, as proved by the unit tests. Note
  // that an application has to use XML_PARSE_DTDVALID or XML_PARSE_DTDLOAD to
  // be vulnerable here.
  sax_handler.resolveEntity = nullptr;
  // Parse warnings/errors won't be produced to stdout/stderr.
  sax_handler.error = nullptr;
  sax_handler.warning = nullptr;
  // For multithreaded operation, libxml must initialized through this
  // non-threadsafe function:
  xmlInitParser();
}

inline void StripNullChar(std::string* str) {
  auto is_not_null = [](char c) { return c != '\0'; };
  auto r_it = std::find_if(str->rbegin(), str->rend(), is_not_null);
  str->erase(str->rend() - r_it);
  auto it = std::find_if(str->begin(), str->end(), is_not_null);
  str->erase(str->begin(), it);
}
}  // namespace

bool BufferToUtf8::Init(const char* encode_name) {
  ::maldoca::ResetFailedEncoding();

#if !defined(_WIN32)
  if (converter_ != nullptr) {
    iconv_close(converter_);
    converter_ = nullptr;
  }
  internal_converter_ = InternalConverter::kNone;
  // Fixing missing encoding;
  // cp10000 is calld MAC in iconv
  if (absl::EqualsIgnoreCase(encode_name, "cp10000")) {
    encode_name = "MAC";
    DLOG(INFO) << "Replaced cp10000 with MAC";
  }

  converter_ = iconv_open("UTF-8", encode_name);
  if (converter_ == reinterpret_cast<iconv_t>(-1)) {
    converter_ = nullptr;
    LOG(ERROR) << "Failed to open iconv for '" << encode_name
               << "', error code: " << errno;
    // Windows encoding, we really want to make sure this works so we'll use our
    // own
    ::maldoca::SetFailedEncoding(encode_name);
    if (absl::EqualsIgnoreCase(encode_name, "cp1251")) {
      internal_converter_ = InternalConverter::kCp1251;
      DLOG(INFO) << "Use internal cp1251 encoder";
      return true;
    }
    if (absl::EqualsIgnoreCase(encode_name, "cp1252")) {
      internal_converter_ = InternalConverter::kCp1252;
      DLOG(INFO) << "Use internal cp1252 encoder";
      return true;
    }
    if (absl::EqualsIgnoreCase(encode_name, "LATIN1")) {
      internal_converter_ = InternalConverter::kLatin1;
      DLOG(INFO) << "Use internal LATIN1 encoder";
      return true;
    }
    return false;
  }
  return true;

#else   // _WIN32
  // Supported Windows code pages have to be mapped manually.
  // Source:
  // https://docs.microsoft.com/en-us/windows/win32/intl/code-page-identifiers
  // Aliases for the code pages are from:
  // https://www.ibm.com/docs/en/ibm-mq/8.0?topic=administering-available-code-pages
  static const absl::flat_hash_map<std::string, int>* name_to_code_map = [] {
    auto* map = new absl::flat_hash_map<std::string, int>();
    *map = {{"ibm037", 037},
            {"ibm437", 437},
            {"cspc8codepage437", 437},
            {"cp437", 437},
            {"ibm-437", 437},
            {"ibm500", 500},
            {"cp500", 500},
            {"ibm-500", 500},
            {"dos-720", 720},
            {"ibm-737", 737},
            {"cp737", 737},
            {"ibm737", 737},
            {"cp775", 775},
            {"ibm775", 775},
            {"ibm-775", 775},
            {"cspc850multilingual", 850},
            {"cp850", 850},
            {"ibm850", 850},
            {"ibm-850", 850},
            {"cp852", 852},
            {"ibm852", 852},
            {"ibm-852", 852},
            {"cspcp852", 852},
            {"cp855", 855},
            {"ibm855", 855},
            {"ibm-855", 855},
            {"cp857", 857},
            {"csibm857", 857},
            {"ibm857", 857},
            {"ibm-857", 857},
            {"ibm00858", 858},
            {"ibm-860", 860},
            {"ibm860", 860},
            {"cp860", 860},
            {"ibm-861", 861},
            {"ibm861", 861},
            {"cp861", 861},
            {"dos-862", 862},
            {"ibm-863", 863},
            {"ibm863", 863},
            {"cp863", 863},
            {"ibm-864", 864},
            {"ibm864", 864},
            {"cp864", 864},
            {"ibm-865", 865},
            {"ibm865", 865},
            {"cp865", 865},
            {"ibm-866", 866},
            {"ibm866", 866},
            {"cp866", 866},
            {"ibm-869", 869},
            {"ibm869", 869},
            {"cp869", 869},
            {"cp870", 870},
            {"ibm870", 870},
            {"ibm-870", 870},
            {"ms874", 874},
            {"windows-874", 874},
            {"cp875", 875},
            {"ibm875", 875},
            {"ibm-875", 875},
            {"shift_jis", 932},
            {"gb2312", 936},
            {"cp936", 936},
            {"big5", 950},
            {"big5-0", 950},
            {"big5-hkscs", 950},
            {"ibm1026", 1026},
            {"cp1026", 1026},
            {"ibm-1026", 1026},
            {"ibm01047", 1047},
            {"ibm01140", 1140},
            {"ibm01141", 1141},
            {"ibm01142", 1142},
            {"ibm01143", 1143},
            {"ibm01144", 1144},
            {"ibm01145", 1145},
            {"ibm01146", 1146},
            {"ibm01147", 1147},
            {"ibm01148", 1148},
            {"ibm01149", 1149},
            {"utf_16le", 1200},
            {"utf16le", 1200},
            {"utf-16le", 1200},
            {"unicodelittleunmarked", 1200},
            {"x-utf-16le", 1200},
            {"utf-16", 1200},
            {"unicodefffe", 1201},
            {"ibm-1250", 1250},
            {"windows-1250", 1250},
            {"cp1250", 1250},
            {"cp1251", 1251},
            {"ibm-1251", 1251},
            {"windows-1251", 1251},
            {"ibm-1252", 1252},
            {"windows-1252", 1252},
            {"cp1252", 1252},
            {"ibm-1253", 1253},
            {"windows-1253", 1253},
            {"cp1253", 1253},
            {"windows-1254", 1254},
            {"ibm-1254", 1254},
            {"cp1254", 1254},
            {"ibm-1255", 1255},
            {"windows-1255", 1255},
            {"cp1255", 1255},
            {"ibm-1256", 1256},
            {"windows-1256", 1256},
            {"cp1256", 1256},
            {"ibm-1257", 1257},
            {"windows-1257", 1257},
            {"cp1257", 1257},
            {"ibm-1129", 1258},
            {"ibm-1258", 1258},
            {"windows-1258", 1258},
            {"cp1258", 1258},
            {"johab", 1361},
            {"macintosh", 10000},
            {"x-mac-japanese", 10001},
            {"x-mac-chinesetrad", 10002},
            {"x-mac-korean", 10003},
            {"x-mac-arabic", 10004},
            {"x-mac-hebrew", 10005},
            {"x-mac-greek", 10006},
            {"x-mac-cyrillic", 10007},
            {"x-mac-chinesesimp", 10008},
            {"x-mac-romanian", 10010},
            {"x-mac-ukrainian", 10017},
            {"x-mac-thai", 10021},
            {"x-mac-ce", 10029},
            {"x-mac-icelandic", 10079},
            {"x-mac-turkish", 10081},
            {"x-mac-croatian", 10082},
            {"utf-32", 12000},
            {"utf-32be", 12001},
            {"x-chinese_cns", 20000},
            {"x-cp20001", 20001},
            {"x_chinese-eten", 20002},
            {"x-cp20003", 20003},
            {"x-cp20004", 20004},
            {"x-cp20005", 20005},
            {"x-ia5", 20105},
            {"x-ia5-german", 20106},
            {"x-ia5-swedish", 20107},
            {"x-ia5-norwegian", 20108},
            {"direct", 20127},
            {"646", 20127},
            {"ascii7", 20127},
            {"csascii", 20127},
            {"us-ascii", 20127},
            {"ansi_x3.4-1968", 20127},
            {"cp367", 20127},
            {"iso-646.irv:1991", 20127},
            {"ascii", 20127},
            {"iso-646.irv:1983", 20127},
            {"iso646-us", 20127},
            {"ibm-367", 20127},
            {"ansi_x3.4-1986", 20127},
            {"iso-ir-6", 20127},
            {"default", 20127},
            {"us", 20127},
            {"x-cp20261", 20261},
            {"x-cp20269", 20269},
            {"ibm-273", 20273},
            {"ibm273", 20273},
            {"cp273", 20273},
            {"ibm-277", 20277},
            {"ibm277", 20277},
            {"cp277", 20277},
            {"ibm-278", 20278},
            {"ibm278", 20278},
            {"cp278", 20278},
            {"cp280", 20280},
            {"ibm280", 20280},
            {"ibm-280", 20280},
            {"cp284", 20284},
            {"ibm284", 20284},
            {"ibm-284", 20284},
            {"cp285", 20285},
            {"ibm285", 20285},
            {"ibm-285", 20285},
            {"ibm290", 20290},
            {"ibm-297", 20297},
            {"cp297", 20297},
            {"ibm297", 20297},
            {"cp420", 20420},
            {"ibm-420", 20420},
            {"ibm420", 20420},
            {"ibm423", 20423},
            {"cp424", 20424},
            {"ibm-424", 20424},
            {"ibm424", 20424},
            {"x-ebcdic-koreanextended", 20833},
            {"ibm-thai", 20838},
            {"koi8", 20866},
            {"cskoi8r", 20866},
            {"koi8_r", 20866},
            {"koi8-r", 20866},
            {"ibm-878", 20866},
            {"cp871", 20871},
            {"ibm871", 20871},
            {"ibm-871", 20871},
            {"ibm880", 20880},
            {"ibm905", 20905},
            {"ibm00924", 20924},
            {"x-cp20936", 20936},
            {"x-cp20949", 20949},
            {"ibm1025", 21025},
            {"cp1025", 21025},
            {"ibm-1025", 21025},
            {"koi8-u", 21866},
            {"iso-8859-1", 28591},
            {"iso-ir-100", 28591},
            {"8859-1", 28591},
            {"csisolatin1", 28591},
            {"ibm819", 28591},
            {"iso8859-1", 28591},
            {"ibm-819", 28591},
            {"iso-8859-1:1987", 28591},
            {"iso8859_1", 28591},
            {"l1", 28591},
            {"latin1", 28591},
            {"cp819", 28591},
            {"iso-8859-2", 28592},
            {"iso-ir-101", 28592},
            {"8859-2", 28592},
            {"ibm912", 28592},
            {"ibm-912", 28592},
            {"latin2", 28592},
            {"iso-8859-2:1987", 28592},
            {"csisolatin2", 28592},
            {"iso8859-2", 28592},
            {"iso8859_2", 28592},
            {"cp912", 28592},
            {"l2", 28592},
            {"iso-8859-3", 28593},
            {"iso-ir-109", 28593},
            {"8859-3", 28593},
            {"ibm-913", 28593},
            {"latin3", 28593},
            {"csisolatin3", 28593},
            {"iso8859-3", 28593},
            {"iso8859_3", 28593},
            {"iso-8859-3:1988", 28593},
            {"cp913", 28593},
            {"l3", 28593},
            {"iso-ir-110", 28594},
            {"iso-8859-4", 28594},
            {"8859-4", 28594},
            {"latin4", 28594},
            {"ibm-914", 28594},
            {"csisolatin4", 28594},
            {"iso8859-4", 28594},
            {"iso-8859-4:1988", 28594},
            {"iso8859_4", 28594},
            {"cp914", 28594},
            {"l4", 28594},
            {"iso-8859-5", 28595},
            {"8859-5", 28595},
            {"cyrillic", 28595},
            {"ibm915", 28595},
            {"iso-ir-144", 28595},
            {"ibm-915", 28595},
            {"iso-8859-5:1988", 28595},
            {"iso8859-5", 28595},
            {"iso8859_5", 28595},
            {"csisolatincyrillic", 28595},
            {"cp915", 28595},
            {"asmo-708", 28596},
            {"csisolatinarabic", 28596},
            {"iso-8859-6", 28596},
            {"ibm1089", 28596},
            {"ecma-114", 28596},
            {"8859-6", 28596},
            {"iso-8859-6:1987", 28596},
            {"ibm-1089", 28596},
            {"iso8859-6", 28596},
            {"iso8859_6", 28596},
            {"cp1089", 28596},
            {"arabic", 28596},
            {"iso-ir-127", 28596},
            {"iso-8859-7:1987", 28597},
            {"iso8859-7", 28597},
            {"iso-8859-7", 28597},
            {"greek", 28597},
            {"ecma-118", 28597},
            {"greek8", 28597},
            {"csisolatingreek", 28597},
            {"ibm813", 28597},
            {"8859-7", 28597},
            {"elot-928", 28597},
            {"iso8859_7", 28597},
            {"ibm-813", 28597},
            {"iso-ir-126", 28597},
            {"cp813", 28597},
            {"csisolatinhebrew", 28598},
            {"hebrew", 28598},
            {"iso-8859-8", 28598},
            {"8859-8", 28598},
            {"ibm916", 28598},
            {"iso8859-8", 28598},
            {"iso-ir-138", 28598},
            {"iso8859_8", 28598},
            {"iso-8859-8:1988", 28598},
            {"cp916", 28598},
            {"ibm-916", 28598},
            {"iso-8859-9", 28599},
            {"l5", 28599},
            {"ibm920", 28599},
            {"8859-9", 28599},
            {"iso-ir-148", 28599},
            {"latin5", 28599},
            {"csisolatin5", 28599},
            {"iso8859-9", 28599},
            {"iso8859_9", 28599},
            {"ibm-920", 28599},
            {"cp920", 28599},
            {"8859-13", 28603},
            {"iso-8859-13", 28603},
            {"iso8859_13", 28603},
            {"iso8859-13", 28603},
            {"l9", 28605},
            {"latin9", 28605},
            {"csisolatin9", 28605},
            {"iso8859_15_fdis", 28605},
            {"ibm-923", 28605},
            {"iso8859-15", 28605},
            {"ibm923", 28605},
            {"iso-8859-15", 28605},
            {"latin0", 28605},
            {"iso8859_15", 28605},
            {"cp923", 28605},
            {"x-europa", 29001},
            {"iso-8859-8-i", 38598},
            {"csiso2022jp", 50221},
            {"iso-2022-jp", 50222},
            {"jis", 50222},
            {"iso2022-jp", 50222},
            {"jis-encoding", 50222},
            {"iso2022jp", 50222},
            {"iso-2022-jp2", 50222},
            {"csiso2022jp2", 50222},
            {"csjisencoding", 50222},
            {"iso2022kr", 50225},
            {"iso2022-kr", 50225},
            {"csiso2022kr", 50225},
            {"iso-2022-kr", 50225},
            {"x-cp50227", 50227},
            {"eucjp", 51932},
            {"euc_jp_linux", 51932},
            {"euc-jp-linux", 51932},
            {"euc-jp", 51932},
            {"x-euc-jp", 51932},
            {"euc_jp", 51932},
            {"x-eucjp", 51932},
            {"euc-cn", 51936},
            {"euc_cn", 51936},
            {"euccn", 51936},
            {"ibm-euccn", 51936},
            {"x-euc-cn", 51936},
            {"ksc5601-1987", 51949},
            {"euc_kr", 51949},
            {"ksc5601_1987", 51949},
            {"cp970", 51949},
            {"ks_c_5601-1987", 51949},
            {"ibm-euckr", 51949},
            {"euc-kr", 51949},
            {"euckr", 51949},
            {"5601", 51949},
            {"ibm-970", 51949},
            {"ksc_5601", 51949},
            {"hz-gb-2312", 52936},
            {"gb18030", 54936},
            {"gb18030-2000", 54936},
            {"ibm-1392", 54936},
            {"windows-54936", 54936},
            {"x-iscii-de", 57002},
            {"x-iscii-be", 57003},
            {"x-iscii-ta", 57004},
            {"x-iscii-te", 57005},
            {"x-iscii-as", 57006},
            {"x-iscii-or", 57007},
            {"x-iscii-ka", 57008},
            {"x-iscii-ma", 57009},
            {"x-iscii-gu", 57010},
            {"x-iscii-pa", 57011},
            {"utf-7", 65000},
            {"utf8", 65001},
            {"utf_8", 65001},
            {"utf-8", 65001}};
    return map;
  }();

  std::string encode_name_lower(encode_name);
  absl::AsciiStrToLower(&encode_name_lower);

  auto iter = name_to_code_map->find(encode_name_lower);
  if (iter != name_to_code_map->end()) {
    code_page_ = iter->second;
    init_success_ = true;
    DLOG(INFO) << "Using Windows code page: " << code_page_
               << " for encoding: " << encode_name_lower;
    return true;
  } else {
    init_success_ = false;
    ::maldoca::SetFailedEncoding(encode_name);
    LOG(ERROR) << "Windows code page is not supported: " << encode_name;
    return false;
  }
#endif  // _WIN32
}

bool BufferToUtf8::ConvertLatin1BufferToUTF8String(absl::string_view input,
                                                   std::string* out_str,
                                                   int* bytes_consumed,
                                                   int* bytes_filled,
                                                   int* error_char_count) {
  auto old_output_size = out_str->size();
  *error_char_count = 0;
  for (uint8_t c : input) {
    ++(*bytes_consumed);
    if (c == 0) {
      ++(*error_char_count);
      continue;
    }
    if (c < 0x80) {
      out_str->push_back(c);
    } else {
      out_str->push_back(0xc0 | (c >> 6));
      out_str->push_back(0x80 | (c & 0x3f));
    }
  }
  *bytes_filled = out_str->size() - old_output_size;
  return *error_char_count <= max_error_;
}

bool BufferToUtf8::ConvertCp1251BufferToUTF8String(absl::string_view input,
                                                   std::string* out_str,
                                                   int* bytes_consumed,
                                                   int* bytes_filled,
                                                   int* error_char_count) {
  auto old_output_size = out_str->size();
  *error_char_count = 0;
  static const char utf_8[128][4] = {"\xD0\x82",
                                     "\xD0\x83",
                                     "\xE2\x80\x9A",
                                     "\xD1\x93",
                                     "\xE2\x80\x9E",
                                     "\xE2\x80\xA6",
                                     "\xE2\x80\xA0",
                                     "\xE2\x80\xA1",
                                     "\xE2\x82\xAC",
                                     "\xE2\x80\xB0",
                                     "\xD0\x89",
                                     "\xE2\x80\xB9",
                                     "\xD0\x8A",
                                     "\xD0\x8C",
                                     "\xD0\x8B",
                                     "\xD0\x8F",
                                     "\xD1\x92",
                                     "\xE2\x80\x98",
                                     "\xE2\x80\x99",
                                     "\xE2\x80\x9C",
                                     "\xE2\x80\x9D",
                                     "\xE2\x80\xA2",
                                     "\xE2\x80\x93",
                                     "\xE2\x80\x94",
                                     "",
                                     "\xE2\x84\xA2",
                                     "\xD1\x99",
                                     "\xE2\x80\xBA",
                                     "\xD1\x9A",
                                     "\xD1\x9C",
                                     "\xD1\x9B",
                                     "\xD1\x9F",
                                     "\xC2\xA0",
                                     "\xD0\x8E",
                                     "\xD1\x9E",
                                     "\xD0\x88",
                                     "\xC2\xA4",
                                     "\xD2\x90",
                                     "\xC2\xA6",
                                     "\xC2\xA7",
                                     "\xD0\x81",
                                     "\xC2\xA9",
                                     "\xD0\x84",
                                     "\xC2\xAB",
                                     "\xC2\xAC",
                                     "\xC2\xAD",
                                     "\xC2\xAE",
                                     "\xD0\x87",
                                     "\xC2\xB0",
                                     "\xC2\xB1",
                                     "\xD0\x86",
                                     "\xD1\x96",
                                     "\xD2\x91",
                                     "\xC2\xB5",
                                     "\xC2\xB6",
                                     "\xC2\xB7",
                                     "\xD1\x91",
                                     "\xE2\x84\x96",
                                     "\xD1\x94",
                                     "\xC2\xBB",
                                     "\xD1\x98",
                                     "\xD0\x85",
                                     "\xD1\x95",
                                     "\xD1\x97",
                                     "\xD0\x90",
                                     "\xD0\x91",
                                     "\xD0\x92",
                                     "\xD0\x93",
                                     "\xD0\x94",
                                     "\xD0\x95",
                                     "\xD0\x96",
                                     "\xD0\x97",
                                     "\xD0\x98",
                                     "\xD0\x99",
                                     "\xD0\x9A",
                                     "\xD0\x9B",
                                     "\xD0\x9C",
                                     "\xD0\x9D",
                                     "\xD0\x9E",
                                     "\xD0\x9F",
                                     "\xD0\xA0",
                                     "\xD0\xA1",
                                     "\xD0\xA2",
                                     "\xD0\xA3",
                                     "\xD0\xA4",
                                     "\xD0\xA5",
                                     "\xD0\xA6",
                                     "\xD0\xA7",
                                     "\xD0\xA8",
                                     "\xD0\xA9",
                                     "\xD0\xAA",
                                     "\xD0\xAB",
                                     "\xD0\xAC",
                                     "\xD0\xAD",
                                     "\xD0\xAE",
                                     "\xD0\xAF",
                                     "\xD0\xB0",
                                     "\xD0\xB1",
                                     "\xD0\xB2",
                                     "\xD0\xB3",
                                     "\xD0\xB4",
                                     "\xD0\xB5",
                                     "\xD0\xB6",
                                     "\xD0\xB7",
                                     "\xD0\xB8",
                                     "\xD0\xB9",
                                     "\xD0\xBA",
                                     "\xD0\xBB",
                                     "\xD0\xBC",
                                     "\xD0\xBD",
                                     "\xD0\xBE",
                                     "\xD0\xBF",
                                     "\xD1\x80",
                                     "\xD1\x81",
                                     "\xD1\x82",
                                     "\xD1\x83",
                                     "\xD1\x84",
                                     "\xD1\x85",
                                     "\xD1\x86",
                                     "\xD1\x87",
                                     "\xD1\x88",
                                     "\xD1\x89",
                                     "\xD1\x8A",
                                     "\xD1\x8B",
                                     "\xD1\x8C",
                                     "\xD1\x8D",
                                     "\xD1\x8E",
                                     "\xD1\x8F"};
  for (uint8_t c : input) {
    ++(*bytes_consumed);
    if (c == 0) {
      ++(*error_char_count);
      continue;
    }
    if (c < 0x80) {
      out_str->push_back(c);
    } else {
      c -= 0x80;
      auto val = utf_8[c];
      int len = strlen(val);
      if (len == 0) {
        ++(*error_char_count);
        continue;
      }
      out_str->append(val, len);
    }
  }
  *bytes_filled = out_str->size() - old_output_size;
  return *error_char_count <= max_error_;
}

bool BufferToUtf8::ConvertCp1252BufferToUTF8String(absl::string_view input,
                                                   std::string* out_str,
                                                   int* bytes_consumed,
                                                   int* bytes_filled,
                                                   int* error_char_count) {
  auto old_output_size = out_str->size();
  *error_char_count = 0;
  static const char utf_8[128][5] = {"\xE2\x82\xAC",
                                     "",
                                     "\xE2\x80\x9A",
                                     "\xC6\x92",
                                     "\xE2\x80\x9E",
                                     "\xE2\x80\xA6",
                                     "\xE2\x80\xA0",
                                     "\xE2\x80\xA1",
                                     "\xCB\x86",
                                     "\xE2\x80\xB0",
                                     "\xC5\xA0",
                                     "\xE2\x80\xB9",
                                     "\xC5\x92",
                                     "",
                                     "\xC5\xBD",
                                     "",
                                     "",
                                     "\xE2\x80\x98",
                                     "\xE2\x80\x99",
                                     "\xE2\x80\x9C",
                                     "\xE2\x80\x9D",
                                     "\xE2\x80\xA2",
                                     "\xE2\x80\x93",
                                     "\xE2\x80\x94",
                                     "\xCB\x9C",
                                     "\xE2\x84\xA2",
                                     "\xC5\xA1",
                                     "\xE2\x80\xBA",
                                     "\xC5\x93",
                                     "",
                                     "\xC5\xBE",
                                     "\xC5\xB8"};
  for (uint8_t c : input) {
    ++(*bytes_consumed);
    if (c == 0) {
      ++(*error_char_count);
      continue;
    }
    if (c < 0x80) {
      out_str->push_back(c);
    } else if (c >= 0x80 && c <= 0x9f) {
      c -= 0x80;
      auto val = utf_8[c];
      int len = strlen(val);
      if (len == 0) {
        ++(*error_char_count);
        continue;
      }
      out_str->append(val, len);
    } else {
      out_str->push_back(0xc0 | (c >> 6));
      out_str->push_back(0x80 | (c & 0x3f));
    }
  }
  *bytes_filled = out_str->size() - old_output_size;
  return *error_char_count <= max_error_;
}

bool BufferToUtf8::ConvertEncodingBufferToUTF8String(absl::string_view input,
                                                     std::string* out_str,
                                                     int* bytes_consumed,
                                                     int* bytes_filled,
                                                     int* error_char_count) {
  // TODO: refactor and split up this function.
  CHECK_NE(bytes_consumed, static_cast<int*>(nullptr));
  CHECK_NE(bytes_filled, static_cast<int*>(nullptr));
  CHECK_NE(error_char_count, static_cast<int*>(nullptr));
  *bytes_consumed = 0;
  *bytes_filled = 0;
  *error_char_count = 0;
  switch (internal_converter_) {
    case InternalConverter::kCp1251:
      return ConvertCp1251BufferToUTF8String(input, out_str, bytes_consumed,
                                             bytes_filled, error_char_count);

    case InternalConverter::kCp1252:
      return ConvertCp1252BufferToUTF8String(input, out_str, bytes_consumed,
                                             bytes_filled, error_char_count);

    case InternalConverter::kLatin1:
      return ConvertLatin1BufferToUTF8String(input, out_str, bytes_consumed,
                                             bytes_filled, error_char_count);
    case InternalConverter::kNone:
      break;
      // Intentionally fallthrough
  }
  CHECK(internal_converter_ == InternalConverter::kNone);
  size_t in_bytes_left = input.size();
  if (in_bytes_left == 0) {
    return true;
  }
  const char* input_ptr = input.data();

#if defined(_WIN32)
  int wc_size = 0;  // size of the wide-character string in characters
  int mb_size = 0;  // size of the multi-byte string in bytes
  std::unique_ptr<wchar_t[]> wc_data;
  const wchar_t* wc_str;
  bool is_already_utf16 = code_page_ == kUtf16LECodePage;

  // No need to convert to UTF-16, if the string is already UTF-16 encoded.
  if (!is_already_utf16) {
    // Calculate output size in UTF-16.
    wc_size =
        MultiByteToWideChar(code_page_, 0, input_ptr, input.size(), NULL, 0);
    DLOG(INFO) << "Output size in UTF-16: " << wc_size;
    if (wc_size <= 0) {
      LOG(ERROR) << "Error while calculating output size in UTF-16: "
                 << GetLastError();
      return false;
    }

    // Allocate memory for the temp UTF-16 output and convert input to UTF-16.
    wc_data = std::make_unique<wchar_t[]>(wc_size);
    wc_str = wc_data.get();
    wc_size = MultiByteToWideChar(code_page_, 0, input_ptr, input.size(),
                                  wc_data.get(), wc_size);
    if (wc_size <= 0) {
      LOG(ERROR) << "Error while converting to UTF-16: " << GetLastError();
      return false;
    }
  } else {
    wc_str = reinterpret_cast<const wchar_t*>(input.data());
    // We're casting the input from char (1 byte) to wchar_t (2 bytes), so we
    // also have to half the size (in wchar_t).
    wc_size = input.size() / 2;
  }

  // Calculate output size in UTF-8.
  mb_size =
      WideCharToMultiByte(CP_UTF8, 0, wc_str, wc_size, NULL, 0, NULL, NULL);
  DLOG(INFO) << "Output size in UTF-8: " << mb_size;
  if (mb_size <= 0) {
    LOG(ERROR) << "Error while calculating output size in UTF-8: "
               << GetLastError();
    return false;
  }

  // Allocate proper memory and convert input to UTF-8.
  out_str->resize(mb_size);
  mb_size = WideCharToMultiByte(CP_UTF8, 0, wc_str, wc_size, &(*out_str)[0],
                                mb_size, NULL, NULL);
  if (mb_size <= 0) {
    LOG(ERROR) << "Error while converting to UTF-8: " << GetLastError();
    return false;
  }

  // TODO: fallback to internal converters if available.

  // For some reason, it preserves start and trailing \0 so remove them
  StripNullChar(out_str);
  *bytes_consumed = input.size();
  *bytes_filled = out_str->size();

  return true;
#else   // !_WIN32
  // Guess what the output size will be; make it the same to start
  // TODO(somebody): make a better guess here.
  size_t out_bytes_left = in_bytes_left;
  size_t old_output_size = out_str->size();
  size_t new_output_size = old_output_size + out_bytes_left;
  out_str->resize(new_output_size);
  char* out_ptr = const_cast<char*>(out_str->data() + old_output_size);

  while (in_bytes_left > 0) {
    size_t done = iconv(converter_, const_cast<char**>(&input_ptr),
                        &in_bytes_left, &out_ptr, &out_bytes_left);
    if (done == static_cast<size_t>(-1)) {
      // try to handle error
      switch (errno) {
        case E2BIG: {  // output too small, increase size and continue
          auto already_out = out_ptr - out_str->data();
          size_t additional_alloc =
              std::max(2u * in_bytes_left, static_cast<size_t>(16UL));
          out_str->resize(out_str->size() + additional_alloc);
          out_ptr = const_cast<char*>(out_str->data() + already_out);
          out_bytes_left = out_str->size() - already_out;
          break;
        }

        case EINVAL:
          // ignore last incomplete char
          in_bytes_left = 0;
          break;

        case EILSEQ:
          // bad char, skip unless too many errors.
          if (++(*error_char_count) <= max_error_) {
            ++input_ptr;
            --in_bytes_left;
            DLOG(INFO) << "skipping EILSEQ: in_bytes_left: " << in_bytes_left
                       << ", out_bytes_left: " << out_bytes_left
                       << ", char: " << *(input_ptr - 1);
            break;
          }
          // intent to fall through
          [[fallthrough]];

        default:
          // give up
          out_str->resize(
              std::max(static_cast<long>(out_ptr - out_str->data()), 0l));
          LOG(ERROR) << "failed with error: " << errno
                     << ", in_bytes_left: " << in_bytes_left
                     << ", out_bytes_left: " << out_bytes_left
                     << ", char: " << *(input_ptr - 1);
          return false;
      }
    }
  }
  // resize to actual size
  out_str->resize(std::max(static_cast<long>(out_ptr - out_str->data()), 0l));
  // For some reason, it preserves start and trailing \0 so remove them
  StripNullChar(out_str);
  *bytes_consumed = input.size() - in_bytes_left;
  *bytes_filled = out_str->size();
  return *error_char_count <= max_error_;
#endif  // _WIN32
}

xmlDocPtr XmlParseMemory(const char* buffer, int size) {
  #if defined(_WIN32)
  std::call_once(once_init, &InitSAXHandler);
  #else
  absl::call_once(once_init, &InitSAXHandler);
  #endif
  return xmlSAXParseMemory(&sax_handler, buffer, size, 0);
}

// Converts an `xmlChar*` object to a string_view.
absl::string_view XmlCharPointerToString(const xmlChar* word) {
  return absl::string_view(reinterpret_cast<const char*>(word));
}

// Recursively splitting the 32 bits into upper 16 and lower 16 etc and
// add the offset depending if the upper half is 0 or not until
// down to 8 bits and use a lookup table.
int Log2Floor(uint32_t n) {
  // A lookup table for just 8 bits
  static const char* log_table = [] {
    char* table = new char[256];
    table[0] = table[1] = 0;
    for (int i = 2; i < 256; ++i) {
      table[i] = 1 + table[i >> 1];
    }
    table[0] = -1;
    return table;
  }();
  int lg;
  uint32_t tmp1 = n >> 16;
  if (tmp1) {
    uint32_t tmp2 = tmp1 >> 8;
    lg = tmp2 ? 24 + log_table[tmp2] : 16 + log_table[tmp1];
  } else {
    uint32_t tmp2 = n >> 8;
    lg = tmp2 ? 8 + log_table[tmp2] : log_table[n];
  }
  return lg;
}

int Log2Ceiling(uint32_t n) {
  if (n == 0) {
    return -1;
  }
  int floor = Log2Floor(n);
  if ((1u << floor) == n) {
    return floor;
  } else {
    return floor + 1;
  }
}

// TODO(somebody): Fix me! Currently uses a minimum implementation but could
// be better.
bool ReadFileToString(absl::string_view filename, std::string* content,
                      bool log_error, bool xor_decode_file) {
  auto status_or = file::GetContents(filename, xor_decode_file);
  if (!status_or.ok()) {
    if (log_error) {
      LOG(ERROR) << "Can not read " << filename
                 << ", error: " << status_or.status();
    }
    return false;
  }
  *content = status_or.value();
  return true;
}
#ifndef MALDOCA_CHROME
void StringErrorCollector::AddError(int line,
                                    ::google::protobuf::io::ColumnNumber column,
                                    const std::string& message) {
  absl::StrAppend(&message_, absl::StrFormat("ERROR (%d, %d): %s\n", line + 1,
                                             column + 1, message));
}

void StringErrorCollector::AddWarning(
    int line, ::google::protobuf::io::ColumnNumber column,
    const std::string& message) {
  absl::StrAppend(&message_, absl::StrFormat("WARNING (%d, %d): %s\n", line + 1,
                                             column + 1, message));
}

bool ProtoToText(const ::google::protobuf::Message& message, bool one_line,
                 std::string* text) {
  ::google::protobuf::TextFormat::Printer printer;
  printer.SetSingleLineMode(one_line);
  return printer.PrintToString(message, text);
}

bool ParseProtoFromText(std::string text, int32_t max_recursion,
                        ::google::protobuf::Message* message,
                        ::google::protobuf::io::ErrorCollector* error) {
  ::google::protobuf::TextFormat::Parser parser;
  parser.SetRecursionLimit(max_recursion);
  parser.RecordErrorsTo(error);
  return parser.ParseFromString(text, message);
}

bool ParseProtoFromText(std::string text,
                        ::google::protobuf::Message* message) {
  return ParseProtoFromText(
      text, absl::GetFlag(FLAGS_default_max_proto_recursion), message, nullptr);
}
#endif  // MALDOCA_CHROME
}  // namespace utils
}  // namespace maldoca
