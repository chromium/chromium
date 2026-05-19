//! Tokens are what AFL calls extras or dictionaries.
//! They may be inserted as part of mutations during fuzzing.
use alloc::{borrow::Cow, vec::Vec};
#[cfg(any(target_os = "linux", target_vendor = "apple"))]
use core::slice::from_raw_parts;
use core::{
    fmt::Debug,
    mem::size_of,
    num::NonZero,
    ops::{Add, AddAssign, Deref},
    slice::Iter,
};
#[cfg(feature = "std")]
use std::{
    fs::File,
    io::{BufRead, BufReader},
    path::Path,
};

use hashbrown::HashSet;
use libafl_bolts::{AsSlice, HasLen, rands::Rand};
use serde::{Deserialize, Serialize};

#[cfg(feature = "std")]
use crate::mutators::str_decode;
use crate::{
    Error, HasMetadata,
    corpus::{CorpusId, HasCurrentCorpusId},
    inputs::{HasMutatorBytes, ResizableMutator},
    mutators::{
        MultiMutator, MutationResult, Mutator, Named, buffer_self_copy, mutations::buffer_copy,
    },
    observers::cmp::{AflppCmpValuesMetadata, CmpValues, CmpValuesMetadata},
    stages::TaintMetadata,
    state::{HasCorpus, HasMaxSize, HasRand},
};

/// A state metadata holding a list of tokens
#[expect(clippy::unsafe_derive_deserialize)]
#[derive(Debug, Default, Clone, Serialize, Deserialize)]
pub struct Tokens {
    // We keep a vec and a set, set for faster deduplication, vec for access
    tokens_vec: Vec<Vec<u8>>,
    tokens_set: HashSet<Vec<u8>>,
}

libafl_bolts::impl_serdeany!(Tokens);

/// The metadata used for token mutators
impl Tokens {
    /// Creates a new tokens metadata (old-skool afl name: `dictornary`)
    #[must_use]
    pub fn new() -> Self {
        Tokens::default()
    }

    /// Add tokens from a slice of Vecs of bytes
    pub fn add_tokens<IT, V>(&mut self, tokens: IT) -> &mut Self
    where
        IT: IntoIterator<Item = V>,
        V: AsRef<Vec<u8>>,
    {
        for token in tokens {
            self.add_token(token.as_ref());
        }
        self
    }

    /// Build tokens from files
    #[cfg(feature = "std")]
    pub fn add_from_files<IT, P>(mut self, files: IT) -> Result<Self, Error>
    where
        IT: IntoIterator<Item = P>,
        P: AsRef<Path>,
    {
        for file in files {
            self.add_from_file(file)?;
        }
        Ok(self)
    }

    /// Parse autodict section
    pub fn parse_autodict(&mut self, slice: &[u8], size: usize) {
        let mut head = 0;
        loop {
            if head >= size {
                // Make double sure this is not completely off
                assert!(head == size);
                break;
            }
            let size = slice[head] as usize;
            head += 1;
            if size > 0 {
                self.add_token(&slice[head..head + size].to_vec());
                log::info!(
                    "Token size: {} content: {:x?}",
                    size,
                    &slice[head..head + size].to_vec()
                );
                head += size;
            }
        }
    }

    /// Create a token section from a start and an end pointer
    /// Reads from an autotokens section, returning the count of new entries read
    ///
    /// # Safety
    /// The caller must ensure that the region between `token_start` and `token_stop`
    /// is a valid region, containing autotokens in the expected format.
    #[cfg(any(target_os = "linux", target_vendor = "apple"))]
    pub unsafe fn from_mut_ptrs(
        token_start: *const u8,
        token_stop: *const u8,
    ) -> Result<Self, Error> {
        unsafe {
            let mut ret = Self::default();
            if token_start.is_null() || token_stop.is_null() {
                return Ok(Self::new());
            }
            if token_stop < token_start {
                return Err(Error::illegal_argument(format!(
                    "Tried to create tokens from illegal section: stop < start ({token_stop:?} < {token_start:?})"
                )));
            }
            let section_size: usize = token_stop.offset_from(token_start).try_into().unwrap();
            // log::info!("size: {}", section_size);
            let slice = from_raw_parts(token_start, section_size);

            // Now we know the beginning and the end of the token section.. let's parse them into tokens
            ret.parse_autodict(slice, section_size);

            Ok(ret)
        }
    }

    /// Creates a new instance from a file
    #[cfg(feature = "std")]
    pub fn from_file<P>(file: P) -> Result<Self, Error>
    where
        P: AsRef<Path>,
    {
        let mut ret = Self::new();
        ret.add_from_file(file)?;
        Ok(ret)
    }

    /// Adds a token to a dictionary, checking it is not a duplicate
    /// Returns `false` if the token was already present and did not get added.
    #[expect(clippy::ptr_arg)]
    pub fn add_token(&mut self, token: &Vec<u8>) -> bool {
        if !self.tokens_set.insert(token.clone()) {
            return false;
        }
        self.tokens_vec.push(token.clone());
        true
    }

    /// Reads a tokens file, returning the count of new entries read
    #[cfg(feature = "std")]
    pub fn add_from_file<P>(&mut self, file: P) -> Result<&mut Self, Error>
    where
        P: AsRef<Path>,
    {
        // log::info!("Loading tokens file {:?} ...", file);

        let file = File::open(file)?; // panic if not found
        let reader = BufReader::new(file);

        for line in reader.lines() {
            let line = line.unwrap();
            let line = line.trim_start().trim_end();

            // we are only interested in '"..."', not prefixed 'foo = '
            let start = line.chars().next();
            if line.is_empty() || start == Some('#') {
                continue;
            }
            let Some(pos_quote) = line.find('\"') else {
                return Err(Error::illegal_argument(format!("Illegal line: {line}")));
            };
            if line.chars().nth(line.len() - 1) != Some('"') {
                return Err(Error::illegal_argument(format!("Illegal line: {line}")));
            }

            // extract item
            let Some(item) = line.get(pos_quote + 1..line.len() - 1) else {
                return Err(Error::illegal_argument(format!("Illegal line: {line}")));
            };
            if item.is_empty() {
                continue;
            }

            // decode
            let token: Vec<u8> = match str_decode(item) {
                Ok(val) => val,
                Err(_) => {
                    return Err(Error::illegal_argument(format!(
                        "Illegal line (hex decoding): {line}"
                    )));
                }
            };

            // add
            self.add_token(&token);
        }

        Ok(self)
    }

    /// Returns the amount of tokens in this Tokens instance
    #[inline]
    #[must_use]
    pub fn len(&self) -> usize {
        self.tokens_vec.len()
    }

    /// Returns if this tokens-instance is empty
    #[inline]
    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.tokens_vec.is_empty()
    }

    /// Gets the tokens stored in this db
    #[must_use]
    pub fn tokens(&self) -> &[Vec<u8>] {
        &self.tokens_vec
    }

    /// Returns an iterator over the tokens.
    pub fn iter(&self) -> Iter<'_, Vec<u8>> {
        <&Self as IntoIterator>::into_iter(self)
    }
}

impl AddAssign for Tokens {
    fn add_assign(&mut self, other: Self) {
        self.add_tokens(&other);
    }
}

impl AddAssign<&[Vec<u8>]> for Tokens {
    fn add_assign(&mut self, other: &[Vec<u8>]) {
        self.add_tokens(other);
    }
}

impl Add<&[Vec<u8>]> for Tokens {
    type Output = Self;
    fn add(self, other: &[Vec<u8>]) -> Self {
        let mut ret = self;
        ret.add_tokens(other);
        ret
    }
}

impl Add for Tokens {
    type Output = Self;

    fn add(self, other: Self) -> Self {
        self.add(other.tokens_vec.as_slice())
    }
}

impl<IT, V> From<IT> for Tokens
where
    IT: IntoIterator<Item = V>,
    V: AsRef<Vec<u8>>,
{
    fn from(tokens: IT) -> Self {
        let mut ret = Self::default();
        ret.add_tokens(tokens);
        ret
    }
}

impl Deref for Tokens {
    type Target = [Vec<u8>];
    fn deref(&self) -> &[Vec<u8>] {
        self.tokens()
    }
}

impl Add for &Tokens {
    type Output = Tokens;

    fn add(self, other: Self) -> Tokens {
        let mut ret: Tokens = self.clone();
        ret.add_tokens(other);
        ret
    }
}

impl<'it> IntoIterator for &'it Tokens {
    type Item = <Iter<'it, Vec<u8>> as Iterator>::Item;
    type IntoIter = Iter<'it, Vec<u8>>;

    fn into_iter(self) -> Self::IntoIter {
        self.as_slice().iter()
    }
}

/// Inserts a random token at a random position in the `Input`.
#[derive(Debug, Default)]
pub struct TokenInsert;

impl<I, S> Mutator<I, S> for TokenInsert
where
    S: HasMetadata + HasRand + HasMaxSize,
    I: ResizableMutator<u8> + HasMutatorBytes,
{
    fn mutate(&mut self, state: &mut S, input: &mut I) -> Result<MutationResult, Error> {
        let max_size = state.max_size();
        let tokens_len = {
            let Some(meta) = state.metadata_map().get::<Tokens>() else {
                return Ok(MutationResult::Skipped);
            };
            if let Some(tokens_len) = NonZero::new(meta.tokens().len()) {
                tokens_len
            } else {
                return Ok(MutationResult::Skipped);
            }
        };
        let token_idx = state.rand_mut().below(tokens_len);

        let size = input.mutator_bytes().len();
        // # Safety
        // after saturating add it's always above 0

        let off = state
            .rand_mut()
            .below(unsafe { NonZero::new(size.saturating_add(1)).unwrap_unchecked() });

        let meta = state.metadata_map().get::<Tokens>().unwrap();
        let token = &meta.tokens()[token_idx];
        let mut len = token.len();

        if size + len > max_size {
            if max_size > size {
                len = max_size - size;
            } else {
                return Ok(MutationResult::Skipped);
            }
        }

        input.resize(size + len, 0);
        unsafe {
            buffer_self_copy(input.mutator_bytes_mut(), off, off + len, size - off);
            buffer_copy(input.mutator_bytes_mut(), token, 0, off, len);
        }

        Ok(MutationResult::Mutated)
    }
    #[inline]
    fn post_exec(&mut self, _state: &mut S, _new_corpus_id: Option<CorpusId>) -> Result<(), Error> {
        Ok(())
    }
}

impl Named for TokenInsert {
    fn name(&self) -> &Cow<'static, str> {
        static NAME: Cow<'static, str> = Cow::Borrowed("TokenInsert");
        &NAME
    }
}

impl TokenInsert {
    /// Create a `TokenInsert` `Mutation`.
    #[must_use]
    pub fn new() -> Self {
        Self
    }
}

/// A `TokenReplace` [`Mutator`] replaces a random part of the input with one of a range of tokens.
/// From AFL terms, this is called as `Dictionary` mutation (which doesn't really make sense ;) ).
#[derive(Debug, Default)]
pub struct TokenReplace;

impl<I, S> Mutator<I, S> for TokenReplace
where
    S: HasMetadata + HasRand + HasMaxSize,
    I: ResizableMutator<u8> + HasMutatorBytes,
{
    fn mutate(&mut self, state: &mut S, input: &mut I) -> Result<MutationResult, Error> {
        let size = input.mutator_bytes().len();
        let off = if let Some(nz) = NonZero::new(size) {
            state.rand_mut().below(nz)
        } else {
            return Ok(MutationResult::Skipped);
        };

        let tokens_len = {
            let Some(meta) = state.metadata_map().get::<Tokens>() else {
                return Ok(MutationResult::Skipped);
            };
            if let Some(tokens_len) = NonZero::new(meta.tokens().len()) {
                tokens_len
            } else {
                return Ok(MutationResult::Skipped);
            }
        };
        let token_idx = state.rand_mut().below(tokens_len);

        let meta = state.metadata_map().get::<Tokens>().unwrap();
        let token = &meta.tokens()[token_idx];
        let mut len = token.len();
        if off + len > size {
            len = size - off;
        }

        unsafe {
            buffer_copy(input.mutator_bytes_mut(), token, 0, off, len);
        }

        Ok(MutationResult::Mutated)
    }
    #[inline]
    fn post_exec(&mut self, _state: &mut S, _new_corpus_id: Option<CorpusId>) -> Result<(), Error> {
        Ok(())
    }
}

impl Named for TokenReplace {
    fn name(&self) -> &Cow<'static, str> {
        static NAME: Cow<'static, str> = Cow::Borrowed("TokenReplace");
        &NAME
    }
}

impl TokenReplace {
    /// Creates a new `TokenReplace` struct.
    #[must_use]
    pub fn new() -> Self {
        Self
    }
}

/// A `I2SRandReplace` [`Mutator`] replaces a random matching input-2-state comparison operand with the other.
/// It needs a valid [`CmpValuesMetadata`] in the state.
#[derive(Debug, Default)]
pub struct I2SRandReplace;

impl<I, S> Mutator<I, S> for I2SRandReplace
where
    S: HasMetadata + HasRand + HasMaxSize,
    I: ResizableMutator<u8> + HasMutatorBytes,
{
    #[expect(clippy::too_many_lines)]
    fn mutate(&mut self, state: &mut S, input: &mut I) -> Result<MutationResult, Error> {
        let size = input.mutator_bytes().len();
        let Some(size) = NonZero::new(size) else {
            return Ok(MutationResult::Skipped);
        };

        let cmps_len = {
            let Some(meta) = state.metadata_map().get::<CmpValuesMetadata>() else {
                return Ok(MutationResult::Skipped);
            };
            log::trace!("meta: {meta:x?}");
            meta.list.len()
        };

        let Some(cmps_len) = NonZero::new(cmps_len) else {
            return Ok(MutationResult::Skipped);
        };

        let idx = state.rand_mut().below(cmps_len);

        let off = state.rand_mut().below(size);
        let len = input.mutator_bytes().len();
        let bytes = input.mutator_bytes_mut();

        let meta = state.metadata_map().get::<CmpValuesMetadata>().unwrap();
        let cmp_values = &meta.list[idx];

        let mut result = MutationResult::Skipped;
        match cmp_values {
            CmpValues::U8((v1, v2, v1_is_const)) => {
                for byte in bytes.iter_mut().take(len).skip(off) {
                    if !v1_is_const && *byte == *v1 {
                        *byte = *v2;
                        result = MutationResult::Mutated;
                        break;
                    } else if *byte == *v2 {
                        *byte = *v1;
                        result = MutationResult::Mutated;
                        break;
                    }
                }
            }
            CmpValues::U16((v1, v2, v1_is_const)) => {
                if len >= size_of::<u16>() {
                    for i in off..=len - size_of::<u16>() {
                        let val =
                            u16::from_ne_bytes(bytes[i..i + size_of::<u16>()].try_into().unwrap());
                        if !v1_is_const && val == *v1 {
                            let new_bytes = v2.to_ne_bytes();
                            bytes[i..i + size_of::<u16>()].copy_from_slice(&new_bytes);
                            result = MutationResult::Mutated;
                            break;
                        } else if !v1_is_const && val.swap_bytes() == *v1 {
                            let new_bytes = v2.swap_bytes().to_ne_bytes();
                            bytes[i..i + size_of::<u16>()].copy_from_slice(&new_bytes);
                            result = MutationResult::Mutated;
                            break;
                        } else if val == *v2 {
                            let new_bytes = v1.to_ne_bytes();
                            bytes[i..i + size_of::<u16>()].copy_from_slice(&new_bytes);
                            result = MutationResult::Mutated;
                            break;
                        } else if val.swap_bytes() == *v2 {
                            let new_bytes = v1.swap_bytes().to_ne_bytes();
                            bytes[i..i + size_of::<u16>()].copy_from_slice(&new_bytes);
                            result = MutationResult::Mutated;
                            break;
                        }
                    }
                }
            }
            CmpValues::U32((v1, v2, v1_is_const)) => {
                if len >= size_of::<u32>() {
                    for i in off..=len - size_of::<u32>() {
                        let val =
                            u32::from_ne_bytes(bytes[i..i + size_of::<u32>()].try_into().unwrap());
                        if !v1_is_const && val == *v1 {
                            let new_bytes = v2.to_ne_bytes();
                            bytes[i..i + size_of::<u32>()].copy_from_slice(&new_bytes);
                            result = MutationResult::Mutated;
                            break;
                        } else if !v1_is_const && val.swap_bytes() == *v1 {
                            let new_bytes = v2.swap_bytes().to_ne_bytes();
                            bytes[i..i + size_of::<u32>()].copy_from_slice(&new_bytes);
                            result = MutationResult::Mutated;
                            break;
                        } else if val == *v2 {
                            let new_bytes = v1.to_ne_bytes();
                            bytes[i..i + size_of::<u32>()].copy_from_slice(&new_bytes);
                            result = MutationResult::Mutated;
                            break;
                        } else if val.swap_bytes() == *v2 {
                            let new_bytes = v1.swap_bytes().to_ne_bytes();
                            bytes[i..i + size_of::<u32>()].copy_from_slice(&new_bytes);
                            result = MutationResult::Mutated;
                            break;
                        }
                    }
                }
            }
            CmpValues::U64((v1, v2, v1_is_const)) => {
                if len >= size_of::<u64>() {
                    for i in off..=len - size_of::<u64>() {
                        let val =
                            u64::from_ne_bytes(bytes[i..i + size_of::<u64>()].try_into().unwrap());
                        if !v1_is_const && val == *v1 {
                            let new_bytes = v2.to_ne_bytes();
                            bytes[i..i + size_of::<u64>()].copy_from_slice(&new_bytes);
                            result = MutationResult::Mutated;
                            break;
                        } else if !v1_is_const && val.swap_bytes() == *v1 {
                            let new_bytes = v2.swap_bytes().to_ne_bytes();
                            bytes[i..i + size_of::<u64>()].copy_from_slice(&new_bytes);
                            result = MutationResult::Mutated;
                            break;
                        } else if val == *v2 {
                            let new_bytes = v1.to_ne_bytes();
                            bytes[i..i + size_of::<u64>()].copy_from_slice(&new_bytes);
                            result = MutationResult::Mutated;
                            break;
                        } else if val.swap_bytes() == *v2 {
                            let new_bytes = v1.swap_bytes().to_ne_bytes();
                            bytes[i..i + size_of::<u64>()].copy_from_slice(&new_bytes);
                            result = MutationResult::Mutated;
                            break;
                        }
                    }
                }
            }
            CmpValues::Bytes(v) => {
                'outer: for i in off..len {
                    let mut size = core::cmp::min(v.0.len(), len - i);
                    while size != 0 {
                        if v.0.as_slice()[0..size] == input.mutator_bytes()[i..i + size] {
                            unsafe {
                                buffer_copy(input.mutator_bytes_mut(), v.1.as_slice(), 0, i, size);
                            }
                            result = MutationResult::Mutated;
                            break 'outer;
                        }
                        size -= 1;
                    }
                    size = core::cmp::min(v.1.len(), len - i);
                    while size != 0 {
                        if v.1.as_slice()[0..size] == input.mutator_bytes()[i..i + size] {
                            unsafe {
                                buffer_copy(input.mutator_bytes_mut(), v.0.as_slice(), 0, i, size);
                            }
                            result = MutationResult::Mutated;
                            break 'outer;
                        }
                        size -= 1;
                    }
                }
            }
        }

        Ok(result)
    }
    #[inline]
    fn post_exec(&mut self, _state: &mut S, _new_corpus_id: Option<CorpusId>) -> Result<(), Error> {
        Ok(())
    }
}

impl Named for I2SRandReplace {
    fn name(&self) -> &Cow<'static, str> {
        static NAME: Cow<'static, str> = Cow::Borrowed("I2SRandReplace");
        &NAME
    }
}

impl I2SRandReplace {
    /// Creates a new `I2SRandReplace` struct.
    #[must_use]
    pub fn new() -> Self {
        Self
    }
}

// A `I2SRandReplaceBinonly` [`Mutator`] replaces a random matching input-2-state comparison operand with the other.
/// It needs a valid [`CmpValuesMetadata`] in the state.
/// This version has been designed for binary-only fuzzing, for which cmp sized can be larger than necessary.
#[derive(Debug, Default)]
pub struct I2SRandReplaceBinonly;

fn random_slice_size<const SZ: usize, S>(state: &mut S) -> usize
where
    S: HasRand,
{
    let sz_log = SZ.ilog2() as usize;
    // # Safety
    // We add 1 so this can never be 0.
    // On 32 bit systems this could overflow in theory but this is highly unlikely.
    let sz_log_inclusive = unsafe { NonZero::new(sz_log + 1).unwrap_unchecked() };
    let res = state.rand_mut().below(sz_log_inclusive);
    2_usize.pow(res as u32)
}

impl<I, S> Mutator<I, S> for I2SRandReplaceBinonly
where
    S: HasMetadata + HasRand + HasMaxSize,
    I: ResizableMutator<u8> + HasMutatorBytes,
{
    #[expect(clippy::too_many_lines)]
    fn mutate(&mut self, state: &mut S, input: &mut I) -> Result<MutationResult, Error> {
        let Some(size) = NonZero::new(input.mutator_bytes().len()) else {
            return Ok(MutationResult::Skipped);
        };
        let Some(meta) = state.metadata_map().get::<CmpValuesMetadata>() else {
            return Ok(MutationResult::Skipped);
        };
        log::trace!("meta: {meta:x?}");

        let Some(cmps_len) = NonZero::new(meta.list.len()) else {
            return Ok(MutationResult::Skipped);
        };
        let idx = state.rand_mut().below(cmps_len);

        let off = state.rand_mut().below(size);
        let len = input.mutator_bytes().len();
        let bytes = input.mutator_bytes_mut();

        let meta = state.metadata_map().get::<CmpValuesMetadata>().unwrap();
        let cmp_values = &meta.list[idx];

        // TODO: do not use from_ne_bytes, it's for host not for target!! we should use a from_target_ne_bytes....

        let mut result = MutationResult::Skipped;
        match cmp_values.clone() {
            CmpValues::U8(v) => {
                for byte in bytes.iter_mut().take(len).skip(off) {
                    if *byte == v.0 {
                        *byte = v.1;
                        result = MutationResult::Mutated;
                        break;
                    } else if *byte == v.1 {
                        *byte = v.0;
                        result = MutationResult::Mutated;
                        break;
                    }
                }
            }
            CmpValues::U16(v) => {
                let cmp_size = random_slice_size::<{ size_of::<u16>() }, S>(state);

                if len >= cmp_size {
                    for i in off..len - (cmp_size - 1) {
                        let mut val_bytes = [0; size_of::<u16>()];
                        val_bytes[..cmp_size].copy_from_slice(&bytes[i..i + cmp_size]);
                        let val = u16::from_ne_bytes(val_bytes);

                        if val == v.0 {
                            let new_bytes = &v.1.to_ne_bytes()[..cmp_size];
                            bytes[i..i + cmp_size].copy_from_slice(new_bytes);
                            result = MutationResult::Mutated;
                            break;
                        } else if val == v.1 {
                            let new_bytes = &v.0.to_ne_bytes()[..cmp_size];
                            bytes[i..i + cmp_size].copy_from_slice(new_bytes);
                            result = MutationResult::Mutated;
                            break;
                        } else if val.swap_bytes() == v.0 {
                            let new_bytes = v.1.swap_bytes().to_ne_bytes();
                            bytes[i..i + cmp_size].copy_from_slice(&new_bytes[..cmp_size]);
                            result = MutationResult::Mutated;
                            break;
                        } else if val.swap_bytes() == v.1 {
                            let new_bytes = v.0.swap_bytes().to_ne_bytes();
                            bytes[i..i + cmp_size].copy_from_slice(&new_bytes[..cmp_size]);
                            result = MutationResult::Mutated;
                            break;
                        }
                    }
                }
            }
            CmpValues::U32(v) => {
                let cmp_size = random_slice_size::<{ size_of::<u32>() }, S>(state);
                if len >= cmp_size {
                    for i in off..len - (cmp_size - 1) {
                        let mut val_bytes = [0; size_of::<u32>()];
                        val_bytes[..cmp_size].copy_from_slice(&bytes[i..i + cmp_size]);
                        let val = u32::from_ne_bytes(val_bytes);

                        if val == v.0 {
                            let new_bytes = &v.1.to_ne_bytes()[..cmp_size];
                            bytes[i..i + cmp_size].copy_from_slice(new_bytes);
                            result = MutationResult::Mutated;
                            break;
                        } else if val == v.1 {
                            let new_bytes = &v.0.to_ne_bytes()[..cmp_size];
                            bytes[i..i + cmp_size].copy_from_slice(new_bytes);
                            result = MutationResult::Mutated;
                            break;
                        } else if val.swap_bytes() == v.0 {
                            let new_bytes = v.1.swap_bytes().to_ne_bytes();
                            bytes[i..i + cmp_size].copy_from_slice(&new_bytes[..cmp_size]);
                            result = MutationResult::Mutated;
                            break;
                        } else if val.swap_bytes() == v.1 {
                            let new_bytes = v.0.swap_bytes().to_ne_bytes();
                            bytes[i..i + cmp_size].copy_from_slice(&new_bytes[..cmp_size]);
                            result = MutationResult::Mutated;
                            break;
                        }
                    }
                }
            }
            CmpValues::U64(v) => {
                let cmp_size = random_slice_size::<{ size_of::<u64>() }, S>(state);

                if len >= cmp_size {
                    for i in off..(len - (cmp_size - 1)) {
                        let mut val_bytes = [0; size_of::<u64>()];
                        val_bytes[..cmp_size].copy_from_slice(&bytes[i..i + cmp_size]);
                        let val = u64::from_ne_bytes(val_bytes);

                        if val == v.0 {
                            let new_bytes = &v.1.to_ne_bytes()[..cmp_size];
                            bytes[i..i + cmp_size].copy_from_slice(new_bytes);
                            result = MutationResult::Mutated;
                            break;
                        } else if val == v.1 {
                            let new_bytes = &v.0.to_ne_bytes()[..cmp_size];
                            bytes[i..i + cmp_size].copy_from_slice(new_bytes);
                            result = MutationResult::Mutated;
                            break;
                        } else if val.swap_bytes() == v.0 {
                            let new_bytes = v.1.swap_bytes().to_ne_bytes();
                            bytes[i..i + cmp_size].copy_from_slice(&new_bytes[..cmp_size]);
                            result = MutationResult::Mutated;
                            break;
                        } else if val.swap_bytes() == v.1 {
                            let new_bytes = v.0.swap_bytes().to_ne_bytes();
                            bytes[i..i + cmp_size].copy_from_slice(&new_bytes[..cmp_size]);
                            result = MutationResult::Mutated;
                            break;
                        }
                    }
                }
            }
            CmpValues::Bytes(v) => {
                'outer: for i in off..len {
                    let mut size = core::cmp::min(v.0.len(), len - i);
                    while size != 0 {
                        if v.0.as_slice()[0..size] == input.mutator_bytes()[i..i + size] {
                            unsafe {
                                buffer_copy(input.mutator_bytes_mut(), v.1.as_slice(), 0, i, size);
                            }
                            result = MutationResult::Mutated;
                            break 'outer;
                        }
                        size -= 1;
                    }
                    size = core::cmp::min(v.1.len(), len - i);
                    while size != 0 {
                        if v.1.as_slice()[0..size] == input.mutator_bytes()[i..i + size] {
                            unsafe {
                                buffer_copy(input.mutator_bytes_mut(), v.0.as_slice(), 0, i, size);
                            }
                            result = MutationResult::Mutated;
                            break 'outer;
                        }
                        size -= 1;
                    }
                }
            }
        }

        Ok(result)
    }
    #[inline]
    fn post_exec(&mut self, _state: &mut S, _new_corpus_id: Option<CorpusId>) -> Result<(), Error> {
        Ok(())
    }
}

impl Named for I2SRandReplaceBinonly {
    fn name(&self) -> &Cow<'static, str> {
        static NAME: Cow<'static, str> = Cow::Borrowed("I2SRandReplace");
        &NAME
    }
}

impl I2SRandReplaceBinonly {
    /// Creates a new `I2SRandReplace` struct.
    #[must_use]
    pub fn new() -> Self {
        Self
    }
}
const CMP_ATTTRIBUTE_IS_EQUAL: u8 = 1;
const CMP_ATTRIBUTE_IS_GREATER: u8 = 2;
const CMP_ATTRIBUTE_IS_LESSER: u8 = 4;
const CMP_ATTRIBUTE_IS_FP: u8 = 8;
const CMP_ATTRIBUTE_IS_FP_MOD: u8 = 16;
const CMP_ATTRIBUTE_IS_INT_MOD: u8 = 32;
const CMP_ATTRIBUTE_IS_TRANSFORM: u8 = 64;

/// AFL++ redqueen mutation
#[derive(Debug, Default)]
pub struct AflppRedQueen {
    enable_transform: bool,
    enable_arith: bool,
    text_type: TextType,
    /// We use this variable to check if we scheduled a new `corpus_id`
    /// - and, hence, need to recalculate `text_type`
    last_corpus_id: Option<CorpusId>,
}

impl AflppRedQueen {
    #[inline]
    fn swapa(x: u8) -> u8 {
        (x & 0xf8) + ((x & 7) ^ 0x07)
    }

    /// Cmplog Pattern Matching
    #[expect(
        clippy::cast_sign_loss,
        clippy::too_many_arguments,
        clippy::too_many_lines,
        clippy::cast_possible_wrap,
        clippy::cast_precision_loss
    )]
    pub fn cmp_extend_encoding(
        &self,
        pattern: u64,
        repl: u64,
        another_pattern: u64,
        changed_val: u64,
        attr: u8,
        another_buf: &[u8],
        buf: &[u8],
        buf_idx: usize,
        taint_len: usize,
        input_len: usize,
        hshape: usize,
        vec: &mut Vec<Vec<u8>>,
    ) -> Result<bool, Error> {
        // TODO: ascii2num (we need check q->is_ascii (in calibration stage(?)))

        // try Transform
        if self.enable_transform
            && pattern != another_pattern
            && repl == changed_val
            && attr <= CMP_ATTTRIBUTE_IS_EQUAL
        {
            // Try to identify transform magic
            let mut bytes: usize = match hshape {
                0 => 0, // NEVER happen
                1 => 1,
                2 => 2,
                3 | 4 => 4,
                _ => 8,
            };
            // prevent overflow
            bytes = core::cmp::min(bytes, input_len.wrapping_sub(buf_idx));

            let (b_val, o_b_val, mask): (u64, u64, u64) = match bytes {
                0 => {
                    (0, 0, 0) // cannot happen
                }
                1 => (
                    u64::from(buf[buf_idx]),
                    u64::from(another_buf[buf_idx]),
                    0xff,
                ),
                2 | 3 => (
                    u64::from(u16::from_be_bytes(
                        another_buf[buf_idx..buf_idx + 2].try_into().unwrap(),
                    )),
                    u64::from(u16::from_be_bytes(
                        another_buf[buf_idx..buf_idx + 2].try_into().unwrap(),
                    )),
                    0xffff,
                ),
                4..=7 => (
                    u64::from(u32::from_be_bytes(
                        buf[buf_idx..buf_idx + 4].try_into().unwrap(),
                    )),
                    u64::from(u32::from_be_bytes(
                        another_buf[buf_idx..buf_idx + 4].try_into().unwrap(),
                    )),
                    0xffff_ffff,
                ),
                _ => (
                    u64::from_be_bytes(buf[buf_idx..buf_idx + 8].try_into().unwrap()),
                    u64::from_be_bytes(another_buf[buf_idx..buf_idx + 8].try_into().unwrap()),
                    0xffff_ffff_ffff_ffff,
                ),
            };

            // Try arith
            let diff = (pattern as i64).wrapping_sub(b_val as i64);
            let new_diff = (another_pattern as i64).wrapping_sub(o_b_val as i64);

            if diff == new_diff && diff != 0 {
                let new_repl: u64 = (repl as i64).wrapping_sub(diff) as u64;

                let ret = self.cmp_extend_encoding(
                    pattern,
                    new_repl,
                    another_pattern,
                    repl,
                    CMP_ATTRIBUTE_IS_TRANSFORM,
                    another_buf,
                    buf,
                    buf_idx,
                    taint_len,
                    input_len,
                    hshape,
                    vec,
                )?;
                if ret {
                    return Ok(true);
                }
            }

            // Try XOR

            // Shadowing
            let diff: i64 = (pattern ^ b_val) as i64;
            let new_diff: i64 = (another_pattern ^ o_b_val) as i64;

            if diff == new_diff && diff != 0 {
                let new_repl: u64 = (repl as i64 ^ diff) as u64;
                let ret = self.cmp_extend_encoding(
                    pattern,
                    new_repl,
                    another_pattern,
                    repl,
                    CMP_ATTRIBUTE_IS_TRANSFORM,
                    another_buf,
                    buf,
                    buf_idx,
                    taint_len,
                    input_len,
                    hshape,
                    vec,
                )?;

                if ret {
                    return Ok(true);
                }
            }

            // Try Lowercase
            // Shadowing
            let diff = (b_val | 0x2020_2020_2020_2020 & mask) == (pattern & mask);

            let new_diff = (b_val | 0x2020_2020_2020_2020 & mask) == (another_pattern & mask);

            if new_diff && diff {
                let new_repl: u64 = repl & (0x5f5f_5f5f_5f5f_5f5f & mask);
                let ret = self.cmp_extend_encoding(
                    pattern,
                    new_repl,
                    another_pattern,
                    repl,
                    CMP_ATTRIBUTE_IS_TRANSFORM,
                    another_buf,
                    buf,
                    buf_idx,
                    taint_len,
                    input_len,
                    hshape,
                    vec,
                )?;

                if ret {
                    return Ok(true);
                }
            }

            // Try Uppercase
            // Shadowing
            let diff = (b_val | 0x5f5f_5f5f_5f5f_5f5f & mask) == (pattern & mask);

            let o_diff = (b_val | 0x5f5f_5f5f_5f5f_5f5f & mask) == (another_pattern & mask);

            if o_diff && diff {
                let new_repl: u64 = repl & (0x2020_2020_2020_2020 & mask);
                let ret = self.cmp_extend_encoding(
                    pattern,
                    new_repl,
                    another_pattern,
                    repl,
                    CMP_ATTRIBUTE_IS_TRANSFORM,
                    another_buf,
                    buf,
                    buf_idx,
                    taint_len,
                    input_len,
                    hshape,
                    vec,
                )?;

                if ret {
                    return Ok(true);
                }
            }
        }

        let its_len = core::cmp::min(input_len.wrapping_sub(buf_idx), taint_len);

        // Try pattern matching
        // println!("Pattern match");
        match hshape {
            0 => (), // NEVER HAPPEN, Do nothing
            1 => {
                // 1 byte pattern match
                let buf_8 = buf[buf_idx];
                let another_buf_8 = another_buf[buf_idx];
                if buf_8 == pattern as u8 && another_buf_8 == another_pattern as u8 {
                    let mut cloned = buf.to_vec();
                    cloned[buf_idx] = repl as u8;
                    vec.push(cloned);
                    return Ok(true);
                }
            }
            2 | 3 => {
                if its_len >= 2 {
                    let buf_16 = u16::from_be_bytes(buf[buf_idx..buf_idx + 2].try_into()?);
                    let another_buf_16 =
                        u16::from_be_bytes(another_buf[buf_idx..buf_idx + 2].try_into()?);

                    if buf_16 == pattern as u16 && another_buf_16 == another_pattern as u16 {
                        let mut cloned = buf.to_vec();
                        cloned[buf_idx + 1] = (repl & 0xff) as u8;
                        cloned[buf_idx] = ((repl >> 8) & 0xff) as u8;
                        vec.push(cloned);
                        return Ok(true);
                    }
                }
            }
            4..=7 => {
                if its_len >= 4 {
                    let buf_32 = u32::from_be_bytes(buf[buf_idx..buf_idx + 4].try_into()?);
                    let another_buf_32 =
                        u32::from_be_bytes(another_buf[buf_idx..buf_idx + 4].try_into()?);
                    // println!("buf: {buf_32} {another_buf_32} {pattern} {another_pattern}");
                    if buf_32 == pattern as u32 && another_buf_32 == another_pattern as u32 {
                        let mut cloned = buf.to_vec();
                        cloned[buf_idx + 3] = (repl & 0xff) as u8;
                        cloned[buf_idx + 2] = ((repl >> 8) & 0xff) as u8;
                        cloned[buf_idx + 1] = ((repl >> 16) & 0xff) as u8;
                        cloned[buf_idx] = ((repl >> 24) & 0xff) as u8;
                        vec.push(cloned);

                        return Ok(true);
                    }
                }
            }
            _ => {
                if its_len >= 8 {
                    let buf_64 = u64::from_be_bytes(buf[buf_idx..buf_idx + 8].try_into()?);
                    let another_buf_64 =
                        u64::from_be_bytes(another_buf[buf_idx..buf_idx + 8].try_into()?);

                    if buf_64 == pattern && another_buf_64 == another_pattern {
                        let mut cloned = buf.to_vec();

                        cloned[buf_idx + 7] = (repl & 0xff) as u8;
                        cloned[buf_idx + 6] = ((repl >> 8) & 0xff) as u8;
                        cloned[buf_idx + 5] = ((repl >> 16) & 0xff) as u8;
                        cloned[buf_idx + 4] = ((repl >> 24) & 0xff) as u8;
                        cloned[buf_idx + 3] = ((repl >> 32) & 0xff) as u8;
                        cloned[buf_idx + 2] = ((repl >> 32) & 0xff) as u8;
                        cloned[buf_idx + 1] = ((repl >> 40) & 0xff) as u8;
                        cloned[buf_idx] = ((repl >> 48) & 0xff) as u8;

                        vec.push(cloned);
                        return Ok(true);
                    }
                }
            }
        }

        // Try arith
        if self.enable_arith || attr != CMP_ATTRIBUTE_IS_TRANSFORM {
            if (attr & (CMP_ATTRIBUTE_IS_GREATER | CMP_ATTRIBUTE_IS_LESSER)) == 0 || hshape < 4 {
                return Ok(false);
            }

            // Transform >= to < and <= to >
            let attr = if (attr & CMP_ATTTRIBUTE_IS_EQUAL) != 0
                && (attr & (CMP_ATTRIBUTE_IS_GREATER | CMP_ATTRIBUTE_IS_LESSER)) != 0
            {
                if attr & CMP_ATTRIBUTE_IS_GREATER != 0 {
                    attr + 2
                } else {
                    attr - 2
                }
            } else {
                attr
            };

            // FP
            if (CMP_ATTRIBUTE_IS_FP..CMP_ATTRIBUTE_IS_FP_MOD).contains(&attr) {
                let repl_new: u64;

                if attr & CMP_ATTRIBUTE_IS_GREATER != 0 {
                    if hshape == 4 && its_len >= 4 {
                        let mut g = repl as f32;
                        g += 1.0;
                        repl_new = u64::from(g as u32);
                    } else if hshape == 8 && its_len >= 8 {
                        let mut g = repl as f64;
                        g += 1.0;
                        repl_new = g as u64;
                    } else {
                        return Ok(false);
                    }

                    let ret = self.cmp_extend_encoding(
                        pattern,
                        repl,
                        another_pattern,
                        repl_new,
                        CMP_ATTRIBUTE_IS_FP_MOD,
                        another_buf,
                        buf,
                        buf_idx,
                        taint_len,
                        input_len,
                        hshape,
                        vec,
                    )?;
                    if ret {
                        return Ok(true);
                    }
                } else {
                    if hshape == 4 && its_len >= 4 {
                        let mut g = repl as f32;
                        g -= 1.0;
                        repl_new = u64::from(g as u32);
                    } else if hshape == 8 && its_len >= 8 {
                        let mut g = repl as f64;
                        g -= 1.0;
                        repl_new = g as u64;
                    } else {
                        return Ok(false);
                    }

                    let ret = self.cmp_extend_encoding(
                        pattern,
                        repl,
                        another_pattern,
                        repl_new,
                        CMP_ATTRIBUTE_IS_FP_MOD,
                        another_buf,
                        buf,
                        buf_idx,
                        taint_len,
                        input_len,
                        hshape,
                        vec,
                    )?;
                    if ret {
                        return Ok(true);
                    }
                }
            } else if attr < CMP_ATTRIBUTE_IS_FP {
                if attr & CMP_ATTRIBUTE_IS_GREATER != 0 {
                    let repl_new = repl.wrapping_add(1);

                    let ret = self.cmp_extend_encoding(
                        pattern,
                        repl,
                        another_pattern,
                        repl_new,
                        CMP_ATTRIBUTE_IS_INT_MOD,
                        another_buf,
                        buf,
                        buf_idx,
                        taint_len,
                        input_len,
                        hshape,
                        vec,
                    )?;

                    if ret {
                        return Ok(true);
                    }
                } else {
                    let repl_new = repl.wrapping_sub(1);

                    let ret = self.cmp_extend_encoding(
                        pattern,
                        repl,
                        another_pattern,
                        repl_new,
                        CMP_ATTRIBUTE_IS_INT_MOD,
                        another_buf,
                        buf,
                        buf_idx,
                        taint_len,
                        input_len,
                        hshape,
                        vec,
                    )?;

                    if ret {
                        return Ok(true);
                    }
                }
            } else {
                return Ok(false);
            }
        }

        Ok(false)
    }

    /// rtn part from AFL++
    #[expect(clippy::too_many_arguments)]
    pub fn rtn_extend_encoding(
        &self,
        pattern: &[u8],
        repl: &[u8],
        o_pattern: &[u8],
        _changed_val: &[u8],
        o_buf: &[u8],
        buf: &[u8],
        buf_idx: usize,
        taint_len: usize,
        input_len: usize,
        hshape: usize,
        vec: &mut Vec<Vec<u8>>,
    ) -> bool {
        let l0 = pattern.len();
        let ol0 = o_pattern.len();
        let lmax = core::cmp::max(l0, ol0);
        let its_len = core::cmp::min(
            core::cmp::min(input_len.wrapping_sub(buf_idx), taint_len),
            core::cmp::min(lmax, hshape),
        );

        // TODO: Match before (This: https://github.com/AFLplusplus/AFLplusplus/blob/ea14f3fd40e32234989043a525e3853fcb33c1b6/src/afl-fuzz-redqueen.c#L2047)
        let mut copy_len = 0;
        for i in 0..its_len {
            let b1 = i < pattern.len() && pattern[i] != buf[buf_idx + i];
            let b2 = i < o_pattern.len() && o_pattern[i] != o_buf[buf_idx + i];

            if b1 || b2 {
                break;
            }
            copy_len += 1;
        }

        if copy_len > 0 {
            unsafe {
                for l in 1..=copy_len {
                    if l <= repl.len() {
                        let mut cloned = buf.to_vec();
                        buffer_copy(&mut cloned, repl, 0, buf_idx, l);
                        vec.push(cloned);
                    }
                }
                // vec.push(cloned);
            }
            true
        } else {
            false
        }

        // TODO: Transform (This: https://github.com/AFLplusplus/AFLplusplus/blob/stable/src/afl-fuzz-redqueen.c#L2089)
        // It's hard to implement this naively
        // because AFL++ redqueen does not check any pattern, but it calls its_fuzz() instead.
        // we can't execute the harness inside a mutator

        // Direct matching
    }
}

impl<I, S> MultiMutator<I, S> for AflppRedQueen
where
    S: HasMetadata + HasRand + HasMaxSize + HasCorpus<I> + HasCurrentCorpusId,
    I: ResizableMutator<u8> + From<Vec<u8>> + HasMutatorBytes,
{
    #[expect(clippy::needless_range_loop, clippy::too_many_lines)]
    fn multi_mutate(
        &mut self,
        state: &mut S,
        input: &I,
        max_count: Option<usize>,
    ) -> Result<Vec<I>, Error> {
        // TODO
        // handle 128-bits logs
        let size = input.mutator_bytes().len();
        if size == 0 {
            return Ok(vec![]);
        }

        let (cmp_len, cmp_meta, taint_meta) = {
            let (Some(cmp_meta), Some(taint_meta)) = (
                state.metadata_map().get::<AflppCmpValuesMetadata>(),
                state.metadata_map().get::<TaintMetadata>(),
            ) else {
                return Ok(vec![]);
            };

            let cmp_len = cmp_meta.headers().len();
            if cmp_len == 0 {
                return Ok(vec![]);
            }
            (cmp_len, cmp_meta, taint_meta)
        };

        // These idxes must saved in this mutator itself!
        let mut taint_idx = 0;
        let orig_cmpvals = cmp_meta.orig_cmpvals();
        let new_cmpvals = cmp_meta.new_cmpvals();
        let headers = cmp_meta.headers();
        let input_len = input.mutator_bytes().len();
        let new_bytes = taint_meta.input_vec();
        let orig_bytes = input.mutator_bytes();

        let taint = taint_meta.ranges();
        let mut ret = max_count.map_or_else(Vec::new, Vec::with_capacity);
        let mut gathered_tokens = Tokens::new();
        // println!("orig: {:#?} new: {:#?}", orig_cmpvals, new_cmpvals);

        // Compute when mutating it for the 1st time.
        let current_corpus_id = state.current_corpus_id()?.ok_or_else(|| Error::key_not_found("No corpus-id is currently being fuzzed, but called AflppRedQueen::multi_mutated()."))?;
        if self.last_corpus_id.is_none() || self.last_corpus_id.unwrap() != current_corpus_id {
            self.text_type = check_if_text(orig_bytes, orig_bytes.len());
            self.last_corpus_id = Some(current_corpus_id);
        }
        // println!("approximate size: {cmp_len} x {input_len}");
        for cmp_idx in 0..cmp_len {
            let (w_idx, header) = headers[cmp_idx];

            if orig_cmpvals.get(&w_idx).is_none() || new_cmpvals.get(&w_idx).is_none() {
                // These two should have same boolean value

                // so there's nothing interesting at cmp_idx, then just skip!
                continue;
            }

            let orig_val = orig_cmpvals.get(&w_idx).unwrap();
            let new_val = new_cmpvals.get(&w_idx).unwrap();

            let logged = core::cmp::min(orig_val.len(), new_val.len());

            for cmp_h_idx in 0..logged {
                let mut skip_opt = false;
                for prev_idx in 0..cmp_h_idx {
                    if new_val[prev_idx] == new_val[cmp_h_idx] {
                        skip_opt = true;
                    }
                }
                // Opt not in the paper
                if skip_opt {
                    continue;
                }

                for cmp_buf_idx in 0..input_len {
                    if let Some(max_count) = max_count {
                        if ret.len() >= max_count {
                            // TODO: does this bias towards earlier mutations?
                            break;
                        }
                    }

                    let taint_len = match taint.get(taint_idx) {
                        Some(t) => {
                            if cmp_buf_idx < t.start {
                                input_len - cmp_buf_idx
                            } else {
                                // if cmp_buf_idx == t.end go to next range
                                if cmp_buf_idx == t.end {
                                    taint_idx += 1;
                                }

                                // Here cmp_buf_idx >= t.start
                                t.end - cmp_buf_idx
                            }
                        }
                        None => input_len - cmp_buf_idx,
                    };

                    let hshape = (header.shape().value() + 1) as usize;

                    match (&orig_val[cmp_h_idx], &new_val[cmp_h_idx]) {
                        (CmpValues::U8(_orig), CmpValues::U8(_new)) => {
                            /* just don't do it for u8, not worth it. not even instrumented

                            let (orig_v0, orig_v1, new_v0, new_v1) = (orig.0, orig.1, new.0, new.1);
                            let attribute = header.attribute() as u8;

                            let mut cmp_found = false;
                            if new_v0 != orig_v0 && orig_v0 != orig_v1 {
                                // Compare v0 against v1
                                cmp_found |= self.cmp_extend_encoding(
                                    orig_v0.into(),
                                    orig_v1.into(),
                                    new_v0.into(),
                                    new_v1.into(),
                                    attribute,
                                    new_bytes,
                                    orig_bytes,
                                    cmp_buf_idx,
                                    taint_len,
                                    input_len,
                                    hshape,
                                    &mut ret,
                                );

                                // Swapped
                                cmp_found |= self.cmp_extend_encoding(
                                    orig_v0.swap_bytes().into(),
                                    orig_v1.swap_bytes().into(),
                                    new_v0.swap_bytes().into(),
                                    new_v1.swap_bytes().into(),
                                    attribute,
                                    new_bytes,
                                    orig_bytes,
                                    cmp_buf_idx,
                                    taint_len,
                                    input_len,
                                    hshape,
                                    &mut ret,
                                );
                            }

                            if new_v1 != orig_v1 && orig_v0 != orig_v1 {
                                // Compare v1 against v0
                                cmp_found |= self.cmp_extend_encoding(
                                    orig_v1.into(),
                                    orig_v0.into(),
                                    new_v1.into(),
                                    new_v0.into(),
                                    Self::swapa(attribute),
                                    new_bytes,
                                    orig_bytes,
                                    cmp_buf_idx,
                                    taint_len,
                                    input_len,
                                    hshape,
                                    &mut ret,
                                );

                                // Swapped
                                cmp_found |= self.cmp_extend_encoding(
                                    orig_v1.swap_bytes().into(),
                                    orig_v0.swap_bytes().into(),
                                    new_v1.swap_bytes().into(),
                                    new_v0.swap_bytes().into(),
                                    Self::swapa(attribute),
                                    new_bytes,
                                    orig_bytes,
                                    cmp_buf_idx,
                                    taint_len,
                                    input_len,
                                    hshape,
                                    &mut ret,
                                );
                            }
                            */

                            /*
                            U8 or U16 is not worth
                            if !cmp_found && self.text_type.is_ascii_or_utf8() {
                                if orig_v0 == new_v0 {
                                    let v = orig_v0.to_ne_bytes().to_vec();
                                    Self::try_add_autotokens(&mut gathered_tokens, &v, hshape);
                                }

                                if orig_v1 == new_v1 {
                                    let v = orig_v1.to_ne_bytes().to_vec();
                                    Self::try_add_autotokens(&mut gathered_tokens, &v, hshape);
                                }
                            }
                            */
                        }
                        (CmpValues::U16(orig), CmpValues::U16(new)) => {
                            let (orig_v0, orig_v1, new_v0, new_v1) = (orig.0, orig.1, new.0, new.1);
                            let attribute: u8 = header.attribute().value();

                            if new_v0 != orig_v0 && orig_v0 != orig_v1 {
                                // Compare v0 against v1
                                self.cmp_extend_encoding(
                                    orig_v0.into(),
                                    orig_v1.into(),
                                    new_v0.into(),
                                    new_v1.into(),
                                    attribute,
                                    new_bytes,
                                    orig_bytes,
                                    cmp_buf_idx,
                                    taint_len,
                                    input_len,
                                    hshape,
                                    &mut ret,
                                )?;

                                // Swapped
                                // Compare v0 against v1
                                self.cmp_extend_encoding(
                                    orig_v0.swap_bytes().into(),
                                    orig_v1.swap_bytes().into(),
                                    new_v0.swap_bytes().into(),
                                    new_v1.swap_bytes().into(),
                                    attribute,
                                    new_bytes,
                                    orig_bytes,
                                    cmp_buf_idx,
                                    taint_len,
                                    input_len,
                                    hshape,
                                    &mut ret,
                                )?;
                            }

                            if new_v1 != orig_v1 && orig_v0 != orig_v1 {
                                // Compare v1 against v0
                                self.cmp_extend_encoding(
                                    orig_v1.into(),
                                    orig_v0.into(),
                                    new_v1.into(),
                                    new_v0.into(),
                                    Self::swapa(attribute),
                                    new_bytes,
                                    orig_bytes,
                                    cmp_buf_idx,
                                    taint_len,
                                    input_len,
                                    hshape,
                                    &mut ret,
                                )?;

                                // Swapped
                                self.cmp_extend_encoding(
                                    orig_v1.swap_bytes().into(),
                                    orig_v0.swap_bytes().into(),
                                    new_v1.swap_bytes().into(),
                                    new_v0.swap_bytes().into(),
                                    Self::swapa(attribute),
                                    new_bytes,
                                    orig_bytes,
                                    cmp_buf_idx,
                                    taint_len,
                                    input_len,
                                    hshape,
                                    &mut ret,
                                )?;
                            }

                            /*
                            U8 or U16 is not worth
                            if !cmp_found && self.text_type.is_ascii_or_utf8() {
                                if orig_v0 == new_v0 {
                                    let v = orig_v0.to_ne_bytes().to_vec();
                                    Self::try_add_autotokens(&mut gathered_tokens, &v, hshape);
                                }

                                if orig_v1 == new_v1 {
                                    let v = orig_v1.to_ne_bytes().to_vec();
                                    Self::try_add_autotokens(&mut gathered_tokens, &v, hshape);
                                }
                            }
                            */
                        }
                        (CmpValues::U32(orig), CmpValues::U32(new)) => {
                            let (orig_v0, orig_v1, new_v0, new_v1) = (orig.0, orig.1, new.0, new.1);
                            let attribute = header.attribute().value();

                            let mut cmp_found = false;
                            if new_v0 != orig_v0 && orig_v0 != orig_v1 {
                                // Compare v0 against v1
                                cmp_found |= self.cmp_extend_encoding(
                                    orig_v0.into(),
                                    orig_v1.into(),
                                    new_v0.into(),
                                    new_v1.into(),
                                    attribute,
                                    new_bytes,
                                    orig_bytes,
                                    cmp_buf_idx,
                                    taint_len,
                                    input_len,
                                    hshape,
                                    &mut ret,
                                )?;

                                // swapped
                                // Compare v0 against v1
                                cmp_found |= self.cmp_extend_encoding(
                                    orig_v0.swap_bytes().into(),
                                    orig_v1.swap_bytes().into(),
                                    new_v0.swap_bytes().into(),
                                    new_v1.swap_bytes().into(),
                                    attribute,
                                    new_bytes,
                                    orig_bytes,
                                    cmp_buf_idx,
                                    taint_len,
                                    input_len,
                                    hshape,
                                    &mut ret,
                                )?;
                            }

                            if new_v1 != orig_v1 && orig_v0 != orig_v1 {
                                // Compare v1 against v0
                                cmp_found |= self.cmp_extend_encoding(
                                    orig_v1.into(),
                                    orig_v0.into(),
                                    new_v1.into(),
                                    new_v0.into(),
                                    Self::swapa(attribute),
                                    new_bytes,
                                    orig_bytes,
                                    cmp_buf_idx,
                                    taint_len,
                                    input_len,
                                    hshape,
                                    &mut ret,
                                )?;

                                // Swapped
                                // Compare v1 against v0
                                cmp_found |= self.cmp_extend_encoding(
                                    orig_v1.swap_bytes().into(),
                                    orig_v0.swap_bytes().into(),
                                    new_v1.swap_bytes().into(),
                                    new_v0.swap_bytes().into(),
                                    Self::swapa(attribute),
                                    new_bytes,
                                    orig_bytes,
                                    cmp_buf_idx,
                                    taint_len,
                                    input_len,
                                    hshape,
                                    &mut ret,
                                )?;
                            }

                            if !cmp_found {
                                if orig_v0 == new_v0
                                    && check_if_text(orig_v0.to_ne_bytes().as_ref(), hshape).size()
                                        == hshape
                                {
                                    let v = orig_v0.to_ne_bytes().to_vec();
                                    Self::try_add_autotokens(&mut gathered_tokens, &v, hshape);
                                }

                                if orig_v1 == new_v1
                                    && check_if_text(orig_v1.to_ne_bytes().as_ref(), hshape).size()
                                        == hshape
                                {
                                    let v = orig_v1.to_ne_bytes().to_vec();
                                    Self::try_add_autotokens(&mut gathered_tokens, &v, hshape);
                                }
                            }
                        }
                        (CmpValues::U64(orig), CmpValues::U64(new)) => {
                            let (orig_v0, orig_v1, new_v0, new_v1) = (orig.0, orig.1, new.0, new.1);
                            let attribute = header.attribute().value();

                            let mut cmp_found = false;
                            if new_v0 != orig_v0 && orig_v0 != orig_v1 {
                                // Compare v0 against v1
                                cmp_found |= self.cmp_extend_encoding(
                                    orig_v0,
                                    orig_v1,
                                    new_v0,
                                    new_v1,
                                    attribute,
                                    new_bytes,
                                    orig_bytes,
                                    cmp_buf_idx,
                                    taint_len,
                                    input_len,
                                    hshape,
                                    &mut ret,
                                )?;

                                // Swapped
                                // Compare v0 against v1
                                cmp_found |= self.cmp_extend_encoding(
                                    orig_v0.swap_bytes(),
                                    orig_v1.swap_bytes(),
                                    new_v0.swap_bytes(),
                                    new_v1.swap_bytes(),
                                    attribute,
                                    new_bytes,
                                    orig_bytes,
                                    cmp_buf_idx,
                                    taint_len,
                                    input_len,
                                    hshape,
                                    &mut ret,
                                )?;
                            }

                            if new_v1 != orig_v1 && orig_v0 != orig_v1 {
                                // Compare v1 against v0
                                cmp_found |= self.cmp_extend_encoding(
                                    orig_v1,
                                    orig_v0,
                                    new_v1,
                                    new_v0,
                                    Self::swapa(attribute),
                                    new_bytes,
                                    orig_bytes,
                                    cmp_buf_idx,
                                    taint_len,
                                    input_len,
                                    hshape,
                                    &mut ret,
                                )?;

                                // Swapped
                                // Compare v1 against v0
                                cmp_found |= self.cmp_extend_encoding(
                                    orig_v1.swap_bytes(),
                                    orig_v0.swap_bytes(),
                                    new_v1.swap_bytes(),
                                    new_v0.swap_bytes(),
                                    Self::swapa(attribute),
                                    new_bytes,
                                    orig_bytes,
                                    cmp_buf_idx,
                                    taint_len,
                                    input_len,
                                    hshape,
                                    &mut ret,
                                )?;
                            }

                            if !cmp_found {
                                if orig_v0 == new_v0
                                    && check_if_text(orig_v0.to_ne_bytes().as_ref(), hshape).size()
                                        == hshape
                                {
                                    let v = orig_v0.to_ne_bytes().to_vec();
                                    Self::try_add_autotokens(&mut gathered_tokens, &v, hshape);
                                }

                                if orig_v1 == new_v1
                                    && check_if_text(orig_v1.to_ne_bytes().as_ref(), hshape).size()
                                        == hshape
                                {
                                    let v = orig_v1.to_ne_bytes().to_vec();
                                    Self::try_add_autotokens(&mut gathered_tokens, &v, hshape);
                                }
                            }
                        }
                        (CmpValues::Bytes(orig), CmpValues::Bytes(new)) => {
                            let (orig_v0, orig_v1, new_v0, new_v1) =
                                (&orig.0, &orig.1, &new.0, &new.1);
                            // let attribute = header.attribute() as u8;
                            let mut rtn_found = false;
                            // Compare v0 against v1
                            rtn_found |= self.rtn_extend_encoding(
                                orig_v0.as_slice(),
                                orig_v1.as_slice(),
                                new_v0.as_slice(),
                                new_v1.as_slice(),
                                new_bytes,
                                orig_bytes,
                                cmp_buf_idx,
                                taint_len,
                                input_len,
                                hshape,
                                &mut ret,
                            );

                            // Compare v1 against v0
                            rtn_found |= self.rtn_extend_encoding(
                                orig_v1.as_slice(),
                                orig_v0.as_slice(),
                                new_v1.as_slice(),
                                new_v0.as_slice(),
                                new_bytes,
                                orig_bytes,
                                cmp_buf_idx,
                                taint_len,
                                input_len,
                                hshape,
                                &mut ret,
                            );

                            let is_ascii_or_utf8 = self.text_type.is_ascii_or_utf8();
                            let mut v0_len = orig_v0.len();
                            let mut v1_len = orig_v1.len();
                            if v0_len > 0
                                && (is_ascii_or_utf8
                                    || check_if_text(orig_v0.as_slice(), v0_len).size() == hshape)
                            {
                                // this is not utf8.
                                let v = strlen(orig_v0.as_slice());
                                if v > 0 {
                                    v0_len = v;
                                }
                            }

                            if v1_len > 0
                                && (is_ascii_or_utf8
                                    || check_if_text(orig_v1.as_slice(), v1_len).size() == hshape)
                            {
                                // this is not utf8.
                                let v = strlen(orig_v1.as_slice());
                                if v > 0 {
                                    v1_len = v;
                                }
                            }

                            if v0_len > 0
                                && orig_v0 == new_v0
                                && (!rtn_found
                                    || check_if_text(orig_v0.as_slice(), v0_len).size() == v0_len)
                            {
                                Self::try_add_autotokens(
                                    &mut gathered_tokens,
                                    orig_v0.as_slice(),
                                    v0_len,
                                );
                            }

                            if v1_len > 0
                                && orig_v1 == new_v1
                                && (!rtn_found
                                    || check_if_text(orig_v1.as_slice(), v1_len).size() == v1_len)
                            {
                                Self::try_add_autotokens(
                                    &mut gathered_tokens,
                                    orig_v1.as_slice(),
                                    v1_len,
                                );
                            }
                        }
                        (_, _) => {
                            // not gonna happen
                        }
                    }

                    /*
                    if matched {
                        // before returning the result
                        // save indexes
                        self.cmp_start_idx = cmp_start_idx;
                        self.cmp_h_start_idx = cmp_h_start_idx;
                        self.cmp_buf_start_idx = cmp_buf_start_idx + 1; // next
                        self.taint_idx = taint_idx;

                        return Ok(MutationResult::Mutated);
                    }
                    */
                    // if no match then go to next round
                }
            }
        }

        match state.metadata_mut::<Tokens>() {
            Ok(existing) => {
                existing.add_tokens(&gathered_tokens);
                // println!("we have {} tokens", existing.len())
            }
            Err(_) => {
                state.add_metadata(gathered_tokens);
            }
        }

        if let Some(max_count) = max_count {
            Ok(ret.into_iter().take(max_count).map(I::from).collect())
        } else {
            Ok(ret.into_iter().map(I::from).collect())
        }
    }
}

impl Named for AflppRedQueen {
    fn name(&self) -> &Cow<'static, str> {
        static NAME: Cow<'static, str> = Cow::Borrowed("AflppRedQueen");
        &NAME
    }
}

impl AflppRedQueen {
    /// Create a new `AflppRedQueen` Mutator
    #[must_use]
    pub fn new() -> Self {
        Self {
            enable_transform: false,
            enable_arith: false,
            text_type: TextType::None,
            last_corpus_id: None,
        }
    }

    /// Constructor with cmplog options
    #[must_use]
    pub fn with_cmplog_options(transform: bool, arith: bool) -> Self {
        Self {
            enable_transform: transform,
            enable_arith: arith,
            text_type: TextType::None,
            last_corpus_id: None,
        }
    }

    #[expect(clippy::needless_range_loop)]
    fn try_add_autotokens(tokens: &mut Tokens, b: &[u8], shape: usize) {
        let mut cons_ff = 0;
        let mut cons_0 = 0;

        for idx in 0..shape {
            if b[idx] == 0 {
                cons_0 += 1;
            } else if b[idx] == 0xff {
                cons_ff += 1;
            } else {
                cons_0 = 0;
                cons_ff = 0;
            }

            if cons_0 > 1 || cons_ff > 1 {
                return;
            }
        }
        let mut v = b.to_vec();
        tokens.add_token(&v);
        v.reverse();
        tokens.add_token(&v);
    }
}
#[derive(Default, Debug, Copy, Clone)]
enum TextType {
    #[default]
    None,
    Ascii(usize),
    UTF8(usize),
}

impl TextType {
    fn is_ascii_or_utf8(self) -> bool {
        match self {
            Self::None => false,
            Self::Ascii(_) | Self::UTF8(_) => true,
        }
    }

    fn size(self) -> usize {
        match self {
            Self::None => 0,
            Self::Ascii(sz) | Self::UTF8(sz) => sz,
        }
    }
}

/// Returns `true` if the given `u8` char is
/// in the valid ascii range (`<= 0x7F`)
#[inline]
const fn isascii(c: u8) -> bool {
    c <= 0x7F
}

/// Returns `true` if the given `u8` char is
/// a valid printable character (between `0x20` and `0x7E`)
#[inline]
const fn isprint(c: u8) -> bool {
    c >= 0x20 && c <= 0x7E
}

#[inline]
const fn strlen(buf: &[u8]) -> usize {
    let mut count = 0;
    while count < buf.len() {
        if buf[count] == 0x0 {
            break;
        }
        count += 1;
    }
    count
}

fn check_if_text(buf: &[u8], max_len: usize) -> TextType {
    // assert!(buf.len() >= max_len);
    let len = max_len;
    let mut offset: usize = 0;
    let mut ascii = 0;
    let mut utf8 = 0;
    let mut comp = len;

    while offset < max_len {
        if buf[offset] == 0x09
            || buf[offset] == 0x0A
            || buf[offset] == 0x0D
            || (0x20 <= buf[offset] && buf[offset] <= 0x7E)
        {
            offset += 1;
            utf8 += 1;
            ascii += 1;
            continue;
        }

        if isascii(buf[offset]) || isprint(buf[offset]) {
            ascii += 1;
        }

        // non-overlong 2-byte
        if len - offset > 1
            && ((0xC2 <= buf[offset] && buf[offset] <= 0xDF)
                && (0x80 <= buf[offset + 1] && buf[offset + 1] <= 0xBF))
        {
            offset += 2;
            utf8 += 1;
            comp -= 1;
            continue;
        }

        // excluding overlongs

        if (len - offset > 2)
            && ((buf[offset] == 0xE0 && (0xA0 <= buf[offset + 1] && buf[offset + 1] <= 0xBF) && (0x80 <= buf[offset + 2] && buf[offset + 2] <= 0xBF)) ||  // straight 3-byte
            (((0xE1 <= buf[offset] && buf[offset] <= 0xEC) || buf[offset] == 0xEE || buf[offset] == 0xEF) && (0x80 <= buf[offset + 1] && buf[offset + 1] <= 0xBF) && (0x80 <= buf[offset + 2] && buf[offset + 2] <= 0xBF)) ||  // excluding surrogates
            (buf[offset] == 0xED && (0x80 <= buf[offset + 1] && buf[offset + 1] <= 0x9F) && (0x80 <= buf[offset + 2] && buf[offset + 2] <= 0xBF)))
        {
            offset += 3;
            utf8 += 1;
            comp -= 2;
            continue;
        }

        // planes 1-3
        if (len - offset > 3)
            && ((buf[offset] == 0xF0 && (0x90 <= buf[offset + 1] && buf[offset + 1] <= 0xBF) && (0x80 <= buf[offset + 2] && buf[offset + 2] <= 0xBF) && (0x80 <= buf[offset + 3] && buf[offset + 3] <= 0xBF)) ||  // planes 4-15
            ((0xF1 <= buf[offset] && buf[offset] <= 0xF3) && (0x80 <= buf[offset + 1] && buf[offset + 1] <= 0xBF) && (0x80 <= buf[offset + 2] && buf[offset + 2] <= 0xBF) && (0x80 <= buf[offset + 3] && buf[offset + 3] <= 0xBF)) ||  // plane 16
            (buf[offset] == 0xF4 && (0x80 <= buf[offset + 1] && buf[offset + 1] <= 0x8F) && (0x80 <= buf[offset + 2] && buf[offset + 2] <= 0xBF) && (0x80 <= buf[offset + 3] && buf[offset + 3] <= 0xBF)))
        {
            offset += 4;
            utf8 += 1;
            comp -= 3;
            continue;
        }

        offset += 1;
    }
    let percent_utf8 = (utf8 * 100) / comp;
    let percent_ascii = (ascii * 100) / len;

    if percent_utf8 >= percent_ascii && percent_utf8 >= 99 {
        // utf
        return TextType::UTF8(utf8);
    }
    if percent_ascii >= 99 {
        return TextType::Ascii(ascii);
    }
    TextType::None
}

#[cfg(test)]
mod tests {
    #[cfg(feature = "std")]
    use std::fs;

    #[cfg(feature = "std")]
    use super::{AflppRedQueen, Tokens};

    #[cfg(feature = "std")]
    #[test]
    fn test_read_tokens() {
        let _res = fs::remove_file("test.tkns");
        let data = r#"
# comment
token1@123="AAA"
token1="A\x41A"
"A\AA"
token2="B"
        "#;
        fs::write("test.tkns", data).expect("Unable to write test.tkns");
        let tokens = Tokens::from_file("test.tkns").unwrap();
        log::info!("Token file entries: {:?}", tokens.tokens());
        assert_eq!(tokens.tokens().len(), 2);
        let _res = fs::remove_file("test.tkns");
    }

    #[cfg(feature = "std")]
    #[test]
    fn test_token_mutations() {
        let rq = AflppRedQueen::with_cmplog_options(true, true);
        let pattern = 0;
        let repl = 0;
        let another_pattern = 0;
        let changed_val = 0;
        let attr = 0;
        let another_buf = &[0, 0, 0, 0];
        let buf = &[0, 0, 0, 0];
        let buf_idx = 0;
        let taint_len = 0;
        let input_len = 0;
        let hshape = 0;
        let mut vec = alloc::vec::Vec::new();

        let _res = rq.cmp_extend_encoding(
            pattern,
            repl,
            another_pattern,
            changed_val,
            attr,
            another_buf,
            buf,
            buf_idx,
            taint_len,
            input_len,
            hshape,
            &mut vec,
        );
    }
}
