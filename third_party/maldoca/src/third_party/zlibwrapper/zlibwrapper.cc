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

#include "zlibwrapper/zlibwrapper.h"

#include <assert.h>

#include <cstring>
#include <string>

#include "maldoca/base/logging.h"
#include "zlibwrapper/gzipheader.h"

// The GZIP header (see RFC 1952):
//   +---+---+---+---+---+---+---+---+---+---+
//   |ID1|ID2|CM |FLG|     MTIME     |XFL|OS |
//   +---+---+---+---+---+---+---+---+---+---+
//     ID1     \037
//     ID2     \213
//     CM      \010 (compression method == DEFLATE)
//     FLG     \000 (special flags that we do not support)
//     MTIME   Unix format modification time (0 means not available)
//     XFL     2-4? DEFLATE flags
//     OS      ???? Operating system indicator (255 means unknown)

// Header value we generate:
// We use a #define so sizeof() works correctly
#define GZIP_HEADER "\037\213\010\000\000\000\000\000\002\377"

// We allow all kinds of bad footers when this flag is true.
// Some web servers send bad pages corresponding to these cases
// and IE is tolerant with it.
// - Extra bytes after gzip footer (see bug 69126)
// - No gzip footer (see bug 72896)
// - Incomplete gzip footer (see bug 71871706)
bool ZLib::should_be_flexible_with_gzip_footer_ = false;

// Initialize the ZLib class
ZLib::ZLib() : uncomp_init_(false), gzip_header_(new GZipHeader) {
  Reinit();
  init_settings_ = settings_;
}

ZLib::~ZLib() {
  if (uncomp_init_) {
    inflateEnd(&uncomp_stream_);
  }
  delete gzip_header_;
}

void ZLib::Reinit() {
  settings_.dictionary_ = nullptr;
  settings_.dict_len_ = 0;
  settings_.compression_level_ = Z_DEFAULT_COMPRESSION;
  settings_.window_bits_ = MAX_WBITS;
  settings_.mem_level_ = 8;  // DEF_MEM_LEVEL
  settings_.no_header_mode_ = false;
  settings_.gzip_header_mode_ = false;
  settings_.dont_hide_zstream_end_ = false;

  if (uncomp_init_) {
    // Use negative window bits size to indicate bare stream with no header.
    int wbits = (settings_.no_header_mode_ ? -MAX_WBITS : MAX_WBITS);
    int err = inflateReset2(&uncomp_stream_, wbits);
    if (err == Z_OK) {
      init_settings_.no_header_mode_ = settings_.no_header_mode_;
    } else {
      inflateEnd(&uncomp_stream_);
      uncomp_init_ = false;
    }
  }
  crc_ = 0;
  uncompressed_size_ = 0;
  gzip_header_->Reset();
  gzip_footer_bytes_ = -1;
  first_chunk_ = true;
}

void ZLib::Reset() {
  first_chunk_ = true;
  gzip_header_->Reset();
}

// --------- UNCOMPRESS MODE

int ZLib::InflateInit() {
  // Use negative window bits size to indicate bare stream with no header.
  int wbits = (settings_.no_header_mode_ ? -MAX_WBITS : MAX_WBITS);
  int err = inflateInit2(&uncomp_stream_, wbits);
  if (err == Z_OK) {
    init_settings_.no_header_mode_ = settings_.no_header_mode_;
  }
  return err;
}

// Initialization method to be called if we hit an error while
// uncompressing. On hitting an error, call this method before
// returning the error.
void ZLib::UncompressErrorInit() {
  if (uncomp_init_) {
    inflateEnd(&uncomp_stream_);
    uncomp_init_ = false;
  }
  Reset();
}

int ZLib::UncompressInit(Bytef* dest, uLongf* destLen, const Bytef* source,
                         uLong* sourceLen) {
  int err;

  uncomp_stream_.next_in = (Bytef*)source;  // NOLINT
  uncomp_stream_.avail_in = (uInt)*sourceLen;
  // Check for sourceLen (unsigned long) to fit into avail_in (unsigned int).
  if ((uLong)uncomp_stream_.avail_in != *sourceLen) return Z_BUF_ERROR;

  uncomp_stream_.next_out = dest;
  uncomp_stream_.avail_out = (uInt)*destLen;
  // Check for destLen (unsigned long) to fit into avail_out (unsigned int).
  if ((uLong)uncomp_stream_.avail_out != *destLen) return Z_BUF_ERROR;

  if (!first_chunk_)  // only need to set up stream the first time through
    return Z_OK;

  // Force full reinit if properties have changed in a way we can't adjust.
  if (uncomp_init_ && (init_settings_.dictionary_ != settings_.dictionary_ ||
                       init_settings_.dict_len_ != settings_.dict_len_)) {
    inflateEnd(&uncomp_stream_);
    uncomp_init_ = false;
  }

  // Reuse if we've already initted the object.
  if (uncomp_init_) {
    // Use negative window bits size to indicate bare stream with no header.
    int wbits = (settings_.no_header_mode_ ? -MAX_WBITS : MAX_WBITS);
    err = inflateReset2(&uncomp_stream_, wbits);
    if (err == Z_OK) {
      init_settings_.no_header_mode_ = settings_.no_header_mode_;
    } else {
      UncompressErrorInit();
    }
  }

  // First use or previous state was not reusable with current settings.
  if (!uncomp_init_) {
    uncomp_stream_.zalloc = (alloc_func)0;
    uncomp_stream_.zfree = (free_func)0;
    uncomp_stream_.opaque = (voidpf)0;
    err = InflateInit();
    if (err != Z_OK) return err;
    uncomp_init_ = true;
  }
  return Z_OK;
}

// If you compressed your data a chunk at a time, with CompressChunk,
// you can uncompress it a chunk at a time with UncompressChunk.
// Only difference bewteen chunked and unchunked uncompression
// is the flush mode we use: Z_SYNC_FLUSH (chunked) or Z_FINISH (unchunked).
int ZLib::UncompressAtMostOrAll(Bytef* dest, uLongf* destLen,
                                const Bytef* source, uLong* sourceLen,
                                int flush_mode) {  // Z_SYNC_FLUSH or Z_FINISH
  int err = Z_OK;

  if (first_chunk_) {
    gzip_footer_bytes_ = -1;
    if (settings_.gzip_header_mode_) {
      // If we haven't read our first chunk of actual compressed data,
      // and we're expecting gzip headers, then parse some more bytes
      // from the gzip headers.
      const Bytef* bodyBegin = nullptr;
      GZipHeader::Status status = gzip_header_->ReadMore(
          reinterpret_cast<const char*>(source), *sourceLen,
          reinterpret_cast<const char**>(&bodyBegin));
      switch (status) {
        case GZipHeader::INCOMPLETE_HEADER:  // don't have the complete header
          *destLen = 0;
          return Z_OK;
        case GZipHeader::INVALID_HEADER:  // bogus header
          Reset();
          return Z_DATA_ERROR;
        case GZipHeader::COMPLETE_HEADER:      // we have the full header
          *sourceLen -= (bodyBegin - source);  // skip past header bytes
          source = bodyBegin;
          crc_ = crc32(0, nullptr, 0);  // initialize CRC
          break;
        default:
          LOG(ERROR) << "Unexpected gzip header parsing result: " << status;
      }
    }
  } else if (gzip_footer_bytes_ >= 0) {
    // We're now just reading the gzip footer. We already read all the data.
    if (gzip_footer_bytes_ + *sourceLen > sizeof(gzip_footer_) &&
        // When this flag is true, we allow some extra bytes after the
        // gzip footer.
        !should_be_flexible_with_gzip_footer_) {
      Reset();
      return Z_DATA_ERROR;
    }
    uLong len = sizeof(gzip_footer_) - gzip_footer_bytes_;
    if (len > *sourceLen) len = *sourceLen;
    if (len > 0) {
      memcpy(gzip_footer_ + gzip_footer_bytes_, source, len);
      gzip_footer_bytes_ += len;
    }
    *destLen = 0;
    return Z_OK;
  }

  if ((err = UncompressInit(dest, destLen, source, sourceLen)) != Z_OK) {
    LOG(WARNING) << "ZLib: UncompressInit: Error: " << err
                 << " SourceLen: " << *sourceLen;
    return err;
  }

  // This is used to figure out how many output bytes we wrote *this chunk*:
  const uLong old_total_out = uncomp_stream_.total_out;

  // This is used to figure out how many input bytes we read *this chunk*:
  const uLong old_total_in = uncomp_stream_.total_in;

  // Some setup happens only for the first chunk we compress in a run
  if (first_chunk_) {
    // Initialize the dictionary just before we start compressing
    if (settings_.gzip_header_mode_ || settings_.no_header_mode_) {
      // In no_header_mode, we can just set the dictionary, since no
      // checking is done to advance past header bits to get us in the
      // dictionary setting mode. In settings_.gzip_header_mode_ we've already
      // removed headers, so this code works too.
      if (settings_.dictionary_) {
        err = inflateSetDictionary(&uncomp_stream_, settings_.dictionary_,
                                   settings_.dict_len_);
        if (err != Z_OK) {
          LOG(WARNING) << "inflateSetDictionary: Error: " << err
                       << " dict_len: " << settings_.dict_len_;
          UncompressErrorInit();
          return err;
        }
        init_settings_.dictionary_ = settings_.dictionary_;
        init_settings_.dict_len_ = settings_.dict_len_;
      }
    }

    first_chunk_ = false;  // so we don't do this again

    // For the first chunk *only* (to avoid infinite troubles), we let
    // there be no actual data to uncompress.  This sometimes triggers
    // when the input is only the gzip header, say.
    if (*sourceLen == 0) {
      *destLen = 0;
      return Z_OK;
    }
  }

  // We'll uncompress as much as we can.  If we end OK great, otherwise
  // if we get an error that seems to be the gzip footer, we store the
  // gzip footer and return OK, otherwise we return the error.

  // flush_mode is Z_SYNC_FLUSH for chunked mode, Z_FINISH for all mode.
  err = inflate(&uncomp_stream_, flush_mode);
  if (settings_.dictionary_ && err == Z_NEED_DICT) {
    err = inflateSetDictionary(&uncomp_stream_, settings_.dictionary_,
                               settings_.dict_len_);
    if (err != Z_OK) {
      LOG(WARNING) << "UncompressChunkOrAll: failed in inflateSetDictionary: "
                   << err;
      UncompressErrorInit();
      return err;
    }
    init_settings_.dictionary_ = settings_.dictionary_;
    init_settings_.dict_len_ = settings_.dict_len_;
    err = inflate(&uncomp_stream_, flush_mode);
  }

  // Figure out how many bytes of the input zlib slurped up:
  const uLong bytes_read = uncomp_stream_.total_in - old_total_in;
  CHECK(source + bytes_read <= source + *sourceLen);
  *sourceLen = uncomp_stream_.avail_in;

  // Next we look at the footer, if any. Note that we might currently
  // have just part of the footer (eg, if this data is arriving over a
  // socket). After looking for a footer, log a warning if there is
  // extra cruft.
  if ((err == Z_STREAM_END) &&
      ((gzip_footer_bytes_ == -1) ||
       (gzip_footer_bytes_ < sizeof(gzip_footer_))) &&
      (uncomp_stream_.avail_in <= sizeof(gzip_footer_) ||
       // When this flag is true, we allow some extra bytes after the
       // zlib footer.
       should_be_flexible_with_gzip_footer_)) {
    // Due to a bug in old versions of zlibwrapper, we appended the gzip
    // footer even in non-gzip mode.  Thus we always allow a gzip footer
    // even if we're not in gzip mode, so we can continue to uncompress
    // the old data.  :-(

    // Store gzip footer bytes so we can check for footer consistency
    // in UncompressChunkDone(). (If we have the whole footer, we
    // could do the checking here, but we don't to keep consistency
    // with CompressChunkDone().)
    gzip_footer_bytes_ = std::min(static_cast<size_t>(uncomp_stream_.avail_in),
                                  sizeof(gzip_footer_));
    memcpy(gzip_footer_, source + bytes_read, gzip_footer_bytes_);
  } else if ((err == Z_STREAM_END || err == Z_OK)  // everything went ok
             && uncomp_stream_.avail_in == 0) {    // and we read it all
    ;
  } else if (err == Z_STREAM_END && uncomp_stream_.avail_in > 0) {
    UncompressErrorInit();
    return Z_DATA_ERROR;  // what's the extra data for?
  } else if (err != Z_OK && err != Z_STREAM_END && err != Z_BUF_ERROR) {
    // an error happened
    UncompressErrorInit();
    return err;
  } else if (uncomp_stream_.avail_out == 0) {
    err = Z_BUF_ERROR;
  }

  assert(err == Z_OK || err == Z_BUF_ERROR || err == Z_STREAM_END);
  if (err == Z_STREAM_END && !settings_.dont_hide_zstream_end_) err = Z_OK;

  // update the crc and other metadata
  uncompressed_size_ = uncomp_stream_.total_out;
  *destLen = uncomp_stream_.total_out - old_total_out;  // size for this call
  if (settings_.gzip_header_mode_) crc_ = crc32(crc_, dest, *destLen);

  return err;
}

int ZLib::UncompressChunkOrAll(Bytef* dest, uLongf* destLen,
                               const Bytef* source, uLong sourceLen,
                               int flush_mode) {  // Z_SYNC_FLUSH or Z_FINISH
  const int ret =
      UncompressAtMostOrAll(dest, destLen, source, &sourceLen, flush_mode);
  if (ret == Z_BUF_ERROR) UncompressErrorInit();
  return ret;
}

int ZLib::UncompressChunk(Bytef* dest, uLongf* destLen, const Bytef* source,
                          uLong sourceLen) {
  return UncompressChunkOrAll(dest, destLen, source, sourceLen, Z_SYNC_FLUSH);
}
