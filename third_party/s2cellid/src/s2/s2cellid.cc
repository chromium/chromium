// Copyright 2005 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// Author: ericv@google.com (Eric Veach)

#include "s2/s2cellid.h"

#include <algorithm>
#include <cfloat>
#include <cstring>
#include <iosfwd>
#include <vector>

#include <mutex>
#include "base/check_op.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "s2/r1interval.h"
#include "s2/s2latlng.h"

#ifdef _MSC_VER
#pragma warning(disable : 4018) /* '<' : signed/unsigned mismatch */
#endif

using S2::internal::kSwapMask;
using S2::internal::kInvertMask;
using S2::internal::kPosToIJ;
using S2::internal::kPosToOrientation;
using std::max;
using std::min;
using std::vector;

// The following lookup tables are used to convert efficiently between an
// (i,j) cell index and the corresponding position along the Hilbert curve.
// "lookup_pos" maps 4 bits of "i", 4 bits of "j", and 2 bits representing the
// orientation of the current cell into 8 bits representing the order in which
// that subcell is visited by the Hilbert curve, plus 2 bits indicating the
// new orientation of the Hilbert curve within that subcell.  (Cell
// orientations are represented as combination of kSwapMask and kInvertMask.)
//
// "lookup_ij" is an inverted table used for mapping in the opposite
// direction.
//
// We also experimented with looking up 16 bits at a time (14 bits of position
// plus 2 of orientation) but found that smaller lookup tables gave better
// performance.  (2KB fits easily in the primary cache.)

// Values for these constants are *declared* in the *.h file. Even though
// the declaration specifies a value for the constant, that declaration
// is not a *definition* of storage for the value. Because the values are
// supplied in the declaration, we don't need the values here. Failing to
// define storage causes link errors for any code that tries to take the
// address of one of these values.
int const S2CellId::kFaceBits;
int const S2CellId::kNumFaces;
int const S2CellId::kMaxLevel;
int const S2CellId::kPosBits;
int const S2CellId::kMaxSize;

static int const kLookupBits = 4;
static uint16_t lookup_pos[1 << (2 * kLookupBits + 2)];
static uint16_t lookup_ij[1 << (2 * kLookupBits + 2)];

static void InitLookupCell(int level,
                           int i,
                           int j,
                           int orig_orientation,
                           int pos,
                           int orientation) {
  if (level == kLookupBits) {
    int ij = (i << kLookupBits) + j;
    lookup_pos[(ij << 2) + orig_orientation] = (pos << 2) + orientation;
    lookup_ij[(pos << 2) + orig_orientation] = (ij << 2) + orientation;
  } else {
    level++;
    i <<= 1;
    j <<= 1;
    pos <<= 2;
    int const* r = kPosToIJ[orientation];
    InitLookupCell(level, i + (r[0] >> 1), j + (r[0] & 1), orig_orientation,
                   pos, orientation ^ kPosToOrientation[0]);
    InitLookupCell(level, i + (r[1] >> 1), j + (r[1] & 1), orig_orientation,
                   pos + 1, orientation ^ kPosToOrientation[1]);
    InitLookupCell(level, i + (r[2] >> 1), j + (r[2] & 1), orig_orientation,
                   pos + 2, orientation ^ kPosToOrientation[2]);
    InitLookupCell(level, i + (r[3] >> 1), j + (r[3] & 1), orig_orientation,
                   pos + 3, orientation ^ kPosToOrientation[3]);
  }
}

static std::once_flag flag;
inline static void MaybeInit() {
  std::call_once(flag, [] {
    InitLookupCell(0, 0, 0, 0, 0, 0);
    InitLookupCell(0, 0, 0, kSwapMask, 0, kSwapMask);
    InitLookupCell(0, 0, 0, kInvertMask, 0, kInvertMask);
    InitLookupCell(0, 0, 0, kSwapMask | kInvertMask, 0,
                   kSwapMask | kInvertMask);
  });
}

S2CellId S2CellId::advance(int64_t steps) const {
  if (steps == 0)
    return *this;

  // We clamp the number of steps if necessary to ensure that we do not
  // advance past the End() or before the Begin() of this level.  Note that
  // min_steps and max_steps always fit in a signed 64-bit integer.

  int step_shift = 2 * (kMaxLevel - level()) + 1;
  if (steps < 0) {
    int64_t min_steps = -static_cast<int64_t>(id_ >> step_shift);
    if (steps < min_steps)
      steps = min_steps;
  } else {
    int64_t max_steps = (kWrapOffset + lsb() - id_) >> step_shift;
    if (steps > max_steps)
      steps = max_steps;
  }
  // If steps is negative, then shifting it left has undefined behavior.
  // Cast to uint64_t for a 2's complement answer.
  return S2CellId(id_ + (static_cast<uint64_t>(steps) << step_shift));
}

int64_t S2CellId::distance_from_begin() const {
  const int step_shift = 2 * (kMaxLevel - level()) + 1;
  return id_ >> step_shift;
}

S2CellId S2CellId::advance_wrap(int64_t steps) const {
  DCHECK(is_valid());
  if (steps == 0)
    return *this;

  int step_shift = 2 * (kMaxLevel - level()) + 1;
  if (steps < 0) {
    int64_t min_steps = -static_cast<int64_t>(id_ >> step_shift);
    if (steps < min_steps) {
      int64_t step_wrap = kWrapOffset >> step_shift;
      steps %= step_wrap;
      if (steps < min_steps)
        steps += step_wrap;
    }
  } else {
    // Unlike advance(), we don't want to return End(level).
    int64_t max_steps = (kWrapOffset - id_) >> step_shift;
    if (steps > max_steps) {
      int64_t step_wrap = kWrapOffset >> step_shift;
      steps %= step_wrap;
      if (steps > max_steps)
        steps -= step_wrap;
    }
  }
  return S2CellId(id_ + (static_cast<uint64_t>(steps) << step_shift));
}

S2CellId S2CellId::maximum_tile(S2CellId const limit) const {
  S2CellId id = *this;
  S2CellId start = id.range_min();
  if (start >= limit.range_min())
    return limit;

  if (id.range_max() >= limit) {
    // The cell is too large.  Shrink it.  Note that when generating coverings
    // of S2CellId ranges, this loop usually executes only once.  Also because
    // id.range_min() < limit.range_min(), we will always exit the loop by the
    // time we reach a leaf cell.
    do {
      id = id.child(0);
    } while (id.range_max() >= limit);
    return id;
  }
  // The cell may be too small.  Grow it if necessary.  Note that generally
  // this loop only iterates once.
  while (!id.is_face()) {
    S2CellId parent = id.parent();
    if (parent.range_min() != start || parent.range_max() >= limit)
      break;
    id = parent;
  }
  return id;
}

int S2CellId::GetCommonAncestorLevel(S2CellId other) const {
  // Basically we find the first bit position at which the two S2CellIds
  // differ and convert that to a level.  The max() below is necessary for the
  // case where one S2CellId is a descendant of the other.
  uint64_t bits = max(id() ^ other.id(), max(lsb(), other.lsb()));

  // Compute the position of the most significant bit, and then map
  // {0} -> 30, {1,2} -> 29, {3,4} -> 28, ... , {59,60} -> 0, {61,62,63} -> -1.
  return max(60 - Bits::FindMSBSetNonZero64(bits), -1) >> 1;
}

// Print the num_digits low order hex digits.
static std::string HexFormatString(uint64_t val, size_t num_digits) {
  std::string result(num_digits, ' ');
  for (; num_digits--; val >>= 4)
    result[num_digits] = "0123456789abcdef"[val & 0xF];
  return result;
}

std::string S2CellId::ToToken() const {
  // Simple implementation: print the id in hex without trailing zeros.
  // Using hex has the advantage that the tokens are case-insensitive, all
  // characters are alphanumeric, no characters require any special escaping
  // in queries for most indexing systems, and it's easy to compare cell
  // tokens against the feature ids of the corresponding features.
  //
  // Using base 64 would produce slightly shorter tokens, but for typical cell
  // sizes used during indexing (up to level 15 or so) the average savings
  // would be less than 2 bytes per cell which doesn't seem worth it.

  // "0" with trailing 0s stripped is the empty std::string, which is not a
  // reasonable token.  Encode as "X".
  if (id_ == 0)
    return "X";
  size_t const num_zero_digits = Bits::FindLSBSetNonZero64(id_) / 4;
  return HexFormatString(id_ >> (4 * num_zero_digits), 16 - num_zero_digits);
}

S2CellId S2CellId::FromToken(const char* token, size_t length) {
  if (length > 16)
    return S2CellId::None();
  uint64_t id = 0;
  for (int i = 0, pos = 60; i < length; ++i, pos -= 4) {
    uint64_t d;
    if ('0' <= token[i] && token[i] <= '9') {
      d = token[i] - '0';
    } else if ('a' <= token[i] && token[i] <= 'f') {
      d = token[i] - 'a' + 10;
    } else if ('A' <= token[i] && token[i] <= 'F') {
      d = token[i] - 'A' + 10;
    } else {
      return S2CellId::None();
    }
    id |= d << pos;
  }
  return S2CellId(id);
}

S2CellId S2CellId::FromToken(std::string const& token) {
  return FromToken(token.data(), token.size());
}

S2CellId S2CellId::FromFaceIJ(int face, int i, int j) {
  // Initialization if not done yet
  MaybeInit();

  // Optimization notes:
  //  - Non-overlapping bit fields can be combined with either "+" or "|".
  //    Generally "+" seems to produce better code, but not always.

  // Note that this value gets shifted one bit to the left at the end
  // of the function.
  uint64_t n = static_cast<uint64_t>(face) << (kPosBits - 1);

  // Alternating faces have opposite Hilbert curve orientations; this
  // is necessary in order for all faces to have a right-handed
  // coordinate system.
  uint64_t bits = (face & kSwapMask);

// Each iteration maps 4 bits of "i" and "j" into 8 bits of the Hilbert
// curve position.  The lookup table transforms a 10-bit key of the form
// "iiiijjjjoo" to a 10-bit value of the form "ppppppppoo", where the
// letters [ijpo] denote bits of "i", "j", Hilbert curve position, and
// Hilbert curve orientation respectively.
#define GET_BITS(k)                                                 \
  do {                                                              \
    int const mask = (1 << kLookupBits) - 1;                        \
    bits += ((i >> (k * kLookupBits)) & mask) << (kLookupBits + 2); \
    bits += ((j >> (k * kLookupBits)) & mask) << 2;                 \
    bits = lookup_pos[bits];                                        \
    n |= (bits >> 2) << (k * 2 * kLookupBits);                      \
    bits &= (kSwapMask | kInvertMask);                              \
  } while (0)

  GET_BITS(7);
  GET_BITS(6);
  GET_BITS(5);
  GET_BITS(4);
  GET_BITS(3);
  GET_BITS(2);
  GET_BITS(1);
  GET_BITS(0);
#undef GET_BITS

  return S2CellId(n * 2 + 1);
}

S2CellId::S2CellId(S2Point const& p) {
  double u, v;
  int face = S2::XYZtoFaceUV(p, &u, &v);
  int i = S2::STtoIJ(S2::UVtoST(u));
  int j = S2::STtoIJ(S2::UVtoST(v));
  id_ = FromFaceIJ(face, i, j).id();
}

S2CellId::S2CellId(S2LatLng const& ll) : S2CellId(ll.ToPoint()) {}

int S2CellId::ToFaceIJOrientation(int* pi, int* pj, int* orientation) const {
  // Initialization if not done yet
  MaybeInit();

  int i = 0, j = 0;
  int face = this->face();
  int bits = (face & kSwapMask);

// Each iteration maps 8 bits of the Hilbert curve position into
// 4 bits of "i" and "j".  The lookup table transforms a key of the
// form "ppppppppoo" to a value of the form "iiiijjjjoo", where the
// letters [ijpo] represents bits of "i", "j", the Hilbert curve
// position, and the Hilbert curve orientation respectively.
//
// On the first iteration we need to be careful to clear out the bits
// representing the cube face.
#define GET_BITS(k)                                                           \
  do {                                                                        \
    int const nbits = (k == 7) ? (kMaxLevel - 7 * kLookupBits) : kLookupBits; \
    bits += (static_cast<int>(id_ >> (k * 2 * kLookupBits + 1)) &             \
             ((1 << (2 * nbits)) - 1))                                        \
            << 2;                                                             \
    bits = lookup_ij[bits];                                                   \
    i += (bits >> (kLookupBits + 2)) << (k * kLookupBits);                    \
    j += ((bits >> 2) & ((1 << kLookupBits) - 1)) << (k * kLookupBits);       \
    bits &= (kSwapMask | kInvertMask);                                        \
  } while (0)

  GET_BITS(7);
  GET_BITS(6);
  GET_BITS(5);
  GET_BITS(4);
  GET_BITS(3);
  GET_BITS(2);
  GET_BITS(1);
  GET_BITS(0);
#undef GET_BITS

  *pi = i;
  *pj = j;

  if (orientation != nullptr) {
    // The position of a non-leaf cell at level "n" consists of a prefix of
    // 2*n bits that identifies the cell, followed by a suffix of
    // 2*(kMaxLevel-n)+1 bits of the form 10*.  If n==kMaxLevel, the suffix is
    // just "1" and has no effect.  Otherwise, it consists of "10", followed
    // by (kMaxLevel-n-1) repetitions of "00", followed by "0".  The "10" has
    // no effect, while each occurrence of "00" has the effect of reversing
    // the kSwapMask bit.
    DCHECK_EQ(0, kPosToOrientation[2]);
    DCHECK_EQ(kSwapMask, kPosToOrientation[0]);
    if (lsb() & static_cast<uint64_t>(0x1111111111111110)) {
      bits ^= kSwapMask;
    }
    *orientation = bits;
  }
  return face;
}

S2Point S2CellId::ToPointRaw() const {
  int si, ti;
  int face = GetCenterSiTi(&si, &ti);
  return S2::FaceSiTitoXYZ(face, si, ti);
}

S2LatLng S2CellId::ToLatLng() const {
  return S2LatLng(ToPointRaw());
}

R2Point S2CellId::GetCenterST() const {
  int si, ti;
  GetCenterSiTi(&si, &ti);
  return R2Point(S2::SiTitoST(si), S2::SiTitoST(ti));
}

R2Point S2CellId::GetCenterUV() const {
  int si, ti;
  GetCenterSiTi(&si, &ti);
  return R2Point(S2::STtoUV(S2::SiTitoST(si)), S2::STtoUV(S2::SiTitoST(ti)));
}

R2Rect S2CellId::IJLevelToBoundUV(int ij[2], int level) {
  R2Rect bound;
  int cell_size = GetSizeIJ(level);
  for (int d = 0; d < 2; ++d) {
    int ij_lo = ij[d] & -cell_size;
    int ij_hi = ij_lo + cell_size;
    bound[d][0] = S2::STtoUV(S2::IJtoSTMin(ij_lo));
    bound[d][1] = S2::STtoUV(S2::IJtoSTMin(ij_hi));
  }
  return bound;
}

R2Rect S2CellId::GetBoundST() const {
  double size = GetSizeST();
  return R2Rect::FromCenterSize(GetCenterST(), R2Point(size, size));
}

R2Rect S2CellId::GetBoundUV() const {
  int ij[2];
  ToFaceIJOrientation(&ij[0], &ij[1], nullptr);
  return IJLevelToBoundUV(ij, level());
}

// This is a helper function for ExpandedByDistanceUV().
//
// Given an edge of the form (u,v0)-(u,v1), let max_v = max(abs(v0), abs(v1)).
// This method returns a new u-coordinate u' such that the distance from the
// line u=u' to the given edge (u,v0)-(u,v1) is exactly the given distance
// (which is specified as the sine of the angle corresponding to the distance).
static double ExpandEndpoint(double u, double max_v, double sin_dist) {
  // This is based on solving a spherical right triangle, similar to the
  // calculation in S2Cap::GetRectBound.
  double sin_u_shift =
      sin_dist * sqrt((1 + u * u + max_v * max_v) / (1 + u * u));
  double cos_u_shift = sqrt(1 - sin_u_shift * sin_u_shift);
  // The following is an expansion of tan(atan(u) + asin(sin_u_shift)).
  return (cos_u_shift * u + sin_u_shift) / (cos_u_shift - sin_u_shift * u);
}

/* static */
R2Rect S2CellId::ExpandedByDistanceUV(R2Rect const& uv, S1Angle distance) {
  // Expand each of the four sides of the rectangle just enough to include all
  // points within the given distance of that side.  (The rectangle may be
  // expanded by a different amount in (u,v)-space on each side.)
  double u0 = uv[0][0], u1 = uv[0][1], v0 = uv[1][0], v1 = uv[1][1];
  double max_u = std::max(std::abs(u0), std::abs(u1));
  double max_v = std::max(std::abs(v0), std::abs(v1));
  double sin_dist = sin(distance);
  return R2Rect(R1Interval(ExpandEndpoint(u0, max_v, -sin_dist),
                           ExpandEndpoint(u1, max_v, sin_dist)),
                R1Interval(ExpandEndpoint(v0, max_u, -sin_dist),
                           ExpandEndpoint(v1, max_u, sin_dist)));
}

S2CellId S2CellId::FromFaceIJWrap(int face, int i, int j) {
  // Convert i and j to the coordinates of a leaf cell just beyond the
  // boundary of this face.  This prevents 32-bit overflow in the case
  // of finding the neighbors of a face cell.
  i = max(-1, min(kMaxSize, i));
  j = max(-1, min(kMaxSize, j));

  // We want to wrap these coordinates onto the appropriate adjacent face.
  // The easiest way to do this is to convert the (i,j) coordinates to (x,y,z)
  // (which yields a point outside the normal face boundary), and then call
  // S2::XYZtoFaceUV() to project back onto the correct face.
  //
  // The code below converts (i,j) to (si,ti), and then (si,ti) to (u,v) using
  // the linear projection (u=2*s-1 and v=2*t-1).  (The code further below
  // converts back using the inverse projection, s=0.5*(u+1) and t=0.5*(v+1).
  // Any projection would work here, so we use the simplest.)  We also clamp
  // the (u,v) coordinates so that the point is barely outside the
  // [-1,1]x[-1,1] face rectangle, since otherwise the reprojection step
  // (which divides by the new z coordinate) might change the other
  // coordinates enough so that we end up in the wrong leaf cell.
  static const double kScale = 1.0 / kMaxSize;
  static const double kLimit = 1.0 + DBL_EPSILON;
  // The arithmetic below is designed to avoid 32-bit integer overflows.
  DCHECK_EQ(0, kMaxSize % 2);
  double u = max(-kLimit, min(kLimit, kScale * (2 * (i - kMaxSize / 2) + 1)));
  double v = max(-kLimit, min(kLimit, kScale * (2 * (j - kMaxSize / 2) + 1)));

  // Find the leaf cell coordinates on the adjacent face, and convert
  // them to a cell id at the appropriate level.
  face = S2::XYZtoFaceUV(S2::FaceUVtoXYZ(face, u, v), &u, &v);
  return FromFaceIJ(face, S2::STtoIJ(0.5 * (u + 1)), S2::STtoIJ(0.5 * (v + 1)));
}

inline S2CellId S2CellId::FromFaceIJSame(int face,
                                         int i,
                                         int j,
                                         bool same_face) {
  if (same_face)
    return S2CellId::FromFaceIJ(face, i, j);
  else
    return S2CellId::FromFaceIJWrap(face, i, j);
}

void S2CellId::GetEdgeNeighbors(S2CellId neighbors[4]) const {
  int i, j;
  int level = this->level();
  int size = GetSizeIJ(level);
  int face = ToFaceIJOrientation(&i, &j, nullptr);

  // Edges 0, 1, 2, 3 are in the down, right, up, left directions.
  neighbors[0] = FromFaceIJSame(face, i, j - size, j - size >= 0).parent(level);
  neighbors[1] =
      FromFaceIJSame(face, i + size, j, i + size < kMaxSize).parent(level);
  neighbors[2] =
      FromFaceIJSame(face, i, j + size, j + size < kMaxSize).parent(level);
  neighbors[3] = FromFaceIJSame(face, i - size, j, i - size >= 0).parent(level);
}

void S2CellId::AppendVertexNeighbors(int level,
                                     vector<S2CellId>* output) const {
  // "level" must be strictly less than this cell's level so that we can
  // determine which vertex this cell is closest to.
  DCHECK_LT(level, this->level());
  int i, j;
  int face = ToFaceIJOrientation(&i, &j, nullptr);

  // Determine the i- and j-offsets to the closest neighboring cell in each
  // direction.  This involves looking at the next bit of "i" and "j" to
  // determine which quadrant of this->parent(level) this cell lies in.
  int halfsize = GetSizeIJ(level + 1);
  int size = halfsize << 1;
  bool isame, jsame;
  int ioffset, joffset;
  if (i & halfsize) {
    ioffset = size;
    isame = (i + size) < kMaxSize;
  } else {
    ioffset = -size;
    isame = (i - size) >= 0;
  }
  if (j & halfsize) {
    joffset = size;
    jsame = (j + size) < kMaxSize;
  } else {
    joffset = -size;
    jsame = (j - size) >= 0;
  }

  output->push_back(parent(level));
  output->push_back(FromFaceIJSame(face, i + ioffset, j, isame).parent(level));
  output->push_back(FromFaceIJSame(face, i, j + joffset, jsame).parent(level));
  // If i- and j- edge neighbors are *both* on a different face, then this
  // vertex only has three neighbors (it is one of the 8 cube vertices).
  if (isame || jsame) {
    output->push_back(
        FromFaceIJSame(face, i + ioffset, j + joffset, isame && jsame)
            .parent(level));
  }
}

void S2CellId::AppendAllNeighbors(int nbr_level,
                                  vector<S2CellId>* output) const {
  DCHECK_GE(nbr_level, level());
  int i, j;
  int face = ToFaceIJOrientation(&i, &j, nullptr);

  // Find the coordinates of the lower left-hand leaf cell.  We need to
  // normalize (i,j) to a known position within the cell because nbr_level
  // may be larger than this cell's level.
  int size = GetSizeIJ();
  i &= -size;
  j &= -size;

  int nbr_size = GetSizeIJ(nbr_level);
  DCHECK_LE(nbr_size, size);

  // We compute the top-bottom, left-right, and diagonal neighbors in one
  // pass.  The loop test is at the end of the loop to avoid 32-bit overflow.
  for (int k = -nbr_size;; k += nbr_size) {
    bool same_face;
    if (k < 0) {
      same_face = (j + k >= 0);
    } else if (k >= size) {
      same_face = (j + k < kMaxSize);
    } else {
      same_face = true;
      // Top and bottom neighbors.
      output->push_back(FromFaceIJSame(face, i + k, j - nbr_size, j - size >= 0)
                            .parent(nbr_level));
      output->push_back(
          FromFaceIJSame(face, i + k, j + size, j + size < kMaxSize)
              .parent(nbr_level));
    }
    // Left, right, and diagonal neighbors.
    output->push_back(
        FromFaceIJSame(face, i - nbr_size, j + k, same_face && i - size >= 0)
            .parent(nbr_level));
    output->push_back(
        FromFaceIJSame(face, i + size, j + k, same_face && i + size < kMaxSize)
            .parent(nbr_level));
    if (k >= size)
      break;
  }
}

std::string S2CellId::ToString() const {
  if (!is_valid()) {
    return base::StringPrintf("Invalid: %016" PRIu64, id());
  }
  std::string out = base::StringPrintf("%d/", face());
  for (int current_level = 1; current_level <= level(); ++current_level) {
    // Avoid dependencies of SimpleItoA, and slowness of StringAppendF &
    // std::to_string.
    out += "0123"[child_position(current_level)];
  }
  return out;
}

std::ostream& operator<<(std::ostream& os, S2CellId id) {
  return os << id.ToString();
}
