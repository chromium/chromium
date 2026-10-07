//! Allowing mixing and matching between [`Mutator`] and [`crate::inputs::Input`] types.
use alloc::borrow::Cow;

use libafl_bolts::{Named, tuples::MappingFunctor};

use crate::{
    Error,
    corpus::CorpusId,
    mutators::{MutationResult, Mutator},
};

/// Mapping [`Mutator`] using a function returning a reference.
///
/// Allows using [`Mutator`]s for a certain type on (parts of) other input types that can be mapped to this type.
///
/// For a more flexible alternative, which allows access to `state`, see [`StateAwareMappingMutator`].
///
/// # Example
#[cfg_attr(feature = "std", doc = " ```")]
#[cfg_attr(not(feature = "std"), doc = " ```ignore")]
/// use std::vec::Vec;
///
/// use libafl::{
///     mutators::{ByteIncMutator, MappingMutator, MutationResult, Mutator},
///     state::NopState,
/// };
///
/// #[derive(Debug, PartialEq)]
/// struct CustomInput(Vec<u8>);
///
/// impl CustomInput {
///     pub fn vec_mut(&mut self) -> &mut Vec<u8> {
///         &mut self.0
///     }
/// }
///
/// // construct a mutator that works on &mut Vec<u8> (since it impls `HasMutatorBytes`)
/// let inner = ByteIncMutator::new();
/// // construct a mutator that works on &mut CustomInput
/// let mut outer = MappingMutator::new(CustomInput::vec_mut, inner);
///
/// let mut input = CustomInput(vec![1]);
///
/// let mut state: NopState<CustomInput> = NopState::new();
/// let res = outer.mutate(&mut state, &mut input).unwrap();
/// assert_eq!(res, MutationResult::Mutated);
/// assert_eq!(input, CustomInput(vec![2],));
/// ```
#[derive(Debug)]
pub struct MappingMutator<M, F> {
    mapper: F,
    inner: M,
    name: Cow<'static, str>,
}

impl<M, F> MappingMutator<M, F> {
    /// Creates a new [`MappingMutator`]
    pub fn new(mapper: F, inner: M) -> Self
    where
        M: Named,
    {
        let name = Cow::Owned(format!("MappingMutator<{}>", inner.name()));
        Self {
            mapper,
            inner,
            name,
        }
    }
}

impl<M, S, F, IO, II> Mutator<IO, S> for MappingMutator<M, F>
where
    F: FnMut(&mut IO) -> &mut II,
    M: Mutator<II, S>,
{
    fn mutate(&mut self, state: &mut S, input: &mut IO) -> Result<MutationResult, Error> {
        self.inner.mutate(state, (self.mapper)(input))
    }
    #[inline]
    fn post_exec(&mut self, state: &mut S, new_corpus_id: Option<CorpusId>) -> Result<(), Error> {
        self.inner.post_exec(state, new_corpus_id)
    }
}

impl<M, F> Named for MappingMutator<M, F> {
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

/// Mapper to use to map a [`tuple_list`] of [`Mutator`]s using [`MappingMutator`]s.
///
/// See the explanation of [`MappingMutator`] for details.
///
/// # Example
#[cfg_attr(feature = "std", doc = " ```")]
#[cfg_attr(not(feature = "std"), doc = " ```ignore")]
/// use std::vec::Vec;
///
/// use libafl::{
///     mutators::{
///         ByteIncMutator, MutationResult, MutatorsTuple, ToMappingMutator,
///     },
///     state::NopState,
/// };
///  
/// use libafl_bolts::tuples::{tuple_list, Map};
///  
/// #[derive(Debug, PartialEq)]
/// struct CustomInput(Vec<u8>);
///  
/// impl CustomInput {
///     pub fn vec_mut(&mut self) -> &mut Vec<u8> {
///         &mut self.0
///     }
/// }
///  
/// // construct a mutator that works on &mut Vec<u8> (since it impls `HasMutatorBytes`)
/// let mutators = tuple_list!(ByteIncMutator::new(), ByteIncMutator::new());
/// // construct a mutator that works on &mut CustomInput
/// let mut mapped_mutators =
///     mutators.map(ToMappingMutator::new(CustomInput::vec_mut));
///  
/// let mut input = CustomInput(vec![1]);
///  
/// let mut state: NopState<CustomInput> = NopState::new();
/// let res = mapped_mutators.mutate_all(&mut state, &mut input).unwrap();
/// assert_eq!(res, MutationResult::Mutated);
/// assert_eq!(input, CustomInput(vec![3],));
/// ```
#[derive(Debug)]
pub struct ToMappingMutator<F> {
    mapper: F,
}

impl<F> ToMappingMutator<F> {
    /// Creates a new [`ToMappingMutator`]
    pub fn new(mapper: F) -> Self {
        Self { mapper }
    }
}

impl<M, F> MappingFunctor<M> for ToMappingMutator<F>
where
    F: Clone,
    M: Named,
{
    type Output = MappingMutator<M, F>;

    fn apply(&mut self, from: M) -> Self::Output {
        MappingMutator::new(self.mapper.clone(), from)
    }
}

/// Mapping [`Mutator`] for dealing with input parts wrapped in [`Option`].
///
/// Allows using [`Mutator`]s for a certain type on (parts of) other input types that can be mapped to an [`Option`] of said type.
///
/// Returns [`MutationResult::Skipped`] if the mapper returns [`None`].
///
/// # Example
#[cfg_attr(feature = "std", doc = " ```")]
#[cfg_attr(not(feature = "std"), doc = " ```ignore")]
/// use libafl::{
///     inputs::MutVecInput,
///     mutators::{ByteIncMutator, MutationResult, Mutator, OptionalMutator},
///     state::NopState,
/// };
///
/// let inner = ByteIncMutator::new();
/// let mut outer = OptionalMutator::new(inner);
///
/// let mut input_raw = vec![1];
/// let input: MutVecInput = (&mut input_raw).into();
/// let mut input_wrapped = Some(input);
/// let mut state: NopState<Option<MutVecInput>> = NopState::new();
/// let res = outer.mutate(&mut state, &mut input_wrapped).unwrap();
/// assert_eq!(res, MutationResult::Mutated);
/// assert_eq!(input_raw, vec![2]);
///
/// let mut empty_input: Option<MutVecInput> = None;
/// let res2 = outer.mutate(&mut state, &mut empty_input).unwrap();
/// assert_eq!(res2, MutationResult::Skipped);
/// ```
#[derive(Debug)]
pub struct OptionalMutator<M> {
    inner: M,
    name: Cow<'static, str>,
}

impl<M> OptionalMutator<M> {
    /// Creates a new [`OptionalMutator`]
    pub fn new(inner: M) -> Self
    where
        M: Named,
    {
        let name = Cow::Owned(format!("OptionalMutator<{}>", inner.name()));
        Self { inner, name }
    }
}

impl<I, S, M> Mutator<Option<I>, S> for OptionalMutator<M>
where
    M: Mutator<I, S>,
{
    fn mutate(&mut self, state: &mut S, input: &mut Option<I>) -> Result<MutationResult, Error> {
        match input {
            None => Ok(MutationResult::Skipped),
            Some(i) => self.inner.mutate(state, i),
        }
    }
    #[inline]
    fn post_exec(&mut self, state: &mut S, new_corpus_id: Option<CorpusId>) -> Result<(), Error> {
        self.inner.post_exec(state, new_corpus_id)
    }
}

impl<M> Named for OptionalMutator<M>
where
    M: Named,
{
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

/// Mapper to use to map a [`tuple_list`] of [`Mutator`]s using [`OptionalMutator`]s.
///
/// See the explanation of [`OptionalMutator`] for details.
///
/// # Example
#[cfg_attr(feature = "std", doc = " ```")]
#[cfg_attr(not(feature = "std"), doc = " ```ignore")]
/// use libafl::{
///     inputs::MutVecInput,
///     mutators::{ByteIncMutator, MutationResult, Mutator, ToOptionalMutator},
///     state::NopState,
/// };
/// use libafl_bolts::tuples::{tuple_list, Map};
///
/// let inner = tuple_list!(ByteIncMutator::new());
/// let outer_list = inner.map(ToOptionalMutator);
/// let mut outer = outer_list.0;
///
/// let mut input_raw = vec![1];
/// let input: MutVecInput = (&mut input_raw).into();
/// let mut input_wrapped = Some(input);
/// let mut state: NopState<Option<MutVecInput>> = NopState::new();
/// let res = outer.mutate(&mut state, &mut input_wrapped).unwrap();
/// assert_eq!(res, MutationResult::Mutated);
/// assert_eq!(input_raw, vec![2]);
///
/// let mut empty_input: Option<MutVecInput> = None;
/// let res2 = outer.mutate(&mut state, &mut empty_input).unwrap();
/// assert_eq!(res2, MutationResult::Skipped);
/// ```
#[derive(Debug)]
pub struct ToOptionalMutator;

impl<M> MappingFunctor<M> for ToOptionalMutator
where
    M: Named,
{
    type Output = OptionalMutator<M>;

    fn apply(&mut self, from: M) -> Self::Output {
        OptionalMutator::new(from)
    }
}

/// Mapping [`Mutator`] using a function returning a reference.
///
/// Allows using [`Mutator`]s for a certain type on (parts of) other input types that can be mapped to this type.
///
/// Provides access to the state. If [`Option::None`] is returned from the mapping function, the mutator is returning [`MutationResult::Skipped`].
///
/// # Example
#[cfg_attr(feature = "std", doc = " ```")]
#[cfg_attr(not(feature = "std"), doc = " ```ignore")]
/// use std::vec::Vec;
///
/// use libafl::{
///     mutators::{ByteIncMutator, MutationResult, Mutator, StateAwareMappingMutator},
///     state::{HasRand, NopState},
/// };
/// use libafl_bolts::rands::Rand as _;
///
/// #[derive(Debug, PartialEq)]
/// struct CustomInput(Vec<u8>);
///
/// impl CustomInput {
///     pub fn possibly_vec<'a, S: HasRand>(
///         &'a mut self,
///         state: &'a mut S,
///     ) -> (Option<&'a mut Vec<u8>>, &'a mut S) {
///         // we have access to the state
///         if state.rand_mut().coinflip(0.5) {
///             // If this input cannot be mutated with the outer mutator, return None
///             // e.g. because the input doesn't contain the field this mutator is supposed to mutate
///             (None, state)
///         } else {
///             // else, return the type that the outer mutator can mutate
///             (Some(&mut self.0), state)
///         }
///     }
/// }
///
/// // construct a mutator that works on &mut Vec<u8> (since it impls `HasMutatorBytes`)
/// let inner = ByteIncMutator::new();
/// // construct a mutator that works on &mut CustomInput
/// let mut outer = StateAwareMappingMutator::new(CustomInput::possibly_vec, inner);
///
/// let mut input = CustomInput(vec![1]);
///
/// let mut state: NopState<CustomInput> = NopState::new();
/// let res = outer.mutate(&mut state, &mut input).unwrap();
/// if res == MutationResult::Mutated {
///     assert_eq!(input, CustomInput(vec![2],));
/// } else {
///     assert_eq!(input, CustomInput(vec![1],));
/// }
/// ```
#[derive(Debug)]
pub struct StateAwareMappingMutator<M, F> {
    mapper: F,
    inner: M,
    name: Cow<'static, str>,
}

impl<M, F> StateAwareMappingMutator<M, F> {
    /// Creates a new [`StateAwareMappingMutator`]
    pub fn new(mapper: F, inner: M) -> Self
    where
        M: Named,
    {
        let name = Cow::Owned(format!("StateAwareMappingMutator<{}>", inner.name()));
        Self {
            mapper,
            inner,
            name,
        }
    }
}

impl<M, S, F, IO, II> Mutator<IO, S> for StateAwareMappingMutator<M, F>
where
    F: for<'a> FnMut(&'a mut IO, &'a mut S) -> (Option<&'a mut II>, &'a mut S),
    M: Mutator<II, S>,
{
    fn mutate(&mut self, state: &mut S, input: &mut IO) -> Result<MutationResult, Error> {
        let (mapped, state) = (self.mapper)(input, state);
        match mapped {
            Some(mapped) => self.inner.mutate(state, mapped),
            None => Ok(MutationResult::Skipped),
        }
    }
    #[inline]
    fn post_exec(&mut self, state: &mut S, new_corpus_id: Option<CorpusId>) -> Result<(), Error> {
        self.inner.post_exec(state, new_corpus_id)
    }
}

impl<M, F> Named for StateAwareMappingMutator<M, F> {
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

/// Mapper to use to map a [`tuple_list`] of [`Mutator`]s using [`StateAwareMappingMutator`]s.
///
/// See the explanation of [`StateAwareMappingMutator`] for details.
///
/// # Example
#[cfg_attr(feature = "std", doc = " ```")]
#[cfg_attr(not(feature = "std"), doc = " ```ignore")]
/// use std::vec::Vec;
///
/// use libafl::{
///     mutators::{
///         ByteIncMutator, MutationResult, MutatorsTuple, ToStateAwareMappingMutator,
///     },
///     state::{HasRand, NopState},
/// };
///  
/// use libafl_bolts::{
///     tuples::{tuple_list, Map},
///     rands::Rand as _,
/// };
///  
/// #[derive(Debug, PartialEq)]
/// struct CustomInput(Vec<u8>);
///  
/// impl CustomInput {
///     pub fn possibly_vec<'a, S: HasRand>(
///         &'a mut self,
///         state: &'a mut S,
///     ) -> (Option<&'a mut Vec<u8>>, &'a mut S) {
///         // we have access to the state
///         if state.rand_mut().coinflip(0.5) {
///             // If this input cannot be mutated with the outer mutator, return None
///             // e.g. because the input doesn't contain the field this mutator is supposed to mutate
///             (None, state)
///         } else {
///             // else, return the type that the outer mutator can mutate
///             (Some(&mut self.0), state)
///         }
///     }
/// }
///  
/// // construct a mutator that works on &mut Vec<u8> (since it impls `HasMutatorBytes`)
/// let mutators = tuple_list!(ByteIncMutator::new(), ByteIncMutator::new());
/// // construct a mutator that works on &mut CustomInput
/// let mut mapped_mutators =
///     mutators.map(ToStateAwareMappingMutator::new(CustomInput::possibly_vec));
///  
/// let mut input = CustomInput(vec![1]);
///  
/// let mut state: NopState<CustomInput> = NopState::new();
/// let res = mapped_mutators.mutate_all(&mut state, &mut input).unwrap();
/// if res == MutationResult::Mutated {
///     // no way of knowing if either or both mutated
///     assert!(input.0 == vec![2] || input.0 == vec![3]);
/// } else {
///     assert_eq!(input, CustomInput(vec![1],));
/// }
/// ```
#[derive(Debug)]
pub struct ToStateAwareMappingMutator<F> {
    mapper: F,
}

impl<F> ToStateAwareMappingMutator<F> {
    /// Creates a new [`ToStateAwareMappingMutator`]
    pub fn new(mapper: F) -> Self {
        Self { mapper }
    }
}

impl<M, F> MappingFunctor<M> for ToStateAwareMappingMutator<F>
where
    F: Clone,
    M: Named,
{
    type Output = StateAwareMappingMutator<M, F>;

    fn apply(&mut self, from: M) -> Self::Output {
        StateAwareMappingMutator::new(self.mapper.clone(), from)
    }
}
