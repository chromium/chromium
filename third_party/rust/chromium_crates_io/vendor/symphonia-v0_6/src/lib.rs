// Symphonia
// Copyright (c) 2019-2022 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![warn(rust_2018_idioms)]
#![forbid(unsafe_code)]

//! # Project Symphonia
//!
//! Symphonia is a 100% pure Rust audio decoding and multimedia format demuxing framework.
//!
//! # Support
//!
//! Supported formats, codecs, and metadata tags are listed below. By default Symphonia only enables
//! royalty-free open standard media formats and codecs. Other formats and codecs must be enabled
//! using feature flags.
//!
//! **Tip:** All format, codec, and metadata support can be enabled with the `all` feature flag.
//!
//! ## Formats
//!
//! The following container formats are supported.
//!
//! | Format   | Feature Flag | Gapless* | Default |
//! |----------|--------------|----------|---------|
//! | AIFF     | `aiff`       | Yes      | No      |
//! | CAF      | `caf`        | No       | No      |
//! | ISO/MP4  | `isomp4`     | No       | No      |
//! | MKV/WebM | `mkv`        | No       | Yes     |
//! | OGG      | `ogg`        | Yes      | Yes     |
//! | Wave     | `wav`        | Yes      | Yes     |
//!
//! \* Gapless playback requires support from both the demuxer and decoder.
//!
//! **Tip:** All formats can be enabled with the `all-codecs` feature flag.
//!
//! ## Codecs
//!
//! The following codecs are supported.
//!
//! | Codec    | Feature Flag | Gapless | Default |
//! |----------|--------------|---------|---------|
//! | AAC-LC   | `aac`        | No      | No      |
//! | ADPCM    | `adpcm`      | Yes     | Yes     |
//! | ALAC     | `alac`       | Yes     | No      |
//! | FLAC     | `flac`       | Yes     | Yes     |
//! | MP1      | `mp1`, `mpa` | No      | No      |
//! | MP2      | `mp2`, `mpa` | No      | No      |
//! | MP3      | `mp3`, `mpa` | Yes     | No      |
//! | PCM      | `pcm`        | Yes     | Yes     |
//! | Vorbis   | `vorbis`     | Yes     | Yes     |
//!
//! **Tip:** All codecs can be enabled with the `all-codecs` feature flag. Similarly, all MPEG
//! audio codecs can be enabled with the `mpa` feature flag.
//!
//! ## Metadata
//!
//! For metadata formats that are standalone and not part of the media container, a feature flag may
//! be used to toggle support.
//!
//! | Format                | Feature Flag | Default |
//! |-----------------------|--------------|---------|
//! | APEv1                 | `ape`        | Yes     |
//! | APEv2                 | `ape`        | Yes     |
//! | ID3v1                 | `id3v1`      | Yes     |
//! | ID3v2                 | `id3v2`      | Yes     |
//! | ISO/MP4               | N/A          | N/A     |
//! | RIFF                  | N/A          | N/A     |
//! | Vorbis comment (FLAC) | N/A          | N/A     |
//! | Vorbis comment (OGG)  | N/A          | N/A     |
//!
//! **Tip:** All metadata formats can be enabled with the `all-meta` feature flag.
//!
//! ## Optimizations
//!
//! SIMD optimizations are enabled by default. Precise control over which SIMD instruction sets are
//! supported may be controlled using the following feature flags. Enabling any SIMD support feature
//! flag will pull in the `rustfft` dependency.
//!
//! | Instruction Set | Feature Flag    | Default |
//! |-----------------|-----------------|---------|
//! | SSE             | `opt-simd-sse`  | Yes     |
//! | AVX             | `opt-simd-avx`  | Yes     |
//! | Neon            | `opt-simd-neon` | Yes     |
//!
//! **Tip:** All SIMD optimizations can be enabled with the `opt-simd` feature flag.
//!
//! # Experimental Features
//!
//! Previews of experimental new features may be enabled by using feature flags. Experimental
//! features should be used for development purposes only. Before using an experimental feature,
//! please observe the warnings below. Never use experimental features in a production application.
//!
//! | Experimental Feature   | Feature Flag          |
//! |------------------------|-----------------------|
//! | Subtitle codec support | `exp-subtitle-codecs` |
//! | Video codec support    | `exp-video-codecs`    |
//!
//! ## Warnings
//!
//! * SemVer compatibilty is **not** guaranteed. Be prepared for build failures.
//! * Experimental features and their associated feature flags **may be removed at any time.**
//! * Functionality **may change or break at any time.**
//! * Again, **never** use in any production application.
//!
//! # Usage
//!
//! An example implementation of a simple audio player (symphonia-play) can be found in the
//! Project Symphonia git repository.
//!
//! # Adding new formats and codecs
//!
//! Simply implement the [`AudioDecoder`][core::codecs::audio::AudioDecoder] trait for an audio
//! decoder or the [`FormatReader`][core::formats::FormatReader] trait for a demuxer trait and
//! register with the appropriate registry or probe!

pub mod default {
    //! The `default` module provides convenience functions and registries to get an implementer
    //! up-and-running as quickly as possible, and to reduce boiler-plate. Using the `default`
    //! module is completely optional and incurs no overhead unless actually used.

    pub mod codecs {
        //! The `codecs` module re-exports all enabled Symphonia decoders.

        #[cfg(feature = "flac")]
        pub use symphonia_bundle_flac::FlacDecoder;
        #[cfg(any(feature = "mp1", feature = "mp2", feature = "mp3"))]
        pub use symphonia_bundle_mp3::MpaDecoder;
        #[cfg(feature = "aac")]
        pub use symphonia_codec_aac::AacDecoder;
        #[cfg(feature = "adpcm")]
        pub use symphonia_codec_adpcm::AdpcmDecoder;
        #[cfg(feature = "alac")]
        pub use symphonia_codec_alac::AlacDecoder;
        #[cfg(feature = "pcm")]
        pub use symphonia_codec_pcm::PcmDecoder;
        #[cfg(feature = "vorbis")]
        pub use symphonia_codec_vorbis::VorbisDecoder;

        #[deprecated = "use `default::codecs::MpaDecoder` instead"]
        #[cfg(any(feature = "mp1", feature = "mp2", feature = "mp3"))]
        pub type Mp3Decoder = MpaDecoder;
    }

    pub mod formats {
        //! The `formats` module re-exports all enabled Symphonia format readers.

        #[cfg(feature = "flac")]
        pub use symphonia_bundle_flac::FlacReader;
        #[cfg(any(feature = "mp1", feature = "mp2", feature = "mp3"))]
        pub use symphonia_bundle_mp3::MpaReader;
        #[cfg(feature = "aac")]
        pub use symphonia_codec_aac::AdtsReader;
        #[cfg(feature = "caf")]
        pub use symphonia_format_caf::CafReader;
        #[cfg(feature = "isomp4")]
        pub use symphonia_format_isomp4::IsoMp4Reader;
        #[cfg(feature = "mkv")]
        pub use symphonia_format_mkv::MkvReader;
        #[cfg(feature = "ogg")]
        pub use symphonia_format_ogg::OggReader;
        #[cfg(feature = "aiff")]
        pub use symphonia_format_riff::AiffReader;
        #[cfg(feature = "wav")]
        pub use symphonia_format_riff::WavReader;

        #[deprecated = "use `default::formats::MpaReader` instead"]
        #[cfg(any(feature = "mp1", feature = "mp2", feature = "mp3"))]
        pub type Mp3Reader<'s> = MpaReader<'s>;
    }

    pub mod meta {
        //! The `meta` module re-exports all enabled Symphonia metadata readers.

        #[cfg(feature = "ape")]
        pub use symphonia_metadata::ape::ApeReader;
        #[cfg(feature = "id3v1")]
        pub use symphonia_metadata::id3v1::Id3v1Reader;
        #[cfg(feature = "id3v2")]
        pub use symphonia_metadata::id3v2::Id3v2Reader;

        pub use symphonia_metadata::embedded;
    }

    use lazy_static::lazy_static;

    use symphonia_core::codecs::registry::CodecRegistry;
    use symphonia_core::formats::probe::Probe;

    lazy_static! {
        static ref CODEC_REGISTRY: CodecRegistry = {
            let mut registry = CodecRegistry::new();
            register_enabled_codecs(&mut registry);
            registry
        };
    }

    lazy_static! {
        static ref PROBE: Probe = {
            let mut probe: Probe = Default::default();
            register_enabled_formats(&mut probe);
            probe
        };
    }

    /// Gets the default `CodecRegistry`. This registry pre-registers all the codecs selected by the
    /// `feature` flags in the includer's `Cargo.toml`. If `features` is not set, the default set of
    /// Symphonia codecs is registered.
    ///
    /// This function is lazy and does not instantiate the `CodecRegistry` until the first call to
    /// this function.
    pub fn get_codecs() -> &'static CodecRegistry {
        &CODEC_REGISTRY
    }

    /// Gets the default `Probe`. This registry pre-registers all the formats selected by the
    /// `feature` flags in the includer's `Cargo.toml`. If `features` is not set, the default set of
    /// Symphonia formats is registered.
    ///
    /// This function is lazy and does not instantiate the `Probe` until the first call to this
    /// function.
    pub fn get_probe() -> &'static Probe {
        &PROBE
    }

    /// Registers all the codecs selected by the `feature` flags in the includer's `Cargo.toml` on
    /// the provided `CodecRegistry`. If `features` is not set, the default set of Symphonia codecs
    /// is registered.
    ///
    /// Use this function to easily populate a custom registry with all enabled codecs.
    pub fn register_enabled_codecs(registry: &mut CodecRegistry) {
        #[cfg(feature = "aac")]
        registry.register_audio_decoder::<codecs::AacDecoder>();

        #[cfg(feature = "adpcm")]
        registry.register_audio_decoder::<codecs::AdpcmDecoder>();

        #[cfg(feature = "alac")]
        registry.register_audio_decoder::<codecs::AlacDecoder>();

        #[cfg(feature = "flac")]
        registry.register_audio_decoder::<codecs::FlacDecoder>();

        #[cfg(any(feature = "mp1", feature = "mp2", feature = "mp3"))]
        registry.register_audio_decoder::<codecs::MpaDecoder>();

        #[cfg(feature = "pcm")]
        registry.register_audio_decoder::<codecs::PcmDecoder>();

        #[cfg(feature = "vorbis")]
        registry.register_audio_decoder::<codecs::VorbisDecoder>();
    }

    /// Registers all the formats selected by the `feature` flags in the includer's `Cargo.toml` on
    /// the provided `Probe`. If `features` is not set, the default set of Symphonia formats is
    /// registered.
    ///
    /// Use this function to easily populate a custom probe with all enabled formats.
    pub fn register_enabled_formats(probe: &mut Probe) {
        // Formats
        #[cfg(feature = "aac")]
        probe.register_format::<formats::AdtsReader<'_>>();

        #[cfg(feature = "caf")]
        probe.register_format::<formats::CafReader<'_>>();

        #[cfg(feature = "flac")]
        probe.register_format::<formats::FlacReader<'_>>();

        #[cfg(feature = "isomp4")]
        probe.register_format::<formats::IsoMp4Reader<'_>>();

        #[cfg(any(feature = "mp1", feature = "mp2", feature = "mp3"))]
        probe.register_format::<formats::MpaReader<'_>>();

        #[cfg(feature = "aiff")]
        probe.register_format::<formats::AiffReader<'_>>();

        #[cfg(feature = "wav")]
        probe.register_format::<formats::WavReader<'_>>();

        #[cfg(feature = "ogg")]
        probe.register_format::<formats::OggReader<'_>>();

        #[cfg(feature = "mkv")]
        probe.register_format::<formats::MkvReader<'_>>();

        // Metadata
        #[cfg(feature = "ape")]
        probe.register_metadata::<meta::ApeReader<'_>>();

        #[cfg(feature = "id3v1")]
        probe.register_metadata::<meta::Id3v1Reader<'_>>();

        #[cfg(feature = "id3v2")]
        probe.register_metadata::<meta::Id3v2Reader<'_>>();
    }
}

pub use symphonia_core as core;
