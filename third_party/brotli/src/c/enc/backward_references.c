/* Copyright 2013 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* Function to find backward reference copies. */

#include "backward_references.h"

#include "../common/constants.h"
#include "../common/context.h"
#include "../common/platform.h"
#include "command.h"
#include "compound_dictionary.h"
#include "encoder_dict.h"
#include "hash.h"
#include "params.h"
#include "quality.h"  /* IWYU pragma: keep for inc */

BROTLI_INTERNAL const BROTLI_MODEL("small")
    uint8_t kIsBase64[256] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        0, 0, 0, 1, /* 43 '+', 47 '/' (45 '-' is 0) */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, /* 48-57 '0'-'9' (61 '='
                                                           is 0) */
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 65-79 'A'-'O' */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, /* 80-90 'P'-'Z' */
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 97-111 'a'-'o' */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, /* 112-122 'p'-'z' */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const size_t kBase64TriggerLen = 8;

static BROTLI_INLINE BROTLI_BOOL IsBase64Char(uint8_t c) {
  return TO_BROTLI_BOOL(kIsBase64[c]);
}

static BROTLI_INLINE BROTLI_BOOL MatchTrigger(const uint8_t* ringbuffer,
                                              size_t mask, size_t pos) {
  const char* trigger = ";base64,";
  size_t i;
  for (i = 0; i < kBase64TriggerLen; ++i) {
    if (ringbuffer[(pos + i) & mask] != trigger[i]) return BROTLI_FALSE;
  }
  return BROTLI_TRUE;
}

static const uint64_t kBase64Trigger64 =
    BROTLI_MAKE_UINT64_T(0x2C343665u, 0x7361623Bu);

static size_t FindNextBase64Trigger(const uint8_t* ringbuffer, size_t mask,
                                    size_t pos, size_t end) {
  while (pos + kBase64TriggerLen <= end) {
    size_t ringbuffer_size = mask + 1;
    size_t pos_index = pos & mask;
    size_t contiguous_len = ringbuffer_size - pos_index;
    size_t max_scan_len = end - pos;
    size_t scan_len = BROTLI_MIN(size_t, contiguous_len, max_scan_len);

    const uint8_t* p =
        (const uint8_t*)memchr(&ringbuffer[pos_index], ';', scan_len);
    if (p != NULL) {
      size_t offset = (size_t)(p - &ringbuffer[pos_index]);
      if (offset + kBase64TriggerLen <= scan_len) {
        if (BROTLI_UNALIGNED_LOAD64LE(p) == kBase64Trigger64) {
          return pos + offset;
        }
      } else if (pos + offset + kBase64TriggerLen <= end) {
        if (MatchTrigger(ringbuffer, mask, pos + offset)) {
          return pos + offset;
        }
      } else {
        return end;
      }
      pos += offset + 1;
    } else {
      pos += scan_len;
    }
  }
  return end;
}

static BROTLI_INLINE BROTLI_BOOL CompareRingbuffer(
    const uint8_t* ringbuffer, size_t mask,
    size_t pos1, size_t pos2, size_t len) {
  size_t idx1 = pos1 & mask;
  size_t idx2 = pos2 & mask;
  if (idx1 + len <= mask + 1 && idx2 + len <= mask + 1) {
    return TO_BROTLI_BOOL(
        memcmp(&ringbuffer[idx1], &ringbuffer[idx2], len) == 0);
  }
  while (len > 0) {
    size_t chunk1 = mask + 1 - (pos1 & mask);
    size_t chunk2 = mask + 1 - (pos2 & mask);
    size_t chunk = BROTLI_MIN(size_t, len, BROTLI_MIN(size_t, chunk1, chunk2));
    if (memcmp(&ringbuffer[pos1 & mask], &ringbuffer[pos2 & mask], chunk) != 0) {
      return BROTLI_FALSE;
    }
    pos1 += chunk;
    pos2 += chunk;
    len -= chunk;
  }
  return BROTLI_TRUE;
}

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

static BROTLI_INLINE size_t ComputeDistanceCode(size_t distance,
                                                size_t max_distance,
                                                const int* dist_cache) {
  if (distance <= max_distance) {
    size_t distance_plus_3 = distance + 3;
    size_t offset0 = distance_plus_3 - (size_t)dist_cache[0];
    size_t offset1 = distance_plus_3 - (size_t)dist_cache[1];
    if (distance == (size_t)dist_cache[0]) {
      return 0;
    } else if (distance == (size_t)dist_cache[1]) {
      return 1;
    } else if (offset0 < 7) {
      return (0x9750468 >> (4 * offset0)) & 0xF;
    } else if (offset1 < 7) {
      return (0xFDB1ACE >> (4 * offset1)) & 0xF;
    } else if (distance == (size_t)dist_cache[2]) {
      return 2;
    } else if (distance == (size_t)dist_cache[3]) {
      return 3;
    }
  }
  return distance + BROTLI_NUM_DISTANCE_SHORT_CODES - 1;
}

#define EXPAND_CAT(a, b) CAT(a, b)
#define CAT(a, b) a ## b
#define FN(X) EXPAND_CAT(X, HASHER())
#define EXPORT_FN(X) EXPAND_CAT(X, EXPAND_CAT(PREFIX(), HASHER()))

#define PREFIX() N
#define ENABLE_COMPOUND_DICTIONARY 0

#define HASHER() H2
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER

#define HASHER() H3
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER

#define HASHER() H4
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER

#define HASHER() H5
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER

#define HASHER() H6
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER

#define HASHER() H40
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER

#define HASHER() H41
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER

#define HASHER() H42
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER

#define HASHER() H54
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER

#define HASHER() H35
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER

#define HASHER() H55
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER

#define HASHER() H65
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER

#if defined(BROTLI_MAX_SIMD_QUALITY)
#define HASHER() H58
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER

#define HASHER() H59
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_opt_inc.h"
#undef HASHER

#define HASHER() H68
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER
#endif

#undef ENABLE_COMPOUND_DICTIONARY
#undef PREFIX
#define PREFIX() D
#define ENABLE_COMPOUND_DICTIONARY 1

#define HASHER() H3
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER
#define HASHER() H4
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER
#define HASHER() H5
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER
#define HASHER() H6
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER
#define HASHER() H40
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER
#define HASHER() H41
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER
#define HASHER() H42
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER
#define HASHER() H55
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER
#define HASHER() H65
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER
#if defined(BROTLI_MAX_SIMD_QUALITY)
#define HASHER() H58
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER
#define HASHER() H59
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_opt_inc.h"
#undef HASHER
#define HASHER() H68
/* NOLINTNEXTLINE(build/include) */
#include "backward_references_inc.h"
#undef HASHER
#endif

#undef ENABLE_COMPOUND_DICTIONARY
#undef PREFIX

#undef EXPORT_FN
#undef FN
#undef CAT
#undef EXPAND_CAT

void BrotliCreateBackwardReferences(size_t num_bytes,
    size_t position, const uint8_t* ringbuffer, size_t ringbuffer_mask,
    ContextLut literal_context_lut, const BrotliEncoderParams* params,
    Hasher* hasher, int* dist_cache, size_t* last_insert_len,
    Command* commands, size_t* num_commands, size_t* num_literals) {
  if (params->dictionary.compound.num_chunks != 0) {
    switch (params->hasher.type) {
#define CASE_(N)                                                    \
      case N:                                                       \
        CreateBackwardReferencesDH ## N(num_bytes,                  \
            position, ringbuffer, ringbuffer_mask,                  \
            literal_context_lut, params, hasher, dist_cache,        \
            last_insert_len, commands, num_commands, num_literals); \
        return;
      CASE_(3)
      CASE_(4)
      CASE_(5)
      CASE_(6)
#if defined(BROTLI_MAX_SIMD_QUALITY)
      CASE_(58)
      CASE_(59)
      CASE_(68)
#endif
      CASE_(40)
      CASE_(41)
      CASE_(42)
      CASE_(55)
      CASE_(65)
#undef CASE_
      default:
        BROTLI_DCHECK(BROTLI_FALSE);
        break;
    }
  }

  switch (params->hasher.type) {
#define CASE_(N)                                                  \
    case N:                                                       \
      CreateBackwardReferencesNH ## N(num_bytes,                  \
          position, ringbuffer, ringbuffer_mask,                  \
          literal_context_lut, params, hasher, dist_cache,        \
          last_insert_len, commands, num_commands, num_literals); \
      return;
    FOR_GENERIC_HASHERS(CASE_)
#undef CASE_
    default:
      BROTLI_DCHECK(BROTLI_FALSE);
      break;
  }
}

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif
