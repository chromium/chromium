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

// Reading the appropriate directory to extract VBA code. See vba.h for details.

#include "maldoca/ole/vba.h"

#include <iomanip>
#include <map>
#include <memory>
#include <vector>

#include "absl/container/node_hash_map.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "maldoca/base/digest.h"
#include "maldoca/base/logging.h"
#include "maldoca/ole/dir.h"
#include "maldoca/ole/endian_reader.h"
#include "maldoca/ole/fat.h"
#include "maldoca/ole/oss_utils.h"
#include "maldoca/ole/proto/vba_extraction.pb.h"
#include "maldoca/ole/stream.h"
#include "re2/re2.h"

namespace maldoca {
namespace vba_code {
namespace {
// Mapping between what we find in the code module stream and code
// chunk filename extensions.
const std::vector<std::pair<std::string, std::string>>& NameExtensionMap() {
  static const std::vector<std::pair<std::string, std::string>>*
      name_extension_map = new std::vector<std::pair<std::string, std::string>>(
          {{"Module", "bas"}, {"Class", "cls"}, {"BaseClass", "frm"}});
  return *name_extension_map;
}

LazyRE2 kAttributeRE = {"(\\0Attribut[^e])"};
const char kUtf16le[] = "utf-16le";

std::string GetHipHCompatibleMacroHash(absl::string_view vba_code) {
  // Prior to hashing, strip Attributes and the last line-break to mimic
  // VBProject.VBComponents.Codemodule.Lines
  std::vector<absl::string_view> vba_lines = absl::StrSplit(vba_code, "\r\n");
  if (vba_lines.size() < 2) {
    // this handles documents created on MacOS
    vba_lines = absl::StrSplit(vba_code, '\n');
  }
  int attributes_number = 0;
  for (const auto& line : vba_lines) {
    if (!absl::StartsWith(line, "Attribute ")) {
      break;
    }
    ++attributes_number;
  }
  vba_lines.erase(vba_lines.begin(), vba_lines.begin() + attributes_number);
  if (!vba_lines.empty() && vba_lines.back().empty()) {
    vba_lines.pop_back();
  }
  if (!vba_lines.empty()) {
    return maldoca::Sha1Hash(absl::StrJoin(vba_lines, "\r\n"));
  }
  return "";
}

// Decode input using encoding into output. Return true upon success.
bool DecodeEncodedString(absl::string_view input, const std::string& encoding,
                         std::string* output) {
  int bytes_consumed, bytes_filled, error_char_count;
  bool success = utils::ConvertEncodingBufferToUTF8String(
      input, encoding.c_str(), output, &bytes_consumed, &bytes_filled,
      &error_char_count);
  DLOG(INFO) << "DecodeEncodedString success: " << success
             << ", error_cnt: " << error_char_count;
  return success && error_char_count == 0;
}

// Decompresses VBA code in orphaned/malformed directory entries.
bool DecompressMalformedStream(
    uint32_t offset, absl::string_view vba_code_stream,
    const OLEDirectoryEntry* entry,
    const absl::node_hash_map<std::string, std::string>& code_modules,
    bool include_hash, VBACodeChunks* code_chunks) {
  // Decompress the code stream at the previously found offset.
  std::string vba_code;
  if (offset >= vba_code_stream.size()) {
    DLOG(ERROR) << "Can not read vba_code_stream from offset " << offset;
    return false;
  }
  if (!OLEStream::DecompressStream(
          vba_code_stream.substr(offset, std::string::npos), &vba_code)) {
    DLOG(ERROR) << "Decompressing the VBA code stream at offset " << offset
                << " failed.";
    return false;
  }

  // Build the code chunk file name using the code_modules map to
  // find code chunk file name extension.
  const auto found = code_modules.find(
      absl::AsciiStrToLower(absl::string_view(entry->Name())));
  std::string filename = absl::StrFormat(
      "%s.%s", entry->Name(),
      (found == code_modules.end() ? "bin" : found->second.c_str()));

  // Create, initialize and add a new code chunk to the existing
  // ones.
  VBACodeChunk* chunk = code_chunks->add_chunk();
  chunk->set_code(vba_code);
  chunk->set_path(entry->Path());
  chunk->set_filename(filename);
  chunk->set_extracted_from_malformed_entry(true);

  const std::string sha1 =
      include_hash ? GetHipHCompatibleMacroHash(vba_code) : "";
  if (!sha1.empty()) {
    chunk->set_sha1(sha1);
  }
  chunk->set_sha256_hex_string(Sha256HexString(vba_code));
  return true;
}
}  // namespace
// PROJECTREFERENCE records ID values. We keep name that can easily
// be looked up in the MSDN documentation (not the most readable form.)
enum : uint16_t {
  REFERENCEREGISTERED = 0x000d,
  REFERENCEPROJECT = 0x000e,
  REFERENCENAME = 0x0016,
  REFERENCECONTROL = 0x002f,
  REFERENCEORIGINAL = 0x0033,
};

// PROJETMODULE records ID values. We keep name that can easily be
// looked up in the MSDN documentation (not the most readable form.)
enum : uint16_t {
  MODULESTREAMNAME = 0x001a,
  MODULEDOCSTRING = 0x001c,
  MODULEHELPCONTEXT = 0x001e,
  MODULETYPEPROCEDURAL = 0x0021,
  MODULETYPEOTHER = 0x0022,
  MODULEREADONLY = 0x0025,
  MODULEPRIVATE = 0x0028,
  MODULETERMINATOR = 0x002b,
  MODULECOOKIE = 0x002c,
  MODULEOFFSET = 0x0031,
  MODULENAMEUNICODE = 0x0047,
};

// Vector initialization and macro carefully laid out for
// readability. Do not clang-format.
// clang-format off

// This is a set of macros to read uint16_t and uint32_t values and
// evaluate a condition (possibly against the retrieved values.) The
// goal here is to avoid the tedious repetition of simple code
// patterns. These macros are declared here to discourage their reuse
// elsewhere as they are heavily tuned to be using in the ExtractVBA
// method and making them useful anywhere else would anywhere else
// would complicate their invocation and defeat their original
// purpose.
//
// The values are stored in variables that are either declared on the
// fly by the macro or already declared and simply referred to as
// storage. The macros are expecting a runtime environment featuring
// a StringPiece instance (input).
// We use the C pre-processor stringification operator to produce
// informative log entries. For example:
//
//   READ_UINT16_EXPECT_EQUAL(foobar, 0x123);
//
// declares foobar as a uint16_t, read a value from input and compares
// is to 0x123. If reading the value fails, the following is logged:
//
//   foobar: can not read 2 bytes from input.
//
// And if the evaluation of the condition fails, the following is logged:
//
//   foobar: can not satisfy condition 'foobar == 0x123'
//
// Macros featuring a trailing '_' in their name are not supposed to
// be directly used by the code but rather used to define other
// macros.

// Read a variable of a given type and evaluate a condition. The
// variable should have already been declared. Return false when the
// value can not successfully be read or when the condition evaluates
// to false.
#define READ_TYPE_EXPECT_COND_NO_DECL_(__VARIABLE__,                    \
                                       __CONSUME__, __COND__)           \
  do {                                                                  \
    if (!(LittleEndianReader::__CONSUME__(&input, &__VARIABLE__))) {    \
      DLOG(ERROR) << #__VARIABLE__                                      \
                  << ": can not read "                                  \
                  << sizeof(__VARIABLE__)                               \
                  << " bytes from input";                               \
      return false;                                                     \
    }                                                                   \
    if (!(__COND__)) {                                                  \
      DLOG(ERROR) << #__VARIABLE__                                      \
                  << ": can not satisfy condition '"                    \
                  << #__COND__ << "'";                                  \
      return false;                                                     \
    }                                                                   \
  } while (false)

// Declare a variable of a given type, read a value of that type from
// input and evaluate a condition.
#define READ_TYPE_EXPECT_COND_(__TYPE__, __VARIABLE__,                  \
                               __CONSUME__, __COND__)                   \
  __TYPE__ __VARIABLE__;                                                \
  READ_TYPE_EXPECT_COND_NO_DECL_(__VARIABLE__, __CONSUME__, __COND__)

// Read into a string variable a given amount of characters.
#define READ_STRING_NO_DECL_(__VARIABLE__, __SIZE__)                    \
  do {                                                                  \
    if (!LittleEndianReader::ConsumeString(&input,                      \
                                           __SIZE__, &__VARIABLE__)) {  \
      DLOG(ERROR) << #__VARIABLE__                                      \
                  << ": can not read string of size <= "                \
                  << __SIZE__ << " bytes";                              \
      return false;                                                     \
    }                                                                   \
  } while (false)

// We are now defining macros that can be used directly in the code.
//
// Read a declared uint16_t, the returned value is not verified.
#define READ_EXISTING_UINT16(__VARIABLE__)                              \
  READ_TYPE_EXPECT_COND_NO_DECL_(__VARIABLE__, ConsumeUInt16, true);

// Declare and read a uint16_t, the returned value is not verified.
#define READ_UINT16(__VARIABLE__)                                       \
  READ_TYPE_EXPECT_COND_(uint16_t, __VARIABLE__, ConsumeUInt16, true)

// Declare and read a uint16_t, the returned value is verified to be
// equal to a provided constant.
#define READ_UINT16_EXPECT_EQUAL(__VARIABLE__, __VALUE__)               \
  READ_TYPE_EXPECT_COND_(uint16_t, __VARIABLE__, ConsumeUInt16,         \
                      __VARIABLE__ == __VALUE__)

// Read a declared uint32_t, the returned value is not verified.
#define READ_EXISTING_UINT32(__VARIABLE__)                              \
  READ_TYPE_EXPECT_COND_NO_DECL_(__VARIABLE__, ConsumeUInt32, true);

// Declare and read a uint32_t, the returned value is not verified.
#define READ_UINT32(__VARIABLE__)                                       \
  READ_TYPE_EXPECT_COND_(uint32_t, __VARIABLE__, ConsumeUInt32, true)

// Declare and read a uint32_t, the returned value is verified to be
// equal to a provided constant.
#define READ_UINT32_EXPECT_EQUAL(__VARIABLE__, __VALUE__)               \
  READ_TYPE_EXPECT_COND_(uint32_t, __VARIABLE__, ConsumeUInt32,         \
                         __VARIABLE__ == __VALUE__)

// Declare and read a uint32_t, the returned value is verified to
// be within the specified inclusive range.
#define READ_UINT32_EXPECT_RANGE(__VARIABLE__, __LOW__, __HIGH__)       \
  READ_TYPE_EXPECT_COND_(uint32_t, __VARIABLE__, ConsumeUInt32,         \
                         (__VARIABLE__ >= __LOW__ &&                    \
                          __VARIABLE__ <= __HIGH__))

// Declare and read a uint32_t, evaluate the provided condition. The
// condition may or may not refer to the declared variable.
#define READ_UINT32_EXPECT_COND(__VARIABLE__, __COND__)                 \
  READ_TYPE_EXPECT_COND_(uint32_t, __VARIABLE__, ConsumeUInt32, __COND__)

// Declare and read into a string variable a given amount of characters.
#define READ_STRING(__VARIABLE__, __SIZE__)                             \
  std::string __VARIABLE__;                                             \
  READ_STRING_NO_DECL_(__VARIABLE__, __SIZE__);

// Read into an existing string variable a given amount of characters.
#define READ_EXISTING_STRING(__VARIABLE__, __SIZE__)                    \
  READ_STRING_NO_DECL_(__VARIABLE__, __SIZE__);

// Declare and read into a string variable a given amount of
// characters. The variable is declared within a scope so that it is
// destroyed immediately after having been initialized.
#define READ_STRING_DTOR(__VARIABLE__, __SIZE__)                        \
  { READ_STRING(__VARIABLE__, __SIZE__); }

// Read into an existing string variable a given amount of characters
// and evaluate the provided condition. The condition may or may not
// refer to the declared variable.
#define READ_STRING_COND(__VARIABLE__, __SIZE__, __COND__)              \
  do {                                                                  \
    READ_STRING(__VARIABLE__, __SIZE__);                                \
    if (!(__COND__)) {                                                  \
      DLOG(ERROR) << #__VARIABLE__                                      \
                  << ": can not satisfy condition " << #__COND__;       \
      return false;                                                     \
    }                                                                   \
  } while (false)

// clang-format on

void InsertPathPrefix(const std::string& prefix, VBACodeChunk* chunk) {
  std::string* path = chunk->mutable_path();
  path->insert(0, prefix);
  path->insert(prefix.length(), 1, ':');
}

uint32_t ExtractMalformedOrOrphan(
    absl::string_view input, uint32_t index, const OLEHeader& header,
    const OLEDirectoryEntry& root, absl::string_view directory_stream,
    const absl::node_hash_map<std::string, std::string>& code_modules,
    const std::vector<uint32_t>& fat, bool include_hash,
    OLEDirectoryEntry* entry, VBACodeChunks* code_chunks) {
  OLEDirectoryEntry new_entry;
  if (entry == nullptr) {
    if (!OLEDirectoryEntry::ReadDirectoryEntryFromStream(
            directory_stream, index, header.SectorSize(), &new_entry)) {
      DLOG(ERROR) << "Failed to read sector " << index << " from stream.";
      return false;
    }
    entry = &new_entry;
  }
  if (entry->EntryType() != DirectoryStorageType::Stream) {
    return false;
  }
  std::string vba_code_stream;
  if (!OLEStream::ReadDirectoryContentUsingRoot(input, header, root, *entry,
                                                fat, &vba_code_stream)) {
    return false;
  }

  // TODO(somebody): replace with absl::string_view when RE2 is in absl
  re2::StringPiece result;
  re2::StringPiece code_stream(vba_code_stream);
  uint32_t number_vba_code_chunks = 0;
  while (RE2::FindAndConsume(&code_stream, *kAttributeRE, &result)) {
    const uint32_t offset = result.data() - vba_code_stream.data() - 3;
    DLOG(INFO) << "Found VBA compressed code at offset " << offset << ".";
    if (DecompressMalformedStream(offset, vba_code_stream, entry, code_modules,
                                  include_hash, code_chunks)) {
      number_vba_code_chunks++;
    }
  }
  return number_vba_code_chunks;
}

// TODO(b/74508679): refactoring
bool ExtractVBAInternal(
    absl::string_view main_input_string, const OLEHeader& header,
    const absl::node_hash_map<std::string, std::string>& code_modules,
    const OLEDirectoryEntry& root, const std::vector<uint32_t>& fat,
    absl::string_view dir_input_string, bool include_hash,
    absl::flat_hash_set<uint32_t>* extracted_indices,
    std::vector<OLEDirectoryEntry*>* dir_entries, VBACodeChunks* code_chunks,
    bool continue_extraction) {
  // Input must exist and the code chunk can not have already been
  // filled.
  CHECK(!main_input_string.empty());
  CHECK(!dir_input_string.empty());
  //  Code chunk can not have already been filled unless explicitly continuing
  //  extraction.
  if (!continue_extraction) {
    CHECK_EQ(code_chunks->chunk_size(), 0);
  }

  absl::string_view input = dir_input_string;

  // This implementation is organized is three sections:
  //
  // - First we go through a set of records whose extracted values
  //   have to sometimes match expectations. Not matching expectations
  //   will make the routine end with an error and no VBA code will be
  //   extracted. An important value extracted during that phase is an
  //   encoding information that we will use later to decode the name
  //   of a stream containing the VBA code.
  //
  // - Then we go into a while() loop where more records of different
  //   types are processed and checked against expectations until a
  //   special record is found. Reaching this special record enables
  //   execution of the next section.
  //
  // - After having read a project_count value, we enter a for loop to
  //   iterate over projects that contain the VBA code:
  //
  //   * A project is split into sections: two of them are important:
  //     the one that yields the name of a stream containing the VBA
  //     code and the one that yields a text offset that marks the
  //     beginning of the compressed code in that stream. More
  //     sections are read until a special end section is found, which
  //     allows us to finally move on to extracting the VBA source.
  //
  //   * Code extraction: after having decoded the stream name, we
  //     search in the VBA/ directory found in the VBA root the entry
  //     that bears the decoded stream name and read its content. We
  //     then decompress that content from the previously extracted
  //     text offset to yield the VBA code.

  // PROJECTSYSKIND record
  READ_UINT16_EXPECT_EQUAL(syskind_id, 0x1);
  READ_UINT32_EXPECT_EQUAL(syskind_size, 0x4);
  READ_UINT32_EXPECT_RANGE(syskind_kind, 0x1, 0x3);  // > 0x0 && <= 0x3

  // PROJECTLCID record
  READ_UINT16_EXPECT_EQUAL(lcid_id, 0x2);
  READ_UINT32_EXPECT_EQUAL(lcid_size, 0x4);
  READ_UINT32_EXPECT_EQUAL(lcid_lcid, 0x409);

  // PROJECTLCIDINVOKE record
  READ_UINT16_EXPECT_EQUAL(lcid_invoke_id, 0x14);
  READ_UINT32_EXPECT_EQUAL(lcid_invoke_size, 0x4);
  READ_UINT32_EXPECT_EQUAL(lcid_invoke_lcid_invoke, 0x409);

  // PROJECTCODEPAGE record
  READ_UINT16_EXPECT_EQUAL(codepage_id, 0x3);
  READ_UINT32_EXPECT_EQUAL(codepage_size, 0x2);
  // This is an important value that will be used later.
  READ_UINT16(codepage_codepage);

  // PROJECTNAME record
  READ_UINT16_EXPECT_EQUAL(name_id, 0x4);
  READ_UINT32_EXPECT_COND(name_sizeof_projectname,
                          name_sizeof_projectname <= 128);
  READ_STRING(project_name, name_sizeof_projectname);

  // PROJECTDOCTSTRING record
  READ_UINT16_EXPECT_EQUAL(doctstring_id, 0x5);
  READ_UINT32_EXPECT_COND(docstring_sizeof_docstring,
                          docstring_sizeof_docstring <= 2000);
  READ_STRING(docstring, docstring_sizeof_docstring);
  READ_UINT16_EXPECT_EQUAL(docstring_reserved, 0x40);
  READ_UINT32_EXPECT_COND(docstring_sizeof_unicode_docstring,
                          docstring_sizeof_unicode_docstring % 2 == 0);
  READ_STRING(unicode_docstring, docstring_sizeof_unicode_docstring);

  // PROJECTHELPFILEPATH record
  READ_UINT16_EXPECT_EQUAL(help_filepath_id, 0x6);
  READ_UINT32_EXPECT_COND(filepath_sizeof_filepath_1,
                          filepath_sizeof_filepath_1 <= 260);
  READ_STRING(filepath_1, filepath_sizeof_filepath_1);
  READ_UINT16_EXPECT_EQUAL(help_filepath_reserved, 0x3d);
  READ_UINT32_EXPECT_COND(
      filepath_sizeof_filepath_2,
      filepath_sizeof_filepath_1 == filepath_sizeof_filepath_2);
  READ_STRING_COND(filepath_2, filepath_sizeof_filepath_2,
                   filepath_1 == filepath_2);

  // PROJECTHELPCONTEXT record
  READ_UINT16_EXPECT_EQUAL(help_context_id, 0x7);
  READ_UINT32_EXPECT_EQUAL(help_context_size, 0x4);
  READ_UINT32(help_context);

  // PROJECTLIBFLAGS record
  READ_UINT16_EXPECT_EQUAL(lib_flags_id, 0x8);
  READ_UINT32_EXPECT_EQUAL(lib_flags_size, 0x4);
  READ_UINT32(lib_flags);

  // PROJECTVERSION record
  READ_UINT16_EXPECT_EQUAL(version_id, 0x9);
  READ_UINT32_EXPECT_EQUAL(version_reserved, 0x4);
  READ_UINT32(version_major);
  READ_UINT16(version_minor);

  // PROJECTCONSTANTS record
  READ_UINT16_EXPECT_EQUAL(constants_id, 0xc);
  READ_UINT32_EXPECT_COND(constants_sizeof, constants_sizeof <= 1015);
  READ_STRING(constants, constants_sizeof);
  READ_UINT16_EXPECT_EQUAL(constants_reserved, 0x3c);
  READ_UINT32_EXPECT_COND(constants_sizeof_unicode_constants,
                          constants_sizeof_unicode_constants % 2 == 0);
  READ_STRING(unicode_constants, constants_sizeof_unicode_constants);

  // We're now reaching the point where we can start looking at the
  // arrays of REFERENCE records. We need to go through them to be
  // able to reach projects section.
  bool not_done = true;
  // Keep track of whether we've seen an intermediate error (non fatal
  // but turning the code extraction into a not entirely successful
  // operation)
  bool intermediate_code_chunk_read_error = false;
  while (not_done) {
    READ_UINT16(record_type);
    switch (record_type) {
      case 0x000f:
        not_done = false;
        break;

      case REFERENCENAME:
        READ_UINT32(name_sizeof_name);
        READ_STRING_DTOR(name_name, name_sizeof_name);
        // According to MSDN:
        // "Reserved (2 bytes): MUST be 0x003E. MUST be ignored."
        // https://msdn.microsoft.com/en-us/library/dd920693(v=office.12).aspx
        // It seems that MS Word ignores this value, and macros inside samples
        // with values different than 0x3E are executed normally.
        READ_UINT16(name_reserved);
        READ_UINT32(name_sizeof_unicode_name);
        READ_STRING_DTOR(name_unicode_name, name_sizeof_unicode_name);
        continue;

      case REFERENCEORIGINAL:
        READ_UINT32(original_sizeof_libid_original);
        READ_STRING_DTOR(original_libid_original,
                         original_sizeof_libid_original);
        continue;

      case REFERENCECONTROL:
        READ_UINT32(control_libid_twiddled_ignored);
        READ_UINT32(control_sizeof_libid_twiddled);
        READ_STRING_DTOR(control_libid_twiddled, control_sizeof_libid_twiddled);
        READ_UINT32_EXPECT_EQUAL(control_reserved1, 0x0);
        READ_UINT16_EXPECT_EQUAL(control_reserved2, 0x0);
        READ_UINT16(control_check);

        // Wait, there's optionally more...
        if (control_check == 0x16) {
          READ_UINT32(control_extended_sizeof_name);
          READ_STRING(control_extended_name, control_extended_sizeof_name);
          READ_EXISTING_UINT16(control_check);

          // It's possible to terminate early the REFERENCENAME record.
          if (control_check != 0x30) {
            READ_UINT32(control_extended_sizeof_unicode_name);
            READ_STRING(control_extended_unicode_name,
                        control_extended_sizeof_unicode_name);
            READ_EXISTING_UINT16(control_check);
          }
        }

        if (control_check != 0x30) {
          DLOG(ERROR) << "control_check: unexpected value " << std::hex
                      << std::setfill('0') << std::setw(2) << control_check;
          return false;
        }
        READ_UINT32(control_size_extended);
        READ_UINT32(control_sizeof_libid_extended);
        READ_STRING_DTOR(control_libid_extended, control_sizeof_libid_extended);
        READ_UINT32(control_extended_reserved3);
        READ_UINT16(control_extended_reserved4);
        READ_STRING_DTOR(control_original_typelib, 16);
        READ_UINT32(control_cookie);
        continue;

      case REFERENCEREGISTERED:
        READ_UINT32(registered_size);
        READ_UINT32(registered_sizeof_libid);
        READ_STRING_DTOR(registered_libid, registered_sizeof_libid);
        READ_UINT16_EXPECT_EQUAL(registered_reserved1, 0x0);
        READ_UINT32_EXPECT_EQUAL(registered_reserved2, 0x0);
        continue;

      case REFERENCEPROJECT:
        READ_UINT32(project_size);
        READ_UINT32(project_sizeof_libid_absolute);
        READ_STRING_DTOR(project_libid_absolute, project_sizeof_libid_absolute);
        READ_UINT32(project_sizeof_libid_relative);
        READ_STRING_DTOR(project_libid_relative, project_sizeof_libid_relative);
        READ_UINT32(project_major_version);
        READ_UINT16(project_minor_version);
        continue;

      default:
        DLOG(ERROR) << "Unexpected record_type value " << std::hex
                    << std::setfill('0') << std::setw(2) << record_type;
        return false;
    }

    // This really shouldn't happen.
    if (record_type != 0x000f) {
      DLOG(ERROR) << "Unexpected value for record_type: " << std::hex
                  << std::setfill('0') << std::setw(2) << record_type;
      return false;
    }

    READ_UINT32_EXPECT_EQUAL(size, 0x2);
    READ_UINT16(project_count);
    READ_UINT16_EXPECT_EQUAL(cookie_record_id, 0x13);
    READ_UINT32_EXPECT_EQUAL(cookie_record_size, 0x2);
    READ_UINT16(cookie_record_cookie);

    // Find the VBA/ directory from the VBA root. We will need it
    // later.
    OLEDirectoryEntry* vba_dir =
        root.FindChildByName("vba", DirectoryStorageType::Storage);
    if (vba_dir == nullptr) {
      DLOG(ERROR) << "Can not find VBA/ from the VBA root " << root.Path();
      return false;
    }

    // We are now iterating over projects - a project host a chunk of
    // VBA code and there can be several projects in a given OLE2
    // payload.
    for (uint32_t index = 0; index < project_count; index++) {
      // These are initialized in this loop and need to be accessible
      // from anywhere in this loop.
      std::string stream_name;
      std::string stream_unicode_name;
      std::string module_unicode_name;
      uint32_t text_offset = 0;

      READ_UINT16_EXPECT_EQUAL(project_module, 0x19);
      READ_UINT32(sizeof_module_name);
      // We found the module name which will allow us to find, by
      // looking up the code_modules map, the current code chunk
      // filename extension.
      READ_STRING(module_name, sizeof_module_name);
      READ_UINT16(module_id);

      // This section branches on section IDs and a matching block
      // will compute the next section ID that might be matched right
      // after.
      bool terminator_reached = false;
      if (module_id == MODULENAMEUNICODE) {
        READ_UINT32(sizeof_module_unicode_name);
        READ_EXISTING_STRING(module_unicode_name, sizeof_module_unicode_name);
        READ_EXISTING_UINT16(module_id);
      }

      if (module_id == MODULESTREAMNAME) {
        READ_UINT32(sizeof_stream_name);
        // We found the name of the stream in the VBA/ directory that
        // contains the VBA code for that project section.
        READ_EXISTING_STRING(stream_name, sizeof_stream_name);
        READ_UINT16_EXPECT_EQUAL(reserved, 0x32);
        READ_UINT32(sizeof_stream_unicode_name);
        READ_EXISTING_STRING(stream_unicode_name, sizeof_stream_unicode_name);
        READ_EXISTING_UINT16(module_id);
      }

      if (module_id == MODULEDOCSTRING) {
        READ_UINT32(sizeof_doc_string);
        READ_STRING_DTOR(doct_string, sizeof_doc_string);
        READ_UINT16(reserved);
        READ_UINT32(sizeof_doc_string_unicode);
        READ_STRING_DTOR(doc_string_unicode, sizeof_doc_string_unicode);
        READ_EXISTING_UINT16(module_id);
      }

      if (module_id == MODULEOFFSET) {
        READ_UINT32_EXPECT_EQUAL(size, 0x4);
        // We found the offset that marks the beginning of the
        // compressed VBA code in the project code stream.
        READ_EXISTING_UINT32(text_offset);
        READ_EXISTING_UINT16(module_id);
      }

      if (module_id == MODULEHELPCONTEXT) {
        READ_UINT32_EXPECT_EQUAL(size, 0x4);
        READ_UINT32(help_context);
        READ_EXISTING_UINT16(module_id);
      }

      if (module_id == MODULECOOKIE) {
        READ_UINT32_EXPECT_EQUAL(size, 0x2);
        READ_UINT16(cookie);
        READ_EXISTING_UINT16(module_id);
      }

      if (module_id == MODULETYPEPROCEDURAL || module_id == MODULETYPEOTHER) {
        READ_UINT32(reserved);
        READ_EXISTING_UINT16(module_id);
      }

      if (module_id == MODULEREADONLY) {
        READ_UINT32_EXPECT_EQUAL(reserved, 0x0);
        READ_EXISTING_UINT16(module_id);
      }

      if (module_id == MODULEPRIVATE) {
        READ_UINT32_EXPECT_EQUAL(reserved, 0x0);
        READ_EXISTING_UINT16(module_id);
      }

      if (module_id == MODULETERMINATOR) {
        READ_UINT32_EXPECT_EQUAL(reserved, 0x0);
        terminator_reached = true;
      }

      if (!terminator_reached) {
        DLOG(ERROR) << "terminator_reached is still set to false";
        return false;
      }

      // Now we get to extract VBA code. Any error we encounter now
      // will stop the retrieval of a given code chunk but won't
      // prevent us from reading the next code chunk.

      // Decode the name of the stream containing the VBA code.
      std::string vba_codepage = absl::StrFormat("cp%d", codepage_codepage);
      std::string stream_name_decoded;
      std::string stream_unicode_name_decoded;
      std::string module_unicode_name_decoded;
      if (!DecodeEncodedString(absl::string_view(stream_name), vba_codepage,
                               &stream_name_decoded)) {
        // this should be LOG instead of DLOG so we can see code pages which are
        // not supported by the 'encodings' library and possible update the
        // encodings (like in cl/163171264)
        LOG(ERROR) << "Can not decode stream name with codepage "
                   << vba_codepage;
        intermediate_code_chunk_read_error = true;
        continue;
      }
      if (!DecodeEncodedString(stream_unicode_name, kUtf16le,
                               &stream_unicode_name_decoded)) {
        LOG(INFO) << "Can not decode unicode stream name.";
      }
      if (!DecodeEncodedString(module_unicode_name, kUtf16le,
                               &module_unicode_name_decoded)) {
        LOG(INFO) << "Can not decode module unicde name.";
      }

      // Find the stream in the VBA/ directory.
      OLEDirectoryEntry* code_dir = nullptr;
      std::vector<std::string> names = {stream_name_decoded,
                                        stream_unicode_name_decoded,
                                        module_unicode_name_decoded};
      for (const auto& name : names) {
        code_dir = vba_dir->FindChildByName(absl::AsciiStrToLower(name),
                                            DirectoryStorageType::Stream);
        if (code_dir != nullptr) {
          break;
        }
        DLOG(ERROR) << "Can not find " << name << " from " << vba_dir->Path()
                    << ". Try other name.";
      }
      if (code_dir == nullptr) {
        DLOG(ERROR) << "Can not find " << stream_name_decoded << ", "
                    << stream_unicode_name_decoded << ", or "
                    << module_unicode_name_decoded << " from "
                    << vba_dir->Path();
        intermediate_code_chunk_read_error = true;
        continue;
      }

      // Read the code stream.
      std::string vba_code_stream;
      if (!OLEStream::ReadDirectoryContent(main_input_string, header, *code_dir,
                                           fat, &vba_code_stream)) {
        DLOG(ERROR) << "Can not read compressed VBA code stream in "
                    << code_dir->Path();
        intermediate_code_chunk_read_error = true;
        continue;
      }

      // Decompress the code stream at the previously found offset.
      std::string vba_code;
      if (text_offset >= vba_code_stream.size()) {
        DLOG(ERROR) << "Can not read vba_code_stream from offset "
                    << text_offset;
        intermediate_code_chunk_read_error = true;
        continue;
      }
      if (!OLEStream::DecompressStream(
              vba_code_stream.substr(text_offset, std::string::npos),
              &vba_code)) {
        DLOG(ERROR) << "Decompressing the VBA code stream at offset "
                    << text_offset << " failed.";
        intermediate_code_chunk_read_error = true;
        continue;
      }

      // Build the code chunk file name using the code_modules map to
      // find code chunk file name extension.
      const auto found = code_modules.find(
          absl::AsciiStrToLower(absl::string_view(module_name)));
      std::string filename = absl::StrFormat(
          "%s.%s", module_name,
          (found == code_modules.end() ? "bin" : found->second.c_str()));

      // VBA file name and source code are encoded with the same code page as
      // the vba stream name thus it should be converted to the proper UTF8
      // before storing in the protobuf (protobuf doesn't allow storing invalid
      // UTF8 characters inside the string type, it fails during deserialization
      // of such proto).
      // In case of error inside DecodeEncodedString, we still want to keep the
      // results, as all invalid characters should be changed to the UTF
      // replacement character, so we will not break the serialized proto
      std::string vba_code_utf8;
      if (!DecodeEncodedString(absl::string_view(vba_code), vba_codepage,
                               &vba_code_utf8)) {
        LOG(WARNING) << "Can not decode vba code stream with codepage "
                     << vba_codepage;
      }
      std::string filename_utf8;
      if (!DecodeEncodedString(absl::string_view(filename), vba_codepage,
                               &filename_utf8)) {
        LOG(WARNING) << "Can not decode vba filename with codepage "
                     << vba_codepage;
      }

      // Create, initialize and add a new code chunk to the existing
      // ones.
      VBACodeChunk* chunk = code_chunks->add_chunk();
      chunk->set_code(vba_code_utf8);
      chunk->set_path(code_dir->Path());
      chunk->set_filename(filename_utf8);

      const std::string sha1 =
          include_hash ? GetHipHCompatibleMacroHash(vba_code) : "";
      if (!sha1.empty()) {
        chunk->set_sha1(sha1);
      }
      chunk->set_sha256_hex_string(Sha256HexString(vba_code));
      extracted_indices->insert(code_dir->NodeIndex());
    }
  }
  // Return overall success if we've been able to read all the defined
  // code chunks.
  return !intermediate_code_chunk_read_error;
}

bool ExtractVBA2(
    absl::string_view main_input_string, const OLEHeader& header,
    const absl::node_hash_map<std::string, std::string>& code_modules,
    const OLEDirectoryEntry& root, const std::vector<uint32_t>& fat,
    absl::string_view dir_input_string,
    absl::flat_hash_set<uint32_t>* extracted_indices,
    std::vector<OLEDirectoryEntry*>* dir_entries, VBACodeChunks* code_chunks,
    bool continue_extraction) {
  return ExtractVBAInternal(main_input_string, header, code_modules, root, fat,
                            dir_input_string, false, extracted_indices,
                            dir_entries, code_chunks, continue_extraction);
}

bool ExtractVBAWithHash(
    absl::string_view main_input_string, const OLEHeader& header,
    const absl::node_hash_map<std::string, std::string>& code_modules,
    const OLEDirectoryEntry& root, const std::vector<uint32_t>& fat,
    absl::string_view dir_input_string,
    absl::flat_hash_set<uint32_t>* extracted_indices,
    std::vector<OLEDirectoryEntry*>* dir_entries, VBACodeChunks* code_chunks,
    bool continue_extraction) {
  return ExtractVBAInternal(main_input_string, header, code_modules, root, fat,
                            dir_input_string, true, extracted_indices,
                            dir_entries, code_chunks, continue_extraction);
}

bool ParseCodeModules(
    absl::string_view project_stream,
    absl::node_hash_map<std::string, std::string>* code_modules) {
  CHECK(!project_stream.empty());
  CHECK(code_modules->empty());

  for (absl::string_view part :
       absl::StrSplit(project_stream, '\n', absl::SkipEmpty())) {
    // We're splitting the line at the first equal sign
    if (part.find('=') == std::string::npos) {
      continue;
    }
    part = absl::StripAsciiWhitespace(part);
    std::vector<std::string> name_value =
        absl::StrSplit(part, absl::MaxSplits('=', 1));
    if (name_value.size() != 2) {
      DLOG(ERROR) << "key=value pair expected, got " << part;
      return false;
    }
    absl::AsciiStrToLower(&name_value[1]);
    if (name_value[0] == "Document") {
      // Drop everything there is after the / in the value part.
      std::string sub_value = name_value[1].substr(0, name_value[1].find('/'));
      if (!sub_value.empty()) {
        code_modules->insert(std::make_pair(sub_value, "cls"));
      } else {
        DLOG(WARNING) << "<part1>/<part2> expected in value part, got "
                      << name_value[1];
      }
    } else {
      for (auto const& name_extension : NameExtensionMap()) {
        if (name_value[0] == name_extension.first) {
          code_modules->insert(
              std::make_pair(name_value[1], name_extension.second));
        }
      }
    }
  }
  return !code_modules->empty();
}

}  // namespace vba_code
}  // namespace maldoca
