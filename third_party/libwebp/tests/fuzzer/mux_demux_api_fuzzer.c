// Copyright 2018 Google Inc.
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
//
////////////////////////////////////////////////////////////////////////////////

#include "./fuzz_utils.h"
#include "src/webp/demux.h"
#include "src/webp/mux.h"

int LLVMFuzzerTestOneInput(const uint8_t* const data, size_t size) {
  WebPData webp_data;
  WebPDataInit(&webp_data);
  webp_data.size = size;
  webp_data.bytes = data;

  // Extracted chunks and frames are not processed or decoded,
  // which is already covered extensively by the other fuzz targets.

  if (size & 1) {
    // Mux API
    WebPMux* mux = WebPMuxCreate(&webp_data, size & 2);
    if (!mux) return 0;

    WebPData chunk;
    WebPMuxGetChunk(mux, "EXIF", &chunk);
    WebPMuxGetChunk(mux, "ICCP", &chunk);
    WebPMuxGetChunk(mux, "FUZZ", &chunk);  // unknown

    uint32_t flags;
    WebPMuxGetFeatures(mux, &flags);

    WebPMuxAnimParams params;
    WebPMuxGetAnimationParams(mux, &params);

    WebPMuxError status;
    WebPMuxFrameInfo info;
    for (int i = 0; i < kFuzzFrameLimit; i++) {
      status = WebPMuxGetFrame(mux, i + 1, &info);
      if (status == WEBP_MUX_NOT_FOUND) {
        break;
      } else if (status == WEBP_MUX_OK) {
        WebPDataClear(&info.bitstream);
      }
    }

    WebPMuxDelete(mux);
  } else {
    // Demux API
    WebPDemuxer* demux;
    if (size & 2) {
      WebPDemuxState state;
      demux = WebPDemuxPartial(&webp_data, &state);
      if (state < WEBP_DEMUX_PARSED_HEADER) {
        WebPDemuxDelete(demux);
        return 0;
      }
    } else {
      demux = WebPDemux(&webp_data);
      if (!demux) return 0;
    }

    WebPChunkIterator chunk_iter;
    if (WebPDemuxGetChunk(demux, "EXIF", 1, &chunk_iter)) {
      WebPDemuxNextChunk(&chunk_iter);
    }
    WebPDemuxReleaseChunkIterator(&chunk_iter);
    if (WebPDemuxGetChunk(demux, "ICCP", 0, &chunk_iter)) {  // 0 == last
      WebPDemuxPrevChunk(&chunk_iter);
    }
    WebPDemuxReleaseChunkIterator(&chunk_iter);
    // Skips FUZZ because the Demux API has no concept of (un)known chunks.

    WebPIterator iter;
    if (WebPDemuxGetFrame(demux, 1, &iter)) {
      for (int i = 1; i < kFuzzFrameLimit; i++) {
        if (!WebPDemuxNextFrame(&iter)) break;
      }
    }

    WebPDemuxReleaseIterator(&iter);
    WebPDemuxDelete(demux);
  }

  return 0;
}
