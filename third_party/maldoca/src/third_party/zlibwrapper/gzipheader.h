#ifndef ZLIBWRAPPER_GZIPHEADER_H_
#define ZLIBWRAPPER_GZIPHEADER_H_

//
// Copyright 2000 Google LLC
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
// The GZipHeader class allows you to parse a gzip header, such as you
// might find at the beginning of a file compressed by gzip (ie, a .gz
// file), or at the beginning of an HTTP response that uses a gzip
// Content-Encoding. See RFC 1952 for the specification for the gzip
// header.
//
// The model is that you call ReadMore() for each chunk of bytes
// you've read from a file or socket.
//

#include <stdint.h>

class GZipHeader {
 public:
  GZipHeader() { Reset(); }
  ~GZipHeader() {}

  // Wipe the slate clean and start from scratch.
  void Reset() {
    state_ = IN_HEADER_ID1;
    flags_ = 0;
    extra_length_ = 0;
  }

  enum Status {
    INCOMPLETE_HEADER,  // don't have all the bits yet...
    COMPLETE_HEADER,    // complete, valid header
    INVALID_HEADER,     // found something invalid in the header
  };

  // Attempt to parse the given buffer as the next installment of
  // bytes from a gzip header. If the bytes we've seen so far do not
  // yet constitute a complete gzip header, return
  // INCOMPLETE_HEADER. If these bytes do not constitute a *valid*
  // gzip header, return INVALID_HEADER. When we've seen a complete
  // gzip header, return COMPLETE_HEADER and set the pointer pointed
  // to by header_end to the first byte beyond the gzip header.
  Status ReadMore(const char* inbuf, int inbuf_len, const char** header_end);

 private:
  // NOLINTNEXTLINE
  static const uint8_t magic[];  // gzip magic header

  enum {                   // flags (see RFC)
    FLAG_FTEXT = 0x01,     // bit 0 set: file probably ascii text
    FLAG_FHCRC = 0x02,     // bit 1 set: header CRC present
    FLAG_FEXTRA = 0x04,    // bit 2 set: extra field present
    FLAG_FNAME = 0x08,     // bit 3 set: original file name present
    FLAG_FCOMMENT = 0x10,  // bit 4 set: file comment present
    FLAG_RESERVED = 0xE0,  // bits 5..7: reserved
  };

  enum State {
    // The first 10 bytes are the fixed-size header:
    IN_HEADER_ID1,
    IN_HEADER_ID2,
    IN_HEADER_CM,
    IN_HEADER_FLG,
    IN_HEADER_MTIME_BYTE_0,
    IN_HEADER_MTIME_BYTE_1,
    IN_HEADER_MTIME_BYTE_2,
    IN_HEADER_MTIME_BYTE_3,
    IN_HEADER_XFL,
    IN_HEADER_OS,

    IN_XLEN_BYTE_0,
    IN_XLEN_BYTE_1,
    IN_FEXTRA,

    IN_FNAME,

    IN_FCOMMENT,

    IN_FHCRC_BYTE_0,
    IN_FHCRC_BYTE_1,

    IN_DONE,
  };

  int state_;      // our current State in the parsing FSM: an int so we can ++
  uint8_t flags_;  // the flags byte of the header ("FLG" in the RFC)
  uint16_t extra_length_;  // how much of the "extra field" we have yet to read
};

#endif  // FIREBASE_APP_CLIENT_CPP_REST_GZIPHEADER_H_
