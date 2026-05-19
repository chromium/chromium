//! The [`DumpToDiskStage`] is a stage that dumps the corpus and the solutions to disk to e.g. allow AFL to sync

use alloc::{
    string::{String, ToString},
    vec::Vec,
};
use core::{clone::Clone, marker::PhantomData};
use std::{
    fs::{self, File},
    io::Write,
    path::{Path, PathBuf},
};

use libafl_bolts::impl_serdeany;
use serde::{Deserialize, Serialize};

use crate::{
    Error,
    common::HasMetadata,
    corpus::{Corpus, CorpusId, Testcase},
    fuzzer::HasTargetBytesConverter,
    inputs::{Input, ToTargetBytes},
    stages::{Restartable, Stage},
    state::{HasCorpus, HasRand, HasSolutions},
};

/// The default function to generate a filename for a testcase
pub fn generate_filename<I: Input>(testcase: &Testcase<I>, id: &CorpusId) -> String {
    [
        Some(id.0.to_string()),
        testcase.filename().clone(),
        testcase
            .input()
            .as_ref()
            .map(|t| t.generate_name(Some(*id))),
    ]
    .iter()
    .flatten()
    .map(String::as_str)
    .collect::<Vec<_>>()
    .join("-")
}

/// The default function to create the directories if they don't exist
fn create_dirs_if_needed<A, B>(corpus_dir: A, solutions_dir: B) -> Result<(PathBuf, PathBuf), Error>
where
    A: Into<PathBuf>,
    B: Into<PathBuf>,
{
    let corpus_dir = corpus_dir.into();
    if let Err(e) = fs::create_dir(&corpus_dir) {
        if !corpus_dir.is_dir() {
            return Err(Error::os_error(
                e,
                format!("Error creating directory {}", corpus_dir.display()),
            ));
        }
    }
    let solutions_dir = solutions_dir.into();
    if let Err(e) = fs::create_dir(&solutions_dir) {
        if !solutions_dir.is_dir() {
            return Err(Error::os_error(
                e,
                format!("Error creating directory {}", solutions_dir.display()),
            ));
        }
    }
    Ok((corpus_dir, solutions_dir))
}

/// The default function to dump a corpus to disk
fn dump_from_corpus<C, F, G, I, P>(
    corpus: &C,
    dir: &Path,
    generate_filename: &mut G,
    get_bytes: &mut F,
    start_id: Option<CorpusId>,
) -> Result<(), Error>
where
    C: Corpus<I>,
    I: Input,
    F: FnMut(&mut Testcase<I>) -> Result<Vec<u8>, Error>,
    G: FnMut(&Testcase<I>, &CorpusId) -> P,
    P: AsRef<Path>,
{
    let mut id = start_id.or_else(|| corpus.first());
    while let Some(i) = id {
        let testcase = corpus.get(i)?;
        let fname = dir.join(generate_filename(&testcase.borrow(), &i));

        let mut testcase = testcase.borrow_mut();
        corpus.load_input_into(&mut testcase)?;
        let bytes = get_bytes(&mut testcase)?;

        let mut f = File::create(fname)?;
        f.write_all(&bytes)?;

        id = corpus.next(i);
    }
    Ok(())
}

/// Metadata used to store information about disk dump indexes for names
#[cfg_attr(
    any(not(feature = "serdeany_autoreg"), miri),
    expect(clippy::unsafe_derive_deserialize)
)] // for SerdeAny
#[derive(Default, Serialize, Deserialize, Debug, Clone)]
pub struct DumpToDiskMetadata {
    last_corpus: Option<CorpusId>,
    last_solution: Option<CorpusId>,
}

impl_serdeany!(DumpToDiskMetadata);

/// The [`DumpToDiskStage`] is a stage that dumps the corpus and the solutions to disk
#[derive(Debug)]
pub struct DumpToDiskStage<CB1, CB2, EM, I, S, Z> {
    solutions_dir: PathBuf,
    corpus_dir: PathBuf,
    to_bytes: CB1,
    generate_filename: CB2,
    phantom: PhantomData<(EM, I, S, Z)>,
}

impl<CB1, CB2, E, EM, I, S, P, Z> Stage<E, EM, S, Z> for DumpToDiskStage<CB1, CB2, EM, I, S, Z>
where
    CB1: FnMut(&Testcase<I>, &S) -> Vec<u8>,
    CB2: FnMut(&Testcase<I>, &CorpusId) -> P,
    S: HasCorpus<I> + HasSolutions<I> + HasRand + HasMetadata,
    I: Input,
    P: AsRef<Path>,
{
    #[inline]
    fn perform(
        &mut self,
        _fuzzer: &mut Z,
        _executor: &mut E,
        state: &mut S,
        _manager: &mut EM,
    ) -> Result<(), Error> {
        let (last_corpus, last_solution) =
            if let Some(meta) = state.metadata_map().get::<DumpToDiskMetadata>() {
                (
                    meta.last_corpus.and_then(|x| state.corpus().next(x)),
                    meta.last_solution.and_then(|x| state.solutions().next(x)),
                )
            } else {
                (state.corpus().first(), state.solutions().first())
            };

        let mut get_bytes = |tc: &mut Testcase<I>| Ok((self.to_bytes)(tc, state));

        dump_from_corpus(
            state.corpus(),
            &self.corpus_dir,
            &mut self.generate_filename,
            &mut get_bytes,
            last_corpus,
        )?;

        dump_from_corpus(
            state.solutions(),
            &self.solutions_dir,
            &mut self.generate_filename,
            &mut get_bytes,
            last_solution,
        )?;

        state.add_metadata(DumpToDiskMetadata {
            last_corpus: state.corpus().last(),
            last_solution: state.solutions().last(),
        });

        Ok(())
    }
}

impl<CB1, EM, I, S, Z> Restartable<S>
    for DumpToDiskStage<CB1, fn(&Testcase<I>, &CorpusId) -> String, EM, I, S, Z>
{
    #[inline]
    fn should_restart(&mut self, _state: &mut S) -> Result<bool, Error> {
        // Not executing the target, so restart safety is not needed
        Ok(true)
    }

    #[inline]
    fn clear_progress(&mut self, _state: &mut S) -> Result<(), Error> {
        // Not executing the target, so restart safety is not needed
        Ok(())
    }
}

/// Implementation for `DumpToDiskStage` with a default `generate_filename` function.
impl<CB1, EM, I, S, Z> DumpToDiskStage<CB1, fn(&Testcase<I>, &CorpusId) -> String, EM, I, S, Z>
where
    S: HasSolutions<I> + HasRand + HasMetadata,
    I: Input,
{
    /// Create a new [`DumpToDiskStage`] with a default `generate_filename` function.
    pub fn new<A, B>(to_bytes: CB1, corpus_dir: A, solutions_dir: B) -> Result<Self, Error>
    where
        A: Into<PathBuf>,
        B: Into<PathBuf>,
    {
        Self::new_with_custom_filenames(
            to_bytes,
            generate_filename, // This is now of type `fn(&Testcase<EM::Input>, &CorpusId) -> String`
            corpus_dir,
            solutions_dir,
        )
    }
}

impl<CB1, CB2, EM, I, S, Z> DumpToDiskStage<CB1, CB2, EM, I, S, Z>
where
    S: HasMetadata + HasSolutions<I>,
{
    /// Create a new [`DumpToDiskStage`] with a custom `generate_filename` function.
    pub fn new_with_custom_filenames<A, B>(
        to_bytes: CB1,
        generate_filename: CB2,
        corpus_dir: A,
        solutions_dir: B,
    ) -> Result<Self, Error>
    where
        A: Into<PathBuf>,
        B: Into<PathBuf>,
    {
        let (corpus_dir, solutions_dir) = create_dirs_if_needed(corpus_dir, solutions_dir)?;
        Ok(Self {
            to_bytes,
            generate_filename,
            solutions_dir,
            corpus_dir,
            phantom: PhantomData,
        })
    }
}

/// A stage that dumps the corpus and the solutions to disk,
/// using the fuzzer's [`crate::fuzzer::HasTargetBytesConverter`] (if available).
///
/// Set the converter using the fuzzer builder's [`crate::fuzzer::StdFuzzerBuilder::target_bytes_converter`].
#[derive(Debug)]
pub struct DumpTargetBytesToDiskStage<CB, EM, I, S, Z> {
    solutions_dir: PathBuf,
    corpus_dir: PathBuf,
    generate_filename: CB,
    phantom: PhantomData<(EM, I, S, Z)>,
}

impl<CB, E, EM, I, S, P, Z> Stage<E, EM, S, Z> for DumpTargetBytesToDiskStage<CB, EM, I, S, Z>
where
    CB: FnMut(&Testcase<I>, &CorpusId) -> P,
    S: HasCorpus<I> + HasSolutions<I> + HasRand + HasMetadata,
    P: AsRef<Path>,
    Z: HasTargetBytesConverter,
    Z::Converter: ToTargetBytes<I>,
    I: Input,
{
    #[inline]
    fn perform(
        &mut self,
        fuzzer: &mut Z,
        _executor: &mut E,
        state: &mut S,
        _manager: &mut EM,
    ) -> Result<(), Error> {
        let (last_corpus, last_solution) =
            if let Some(meta) = state.metadata_map().get::<DumpToDiskMetadata>() {
                (
                    meta.last_corpus.and_then(|x| state.corpus().next(x)),
                    meta.last_solution.and_then(|x| state.solutions().next(x)),
                )
            } else {
                (state.corpus().first(), state.solutions().first())
            };

        let mut get_bytes = |tc: &mut Testcase<I>| {
            let input = tc.input().as_ref().unwrap();
            Ok(fuzzer.to_target_bytes(input).to_vec())
        };

        dump_from_corpus(
            state.corpus(),
            &self.corpus_dir,
            &mut self.generate_filename,
            &mut get_bytes,
            last_corpus,
        )?;

        dump_from_corpus(
            state.solutions(),
            &self.solutions_dir,
            &mut self.generate_filename,
            &mut get_bytes,
            last_solution,
        )?;

        state.add_metadata(DumpToDiskMetadata {
            last_corpus: state.corpus().last(),
            last_solution: state.solutions().last(),
        });

        Ok(())
    }
}

impl<EM, I, S, Z> Restartable<S>
    for DumpTargetBytesToDiskStage<fn(&Testcase<I>, &CorpusId) -> String, EM, I, S, Z>
{
    #[inline]
    fn should_restart(&mut self, _state: &mut S) -> Result<bool, Error> {
        // Not executing the target, so restart safety is not needed
        Ok(true)
    }

    #[inline]
    fn clear_progress(&mut self, _state: &mut S) -> Result<(), Error> {
        // Not executing the target, so restart safety is not needed
        Ok(())
    }
}

/// Implementation for `DumpTargetBytesToDiskStage` with a default `generate_filename` function.
impl<EM, I, S, Z> DumpTargetBytesToDiskStage<fn(&Testcase<I>, &CorpusId) -> String, EM, I, S, Z>
where
    S: HasSolutions<I> + HasRand + HasMetadata,
    I: Input,
{
    /// Create a new [`DumpTargetBytesToDiskStage`] with a default `generate_filename` function.
    pub fn new<A, B>(corpus_dir: A, solutions_dir: B) -> Result<Self, Error>
    where
        A: Into<PathBuf>,
        B: Into<PathBuf>,
    {
        Self::new_with_custom_filenames(generate_filename, corpus_dir, solutions_dir)
    }
}

impl<CB, EM, I, S, Z> DumpTargetBytesToDiskStage<CB, EM, I, S, Z>
where
    S: HasMetadata + HasSolutions<I>,
    I: Input,
{
    /// Create a new [`DumpTargetBytesToDiskStage`] with a custom `generate_filename` function.
    pub fn new_with_custom_filenames<A, B>(
        generate_filename: CB,
        corpus_dir: A,
        solutions_dir: B,
    ) -> Result<Self, Error>
    where
        A: Into<PathBuf>,
        B: Into<PathBuf>,
    {
        let (corpus_dir, solutions_dir) = create_dirs_if_needed(corpus_dir, solutions_dir)?;
        Ok(Self {
            generate_filename,
            solutions_dir,
            corpus_dir,
            phantom: PhantomData,
        })
    }
}
