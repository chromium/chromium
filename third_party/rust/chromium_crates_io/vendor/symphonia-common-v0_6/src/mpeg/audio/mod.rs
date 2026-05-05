// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use symphonia_core::audio::{Channels, layouts};
use symphonia_core::errors::{Result, decode_error, unsupported_error};
use symphonia_core::io::{BitReaderLtr, FiniteBitStream, ReadBitsLtr};

/// MPEG4 audio object types.
#[non_exhaustive]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum AudioObjectType {
    #[default]
    None,
    Main,
    Lc,
    Ssr,
    Ltp,
    Sbr,
    Scalable,
    TwinVq,
    Celp,
    Hvxc,
    Ttsi,
    MainSynth,
    WavetableSynth,
    GeneralMidi,
    Algorithmic,
    ErAacLc,
    ErAacLtp,
    ErAacScalable,
    ErTwinVq,
    ErBsac,
    ErAacLd,
    ErCelp,
    ErHvxc,
    ErHiln,
    ErParametric,
    Ssc,
    Ps,
    MpegSurround,
    Layer1,
    Layer2,
    Layer3,
    Dst,
    Als,
    Sls,
    SlsNonCore,
    ErAacEld,
    SmrSimple,
    SmrMain,
    Reserved,
    Unknown,
}

impl std::fmt::Display for AudioObjectType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", AUDIO_OBJECT_TYPE_NAMES[*self as usize])
    }
}

const AUDIO_OBJECT_TYPES: &[AudioObjectType] = &[
    AudioObjectType::None,
    AudioObjectType::Main,
    AudioObjectType::Lc,
    AudioObjectType::Ssr,
    AudioObjectType::Ltp,
    AudioObjectType::Sbr,
    AudioObjectType::Scalable,
    AudioObjectType::TwinVq,
    AudioObjectType::Celp,
    AudioObjectType::Hvxc,
    AudioObjectType::Reserved,
    AudioObjectType::Reserved,
    AudioObjectType::Ttsi,
    AudioObjectType::MainSynth,
    AudioObjectType::WavetableSynth,
    AudioObjectType::GeneralMidi,
    AudioObjectType::Algorithmic,
    AudioObjectType::ErAacLc,
    AudioObjectType::Reserved,
    AudioObjectType::ErAacLtp,
    AudioObjectType::ErAacScalable,
    AudioObjectType::ErTwinVq,
    AudioObjectType::ErBsac,
    AudioObjectType::ErAacLd,
    AudioObjectType::ErCelp,
    AudioObjectType::ErHvxc,
    AudioObjectType::ErHiln,
    AudioObjectType::ErParametric,
    AudioObjectType::Ssc,
    AudioObjectType::Ps,
    AudioObjectType::MpegSurround,
    AudioObjectType::Reserved, // Escape
    AudioObjectType::Layer1,
    AudioObjectType::Layer2,
    AudioObjectType::Layer3,
    AudioObjectType::Dst,
    AudioObjectType::Als,
    AudioObjectType::Sls,
    AudioObjectType::SlsNonCore,
    AudioObjectType::ErAacEld,
    AudioObjectType::SmrSimple,
    AudioObjectType::SmrMain,
];

const AUDIO_OBJECT_TYPE_NAMES: &[&str] = &[
    "None",
    "AAC Main",
    "AAC LC",
    "AAC SSR",
    "AAC LTP",
    "SBR",
    "AAC Scalable",
    "TwinVQ",
    "CELP",
    "HVXC",
    // "(Reserved10)",
    // "(Reserved11)",
    "TTSI",
    "Main synthetic",
    "Wavetable synthesis",
    "General MIDI",
    "Algorithmic Synthesis and Audio FX",
    "ER AAC LC",
    // "(Reserved18)",
    "ER AAC LTP",
    "ER AAC Scalable",
    "ER TwinVQ",
    "ER BSAC",
    "ER AAC LD",
    "ER CELP",
    "ER HVXC",
    "ER HILN",
    "ER Parametric",
    "SSC",
    "PS",
    "MPEG Surround",
    // "(Escape)",
    "Layer-1",
    "Layer-2",
    "Layer-3",
    "DST",
    "ALS",
    "SLS",
    "SLS non-core",
    "ER AAC ELD",
    "SMR Simple",
    "SMR Main",
    "(Reserved)",
    "(Unknown)",
];

/// Try to get the audio object type from the given audio object type index.
pub fn get_mpeg4_audio_object_type_by_index(index: u32) -> Option<AudioObjectType> {
    AUDIO_OBJECT_TYPES.get(index as usize).copied()
}

/// MPEG4 audio sample rate result.
pub enum Mpeg4AudioSampleRate {
    /// The sample rate is known.
    SampleRate(u32),
    /// The sample rate should be read manually.
    ///
    /// For the MPEG4 Audio Specific Config, the sample rate should be read as the next 24-bits
    /// after the sample rate index. For ADTS, this is an error.
    Escape,
    /// The sample rate index was invalid.
    Invalid,
}

/// Try to get the audio sample rate given the sample rate index.
pub fn get_mpeg4_audio_sample_rate_by_index(index: u32) -> Mpeg4AudioSampleRate {
    const MPEG4_AUDIO_SAMPLE_RATES: [u32; 13] =
        [96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350];

    match index {
        0..=12 => Mpeg4AudioSampleRate::SampleRate(MPEG4_AUDIO_SAMPLE_RATES[index as usize]),
        15 => Mpeg4AudioSampleRate::Escape,
        _ => Mpeg4AudioSampleRate::Invalid,
    }
}

/// MPEG4 audio channel configuration result.
pub enum Mpeg4AudioChannels {
    /// The channel configuration is known.
    Channels(Channels),
    /// The channel configuration is defined in-band based on the audio object type (e.g., from the
    /// program configuration element).
    Escape,
    /// The channel configuration index was invalid.
    Invalid,
}

/// Try to get the set of channels given the configuration index.
pub fn get_mpeg4_audio_channels_by_config_index(index: u32) -> Mpeg4AudioChannels {
    let channels = match index {
        0 => return Mpeg4AudioChannels::Escape,
        1 => layouts::CHANNEL_LAYOUT_MONO,
        2 => layouts::CHANNEL_LAYOUT_STEREO,
        3 => layouts::CHANNEL_LAYOUT_AAC_3P0,
        4 => layouts::CHANNEL_LAYOUT_AAC_4P0,
        5 => layouts::CHANNEL_LAYOUT_AAC_5P0,
        6 => layouts::CHANNEL_LAYOUT_AAC_5P1,
        7 => layouts::CHANNEL_LAYOUT_AAC_7P1,
        _ => return Mpeg4AudioChannels::Invalid,
    };
    Mpeg4AudioChannels::Channels(channels)
}

/// MPEG4 Audio Specific Configuration.
#[non_exhaustive]
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct AudioSpecificConfig {
    pub object_type: AudioObjectType,
    pub sample_rate: u32,
    pub channels: Option<Channels>,
    pub samples: usize,
    pub sbr_ps_info: Option<(u32, Option<Channels>)>,
    pub sbr_present: bool,
    pub ps_present: bool,
}

impl AudioSpecificConfig {
    /// Read the audio specific configuration from the provided buffer.
    pub fn read(buf: &[u8]) -> Result<AudioSpecificConfig> {
        let mut bs = BitReaderLtr::new(buf);

        let mut asc = AudioSpecificConfig {
            object_type: Self::read_audio_object_type(&mut bs)?,
            sample_rate: Self::read_sampling_frequency(&mut bs)?,
            ..Default::default()
        };

        if asc.sample_rate == 0 {
            return decode_error("common (mp4a): a sample rate of 0 is invalid");
        }

        asc.channels = Self::read_channel_config(&mut bs)?;

        if (asc.object_type == AudioObjectType::Sbr) || (asc.object_type == AudioObjectType::Ps) {
            let ext_srate = Self::read_sampling_frequency(&mut bs)?;
            asc.object_type = Self::read_audio_object_type(&mut bs)?;

            let ext_chans = if asc.object_type == AudioObjectType::ErBsac {
                Self::read_channel_config(&mut bs)?
            }
            else {
                None
            };

            asc.sbr_ps_info = Some((ext_srate, ext_chans));
        }

        match asc.object_type {
            AudioObjectType::Main
            | AudioObjectType::Lc
            | AudioObjectType::Ssr
            | AudioObjectType::Scalable
            | AudioObjectType::TwinVq
            | AudioObjectType::ErAacLc
            | AudioObjectType::ErAacLtp
            | AudioObjectType::ErAacScalable
            | AudioObjectType::ErTwinVq
            | AudioObjectType::ErBsac
            | AudioObjectType::ErAacLd => {
                // GASpecificConfig
                let short_frame = bs.read_bool()?;

                asc.samples = if short_frame { 960 } else { 1024 };

                let depends_on_core = bs.read_bool()?;

                if depends_on_core {
                    let _delay = bs.read_bits_leq32(14)?;
                }

                let extension_flag = bs.read_bool()?;

                if asc.channels.is_none() {
                    return unsupported_error("common (mp4a): program config element");
                }

                if (asc.object_type == AudioObjectType::Scalable)
                    || (asc.object_type == AudioObjectType::ErAacScalable)
                {
                    let _layer = bs.read_bits_leq32(3)?;
                }

                if extension_flag {
                    if asc.object_type == AudioObjectType::ErBsac {
                        let _num_subframes = bs.read_bits_leq32(5)? as usize;
                        let _layer_length = bs.read_bits_leq32(11)?;
                    }

                    if (asc.object_type == AudioObjectType::ErAacLc)
                        || (asc.object_type == AudioObjectType::ErAacLtp)
                        || (asc.object_type == AudioObjectType::ErAacScalable)
                        || (asc.object_type == AudioObjectType::ErAacLd)
                    {
                        let _section_data_resilience = bs.read_bool()?;
                        let _scalefactors_resilience = bs.read_bool()?;
                        let _spectral_data_resilience = bs.read_bool()?;
                    }

                    let extension_flag3 = bs.read_bool()?;

                    if extension_flag3 {
                        return unsupported_error("common (mp4a): version3 extensions");
                    }
                }
            }
            AudioObjectType::Celp => {
                return unsupported_error("common (mp4a): CELP config");
            }
            AudioObjectType::Hvxc => {
                return unsupported_error("common (mp4a): HVXC config");
            }
            AudioObjectType::Ttsi => {
                return unsupported_error("common (mp4a): TTS config");
            }
            AudioObjectType::MainSynth
            | AudioObjectType::WavetableSynth
            | AudioObjectType::GeneralMidi
            | AudioObjectType::Algorithmic => {
                return unsupported_error("common (mp4a): structured audio config");
            }
            AudioObjectType::ErCelp => {
                return unsupported_error("common (mp4a): ER CELP config");
            }
            AudioObjectType::ErHvxc => {
                return unsupported_error("common (mp4a): ER HVXC config");
            }
            AudioObjectType::ErHiln | AudioObjectType::ErParametric => {
                return unsupported_error("common (mp4a): parametric config");
            }
            AudioObjectType::Ssc => {
                return unsupported_error("common (mp4a): SSC config");
            }
            AudioObjectType::MpegSurround => {
                // bs.ignore_bits(1)?; // sacPayloadEmbedding
                return unsupported_error("common (mp4a): MPEG Surround config");
            }
            AudioObjectType::Layer1 | AudioObjectType::Layer2 | AudioObjectType::Layer3 => {
                return unsupported_error("common (mp4a): MPEG Layer 1/2/3 config");
            }
            AudioObjectType::Dst => {
                return unsupported_error("common (mp4a): DST config");
            }
            AudioObjectType::Als => {
                // bs.ignore_bits(5)?; // fillBits
                return unsupported_error("common (mp4a): ALS config");
            }
            AudioObjectType::Sls | AudioObjectType::SlsNonCore => {
                return unsupported_error("common (mp4a): SLS config");
            }
            AudioObjectType::ErAacEld => {
                return unsupported_error("common (mp4a): ELD config");
            }
            AudioObjectType::SmrSimple | AudioObjectType::SmrMain => {
                return unsupported_error("common (mp4a): symbolic music config");
            }
            _ => {}
        };

        match asc.object_type {
            AudioObjectType::ErAacLc
            | AudioObjectType::ErAacLtp
            | AudioObjectType::ErAacScalable
            | AudioObjectType::ErTwinVq
            | AudioObjectType::ErBsac
            | AudioObjectType::ErAacLd
            | AudioObjectType::ErCelp
            | AudioObjectType::ErHvxc
            | AudioObjectType::ErHiln
            | AudioObjectType::ErParametric
            | AudioObjectType::ErAacEld => {
                let ep_config = bs.read_bits_leq32(2)?;

                if (ep_config == 2) || (ep_config == 3) {
                    return unsupported_error("common (mp4a): error protection config");
                }
                // if ep_config == 3 {
                //     let direct_mapping = bs.read_bit()?;
                //     validate!(direct_mapping);
                // }
            }
            _ => {}
        };

        if asc.sbr_ps_info.is_some() && (bs.bits_left() >= 16) {
            let sync = bs.read_bits_leq32(11)?;

            if sync == 0x2B7 {
                let ext_otype = Self::read_audio_object_type(&mut bs)?;
                if ext_otype == AudioObjectType::Sbr {
                    asc.sbr_present = bs.read_bool()?;
                    if asc.sbr_present {
                        let _ext_srate = Self::read_sampling_frequency(&mut bs)?;
                        if bs.bits_left() >= 12 {
                            let sync = bs.read_bits_leq32(11)?;
                            if sync == 0x548 {
                                asc.ps_present = bs.read_bool()?;
                            }
                        }
                    }
                }
                if ext_otype == AudioObjectType::Ps {
                    asc.sbr_present = bs.read_bool()?;
                    if asc.sbr_present {
                        let _ext_srate = Self::read_sampling_frequency(&mut bs)?;
                    }
                    let _ext_channels = bs.read_bits_leq32(4)?;
                }
            }
        }

        Ok(asc)
    }

    fn read_audio_object_type<B: ReadBitsLtr>(bs: &mut B) -> Result<AudioObjectType> {
        let index = match bs.read_bits_leq32(5)? {
            index if index < 31 => index as usize,
            31 => (bs.read_bits_leq32(6)? + 32) as usize,
            _ => unreachable!(),
        };

        let aot = AUDIO_OBJECT_TYPES.get(index).copied().unwrap_or(AudioObjectType::Unknown);

        Ok(aot)
    }

    fn read_sampling_frequency<B: ReadBitsLtr>(bs: &mut B) -> Result<u32> {
        let index = bs.read_bits_leq32(4)?;
        let rate = match get_mpeg4_audio_sample_rate_by_index(index) {
            Mpeg4AudioSampleRate::SampleRate(rate) => rate,
            Mpeg4AudioSampleRate::Escape => bs.read_bits_leq32(24)?,
            Mpeg4AudioSampleRate::Invalid => {
                return decode_error("common (mp4a): invalid sample rate");
            }
        };
        Ok(rate)
    }

    fn read_channel_config<B: ReadBitsLtr>(bs: &mut B) -> Result<Option<Channels>> {
        let index = bs.read_bits_leq32(4)?;
        let channels = match get_mpeg4_audio_channels_by_config_index(index) {
            Mpeg4AudioChannels::Channels(channels) => Some(channels),
            Mpeg4AudioChannels::Escape => None,
            Mpeg4AudioChannels::Invalid => {
                return decode_error("common (mp4a): invalid channel configuration");
            }
        };
        Ok(channels)
    }
}
