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

// Use third_party/protobuf/text_format.h for oss
#include "absl/base/call_once.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#ifndef MALDOCA_CHROME
#include "google/protobuf/text_format.h"
#endif  // MALDOCA_CHROME
#include "libxml/SAX2.h"
#include "libxml/parserInternals.h"

#ifdef MALDOCA_CHROME
const static int32_t default_max_proto_recursion = 400;
#else
ABSL_FLAG(int32_t, default_max_proto_recursion, 400,
          "Default max allowed recursion in proto parsing from text.");
#endif

namespace maldoca {
namespace utils {
namespace {
xmlSAXHandler sax_handler;
absl::once_flag once_init;

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
  if (converter_ != nullptr) {
    iconv_close(converter_);
  }
  internal_converter_ = InternalConverter::kNone;
  // Fixing missing encoding;
  // cp10000 is calld MAC in iconv
  if (strcmp(encode_name, "cp10000") == 0) {
    encode_name = "MAC";
    DLOG(INFO) << "Replaced cp10000 with MAC";
  }
  converter_ = iconv_open("UTF-8", encode_name);
  if (converter_ == reinterpret_cast<iconv_t>(-1)) {
    converter_ = nullptr;
    LOG(ERROR) << "Fail to open iconv for " << encode_name << ": " << errno;
    // Windows encoding, we really want to make sure this works so we'll use our
    // own
    if (strcasecmp(encode_name, "cp1251") == 0) {
      internal_converter_ = InternalConverter::kCp1251;
      DLOG(INFO) << "Use internal cp1251 encoder";
      return true;
    }
    if (strcasecmp(encode_name, "cp1252") == 0) {
      internal_converter_ = InternalConverter::kCp1252;
      DLOG(INFO) << "Use internal cp1252 encoder";
      return true;
    }
    if (strcasecmp(encode_name, "LATIN1") == 0) {
      internal_converter_ = InternalConverter::kLatin1;
      DLOG(INFO) << "Use internal LATIN1 encoder";
      return true;
    }
    return false;
  }
  return true;
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
  // Guess what the output size will be; make it the same to start
  // TODO(somebody): make a better guess here.
  size_t out_bytes_left = in_bytes_left;
  size_t old_output_size = out_str->size();
  out_str->resize(old_output_size + out_bytes_left);
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
          out_str->resize(std::max(out_ptr - out_str->data(), 0l));
          LOG(ERROR) << "failed with error: " << errno
                     << ", in_bytes_left: " << in_bytes_left
                     << ", out_bytes_left: " << out_bytes_left
                     << ", char: " << *(input_ptr - 1);
          return false;
      }
    }
  }
  // resize to actual size
  out_str->resize(std::max(out_ptr - out_str->data(), 0l));
  // For some reason, it preserves start and trailing \0 so remove them
  StripNullChar(out_str);
  *bytes_consumed = input.size() - in_bytes_left;
  *bytes_filled = out_str->size() - old_output_size;
  return *error_char_count <= max_error_;
}

xmlDocPtr XmlParseMemory(const char* buffer, int size) {
  absl::call_once(once_init, &InitSAXHandler);
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
                      bool log_error) {
  auto status_or = file::GetContents(filename);
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
  return ParseProtoFromText(text, default_max_proto_recursion, message,
                            nullptr);
}
#endif  // MALDOCA_CHROME
}  // namespace utils
}  // namespace maldoca
