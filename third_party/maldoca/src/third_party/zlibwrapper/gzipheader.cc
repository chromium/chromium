// Copyright 2002 Google LLC
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
//
// Author: Neal Cardwell
//

#include "zlibwrapper/gzipheader.h"

#include <algorithm>
#include <cstring>

#include "base/compiler_specific.h"
#include "maldoca/base/logging.h"
#ifdef MALDOCA_IN_CHROMIUM
#include "third_party/zlib/zlib.h"  // for Z_DEFAULT_COMPRESSION
#else
#include "zlib/include/zlib.h"  // for Z_DEFAULT_COMPRESSION
#endif

const uint8_t GZipHeader::magic[] = {0x1f, 0x8b};

// ----------------------------------------------------------------------
// GZipHeader::ReadMore()
//    Attempt to parse the beginning of the given buffer as a gzip
//    header. If these bytes do not constitute a complete gzip header,
//    return INCOMPLETE_HEADER. If these bytes do not constitute a
//    *valid* gzip header, return INVALID_HEADER. If we find a
//    complete header, return COMPLETE_HEADER and set the pointer
//    pointed to by header_end to the first byte beyond the gzip header.
// ----------------------------------------------------------------------

GZipHeader::Status GZipHeader::ReadMore(const char* inbuf, int inbuf_len,
                                        const char** header_end) {
  CHECK(inbuf_len >= 0);
  const uint8_t* pos = reinterpret_cast<const uint8_t*>(inbuf);
  const uint8_t* const end = pos + inbuf_len;

  while (pos < end) {
    switch (state_) {
      case IN_HEADER_ID1:
        if (*pos != magic[0]) return INVALID_HEADER;
        pos++;
        state_++;
        break;
      case IN_HEADER_ID2:
        if (*pos != magic[1]) return INVALID_HEADER;
        pos++;
        state_++;
        break;
      case IN_HEADER_CM:
        if (*pos != Z_DEFLATED) return INVALID_HEADER;
        pos++;
        state_++;
        break;
      case IN_HEADER_FLG:
        flags_ =
            (*pos) & (FLAG_FHCRC | FLAG_FEXTRA | FLAG_FNAME | FLAG_FCOMMENT);
        pos++;
        state_++;
        break;

      case IN_HEADER_MTIME_BYTE_0:
        pos++;
        state_++;
        break;
      case IN_HEADER_MTIME_BYTE_1:
        pos++;
        state_++;
        break;
      case IN_HEADER_MTIME_BYTE_2:
        pos++;
        state_++;
        break;
      case IN_HEADER_MTIME_BYTE_3:
        pos++;
        state_++;
        break;

      case IN_HEADER_XFL:
        pos++;
        state_++;
        break;

      case IN_HEADER_OS:
        pos++;
        state_++;
        break;

      case IN_XLEN_BYTE_0:
        if (!(flags_ & FLAG_FEXTRA)) {
          state_ = IN_FNAME;
          break;
        }
        // We have a two-byte little-endian length, followed by a
        // field of that length.
        extra_length_ = *pos;
        pos++;
        state_++;
        break;
      case IN_XLEN_BYTE_1:
        extra_length_ += *pos << 8;
        pos++;
        state_++;
        // If we have a zero-length FEXTRA, we want to check to notice that
        // we're done reading the FEXTRA before we exit this loop...
        [[fallthrough]];

      case IN_FEXTRA: {
        // Grab the rest of the bytes in the extra field, or as many
        // of them as are actually present so far.
        const int num_extra_bytes = std::min<int>(extra_length_, (end - pos));
        pos += num_extra_bytes;
        extra_length_ -= num_extra_bytes;
        if (extra_length_ == 0) {
          state_ = IN_FNAME;  // advance when we've seen extra_length_ bytes
          flags_ &= ~FLAG_FEXTRA;  // we're done with the FEXTRA stuff
        }
        break;
      }

      case IN_FNAME:
        if (!(flags_ & FLAG_FNAME)) {
          state_ = IN_FCOMMENT;
          break;
        }
        // See if we can find the end of the \0-terminated FNAME field.
        pos = reinterpret_cast<const uint8_t*>(memchr(pos, '\0', (end - pos)));
        if (pos != nullptr) {
          pos++;                  // advance past the '\0'
          flags_ &= ~FLAG_FNAME;  // we're done with the FNAME stuff
          state_ = IN_FCOMMENT;
        } else {
          pos = end;  // everything we have so far is part of the FNAME
        }
        break;

      case IN_FCOMMENT:
        if (!(flags_ & FLAG_FCOMMENT)) {
          state_ = IN_FHCRC_BYTE_0;
          break;
        }
        // See if we can find the end of the \0-terminated FCOMMENT field.
        pos = reinterpret_cast<const uint8_t*>(memchr(pos, '\0', (end - pos)));
        if (pos != nullptr) {
          pos++;                     // advance past the '\0'
          flags_ &= ~FLAG_FCOMMENT;  // we're done with the FCOMMENT stuff
          state_ = IN_FHCRC_BYTE_0;
        } else {
          pos = end;  // everything we have so far is part of the FNAME
        }
        break;

      case IN_FHCRC_BYTE_0:
        if (!(flags_ & FLAG_FHCRC)) {
          state_ = IN_DONE;
          break;
        }
        pos++;
        state_++;
        break;

      case IN_FHCRC_BYTE_1:
        pos++;
        flags_ &= ~FLAG_FHCRC;  // we're done with the FHCRC stuff
        state_++;
        break;

      case IN_DONE:
        *header_end = reinterpret_cast<const char*>(pos);
        return COMPLETE_HEADER;
    }
  }

  if ((state_ > IN_HEADER_OS) && (flags_ == 0)) {
    *header_end = reinterpret_cast<const char*>(pos);
    return COMPLETE_HEADER;
  } else {
    return INCOMPLETE_HEADER;
  }
}
