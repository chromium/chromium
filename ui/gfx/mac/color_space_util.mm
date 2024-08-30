// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mac/color_space_util.h"

#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <optional>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/memory/scoped_policy.h"
#include "base/no_destructor.h"
#include "third_party/skia/modules/skcms/skcms.h"
#include "ui/gfx/color_space.h"

namespace gfx {

namespace {

// Read the value for the key in |key| to CFString and convert it to IdType.
// Use the list of pairs in |cfstr_id_pairs| to do the conversion (by doing a
// linear lookup).
template <typename IdType, typename StringIdPair>
bool GetImageBufferProperty(CFTypeRef value_untyped,
                            const std::vector<StringIdPair>& cfstr_id_pairs,
                            IdType* value_as_id) {
  CFStringRef value_as_string = base::apple::CFCast<CFStringRef>(value_untyped);
  if (!value_as_string) {
    return false;
  }

  for (const auto& p : cfstr_id_pairs) {
    if (p.cfstr_cm) {
      DCHECK(!CFStringCompare(p.cfstr_cv, p.cfstr_cm, 0));
    }
    if (!CFStringCompare(value_as_string, p.cfstr_cv, 0)) {
      *value_as_id = p.id;
      return true;
    }
  }

  return false;
}

struct CVImagePrimary {
  const CFStringRef cfstr_cv;
  const CFStringRef cfstr_cm;
  const gfx::ColorSpace::PrimaryID id;
};
const std::vector<CVImagePrimary>& GetSupportedImagePrimaries() {
  static const base::NoDestructor<std::vector<CVImagePrimary>>
      kSupportedPrimaries([] {
        std::vector<CVImagePrimary> supported_primaries;
        supported_primaries.push_back(
            {kCVImageBufferColorPrimaries_ITU_R_709_2,
             kCMFormatDescriptionColorPrimaries_ITU_R_709_2,
             gfx::ColorSpace::PrimaryID::BT709});
        supported_primaries.push_back(
            {kCVImageBufferColorPrimaries_EBU_3213,
             kCMFormatDescriptionColorPrimaries_EBU_3213,
             gfx::ColorSpace::PrimaryID::BT470BG});
        supported_primaries.push_back(
            {kCVImageBufferColorPrimaries_EBU_3213,
             kCMFormatDescriptionColorPrimaries_EBU_3213,
             gfx::ColorSpace::PrimaryID::EBU_3213_E});
        supported_primaries.push_back(
            {kCVImageBufferColorPrimaries_SMPTE_C,
             kCMFormatDescriptionColorPrimaries_SMPTE_C,
             gfx::ColorSpace::PrimaryID::SMPTE170M});
        supported_primaries.push_back(
            {kCVImageBufferColorPrimaries_SMPTE_C,
             kCMFormatDescriptionColorPrimaries_SMPTE_C,
             gfx::ColorSpace::PrimaryID::SMPTE240M});
        supported_primaries.push_back(
            {kCVImageBufferColorPrimaries_ITU_R_2020,
             kCMFormatDescriptionColorPrimaries_ITU_R_2020,
             gfx::ColorSpace::PrimaryID::BT2020});
        supported_primaries.push_back(
            {kCVImageBufferColorPrimaries_DCI_P3,
             kCMFormatDescriptionColorPrimaries_DCI_P3,
             gfx::ColorSpace::PrimaryID::SMPTEST431_2});
        supported_primaries.push_back(
            {kCVImageBufferColorPrimaries_P3_D65,
             kCMFormatDescriptionColorPrimaries_P3_D65,
             gfx::ColorSpace::PrimaryID::P3});
        return supported_primaries;
      }());
  return *kSupportedPrimaries;
}

gfx::ColorSpace::PrimaryID GetCoreVideoPrimary(CFTypeRef primaries_untyped) {
  auto primary_id = gfx::ColorSpace::PrimaryID::INVALID;
  if (!GetImageBufferProperty(primaries_untyped, GetSupportedImagePrimaries(),
                              &primary_id)) {
    DLOG(ERROR) << "Failed to find CVImageBufferRef primaries: "
                << primaries_untyped;
  }
  return primary_id;
}

struct CVImageTransferFn {
  const CFStringRef cfstr_cv;
  const CFStringRef cfstr_cm;
  const gfx::ColorSpace::TransferID id;
};
const std::vector<CVImageTransferFn>& GetSupportedImageTransferFn() {
  static const base::NoDestructor<std::vector<CVImageTransferFn>>
      kSupportedTransferFuncs([] {
        std::vector<CVImageTransferFn> supported_transfer_funcs;
        supported_transfer_funcs.push_back(
            {kCVImageBufferTransferFunction_ITU_R_709_2,
             kCMFormatDescriptionTransferFunction_ITU_R_709_2,
             gfx::ColorSpace::TransferID::BT709_APPLE});
        supported_transfer_funcs.push_back(
            {kCVImageBufferTransferFunction_ITU_R_709_2,
             kCMFormatDescriptionTransferFunction_ITU_R_709_2,
             gfx::ColorSpace::TransferID::BT709});
        supported_transfer_funcs.push_back(
            {kCVImageBufferTransferFunction_ITU_R_709_2,
             kCMFormatDescriptionTransferFunction_ITU_R_709_2,
             gfx::ColorSpace::TransferID::SMPTE170M});
        supported_transfer_funcs.push_back(
            {kCVImageBufferTransferFunction_SMPTE_240M_1995,
             kCMFormatDescriptionTransferFunction_SMPTE_240M_1995,
             gfx::ColorSpace::TransferID::SMPTE240M});
        supported_transfer_funcs.push_back(
            {kCVImageBufferTransferFunction_UseGamma,
             kCMFormatDescriptionTransferFunction_UseGamma,
             gfx::ColorSpace::TransferID::CUSTOM});
        supported_transfer_funcs.push_back(
            {kCVImageBufferTransferFunction_ITU_R_2020,
             kCMFormatDescriptionTransferFunction_ITU_R_2020,
             gfx::ColorSpace::TransferID::BT2020_10});
        supported_transfer_funcs.push_back(
            {kCVImageBufferTransferFunction_ITU_R_2020,
             kCMFormatDescriptionTransferFunction_ITU_R_2020,
             gfx::ColorSpace::TransferID::BT2020_12});
        supported_transfer_funcs.push_back(
            {kCVImageBufferTransferFunction_SMPTE_ST_428_1,
             kCMFormatDescriptionTransferFunction_SMPTE_ST_428_1,
             gfx::ColorSpace::TransferID::SMPTEST428_1});
        supported_transfer_funcs.push_back(
            {kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ,
             kCMFormatDescriptionTransferFunction_SMPTE_ST_2084_PQ,
             gfx::ColorSpace::TransferID::PQ});
        supported_transfer_funcs.push_back(
            {kCVImageBufferTransferFunction_ITU_R_2100_HLG,
             kCMFormatDescriptionTransferFunction_ITU_R_2100_HLG,
             gfx::ColorSpace::TransferID::HLG});
        supported_transfer_funcs.push_back({kCVImageBufferTransferFunction_sRGB,
                                            nullptr,
                                            gfx::ColorSpace::TransferID::SRGB});
        supported_transfer_funcs.push_back(
            {kCVImageBufferTransferFunction_Linear,
             kCMFormatDescriptionTransferFunction_Linear,
             gfx::ColorSpace::TransferID::LINEAR});
        supported_transfer_funcs.push_back(
            {kCVImageBufferTransferFunction_sRGB,
             kCMFormatDescriptionTransferFunction_sRGB,
             gfx::ColorSpace::TransferID::SRGB});

        return supported_transfer_funcs;
      }());
  return *kSupportedTransferFuncs;
}

gfx::ColorSpace::TransferID GetCoreVideoTransferFn(CFTypeRef transfer_untyped,
                                                   CFTypeRef gamma_untyped,
                                                   double* gamma) {
  // The named transfer function.
  auto transfer_id = gfx::ColorSpace::TransferID::INVALID;
  if (!GetImageBufferProperty(transfer_untyped, GetSupportedImageTransferFn(),
                              &transfer_id)) {
    DLOG(ERROR) << "Failed to find CVImageBufferRef transfer: "
                << transfer_untyped;
  }

  if (transfer_id != gfx::ColorSpace::TransferID::CUSTOM) {
    return transfer_id;
  }

  CFNumberRef gamma_number = base::apple::CFCast<CFNumberRef>(gamma_untyped);
  if (!gamma_number) {
    DLOG(ERROR) << "Failed to get gamma level.";
    return gfx::ColorSpace::TransferID::INVALID;
  }

  // CGFloat is a double on 64-bit systems.
  CGFloat gamma_double = 0;
  if (!CFNumberGetValue(gamma_number, kCFNumberCGFloatType, &gamma_double)) {
    DLOG(ERROR) << "Failed to get CVImageBufferRef gamma level as float.";
    return gfx::ColorSpace::TransferID::INVALID;
  }

  if (gamma_double == 2.2) {
    return gfx::ColorSpace::TransferID::GAMMA22;
  }
  if (gamma_double == 2.8) {
    return gfx::ColorSpace::TransferID::GAMMA28;
  }

  *gamma = gamma_double;
  return transfer_id;
}

struct CVImageMatrix {
  const CFStringRef cfstr_cv;
  const CFStringRef cfstr_cm;
  gfx::ColorSpace::MatrixID id;
};
const std::vector<CVImageMatrix>& GetSupportedImageMatrix() {
  static const base::NoDestructor<std::vector<CVImageMatrix>>
      kSupportedMatrices([] {
        std::vector<CVImageMatrix> supported_matrices;
        supported_matrices.push_back(
            {kCVImageBufferYCbCrMatrix_ITU_R_709_2,
             kCMFormatDescriptionYCbCrMatrix_ITU_R_709_2,
             gfx::ColorSpace::MatrixID::BT709});
        supported_matrices.push_back(
            {kCVImageBufferYCbCrMatrix_ITU_R_601_4,
             kCMFormatDescriptionYCbCrMatrix_ITU_R_601_4,
             gfx::ColorSpace::MatrixID::SMPTE170M});
        supported_matrices.push_back(
            {kCVImageBufferYCbCrMatrix_ITU_R_601_4,
             kCMFormatDescriptionYCbCrMatrix_ITU_R_601_4,
             gfx::ColorSpace::MatrixID::BT470BG});
        supported_matrices.push_back(
            {kCVImageBufferYCbCrMatrix_SMPTE_240M_1995,
             kCMFormatDescriptionYCbCrMatrix_SMPTE_240M_1995,
             gfx::ColorSpace::MatrixID::SMPTE240M});
        supported_matrices.push_back(
            {kCVImageBufferYCbCrMatrix_ITU_R_2020,
             kCMFormatDescriptionYCbCrMatrix_ITU_R_2020,
             gfx::ColorSpace::MatrixID::BT2020_NCL});
        return supported_matrices;
      }());
  return *kSupportedMatrices;
}

gfx::ColorSpace::MatrixID GetCoreVideoMatrix(CFTypeRef matrix_untyped) {
  auto matrix_id = gfx::ColorSpace::MatrixID::INVALID;
  if (!GetImageBufferProperty(matrix_untyped, GetSupportedImageMatrix(),
                              &matrix_id)) {
    DLOG(ERROR) << "Failed to find CVImageBufferRef YUV matrix: "
                << matrix_untyped;
  }
  return matrix_id;
}

}  // anonymous namespace

gfx::ColorSpace ColorSpaceFromCVImageBufferKeys(CFTypeRef primaries_untyped,
                                                CFTypeRef transfer_untyped,
                                                CFTypeRef gamma_untyped,
                                                CFTypeRef matrix_untyped) {
  double gamma;
  auto primary_id = GetCoreVideoPrimary(primaries_untyped);
  auto matrix_id = GetCoreVideoMatrix(matrix_untyped);
  auto transfer_id =
      GetCoreVideoTransferFn(transfer_untyped, gamma_untyped, &gamma);

  if (primary_id == gfx::ColorSpace::PrimaryID::INVALID ||
      matrix_id == gfx::ColorSpace::MatrixID::INVALID ||
      transfer_id == gfx::ColorSpace::TransferID::INVALID) {
    return gfx::ColorSpace();
  }

  // It is specified to the decoder to use luma=[16,235] chroma=[16,240] via
  // the kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange.
  //
  // TODO(crbug.com/40139254): We'll probably need support for more than limited
  // range content if we want this to be used for more than video sites.
  auto range_id = gfx::ColorSpace::RangeID::LIMITED;

  if (transfer_id == gfx::ColorSpace::TransferID::CUSTOM) {
    // Transfer functions can also be specified as a gamma value.
    skcms_TransferFunction custom_tr_fn = {2.2f, 1, 0, 1, 0, 0, 0};
    if (transfer_id == gfx::ColorSpace::TransferID::CUSTOM) {
      custom_tr_fn.g = gamma;
    }

    return gfx::ColorSpace(primary_id, gfx::ColorSpace::TransferID::CUSTOM,
                           matrix_id, range_id, nullptr, &custom_tr_fn);
  }

  return gfx::ColorSpace(primary_id, transfer_id, matrix_id, range_id);
}

// Converts a gfx::ColorSpace to individual kCVImageBuffer* keys.
bool ColorSpaceToCVImageBufferKeys(const gfx::ColorSpace& color_space,
                                   bool prefer_srgb_trfn,
                                   CFStringRef* out_primaries,
                                   CFStringRef* out_transfer,
                                   CFStringRef* out_matrix) {
  DCHECK(out_primaries);
  DCHECK(out_transfer);
  DCHECK(out_matrix);

  bool found_primary = false;
  for (const auto& primaries : GetSupportedImagePrimaries()) {
    if (primaries.id == color_space.GetPrimaryID()) {
      *out_primaries = primaries.cfstr_cv;
      found_primary = true;
      break;
    }
  }

  bool found_transfer = false;
  for (const auto& transfer : GetSupportedImageTransferFn()) {
    if (transfer.id == color_space.GetTransferID()) {
      *out_transfer = transfer.cfstr_cv;
      found_transfer = true;
      break;
    }
  }
  if (found_transfer && prefer_srgb_trfn) {
    if (*out_transfer == kCVImageBufferTransferFunction_ITU_R_709_2) {
      *out_transfer = kCVImageBufferTransferFunction_sRGB;
    }
  }

  bool found_matrix = false;
  for (const auto& matrix : GetSupportedImageMatrix()) {
    if (matrix.id == color_space.GetMatrixID()) {
      *out_matrix = matrix.cfstr_cv;
      found_matrix = true;
      break;
    }
  }

  return found_primary && found_transfer && found_matrix;
}

}  // namespace gfx
