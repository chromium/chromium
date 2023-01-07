#ifndef ZLIBWRAPPER_ZLIBWRAPPER_H_
#define ZLIBWRAPPER_ZLIBWRAPPER_H_

/*
 * Copyright 1999 Google LLC
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

#include "third_party/zlib/zlib.h"

class GZipHeader;

class ZLib {
 public:
  ZLib();
  ~ZLib();

  // Wipe a ZLib object to a virgin state.  This differs from Reset()
  // in that it also breaks any dictionary, gzip, etc, state.
  void Reinit();

  // Call this to make a zlib buffer as good as new.  Here's the only
  // case where they differ:
  //    CompressChunk(a); CompressChunk(b); CompressChunkDone();   vs
  //    CompressChunk(a); Reset(); CompressChunk(b); CompressChunkDone();
  // You'll want to use Reset(), then, when you interrupt a compress
  // (or uncompress) in the middle of a chunk and want to start over.
  void Reset();

  // Uncompress data one chunk at a time -- ie you can call this
  // more than once.  To get this to work you need to call per-chunk
  // and "done" routines.
  //
  // Returns Z_OK if success, Z_MEM_ERROR if there was not
  // enough memory, Z_BUF_ERROR if there was not enough room in the
  // output buffer.
  int UncompressChunk(Bytef* dest, uLongf* destLen, const Bytef* source,
                      uLong sourceLen);

 private:
  int InflateInit();  // sets up the zlib inflate structure

  // Init the zlib data structures for uncompressing
  int UncompressInit(Bytef* dest, uLongf* destLen, const Bytef* source,
                     uLong* sourceLen);

  // Initialization method to be called if we hit an error while
  // uncompressing. On hitting an error, call this method before
  // returning the error.
  void UncompressErrorInit();

  int UncompressChunkOrAll(Bytef* dest, uLongf* destLen, const Bytef* source,
                           uLong sourceLen, int flush_mode);

  // Helper function for UncompressChunk
  int UncompressAtMostOrAll(Bytef* dest, uLongf* destLen, const Bytef* source,
                            uLong* sourceLen, int flush_mode);

  struct Settings {
    // null if we don't want an initial dictionary
    Bytef* dictionary_;  // NOLINT

    // initial dictionary length
    unsigned int dict_len_;  // NOLINT

    // compression level
    int compression_level_;  // NOLINT

    // log base 2 of the window size used in compression
    int window_bits_;  // NOLINT

    // specifies the amount of memory to be used by compressor (1-9)
    int mem_level_;  // NOLINT

    // true if we want/expect no zlib headers
    bool no_header_mode_;  // NOLINT

    // true if we want/expect gzip headers
    bool gzip_header_mode_;  // NOLINT

    // Controls behavior of UncompressAtMostOrAll with regards to returning
    // Z_STREAM_END. See comments for SetDontHideStreamEnd.
    bool dont_hide_zstream_end_;  // NOLINT
  };

  // We allow all kinds of bad footers when this flag is true.
  // Some web servers send bad pages corresponding to these cases
  // and IE is tolerant with it.
  // - Extra bytes after gzip footer (see bug 69126)
  // - No gzip footer (see bug 72896)
  // - Incomplete gzip footer (see bug 71871706)
  static bool should_be_flexible_with_gzip_footer_;

  // "Current" settings. These will be used whenever we next configure zlib.
  // For example changing compression level or header mode will be recorded
  // in these, but don't usually get applied immediately but on next compress.
  Settings settings_;

  // Settings last used to initialise and configure zlib. These are needed
  // to know if the current desired configuration in settings_ is sufficiently
  // compatible with the previous configuration and we can just reconfigure the
  // underlying zlib objects, or have to recreate them from scratch.
  Settings init_settings_;

  z_stream uncomp_stream_;  // Zlib stream data structure
  bool uncomp_init_;        // True if we have initialized uncomp_stream_

  // These are used only in gzip compression mode
  uLong crc_;  // stored in gzip footer, fitting 4 bytes
  uLong uncompressed_size_;

  GZipHeader* gzip_header_;  // our gzip header state

  Byte gzip_footer_[8];    // stored footer, used to uncompress
  int gzip_footer_bytes_;  // num of footer bytes read so far, or -1

  // These are used only with chunked compression.
  bool first_chunk_;  // true if we need to emit headers with this chunk
};

#endif  // FIREBASE_APP_CLIENT_CPP_REST_ZLIBWRAPPER_H_
