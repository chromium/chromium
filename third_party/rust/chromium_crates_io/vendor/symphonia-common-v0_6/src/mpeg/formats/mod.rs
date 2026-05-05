// Symphonia
// Copyright (c) 2019-2022 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use symphonia_core::codecs::CodecId;
use symphonia_core::errors::{Result, decode_error, unsupported_error};
use symphonia_core::io::{FiniteStream, ReadBytes, ScopedStream};

use log::debug;

/// The minimum size of an object descriptor (minimum header size).
pub const MIN_OBJECT_DESCRIPTOR_SIZE: u64 = 2;

/// Object descriptor tags as defined in ISO/IEC 14496-1.
#[non_exhaustive]
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum ClassTag {
    ObjectDescriptor,
    InitialObjectDescriptor,
    EsDescriptor,
    DecoderConfigDescriptor,
    DecoderSpecificInfo,
    SlConfigDescriptor,
    UserPrivate(u8),
    Unknown(u8),
}

/// Read an ISO/IEC 14496-1 Object Descriptor header and return the class tag and size.
pub fn read_object_descriptor_header<B: ReadBytes>(reader: &mut B) -> Result<(ClassTag, u64)> {
    let tag = match reader.read_u8()? {
        0x0 | 0xff => return decode_error("common (mp4): forbidden object descriptor tag"),
        0x1 => ClassTag::ObjectDescriptor,
        0x2 => ClassTag::InitialObjectDescriptor,
        0x3 => ClassTag::EsDescriptor,
        0x4 => ClassTag::DecoderConfigDescriptor,
        0x5 => ClassTag::DecoderSpecificInfo,
        0x6 => ClassTag::SlConfigDescriptor,
        user @ 0xc0..=0xfe => ClassTag::UserPrivate(user),
        other => ClassTag::Unknown(other),
    };

    let mut size = 0;

    for _ in 0..4 {
        let val = reader.read_u8()?;
        size = (size << 7) | u64::from(val & 0x7f);
        if val & 0x80 == 0 {
            break;
        }
    }

    Ok((tag, size))
}

/// Try to get a codec ID from from an object type indication.
pub fn codec_id_from_object_type_indication(obj_type: u8) -> Option<CodecId> {
    use symphonia_core::codecs::audio::well_known::{
        CODEC_ID_AAC, CODEC_ID_AC3, CODEC_ID_DCA, CODEC_ID_EAC3, CODEC_ID_MP3,
    };
    use symphonia_core::codecs::video::well_known::{
        CODEC_ID_H264, CODEC_ID_HEVC, CODEC_ID_MPEG2, CODEC_ID_MPEG4, CODEC_ID_VP9,
    };

    // AAC
    const OBJ_TYPE_AUDIO_MPEG4_3: u8 = 0x40; // Audio ISO/IEC 14496-3
    const OBJ_TYPE_AUDIO_MPEG2_7_MAIN: u8 = 0x66; // Audio ISO/IEC 13818-7 Main Profile
    const OBJ_TYPE_AUDIO_MPEG2_7_LC: u8 = 0x67; // Audio ISO/IEC 13818-7 Low Complexity

    // MP3
    const OBJ_TYPE_AUDIO_MPEG2_3: u8 = 0x69; // Audio ISO/IEC 13818-3 (MP3)
    const OBJ_TYPE_AUDIO_MPEG1_3: u8 = 0x6b; // Audio ISO/IEC 11172-3 (MP3)

    const OBJ_TYPE_AUDIO_AC3: u8 = 0xa5;
    const OBJ_TYPE_AUDIO_EAC3: u8 = 0xa6;
    const OBJ_TYPE_AUDIO_DTS: u8 = 0xa9;

    // MPEG2 video
    const OBJ_TYPE_VISUAL_MPEG2_2_SP: u8 = 0x60; // Visual ISO/IEC 13818-2 Simple Profile
    const OBJ_TYPE_VISUAL_MPEG2_2_MP: u8 = 0x61; // Visual ISO/IEC 13818-2 Main Profile
    const OBJ_TYPE_VISUAL_MPEG2_2_SNR: u8 = 0x62; // Visual ISO/IEC 13818-2 SNR Profile
    const OBJ_TYPE_VISUAL_MPEG2_2_SPATIAL: u8 = 0x63; // Visual ISO/IEC 13818-2 Spatial Profile
    const OBJ_TYPE_VISUAL_MPEG2_2_HP: u8 = 0x64; // Visual ISO/IEC 13818-2 High Profile
    const OBJ_TYPE_VISUAL_MPEG2_2_422: u8 = 0x65; // Visual ISO/IEC 13818-2 422 Profile

    // MPEG4 video
    const OBJ_TYPE_VISUAL_MPEG4_2: u8 = 0x20; // Visual ISO/IEC 14496-2

    // H264
    const OBJ_TYPE_VISUAL_AVC1: u8 = 0x21; // ISO/IEC 14496-10

    // HEVC
    const OBJ_TYPE_VISUAL_HEVC1: u8 = 0x23; // Visual ISO/IEC 23008-2

    // VP9
    const OBJ_TYPE_VISUAL_VP09: u8 = 0xb1;

    let codec_id = match obj_type {
        OBJ_TYPE_AUDIO_MPEG4_3 | OBJ_TYPE_AUDIO_MPEG2_7_LC | OBJ_TYPE_AUDIO_MPEG2_7_MAIN => {
            CodecId::Audio(CODEC_ID_AAC)
        }
        OBJ_TYPE_AUDIO_MPEG2_3 | OBJ_TYPE_AUDIO_MPEG1_3 => CodecId::Audio(CODEC_ID_MP3),
        OBJ_TYPE_AUDIO_AC3 => CodecId::Audio(CODEC_ID_AC3),
        OBJ_TYPE_AUDIO_EAC3 => CodecId::Audio(CODEC_ID_EAC3),
        OBJ_TYPE_AUDIO_DTS => CodecId::Audio(CODEC_ID_DCA),
        OBJ_TYPE_VISUAL_MPEG2_2_SP
        | OBJ_TYPE_VISUAL_MPEG2_2_MP
        | OBJ_TYPE_VISUAL_MPEG2_2_SNR
        | OBJ_TYPE_VISUAL_MPEG2_2_SPATIAL
        | OBJ_TYPE_VISUAL_MPEG2_2_HP
        | OBJ_TYPE_VISUAL_MPEG2_2_422 => CodecId::Video(CODEC_ID_MPEG2),
        OBJ_TYPE_VISUAL_MPEG4_2 => CodecId::Video(CODEC_ID_MPEG4),
        OBJ_TYPE_VISUAL_AVC1 => CodecId::Video(CODEC_ID_H264),
        OBJ_TYPE_VISUAL_HEVC1 => CodecId::Video(CODEC_ID_HEVC),
        OBJ_TYPE_VISUAL_VP09 => CodecId::Video(CODEC_ID_VP9),
        _ => {
            debug!("unknown object type indication {obj_type:#x} for decoder config descriptor");
            return None;
        }
    };

    Some(codec_id)
}

pub trait ObjectDescriptor: Sized {
    fn read<B: ReadBytes>(reader: &mut B, len: u64) -> Result<Self>;
}

/*
class ES_Descriptor extends BaseDescriptor : bit(8) tag=ES_DescrTag {
    bit(16) ES_ID;
    bit(1) streamDependenceFlag;
    bit(1) URL_Flag;
    bit(1) OCRstreamFlag;
    bit(5) streamPriority;
    if (streamDependenceFlag)
        bit(16) dependsOn_ES_ID;
    if (URL_Flag) {
        bit(8) URLlength;
        bit(8) URLstring[URLlength];
    }
    if (OCRstreamFlag)
        bit(16) OCR_ES_Id;
    DecoderConfigDescriptor decConfigDescr;
    SLConfigDescriptor slConfigDescr;
    IPI_DescrPointer ipiPtr[0 .. 1];
    IP_IdentificationDataSet ipIDS[0 .. 255];
    IPMP_DescriptorPointer ipmpDescrPtr[0 .. 255];
    LanguageDescriptor langDescr[0 .. 255];
    QoS_Descriptor qosDescr[0 .. 1];
    RegistrationDescriptor regDescr[0 .. 1];
    ExtensionDescriptor extDescr[0 .. 255];
}
*/

#[non_exhaustive]
#[derive(Debug)]
pub struct ESDescriptor {
    pub es_id: u16,
    pub dec_config: DecoderConfigDescriptor,
    pub sl_config: SLConfigDescriptor,
}

impl ObjectDescriptor for ESDescriptor {
    fn read<B: ReadBytes>(reader: &mut B, len: u64) -> Result<Self> {
        // ES Descriptor is an expandable object descriptor. All reads must be scoped to the length
        // defined in the header.
        let mut scoped = ScopedStream::new(reader, len);

        let es_id = scoped.read_be_u16()?;
        let es_flags = scoped.read_u8()?;

        // Stream dependence flag.
        if es_flags & 0x80 != 0 {
            let _depends_on_es_id = scoped.read_u16()?;
        }

        // URL flag.
        if es_flags & 0x40 != 0 {
            let url_len = scoped.read_u8()?;
            scoped.ignore_bytes(u64::from(url_len))?;
        }

        // OCR stream flag.
        if es_flags & 0x20 != 0 {
            let _ocr_es_id = scoped.read_u16()?;
        }

        let mut dec_config = None;
        let mut sl_config = None;

        // Multiple descriptors follow, but only the decoder configuration descriptor is useful.
        while scoped.bytes_available() > MIN_OBJECT_DESCRIPTOR_SIZE {
            let (tag, desc_len) = read_object_descriptor_header(&mut scoped)?;

            match tag {
                ClassTag::DecoderConfigDescriptor => {
                    dec_config = Some(DecoderConfigDescriptor::read(&mut scoped, desc_len)?);
                }
                ClassTag::SlConfigDescriptor => {
                    sl_config = Some(SLConfigDescriptor::read(&mut scoped, desc_len)?);
                }
                other => {
                    debug!("skipping {other:?} object in es descriptor");
                    scoped.ignore_bytes(desc_len)?;
                }
            }
        }

        // Consume remaining bytes.
        scoped.ignore()?;

        // Decoder configuration descriptor is mandatory.
        if dec_config.is_none() {
            return decode_error("common (mp4): missing decoder config descriptor");
        }

        // SL descriptor is mandatory.
        if sl_config.is_none() {
            return decode_error("common (mp4): missing sl config descriptor");
        }

        Ok(ESDescriptor { es_id, dec_config: dec_config.unwrap(), sl_config: sl_config.unwrap() })
    }
}

/*
class DecoderConfigDescriptor extends BaseDescriptor : bit(8) tag=DecoderConfigDescrTag {
    bit(8) objectTypeIndication;
    bit(6) streamType;
    bit(1) upStream;
    const bit(1) reserved=1;
    bit(24) bufferSizeDB;
    bit(32) maxBitrate;
    bit(32) avgBitrate;
    DecoderSpecificInfo decSpecificInfo[0 .. 1];
    profileLevelIndicationIndexDescriptor profileLevelIndicationIndexDescr [0..255];
}
*/

#[non_exhaustive]
#[derive(Debug)]
pub struct DecoderConfigDescriptor {
    pub object_type_indication: u8,
    pub dec_specific_info: Option<DecoderSpecificInfo>,
}

impl ObjectDescriptor for DecoderConfigDescriptor {
    fn read<B: ReadBytes>(reader: &mut B, len: u64) -> Result<Self> {
        // Decoder Config Descriptor is an expandable object descriptor. All reads must be scoped to
        // the length defined in the header.
        let mut scoped = ScopedStream::new(reader, len);

        let object_type_indication = scoped.read_u8()?;

        let (_stream_type, _upstream) = {
            let val = scoped.read_u8()?;

            if val & 0x1 != 1 {
                debug!("decoder config descriptor reserved bit is not 1");
            }

            ((val & 0xfc) >> 2, (val & 0x2) >> 1)
        };

        let _buffer_size = scoped.read_be_u24()?;
        let _max_bitrate = scoped.read_be_u32()?;
        let _avg_bitrate = scoped.read_be_u32()?;

        let mut dec_specific_config = None;

        // Multiple descriptors follow, but only the decoder specific info descriptor is useful.
        while scoped.bytes_available() > MIN_OBJECT_DESCRIPTOR_SIZE {
            let (tag, desc_len) = read_object_descriptor_header(&mut scoped)?;

            match tag {
                ClassTag::DecoderSpecificInfo => {
                    dec_specific_config = Some(DecoderSpecificInfo::read(&mut scoped, desc_len)?);
                }
                other => {
                    debug!("skipping {other:?} object in decoder config descriptor");
                    scoped.ignore_bytes(desc_len)?;
                }
            }
        }

        // Consume remaining bytes.
        scoped.ignore()?;

        Ok(DecoderConfigDescriptor {
            object_type_indication,
            dec_specific_info: dec_specific_config,
        })
    }
}

#[non_exhaustive]
#[derive(Debug)]
pub struct DecoderSpecificInfo {
    pub extra_data: Box<[u8]>,
}

impl ObjectDescriptor for DecoderSpecificInfo {
    fn read<B: ReadBytes>(reader: &mut B, len: u64) -> Result<Self> {
        Ok(DecoderSpecificInfo { extra_data: reader.read_boxed_slice_exact(len as usize)? })
    }
}

/*
class SLConfigDescriptor extends BaseDescriptor : bit(8) tag=SLConfigDescrTag {
    bit(8) predefined;
    if (predefined==0) {
        bit(1) useAccessUnitStartFlag;
        bit(1) useAccessUnitEndFlag;
        bit(1) useRandomAccessPointFlag;
        bit(1) hasRandomAccessUnitsOnlyFlag;
        bit(1) usePaddingFlag;
        bit(1) useTimeStampsFlag;
        bit(1) useIdleFlag;
        bit(1) durationFlag;
        bit(32) timeStampResolution;
        bit(32) OCRResolution;
        bit(8) timeStampLength;           // Must be <= 64.
        bit(8) OCRLength;                 // Must be <= 64.
        bit(8) AU_Length;                 // Must be <= 32.
        bit(8) instantBitrateLength;
        bit(4) degradationPriorityLength;
        bit(5) AU_seqNumLength;           // Must be <= 16.
        bit(5) packetSeqNumLength;        // Must be <= 16.
        bit(2) reserved=0b11;
    }
    if (durationFlag) {
        bit(32) timeScale;
        bit(16) accessUnitDuration;
        bit(16) compositionUnitDuration;
    }
    if (!useTimeStampsFlag) {
        bit(timeStampLength) startDecodingTimeStamp;
        bit(timeStampLength) startCompositionTimeStamp;
    }
}
*/

#[non_exhaustive]
#[derive(Debug)]
pub struct SLConfigDescriptor;

impl ObjectDescriptor for SLConfigDescriptor {
    fn read<B: ReadBytes>(reader: &mut B, len: u64) -> Result<Self> {
        const PREDEFINED_CUSTOM: u8 = 0x0;
        const PREDEFINED_NULL: u8 = 0x1;
        const PREDEFINED_MP4: u8 = 0x2;

        // Ensure no reads extend beyond the SL Config Descriptor length as defined in the object
        // descriptor header.
        let mut scoped = ScopedStream::new(reader, len);

        // Ensure the predefined field is valid.
        match scoped.read_u8()? {
            PREDEFINED_CUSTOM | PREDEFINED_NULL | PREDEFINED_MP4 => (),
            _ => {
                return unsupported_error("common (mp4): invalid sl config descriptor predefined");
            }
        };

        // Consume remaining bytes.
        scoped.ignore()?;

        Ok(SLConfigDescriptor {})
    }
}
