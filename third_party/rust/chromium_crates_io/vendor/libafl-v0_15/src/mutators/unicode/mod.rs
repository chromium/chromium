//! Mutators for preserving unicode string categories,
//! which may be useful for certain targets which are primarily string-oriented.
use alloc::{borrow::Cow, vec::Vec};
use core::{
    cmp::{Ordering, Reverse},
    num::NonZero,
    ops::Range,
};

use libafl_bolts::{Error, HasLen, Named, rands::Rand};

use crate::{
    HasMetadata,
    corpus::{CorpusId, HasTestcase, Testcase},
    inputs::{BytesInput, HasMutatorBytes, ResizableMutator},
    mutators::{MutationResult, Mutator, Tokens, rand_range},
    nonzero,
    stages::{
        UnicodeIdentificationMetadata, extract_metadata,
        mutational::{MutatedTransform, MutatedTransformPost},
    },
    state::{HasCorpus, HasMaxSize, HasRand},
};

/// Unicode category data, as used by string analysis and mutators.
#[expect(missing_docs, clippy::redundant_static_lifetimes)]
pub mod unicode_categories;

/// Input which contains the context necessary to perform unicode mutations
pub type UnicodeInput = (BytesInput, UnicodeIdentificationMetadata);

impl<S> MutatedTransform<BytesInput, S> for UnicodeInput
where
    S: HasCorpus<BytesInput> + HasTestcase<BytesInput>,
{
    type Post = UnicodeIdentificationMetadata;

    fn try_transform_from(base: &mut Testcase<BytesInput>, state: &S) -> Result<Self, Error> {
        let input = base.load_input(state.corpus())?.clone();
        let metadata = base.metadata::<UnicodeIdentificationMetadata>().cloned()?;
        Ok((input, metadata))
    }

    fn try_transform_into(self, _state: &S) -> Result<(BytesInput, Self::Post), Error> {
        Ok(self)
    }
}

impl<S> MutatedTransformPost<S> for UnicodeIdentificationMetadata
where
    S: HasTestcase<BytesInput>,
{
    fn post_exec(self, state: &mut S, corpus_id: Option<CorpusId>) -> Result<(), Error> {
        if let Some(corpus_id) = corpus_id {
            let mut tc = state.testcase_mut(corpus_id)?;
            tc.add_metadata(self);
        }
        Ok(())
    }
}

const MAX_CHARS: usize = 16;

fn choose_start<R: Rand>(
    rand: &mut R,
    bytes: &[u8],
    meta: &UnicodeIdentificationMetadata,
) -> Option<(usize, usize)> {
    let bytes_len = NonZero::new(bytes.len())?;

    let idx = rand.below(bytes_len);
    let mut options = Vec::new();
    for (start, range) in meta.ranges() {
        if idx
            .checked_sub(*start) // idx adjusted to start
            .and_then(|idx| (idx < range.len()).then(|| range[idx])) // idx in range
            .is_some_and(|r| r)
        {
            options.push((*start, range));
        }
    }
    match options.len() {
        0 => None,
        1 => Some((options[0].0, options[0].1.len())),
        options_len => {
            // # Safety
            // options.len() is checked above.
            let options_len_squared =
                unsafe { NonZero::new(options_len * options_len).unwrap_unchecked() };
            // bias towards longer strings
            options.sort_by_cached_key(|(_, entries)| entries.count_ones());
            let selected =
                libafl_bolts::math::integer_sqrt(rand.below(options_len_squared) as u64) as usize;
            Some((options[selected].0, options[selected].1.len()))
        }
    }
}

fn get_subcategory<T: Ord + Copy>(needle: T, haystack: &[(T, T)]) -> Option<(T, T)> {
    haystack
        .binary_search_by(|&(min, max)| match min.cmp(&needle) {
            Ordering::Less | Ordering::Equal => match needle.cmp(&max) {
                Ordering::Less | Ordering::Equal => Ordering::Equal,
                Ordering::Greater => Ordering::Less,
            },
            Ordering::Greater => Ordering::Greater,
        })
        .ok()
        .map(|idx| haystack[idx])
}

fn find_range<F: Fn(char) -> bool>(
    chars: &[(usize, char)],
    idx: usize,
    predicate: F,
) -> Range<usize> {
    // walk backwards and discover
    let start = chars[..idx]
        .iter()
        .rev()
        .take_while(|&&(_, c)| predicate(c))
        .last()
        .map_or(chars[idx].0, |&(i, _)| i);
    // walk forwards
    let end = chars[(idx + 1)..]
        .iter()
        .take_while(|&&(_, c)| predicate(c))
        .last()
        .map_or(chars[idx].0 + chars[idx].1.len_utf8(), |&(i, c)| {
            i + c.len_utf8()
        });

    start..end
}

fn choose_category_range<R: Rand>(
    rand: &mut R,
    string: &str,
) -> (Range<usize>, &'static [(u32, u32)]) {
    let chars = string.char_indices().collect::<Vec<_>>();
    let chars_len = NonZero::new(chars.len()).expect("Got empty string in choose_category_range");
    let idx = rand.below(chars_len);
    let c = chars[idx].1;

    // figure out the categories for this char
    let expanded = c as u32;
    #[cfg(test)]
    let mut names = Vec::new();
    let mut categories = Vec::new();
    for (_name, category) in unicode_categories::BY_NAME {
        if get_subcategory(expanded, category).is_some() {
            #[cfg(test)]
            names.push(_name);
            categories.push(category);
        }
    }

    // ok -- we want to bias towards smaller regions to keep the mutations "tight" to original
    // we sort the options by descending length, then pick isqrt of below(n^2)

    categories.sort_by_cached_key(|cat| {
        Reverse(
            cat.iter()
                .map(|&(min, max)| (max - min + 1) as usize)
                .sum::<usize>(),
        )
    });
    let options = NonZero::new(categories.len() * categories.len())
        .expect("Empty categories in choose_category_range");
    let selected_idx = libafl_bolts::math::integer_sqrt(rand.below(options) as u64) as usize;

    let selected = categories[selected_idx];

    #[cfg(test)]
    println!("category for `{c}' ({}): {}", c as u32, names[selected_idx]);

    (
        find_range(&chars, idx, |c| {
            get_subcategory(c as u32, selected).is_some()
        }),
        selected,
    )
}

fn choose_subcategory_range<R: Rand>(rand: &mut R, string: &str) -> (Range<usize>, (u32, u32)) {
    let chars = string.char_indices().collect::<Vec<_>>();
    let idx =
        rand.below(NonZero::new(chars.len()).expect("Empty string in choose_subcategory_range"));
    let c = chars[idx].1;

    // figure out the categories for this char
    let expanded = c as u32;
    #[cfg(test)]
    let mut names = Vec::new();
    let mut subcategories = Vec::new();
    for (_name, category) in unicode_categories::BY_NAME {
        if let Some(subcategory) = get_subcategory(expanded, category) {
            #[cfg(test)]
            names.push(_name);
            subcategories.push(subcategory);
        }
    }

    // see reasoning for selection pattern in choose_category_range

    subcategories.sort_by_key(|&(min, max)| Reverse(max - min + 1));
    let options = NonZero::new(subcategories.len() * subcategories.len())
        .expect("Emtpy subcategories in choose_subcategory_range");
    let selected_idx = libafl_bolts::math::integer_sqrt(rand.below(options) as u64) as usize;
    let selected = subcategories[selected_idx];

    #[cfg(test)]
    println!(
        "subcategory for `{c}' ({}): {} ({:?})",
        c as u32, names[selected_idx], selected
    );

    (
        find_range(&chars, idx, |c| {
            let expanded = c as u32;
            selected.0 <= expanded && expanded <= selected.1
        }),
        selected,
    )
}

fn rand_replace_range<S: HasRand + HasMaxSize, F: Fn(&mut S) -> char>(
    state: &mut S,
    input: &mut UnicodeInput,
    range: Range<usize>,
    char_gen: F,
) -> MutationResult {
    let temp_range = rand_range(state, range.end - range.start, nonzero!(MAX_CHARS));
    let range = (range.start + temp_range.start)..(range.start + temp_range.end);
    let range = match core::str::from_utf8(&input.0.mutator_bytes()[range.clone()]) {
        Ok(_) => range,
        Err(e) => range.start..(range.start + e.valid_up_to()),
    };

    #[cfg(test)]
    println!(
        "mutating range: {:?} ({:?})",
        range,
        core::str::from_utf8(&input.0.mutator_bytes()[range.clone()])
    );
    if range.start == range.end {
        return MutationResult::Skipped;
    }

    let replace_len = state.rand_mut().below(nonzero!(MAX_CHARS));
    let orig_len = range.end - range.start;
    if input.0.len() - orig_len + replace_len > state.max_size() {
        return MutationResult::Skipped;
    }

    let mut replacement = Vec::with_capacity(replace_len);
    let mut dest = [0u8; 4];

    loop {
        let new_c = char_gen(state);
        if replacement.len() + new_c.len_utf8() > replace_len {
            break;
        }
        new_c.encode_utf8(&mut dest);
        replacement.extend_from_slice(&dest[..new_c.len_utf8()]);
        if replacement.len() + new_c.len_utf8() == replace_len {
            break; // nailed it
        }
    }

    input.0.splice(range, replacement);
    input.1 = extract_metadata(input.0.mutator_bytes());

    MutationResult::Mutated
}

/// Mutator which randomly replaces a randomly selected range of bytes with bytes that preserve the
/// range's category
#[derive(Debug, Default)]
pub struct UnicodeCategoryRandMutator;

impl Named for UnicodeCategoryRandMutator {
    fn name(&self) -> &Cow<'static, str> {
        static NAME: Cow<'static, str> = Cow::Borrowed("string-category-rand");
        &NAME
    }
}

impl<S> Mutator<UnicodeInput, S> for UnicodeCategoryRandMutator
where
    S: HasRand + HasMaxSize,
{
    fn mutate(&mut self, state: &mut S, input: &mut UnicodeInput) -> Result<MutationResult, Error> {
        if input.0.mutator_bytes().is_empty() {
            return Ok(MutationResult::Skipped);
        }

        let bytes = input.0.mutator_bytes();
        let meta = &input.1;
        if let Some((base, len)) = choose_start(state.rand_mut(), bytes, meta) {
            let substring = core::str::from_utf8(&bytes[base..][..len])?;
            let (range, category) = choose_category_range(state.rand_mut(), substring);
            #[cfg(test)]
            println!(
                "{:?} => {:?}",
                range,
                core::str::from_utf8(&bytes[range.clone()])
            );

            let options: usize = category
                .iter()
                .map(|&(start, end)| end as usize - start as usize + 1)
                .sum();
            let char_gen = |state: &mut S| loop {
                // Should this skip the mutation instead of expecting?
                let mut selected = state.rand_mut().below(
                    NonZero::new(options).expect("Empty category in UnicodeCatgoryRandMutator"),
                );
                for &(min, max) in category {
                    if let Some(next_selected) =
                        selected.checked_sub(max as usize - min as usize + 1)
                    {
                        selected = next_selected;
                    } else if let Some(new_c) = char::from_u32(selected as u32 + min) {
                        return new_c;
                    } else {
                        break;
                    }
                }
            };

            return Ok(rand_replace_range(state, input, range, char_gen));
        }

        Ok(MutationResult::Skipped)
    }
    #[inline]
    fn post_exec(&mut self, _state: &mut S, _new_corpus_id: Option<CorpusId>) -> Result<(), Error> {
        Ok(())
    }
}

/// Mutator which randomly replaces a randomly selected range of bytes with bytes that preserve the
/// range's subcategory
#[derive(Debug, Default)]
pub struct UnicodeSubcategoryRandMutator;

impl Named for UnicodeSubcategoryRandMutator {
    fn name(&self) -> &Cow<'static, str> {
        static NAME: Cow<'static, str> = Cow::Borrowed("string-subcategory-rand");
        &NAME
    }
}

impl<S> Mutator<UnicodeInput, S> for UnicodeSubcategoryRandMutator
where
    S: HasRand + HasMaxSize,
{
    fn mutate(&mut self, state: &mut S, input: &mut UnicodeInput) -> Result<MutationResult, Error> {
        if input.0.mutator_bytes().is_empty() {
            return Ok(MutationResult::Skipped);
        }

        let bytes = input.0.mutator_bytes();
        let meta = &input.1;
        if let Some((base, len)) = choose_start(state.rand_mut(), bytes, meta) {
            let substring = core::str::from_utf8(&bytes[base..][..len])?;
            let (range, subcategory) = choose_subcategory_range(state.rand_mut(), substring);
            #[cfg(test)]
            println!(
                "{:?} => {:?}",
                range,
                core::str::from_utf8(&bytes[range.clone()])
            );

            let options = subcategory.1 as usize - subcategory.0 as usize + 1;
            let Some(options) = NonZero::new(options) else {
                return Ok(MutationResult::Skipped);
            };
            let char_gen = |state: &mut S| loop {
                let selected = state.rand_mut().below(options);
                if let Some(new_c) = char::from_u32(selected as u32 + subcategory.0) {
                    return new_c;
                }
            };

            return Ok(rand_replace_range(state, input, range, char_gen));
        }

        Ok(MutationResult::Skipped)
    }
    #[inline]
    fn post_exec(&mut self, _state: &mut S, _new_corpus_id: Option<CorpusId>) -> Result<(), Error> {
        Ok(())
    }
}

/// Mutator which randomly replaces a full category-contiguous region of chars with a random token
#[derive(Debug, Default)]
pub struct UnicodeCategoryTokenReplaceMutator;

impl Named for UnicodeCategoryTokenReplaceMutator {
    fn name(&self) -> &Cow<'static, str> {
        static NAME: Cow<'static, str> = Cow::Borrowed("string-category-token-replace");
        &NAME
    }
}

impl<S> Mutator<UnicodeInput, S> for UnicodeCategoryTokenReplaceMutator
where
    S: HasRand + HasMaxSize + HasMetadata,
{
    fn mutate(&mut self, state: &mut S, input: &mut UnicodeInput) -> Result<MutationResult, Error> {
        if input.0.mutator_bytes().is_empty() {
            return Ok(MutationResult::Skipped);
        }

        let Some(meta) = state.metadata_map().get::<Tokens>() else {
            return Ok(MutationResult::Skipped);
        };

        let Some(tokens_len) = NonZero::new(meta.tokens().len()) else {
            return Ok(MutationResult::Skipped);
        };

        let token_idx = state.rand_mut().below(tokens_len);

        let bytes = input.0.mutator_bytes();
        let meta = &input.1;
        if let Some((base, len)) = choose_start(state.rand_mut(), bytes, meta) {
            let substring = core::str::from_utf8(&bytes[base..][..len])?;
            let (range, _) = choose_category_range(state.rand_mut(), substring);

            #[cfg(test)]
            println!(
                "{:?} => {:?}",
                range,
                core::str::from_utf8(&bytes[range.clone()])
            );

            let meta = state.metadata_map().get::<Tokens>().unwrap();
            let token = &meta.tokens()[token_idx];

            if input.0.len() - (range.end - range.start) + token.len() > state.max_size() {
                return Ok(MutationResult::Skipped);
            }

            input.0.splice(range, token.iter().copied());
            input.1 = extract_metadata(input.0.mutator_bytes());
            return Ok(MutationResult::Mutated);
        }

        Ok(MutationResult::Skipped)
    }
    #[inline]
    fn post_exec(&mut self, _state: &mut S, _new_corpus_id: Option<CorpusId>) -> Result<(), Error> {
        Ok(())
    }
}

/// Mutator which randomly replaces a full subcategory-contiguous region of chars with a random token
#[derive(Debug, Default)]
pub struct UnicodeSubcategoryTokenReplaceMutator;

impl Named for UnicodeSubcategoryTokenReplaceMutator {
    fn name(&self) -> &Cow<'static, str> {
        static NAME: Cow<'static, str> = Cow::Borrowed("string-subcategory-replace");
        &NAME
    }
}

impl<S> Mutator<UnicodeInput, S> for UnicodeSubcategoryTokenReplaceMutator
where
    S: HasRand + HasMaxSize + HasMetadata,
{
    fn mutate(&mut self, state: &mut S, input: &mut UnicodeInput) -> Result<MutationResult, Error> {
        if input.0.mutator_bytes().is_empty() {
            return Ok(MutationResult::Skipped);
        }

        let Some(meta) = state.metadata_map().get::<Tokens>() else {
            return Ok(MutationResult::Skipped);
        };

        let Some(tokens_len) = NonZero::new(meta.tokens().len()) else {
            return Ok(MutationResult::Skipped);
        };

        let token_idx = state.rand_mut().below(tokens_len);

        let bytes = input.0.mutator_bytes();
        let meta = &input.1;
        if let Some((base, len)) = choose_start(state.rand_mut(), bytes, meta) {
            let substring = core::str::from_utf8(&bytes[base..][..len])?;
            let (range, _) = choose_subcategory_range(state.rand_mut(), substring);

            #[cfg(test)]
            println!(
                "{:?} => {:?}",
                range,
                core::str::from_utf8(&bytes[range.clone()])
            );

            let meta = state.metadata_map().get::<Tokens>().unwrap();
            let token = &meta.tokens()[token_idx];

            if input.0.len() - (range.end - range.start) + token.len() > state.max_size() {
                return Ok(MutationResult::Skipped);
            }

            input.0.splice(range, token.iter().copied());
            input.1 = extract_metadata(input.0.mutator_bytes());
            return Ok(MutationResult::Mutated);
        }

        Ok(MutationResult::Skipped)
    }
    #[inline]
    fn post_exec(&mut self, _state: &mut S, _new_corpus_id: Option<CorpusId>) -> Result<(), Error> {
        Ok(())
    }
}

#[cfg(test)]
mod test {
    use libafl_bolts::{Error, rands::StdRand};

    use crate::{
        corpus::NopCorpus,
        inputs::{BytesInput, HasMutatorBytes},
        mutators::{Mutator, UnicodeCategoryRandMutator, UnicodeSubcategoryRandMutator},
        stages::extract_metadata,
        state::StdState,
    };

    // a not-so-useful test for this
    #[test]
    fn mutate_hex() {
        let result: Result<(), Error> = (|| {
            let hex = "0123456789abcdef0123456789abcdef";
            let mut bytes = BytesInput::from(hex.as_bytes());

            let mut mutator = UnicodeCategoryRandMutator;

            let mut state = StdState::new(
                StdRand::with_seed(0),
                NopCorpus::<BytesInput>::new(),
                NopCorpus::new(),
                &mut (),
                &mut (),
            )?;

            for _ in 0..(1 << 12) {
                let metadata = extract_metadata(bytes.mutator_bytes());
                let mut input = (bytes, metadata);
                let _ = mutator.mutate(&mut state, &mut input);
                println!(
                    "{:?}",
                    core::str::from_utf8(input.0.mutator_bytes()).unwrap()
                );
                bytes = input.0;
            }

            Ok(())
        })();

        if let Err(e) = result {
            panic!("failed with error: {e}");
        }
    }

    #[test]
    fn mutate_hex_subcat() {
        let result: Result<(), Error> = (|| {
            let hex = "0123456789abcdef0123456789abcdef";
            let mut bytes = BytesInput::from(hex.as_bytes());

            let mut mutator = UnicodeSubcategoryRandMutator;

            let mut state = StdState::new(
                StdRand::with_seed(0),
                NopCorpus::<BytesInput>::new(),
                NopCorpus::new(),
                &mut (),
                &mut (),
            )?;

            for _ in 0..(1 << 12) {
                let metadata = extract_metadata(bytes.mutator_bytes());
                let mut input = (bytes, metadata);
                let _ = mutator.mutate(&mut state, &mut input);
                println!(
                    "{:?}",
                    core::str::from_utf8(input.0.mutator_bytes()).unwrap()
                );
                bytes = input.0;
            }

            Ok(())
        })();

        if let Err(e) = result {
            panic!("failed with error: {e}");
        }
    }
}
