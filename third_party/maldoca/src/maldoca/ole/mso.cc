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

// Extraction and decompression of ActiveMime/MSO files. See mso.h for details.

#include "maldoca/ole/mso.h"

#include <iomanip>
#include <map>
#include <memory>
#include <string>

#ifndef MALDOCA_IN_CHROMIUM
#include "absl/flags/flag.h"  // nogncheck
#endif
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "libxml/tree.h"
#include "libxml/xpath.h"
#include "maldoca/ole/endian_reader.h"
#include "maldoca/ole/oss_utils.h"
#ifdef MALDOCA_IN_CHROMIUM
#include "third_party/zlib/zlib.h"
#else
#include "zlib/include/zlib.h"
#endif

#ifdef MALDOCA_IN_CHROMIUM
static const int32_t mso_deflated_max_file_size = 2 << 20;
#else
ABSL_FLAG(int32_t, mso_deflated_max_file_size, 2 << 20,
          "The maximum deflated MSO file size (in bytes) we are willing to "
          "process. The Office 2003 XML file format stores VBA code in "
          "OLE2 files using the ActiveMime/MSO format which is zlib "
          "compressed. We don't easily know in advance the size of "
          "the deflated MSO so we use a reasonable limit instead.");
#endif

namespace maldoca {
using utils::XmlCharDeleter;
using utils::XmlDocDeleter;

namespace {
struct ZStreamDeleter {
  void operator()(z_stream *z) { inflateEnd(z); }
};
}  // namespace
// static
bool MSOContent::GetBinDataFromXML(absl::string_view xml,
                                   std::string *mso_filename,
                                   std::string *mso_content) {
  // Parse the document first.
  std::unique_ptr<xmlDoc, XmlDocDeleter> doc(
      utils::XmlParseMemory(xml.data(), xml.size()));
  if (doc == nullptr) {
    DLOG(ERROR) << "Can not parse XML content from document";
    return false;
  }

  // Get the document root.
  xmlNodePtr cur = xmlDocGetRootElement(doc.get());
  if (cur == nullptr) {
    DLOG(ERROR) << "Can not find document root element in document";
    return false;
  }

  // Look for /docSuppData/binData.
  for (auto const &entry : {"docSuppData", "binData"}) {
    cur = cur->xmlChildrenNode;
    while (cur != nullptr) {
      if (!xmlStrcmp(cur->name, reinterpret_cast<const xmlChar *>(entry))) {
        break;
      }
      cur = cur->next;
    }
    if (cur == nullptr) {
      break;
    }
  }
  if (cur == nullptr) {
    DLOG(ERROR) << "Can not find docSuppData/binData in document";
    return false;
  }

  // Extract the ActiveMime/MSO data found in the /docSuppdata/
  // binData: it's base64 encoded and should start with "ActiveMime".
  std::unique_ptr<xmlChar, XmlCharDeleter> data(
      xmlNodeListGetString(doc.get(), cur->children, 0));
  std::string b64_content(reinterpret_cast<const char *>(data.get()));
  if (!absl::Base64Unescape(b64_content, mso_content)) {
    DLOG(ERROR) << "Can note decode docSuppData/binData data in document";
    return false;
  }
  if (mso_content->find("ActiveMime") == std::string::npos) {
    mso_content->clear();
    return false;
  }
  // If we're OK with the content, retrieve its name if we can find
  // one.
  std::unique_ptr<xmlChar, XmlCharDeleter> name_attribute(
      xmlGetProp(cur, reinterpret_cast<const xmlChar *>("name")));
  if (name_attribute != nullptr) {
    mso_filename->assign(reinterpret_cast<const char *>(name_attribute.get()));
  }
  return true;
}

// static
bool MSOContent::GetOLE2Data(absl::string_view mso, std::string *ole2) {
  // We're going to look for things at offset 0x1e. mso needs to be at
  // least that size.
  if (mso.size() < 0x1e) {
    DLOG(ERROR) << "No content at offset 0x1e";
    return false;
  }

  absl::string_view input = absl::ClippedSubstr(mso, 0x1e);

  // Read a first offset marking the start of the zlib compressed OLE2
  // file in the MSO stream.
  uint16_t offset;
  if (!(LittleEndianReader::ConsumeUInt16(&input, &offset))) {
    DLOG(ERROR) << "Bytes [30:31]: can not read uint16";
    return false;
  }

  // We're going to try several offsets: the one we found (+ 46) but
  // also 0x32 and 0x22a for Word and Excel, respectively.  These
  // offsets were determined by reverse engineering and not officially
  // documented anywhere, therefore this could be incomplete or
  // inaccurate. We use a map to avoid performing the same check twice
  // in case offset + 46 happens to be 0x32 or 0x22a or any other
  // value we might use in the future.
  // NOTE: When you add a constant larger than any of the constants
  // used here, adjust BugFixesVerificationTest::NoMemoryLeak accordingly.
  std::map<uint16_t, bool> offsets;
  offsets.insert(std::pair<uint16_t, bool>(offset + 46, true));
  offsets.insert(std::pair<uint16_t, bool>(0x32, true));
  offsets.insert(std::pair<uint16_t, bool>(0x22a, true));

  // We don't know beforehand the size of the output and we can't
  // easily compute with the kind of zlib stream we're going to
  // process, so we arbitrarily set it to a large enough limit - we
  // will declare an error if that limit is insufficient.
#ifdef MALDOCA_IN_CHROMIUM
  uLongf dest_length = mso_deflated_max_file_size;
#else
  uLongf dest_length = absl::GetFlag(FLAGS_mso_deflated_max_file_size);
#endif
  std::unique_ptr<char[]> dest(new char[dest_length]);

  for (auto const &current_offset : offsets) {
    z_stream z;
    memset(&z, 0, sizeof(z));
    std::unique_ptr<z_stream, ZStreamDeleter> stream(&z);

    int result = inflateInit2(stream.get(), MAX_WBITS);
    if (result != Z_OK) {
      DLOG(ERROR) << "could not initialize z_stream: " << result;
      return false;
    }
    if (mso.size() < current_offset.first) {
      DLOG(ERROR) << "Can not extract content from offset " << std::hex
                  << std::setfill('0') << std::setw(4) << current_offset.first;
      continue;
    }
    absl::string_view sub = mso.substr(current_offset.first);
    stream->next_in =
        const_cast<Bytef *>(reinterpret_cast<const Bytef *>(sub.data()));
    stream->avail_in = sub.length();
    stream->next_out = reinterpret_cast<Bytef *>(dest.get());
    stream->avail_out = dest_length;
    result = inflate(stream.get(), Z_FINISH);
    inflateEnd(stream.get());
    if (result != Z_STREAM_END) {
      DLOG(INFO) << "zlib deflate failed for offset " << std::hex
                 << std::setfill('0') << std::setw(4) << current_offset.first
                 << ", with code: " << std::dec << result;
      continue;
    }
    // We have a winner: copy the deflated output into our result
    // string and return success.
    ole2->assign(dest.get(), stream->total_out);
    return true;
  }

  // TODO(somebody): If we have failed at well known offsets, we
  // could also try to brute for our way out of zlib decompression,
  // trying our luck at anything that starts with an 'x' or any other
  // appropriate value.
  return false;
}

}  // namespace maldoca
