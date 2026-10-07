//! [`crate::mutators::Mutator`] collection equivalent to AFL++'s havoc mutations

use libafl_bolts::tuples::{
    Map, Merge, map_tuple_list_type, merge_tuple_list_type, tuple_list, tuple_list_type,
};

use crate::mutators::{
    mapping::{ToMappingMutator, ToOptionalMutator, ToStateAwareMappingMutator},
    mutations::{
        BitFlipMutator, ByteAddMutator, ByteDecMutator, ByteFlipMutator, ByteIncMutator,
        ByteInterestingMutator, ByteNegMutator, ByteRandMutator, BytesCopyMutator,
        BytesDeleteMutator, BytesExpandMutator, BytesInsertCopyMutator, BytesInsertMutator,
        BytesRandInsertMutator, BytesRandSetMutator, BytesSetMutator, BytesSwapMutator,
        CrossoverInsertMutator, CrossoverReplaceMutator, DwordAddMutator, DwordInterestingMutator,
        MappedCrossoverInsertMutator, MappedCrossoverReplaceMutator, QwordAddMutator,
        WordAddMutator, WordInterestingMutator,
    },
};

/// Tuple type of the mutations that compose the Havoc mutator without crossover mutations
pub type HavocMutationsNoCrossoverType = tuple_list_type!(
    BitFlipMutator,
    ByteFlipMutator,
    ByteIncMutator,
    ByteDecMutator,
    ByteNegMutator,
    ByteRandMutator,
    ByteAddMutator,
    WordAddMutator,
    DwordAddMutator,
    QwordAddMutator,
    ByteInterestingMutator,
    WordInterestingMutator,
    DwordInterestingMutator,
    BytesDeleteMutator,
    BytesDeleteMutator,
    BytesDeleteMutator,
    BytesDeleteMutator,
    BytesExpandMutator,
    BytesInsertMutator,
    BytesRandInsertMutator,
    BytesSetMutator,
    BytesRandSetMutator,
    BytesCopyMutator,
    BytesInsertCopyMutator,
    BytesSwapMutator,
);

/// Tuple type of the mutations that compose the Havoc mutator's crossover mutations
pub type HavocCrossoverType = tuple_list_type!(CrossoverInsertMutator, CrossoverReplaceMutator);

/// Tuple type of the mutations that compose the Havoc mutator's crossover mutations for mapped input types
pub type MappedHavocCrossoverType<F, I, O> = tuple_list_type!(
    MappedCrossoverInsertMutator<F, I, O>,
    MappedCrossoverReplaceMutator<F, I, O>,
);

/// Tuple type of the mutations that compose the Havoc mutator
pub type HavocMutationsType =
    merge_tuple_list_type!(HavocMutationsNoCrossoverType, HavocCrossoverType);

/// Tuple type of the mutations that compose the Havoc mutator for mapped input types
pub type MappedHavocMutationsType<F1, F2, I, O> = map_tuple_list_type!(
    merge_tuple_list_type!(HavocMutationsNoCrossoverType, MappedHavocCrossoverType<F2,I, O>),
    ToMappingMutator<F1>
);

/// Tuple type of the mutations that compose the Havoc mutator for state-aware-mapped input types
pub type StateAwareMappedHavocMutationsType<F1, F2, I, O> = map_tuple_list_type!(
    merge_tuple_list_type!(HavocMutationsNoCrossoverType, MappedHavocCrossoverType<F2,I, O>),
    ToStateAwareMappingMutator<F1>
);

/// Tuple type of the mutations that compose the Havoc mutator for mapped input types, for optional byte array input parts
pub type OptionMappedHavocMutationsType<F1, F2, I, O> = map_tuple_list_type!(
    map_tuple_list_type!(
        merge_tuple_list_type!(HavocMutationsNoCrossoverType, MappedHavocCrossoverType<F2,I, O>),
        ToOptionalMutator
    ),
    ToMappingMutator<F1>
);

/// Get the mutations that compose the Havoc mutator (only applied to single inputs)
#[must_use]
pub fn havoc_mutations_no_crossover() -> HavocMutationsNoCrossoverType {
    tuple_list!(
        BitFlipMutator::new(),
        ByteFlipMutator::new(),
        ByteIncMutator::new(),
        ByteDecMutator::new(),
        ByteNegMutator::new(),
        ByteRandMutator::new(),
        ByteAddMutator::new(),
        WordAddMutator::new(),
        DwordAddMutator::new(),
        QwordAddMutator::new(),
        ByteInterestingMutator::new(),
        WordInterestingMutator::new(),
        DwordInterestingMutator::new(),
        BytesDeleteMutator::new(),
        BytesDeleteMutator::new(),
        BytesDeleteMutator::new(),
        BytesDeleteMutator::new(),
        BytesExpandMutator::new(),
        BytesInsertMutator::new(),
        BytesRandInsertMutator::new(),
        BytesSetMutator::new(),
        BytesRandSetMutator::new(),
        BytesCopyMutator::new(),
        BytesInsertCopyMutator::new(),
        BytesSwapMutator::new(),
    )
}

/// Get the mutations that compose the Havoc mutator's crossover strategy
#[must_use]
pub fn havoc_crossover() -> HavocCrossoverType {
    tuple_list!(
        CrossoverInsertMutator::new(),
        CrossoverReplaceMutator::new(),
    )
}

/// Get the mutations that compose the Havoc mutator's crossover strategy with custom corpus extraction logic
pub fn havoc_crossover_with_corpus_mapper<F, I, IO, O>(
    input_mapper: F,
) -> MappedHavocCrossoverType<F, I, O>
where
    F: Clone + Fn(&IO) -> &O,
{
    tuple_list!(
        MappedCrossoverInsertMutator::new(input_mapper.clone()),
        MappedCrossoverReplaceMutator::new(input_mapper.clone()),
    )
}

/// Get the mutations that compose the Havoc mutator's crossover strategy with custom corpus extraction logic
pub fn havoc_crossover_with_corpus_mapper_optional<F, I, O>(
    input_mapper: F,
) -> MappedHavocCrossoverType<F, I, O>
where
    F: Clone,
{
    tuple_list!(
        MappedCrossoverInsertMutator::new(input_mapper.clone()),
        MappedCrossoverReplaceMutator::new(input_mapper.clone()),
    )
}

/// Get the mutations that compose the Havoc mutator
#[must_use]
pub fn havoc_mutations() -> HavocMutationsType {
    havoc_mutations_no_crossover().merge(havoc_crossover())
}

/// Get the mutations that compose the Havoc mutator for mapped input types
///
/// Check the example fuzzer for details on how to use this.
#[must_use]
pub fn mapped_havoc_mutations<F1, F2, IO1, IO2, II, O>(
    current_input_mapper: F1,
    input_from_corpus_mapper: F2,
) -> MappedHavocMutationsType<F1, F2, IO1, O>
where
    F1: Clone + FnMut(&mut IO1) -> &mut II,
    F2: Clone + Fn(&IO2) -> &O,
{
    havoc_mutations_no_crossover()
        .merge(havoc_crossover_with_corpus_mapper(input_from_corpus_mapper))
        .map(ToMappingMutator::new(current_input_mapper))
}

/// Get the mutations that compose the Havoc mutator for state-aware-mapped input types
///
/// Check the example fuzzer for details on how to use this.
/// Check the docs of [`crate::mutators::mapping::StateAwareMappingMutator`] for how mapping works internally.
#[must_use]
pub fn state_aware_mapped_havoc_mutations<'a, F1, F2, IO1, IO2, II, O, S>(
    current_input_mapper: F1,
    input_from_corpus_mapper: F2,
) -> StateAwareMappedHavocMutationsType<F1, F2, IO1, O>
where
    F1: Clone + FnMut(&'a mut IO1, &'a mut S) -> (Option<&'a mut II>, &'a mut S),
    F2: Clone + Fn(&IO2) -> &O,
    II: 'a,
    IO1: 'a,
    S: 'a,
{
    havoc_mutations_no_crossover()
        .merge(havoc_crossover_with_corpus_mapper(input_from_corpus_mapper))
        .map(ToStateAwareMappingMutator::new(current_input_mapper))
}

/// Get the mutations that compose the Havoc mutator for mapped input types, for optional input parts
///
/// Check the example fuzzer for details on how to use this.
#[must_use]
pub fn optional_mapped_havoc_mutations<F1, F2, IO1, IO2, II, O>(
    current_input_mapper: F1,
    input_from_corpus_mapper: F2,
) -> OptionMappedHavocMutationsType<F1, F2, IO1, O>
where
    F1: Clone + FnMut(&mut IO1) -> &mut II,
    F2: Clone + Fn(&IO2) -> &O,
{
    havoc_mutations_no_crossover()
        .merge(havoc_crossover_with_corpus_mapper_optional(
            input_from_corpus_mapper,
        ))
        .map(ToOptionalMutator)
        .map(ToMappingMutator::new(current_input_mapper))
}
