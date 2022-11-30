/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Utility to help open source. objects/functions found here will need to have
// open source equivalent or re-implemented.

#ifndef MALDOCA_OLE_OSS_UTILS_H_
#define MALDOCA_OLE_OSS_UTILS_H_

#if !defined(_WIN32)
#include <iconv.h>
#endif  // _WIN32

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#ifndef MALDOCA_IN_CHROMIUM
#include "google/protobuf/io/tokenizer.h"  // nogncheck
#endif
#include "libxml/tree.h"
#include "maldoca/base/file.h"
#include "maldoca/base/logging.h"

namespace maldoca {
namespace utils {

// Custom libXML deleters so that we can wrap xml object or z_stream object
// pointers to std::unique_ptr.
struct XmlCharDeleter {
  void operator()(xmlChar* data) { xmlFree(data); }
};
struct XmlDocDeleter {
  void operator()(xmlDocPtr doc) { xmlFreeDoc(doc); }
};

// Converts an `xmlChar*` object to a string_view.
absl::string_view XmlCharPointerToString(const xmlChar* word);

inline bool Int64To32LosslessConvert(int64_t in_val, int32_t* out) {
  int32_t potential_out = in_val;
  if (potential_out == in_val) {
    *out = potential_out;
    return true;
  }
  return false;
}

// Class to convert an encoded string_view with a specified encoding into
// UTF-8 encided output.
class BufferToUtf8 {
 public:
  explicit BufferToUtf8(const char* encode_name) { Init(encode_name); }
  virtual ~BufferToUtf8() {
#if !defined(_WIN32)
    if (converter_ != nullptr) {
      iconv_close(converter_);
    }
#endif  // _WIN32
  }
  BufferToUtf8& operator=(const BufferToUtf8&) = delete;
  BufferToUtf8(const BufferToUtf8&) = delete;
  // Initialize encoder.
  virtual bool Init(const char* encode_name);
  // Check if the encoder is valid.
  virtual bool IsValid() {
#if defined(_WIN32)
    return init_success_;
#else
    return converter_ != nullptr ||
           internal_converter_ != InternalConverter::kNone;
#endif  // _WIN32
  }
  // Max number of character error before giving up while converting input
  // to output.
  void SetMaxError(int max_error) { max_error_ = max_error; }
  virtual bool ConvertEncodingBufferToUTF8String(absl::string_view input,
                                                 std::string* out_str,
                                                 int* bytes_consumed,
                                                 int* bytes_filled,
                                                 int* error_char_count);

 protected:
  enum class InternalConverter { kNone = 0, kLatin1, kCp1251, kCp1252 };

  // Conversion for window cp1251, likely needed for Office so we make sure it's
  // present (not dependent on library linked).
  bool ConvertCp1251BufferToUTF8String(absl::string_view input,
                                       std::string* out_str,
                                       int* bytes_consumed, int* bytes_filled,
                                       int* error_char_count);
  // Conversion for window cp1252, likely needed for Office so we make sure it's
  // present (not dependent on library linked).
  bool ConvertCp1252BufferToUTF8String(absl::string_view input,
                                       std::string* out_str,
                                       int* bytes_consumed, int* bytes_filled,
                                       int* error_char_count);
  // Provides a minimum converter for latin1
  bool ConvertLatin1BufferToUTF8String(absl::string_view input,
                                       std::string* out_str,
                                       int* bytes_consumed, int* bytes_filled,
                                       int* error_char_count);
#if defined(_WIN32)
  int code_page_ = 0;
  bool init_success_ = false;
#else
  iconv_t converter_ = nullptr;
#endif  // _WIN32
  int max_error_ = 0;  // defaults to give up on any error.
  InternalConverter internal_converter_ = InternalConverter::kNone;
};

inline bool ConvertEncodingBufferToUTF8String(
    absl::string_view input, const char* encoding_name, std::string* out_str,
    int* bytes_consumed, int* bytes_filled, int* error_char_count) {
  BufferToUtf8 converter(encoding_name);
  if (converter.IsValid()) {
    return converter.ConvertEncodingBufferToUTF8String(
        input, out_str, bytes_consumed, bytes_filled, error_char_count);
  } else {
    DLOG(INFO) << "Converter not valid";
  }
  return false;
}

// Parse an XML document in a multithreading safe way.
xmlDocPtr XmlParseMemory(const char* buffer, int size);

// Compute floor/ceiling of log2 of an uint32_t.
int log2Floor(uint32_t n);
int Log2Ceiling(uint32_t n);

// Read the content of filename into content. If log_error is true, errors
// are logged. xor_decode_file determines whether the file's content will be
// xor-decoded or not. Return true upon success. This function MUST be able
// to run before InitGoogle.
bool ReadFileToString(absl::string_view filename, std::string* content,
                      bool log_error, bool xor_decode_file = false);

#ifndef MALDOCA_CHROME
// Simple collector just concat error into a string
class StringErrorCollector : public ::google::protobuf::io::ErrorCollector {
 public:
  StringErrorCollector() = default;
  ~StringErrorCollector() override = default;

  // Indicates that there was an error in the input at the given line and
  // column numbers.  The numbers are zero-based, so you may want to add
  // 1 to each before printing them.
  void AddError(int line, ::google::protobuf::io::ColumnNumber column,
                const std::string& message) override;

  // Indicates that there was a warning in the input at the given line and
  // column numbers.  The numbers are zero-based, so you may want to add
  // 1 to each before printing them.
  void AddWarning(int line, ::google::protobuf::io::ColumnNumber column,
                  const std::string& message) override;

  inline const std::string& Message() const { return message_; }

 private:
  std::string message_;
};

// Print proto to string, optionally as one line. Return true if successful.
bool ProtoToText(const ::google::protobuf::Message& message, bool one_line,
                 std::string* text);
// Parse string version of a proto back into message with max_recursion set.
// Set error to nullptr if don't want to catch parsing errors.
bool ParseProtoFromText(std::string text, int32_t max_recursion,
                        ::google::protobuf::Message* message,
                        ::google::protobuf::io::ErrorCollector* error);
// Use default max_recursion and no error collector.
bool ParseProtoFromText(std::string text, ::google::protobuf::Message* message);
#endif  // MALDOCA_CHROME
}  // namespace utils
}  // namespace maldoca

#endif  // MALDOCA_OLE_OSS_UTILS_H_
