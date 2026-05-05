// Symphonia
// Copyright (c) 2019-2022 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! The `meta` module defines basic metadata elements, and management structures.
//!
//! # Tags
//!
//! Within the context of media, a tag is single piece of metadata about the media as a whole, or a
//! track within the media. The storage of tags, their structure, and organization, varies based on
//! the metadata/tagging format.
//!
//! The [`Tag`] structure represents a single tag, and abstracts over the differences in tagging
//! formats. `Tag` is a composition of a mandatory raw tag, and an optional standard tag.
//!
//! ## Raw Tags
//!
//! A [`RawTag`] stores a tag in a data format that matches as closely as possible to the format of
//! the tag as it was written. The data format depends on the tagging format, and the writer of the
//! tag.
//!
//! A `RawTag` consists of a mandatory key-value pair. For most tagging formats, this is sufficient
//! to faithfully represent the original tag, however, for some more structured tagging formats, a
//! set of additional key-value pairs ([`RawTagSubField`]) may be populated.
//!
//! The meaning of the tag can be derived from its key, however, the key may be named differently
//! based on the underlying tagging format and the writer of the tag.
//!
//! Raw tags can be ignored by most tag consumers. Instead, standard tags should be preferred.
//!
//! ## Standard Tags
//!
//! A [`StandardTag`] is a parsed representation of a tag. Unlike a raw tag, a standard tag has a
//! well-defined data type and meaning.
//!
//! A metadata reader will assign a `StandardTag` to a `Tag` if it is able to identify the meaning
//! of the `RawTag`, and parse its value. If the `RawTag` maps to multiple `StandardTag`s, then
//! the `Tag` (along with the `RawTag`) will be duplicated for each `StandardTag` with each instance
//! being assigned one `StandardTag`.
//!
//! An end-user should prefer consuming standard tags over raw tags.
//!
//! ## Storage Efficiency
//!
//! In many cases, the value of a `RawTag` will be the same as the `StandardTag`. Since a value may
//! be large, duplicating it could be wasteful. For this reason, string and binary data values are
//! stored using an [`Arc`].

use std::borrow::Cow;
use std::collections::VecDeque;
use std::convert::From;
use std::fmt;
use std::num::NonZeroU8;
use std::sync::Arc;

use crate::common::{FourCc, Limit};
use crate::errors::Result;
use crate::io::MediaSourceStream;
use crate::units::Time;

/// A `MetadataId` is a unique identifier used to identify a specific metadata format.
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
pub struct MetadataId(u32);

impl MetadataId {
    /// Create a new metadata ID from a FourCC.
    pub const fn new(cc: FourCc) -> MetadataId {
        // A FourCc always only contains ASCII characters. Therefore, the upper bits are always 0.
        Self(0x8000_0000 | u32::from_be_bytes(cc.get()))
    }
}

impl From<FourCc> for MetadataId {
    fn from(value: FourCc) -> Self {
        MetadataId::new(value)
    }
}

impl fmt::Display for MetadataId {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:#x}", self.0)
    }
}

/// Null metadata format
pub const METADATA_ID_NULL: MetadataId = MetadataId(0x0);

/// Basic information about a metadata format.
#[derive(Copy, Clone, Debug)]
pub struct MetadataInfo {
    /// The `MetadataType` identifier.
    pub metadata: MetadataId,
    /// A short ASCII-only string identifying the format.
    pub short_name: &'static str,
    /// A longer, more descriptive, string identifying the format.
    pub long_name: &'static str,
}

/// A common set of options that all metadata readers use.
#[non_exhaustive]
#[derive(Copy, Clone, Debug, Default)]
pub struct MetadataOptions {
    /// The maximum size in bytes that a tag may occupy in memory once decoded. Tags exceeding this
    /// limit will be skipped by the demuxer. Take note that tags in memory are stored as UTF-8 and
    /// therefore may occupy more than one byte per character.
    ///
    /// Default: `Limit::Default` (a reasonable limit chosen by the reader)
    pub limit_tag_bytes: Limit,

    /// The maximum size in bytes that a visual (picture) may occupy.
    ///
    /// Default: `Limit::Default` (a reasonable limit chosen by the reader)
    pub limit_visual_bytes: Limit,
}

impl MetadataOptions {
    /// The maximum size in bytes that a tag may occupy in memory once decoded. Tags exceeding this
    /// limit will be skipped by the demuxer. Take note that tags in memory are stored as UTF-8 and
    /// therefore may occupy more than one byte per character.
    ///
    /// Default: `Limit::Default` (a reasonable limit chosen by the reader)
    pub fn limit_tag_bytes(mut self, limit: Limit) -> Self {
        self.limit_tag_bytes = limit;
        self
    }

    /// The maximum size in bytes that a visual (picture) may occupy.
    ///
    /// Default: `Limit::Default` (a reasonable limit chosen by the reader)
    pub fn limit_visual_bytes(mut self, limit: Limit) -> Self {
        self.limit_visual_bytes = limit;
        self
    }
}

/// `StandardVisualKey` is an enumeration providing standardized keys for common visual dispositions.
/// A demuxer may assign a `StandardVisualKey` to a `Visual` if the disposition of the attached
/// visual is known and can be mapped to a standard key.
///
/// The visual types listed here are derived from, though do not entirely cover, the ID3v2 APIC
/// frame specification.
#[non_exhaustive]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
pub enum StandardVisualKey {
    FileIcon,
    OtherIcon,
    FrontCover,
    BackCover,
    Leaflet,
    Media,
    LeadArtistPerformerSoloist,
    ArtistPerformer,
    Conductor,
    BandOrchestra,
    Composer,
    Lyricist,
    RecordingLocation,
    RecordingSession,
    Performance,
    ScreenCapture,
    Illustration,
    BandArtistLogo,
    PublisherStudioLogo,
    Other,
}

/// A content advisory.
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub enum ContentAdvisory {
    /// The content is not explicit.
    None,
    /// The content is explicit.
    Explicit,
    /// The content has been cleaned/censored.
    Censored,
}

/// A standard tag is an enumeration of well-defined and well-known tags with parsed values.
#[non_exhaustive]
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub enum StandardTag {
    AccurateRipCount(Arc<String>),
    AccurateRipCountAllOffsets(Arc<String>),
    AccurateRipCountWithOffset(Arc<String>),
    AccurateRipCrc(Arc<String>),
    AccurateRipDiscId(Arc<String>),
    AccurateRipId(Arc<String>),
    AccurateRipOffset(Arc<String>),
    AccurateRipResult(Arc<String>),
    AccurateRipTotal(Arc<String>),
    AcoustIdFingerprint(Arc<String>),
    AcoustIdId(Arc<String>),
    Actor(Arc<String>),
    Album(Arc<String>),
    AlbumArtist(Arc<String>),
    Arranger(Arc<String>),
    ArtDirector(Arc<String>),
    Artist(Arc<String>),
    AssistantDirector(Arc<String>),
    Author(Arc<String>),
    Bpm(u64),
    CdToc(Arc<String>),
    CdTrackIndex(u8),
    ChapterTitle(Arc<String>),
    Choregrapher(Arc<String>),
    Cinematographer(Arc<String>),
    CollectionTitle(Arc<String>),
    Comment(Arc<String>),
    CompilationFlag(bool),
    Composer(Arc<String>),
    Conductor(Arc<String>),
    ContentAdvisory(ContentAdvisory),
    ContentRating(Arc<String>),
    ContentType(Arc<String>),
    Coproducer(Arc<String>),
    Copyright(Arc<String>),
    CostumeDesigner(Arc<String>),
    CueToolsDbDiscConfidence(Arc<String>),
    CueToolsDbTrackConfidence(Arc<String>),
    Description(Arc<String>),
    DigitizedDate(Arc<String>),
    Director(Arc<String>),
    DiscNumber(u64),
    DiscSubtitle(Arc<String>),
    DiscTotal(u64),
    Distributor(Arc<String>),
    EditedBy(Arc<String>),
    EditionTitle(Arc<String>),
    EncodedBy(Arc<String>),
    Encoder(Arc<String>),
    EncoderSettings(Arc<String>),
    EncodingDate(Arc<String>),
    Engineer(Arc<String>),
    Ensemble(Arc<String>),
    ExecutiveProducer(Arc<String>),
    Genre(Arc<String>),
    Grouping(Arc<String>),
    IdentAsin(Arc<String>),
    IdentBarcode(Arc<String>),
    IdentCatalogNumber(Arc<String>),
    IdentEanUpn(Arc<String>),
    IdentIsbn(Arc<String>),
    IdentIsrc(Arc<String>),
    IdentLccn(Arc<String>),
    IdentPn(Arc<String>),
    IdentPodcast(Arc<String>),
    IdentUpc(Arc<String>),
    ImdbTitleId(Arc<String>),
    InitialKey(Arc<String>),
    InternetRadioName(Arc<String>),
    InternetRadioOwner(Arc<String>),
    Keywords(Arc<String>),
    Label(Arc<String>),
    LabelCode(Arc<String>),
    Language(Arc<String>),
    License(Arc<String>),
    Lyricist(Arc<String>),
    Lyrics(Arc<String>),
    Measure(Arc<String>),
    MediaFormat(Arc<String>),
    MixDj(Arc<String>),
    MixEngineer(Arc<String>),
    Mood(Arc<String>),
    MovementName(Arc<String>),
    MovementNumber(u64),
    MovementTotal(u64),
    MovieTitle(Arc<String>),
    Mp3GainAlbumMinMax(Arc<String>),
    Mp3GainMinMax(Arc<String>),
    Mp3GainUndo(Arc<String>),
    MusicBrainzAlbumArtistId(Arc<String>),
    MusicBrainzAlbumId(Arc<String>),
    MusicBrainzArtistId(Arc<String>),
    MusicBrainzDiscId(Arc<String>),
    MusicBrainzGenreId(Arc<String>),
    MusicBrainzLabelId(Arc<String>),
    MusicBrainzOriginalAlbumId(Arc<String>),
    MusicBrainzOriginalArtistId(Arc<String>),
    MusicBrainzRecordingId(Arc<String>),
    MusicBrainzReleaseGroupId(Arc<String>),
    MusicBrainzReleaseStatus(Arc<String>),
    MusicBrainzReleaseTrackId(Arc<String>),
    MusicBrainzReleaseType(Arc<String>),
    MusicBrainzTrackId(Arc<String>),
    MusicBrainzTrmId(Arc<String>),
    MusicBrainzWorkId(Arc<String>),
    Narrator(Arc<String>),
    Opus(Arc<String>),
    OpusNumber(u64),
    OriginalAlbum(Arc<String>),
    OriginalArtist(Arc<String>),
    OriginalFile(Arc<String>),
    OriginalLyricist(Arc<String>),
    OriginalRecordingDate(Arc<String>),
    OriginalRecordingTime(Arc<String>),
    OriginalRecordingYear(u16),
    OriginalReleaseDate(Arc<String>),
    OriginalReleaseTime(Arc<String>),
    OriginalReleaseYear(u16),
    OriginalWriter(Arc<String>),
    Owner(Arc<String>),
    Part(Arc<String>),
    PartNumber(u64),
    PartTitle(Arc<String>),
    PartTotal(u64),
    Performer(Arc<String>),
    Period(Arc<String>),
    PlayCounter(u64),
    PodcastCategory(Arc<String>),
    PodcastDescription(Arc<String>),
    PodcastFlag(bool),
    PodcastKeywords(Arc<String>),
    Producer(Arc<String>),
    ProductionCopyright(Arc<String>),
    ProductionDesigner(Arc<String>),
    ProductionStudio(Arc<String>),
    PurchaseDate(Arc<String>),
    Rating(u32), // In PPM.
    RecordingDate(Arc<String>),
    RecordingLocation(Arc<String>),
    RecordingTime(Arc<String>),
    RecordingYear(u16),
    ReleaseCountry(Arc<String>),
    ReleaseDate(Arc<String>),
    ReleaseTime(Arc<String>),
    ReleaseYear(u16),
    Remixer(Arc<String>),
    ReplayGainAlbumGain(Arc<String>),
    ReplayGainAlbumPeak(Arc<String>),
    ReplayGainAlbumRange(Arc<String>),
    ReplayGainReferenceLoudness(Arc<String>),
    ReplayGainTrackGain(Arc<String>),
    ReplayGainTrackPeak(Arc<String>),
    ReplayGainTrackRange(Arc<String>),
    ScreenplayAuthor(Arc<String>),
    Script(Arc<String>),
    Soloist(Arc<String>),
    SortAlbum(Arc<String>),
    SortAlbumArtist(Arc<String>),
    SortArtist(Arc<String>),
    SortCollectionTitle(Arc<String>),
    SortComposer(Arc<String>),
    SortEditionTitle(Arc<String>),
    SortMovieTitle(Arc<String>),
    SortOpusTitle(Arc<String>),
    SortPartTitle(Arc<String>),
    SortTrackTitle(Arc<String>),
    SortTvEpisodeTitle(Arc<String>),
    SortTvSeasonTitle(Arc<String>),
    SortTvSeriesTitle(Arc<String>),
    SortVolumeTitle(Arc<String>),
    Subject(Arc<String>),
    Summary(Arc<String>),
    Synopsis(Arc<String>),
    TaggingDate(Arc<String>),
    TermsOfUse(Arc<String>),
    Thanks(Arc<String>),
    TmdbMovieId(Arc<String>),
    TmdbSeriesId(Arc<String>),
    TrackNumber(u64),
    TrackSubtitle(Arc<String>),
    TrackTitle(Arc<String>),
    TrackTotal(u64),
    Tuning(Arc<String>),
    TvdbEpisodeId(Arc<String>),
    TvdbMovieId(Arc<String>),
    TvdbSeriesId(Arc<String>),
    TvEpisodeNumber(u64),
    TvEpisodeTitle(Arc<String>),
    TvEpisodeTotal(u64),
    TvNetwork(Arc<String>),
    TvSeasonNumber(u64),
    TvSeasonTitle(Arc<String>),
    TvSeasonTotal(u64),
    TvSeriesTitle(Arc<String>),
    Url(Arc<String>),
    UrlArtist(Arc<String>),
    UrlCopyright(Arc<String>),
    UrlInternetRadio(Arc<String>),
    UrlLabel(Arc<String>),
    UrlOfficial(Arc<String>),
    UrlPayment(Arc<String>),
    UrlPodcast(Arc<String>),
    UrlPurchase(Arc<String>),
    UrlSource(Arc<String>),
    Version(Arc<String>),
    VolumeNumber(u64),
    VolumeTitle(Arc<String>),
    VolumeTotal(u64),
    Work(Arc<String>),
    Writer(Arc<String>),
    WrittenDate(Arc<String>),
}

/// The value of a [`RawTag`].
///
/// Note: The data types in this enumeration are an abstraction. Depending on the particular tagging
/// format, the actual data type of a specific tag may have a lesser width or different encoding
/// than the data type stored here.
#[non_exhaustive]
#[derive(Clone, Debug, PartialEq)]
pub enum RawValue {
    /// A binary buffer.
    Binary(Arc<Box<[u8]>>),
    /// A boolean value.
    Boolean(bool),
    /// A flag or indicator. A flag carries no data, but the presence of the tag has an implicit
    /// meaning.
    Flag,
    /// A floating point number.
    Float(f64),
    /// A signed integer.
    SignedInt(i64),
    /// A string.
    String(Arc<String>),
    /// A list of strings.
    StringList(Arc<Vec<String>>),
    /// An unsigned integer.
    UnsignedInt(u64),
}

macro_rules! impl_from_for_value {
    ($value:ident, $from:ty, $conv:expr) => {
        impl From<$from> for RawValue {
            fn from($value: $from) -> Self {
                $conv
            }
        }
    };
}

impl_from_for_value!(v, &[u8], RawValue::Binary(Arc::new(Box::from(v))));
impl_from_for_value!(v, Box<[u8]>, RawValue::Binary(Arc::new(v)));
impl_from_for_value!(v, Arc<Box<[u8]>>, RawValue::Binary(v));
impl_from_for_value!(v, bool, RawValue::Boolean(v));
impl_from_for_value!(v, f32, RawValue::Float(f64::from(v)));
impl_from_for_value!(v, f64, RawValue::Float(v));
impl_from_for_value!(v, i8, RawValue::SignedInt(i64::from(v)));
impl_from_for_value!(v, i16, RawValue::SignedInt(i64::from(v)));
impl_from_for_value!(v, i32, RawValue::SignedInt(i64::from(v)));
impl_from_for_value!(v, i64, RawValue::SignedInt(v));
impl_from_for_value!(v, u8, RawValue::UnsignedInt(u64::from(v)));
impl_from_for_value!(v, u16, RawValue::UnsignedInt(u64::from(v)));
impl_from_for_value!(v, u32, RawValue::UnsignedInt(u64::from(v)));
impl_from_for_value!(v, u64, RawValue::UnsignedInt(v));
impl_from_for_value!(v, &str, RawValue::String(Arc::new(v.to_string())));
impl_from_for_value!(v, String, RawValue::String(Arc::new(v)));
impl_from_for_value!(v, Arc<String>, RawValue::String(v));
impl_from_for_value!(v, Cow<'_, str>, RawValue::String(Arc::new(v.into_owned())));
impl_from_for_value!(v, Vec<String>, RawValue::StringList(Arc::new(v)));

fn buffer_to_hex_string(buf: &[u8]) -> String {
    let mut output = String::with_capacity(5 * buf.len());

    for ch in buf {
        let u = (ch & 0xf0) >> 4;
        let l = ch & 0x0f;
        output.push_str("\\0x");
        output.push(if u < 10 { (b'0' + u) as char } else { (b'a' + u - 10) as char });
        output.push(if l < 10 { (b'0' + l) as char } else { (b'a' + l - 10) as char });
    }

    output
}

impl fmt::Display for RawValue {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // Implement default formatters for each type.
        match self {
            RawValue::Binary(buf) => f.write_str(&buffer_to_hex_string(buf)),
            RawValue::Boolean(boolean) => fmt::Display::fmt(boolean, f),
            RawValue::Flag => write!(f, "<flag>"),
            RawValue::Float(float) => fmt::Display::fmt(float, f),
            RawValue::SignedInt(int) => fmt::Display::fmt(int, f),
            RawValue::String(string) => fmt::Display::fmt(string, f),
            RawValue::StringList(list) => fmt::Display::fmt(&list.join("\n"), f),
            RawValue::UnsignedInt(uint) => fmt::Display::fmt(uint, f),
        }
    }
}

/// A key-value pair of supplementary data that can be attached to a raw tag.
#[derive(Clone, Debug)]
pub struct RawTagSubField {
    /// The name of the sub-field.
    pub field: String,
    /// The value of the sub-field.
    pub value: RawValue,
}

impl RawTagSubField {
    /// Create a new sub-field from the provided field and value. Consumes the inputs.
    pub fn new<F, V>(field: F, value: V) -> Self
    where
        F: Into<String>,
        V: Into<RawValue>,
    {
        RawTagSubField { field: field.into(), value: value.into() }
    }
}

/// A raw tag represents a tag in a data format that matches, as closely as possible, to the data
/// format that the tag was written in.
#[derive(Clone, Debug)]
pub struct RawTag {
    /// The name of the tag's key.
    pub key: String,
    /// The value of the tag.
    pub value: RawValue,
    /// The tag's sub-fields, if any.
    pub sub_fields: Option<Box<[RawTagSubField]>>,
}

impl RawTag {
    /// Create a new raw tag from the provided key and value, with no sub-fields. Consumes the
    /// inputs.
    pub fn new<K, V>(key: K, value: V) -> Self
    where
        K: Into<String>,
        V: Into<RawValue>,
    {
        RawTag { key: key.into(), value: value.into(), sub_fields: None }
    }

    /// Create a new raw tag with sub-fields from the provided key, value, and sub-fields. Consumes
    /// the inputs.
    pub fn new_with_sub_fields<K, V>(key: K, value: V, sub_fields: Box<[RawTagSubField]>) -> Self
    where
        K: Into<String>,
        V: Into<RawValue>,
    {
        RawTag { key: key.into(), value: value.into(), sub_fields: Some(sub_fields) }
    }
}

/// A tag encapsulates a single piece of metadata.
#[derive(Clone, Debug)]
pub struct Tag {
    /// The raw tag.
    pub raw: RawTag,
    /// An optional standard tag.
    pub std: Option<StandardTag>,
}

impl Tag {
    /// Create a new tag from a raw tag. Consumes the inputs.
    pub fn new(raw: RawTag) -> Self {
        Tag { raw, std: None }
    }

    /// Create a new tag from a raw tag with a standard tag. Consumes the inputs.
    pub fn new_std(raw: RawTag, std: StandardTag) -> Self {
        Tag { raw, std: Some(std) }
    }

    /// Create a new tag from its constituent parts: a key, value, and optional standard tag.
    /// Consumes the inputs.
    pub fn new_from_parts<K, V>(key: K, value: V, std: Option<StandardTag>) -> Self
    where
        K: Into<String>,
        V: Into<RawValue>,
    {
        Tag { raw: RawTag { key: key.into(), value: value.into(), sub_fields: None }, std }
    }

    /// Returns `true` if the tag was recognized as a well-known tag and has a standard tag
    /// assigned.
    pub fn has_std_tag(&self) -> bool {
        self.std.is_some()
    }
}

/// A 2-dimensional (width and height) size type.
#[derive(Copy, Clone, Debug, Default)]
pub struct Size {
    /// The width in pixels.
    pub width: u32,
    /// The height in pixels.
    pub height: u32,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
/// A color model describes how a color is represented.
#[non_exhaustive]
pub enum ColorModel {
    /// Grayscale (1 channel: `Y`), of the indicated bit depth.
    Y(NonZeroU8),
    /// Grayscale with alpha (2 channels: `Y`, `A`), of the indicated bit depth.
    YA(NonZeroU8),
    /// RGB (3 channels: `R`,`G`,`B`), of the indicated bit depth.
    RGB(NonZeroU8),
    /// RGBA (4 channels: `R`,`G`,`B`,`A`), of the indicated bit depth.
    RGBA(NonZeroU8),
    /// CMYK (4 channels: `C`,`M`,`Y`,`K`), of the indicated bit depth.
    CMYK(NonZeroU8),
}

impl ColorModel {
    /// Gets the bits/pixel.
    pub fn bits_per_pixel(&self) -> u32 {
        match self {
            ColorModel::Y(bits) => u32::from(bits.get()),
            ColorModel::YA(bits) => 2 * u32::from(bits.get()),
            ColorModel::RGB(bits) => 3 * u32::from(bits.get()),
            ColorModel::RGBA(bits) => 4 * u32::from(bits.get()),
            ColorModel::CMYK(bits) => 4 * u32::from(bits.get()),
        }
    }

    /// Returns if the color model contains an alpha channel.
    pub fn has_alpha_channel(&self) -> bool {
        matches!(self, ColorModel::YA(_) | ColorModel::RGBA(_))
    }
}

/// A description of the color palette for indexed color mode.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct ColorPaletteInfo {
    /// The number of bits per pixel used to index the palette.
    pub bits_per_pixel: NonZeroU8,
    /// The color model of the entries in the palette.
    pub color_model: ColorModel,
}

/// Indicates how colors are represented in the image.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum ColorMode {
    /// Direct colour mode. Each pixel in the image stores the value of each color model primary.
    ///
    /// For example, in the RGB color model, each pixel will store a value for the red, green, and
    /// blue color primaries.
    Direct(ColorModel),
    /// Indexed colour mode. Each pixel in the image stores an index into a color map (the palette)
    /// that stores the actual color.
    Indexed(ColorPaletteInfo),
}

/// A `Visual` is any 2 dimensional graphic.
#[derive(Clone, Debug)]
pub struct Visual {
    /// The Media Type (MIME Type) used to encode the `Visual`.
    pub media_type: Option<String>,
    /// The dimensions of the `Visual`.
    ///
    /// Note: This value may not be accurate as it comes from metadata, not the embedded graphic
    /// itself. Consider it only a hint.
    pub dimensions: Option<Size>,
    /// The color mode of the `Visual`.
    ///
    /// Note: This value may not be accurate as it comes from metadata, not the embedded graphic
    /// itself. Consider it only a hint.
    pub color_mode: Option<ColorMode>,
    /// The usage and/or content of the `Visual`.
    pub usage: Option<StandardVisualKey>,
    /// Any tags associated with the `Visual`.
    pub tags: Vec<Tag>,
    /// The data of the `Visual`, encoded as per `media_type`.
    pub data: Box<[u8]>,
}

/// A group of chapters and/or other chapter groups.
#[derive(Clone, Debug)]
pub struct ChapterGroup {
    /// A list of chapters and/or chapter groups.
    pub items: Vec<ChapterGroupItem>,
    /// The tags associated with the group of chapters.
    pub tags: Vec<Tag>,
    /// The visuals associated with the group of chapters.
    pub visuals: Vec<Visual>,
}

/// A chapter is a labelled section of a piece of media with a defined start time.
#[derive(Clone, Debug)]
pub struct Chapter {
    /// The offset from the beginning of the media to the start of the chapter.
    pub start_time: Time,
    /// The offset from the beginning of the media to the end of the chapter.
    pub end_time: Option<Time>,
    /// The byte position from the beginning of the media source to the first byte of the first
    /// frame in the chapter.
    pub start_byte: Option<u64>,
    /// The byte position from the beginning of the media source to the first byte of the frame
    /// following the end of the chapter.
    pub end_byte: Option<u64>,
    /// The tags associated with the chapter.
    pub tags: Vec<Tag>,
    /// The visuals associated with the chapter.
    pub visuals: Vec<Visual>,
}

/// A chapter group item is either a chapter or chapter group.
#[derive(Clone, Debug)]
pub enum ChapterGroupItem {
    /// The item is a chapter group.
    Group(ChapterGroup),
    /// The item is a chapter.
    Chapter(Chapter),
}

/// A container of metadata tags and pictures.
#[derive(Clone, Debug, Default)]
pub struct MetadataContainer {
    /// Key-value pairs of metadata.
    pub tags: Vec<Tag>,
    /// Attached pictures.
    pub visuals: Vec<Visual>,
}

/// Container for metadata associated with a specific track. A [`MetadataContainer`] wrapper
/// associating it with a track ID.
#[derive(Clone, Debug, Default)]
pub struct PerTrackMetadata {
    /// The ID of the track the metadata is associated with.
    pub track_id: u64,
    /// Track-specific metadata.
    pub metadata: MetadataContainer,
}

/// A metadata revision is a container for a single discrete version of media metadata.
///
/// A metadata revision contains what a user typically associates with metadata: tags, pictures,
/// etc., for the media as a whole and, optionally, for each contained track.
#[derive(Clone, Debug)]
pub struct MetadataRevision {
    /// Information about the source of this revision of metadata.
    pub info: MetadataInfo,
    /// Media-level, global, metadata.
    ///
    /// The vast majority of metadata is media-level metadata.
    pub media: MetadataContainer,
    /// Per-track metadata.
    pub per_track: Vec<PerTrackMetadata>,
}

/// A builder for [`MetadataRevision`].
#[derive(Clone, Debug)]
pub struct MetadataBuilder {
    revision: MetadataRevision,
}

impl MetadataBuilder {
    /// Instantiate a new `MetadataBuilder`.
    pub fn new(info: MetadataInfo) -> Self {
        let revision =
            MetadataRevision { info, media: Default::default(), per_track: Default::default() };
        MetadataBuilder { revision }
    }

    /// Add a media-level `Tag` to the metadata.
    pub fn add_tag(&mut self, tag: Tag) -> &mut Self {
        self.revision.media.tags.push(tag);
        self
    }

    /// Add a media-level `Visual` to the metadata.
    pub fn add_visual(&mut self, visual: Visual) -> &mut Self {
        self.revision.media.visuals.push(visual);
        self
    }

    /// Add track-specific metadata.
    pub fn add_track(&mut self, per_track: PerTrackMetadata) -> &mut Self {
        self.revision.per_track.push(per_track);
        self
    }

    /// Build and return the metadata revision.
    pub fn build(self) -> MetadataRevision {
        self.revision
    }
}

/// A builder for [`PerTrackMetadata`].
#[derive(Clone, Debug)]
pub struct PerTrackMetadataBuilder {
    per_track: PerTrackMetadata,
}

impl PerTrackMetadataBuilder {
    /// Instantiate a new `MetadataBuilder`.
    pub fn new(track_id: u64) -> Self {
        PerTrackMetadataBuilder {
            per_track: PerTrackMetadata { track_id, metadata: Default::default() },
        }
    }

    /// Add a `Tag` to the per-track metadata.
    pub fn add_tag(&mut self, tag: Tag) -> &mut Self {
        self.per_track.metadata.tags.push(tag);
        self
    }

    /// Add a `Visual` to the per-track metadata.
    pub fn add_visual(&mut self, visual: Visual) -> &mut Self {
        self.per_track.metadata.visuals.push(visual);
        self
    }

    /// Build and return the track-specific metadata.
    pub fn build(self) -> PerTrackMetadata {
        self.per_track
    }
}

/// A reference to the metadata inside of a [`MetadataLog`].
#[derive(Debug)]
pub struct Metadata<'a> {
    revisions: &'a mut VecDeque<MetadataRevision>,
}

impl Metadata<'_> {
    /// Returns `true` if the current metadata revision is the newest, `false` otherwise.
    pub fn is_latest(&self) -> bool {
        self.revisions.len() <= 1
    }

    /// Gets an immutable reference to the current, and therefore oldest, revision of the metadata.
    pub fn current(&self) -> Option<&MetadataRevision> {
        self.revisions.front()
    }

    /// Skips to, and gets an immutable reference to the latest, and therefore newest, revision of
    /// the metadata.
    pub fn skip_to_latest(&mut self) -> Option<&MetadataRevision> {
        loop {
            if self.pop().is_none() {
                break;
            }
        }
        self.current()
    }

    /// If there are newer `Metadata` revisions, advances the `MetadataLog` by discarding the
    /// current revision and replacing it with the next revision, returning the discarded
    /// `Metadata`. When there are no newer revisions, `None` is returned. As such, `pop` will never
    /// completely empty the log.
    pub fn pop(&mut self) -> Option<MetadataRevision> {
        if self.revisions.len() > 1 { self.revisions.pop_front() } else { None }
    }
}

/// A queue for time-ordered [`MetadataRevision`]s.
#[derive(Clone, Debug, Default)]
pub struct MetadataLog {
    revisions: VecDeque<MetadataRevision>,
}

impl MetadataLog {
    /// Returns a reference to the metadata revisions inside the log.
    pub fn metadata(&mut self) -> Metadata<'_> {
        Metadata { revisions: &mut self.revisions }
    }

    /// Push a new metadata revision to the end of the log.
    pub fn push(&mut self, rev: MetadataRevision) {
        self.revisions.push_back(rev);
    }

    /// Moves all metadata revisions from another metadata log to the end of this log.
    pub fn append(&mut self, other: &mut MetadataLog) {
        self.revisions.append(&mut other.revisions);
    }

    /// Push a metadata revision to the front of the log.
    pub fn push_front(&mut self, rev: MetadataRevision) {
        self.revisions.push_front(rev);
    }

    /// Moves all metadata revisions from another metadata log to the front of this log.
    pub fn append_front(&mut self, other: &mut MetadataLog) {
        // Maintain the relative ordering.
        while let Some(revision) = other.revisions.pop_back() {
            self.revisions.push_front(revision)
        }
    }
}

/// Enumeration of types of metadata side data.
#[non_exhaustive]
pub enum MetadataSideData {
    /// Chapter information.
    Chapters(ChapterGroup),
}

/// The decoded contents of read metadata.
pub struct MetadataBuffer {
    /// The revision of metadata containing tags, visuals, and vendor-specific metadata buffers.
    pub revision: MetadataRevision,
    /// Additional pieces of data stored in the metadata, but not part of a metadata revision. These
    /// pieces of data are usually passed to a format reader to support its function.
    pub side_data: Vec<MetadataSideData>,
}

/// A `MetadataReader` reads and decodes metadata and produces a revision of that decoded metadata.
pub trait MetadataReader: Send + Sync {
    /// Get basic information about the metadata format.
    fn metadata_info(&self) -> &MetadataInfo;

    /// Read all metadata and return it if successful.
    fn read_all(&mut self) -> Result<MetadataBuffer>;

    /// Consumes the `MetadataReader` and returns the underlying media source stream
    fn into_inner<'s>(self: Box<Self>) -> MediaSourceStream<'s>
    where
        Self: 's;
}

/// IDs for well-known metadata formats.
pub mod well_known {
    use super::MetadataId;

    // Xiph tags

    /// Vorbis Comment
    pub const METADATA_ID_VORBIS_COMMENT: MetadataId = MetadataId(0x100);
    /// FLAC tags
    pub const METADATA_ID_FLAC: MetadataId = MetadataId(0x101);

    // ID3 tags

    /// ID3v1
    pub const METADATA_ID_ID3V1: MetadataId = MetadataId(0x200);
    /// ID3v2
    pub const METADATA_ID_ID3V2: MetadataId = MetadataId(0x201);

    // APE tags

    /// APEv1
    pub const METADATA_ID_APEV1: MetadataId = MetadataId(0x300);
    /// APEv2
    pub const METADATA_ID_APEV2: MetadataId = MetadataId(0x301);

    // Format-native tags

    /// RIFF tags
    pub const METADATA_ID_WAVE: MetadataId = MetadataId(0x400);
    /// RIFF tags
    pub const METADATA_ID_AIFF: MetadataId = MetadataId(0x401);
    /// EXIF
    pub const METADATA_ID_EXIF: MetadataId = MetadataId(0x402);
    /// Matroska tags
    pub const METADATA_ID_MATROSKA: MetadataId = MetadataId(0x403);
    /// ISOMP4 tags
    pub const METADATA_ID_ISOMP4: MetadataId = MetadataId(0x404);
}
