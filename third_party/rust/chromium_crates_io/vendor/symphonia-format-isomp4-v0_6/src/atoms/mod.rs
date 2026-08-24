// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::{io::SeekFrom, num::NonZeroU64};

use symphonia_core::io::{MediaSource, ReadBytes, SeekBuffered};

/// Atom parsing limits.
pub mod limits {
    /// The maximum depth the atom iterator may recurse.
    pub const MAX_ITERATION_DEPTH: usize = 32;

    /// The maximum capacity a table vector may be pre-allocated. This limit prevents malicious
    /// files from exhausting a system's memory by stating an excessively large table length.
    pub const MAX_TABLE_INITIAL_CAPACITY: usize = 1024;
}

pub(crate) mod alac;
pub(crate) mod avcc;
pub(crate) mod co64;
pub(crate) mod ctts;
pub(crate) mod dac3;
pub(crate) mod dec3;
pub(crate) mod dovi;
pub(crate) mod edts;
pub(crate) mod elst;
pub(crate) mod esds;
pub(crate) mod flac;
pub(crate) mod ftyp;
pub(crate) mod hdlr;
pub(crate) mod hvcc;
pub(crate) mod ilst;
pub(crate) mod mdhd;
pub(crate) mod mdia;
pub(crate) mod mehd;
pub(crate) mod meta;
pub(crate) mod mfhd;
pub(crate) mod minf;
pub(crate) mod moof;
pub(crate) mod moov;
pub(crate) mod mvex;
pub(crate) mod mvhd;
pub(crate) mod opus;
pub(crate) mod sidx;
pub(crate) mod smhd;
pub(crate) mod stbl;
pub(crate) mod stco;
pub(crate) mod stsc;
pub(crate) mod stsd;
pub(crate) mod stss;
pub(crate) mod stsz;
pub(crate) mod stts;
pub(crate) mod tfhd;
pub(crate) mod tkhd;
pub(crate) mod traf;
pub(crate) mod trak;
pub(crate) mod trex;
pub(crate) mod trun;
pub(crate) mod udta;
pub(crate) mod wave;

use crate::atoms::limits::MAX_ITERATION_DEPTH;

pub use self::meta::MetaAtom;
pub use alac::AlacAtom;
pub use avcc::AvcCAtom;
pub use co64::Co64Atom;
#[allow(unused_imports)]
pub use ctts::CttsAtom;
pub use dac3::Dac3Atom;
pub use dec3::Dec3Atom;
pub use dovi::DoviAtom;
pub use edts::EdtsAtom;
pub use elst::ElstAtom;
pub use esds::EsdsAtom;
pub use flac::FlacAtom;
pub use ftyp::FtypAtom;
pub use hdlr::HdlrAtom;
pub use hvcc::HvcCAtom;
pub use ilst::IlstAtom;
pub use mdhd::MdhdAtom;
pub use mdia::MdiaAtom;
pub use mehd::MehdAtom;
pub use mfhd::MfhdAtom;
pub use minf::MinfAtom;
pub use moof::MoofAtom;
pub use moov::MoovAtom;
pub use mvex::MvexAtom;
pub use mvhd::MvhdAtom;
pub use opus::OpusAtom;
pub use sidx::SidxAtom;
pub use smhd::SmhdAtom;
pub use stbl::StblAtom;
pub use stco::StcoAtom;
pub use stsc::StscAtom;
pub use stsd::StsdAtom;
#[allow(unused_imports)]
pub use stss::StssAtom;
pub use stsz::StszAtom;
pub use stts::SttsAtom;
pub use tfhd::TfhdAtom;
pub use tkhd::TkhdAtom;
pub use traf::TrafAtom;
pub use trak::TrakAtom;
pub use trex::TrexAtom;
pub use trun::TrunAtom;
pub use udta::UdtaAtom;
pub use wave::WaveAtom;

/// Atom types.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum AtomType {
    Ac3Config,
    AdvisoryTag,
    AlbumArtistTag,
    AlbumTag,
    ArrangerTag,
    ArtistTag,
    AudioSampleEntryAc3,
    AudioSampleEntryAlac,
    AudioSampleEntryALaw,
    AudioSampleEntryEc3,
    AudioSampleEntryF32,
    AudioSampleEntryF64,
    AudioSampleEntryFlac,
    AudioSampleEntryLpcm,
    AudioSampleEntryMp3,
    AudioSampleEntryMp4a,
    AudioSampleEntryMuLaw,
    AudioSampleEntryOpus,
    AudioSampleEntryQtWave,
    AudioSampleEntryS16Be,
    AudioSampleEntryS16Le,
    AudioSampleEntryS24,
    AudioSampleEntryS32,
    AudioSampleEntryU8,
    AuthorTag,
    AvcConfiguration,
    BitRate,
    ChunkOffset,
    ChunkOffset64,
    CleanAperture,
    CommentTag,
    CompilationTag,
    ComposerTag,
    CompositionTimeToSample,
    ConductorTag,
    CopyrightTag,
    CoverTag,
    CustomGenreTag,
    DateTag,
    DescriptionTag,
    DiskNumberTag,
    DolbyVisionConfiguration,
    Eac3Config,
    Edit,
    EditList,
    EncodedByTag,
    EncoderTag,
    Esds,
    FileCreatorUrlTag,
    FileType,
    FlacDsConfig,
    Free,
    FreeFormTag,
    GaplessPlaybackTag,
    GenreTag,
    GroupingTag,
    Handler,
    HdVideoTag,
    HevcConfiguration,
    IdentPodcastTag,
    IsrcTag,
    ItunesAccountIdTag,
    ItunesAccountTypeIdTag,
    ItunesArtistIdTag,
    ItunesComposerIdTag,
    ItunesContentIdTag,
    ItunesCountryIdTag,
    ItunesGenreIdTag,
    ItunesPlaylistIdTag,
    LabelTag,
    LabelUrlTag,
    LongDescriptionTag,
    LyricsTag,
    Media,
    MediaData,
    MediaHeader,
    MediaInfo,
    MediaTypeTag,
    Meta,
    MetaList,
    MetaTagData,
    MetaTagMeaning,
    MetaTagName,
    MovementCountTag,
    MovementIndexTag,
    MovementTag,
    Movie,
    MovieExtends,
    MovieExtendsHeader,
    MovieFragment,
    MovieFragmentHeader,
    MovieHeader,
    NarratorTag,
    OpusDsConfig,
    OriginalArtistTag,
    OwnerTag,
    PixelAspectRatio,
    PodcastCategoryTag,
    PodcastKeywordsTag,
    PodcastTag,
    ProducerTag,
    PublisherTag,
    PurchaseDateTag,
    RatingTag,
    RecordingCopyrightTag,
    SampleDescription,
    SampleSize,
    SampleTable,
    SampleToChunk,
    SegmentIndex,
    ShowMovementTag,
    Skip,
    SoloistTag,
    SortAlbumArtistTag,
    SortAlbumTag,
    SortArtistTag,
    SortComposerTag,
    SortNameTag,
    SortShowNameTag,
    SoundMediaHeader,
    SubtitleSampleEntryText,
    SubtitleSampleEntryTimedText,
    SubtitleSampleEntryXml,
    SyncSample,
    TempoTag,
    TextConfig,
    TimeToSample,
    Track,
    TrackArtistUrl,
    TrackExtends,
    TrackFragment,
    TrackFragmentHeader,
    TrackFragmentRun,
    TrackHeader,
    TrackNumberTag,
    TrackTitleTag,
    TvEpisodeNameTag,
    TvEpisodeNumberTag,
    TvNetworkNameTag,
    TvSeasonNumberTag,
    TvShowNameTag,
    UrlPodcastTag,
    UserData,
    Uuid,
    VisualSampleEntryAv1,
    VisualSampleEntryAvc1,
    VisualSampleEntryDvh1,
    VisualSampleEntryDvhe,
    VisualSampleEntryHev1,
    VisualSampleEntryHvc1,
    VisualSampleEntryMp4v,
    VisualSampleEntryVp8,
    VisualSampleEntryVp9,
    WorkTag,
    WriterTag,
    XidTag,
    Other([u8; 4]),
}

impl From<[u8; 4]> for AtomType {
    fn from(val: [u8; 4]) -> Self {
        match &val {
            b".mp3" => AtomType::AudioSampleEntryMp3,
            b"ac-3" => AtomType::AudioSampleEntryAc3,
            b"alac" => AtomType::AudioSampleEntryAlac,
            b"alaw" => AtomType::AudioSampleEntryALaw,
            b"av01" => AtomType::VisualSampleEntryAv1,
            b"avc1" => AtomType::VisualSampleEntryAvc1,
            b"avcC" => AtomType::AvcConfiguration,
            b"btrt" => AtomType::BitRate,
            b"ec-3" => AtomType::AudioSampleEntryEc3,
            b"clap" => AtomType::CleanAperture,
            b"co64" => AtomType::ChunkOffset64,
            b"ctts" => AtomType::CompositionTimeToSample,
            b"dac3" => AtomType::Ac3Config,
            b"dec3" => AtomType::Eac3Config,
            b"data" => AtomType::MetaTagData,
            b"dfLa" => AtomType::FlacDsConfig,
            b"dOps" => AtomType::OpusDsConfig,
            b"dvcC" => AtomType::DolbyVisionConfiguration,
            b"dvh1" => AtomType::VisualSampleEntryDvh1,
            b"dvhe" => AtomType::VisualSampleEntryDvhe,
            b"dvvC" => AtomType::DolbyVisionConfiguration,
            b"edts" => AtomType::Edit,
            b"elst" => AtomType::EditList,
            b"esds" => AtomType::Esds,
            b"fl32" => AtomType::AudioSampleEntryF32,
            b"fl64" => AtomType::AudioSampleEntryF64,
            b"fLaC" => AtomType::AudioSampleEntryFlac,
            b"free" => AtomType::Free,
            b"ftyp" => AtomType::FileType,
            b"hdlr" => AtomType::Handler,
            b"hev1" => AtomType::VisualSampleEntryHev1,
            b"hvc1" => AtomType::VisualSampleEntryHvc1,
            b"hvcC" => AtomType::HevcConfiguration,
            b"ilst" => AtomType::MetaList,
            b"in24" => AtomType::AudioSampleEntryS24,
            b"in32" => AtomType::AudioSampleEntryS32,
            b"lpcm" => AtomType::AudioSampleEntryLpcm,
            b"mdat" => AtomType::MediaData,
            b"mdhd" => AtomType::MediaHeader,
            b"mdia" => AtomType::Media,
            b"mean" => AtomType::MetaTagMeaning,
            b"mehd" => AtomType::MovieExtendsHeader,
            b"meta" => AtomType::Meta,
            b"mfhd" => AtomType::MovieFragmentHeader,
            b"minf" => AtomType::MediaInfo,
            b"moof" => AtomType::MovieFragment,
            b"moov" => AtomType::Movie,
            b"mp4a" => AtomType::AudioSampleEntryMp4a,
            b"mp4v" => AtomType::VisualSampleEntryMp4v,
            b"mvex" => AtomType::MovieExtends,
            b"mvhd" => AtomType::MovieHeader,
            b"name" => AtomType::MetaTagName,
            b"Opus" => AtomType::AudioSampleEntryOpus,
            b"pasp" => AtomType::PixelAspectRatio,
            b"raw " => AtomType::AudioSampleEntryU8,
            b"sbtt" => AtomType::SubtitleSampleEntryText,
            b"sidx" => AtomType::SegmentIndex,
            b"skip" => AtomType::Skip,
            b"smhd" => AtomType::SoundMediaHeader,
            b"sowt" => AtomType::AudioSampleEntryS16Le,
            b"stbl" => AtomType::SampleTable,
            b"stco" => AtomType::ChunkOffset,
            b"stpp" => AtomType::SubtitleSampleEntryXml,
            b"stsc" => AtomType::SampleToChunk,
            b"stsd" => AtomType::SampleDescription,
            b"stss" => AtomType::SyncSample,
            b"stsz" => AtomType::SampleSize,
            b"stts" => AtomType::TimeToSample,
            b"tfhd" => AtomType::TrackFragmentHeader,
            b"tkhd" => AtomType::TrackHeader,
            b"traf" => AtomType::TrackFragment,
            b"trak" => AtomType::Track,
            b"trex" => AtomType::TrackExtends,
            b"trun" => AtomType::TrackFragmentRun,
            b"twos" => AtomType::AudioSampleEntryS16Be,
            b"tx3g" => AtomType::SubtitleSampleEntryTimedText,
            b"txtC" => AtomType::TextConfig,
            b"udta" => AtomType::UserData,
            b"ulaw" => AtomType::AudioSampleEntryMuLaw,
            b"uuid" => AtomType::Uuid,
            b"vp08" => AtomType::VisualSampleEntryVp8,
            b"vp09" => AtomType::VisualSampleEntryVp9,
            b"wave" => AtomType::AudioSampleEntryQtWave,
            // Metadata Boxes
            b"----" => AtomType::FreeFormTag,
            b"aART" => AtomType::AlbumArtistTag,
            b"akID" => AtomType::ItunesAccountTypeIdTag,
            b"apID" => AtomType::ItunesAccountIdTag,
            b"atID" => AtomType::ItunesArtistIdTag,
            b"catg" => AtomType::PodcastCategoryTag,
            b"cmID" => AtomType::ItunesComposerIdTag,
            b"cnID" => AtomType::ItunesContentIdTag,
            b"covr" => AtomType::CoverTag,
            b"cpil" => AtomType::CompilationTag,
            b"cprt" => AtomType::CopyrightTag,
            b"desc" => AtomType::DescriptionTag,
            b"disk" => AtomType::DiskNumberTag,
            b"egid" => AtomType::IdentPodcastTag,
            b"geID" => AtomType::ItunesGenreIdTag,
            b"gnre" => AtomType::GenreTag,
            b"hdvd" => AtomType::HdVideoTag,
            b"keyw" => AtomType::PodcastKeywordsTag,
            b"ldes" => AtomType::LongDescriptionTag,
            b"ownr" => AtomType::OwnerTag,
            b"pcst" => AtomType::PodcastTag,
            b"pgap" => AtomType::GaplessPlaybackTag,
            b"plID" => AtomType::ItunesPlaylistIdTag,
            b"purd" => AtomType::PurchaseDateTag,
            b"purl" => AtomType::UrlPodcastTag,
            b"rate" => AtomType::RatingTag,
            b"rtng" => AtomType::AdvisoryTag,
            b"sfID" => AtomType::ItunesCountryIdTag,
            b"shwm" => AtomType::ShowMovementTag,
            b"soaa" => AtomType::SortAlbumArtistTag,
            b"soal" => AtomType::SortAlbumTag,
            b"soar" => AtomType::SortArtistTag,
            b"soco" => AtomType::SortComposerTag,
            b"sonm" => AtomType::SortNameTag,
            b"sosn" => AtomType::SortShowNameTag,
            b"stik" => AtomType::MediaTypeTag,
            b"tmpo" => AtomType::TempoTag,
            b"trkn" => AtomType::TrackNumberTag,
            b"tven" => AtomType::TvEpisodeNameTag,
            b"tves" => AtomType::TvEpisodeNumberTag,
            b"tvnn" => AtomType::TvNetworkNameTag,
            b"tvsh" => AtomType::TvShowNameTag,
            b"tvsn" => AtomType::TvSeasonNumberTag,
            b"xid " => AtomType::XidTag,
            b"\xa9alb" => AtomType::AlbumTag,
            b"\xa9arg" => AtomType::ArrangerTag,
            b"\xa9ART" => AtomType::ArtistTag,
            b"\xa9aut" => AtomType::AuthorTag,
            b"\xa9cmt" => AtomType::CommentTag,
            b"\xa9com" => AtomType::ComposerTag,
            b"\xa9con" => AtomType::ConductorTag,
            b"\xa9day" => AtomType::DateTag,
            b"\xa9enc" => AtomType::EncodedByTag,
            b"\xa9gen" => AtomType::CustomGenreTag,
            b"\xa9grp" => AtomType::GroupingTag,
            b"\xa9isr" => AtomType::IsrcTag,
            b"\xa9lab" => AtomType::LabelTag,
            b"\xa9lal" => AtomType::LabelUrlTag,
            b"\xa9lyr" => AtomType::LyricsTag,
            b"\xa9mal" => AtomType::FileCreatorUrlTag,
            b"\xa9mvc" => AtomType::MovementCountTag,
            b"\xa9mvi" => AtomType::MovementIndexTag,
            b"\xa9mvn" => AtomType::MovementTag,
            b"\xa9nam" => AtomType::TrackTitleTag,
            b"\xa9nrt" => AtomType::NarratorTag,
            b"\xa9ope" => AtomType::OriginalArtistTag,
            b"\xa9phg" => AtomType::RecordingCopyrightTag,
            b"\xa9prd" => AtomType::ProducerTag,
            b"\xa9prl" => AtomType::TrackArtistUrl,
            b"\xa9pub" => AtomType::PublisherTag,
            b"\xa9sol" => AtomType::SoloistTag,
            b"\xa9too" => AtomType::EncoderTag,
            b"\xa9wrk" => AtomType::WorkTag,
            b"\xa9wrt" => AtomType::WriterTag,
            _ => AtomType::Other(val),
        }
    }
}

/// Atom iterator errors.
pub enum AtomError {
    /// The atom's size is invalid.
    InvalidAtomSize,
    /// Invalid UTF-8 was encountered while reading a UTF-8 string from an atom.
    InvalidUtf8,
    /// The maximum iteration depth has been reached.
    MaximumDepthReached,
    /// There is no parent atom.
    NoParentAtom,
    /// The iterator is not ready
    NoPendingAtom,
    /// The parent atom was overrun while reading.
    Overrun,
    /// The seek is out-of-range.
    SeekOutOfRange,
    /// The atom ended while read from it.
    UnexpectedEndOfAtom,
    /// The underlying reader is in an unexpected position.
    UnexpectedPosition,
    /// Unexpected primitive operation.
    UnexpectedReadOperation,
    /// An unknown size atom was nested within a parent atom with a size.
    UnexpectedUnknownSizeAtom,
    /// The size of the atom is unknown,
    UnknownAtomSize,
    /// Other Symphonia errors encountered during atom parsing.
    Other(symphonia_core::errors::Error),
}

impl From<std::io::Error> for AtomError {
    fn from(err: std::io::Error) -> AtomError {
        AtomError::Other(symphonia_core::errors::Error::IoError(err))
    }
}

impl From<symphonia_core::errors::Error> for AtomError {
    fn from(value: symphonia_core::errors::Error) -> Self {
        AtomError::Other(value)
    }
}

/// Atom iterator result.
pub type Result<T> = std::result::Result<T, AtomError>;

/// Convenience function to create a decode error within an `AtomError`.
pub(crate) fn decode_error<T>(desc: &'static str) -> Result<T> {
    Err(AtomError::Other(symphonia_core::errors::Error::DecodeError(desc)))
}

/// Convenience function to create an unsupport feature error within an `AtomError`.
pub(crate) fn unsupported_error<T>(feature: &'static str) -> Result<T> {
    Err(AtomError::Other(symphonia_core::errors::Error::Unsupported(feature)))
}

/// A super-trait of `ReadBytes` and `SeekBuffered` that all readers of `AtomIterator` must
/// implement.
pub(crate) trait ReadAtom: ReadBytes + SeekBuffered {}

/// Atom header.
#[derive(Copy, Clone, Debug)]
pub struct AtomHeader {
    /// The atom type.
    atom_type: AtomType,
    /// The size of all reader headers.
    header_len: u8,
    /// The absolute position of the atom.
    atom_pos: u64,
    /// The total size of the atom including all headers.
    atom_len: Option<NonZeroU64>,
}

impl AtomHeader {
    /// Size of a standard atom header.
    pub const HEADER_SIZE: u8 = 8;
    /// Size of a standard atom header with a 64-bit size.
    const LARGE_HEADER_SIZE: u8 = AtomHeader::HEADER_SIZE + 8;

    /// Reads an atom header from the provided `ByteStream`.
    pub fn read<R: ReadBytes>(reader: &mut R) -> Result<AtomHeader> {
        let atom_pos = reader.pos();

        let atom_len = u64::from(reader.read_be_u32()?);
        let atom_type = AtomType::from(reader.read_quad_bytes()?);

        let (header_len, atom_len) = match atom_len {
            0 => {
                // An atom size of 0 indicates the atom spans the remainder of the stream or file.
                (AtomHeader::HEADER_SIZE, None)
            }
            1 => {
                // An atom size of 1 indicates a 64-bit atom size should be read.
                let large_atom_len = reader.read_be_u64()?;

                // The atom size should be atleast the size of the header.
                if large_atom_len < u64::from(AtomHeader::LARGE_HEADER_SIZE) {
                    return Err(AtomError::InvalidAtomSize);
                }

                (AtomHeader::LARGE_HEADER_SIZE, NonZeroU64::new(large_atom_len))
            }
            _ => {
                // The atom size should be atleast the size of the header.
                if atom_len < u64::from(AtomHeader::HEADER_SIZE) {
                    return Err(AtomError::InvalidAtomSize);
                }

                (AtomHeader::HEADER_SIZE, NonZeroU64::new(atom_len))
            }
        };

        // The end position of the atom must not exceed u64::MAX.
        if atom_len.is_some_and(|len| len.get() > u64::MAX - atom_pos) {
            return Err(AtomError::InvalidAtomSize);
        }

        Ok(AtomHeader { atom_type, atom_pos, atom_len, header_len })
    }

    /// Get the atom type.
    pub fn atom_type(&self) -> AtomType {
        self.atom_type
    }

    /// The atom's end position.
    pub fn end(&self) -> Option<u64> {
        self.atom_len.map(|len| self.atom_pos + len.get())
    }

    /// Get the atom position.
    pub fn pos(&self) -> u64 {
        self.atom_pos
    }

    pub fn data_pos(&self) -> u64 {
        self.atom_pos + u64::from(self.header_len)
    }

    /// If known, get the total atom size.
    pub fn size(&self) -> Option<NonZeroU64> {
        self.atom_len
    }

    /// If the atom size is known, get the total payload data size.
    ///
    /// NOTE: This size includes any UUID, version, or flags fields.
    pub fn data_size(&self) -> Option<u64> {
        self.atom_len.map(|atom_len| atom_len.get() - u64::from(self.header_len))
    }
}

/// Trait for ISO Base Media File Format (ISOBMFF) Atom.
pub trait Atom: Sized {
    fn read<R: ReadAtom>(it: &mut AtomIterator<R>, header: &AtomHeader) -> Result<Self>;
}

/// An ISO Base Media File Format (ISOBMFF) Atom iterator supporting hierarchical traversal.
pub struct AtomIterator<R: ReadAtom> {
    /// The inner reader.
    reader: R,
    /// Stack tracking the ancestors of the current atom.
    stack: Vec<AtomHeader>,
    /// The header of the current atom pending to be read, if there is one. If this is `Some`, then
    /// primitive reading operations are disabled.
    pending: Option<AtomHeader>,
    /// The length of the container, if known.
    len: Option<u64>,
}

impl<R: ReadAtom> AtomIterator<R> {
    /// Instantiate a new atom iterator.
    pub(crate) fn new(reader: R, len: Option<u64>) -> Self {
        let stack = Vec::with_capacity(MAX_ITERATION_DEPTH);
        AtomIterator { reader, stack, pending: None, len }
    }

    /// Consume the iterator and return the inner reader.
    pub(crate) fn into_inner(self) -> R {
        self.reader
    }

    /// Get an immutable reference to the pending atom.
    pub(crate) fn pending(&self) -> Option<&AtomHeader> {
        self.pending.as_ref()
    }

    /// Read the header of the next atom.
    ///
    /// Once an atom header is read its body must be read with `read_atom`, discarded with
    /// `skip_atom`, or undone with `restart_atom`. Primitive reading operations will fail
    /// until one of these calls are made.
    ///
    /// Discards any unread data from the previous atom.
    pub(crate) fn next_header(&mut self) -> Result<Option<&AtomHeader>> {
        // If there is a pending atom, or it wasn't fully consumed, skip over it now.
        let _ = self.skip_atom();

        // Get the parent atom's end position, if available
        let parent_end = self.stack.last().map(|parent| parent.end()).unwrap_or(self.len);

        if let Some(parent_end) = parent_end {
            let pos = self.reader.pos();

            if pos == parent_end {
                return Ok(None);
            }
            else if pos > parent_end {
                // The parent atom was overrun.
                log::warn!("overran atom by {} bytes", pos - parent_end);
                return Err(AtomError::Overrun);
            }
            else if parent_end - pos < u64::from(AtomHeader::HEADER_SIZE) {
                // Remaining data length is not enough for another atom header to be read.
                // Iteration of the current parent atom is done.
                return Ok(None);
            }
        }

        let atom = AtomHeader::read(&mut self.reader)?;

        // let indent = 2 * self.stack.len();
        // log::trace!(
        //     "{:indent$}type={:?}, pos={}, size={}, end_pos={}",
        //     "",
        //     header.atom_type(),
        //     header.pos(),
        //     header.size().map(|len| len.get()).unwrap_or(u64::MAX),
        //     header.end().unwrap_or(u64::MAX),
        // );

        // If the atom has an unknown size (it extends to the end of the file), then all parent
        // atoms must also have an unknown size. In practice, only top-level atoms should have an
        // unknown size.
        if atom.size().is_none() && self.stack.iter().rev().any(|parent| parent.size().is_some()) {
            // Seek back to the start of the atom since it is impossible to proceed past this.
            self.reader.seek_buffered(atom.pos());
            return Err(AtomError::UnexpectedUnknownSizeAtom);
        }

        // Perform checks after header parsing.
        if let Some(parent_end) = parent_end {
            let pos = self.reader.pos();

            if pos > parent_end {
                log::debug!("atom header out-of-bounds, ignoring");
                // Seek back to parent_end, it will always succeed
                self.reader.seek_buffered(parent_end);
                return Ok(None);
            }

            if let Some(child_end) = atom.end() {
                if child_end > parent_end {
                    log::debug!("atom end position exceeds parent's end position, skipping atom");
                    self.reader.ignore_bytes(parent_end - pos)?;
                    return Ok(None);
                }
            }
        }

        self.pending = Some(atom);
        Ok(self.pending.as_ref())
    }

    /// If an atom is pending to be read, or its body was partially read, skip over it.
    pub(crate) fn skip_atom(&mut self) -> Result<()> {
        let atom = self.pending.take().ok_or(AtomError::NoPendingAtom)?;

        match atom.end() {
            Some(end) => {
                let pos = self.reader.pos();

                if pos > end {
                    // The atom was overrun while it was being read.
                    log::warn!("overran atom by {} bytes", pos - end);
                    return Err(AtomError::Overrun);
                }

                if pos < end {
                    // The atom has unread data, skip it.
                    // log::debug!("skipping {} unread bytes", end - pos);
                    self.reader.ignore_bytes(end - pos)?;
                }
            }
            _ => {
                // The atom has an unknown size. It is not possible to know if there is unread data.
                // We can only assume the caller knows the atom has ended.
                log::debug!("skipping atom with an unknown size");
            }
        }

        Ok(())
    }

    /// Read a pending atom.
    pub(crate) fn read_atom<A: Atom>(&mut self) -> Result<A> {
        // Do not allow excessive recursion.
        if self.stack.len() >= MAX_ITERATION_DEPTH {
            return Err(AtomError::MaximumDepthReached);
        }

        // It is a coding error to attempt to read a pending atom without first reading its header
        // with `next_header`.
        let atom = self.pending.take().ok_or(AtomError::NoPendingAtom)?;

        // Push the header of the atom being read onto the stack.
        self.stack.push(atom);

        // Read the atom. On error, we still want to pop the atom so that iteration can continue
        // like normal, so don't abort if this errors.
        let result = A::read(self, &atom);

        // Pop the atom.
        self.pending = self.stack.pop();

        // Skip over any unread data left in the atom.
        let _ = self.skip_atom();

        result
    }

    /// If an atom is pending to be read, repositions the iterator and inner reader to the start
    /// of the pending atom.
    ///
    /// Attempts to cheaply seek within the cache first, before seeking the underlying reader.
    pub(crate) fn seek_atom_start(&mut self) -> Result<()>
    where
        R: MediaSource,
    {
        let atom = self.pending.take().ok_or(AtomError::NoPendingAtom)?;
        self.seek_reader(atom.pos())?;
        Ok(())
    }

    /// If an atom is pending to be read, repositions the iterator and inner reader to the end of
    /// the pending atom.
    ///
    /// Attempts to cheaply seek within the cache first, before seeking the underlying reader.
    pub(crate) fn seek_atom_end(&mut self) -> Result<()>
    where
        R: MediaSource,
    {
        let atom = self.pending.take().ok_or(AtomError::NoPendingAtom)?;

        match atom.end() {
            Some(end) => self.seek_reader(end),
            _ => {
                // The atom has an unknown size. It is not possible to know where it ends.
                Err(AtomError::UnknownAtomSize)
            }
        }
    }

    /// Seek the inner reader to the desired position.
    ///
    /// This seek function first attempts to seek within the cache, before attempting to seek
    /// the media source. If the media source is not seekable, then forward seeks are emulated
    /// by ignoring bytes until the desired position is reached.
    fn seek_reader(&mut self, pos: u64) -> Result<()>
    where
        R: MediaSource,
    {
        // Attempt a seek within the cache first.
        if self.reader.seek_buffered(pos) != pos {
            if self.reader.is_seekable() {
                // Fallback to a slow seek if the stream is seekable.
                self.reader.seek(SeekFrom::Start(pos))?;
            }
            else if pos > self.reader.pos() {
                // The stream is not seekable but the desired seek position is ahead of the reader's
                // current position, thus the seek can be emulated by ignoring the bytes up to the
                // the desired seek position.
                self.reader.ignore_bytes(pos - self.reader.pos())?;
            }
            else {
                // The stream is not seekable and the desired seek position falls outside the lower
                // bound of the buffer cache. This sample cannot be read.
                return Err(AtomError::SeekOutOfRange);
            }
        }
        Ok(())
    }

    /// Get the amount of data left in the atom if the atom's size is known.
    pub(crate) fn data_left(&self) -> Result<Option<u64>> {
        // Must be currently reading the payload of an atom (has a parent).
        let parent = self.stack.last().ok_or(AtomError::NoParentAtom)?;

        match parent.end() {
            Some(end) => {
                let pos = self.reader.pos();

                if pos < parent.data_pos() {
                    // The reader's current position is before atom payload. This is most likely
                    // programmer error not resynchronizing the iterator and reader after a raw
                    // read.
                    return Err(AtomError::UnexpectedPosition);
                }

                let rem = end.saturating_sub(pos);
                Ok(Some(rem))
            }
            _ => Ok(None),
        }
    }

    /// Reads exactly the number of bytes requested, at the specified position.
    ///
    /// After this operation, it is only safe to make more raw read calls. To continue iteration,
    /// the iterator must be re-synchronized by calling `seek_atom_start` or `seek_atom_end`.
    pub(crate) fn read_raw_boxed_slice_exact(&mut self, pos: u64, len: usize) -> Result<Box<[u8]>>
    where
        R: MediaSource,
    {
        // Must currently have a pending atom to allow resynchronizing to it.
        let _ = self.pending.as_ref().ok_or(AtomError::NoPendingAtom)?;

        // Seek to the desired position. Doesn't seek if already in position.
        self.seek_reader(pos)?;

        // Do the read.
        let data = self.reader.read_boxed_slice_exact(len)?;

        Ok(data)
    }

    //
    // Reading primitives
    //
    // The primitive reading functions are only usable when there is no pending atom read and there
    // is a parent atom. In otherwords, when currently reading the payload of an atom.

    /// Ensure there is no pending atom read.
    #[inline]
    fn ensure_no_pending_atom(&self) -> Result<()> {
        if self.pending.is_some() { Err(AtomError::UnexpectedReadOperation) } else { Ok(()) }
    }

    /// Ensure there is no pending atom read, and that there is enough data left in the parent atom
    /// to read from.
    fn ensure_parent_atom_data(&self, num_bytes: u64) -> Result<()> {
        // Ensure there is no pending atom to be read.
        self.ensure_no_pending_atom()?;

        // If the current atom has a known size, then check if there is enough data left to read.
        if let Some(remaining) = self.data_left()? {
            if remaining < num_bytes {
                return Err(AtomError::UnexpectedEndOfAtom);
            }
        }

        Ok(())
    }

    /// Reads the extended header fields.
    #[inline]
    pub(crate) fn read_extended_header(&mut self) -> Result<(u8, u32)> {
        self.ensure_parent_atom_data(4)?;
        Ok((self.reader.read_u8()?, self.reader.read_be_u24()?))
    }

    /// Reads exactly the number of bytes required to fill be provided buffer or returns an error.
    #[inline]
    pub(crate) fn read_buf_exact(&mut self, buf: &mut [u8]) -> Result<()> {
        self.ensure_parent_atom_data(buf.len() as u64)?;
        Ok(self.reader.read_buf_exact(buf)?)
    }

    /// Reads exactly the number of bytes requested, and returns a boxed slice of the data or an
    /// error.
    #[inline]
    pub(crate) fn read_boxed_slice_exact(&mut self, len: usize) -> Result<Box<[u8]>> {
        self.ensure_parent_atom_data(len as u64)?;
        Ok(self.reader.read_boxed_slice_exact(len)?)
    }

    /// Ignores the specified number of bytes from the stream or returns an error.
    #[inline]
    pub(crate) fn ignore_bytes(&mut self, count: u64) -> Result<()> {
        self.ensure_parent_atom_data(count)?;
        Ok(self.reader.ignore_bytes(count)?)
    }

    /// Read a null-terminated UTF-8 string.
    pub(crate) fn read_null_terminated_utf8(&mut self) -> Result<String> {
        // Ensure there is no pending atom to be read.
        self.ensure_no_pending_atom()?;

        // If known, the bytes available in the atom for reading. Otherwise, assume unlimited bytes
        // left.
        let max_bytes = self.data_left()?.unwrap_or(u64::MAX);

        let mut buf = Vec::new();
        let mut num_bytes = 0;

        loop {
            // Do not exceed maximum length.
            if num_bytes >= max_bytes {
                return Err(AtomError::UnexpectedEndOfAtom);
            }

            let byte = self.reader.read_u8()?;
            num_bytes += 1;

            // Break at the null-terminator. Do not add it to the string buffer.
            if byte == 0 {
                break;
            }

            buf.push(byte);
        }

        // Try to convert to a UTF-8 string.
        String::from_utf8(buf).map_err(|_| AtomError::InvalidUtf8)
    }

    /// Reads a single byte from the stream and returns it or an error.
    #[inline]
    pub(crate) fn read_byte(&mut self) -> Result<u8> {
        self.ensure_parent_atom_data(std::mem::size_of::<u8>() as u64)?;
        Ok(self.reader.read_byte()?)
    }

    /// Reads two bytes from the stream and returns them in read-order or an error.
    #[inline]
    pub(crate) fn read_double_bytes(&mut self) -> Result<[u8; 2]> {
        self.ensure_parent_atom_data(std::mem::size_of::<[u8; 2]>() as u64)?;
        Ok(self.reader.read_double_bytes()?)
    }

    /// Reads three bytes from the stream and returns them in read-order or an error.
    #[allow(dead_code)]
    #[inline]
    pub(crate) fn read_triple_bytes(&mut self) -> Result<[u8; 3]> {
        self.ensure_parent_atom_data(std::mem::size_of::<[u8; 3]>() as u64)?;
        Ok(self.reader.read_triple_bytes()?)
    }

    /// Reads four bytes from the stream and returns them in read-order or an error.
    #[inline]
    pub(crate) fn read_quad_bytes(&mut self) -> Result<[u8; 4]> {
        self.ensure_parent_atom_data(std::mem::size_of::<[u8; 4]>() as u64)?;
        Ok(self.reader.read_quad_bytes()?)
    }

    /// Reads a single unsigned byte from the stream and returns it or an error.
    #[inline]
    pub(crate) fn read_u8(&mut self) -> Result<u8> {
        self.read_byte()
    }

    /// Reads a single signed byte from the stream and returns it or an error.
    #[allow(dead_code)]
    #[inline]
    pub(crate) fn read_i8(&mut self) -> Result<i8> {
        Ok(self.read_byte()? as i8)
    }

    /// Reads two bytes from the stream and interprets them as an unsigned 16-bit big-endian
    /// integer or returns an error.
    #[inline]
    pub(crate) fn read_u16(&mut self) -> Result<u16> {
        Ok(u16::from_be_bytes(self.read_double_bytes()?))
    }

    /// Reads two bytes from the stream and interprets them as an signed 16-bit big-endian
    /// integer or returns an error.
    #[allow(dead_code)]
    #[inline]
    pub(crate) fn read_i16(&mut self) -> Result<i16> {
        Ok(i16::from_be_bytes(self.read_double_bytes()?))
    }

    /// Reads three bytes from the stream and interprets them as an unsigned 24-bit big-endian
    /// integer or returns an error.
    #[allow(dead_code)]
    #[inline]
    pub(crate) fn read_u24(&mut self) -> Result<u32> {
        let mut buf = [0u8; std::mem::size_of::<u32>()];
        buf[0..3].clone_from_slice(&self.read_triple_bytes()?);
        Ok(u32::from_be_bytes(buf) >> 8)
    }

    /// Reads three bytes from the stream and interprets them as an signed 24-bit big-endian
    /// integer or returns an error.
    #[allow(dead_code)]
    #[inline]
    pub(crate) fn read_i24(&mut self) -> Result<i32> {
        Ok(((self.read_u24()? << 8) as i32) >> 8)
    }

    /// Reads four bytes from the stream and interprets them as an unsigned 32-bit big-endian
    /// integer or returns an error.
    #[inline]
    pub(crate) fn read_u32(&mut self) -> Result<u32> {
        Ok(u32::from_be_bytes(self.read_quad_bytes()?))
    }

    /// Reads four bytes from the stream and interprets them as a signed 32-bit big-endian
    /// integer or returns an error.
    #[allow(dead_code)]
    #[inline]
    pub(crate) fn read_i32(&mut self) -> Result<i32> {
        Ok(i32::from_be_bytes(self.read_quad_bytes()?))
    }

    /// Reads eight bytes from the stream and interprets them as an unsigned 64-bit big-endian
    /// integer or returns an error.
    #[inline]
    pub(crate) fn read_u64(&mut self) -> Result<u64> {
        let mut buf = [0u8; std::mem::size_of::<u64>()];
        self.read_buf_exact(&mut buf)?;
        Ok(u64::from_be_bytes(buf))
    }

    /// Reads eight bytes from the stream and interprets them as an signed 64-bit big-endian
    /// integer or returns an error.
    #[allow(dead_code)]
    #[inline]
    pub(crate) fn read_i64(&mut self) -> Result<i64> {
        let mut buf = [0u8; std::mem::size_of::<i64>()];
        self.read_buf_exact(&mut buf)?;
        Ok(i64::from_be_bytes(buf))
    }

    /// Reads four bytes from the stream and interprets them as a 32-bit big-endian IEEE-754
    /// floating-point value.
    #[allow(dead_code)]
    #[inline]
    pub(crate) fn read_f32(&mut self) -> Result<f32> {
        Ok(f32::from_be_bytes(self.read_quad_bytes()?))
    }

    /// Reads eight bytes from the stream and interprets them as a 64-bit big-endian IEEE-754
    /// floating-point value.
    #[inline]
    pub(crate) fn read_f64(&mut self) -> Result<f64> {
        let mut buf = [0u8; std::mem::size_of::<u64>()];
        self.read_buf_exact(&mut buf)?;
        Ok(f64::from_be_bytes(buf))
    }
}
