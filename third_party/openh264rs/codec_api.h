// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_OPENH264RS_CODEC_API_H_
#define THIRD_PARTY_OPENH264RS_CODEC_API_H_

#include "third_party/openh264rs/src/src/api/cxx_api.rs.h"

using openh264rs::c_void;
using openh264rs::DECODER_OPTION;
using openh264rs::DECODING_STATE;
using openh264rs::ECOMPLEXITY_MODE;
using openh264rs::ELevelIdc;
using openh264rs::ENCODER_OPTION;
using openh264rs::EParameterSetStrategy;
using openh264rs::EProfileIdc;
using openh264rs::ERROR_CON_IDC;
using openh264rs::ESampleAspectRatio;
using openh264rs::EUsageType;
using openh264rs::EVideoFrameType;
using openh264rs::ISVCDecoder;
using openh264rs::ISVCEncoder;
using openh264rs::KEY_FRAME_REQUEST_TYPE;
using openh264rs::LAYER_NUM;
using openh264rs::OpenH264Version;
using openh264rs::RC_MODES;
using openh264rs::SBitrateInfo;
using openh264rs::SBufferInfo;
using openh264rs::SBufferInfoUsrData;
using openh264rs::SDecoderCapability;
using openh264rs::SDecoderStatistics;
using openh264rs::SDecodingParam;
using openh264rs::SDeliveryStatus;
using openh264rs::SEncoderStatistics;
using openh264rs::SEncParamBase;
using openh264rs::SEncParamExt;
using openh264rs::SFrameBSInfo;
using openh264rs::SLayerBSInfo;
using openh264rs::SLevelInfo;
using openh264rs::SliceModeEnum;
using openh264rs::SLTRConfig;
using openh264rs::SLTRMarkingFeedback;
using openh264rs::SLTRRecoverRequest;
using openh264rs::SParserBsInfo;
using openh264rs::SProfileInfo;
using openh264rs::SSliceArgument;
using openh264rs::SSourcePicture;
using openh264rs::SSpatialLayerConfig;
using openh264rs::SSysMEMBuffer;
using openh264rs::SVideoProperty;
using openh264rs::SVuiSarInfo;
using openh264rs::VIDEO_BITSTREAM_TYPE;
using openh264rs::WelsCreateDecoder;
using openh264rs::WelsCreateSVCEncoder;
using openh264rs::WelsDestroyDecoder;
using openh264rs::WelsDestroySVCEncoder;
using openh264rs::WelsGetCodecVersion;
using openh264rs::WelsGetCodecVersionEx;
using openh264rs::WelsGetDecoderCapability;
using openh264rs::WelsLogLevel;

using enum openh264rs::DECODER_OPTION;
using enum openh264rs::DECODING_STATE;
using enum openh264rs::ECOMPLEXITY_MODE;
using enum openh264rs::ELevelIdc;
using enum openh264rs::ENCODER_OPTION;
using enum openh264rs::EParameterSetStrategy;
using enum openh264rs::EProfileIdc;
using enum openh264rs::ERROR_CON_IDC;
using enum openh264rs::ESampleAspectRatio;
using enum openh264rs::EUsageType;
using enum openh264rs::EVideoFrameType;
using enum openh264rs::KEY_FRAME_REQUEST_TYPE;
using enum openh264rs::LAYER_NUM;
using enum openh264rs::RC_MODES;
using enum openh264rs::SliceModeEnum;
using enum openh264rs::VIDEO_BITSTREAM_TYPE;
using enum openh264rs::WelsLogLevel;

inline constexpr int cmResultSuccess =
    static_cast<int>(openh264rs::CM_RETURN::cmResultSuccess);

// Unscoped enum so qualified `EVideoFormatType::videoFormatI420` callers
// implicitly convert to `int`.
enum EVideoFormatType : int {
  videoFormatI420 =
      static_cast<int>(openh264rs::EVideoFormatType::videoFormatI420),
};

#endif  // THIRD_PARTY_OPENH264RS_CODEC_API_H_
