// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::collections::HashMap;

use crate::ebml::{EbmlDataType, EbmlElementInfo, EbmlSchema};

use lazy_static::lazy_static;

/// MKV element type enumeration.
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord)]
pub(crate) enum MkvElement {
    #[default]
    Unknown,
    Void,
    Crc32,
    Ebml,
    DocType,
    DocTypeExtension,
    DocTypeExtensionName,
    DocTypeExtensionVersion,
    DocTypeReadVersion,
    DocTypeVersion,
    EbmlMaxIdLength,
    EbmlMaxSizeLength,
    EbmlReadVersion,
    EbmlVersion,
    Segment,
    Attachments,
    AttachedFile,
    FileData,
    FileDescription,
    FileMediaType,
    FileName,
    FileReferral, // Deprecated v0
    FileUid,
    FileUsedEndTime,   // Deprecated v0
    FileUsedStartTime, // Deprecated v0
    Chapters,
    EditionEntry,
    ChapterAtom,
    ChapProcess,
    ChapProcessCodecId,
    ChapProcessCommand,
    ChapProcessData,
    ChapProcessTime,
    ChapProcessPrivate,
    ChapterDisplay,
    ChapCountry,
    ChapLanguage,
    ChapLanguageBcp47,
    ChapString,
    ChapterFlagEnabled,
    ChapterFlagHidden,
    ChapterPhysicalEquiv,
    ChapterSegmentEditionUid,
    ChapterSegmentUuid,
    ChapterSkipType,
    ChapterStringUid,
    ChapterTimeEnd,
    ChapterTimeStart,
    ChapterTrack,
    ChapterTrackUid,
    ChapterUid,
    EditionDisplay,
    EditionLanguageIetf,
    EditionString,
    EditionFlagDefault,
    EditionFlagHidden,
    EditionFlagOrdered,
    EditionUid,
    Cluster,
    BlockGroup,
    Block,
    BlockAdditions,
    BlockMore,
    BlockAddId,
    BlockAdditional,
    BlockDuration,
    BlockVirtual, // Deprecated v0
    CodecState,
    DiscardPadding,
    ReferenceBlock,
    ReferenceFrame,     // Deprecated v0
    ReferenceOffset,    // Deprecated v0
    ReferenceTimestamp, // Deprecated v0
    ReferencePriority,
    ReferenceVirtual, // Deprecated v0
    Slices,           // Deprecated v0
    TimeSlice,        // Deprecated v0
    BlockAdditionId,  // Deprecated v0
    Delay,            // Deprecated v0
    FrameNumber,      // Deprecated v0
    LaceNumber,       // Deprecated v0
    SliceDuration,    // Deprecated v0
    EncryptedBlock,   // Deprecated v0
    Position,         // Deprecated v4
    PrevSize,
    SilentTracks,      // Deprecated v0
    SilentTrackNumber, // Deprecated v0
    SimpleBlock,
    Timestamp,
    Cues,
    CuePoint,
    CueTime,
    CueTrackPositions,
    CueBlockNumber,
    CueClusterPosition,
    CueCodecState,
    CueDuration,
    CueReference,
    CueRefCluster,    // Deprecated v0
    CueRefCodecState, // Deprecated v0
    CueRefNumber,     // Deprecated v0
    CueRefTime,
    CueRelativePosition,
    CueTrack,
    Info,
    ChapterTranslate,
    ChapterTranslateCodec,
    ChapterTranslateEditionUid,
    ChapterTranslateId,
    DateUtc,
    Duration,
    MuxingApp,
    NextFilename,
    NextUuid,
    PrevFilename,
    PrevUuid,
    SegmentFamily,
    SegmentFilename,
    SegmentUuid,
    TimestampScale,
    Title,
    WritingApp,
    SeekHead,
    Seek,
    SeekId,
    SeekPosition,
    Tags,
    Tag,
    SimpleTag,
    TagBinary,
    TagDefault,
    TagDefaultBogus, // Deprecated v0
    TagLanguage,
    TagLanguageBcp47,
    TagName,
    TagString,
    Targets,
    TagAttachmentUid,
    TagChapterUid,
    TagEditionUid,
    TagTrackUid,
    TargetType,
    TargetTypeValue,
    Tracks,
    TrackEntry,
    AttachmentLink, // Deprecated v3
    Audio,
    BitDepth,
    ChannelPositions, // Deprecated v0
    Channels,
    Emphasis,
    OutputSamplingFrequency,
    SamplingFrequency,
    BlockAdditionMapping,
    BlockAddIdExtraData,
    BlockAddIdName,
    BlockAddIdType,
    BlockAddIdValue,
    CodecDecodeAll, // Deprecated v0
    CodecDelay,
    CodecDownloadUrl, // Deprecated v0
    CodecId,
    CodecInfoUrl, // Deprecated v0
    CodecName,
    CodecPrivate,
    CodecSettings, // Deprecated v0
    ContentEncodings,
    ContentEncoding,
    ContentCompression,
    ContentCompAlgo,
    ContentCompSettings,
    ContentEncodingOrder,
    ContentEncodingScope,
    ContentEncodingType,
    ContentEncryption,
    ContentEncAesSettings,
    AesSettingsCipherMode,
    ContentEncAlgo,
    ContentEncKeyId,
    ContentSigAlgo,     // Deprecated v0
    ContentSigHashAlgo, // Deprecated v0
    ContentSigKeyId,    // Deprecated v0
    ContentSignature,   // Deprecated v0
    DefaultDecodedFieldDuration,
    DefaultDuration,
    FlagCommentary,
    FlagDefault,
    FlagEnabled,
    FlagForced,
    FlagHearingImpaired,
    FlagLacing,
    FlagOriginal,
    FlagTextDescriptions,
    FlagVisualImpaired,
    Language,
    LanguageBcp47,
    MaxBlockAdditionId,
    MaxCache, // Deprecated v0
    MinCache, // Deprecated v0
    Name,
    SeekPreRoll,
    TrackNumber,
    TrackOffset, // Deprecated v0
    TrackOperation,
    TrackCombinePlanes,
    TrackPlane,
    TrackPlaneType,
    TrackPlaneUid,
    TrackJoinBlocks,
    TrackJoinUid,
    TrackOverlay,        // Deprecated v0
    TrackTimestampScale, // Deprecated v3
    TrackTranslate,
    TrackTranslateCodec,
    TrackTranslateEditionUid,
    TrackTranslateTrackId,
    TrackType,
    TrackUid,
    TrickMasterTrackSegmentUid, // Deprecated v0
    TrickMasterTrackUid,        // Deprecated v0
    TrickTrackFlag,             // Deprecated v0
    TrickTrackSegmentUid,       // Deprecated v0
    TrickTrackUid,              // Deprecated v0
    Video,
    AlphaMode,
    AspectRatioType, // Deprecated v0
    Colour,
    BitsPerChannel,
    CbSubsamplingHorz,
    CbSubsamplingVert,
    ChromaSitingHorz,
    ChromaSitingVert,
    ChromaSubsamplingHorz,
    ChromaSubsamplingVert,
    MasteringMetadata,
    LuminanceMax,
    LuminanceMin,
    PrimaryBChromaticityX,
    PrimaryBChromaticityY,
    PrimaryGChromaticityX,
    PrimaryGChromaticityY,
    PrimaryRChromaticityX,
    PrimaryRChromaticityY,
    WhitePointChromaticityX,
    WhitePointChromaticityY,
    MatrixCoefficients,
    MaxCll,
    MaxFall,
    Primaries,
    Range,
    TransferCharacteristics,
    DisplayHeight,
    DisplayUnit,
    DisplayWidth,
    FieldOrder,
    FlagInterlaced,
    FrameRate,     // Deprecated v0
    GammaValue,    // Deprecated v0
    OldStereoMode, // Deprecated v2
    PixelCropBottom,
    PixelCropLeft,
    PixelCropRight,
    PixelCropTop,
    PixelHeight,
    PixelWidth,
    Projection,
    ProjectionPosePitch,
    ProjectionPoseRoll,
    ProjectionPoseYaw,
    ProjectionPrivate,
    ProjectionType,
    StereoMode,
    UncompressedFourCc,
}

/// The element is global.
const EBML_GLOBAL: u8 = 1 << 7;
/// The element may be nested in itself.
const EBML_RECURSIVE: u8 = 1 << 6;
/// The element may be an unknown size.
const EBML_UNKNOWN: u8 = 1 << 5;
/// Mask of flag bits storing element min. depth.
const EBML_DEPTH_MASK: u8 = 0xf;

/// MKV element information.
///
/// 4-tuple consisting of element type, element data type, flag bits, & element ID. 8 bytes/element.
#[derive(Copy, Clone)]
pub struct MkvElementInfo(MkvElement, EbmlDataType, u8, u32);

impl EbmlElementInfo for MkvElementInfo {
    type ElementType = MkvElement;

    fn element_type(&self) -> Self::ElementType {
        self.0
    }

    fn data_type(&self) -> EbmlDataType {
        self.1
    }

    fn min_depth(&self) -> u8 {
        self.2 & EBML_DEPTH_MASK
    }

    fn is_global(&self) -> bool {
        self.2 & EBML_GLOBAL == EBML_GLOBAL
    }

    fn is_recursive(&self) -> bool {
        self.2 & EBML_RECURSIVE == EBML_RECURSIVE
    }

    fn allow_unknown_size(&self) -> bool {
        self.2 & EBML_UNKNOWN == EBML_UNKNOWN
    }

    fn parent_id(&self) -> u32 {
        self.3
    }
}

lazy_static! {
    static ref MKV_SCHEMA: HashMap<u32, MkvElementInfo> = {
        use MkvElementInfo as E;
        let mut e = HashMap::new();
        // Global
        e.insert(0xec, E(MkvElement::Void, EbmlDataType::Binary, EBML_GLOBAL | 0, 0x0));
        e.insert(0xbf, E(MkvElement::Crc32, EbmlDataType::Binary, EBML_GLOBAL | 1, 0x0));
        // \EBML
        e.insert(0x1a45dfa3, E(MkvElement::Ebml, EbmlDataType::Master, 0, 0x0));
        e.insert(0x4282, E(MkvElement::DocType, EbmlDataType::String, 1, 0x1a45dfa3));
        // \EBML\DocTypeExtension
        e.insert(0x4281, E(MkvElement::DocTypeExtension, EbmlDataType::Master, 1, 0x1a45dfa3));
        e.insert(0x4283, E(MkvElement::DocTypeExtensionName, EbmlDataType::String, 2, 0x4281));
        e.insert(0x4284, E(MkvElement::DocTypeExtensionVersion, EbmlDataType::Unsigned, 2, 0x4281));
        e.insert(0x4285, E(MkvElement::DocTypeReadVersion, EbmlDataType::Unsigned, 1, 0x1a45dfa3));
        e.insert(0x4287, E(MkvElement::DocTypeVersion, EbmlDataType::Unsigned, 1, 0x1a45dfa3));
        e.insert(0x42f2, E(MkvElement::EbmlMaxIdLength, EbmlDataType::Unsigned, 1, 0x1a45dfa3));
        e.insert(0x42f3, E(MkvElement::EbmlMaxSizeLength, EbmlDataType::Unsigned, 1, 0x1a45dfa3));
        e.insert(0x42f7, E(MkvElement::EbmlReadVersion, EbmlDataType::Unsigned, 1, 0x1a45dfa3));
        e.insert(0x4286, E(MkvElement::EbmlVersion, EbmlDataType::Unsigned, 1, 0x1a45dfa3));
        // \Segment
        e.insert(0x18538067, E(MkvElement::Segment, EbmlDataType::Master, EBML_UNKNOWN | 0, 0x0));
        // \Segment\Attachments
        e.insert(0x1941a469, E(MkvElement::Attachments, EbmlDataType::Master, 1, 0x18538067));
        e.insert(0x61a7, E(MkvElement::AttachedFile, EbmlDataType::Master, 2, 0x1941a469));
        e.insert(0x465c, E(MkvElement::FileData, EbmlDataType::Binary, 3, 0x61a7));
        e.insert(0x467e, E(MkvElement::FileDescription, EbmlDataType::String, 3, 0x61a7));
        e.insert(0x4660, E(MkvElement::FileMediaType, EbmlDataType::String, 3, 0x61a7));
        e.insert(0x466e, E(MkvElement::FileName, EbmlDataType::String, 3, 0x61a7));
        e.insert(0x4675, E(MkvElement::FileReferral, EbmlDataType::Binary, 3, 0x61a7));
        e.insert(0x46ae, E(MkvElement::FileUid, EbmlDataType::Unsigned, 3, 0x61a7));
        e.insert(0x4662, E(MkvElement::FileUsedEndTime, EbmlDataType::Unsigned, 3, 0x61a7));
        e.insert(0x4661, E(MkvElement::FileUsedStartTime, EbmlDataType::Unsigned, 3, 0x61a7));
        // \Segment\Chapters
        e.insert(0x1043a770, E(MkvElement::Chapters, EbmlDataType::Master, 1, 0x18538067));
        e.insert(0x45b9, E(MkvElement::EditionEntry, EbmlDataType::Master, 2, 0x1043a770));
        e.insert(0xb6, E(MkvElement::ChapterAtom, EbmlDataType::Master, EBML_RECURSIVE | 3, 0x45b9));
        e.insert(0x6944, E(MkvElement::ChapProcess, EbmlDataType::Master, 4, 0xb6));
        e.insert(0x6955, E(MkvElement::ChapProcessCodecId, EbmlDataType::Unsigned, 5, 0x6944));
        e.insert(0x6911, E(MkvElement::ChapProcessCommand, EbmlDataType::Master, 5, 0x6944));
        e.insert(0x6933, E(MkvElement::ChapProcessData, EbmlDataType::Binary, 6, 0x6911));
        e.insert(0x6922, E(MkvElement::ChapProcessTime, EbmlDataType::Unsigned, 6, 0x6911));
        e.insert(0x450d, E(MkvElement::ChapProcessPrivate, EbmlDataType::Binary, 5, 0x6944));
        e.insert(0x80, E(MkvElement::ChapterDisplay, EbmlDataType::Master, 4, 0xb6));
        e.insert(0x437e, E(MkvElement::ChapCountry, EbmlDataType::String, 5, 0x80));
        e.insert(0x437c, E(MkvElement::ChapLanguage, EbmlDataType::String, 5, 0x80));
        e.insert(0x437d, E(MkvElement::ChapLanguageBcp47, EbmlDataType::String, 5, 0x80));
        e.insert(0x85, E(MkvElement::ChapString, EbmlDataType::String, 5, 0x80));
        e.insert(0x4598, E(MkvElement::ChapterFlagEnabled, EbmlDataType::Unsigned, 4, 0xb6));
        e.insert(0x98, E(MkvElement::ChapterFlagHidden, EbmlDataType::Unsigned, 4, 0xb6));
        e.insert(0x63c3, E(MkvElement::ChapterPhysicalEquiv, EbmlDataType::Unsigned, 4, 0xb6));
        e.insert(0x6ebc, E(MkvElement::ChapterSegmentEditionUid, EbmlDataType::Unsigned, 4, 0xb6));
        e.insert(0x6e67, E(MkvElement::ChapterSegmentUuid, EbmlDataType::Binary, 4, 0xb6));
        e.insert(0x4588, E(MkvElement::ChapterSkipType, EbmlDataType::Unsigned, 4, 0xb6));
        e.insert(0x5654, E(MkvElement::ChapterStringUid, EbmlDataType::String, 4, 0xb6));
        e.insert(0x92, E(MkvElement::ChapterTimeEnd, EbmlDataType::Unsigned, 4, 0xb6));
        e.insert(0x91, E(MkvElement::ChapterTimeStart, EbmlDataType::Unsigned, 4, 0xb6));
        e.insert(0x8f, E(MkvElement::ChapterTrack, EbmlDataType::Master, 4, 0xb6));
        e.insert(0x89, E(MkvElement::ChapterTrackUid, EbmlDataType::Unsigned, 5, 0x8f));
        e.insert(0x73c4, E(MkvElement::ChapterUid, EbmlDataType::Unsigned, 4, 0xb6));
        e.insert(0x4520, E(MkvElement::EditionDisplay, EbmlDataType::Master, 3, 0x45b9));
        e.insert(0x45e4, E(MkvElement::EditionLanguageIetf, EbmlDataType::String, 4, 0x4520));
        e.insert(0x4521, E(MkvElement::EditionString, EbmlDataType::String, 4, 0x4520));
        e.insert(0x45db, E(MkvElement::EditionFlagDefault, EbmlDataType::Unsigned, 3, 0x45b9));
        e.insert(0x45bd, E(MkvElement::EditionFlagHidden, EbmlDataType::Unsigned, 3, 0x45b9));
        e.insert(0x45dd, E(MkvElement::EditionFlagOrdered, EbmlDataType::Unsigned, 3, 0x45b9));
        e.insert(0x45bc, E(MkvElement::EditionUid, EbmlDataType::Unsigned, 3, 0x45b9));
        // \Segment\Cluster
        e.insert(0x1f43b675, E(MkvElement::Cluster, EbmlDataType::Master, EBML_UNKNOWN | 1, 0x18538067));
        e.insert(0xa0, E(MkvElement::BlockGroup, EbmlDataType::Master, 2, 0x1f43b675));
        e.insert(0xa1, E(MkvElement::Block, EbmlDataType::Binary, 3, 0xa0));
        e.insert(0x75a1, E(MkvElement::BlockAdditions, EbmlDataType::Master, 3, 0xa0));
        e.insert(0xa6, E(MkvElement::BlockMore, EbmlDataType::Master, 4, 0x75a1));
        e.insert(0xee, E(MkvElement::BlockAddId, EbmlDataType::Unsigned, 5, 0xa6));
        e.insert(0xa5, E(MkvElement::BlockAdditional, EbmlDataType::Binary, 5, 0xa6));
        e.insert(0x9b, E(MkvElement::BlockDuration, EbmlDataType::Unsigned, 3, 0xa0));
        e.insert(0xa2, E(MkvElement::BlockVirtual, EbmlDataType::Binary, 3, 0xa0));
        e.insert(0xa4, E(MkvElement::CodecState, EbmlDataType::Binary, 3, 0xa0));
        e.insert(0x75a2, E(MkvElement::DiscardPadding, EbmlDataType::Signed, 3, 0xa0));
        e.insert(0xfb, E(MkvElement::ReferenceBlock, EbmlDataType::Signed, 3, 0xa0));
        e.insert(0xc8, E(MkvElement::ReferenceFrame, EbmlDataType::Master, 3, 0xa0));
        e.insert(0xc9, E(MkvElement::ReferenceOffset, EbmlDataType::Unsigned, 4, 0xc8));
        e.insert(0xca, E(MkvElement::ReferenceTimestamp, EbmlDataType::Unsigned, 4, 0xc8));
        e.insert(0xfa, E(MkvElement::ReferencePriority, EbmlDataType::Unsigned, 3, 0xa0));
        e.insert(0xfd, E(MkvElement::ReferenceVirtual, EbmlDataType::Signed, 3, 0xa0));
        e.insert(0x8e, E(MkvElement::Slices, EbmlDataType::Master, 3, 0xa0));
        e.insert(0xe8, E(MkvElement::TimeSlice, EbmlDataType::Master, 4, 0x8e));
        e.insert(0xcb, E(MkvElement::BlockAdditionId, EbmlDataType::Unsigned, 5, 0xe8));
        e.insert(0xce, E(MkvElement::Delay, EbmlDataType::Unsigned, 5, 0xe8));
        e.insert(0xcd, E(MkvElement::FrameNumber, EbmlDataType::Unsigned, 5, 0xe8));
        e.insert(0xcc, E(MkvElement::LaceNumber, EbmlDataType::Unsigned, 5, 0xe8));
        e.insert(0xcf, E(MkvElement::SliceDuration, EbmlDataType::Unsigned, 5, 0xe8));
        e.insert(0xaf, E(MkvElement::EncryptedBlock, EbmlDataType::Binary, 2, 0x1f43b675));
        e.insert(0xa7, E(MkvElement::Position, EbmlDataType::Unsigned, 2, 0x1f43b675));
        e.insert(0xab, E(MkvElement::PrevSize, EbmlDataType::Unsigned, 2, 0x1f43b675));
        e.insert(0x5854, E(MkvElement::SilentTracks, EbmlDataType::Master, 2, 0x1f43b675));
        e.insert(0x58d7, E(MkvElement::SilentTrackNumber, EbmlDataType::Unsigned, 3, 0x5854));
        e.insert(0xa3, E(MkvElement::SimpleBlock, EbmlDataType::Binary, 2, 0x1f43b675));
        e.insert(0xe7, E(MkvElement::Timestamp, EbmlDataType::Unsigned, 2, 0x1f43b675));
        // \Segment\Cues
        e.insert(0x1c53bb6b, E(MkvElement::Cues, EbmlDataType::Master, 1, 0x18538067));
        e.insert(0xbb, E(MkvElement::CuePoint, EbmlDataType::Master, 2, 0x1c53bb6b));
        e.insert(0xb3, E(MkvElement::CueTime, EbmlDataType::Unsigned, 3, 0xbb));
        e.insert(0xb7, E(MkvElement::CueTrackPositions, EbmlDataType::Master, 3, 0xbb));
        e.insert(0x5378, E(MkvElement::CueBlockNumber, EbmlDataType::Unsigned, 4, 0xb7));
        e.insert(0xf1, E(MkvElement::CueClusterPosition, EbmlDataType::Unsigned, 4, 0xb7));
        e.insert(0xea, E(MkvElement::CueCodecState, EbmlDataType::Unsigned, 4, 0xb7));
        e.insert(0xb2, E(MkvElement::CueDuration, EbmlDataType::Unsigned, 4, 0xb7));
        e.insert(0xdb, E(MkvElement::CueReference, EbmlDataType::Master, 4, 0xb7));
        e.insert(0x97, E(MkvElement::CueRefCluster, EbmlDataType::Unsigned, 5, 0xdb));
        e.insert(0xeb, E(MkvElement::CueRefCodecState, EbmlDataType::Unsigned, 5, 0xdb));
        e.insert(0x535f, E(MkvElement::CueRefNumber, EbmlDataType::Unsigned, 5, 0xdb));
        e.insert(0x96, E(MkvElement::CueRefTime, EbmlDataType::Unsigned, 5, 0xdb));
        e.insert(0xf0, E(MkvElement::CueRelativePosition, EbmlDataType::Unsigned, 4, 0xb7));
        e.insert(0xf7, E(MkvElement::CueTrack, EbmlDataType::Unsigned, 4, 0xb7));
        // \Segment\Info
        e.insert(0x1549a966, E(MkvElement::Info, EbmlDataType::Master, 1, 0x18538067));
        e.insert(0x6924, E(MkvElement::ChapterTranslate, EbmlDataType::Master, 2, 0x1549a966));
        e.insert(0x69bf, E(MkvElement::ChapterTranslateCodec, EbmlDataType::Unsigned, 3, 0x6924));
        e.insert(0x69fc, E(MkvElement::ChapterTranslateEditionUid, EbmlDataType::Unsigned, 3, 0x6924));
        e.insert(0x69a5, E(MkvElement::ChapterTranslateId, EbmlDataType::Binary, 3, 0x6924));
        e.insert(0x4461, E(MkvElement::DateUtc, EbmlDataType::Date, 2, 0x1549a966));
        e.insert(0x4489, E(MkvElement::Duration, EbmlDataType::Float, 2, 0x1549a966));
        e.insert(0x4d80, E(MkvElement::MuxingApp, EbmlDataType::String, 2, 0x1549a966));
        e.insert(0x3e83bb, E(MkvElement::NextFilename, EbmlDataType::String, 2, 0x1549a966));
        e.insert(0x3eb923, E(MkvElement::NextUuid, EbmlDataType::Binary, 2, 0x1549a966));
        e.insert(0x3c83ab, E(MkvElement::PrevFilename, EbmlDataType::String, 2, 0x1549a966));
        e.insert(0x3cb923, E(MkvElement::PrevUuid, EbmlDataType::Binary, 2, 0x1549a966));
        e.insert(0x4444, E(MkvElement::SegmentFamily, EbmlDataType::Binary, 2, 0x1549a966));
        e.insert(0x7384, E(MkvElement::SegmentFilename, EbmlDataType::String, 2, 0x1549a966));
        e.insert(0x73a4, E(MkvElement::SegmentUuid, EbmlDataType::Binary, 2, 0x1549a966));
        e.insert(0x2ad7b1, E(MkvElement::TimestampScale, EbmlDataType::Unsigned, 2, 0x1549a966));
        e.insert(0x7ba9, E(MkvElement::Title, EbmlDataType::String, 2, 0x1549a966));
        e.insert(0x5741, E(MkvElement::WritingApp, EbmlDataType::String, 2, 0x1549a966));
        // \Segment\SeekHead
        e.insert(0x114d9b74, E(MkvElement::SeekHead, EbmlDataType::Master, 1, 0x18538067));
        e.insert(0x4dbb, E(MkvElement::Seek, EbmlDataType::Master, 2, 0x114d9b74));
        e.insert(0x53ab, E(MkvElement::SeekId, EbmlDataType::Binary, 3, 0x4dbb));
        e.insert(0x53ac, E(MkvElement::SeekPosition, EbmlDataType::Unsigned, 3, 0x4dbb));
        // \Segment\Tags
        e.insert(0x1254c367, E(MkvElement::Tags, EbmlDataType::Master, 1, 0x18538067));
        e.insert(0x7373, E(MkvElement::Tag, EbmlDataType::Master, 2, 0x1254c367));
        e.insert(0x67c8, E(MkvElement::SimpleTag, EbmlDataType::Master, EBML_RECURSIVE | 3, 0x7373));
        e.insert(0x4485, E(MkvElement::TagBinary, EbmlDataType::Binary, 4, 0x67c8));
        e.insert(0x4484, E(MkvElement::TagDefault, EbmlDataType::Unsigned, 4, 0x67c8));
        e.insert(0x44b4, E(MkvElement::TagDefaultBogus, EbmlDataType::Unsigned, 4, 0x67c8));
        e.insert(0x447a, E(MkvElement::TagLanguage, EbmlDataType::String, 4, 0x67c8));
        e.insert(0x447b, E(MkvElement::TagLanguageBcp47, EbmlDataType::String, 4, 0x67c8));
        e.insert(0x45a3, E(MkvElement::TagName, EbmlDataType::String, 4, 0x67c8));
        e.insert(0x4487, E(MkvElement::TagString, EbmlDataType::String, 4, 0x67c8));
        e.insert(0x63c0, E(MkvElement::Targets, EbmlDataType::Master, 3, 0x7373));
        e.insert(0x63c6, E(MkvElement::TagAttachmentUid, EbmlDataType::Unsigned, 4, 0x63c0));
        e.insert(0x63c4, E(MkvElement::TagChapterUid, EbmlDataType::Unsigned, 4, 0x63c0));
        e.insert(0x63c9, E(MkvElement::TagEditionUid, EbmlDataType::Unsigned, 4, 0x63c0));
        e.insert(0x63c5, E(MkvElement::TagTrackUid, EbmlDataType::Unsigned, 4, 0x63c0));
        e.insert(0x63ca, E(MkvElement::TargetType, EbmlDataType::String, 4, 0x63c0));
        e.insert(0x68ca, E(MkvElement::TargetTypeValue, EbmlDataType::Unsigned, 4, 0x63c0));
        // \Segment\Tracks
        e.insert(0x1654ae6b, E(MkvElement::Tracks, EbmlDataType::Master, 1, 0x18538067));
        e.insert(0xae, E(MkvElement::TrackEntry, EbmlDataType::Master, 2, 0x1654ae6b));
        e.insert(0x7446, E(MkvElement::AttachmentLink, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0xe1, E(MkvElement::Audio, EbmlDataType::Master, 3, 0xae));
        e.insert(0x6264, E(MkvElement::BitDepth, EbmlDataType::Unsigned, 4, 0xe1));
        e.insert(0x7d7b, E(MkvElement::ChannelPositions, EbmlDataType::Binary, 4, 0xe1));
        e.insert(0x9f, E(MkvElement::Channels, EbmlDataType::Unsigned, 4, 0xe1));
        e.insert(0x52f1, E(MkvElement::Emphasis, EbmlDataType::Unsigned, 4, 0xe1));
        e.insert(0x78b5, E(MkvElement::OutputSamplingFrequency, EbmlDataType::Float, 4, 0xe1));
        e.insert(0xb5, E(MkvElement::SamplingFrequency, EbmlDataType::Float, 4, 0xe1));
        e.insert(0x41e4, E(MkvElement::BlockAdditionMapping, EbmlDataType::Master, 3, 0xae));
        e.insert(0x41ed, E(MkvElement::BlockAddIdExtraData, EbmlDataType::Binary, 4, 0x41e4));
        e.insert(0x41a4, E(MkvElement::BlockAddIdName, EbmlDataType::String, 4, 0x41e4));
        e.insert(0x41e7, E(MkvElement::BlockAddIdType, EbmlDataType::Unsigned, 4, 0x41e4));
        e.insert(0x41f0, E(MkvElement::BlockAddIdValue, EbmlDataType::Unsigned, 4, 0x41e4));
        e.insert(0xaa, E(MkvElement::CodecDecodeAll, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0x56aa, E(MkvElement::CodecDelay, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0x26b240, E(MkvElement::CodecDownloadUrl, EbmlDataType::String, 3, 0xae));
        e.insert(0x86, E(MkvElement::CodecId, EbmlDataType::String, 3, 0xae));
        e.insert(0x3b4040, E(MkvElement::CodecInfoUrl, EbmlDataType::String, 3, 0xae));
        e.insert(0x258688, E(MkvElement::CodecName, EbmlDataType::String, 3, 0xae));
        e.insert(0x63a2, E(MkvElement::CodecPrivate, EbmlDataType::Binary, 3, 0xae));
        e.insert(0x3a9697, E(MkvElement::CodecSettings, EbmlDataType::String, 3, 0xae));
        e.insert(0x6d80, E(MkvElement::ContentEncodings, EbmlDataType::Master, 3, 0xae));
        e.insert(0x6240, E(MkvElement::ContentEncoding, EbmlDataType::Master, 4, 0x6d80));
        e.insert(0x5034, E(MkvElement::ContentCompression, EbmlDataType::Master, 5, 0x6240));
        e.insert(0x4254, E(MkvElement::ContentCompAlgo, EbmlDataType::Unsigned, 6, 0x5034));
        e.insert(0x4255, E(MkvElement::ContentCompSettings, EbmlDataType::Binary, 6, 0x5034));
        e.insert(0x5031, E(MkvElement::ContentEncodingOrder, EbmlDataType::Unsigned, 5, 0x6240));
        e.insert(0x5032, E(MkvElement::ContentEncodingScope, EbmlDataType::Unsigned, 5, 0x6240));
        e.insert(0x5033, E(MkvElement::ContentEncodingType, EbmlDataType::Unsigned, 5, 0x6240));
        e.insert(0x5035, E(MkvElement::ContentEncryption, EbmlDataType::Master, 5, 0x6240));
        e.insert(0x47e7, E(MkvElement::ContentEncAesSettings, EbmlDataType::Master, 6, 0x5035));
        e.insert(0x47e8, E(MkvElement::AesSettingsCipherMode, EbmlDataType::Unsigned, 7, 0x47e7));
        e.insert(0x47e1, E(MkvElement::ContentEncAlgo, EbmlDataType::Unsigned, 6, 0x5035));
        e.insert(0x47e2, E(MkvElement::ContentEncKeyId, EbmlDataType::Binary, 6, 0x5035));
        e.insert(0x47e5, E(MkvElement::ContentSigAlgo, EbmlDataType::Unsigned, 6, 0x5035));
        e.insert(0x47e6, E(MkvElement::ContentSigHashAlgo, EbmlDataType::Unsigned, 6, 0x5035));
        e.insert(0x47e4, E(MkvElement::ContentSigKeyId, EbmlDataType::Binary, 6, 0x5035));
        e.insert(0x47e3, E(MkvElement::ContentSignature, EbmlDataType::Binary, 6, 0x5035));
        e.insert(0x234e7a, E(MkvElement::DefaultDecodedFieldDuration, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0x23e383, E(MkvElement::DefaultDuration, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0x55af, E(MkvElement::FlagCommentary, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0x88, E(MkvElement::FlagDefault, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0xb9, E(MkvElement::FlagEnabled, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0x55aa, E(MkvElement::FlagForced, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0x55ab, E(MkvElement::FlagHearingImpaired, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0x9c, E(MkvElement::FlagLacing, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0x55ae, E(MkvElement::FlagOriginal, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0x55ad, E(MkvElement::FlagTextDescriptions, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0x55ac, E(MkvElement::FlagVisualImpaired, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0x22b59c, E(MkvElement::Language, EbmlDataType::String, 3, 0xae));
        e.insert(0x22b59d, E(MkvElement::LanguageBcp47, EbmlDataType::String, 3, 0xae));
        e.insert(0x55ee, E(MkvElement::MaxBlockAdditionId, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0x6df8, E(MkvElement::MaxCache, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0x6de7, E(MkvElement::MinCache, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0x536e, E(MkvElement::Name, EbmlDataType::String, 3, 0xae));
        e.insert(0x56bb, E(MkvElement::SeekPreRoll, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0xd7, E(MkvElement::TrackNumber, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0x537f, E(MkvElement::TrackOffset, EbmlDataType::Signed, 3, 0xae));
        e.insert(0xe2, E(MkvElement::TrackOperation, EbmlDataType::Master, 3, 0xae));
        e.insert(0xe3, E(MkvElement::TrackCombinePlanes, EbmlDataType::Master, 4, 0xe2));
        e.insert(0xe4, E(MkvElement::TrackPlane, EbmlDataType::Master, 5, 0xe3));
        e.insert(0xe6, E(MkvElement::TrackPlaneType, EbmlDataType::Unsigned, 6, 0xe4));
        e.insert(0xe5, E(MkvElement::TrackPlaneUid, EbmlDataType::Unsigned, 6, 0xe4));
        e.insert(0xe9, E(MkvElement::TrackJoinBlocks, EbmlDataType::Master, 4, 0xe2));
        e.insert(0xed, E(MkvElement::TrackJoinUid, EbmlDataType::Unsigned, 5, 0xe9));
        e.insert(0x6fab, E(MkvElement::TrackOverlay, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0x23314f, E(MkvElement::TrackTimestampScale, EbmlDataType::Float, 3, 0xae));
        e.insert(0x6624, E(MkvElement::TrackTranslate, EbmlDataType::Master, 3, 0xae));
        e.insert(0x66bf, E(MkvElement::TrackTranslateCodec, EbmlDataType::Unsigned, 4, 0x6624));
        e.insert(0x66fc, E(MkvElement::TrackTranslateEditionUid, EbmlDataType::Unsigned, 4, 0x6624));
        e.insert(0x66a5, E(MkvElement::TrackTranslateTrackId, EbmlDataType::Binary, 4, 0x6624));
        e.insert(0x83, E(MkvElement::TrackType, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0x73c5, E(MkvElement::TrackUid, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0xc4, E(MkvElement::TrickMasterTrackSegmentUid, EbmlDataType::Binary, 3, 0xae));
        e.insert(0xc7, E(MkvElement::TrickMasterTrackUid, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0xc6, E(MkvElement::TrickTrackFlag, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0xc1, E(MkvElement::TrickTrackSegmentUid, EbmlDataType::Binary, 3, 0xae));
        e.insert(0xc0, E(MkvElement::TrickTrackUid, EbmlDataType::Unsigned, 3, 0xae));
        e.insert(0xe0, E(MkvElement::Video, EbmlDataType::Master, 3, 0xae));
        e.insert(0x53c0, E(MkvElement::AlphaMode, EbmlDataType::Unsigned, 4, 0xe0));
        e.insert(0x54b3, E(MkvElement::AspectRatioType, EbmlDataType::Unsigned, 4, 0xe0));
        e.insert(0x55b0, E(MkvElement::Colour, EbmlDataType::Master, 4, 0xe0));
        e.insert(0x55b2, E(MkvElement::BitsPerChannel, EbmlDataType::Unsigned, 5, 0x55b0));
        e.insert(0x55b5, E(MkvElement::CbSubsamplingHorz, EbmlDataType::Unsigned, 5, 0x55b0));
        e.insert(0x55b6, E(MkvElement::CbSubsamplingVert, EbmlDataType::Unsigned, 5, 0x55b0));
        e.insert(0x55b7, E(MkvElement::ChromaSitingHorz, EbmlDataType::Unsigned, 5, 0x55b0));
        e.insert(0x55b8, E(MkvElement::ChromaSitingVert, EbmlDataType::Unsigned, 5, 0x55b0));
        e.insert(0x55b3, E(MkvElement::ChromaSubsamplingHorz, EbmlDataType::Unsigned, 5, 0x55b0));
        e.insert(0x55b4, E(MkvElement::ChromaSubsamplingVert, EbmlDataType::Unsigned, 5, 0x55b0));
        e.insert(0x55d0, E(MkvElement::MasteringMetadata, EbmlDataType::Master, 5, 0x55b0));
        e.insert(0x55d9, E(MkvElement::LuminanceMax, EbmlDataType::Float, 6, 0x55d0));
        e.insert(0x55da, E(MkvElement::LuminanceMin, EbmlDataType::Float, 6, 0x55d0));
        e.insert(0x55d5, E(MkvElement::PrimaryBChromaticityX, EbmlDataType::Float, 6, 0x55d0));
        e.insert(0x55d6, E(MkvElement::PrimaryBChromaticityY, EbmlDataType::Float, 6, 0x55d0));
        e.insert(0x55d3, E(MkvElement::PrimaryGChromaticityX, EbmlDataType::Float, 6, 0x55d0));
        e.insert(0x55d4, E(MkvElement::PrimaryGChromaticityY, EbmlDataType::Float, 6, 0x55d0));
        e.insert(0x55d1, E(MkvElement::PrimaryRChromaticityX, EbmlDataType::Float, 6, 0x55d0));
        e.insert(0x55d2, E(MkvElement::PrimaryRChromaticityY, EbmlDataType::Float, 6, 0x55d0));
        e.insert(0x55d7, E(MkvElement::WhitePointChromaticityX, EbmlDataType::Float, 6, 0x55d0));
        e.insert(0x55d8, E(MkvElement::WhitePointChromaticityY, EbmlDataType::Float, 6, 0x55d0));
        e.insert(0x55b1, E(MkvElement::MatrixCoefficients, EbmlDataType::Unsigned, 5, 0x55b0));
        e.insert(0x55bc, E(MkvElement::MaxCll, EbmlDataType::Unsigned, 5, 0x55b0));
        e.insert(0x55bd, E(MkvElement::MaxFall, EbmlDataType::Unsigned, 5, 0x55b0));
        e.insert(0x55bb, E(MkvElement::Primaries, EbmlDataType::Unsigned, 5, 0x55b0));
        e.insert(0x55b9, E(MkvElement::Range, EbmlDataType::Unsigned, 5, 0x55b0));
        e.insert(0x55ba, E(MkvElement::TransferCharacteristics, EbmlDataType::Unsigned, 5, 0x55b0));
        e.insert(0x54ba, E(MkvElement::DisplayHeight, EbmlDataType::Unsigned, 4, 0xe0));
        e.insert(0x54b2, E(MkvElement::DisplayUnit, EbmlDataType::Unsigned, 4, 0xe0));
        e.insert(0x54b0, E(MkvElement::DisplayWidth, EbmlDataType::Unsigned, 4, 0xe0));
        e.insert(0x9d, E(MkvElement::FieldOrder, EbmlDataType::Unsigned, 4, 0xe0));
        e.insert(0x9a, E(MkvElement::FlagInterlaced, EbmlDataType::Unsigned, 4, 0xe0));
        e.insert(0x2383e3, E(MkvElement::FrameRate, EbmlDataType::Float, 4, 0xe0));
        e.insert(0x2fb523, E(MkvElement::GammaValue, EbmlDataType::Float, 4, 0xe0));
        e.insert(0x53b9, E(MkvElement::OldStereoMode, EbmlDataType::Unsigned, 4, 0xe0));
        e.insert(0x54aa, E(MkvElement::PixelCropBottom, EbmlDataType::Unsigned, 4, 0xe0));
        e.insert(0x54cc, E(MkvElement::PixelCropLeft, EbmlDataType::Unsigned, 4, 0xe0));
        e.insert(0x54dd, E(MkvElement::PixelCropRight, EbmlDataType::Unsigned, 4, 0xe0));
        e.insert(0x54bb, E(MkvElement::PixelCropTop, EbmlDataType::Unsigned, 4, 0xe0));
        e.insert(0xba, E(MkvElement::PixelHeight, EbmlDataType::Unsigned, 4, 0xe0));
        e.insert(0xb0, E(MkvElement::PixelWidth, EbmlDataType::Unsigned, 4, 0xe0));
        e.insert(0x7670, E(MkvElement::Projection, EbmlDataType::Master, 4, 0xe0));
        e.insert(0x7674, E(MkvElement::ProjectionPosePitch, EbmlDataType::Float, 5, 0x7670));
        e.insert(0x7675, E(MkvElement::ProjectionPoseRoll, EbmlDataType::Float, 5, 0x7670));
        e.insert(0x7673, E(MkvElement::ProjectionPoseYaw, EbmlDataType::Float, 5, 0x7670));
        e.insert(0x7672, E(MkvElement::ProjectionPrivate, EbmlDataType::Binary, 5, 0x7670));
        e.insert(0x7671, E(MkvElement::ProjectionType, EbmlDataType::Unsigned, 5, 0x7670));
        e.insert(0x53b8, E(MkvElement::StereoMode, EbmlDataType::Unsigned, 4, 0xe0));
        e.insert(0x2eb524, E(MkvElement::UncompressedFourCc, EbmlDataType::Binary, 4, 0xe0));
        e
    };
}

/// The EBML schema for MKV/WebM.
#[derive(Default)]
pub struct MkvSchema;

impl EbmlSchema for MkvSchema {
    const MAX_DEPTH: usize = 16;
    type ElementInfo = MkvElementInfo;
    fn get_element_info(&self, id: u32) -> Option<&Self::ElementInfo> {
        MKV_SCHEMA.get(&id)
    }
}
