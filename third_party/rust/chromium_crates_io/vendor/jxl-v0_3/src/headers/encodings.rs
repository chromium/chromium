// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use super::{frame_header::PermutationNonserialized, permutation::Permutation};
use crate::{
    bit_reader::BitReader,
    entropy_coding::decode::{Histograms, SymbolReader, unpack_signed},
    error::Error,
};

pub enum U32 {
    Bits(usize),
    BitsOffset { n: usize, off: u32 },
    Val(u32),
}

impl U32 {
    pub fn read(&self, br: &mut BitReader) -> Result<u32, Error> {
        match *self {
            U32::Bits(n) => Ok(br.read_noinline(n)? as u32),
            U32::BitsOffset { n, off } => Ok(br.read_noinline(n)? as u32 + off),
            U32::Val(val) => Ok(val),
        }
    }
}

pub enum U32Coder {
    Direct(U32),
    Select(U32, U32, U32, U32),
}

#[derive(Default)]
pub struct Empty {}

pub trait UnconditionalCoder<Config>
where
    Self: Sized,
{
    type Nonserialized;
    fn read_unconditional(
        config: &Config,
        br: &mut BitReader,
        nonserialized: &Self::Nonserialized,
    ) -> Result<Self, Error>;
}

impl UnconditionalCoder<()> for bool {
    type Nonserialized = Empty;
    fn read_unconditional(
        _: &(),
        br: &mut BitReader,
        _: &Self::Nonserialized,
    ) -> Result<bool, Error> {
        Ok(br.read_noinline(1)? != 0)
    }
}

impl UnconditionalCoder<()> for f32 {
    type Nonserialized = Empty;
    fn read_unconditional(
        _: &(),
        br: &mut BitReader,
        _: &Self::Nonserialized,
    ) -> Result<f32, Error> {
        use crate::util::f16;
        let ret = f16::from_bits(br.read_noinline(16)? as u16);
        if !ret.is_finite() {
            Err(Error::FloatNaNOrInf)
        } else {
            Ok(ret.to_f32())
        }
    }
}

impl UnconditionalCoder<U32Coder> for u32 {
    type Nonserialized = Empty;
    fn read_unconditional(
        config: &U32Coder,
        br: &mut BitReader,
        _: &Self::Nonserialized,
    ) -> Result<u32, Error> {
        let u = match config {
            U32Coder::Direct(u) => u,
            U32Coder::Select(u0, u1, u2, u3) => {
                let selector = br.read_noinline(2)?;
                match selector {
                    0 => u0,
                    1 => u1,
                    2 => u2,
                    _ => u3,
                }
            }
        };
        u.read(br)
    }
}

impl UnconditionalCoder<U32Coder> for i32 {
    type Nonserialized = Empty;
    fn read_unconditional(
        config: &U32Coder,
        br: &mut BitReader,
        nonserialized: &Self::Nonserialized,
    ) -> Result<i32, Error> {
        let u = u32::read_unconditional(config, br, nonserialized)?;
        Ok(unpack_signed(u))
    }
}

impl UnconditionalCoder<()> for u64 {
    type Nonserialized = Empty;
    fn read_unconditional(
        _: &(),
        br: &mut BitReader,
        _: &Self::Nonserialized,
    ) -> Result<u64, Error> {
        match br.read_noinline(2)? {
            0 => Ok(0),
            1 => Ok(1 + br.read_noinline(4)?),
            2 => Ok(17 + br.read_noinline(8)?),
            _ => {
                let mut result: u64 = br.read_noinline(12)?;
                let mut shift = 12;
                while br.read_noinline(1)? == 1 {
                    if shift >= 60 {
                        assert_eq!(shift, 60);
                        return Ok(result | (br.read_noinline(4)? << shift));
                    }
                    result |= br.read_noinline(8)? << shift;
                    shift += 8;
                }
                Ok(result)
            }
        }
    }
}

impl UnconditionalCoder<()> for String {
    type Nonserialized = Empty;
    fn read_unconditional(
        _: &(),
        br: &mut BitReader,
        nonserialized: &Self::Nonserialized,
    ) -> Result<String, Error> {
        let len = u32::read_unconditional(
            &U32Coder::Select(
                U32::Val(0),
                U32::Bits(4),
                U32::BitsOffset { n: 5, off: 16 },
                U32::BitsOffset { n: 10, off: 48 },
            ),
            br,
            nonserialized,
        )?;
        let mut ret = String::new();
        ret.reserve(len as usize);
        for _ in 0..len {
            match br.read_noinline(8) {
                Ok(c) => ret.push(c as u8 as char),
                Err(Error::OutOfBounds(n)) => {
                    // Use saturating arithmetic to prevent underflow on malformed input
                    // ret.len()+1 cannot overflow since ret.len() <= isize::MAX
                    let remaining = (len as usize)
                        .saturating_add(n)
                        .saturating_sub(ret.len() + 1);
                    return Err(Error::OutOfBounds(remaining));
                }
                Err(e) => return Err(e),
            }
        }
        Ok(ret)
    }
}

impl UnconditionalCoder<()> for Permutation {
    type Nonserialized = PermutationNonserialized;
    fn read_unconditional(
        _: &(),
        br: &mut BitReader,
        nonserialized: &Self::Nonserialized,
    ) -> Result<Permutation, Error> {
        // TODO: This is quadratic when incrementally parsing byte by byte,
        // we might want to find a better way of reading the permutation.
        let ret = if nonserialized.permuted {
            let size = nonserialized.num_entries;
            let num_contexts = 8;
            let histograms = Histograms::decode(num_contexts, br, /*allow_lz77=*/ true)?;
            let mut reader = SymbolReader::new(&histograms, br, None)?;
            Permutation::decode(size, 0, &histograms, br, &mut reader)
        } else {
            Ok(Permutation::default())
        };
        br.jump_to_byte_boundary()?;
        ret
    }
}

impl<T: UnconditionalCoder<Config>, Config, const N: usize> UnconditionalCoder<Config> for [T; N] {
    type Nonserialized = T::Nonserialized;
    fn read_unconditional(
        config: &Config,
        br: &mut BitReader,
        nonserialized: &Self::Nonserialized,
    ) -> Result<[T; N], Error> {
        use array_init::try_array_init;
        try_array_init(|_| T::read_unconditional(config, br, nonserialized))
    }
}

pub struct VectorCoder<T: Sized> {
    pub size_coder: U32Coder,
    pub value_coder: T,
}

impl<Config, T: UnconditionalCoder<Config>> UnconditionalCoder<VectorCoder<Config>> for Vec<T> {
    type Nonserialized = T::Nonserialized;
    fn read_unconditional(
        config: &VectorCoder<Config>,
        br: &mut BitReader,
        nonserialized: &Self::Nonserialized,
    ) -> Result<Vec<T>, Error> {
        let len = u32::read_unconditional(&config.size_coder, br, &Empty {})?;
        let mut ret: Vec<T> = Vec::new();
        ret.reserve_exact(len as usize);
        for _ in 0..len {
            ret.push(T::read_unconditional(
                &config.value_coder,
                br,
                nonserialized,
            )?);
        }
        Ok(ret)
    }
}

pub trait ConditionalCoder<Config>
where
    Self: Sized,
{
    type Nonserialized;
    fn read_conditional(
        config: &Config,
        condition: bool,
        br: &mut BitReader,
        nonserialized: &Self::Nonserialized,
    ) -> Result<Self, Error>;
}

impl<Config, T: UnconditionalCoder<Config>> ConditionalCoder<Config> for Option<T> {
    type Nonserialized = T::Nonserialized;
    fn read_conditional(
        config: &Config,
        condition: bool,
        br: &mut BitReader,
        nonserialized: &Self::Nonserialized,
    ) -> Result<Option<T>, Error> {
        if condition {
            Ok(Some(T::read_unconditional(config, br, nonserialized)?))
        } else {
            Ok(None)
        }
    }
}

impl ConditionalCoder<()> for String {
    type Nonserialized = Empty;
    fn read_conditional(
        _: &(),
        condition: bool,
        br: &mut BitReader,
        nonserialized: &Empty,
    ) -> Result<String, Error> {
        if condition {
            String::read_unconditional(&(), br, nonserialized)
        } else {
            Ok(String::new())
        }
    }
}

impl<Config, T: UnconditionalCoder<Config>> ConditionalCoder<VectorCoder<Config>> for Vec<T> {
    type Nonserialized = T::Nonserialized;
    fn read_conditional(
        config: &VectorCoder<Config>,
        condition: bool,
        br: &mut BitReader,
        nonserialized: &Self::Nonserialized,
    ) -> Result<Vec<T>, Error> {
        if condition {
            Vec::read_unconditional(config, br, nonserialized)
        } else {
            Ok(Vec::new())
        }
    }
}

pub trait DefaultedElementCoder<Config, T>
where
    Self: Sized,
{
    type Nonserialized;
    fn read_defaulted_element(
        config: &Config,
        condition: bool,
        default: T,
        br: &mut BitReader,
        nonserialized: &Self::Nonserialized,
    ) -> Result<Self, Error>;
}

impl<Config, T> DefaultedElementCoder<VectorCoder<Config>, T> for Vec<T>
where
    T: UnconditionalCoder<Config> + Clone,
{
    type Nonserialized = T::Nonserialized;

    fn read_defaulted_element(
        config: &VectorCoder<Config>,
        condition: bool,
        default: T,
        br: &mut BitReader,
        nonserialized: &Self::Nonserialized,
    ) -> Result<Self, Error> {
        let len = u32::read_unconditional(&config.size_coder, br, &Empty {})?;
        if condition {
            let mut ret: Vec<T> = Vec::new();
            ret.reserve_exact(len as usize);
            for _ in 0..len {
                ret.push(T::read_unconditional(
                    &config.value_coder,
                    br,
                    nonserialized,
                )?);
            }
            Ok(ret)
        } else {
            Ok(vec![default; len as usize])
        }
    }
}

pub trait DefaultedCoder<Config>
where
    Self: Sized,
{
    type Nonserialized;
    fn read_defaulted(
        config: &Config,
        condition: bool,
        default: Self,
        br: &mut BitReader,
        nonserialized: &Self::Nonserialized,
    ) -> Result<Self, Error>;
}

impl<Config, T: UnconditionalCoder<Config>> DefaultedCoder<Config> for T {
    type Nonserialized = T::Nonserialized;
    fn read_defaulted(
        config: &Config,
        condition: bool,
        default: Self,
        br: &mut BitReader,
        nonserialized: &Self::Nonserialized,
    ) -> Result<T, Error> {
        if condition {
            Ok(T::read_unconditional(config, br, nonserialized)?)
        } else {
            Ok(default)
        }
    }
}

// TODO(veluca93): this will likely need to be implemented differently if
// there are extensions.
#[derive(Debug, PartialEq, Default, Clone)]
pub struct Extensions {}

impl UnconditionalCoder<()> for Extensions {
    type Nonserialized = Empty;
    fn read_unconditional(
        _: &(),
        br: &mut BitReader,
        _: &Self::Nonserialized,
    ) -> Result<Extensions, Error> {
        let selector = u64::read_unconditional(&(), br, &Empty {})?;
        let mut total_size: u64 = 0;
        for i in 0..64 {
            if (selector & (1u64 << i)) != 0 {
                let size = u64::read_unconditional(&(), br, &Empty {})?;
                let sum = total_size.checked_add(size);
                if let Some(s) = sum {
                    total_size = s;
                } else {
                    return Err(Error::SizeOverflow);
                }
            }
        }
        let total_size = usize::try_from(total_size);
        if let Ok(ts) = total_size {
            br.skip_bits(ts)?;
        } else {
            return Err(Error::SizeOverflow);
        }
        Ok(Extensions {})
    }
}
