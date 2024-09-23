// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides functions necessary to link the decoder fuzz tests. The
// functions are not used by the tests being built, but their dependencies in
// apps/shared and avif_*_helpers.cc unconditionally reference them.

#include <stdio.h>

#include "src/apps/shared/avifjpeg.h"
#include "src/apps/shared/avifpng.h"

avifBool avifJPEGRead(const char * inputFilename,
                      avifImage * avif,
                      avifPixelFormat requestedFormat,
                      uint32_t requestedDepth,
                      avifChromaDownsampling chromaDownsampling,
                      avifBool ignoreColorProfile,
                      avifBool ignoreExif,
                      avifBool ignoreXMP,
                      avifBool ignoreGainMap,
                      uint32_t imageSizeLimit) {
  fprintf(stderr, "The tests were built without JPEG support!\n");
  return AVIF_FALSE;
}

avifBool avifPNGRead(const char * inputFilename,
                     avifImage * avif,
                     avifPixelFormat requestedFormat,
                     uint32_t requestedDepth,
                     avifChromaDownsampling chromaDownsampling,
                     avifBool ignoreColorProfile,
                     avifBool ignoreExif,
                     avifBool ignoreXMP,
                     avifBool allowChangingCicp,
                     uint32_t imageSizeLimit,
                     uint32_t * outPNGDepth) {
  fprintf(stderr, "The tests were built without PNG support!\n");
  return AVIF_FALSE;
}

avifBool avifPNGWrite(const char * outputFilename,
                      const avifImage * avif,
                      uint32_t requestedDepth,
                      avifChromaUpsampling chromaUpsampling,
                      int compressionLevel) {
  fprintf(stderr, "The tests were built without PNG support!\n");
  return AVIF_FALSE;
}
