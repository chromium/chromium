// Symphonia
// Copyright (c) 2019-2024 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::fmt::Debug;

use bitflags::bitflags;

bitflags! {
    /// A bitmask representing positional audio channels.
    ///
    /// The first 18 channel positions are guaranteed to be identical to those specified by the
    /// channel mask in Microsoft's `WAVEFORMATEXTENSIBLE` structure. Positions after the first 18
    /// are defined by Symphonia and are not in any standardized order.
    #[derive(Copy, Clone, Debug, Default, PartialEq, Eq)]
    pub struct Position: u64 {
        /// Front-left (left) channel.
        const FRONT_LEFT          = 1 << 0;
        /// Front-right (right) channel.
        const FRONT_RIGHT         = 1 << 1;
        /// Front-center (center) or the Mono channel.
        const FRONT_CENTER        = 1 << 2;
        /// Low-frequency effects (LFE) channel 1.
        ///
        /// Apple calls this channel "lfe screen".
        const LFE1                = 1 << 3;
        /// Rear-left channel.
        ///
        /// Apple calls this channel "left surround". Dolby calls it "surround rear left".
        const REAR_LEFT           = 1 << 4;
        /// Rear-right channel.
        ///
        /// Apple calls this channel "right surround". Dolby calls it "surround rear right".
        const REAR_RIGHT          = 1 << 5;
        /// Front left-of-center channel.
        ///
        /// Apple and Dolby call this channel "left center".
        const FRONT_LEFT_CENTER   = 1 << 6;
        /// Front right-of-center channel.
        ///
        /// Apple and Dolby call this channel "right center".
        const FRONT_RIGHT_CENTER  = 1 << 7;
        /// Rear-center channel.
        ///
        /// Microsoft calls this channel "back center". Apple calls it "center surround". Dolby
        /// calls it "surround rear center".
        const REAR_CENTER         = 1 << 8;
        /// Side-left channel.
        ///
        /// Apple calls this channel "left surround direct". Dolby calls it "surround left".
        const SIDE_LEFT           = 1 << 9;
        /// Side-right channel.
        ///
        /// Apple calls this channel "right surround direct". Dolby calls it "surround right".
        const SIDE_RIGHT          = 1 << 10;
        /// Top-center channel.
        ///
        /// Apple calls this channel "top center surround".
        const TOP_CENTER          = 1 << 11;
        /// Top-front left channel.
        ///
        /// Apple calls this channel "vertical height left".
        const TOP_FRONT_LEFT      = 1 << 12;
        /// Top-center channel.
        ///
        /// Apple calls this channel "vertical height center".
        const TOP_FRONT_CENTER    = 1 << 13;
        /// Top-front right channel.
        ///
        /// Apple calls this channel "vertical height right".
        const TOP_FRONT_RIGHT     = 1 << 14;
        /// Top-rear left channel.
        ///
        /// Microsoft and Apple call this channel "top back left".
        const TOP_REAR_LEFT       = 1 << 15;
        /// Top-rear center channel.
        ///
        /// Microsoft and Apple call this channel "top back center".
        const TOP_REAR_CENTER     = 1 << 16;
        /// Top-rear right channel.
        ///
        /// Microsoft and Apple call this channel "top back right".
        const TOP_REAR_RIGHT      = 1 << 17;

        // End of standard WAVE channels.

        /// Low-frequency effects channel 2.
        const LFE2                = 1 << 18;
        /// Top-side left channel.
        const TOP_SIDE_LEFT       = 1 << 19;
        /// Top-rear right channel.
        const TOP_SIDE_RIGHT      = 1 << 20;
        /// Bottom-front center channel.
        const BOTTOM_FRONT_CENTER = 1 << 21;
        /// Bottom-front left channel.
        const BOTTOM_FRONT_LEFT   = 1 << 22;
        /// Bottom-front right channel.
        const BOTTOM_FRONT_RIGHT  = 1 << 23;
        /// Front-left wide channel.
        ///
        /// Apple calls this channel "left wide".
        const FRONT_LEFT_WIDE     = 1 << 24;
        /// Front-right wide channel.
        ///
        /// Apple calls this channel "right wide".
        const FRONT_RIGHT_WIDE    = 1 << 25;
    }
}

impl Position {
    /// The number of standard wave channels.
    const NUM_STD_WAVE_CHANNELS: u32 = 18;

    /// Try to create a position mask with the first `count` positions.
    pub fn from_count(count: u32) -> Option<Position> {
        1u64.checked_shl(count).and_then(|shifted| Position::from_bits(shifted - 1))
    }

    /// Try to convert a WAVE channel count into a position mask.
    ///
    /// Only the first 18 bits can be set in a valid WAVE channel bitmask. Therefore, the maximum
    /// valid channel count is also 18. If a count greater than 18 is provided, then this function
    /// will return `None`.
    pub fn from_wave_channel_count(count: u32) -> Option<Position> {
        if count <= Position::NUM_STD_WAVE_CHANNELS {
            // Channel count does not exceed the maximum number of standard WAVE channels.
            Position::from_count(count)
        }
        else {
            None
        }
    }

    /// Try to convert a WAVE channel mask into a position mask.
    ///
    /// Only the first 18 bits can be set in a valid WAVE channel bitmask. If a bit beyond the first
    /// 18 is set, then this function returns `None`.
    pub fn from_wave_channel_mask(mask: u32) -> Option<Position> {
        if mask >> Position::NUM_STD_WAVE_CHANNELS == 0 {
            // The bitmask does not contain any bits outside the standard WAVE channels bitmask.
            Position::from_bits(u64::from(mask))
        }
        else {
            None
        }
    }
}

const POSITION_NAMES: &[&str; 28] = &[
    "FL", "FR", "FC", "LFE1", "RL", "RR", "FLC", "FRC", "RC", "SL", "SR", "TC", "TFL", "TFC",
    "TFR", "TRL", "TRC", "TRR", "LFE2", "TSL", "TSR", "BFC", "BFL", "BFR", "FLW", "FRW", "RLC",
    "RRC",
];

impl std::fmt::Display for Position {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let list = self
            .iter()
            .map(|pos| {
                POSITION_NAMES
                    .get(pos.bits().trailing_zeros() as usize)
                    .unwrap_or(&"???")
                    .to_string()
            })
            .collect::<Vec<_>>()
            .join(",");

        write!(f, "[{list}]")
    }
}

/// Ambisonic B-format channel components up to the 3rd order.
///
/// The traditional B-format only defines components up to the first order. Second and third-order
/// components are defined by Furse-Malham.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum AmbisonicBFormat {
    /// First-order Ambisonic B-format component W.
    W = 0,
    /// First-order Ambisonic B-format component X.
    X,
    /// First-order Ambisonic B-format component Y.
    Y,
    /// First-order Ambisonic B-format component Z.
    Z,
    /// Second-order Ambisonic B-format component R.
    R,
    /// Second-order Ambisonic B-format component S.
    S,
    /// Second-order Ambisonic B-format component T.
    T,
    /// Second-order Ambisonic B-format component U.
    U,
    /// Second-order Ambisonic B-format component V.
    V,
    /// Third-order Ambisonic B-format component K.
    K,
    /// Third-order Ambisonic B-format component L.
    L,
    /// Third-order Ambisonic B-format component M.
    M,
    /// Third-order Ambisonic B-format component N.
    N,
    /// Third-order Ambisonic B-format component O.
    O,
    /// Third-order Ambisonic B-format component P.
    P,
    /// Third-order Ambisonic B-format component Q.
    Q,
}

impl std::fmt::Display for AmbisonicBFormat {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            AmbisonicBFormat::W => write!(f, "W"),
            AmbisonicBFormat::X => write!(f, "X"),
            AmbisonicBFormat::Y => write!(f, "Y"),
            AmbisonicBFormat::Z => write!(f, "Z"),
            AmbisonicBFormat::R => write!(f, "R"),
            AmbisonicBFormat::S => write!(f, "S"),
            AmbisonicBFormat::T => write!(f, "T"),
            AmbisonicBFormat::U => write!(f, "U"),
            AmbisonicBFormat::V => write!(f, "V"),
            AmbisonicBFormat::K => write!(f, "K"),
            AmbisonicBFormat::L => write!(f, "L"),
            AmbisonicBFormat::M => write!(f, "M"),
            AmbisonicBFormat::N => write!(f, "N"),
            AmbisonicBFormat::O => write!(f, "O"),
            AmbisonicBFormat::P => write!(f, "P"),
            AmbisonicBFormat::Q => write!(f, "Q"),
        }
    }
}

/// A channel label.
#[non_exhaustive]
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum ChannelLabel {
    /// The channel is a positioned channel. Only one bit may be set in the position mask.
    Positioned(Position),
    /// The channel is a discrete and independent channel.
    Discrete(u16),
    /// The channel is an Ambisonic component identified by its Ambisonic Channel Number (ACN).
    ///
    /// The order and degree of the channel may be calculated from its ACN (`k`):
    ///
    ///  * order:  `n = floor(sqrt(k))`,
    ///  * degree: `m = k - n * (n + 1)`.
    ///
    /// Ambisonic channels are normalized with Schmidt Semi-Normalization (SN3D).  The
    /// interpretation of the Ambisonics signal as well as detailed definitions of ACN channel
    /// ordering and SN3D normalization are described in AmbiX, Section 2.1.
    Ambisonic(u16),
    /// The channel is an Ambisonic B-format channel component.
    AmbisonicBFormat(AmbisonicBFormat),
}

impl From<Position> for ChannelLabel {
    fn from(value: Position) -> Self {
        ChannelLabel::Positioned(value)
    }
}

impl From<AmbisonicBFormat> for ChannelLabel {
    fn from(value: AmbisonicBFormat) -> Self {
        ChannelLabel::AmbisonicBFormat(value)
    }
}

/// A set of channels.
#[non_exhaustive]
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub enum Channels {
    /// Channels are assigned explict speaker positions. A bit set to 1 indicates that the
    /// corresponding channel position is present.
    Positioned(Position),
    /// Channels 0 to the specified count (0..count) are discrete and independent channels.
    Discrete(u16),
    /// A full Ambisonic channel set of the specified (highest) Ambisonic order.
    ///
    /// The total number of channels is `(1 + n) ^ 2` where `n` is the specified Ambisonic order.
    ///
    /// Each channel in the set is an Ambisonic component ordered and identified by its Ambisonic
    /// Channel Number (ACN).
    ///
    /// The order and degree of each individual channel may be calculated from its ACN (`k`):
    ///
    ///  * order:  `n = floor(sqrt(k))`,
    ///  * degree: `m = k - n * (n + 1)`.
    ///
    /// This representation does not support mixed-order channel sets. To represent a mixed-order
    /// set, a custom list of channel labels may be used instead.
    ///
    /// Ambisonic channels are normalized with Schmidt Semi-Normalization (SN3D).  The
    /// interpretation of the Ambisonics signal as well as detailed definitions of ACN channel
    /// ordering and SN3D normalization are described in AmbiX, Section 2.1.
    Ambisonic(u8),
    /// Channels are identified by channel labels.
    Custom(Box<[ChannelLabel]>),
    /// No channels.
    #[default]
    None,
}

impl Channels {
    /// Get the total number of channels.
    pub fn count(&self) -> usize {
        match self {
            Channels::Positioned(positions) => positions.bits().count_ones() as usize,
            Channels::Discrete(count) => usize::from(*count),
            Channels::Ambisonic(order) => (1 + usize::from(*order)) * (1 + usize::from(*order)),
            Channels::Custom(labels) => labels.len(),
            Channels::None => 0,
        }
    }

    /// Gets the canonical buffer index of a positioned channel given a set of positioned channels.
    ///
    /// # Panics
    ///
    /// Panics if `pos` contains more than one position.
    pub fn get_canonical_index_for_positioned_channel(&self, pos: Position) -> Option<usize> {
        // The selected channel position mask must have exactly a single channel selected.
        assert!(pos.bits().count_ones() == 1, "more than one channel position specified");

        match self {
            Channels::Positioned(positions) => {
                // A channel position mask containing all positions that trail the selected
                // position.
                let trailing = positions.bits() & (pos.bits() - 1);
                Some(trailing.count_ones() as usize)
            }
            _ => None,
        }
    }
}

impl From<Position> for Channels {
    fn from(value: Position) -> Self {
        Channels::Positioned(value)
    }
}

impl From<Vec<ChannelLabel>> for Channels {
    fn from(labels: Vec<ChannelLabel>) -> Self {
        Channels::Custom(labels.into_boxed_slice())
    }
}

impl From<Box<[ChannelLabel]>> for Channels {
    fn from(labels: Box<[ChannelLabel]>) -> Self {
        Channels::Custom(labels)
    }
}

impl std::fmt::Display for Channels {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Channels::Positioned(positions) => std::fmt::Display::fmt(positions, f),
            Channels::Discrete(count) => write!(f, "[D0,..,D{count}]"),
            Channels::Ambisonic(order) => {
                write!(f, "[ACN0,..,ACN{}]", (1 + usize::from(*order)) * (1 + usize::from(*order)))
            }
            Channels::Custom(labels) => {
                let mut list = Vec::new();

                for label in labels.iter() {
                    let name = match label {
                        ChannelLabel::Positioned(pos) => POSITION_NAMES
                            .get(pos.bits().trailing_zeros() as usize)
                            .unwrap_or(&"???")
                            .to_string(),
                        ChannelLabel::Discrete(idx) => format!("D{idx}"),
                        ChannelLabel::Ambisonic(acn) => format!("ACN{acn}"),
                        ChannelLabel::AmbisonicBFormat(fo) => format!("{fo}"),
                    };
                    list.push(name);
                }

                write!(f, "[{}]", list.join(","))
            }
            Channels::None => write!(f, "[]"),
        }
    }
}

pub mod layouts {
    //! Constants for well known, common, or standardized channel layouts.
    //!
    //! The channel layout constants defined in this module are defined as
    //! [`Channels::Positioned`](super::Channels::Positioned) variants.
    //!
    //! ## Naming Conventions
    //!
    //!  * `FRONT` layouts include the front left, front left-of-center, front right-of-center, and
    //!    front right channels.
    //!  * `WIDE` layouts include all channels from the `FRONT` layout plus the front center
    //!    channel.
    //!  * `SIDE` layouts include the side left and side right channels.
    //!  * `XPY` layouts have `X` standard (non-LFE) channels, and `Y` LFE channels.
    //!
    //! ## Channel Order
    //!
    //! Many standardized channel layouts only differ in the order in which the samples for each
    //! channel appear in an audio frame (interleaved audio), or the order of the audio planes
    //! of each channel (planar audio). These different orderings cannot be represented uniquely by
    //! the [`Position`](super::Position) bitmask. Therefore, all such channel layouts will map
    //! to the same value.
    //!
    //! The order of the channels listed in the detailed description of each channel layout are
    //! the order in which the standard describes but is not the order in which Symphonia will store
    //! those channels in a buffer.
    use super::{Channels, Position};

    macro_rules! layout {
        ($($args:expr),*) => {{
            let pos = Position::empty();
            $(
                let pos = pos.union($args);
            )*
            Channels::Positioned(pos)
        }}
    }

    /// Monophonic audio.
    ///
    /// The channels in this layout are:
    /// * Front center
    pub const CHANNEL_LAYOUT_MONO: Channels = layout!(Position::FRONT_CENTER);

    /// Stereophonic audio.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    pub const CHANNEL_LAYOUT_STEREO: Channels =
        layout!(Position::FRONT_LEFT, Position::FRONT_RIGHT);

    /// Stereophonic audio with low-frequency effects.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Low-frequency effects
    pub const CHANNEL_LAYOUT_2P1: Channels =
        layout!(Position::FRONT_LEFT, Position::FRONT_RIGHT, Position::LFE1);

    /// 3.0 audio.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    pub const CHANNEL_LAYOUT_3P0: Channels =
        layout!(Position::FRONT_LEFT, Position::FRONT_RIGHT, Position::FRONT_CENTER);

    /// 3.0 audio with rear surround channel.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Rear center
    pub const CHANNEL_LAYOUT_3P0_REAR: Channels =
        layout!(Position::FRONT_LEFT, Position::FRONT_RIGHT, Position::REAR_CENTER);

    /// 3.1 audio.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    /// * Low-frequency effects
    pub const CHANNEL_LAYOUT_3P1: Channels = layout!(
        Position::FRONT_LEFT,
        Position::FRONT_RIGHT,
        Position::FRONT_CENTER,
        Position::LFE1
    );

    /// Quadrophonic audio with rear channels.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Rear left
    /// * Rear right
    pub const CHANNEL_LAYOUT_4P0_QUAD: Channels = layout!(
        Position::FRONT_LEFT,
        Position::FRONT_RIGHT,
        Position::REAR_LEFT,
        Position::REAR_RIGHT
    );

    /// Quadrophonic audio with side channels.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Side left
    /// * Side right
    pub const CHANNEL_LAYOUT_4P0_QUAD_SIDE: Channels = layout!(
        Position::FRONT_LEFT,
        Position::FRONT_RIGHT,
        Position::SIDE_LEFT,
        Position::SIDE_RIGHT
    );

    /// 4.0 audio.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    /// * Rear center
    pub const CHANNEL_LAYOUT_4P0: Channels = layout!(
        Position::FRONT_LEFT,
        Position::FRONT_RIGHT,
        Position::FRONT_CENTER,
        Position::REAR_CENTER
    );

    /// 4.1 audio.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    /// * Low-frequency effects
    /// * Rear center
    pub const CHANNEL_LAYOUT_4P1: Channels = layout!(
        Position::FRONT_LEFT,
        Position::FRONT_RIGHT,
        Position::FRONT_CENTER,
        Position::LFE1,
        Position::REAR_CENTER
    );

    /// 5.0 audio.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    /// * Rear left
    /// * Rear right
    pub const CHANNEL_LAYOUT_5P0: Channels = layout!(
        Position::FRONT_LEFT,
        Position::FRONT_RIGHT,
        Position::FRONT_CENTER,
        Position::REAR_LEFT,
        Position::REAR_RIGHT
    );

    /// 5.1 audio.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    /// * Low-frequency effects
    /// * Rear left
    /// * Rear right
    pub const CHANNEL_LAYOUT_5P1: Channels = layout!(
        Position::FRONT_LEFT,
        Position::FRONT_RIGHT,
        Position::FRONT_CENTER,
        Position::LFE1,
        Position::REAR_LEFT,
        Position::REAR_RIGHT
    );

    /// 5.0 audio with side channels.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    /// * Side left
    /// * Side right
    pub const CHANNEL_LAYOUT_5P0_SIDE: Channels = layout!(
        Position::FRONT_LEFT,
        Position::FRONT_RIGHT,
        Position::FRONT_CENTER,
        Position::SIDE_LEFT,
        Position::SIDE_RIGHT
    );

    /// 5.1 audio with side channels and low-frequency effects.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    /// * Low-frequency effects
    /// * Side left
    /// * Side right
    pub const CHANNEL_LAYOUT_5P1_SIDE: Channels = layout!(
        Position::FRONT_LEFT,
        Position::FRONT_RIGHT,
        Position::FRONT_CENTER,
        Position::LFE1,
        Position::SIDE_LEFT,
        Position::SIDE_RIGHT
    );

    /// Hexagonal audio.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    /// * Rear left
    /// * Rear right
    /// * Rear center
    pub const CHANNEL_LAYOUT_6P0_HEX: Channels = layout!(
        Position::FRONT_LEFT,
        Position::FRONT_RIGHT,
        Position::FRONT_CENTER,
        Position::REAR_LEFT,
        Position::REAR_RIGHT,
        Position::REAR_CENTER
    );

    /// Hexagonal audio with low-frequency effects.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    /// * Low-frequency effects
    /// * Rear left
    /// * Rear right
    /// * Rear center
    pub const CHANNEL_LAYOUT_6P1_HEX: Channels = layout!(
        Position::FRONT_LEFT,
        Position::FRONT_RIGHT,
        Position::FRONT_CENTER,
        Position::LFE1,
        Position::REAR_LEFT,
        Position::REAR_RIGHT,
        Position::REAR_CENTER
    );

    /// 6.0 audio.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    /// * Side left
    /// * Side right
    /// * Rear center
    pub const CHANNEL_LAYOUT_6P0: Channels = layout!(
        Position::FRONT_LEFT,
        Position::FRONT_RIGHT,
        Position::FRONT_CENTER,
        Position::REAR_CENTER,
        Position::SIDE_LEFT,
        Position::SIDE_RIGHT
    );

    /// 6.1 audio with low-frequency effects.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    /// * Low-frequency effects
    /// * Rear center
    /// * Side left
    /// * Side right
    pub const CHANNEL_LAYOUT_6P1: Channels = layout!(
        Position::FRONT_LEFT,
        Position::FRONT_RIGHT,
        Position::FRONT_CENTER,
        Position::LFE1,
        Position::REAR_CENTER,
        Position::SIDE_LEFT,
        Position::SIDE_RIGHT
    );

    /// 6.0 audio with additional front channels.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front left-of-center
    /// * Front right-of-center
    /// * Side left
    /// * Side right
    pub const CHANNEL_LAYOUT_6P0_FRONT: Channels = layout!(
        Position::FRONT_LEFT,
        Position::FRONT_RIGHT,
        Position::FRONT_LEFT_CENTER,
        Position::FRONT_RIGHT_CENTER,
        Position::SIDE_LEFT,
        Position::SIDE_RIGHT
    );

    /// 6.1 audio with additional front channels and low-frequency effects.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Low-frequency effects
    /// * Front left-of-center
    /// * Front right-of-center
    /// * Side left
    /// * Side right
    pub const CHANNEL_LAYOUT_6P1_FRONT: Channels = layout!(
        Position::FRONT_LEFT,
        Position::FRONT_RIGHT,
        Position::LFE1,
        Position::FRONT_LEFT_CENTER,
        Position::FRONT_RIGHT_CENTER,
        Position::SIDE_LEFT,
        Position::SIDE_RIGHT
    );

    /// 7.0 surround audio.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    /// * Rear left
    /// * Rear right
    /// * Side left
    /// * Side right
    pub const CHANNEL_LAYOUT_7P0: Channels = layout!(
        Position::FRONT_LEFT,
        Position::FRONT_RIGHT,
        Position::FRONT_CENTER,
        Position::REAR_LEFT,
        Position::REAR_RIGHT,
        Position::SIDE_LEFT,
        Position::SIDE_RIGHT
    );

    /// 7.1 surround audio with low-frequency effects.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    /// * Low-frequency effects
    /// * Rear left
    /// * Rear right
    /// * Side left
    /// * Side right
    pub const CHANNEL_LAYOUT_7P1: Channels = layout!(
        Position::FRONT_LEFT,
        Position::FRONT_RIGHT,
        Position::FRONT_CENTER,
        Position::LFE1,
        Position::REAR_LEFT,
        Position::REAR_RIGHT,
        Position::SIDE_LEFT,
        Position::SIDE_RIGHT
    );

    /// 7.0 audio with wide front channels.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    /// * Rear left
    /// * Rear right
    /// * Front left-of-center
    /// * Front right-of-center
    pub const CHANNEL_LAYOUT_7P0_WIDE: Channels = layout!(
        Position::FRONT_LEFT,
        Position::FRONT_RIGHT,
        Position::FRONT_CENTER,
        Position::REAR_LEFT,
        Position::REAR_RIGHT,
        Position::FRONT_LEFT_CENTER,
        Position::FRONT_RIGHT_CENTER
    );

    /// 7.1 audio with wide front channels and low-frequency effects.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    /// * Low-frequency effects
    /// * Rear left
    /// * Rear right
    /// * Front left-of-center
    /// * Front right-of-center
    pub const CHANNEL_LAYOUT_7P1_WIDE: Channels = layout!(
        Position::FRONT_LEFT,
        Position::FRONT_RIGHT,
        Position::FRONT_CENTER,
        Position::LFE1,
        Position::REAR_LEFT,
        Position::REAR_RIGHT,
        Position::FRONT_LEFT_CENTER,
        Position::FRONT_RIGHT_CENTER
    );

    /// 7.0 audio with wide front channels and side channels.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    /// * Front left-of-center
    /// * Front right-of-center
    /// * Side left
    /// * Side right
    pub const CHANNEL_LAYOUT_7P0_WIDE_SIDE: Channels = layout!(
        Position::FRONT_LEFT,
        Position::FRONT_RIGHT,
        Position::FRONT_CENTER,
        Position::FRONT_LEFT_CENTER,
        Position::FRONT_RIGHT_CENTER,
        Position::SIDE_LEFT,
        Position::SIDE_RIGHT
    );

    /// 7.1 audio with wide front channels and side channels.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    /// * Low-frequency effects
    /// * Front left-of-center
    /// * Front right-of-center
    /// * Side left
    /// * Side right
    pub const CHANNEL_LAYOUT_7P1_WIDE_SIDE: Channels = layout!(
        Position::FRONT_LEFT,
        Position::FRONT_RIGHT,
        Position::FRONT_CENTER,
        Position::LFE1,
        Position::FRONT_LEFT_CENTER,
        Position::FRONT_RIGHT_CENTER,
        Position::SIDE_LEFT,
        Position::SIDE_RIGHT
    );

    /// 22.2 audio.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    /// * Low-frequency effects 1
    /// * Rear left
    /// * Rear right
    /// * Front left-of-center
    /// * Front right-of-center
    /// * Rear center
    /// * Low-frequency effects 2
    /// * Side left
    /// * Side right
    /// * Top front left
    /// * Top front right
    /// * Top front center
    /// * Top center
    /// * Top rear left
    /// * Top rear right
    /// * Top side left
    /// * Top side right
    /// * Top rear center
    /// * Bottom front center
    /// * Bottom front left
    /// * Bottom front right
    pub const CHANNEL_LAYOUT_22P2: Channels = layout!(
        Position::FRONT_LEFT,
        Position::FRONT_RIGHT,
        Position::FRONT_CENTER,
        Position::LFE1,
        Position::REAR_LEFT,
        Position::REAR_RIGHT,
        Position::FRONT_LEFT_CENTER,
        Position::FRONT_RIGHT_CENTER,
        Position::REAR_CENTER,
        Position::LFE2,
        Position::SIDE_LEFT,
        Position::SIDE_RIGHT,
        Position::TOP_FRONT_LEFT,
        Position::TOP_FRONT_RIGHT,
        Position::TOP_FRONT_CENTER,
        Position::TOP_CENTER,
        Position::TOP_REAR_LEFT,
        Position::TOP_REAR_RIGHT,
        Position::TOP_SIDE_LEFT,
        Position::TOP_SIDE_RIGHT,
        Position::TOP_REAR_CENTER,
        Position::BOTTOM_FRONT_CENTER,
        Position::BOTTOM_FRONT_LEFT,
        Position::BOTTOM_FRONT_RIGHT
    );

    // Apple-defined Layouts

    /// MPEG 1-channel channel layout.
    ///
    /// The channels in this layout are:
    /// * Front center
    pub const CHANNEL_LAYOUT_MPEG_1P0: Channels = CHANNEL_LAYOUT_MONO;

    /// MPEG 2-channel channel layout.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    pub const CHANNEL_LAYOUT_MPEG_2P0: Channels = CHANNEL_LAYOUT_STEREO;

    /// MPEG 3-channel, configuration A, channel layout.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    pub const CHANNEL_LAYOUT_MPEG_3P0_A: Channels = CHANNEL_LAYOUT_3P0;

    /// MPEG 3-channel, configuration B, channel layout.
    ///
    /// The channels in this layout are:
    /// * Front center
    /// * Front left
    /// * Front right
    pub const CHANNEL_LAYOUT_MPEG_3P0_B: Channels = CHANNEL_LAYOUT_3P0;

    /// MPEG 4-channel, configuration A, channel layout.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    /// * Rear center
    pub const CHANNEL_LAYOUT_MPEG_4P0_A: Channels = CHANNEL_LAYOUT_4P0;

    /// MPEG 4-channel, configuration B, channel layout.
    ///
    /// The channels in this layout are:
    /// * Front center
    /// * Front left
    /// * Front right
    /// * Rear center
    pub const CHANNEL_LAYOUT_MPEG_4P0_B: Channels = CHANNEL_LAYOUT_4P0;

    /// MPEG 5-channel, configuration A, channel layout.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    /// * Rear left
    /// * Rear right
    pub const CHANNEL_LAYOUT_MPEG_5P0_A: Channels = CHANNEL_LAYOUT_5P0;

    /// MPEG 5-channel, configuration B, channel layout.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Rear left
    /// * Rear right
    /// * Front center
    pub const CHANNEL_LAYOUT_MPEG_5P0_B: Channels = CHANNEL_LAYOUT_5P0;

    /// MPEG 5-channel, configuration C, channel layout.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front center
    /// * Front right
    /// * Rear left
    /// * Rear right
    pub const CHANNEL_LAYOUT_MPEG_5P0_C: Channels = CHANNEL_LAYOUT_5P0;

    /// MPEG 5-channel, configuration D, channel layout.
    ///
    /// The channels in this layout are:
    /// * Front center
    /// * Front left
    /// * Front right
    /// * Rear left
    /// * Rear right
    pub const CHANNEL_LAYOUT_MPEG_5P0_D: Channels = CHANNEL_LAYOUT_5P0;

    /// MPEG 5.1-channel, configuration A, channel layout.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    /// * Low-frequency effects
    /// * Rear left
    /// * Rear right
    pub const CHANNEL_LAYOUT_MPEG_5P1_A: Channels = CHANNEL_LAYOUT_5P1;

    /// MPEG 5.1-channel, configuration B, channel layout.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Rear left
    /// * Rear right
    /// * Front center
    /// * Low-frequency effects
    pub const CHANNEL_LAYOUT_MPEG_5P1_B: Channels = CHANNEL_LAYOUT_5P1;

    /// MPEG 5.1-channel, configuration C, channel layout.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front center
    /// * Front right
    /// * Rear left
    /// * Rear right
    /// * Low-frequency effects
    pub const CHANNEL_LAYOUT_MPEG_5P1_C: Channels = CHANNEL_LAYOUT_5P1;

    /// MPEG 5.1-channel, configuration D, channel layout.
    ///
    /// The channels in this layout are:
    /// * Front center
    /// * Front left
    /// * Front right
    /// * Rear left
    /// * Rear right
    /// * Low-frequency effects
    pub const CHANNEL_LAYOUT_MPEG_5P1_D: Channels = CHANNEL_LAYOUT_5P1;

    /// MPEG 6.1-channel, configuration A, channel layout.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    /// * Low-frequency effects
    /// * Rear left
    /// * Rear right
    /// * Rear center
    pub const CHANNEL_LAYOUT_MPEG_6P1_A: Channels = CHANNEL_LAYOUT_6P1_HEX;

    /// MPEG 7.1-channel, configuration A, channel layout.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Front center
    /// * Low-frequency effects
    /// * Rear left
    /// * Rear right
    /// * Front left-of-center
    /// * Front right-of-center
    pub const CHANNEL_LAYOUT_MPEG_7P1_A: Channels = CHANNEL_LAYOUT_7P1_WIDE;

    /// MPEG 7.1-channel, configuration B, channel layout.
    ///
    /// The channels in this layout are:
    /// * Front center
    /// * Front left-of-center
    /// * Front right-of-center
    /// * Front left
    /// * Front right
    /// * Rear left
    /// * Rear right
    /// * Low-frequency effects
    pub const CHANNEL_LAYOUT_MPEG_7P1_B: Channels = CHANNEL_LAYOUT_7P1_WIDE;

    /// AAC 3-channel channel layout.
    ///
    /// The channels in this layout are:
    /// * Front center
    /// * Front left
    /// * Front right
    pub const CHANNEL_LAYOUT_AAC_3P0: Channels = CHANNEL_LAYOUT_MPEG_3P0_B;

    /// AAC quadraphonic surround-based layout.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Rear left
    /// * Rear right
    pub const CHANNEL_LAYOUT_AAC_QUADRAPHONIC: Channels = CHANNEL_LAYOUT_4P0_QUAD;

    /// AAC 4-channel surround-based layout.
    ///
    /// The channels in this layout are:
    /// * Front center
    /// * Front left
    /// * Front right
    /// * Rear center
    pub const CHANNEL_LAYOUT_AAC_4P0: Channels = CHANNEL_LAYOUT_MPEG_4P0_B;

    /// AAC 5-channel surround-based layout.
    ///
    /// The channels in this layout are:
    /// * Front center
    /// * Front left
    /// * Front right
    /// * Rear left
    /// * Rear right
    pub const CHANNEL_LAYOUT_AAC_5P0: Channels = CHANNEL_LAYOUT_MPEG_5P0_D;

    /// AAC 5.1-channel surround-based layout.
    ///
    /// The channels in this layout are:
    /// * Front center
    /// * Front left
    /// * Front right
    /// * Rear left
    /// * Rear right
    /// * Low-frequency effects
    pub const CHANNEL_LAYOUT_AAC_5P1: Channels = CHANNEL_LAYOUT_MPEG_5P1_D;

    /// AAC 6-channel surround-based layout.
    ///
    /// The channels in this layout are:
    /// * Front center
    /// * Front left
    /// * Front right
    /// * Rear left
    /// * Rear right
    /// * Rear center
    pub const CHANNEL_LAYOUT_AAC_6P0: Channels = layout!(
        Position::FRONT_CENTER,
        Position::FRONT_LEFT,
        Position::FRONT_RIGHT,
        Position::REAR_LEFT,
        Position::REAR_RIGHT,
        Position::REAR_CENTER
    );

    /// AAC 6.1-channel surround-based layout.
    ///
    /// The channels in this layout are:
    /// * Front center
    /// * Front left
    /// * Front right
    /// * Rear left
    /// * Rear right
    /// * Rear center
    /// * Low-frequency effects
    pub const CHANNEL_LAYOUT_AAC_6P1: Channels = layout!(
        Position::FRONT_CENTER,
        Position::FRONT_LEFT,
        Position::FRONT_RIGHT,
        Position::REAR_LEFT,
        Position::REAR_RIGHT,
        Position::REAR_CENTER,
        Position::LFE1
    );

    /// AAC 7.1-channel surround-based layout.
    ///
    /// The channels in this layout are:
    /// * Front center
    /// * Front left-of-center
    /// * Front right-of-center
    /// * Front left
    /// * Front right
    /// * Rear left
    /// * Rear right
    /// * Low-frequency effects
    pub const CHANNEL_LAYOUT_AAC_7P1: Channels = CHANNEL_LAYOUT_MPEG_7P1_B;

    /// OGG 1-channel layout.
    ///
    /// The channels in this layout are:
    /// * Front center
    pub const CHANNEL_LAYOUT_OGG_1P0: Channels = CHANNEL_LAYOUT_MONO;

    /// OGG 2-channel layout.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    pub const CHANNEL_LAYOUT_OGG_2P0: Channels = CHANNEL_LAYOUT_STEREO;

    /// OGG 3-channel layout.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front center
    /// * Front right
    pub const CHANNEL_LAYOUT_OGG_3P0: Channels = CHANNEL_LAYOUT_3P0;

    /// OGG 4-channel layout.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front right
    /// * Rear left
    /// * Rear right
    pub const CHANNEL_LAYOUT_OGG_4P0: Channels = CHANNEL_LAYOUT_4P0_QUAD;

    /// OGG 5-channel layout.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front center
    /// * Front right
    /// * Rear left
    /// * Rear right
    pub const CHANNEL_LAYOUT_OGG_5P0: Channels = CHANNEL_LAYOUT_5P0;

    /// OGG 5.1-channel layout.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front center
    /// * Front right
    /// * Rear left
    /// * Rear right
    /// * Low-frequency effects
    pub const CHANNEL_LAYOUT_OGG_5P1: Channels = CHANNEL_LAYOUT_5P1;

    /// OGG 6.1-channel layout.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front center
    /// * Front right
    /// * Side left
    /// * Side right
    /// * Rear center
    /// * Low-frequency effects
    pub const CHANNEL_LAYOUT_OGG_6P1: Channels = CHANNEL_LAYOUT_6P1;

    /// OGG 7.1-channel layout.
    ///
    /// The channels in this layout are:
    /// * Front left
    /// * Front center
    /// * Front right
    /// * Side left
    /// * Side right
    /// * Rear left
    /// * Rear right
    /// * Low-frequency effects
    pub const CHANNEL_LAYOUT_OGG_7P1: Channels = CHANNEL_LAYOUT_7P1;
}
