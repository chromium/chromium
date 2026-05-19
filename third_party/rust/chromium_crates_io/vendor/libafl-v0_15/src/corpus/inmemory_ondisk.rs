//! The [`InMemoryOnDiskCorpus`] stores [`Testcase`]s to disk.
//!
//! Additionally, _all_ of them are kept in memory.
//! For a lower memory footprint, consider using [`crate::corpus::CachedOnDiskCorpus`]
//! which only stores a certain number of [`Testcase`]s and removes additional ones in a FIFO manner.

use alloc::string::{String, ToString};
use core::cell::{Ref, RefCell, RefMut};
use std::{
    fs,
    fs::{File, OpenOptions},
    io,
    io::{Read, Seek, SeekFrom, Write},
    path::{Path, PathBuf},
};

use fs2::FileExt;
#[cfg(feature = "gzip")]
use libafl_bolts::compress::GzipCompressor;
use serde::{Deserialize, Serialize};

use super::{
    EnableDisableCorpus, HasTestcase,
    ondisk::{OnDiskMetadata, OnDiskMetadataFormat},
};
use crate::{
    Error, HasMetadata,
    corpus::{Corpus, CorpusId, InMemoryCorpus, Testcase},
    inputs::Input,
};

/// Creates the given `path` and returns an error if it fails.
/// If the create succeeds, it will return the file.
/// If the create fails for _any_ reason, including, but not limited to, a preexisting existing file of that name,
/// it will instead return the respective [`io::Error`].
fn create_new<P: AsRef<Path>>(path: P) -> Result<File, io::Error> {
    OpenOptions::new()
        .write(true)
        .read(true)
        .create_new(true)
        .open(path)
}

/// Tries to create the given `path` and returns `None` _only_ if the file already existed.
/// If the create succeeds, it will return the file.
/// If the create fails for some other reason, it will instead return the respective [`io::Error`].
fn try_create_new<P: AsRef<Path>>(path: P) -> Result<Option<File>, io::Error> {
    match create_new(path) {
        Ok(ret) => Ok(Some(ret)),
        Err(err) if err.kind() == io::ErrorKind::AlreadyExists => Ok(None),
        Err(err) => Err(err),
    }
}

/// A corpus able to store [`Testcase`]s to disk, while also keeping all of them in memory.
///
/// Metadata is written to a `.<filename>.metadata` file in the same folder by default.
#[derive(Default, Serialize, Deserialize, Clone, Debug)]
pub struct InMemoryOnDiskCorpus<I> {
    inner: InMemoryCorpus<I>,
    dir_path: PathBuf,
    meta_format: Option<OnDiskMetadataFormat>,
    prefix: Option<String>,
    locking: bool,
}

impl<I> Corpus<I> for InMemoryOnDiskCorpus<I>
where
    I: Input,
{
    /// Returns the number of all enabled entries
    #[inline]
    fn count(&self) -> usize {
        self.inner.count()
    }

    /// Returns the number of all disabled entries
    fn count_disabled(&self) -> usize {
        self.inner.count_disabled()
    }

    /// Returns the number of elements including disabled entries
    #[inline]
    fn count_all(&self) -> usize {
        self.inner.count_all()
    }

    /// Add an enabled testcase to the corpus and return its index
    #[inline]
    fn add(&mut self, testcase: Testcase<I>) -> Result<CorpusId, Error> {
        let id = self.inner.add(testcase)?;
        let testcase = &mut self.get(id).unwrap().borrow_mut();
        self.save_testcase(testcase, Some(id))?;
        *testcase.input_mut() = None;
        Ok(id)
    }

    /// Add a disabled testcase to the corpus and return its index
    #[inline]
    fn add_disabled(&mut self, testcase: Testcase<I>) -> Result<CorpusId, Error> {
        let id = self.inner.add_disabled(testcase)?;
        let testcase = &mut self.get_from_all(id).unwrap().borrow_mut();
        self.save_testcase(testcase, Some(id))?;
        *testcase.input_mut() = None;
        Ok(id)
    }

    /// Replaces the testcase at the given idx
    #[inline]
    fn replace(&mut self, id: CorpusId, testcase: Testcase<I>) -> Result<Testcase<I>, Error> {
        let entry = self.inner.replace(id, testcase)?;
        self.remove_testcase(&entry)?;
        let testcase = &mut self.get(id).unwrap().borrow_mut();
        self.save_testcase(testcase, Some(id))?;
        *testcase.input_mut() = None;
        Ok(entry)
    }

    /// Removes an entry from the corpus, returning it if it was present; considers both enabled and disabled corpus
    #[inline]
    fn remove(&mut self, id: CorpusId) -> Result<Testcase<I>, Error> {
        let entry = self.inner.remove(id)?;
        self.remove_testcase(&entry)?;
        Ok(entry)
    }

    /// Get by id; considers only enabled testcases
    #[inline]
    fn get(&self, id: CorpusId) -> Result<&RefCell<Testcase<I>>, Error> {
        self.inner.get(id)
    }

    /// Get by id; considers both enabled and disabled testcases
    #[inline]
    fn get_from_all(&self, id: CorpusId) -> Result<&RefCell<Testcase<I>>, Error> {
        self.inner.get_from_all(id)
    }

    /// Current testcase scheduled
    #[inline]
    fn current(&self) -> &Option<CorpusId> {
        self.inner.current()
    }

    /// Current testcase scheduled (mutable)
    #[inline]
    fn current_mut(&mut self) -> &mut Option<CorpusId> {
        self.inner.current_mut()
    }

    #[inline]
    fn next(&self, id: CorpusId) -> Option<CorpusId> {
        self.inner.next(id)
    }

    /// Peek the next free corpus id
    #[inline]
    fn peek_free_id(&self) -> CorpusId {
        self.inner.peek_free_id()
    }

    #[inline]
    fn prev(&self, id: CorpusId) -> Option<CorpusId> {
        self.inner.prev(id)
    }

    #[inline]
    fn first(&self) -> Option<CorpusId> {
        self.inner.first()
    }

    #[inline]
    fn last(&self) -> Option<CorpusId> {
        self.inner.last()
    }

    /// Get the nth corpus id; considers only enabled testcases
    #[inline]
    fn nth(&self, nth: usize) -> CorpusId {
        self.inner.nth(nth)
    }
    /// Get the nth corpus id; considers both enabled and disabled testcases
    #[inline]
    fn nth_from_all(&self, nth: usize) -> CorpusId {
        self.inner.nth_from_all(nth)
    }

    fn load_input_into(&self, testcase: &mut Testcase<I>) -> Result<(), Error> {
        if testcase.input_mut().is_none() {
            let Some(file_path) = testcase.file_path().as_ref() else {
                return Err(Error::illegal_argument(
                    "No file path set for testcase. Could not load inputs.",
                ));
            };
            let input = I::from_file(file_path)?;
            testcase.set_input(input);
        }
        Ok(())
    }

    fn store_input_from(&self, testcase: &Testcase<I>) -> Result<(), Error> {
        // Store the input to disk
        let Some(file_path) = testcase.file_path() else {
            return Err(Error::illegal_argument(
                "No file path set for testcase. Could not store input to disk.",
            ));
        };
        let Some(input) = testcase.input() else {
            return Err(Error::illegal_argument(
                "No input available for testcase. Could not store anything.",
            ));
        };
        input.to_file(file_path)
    }
}

impl<I> EnableDisableCorpus for InMemoryOnDiskCorpus<I>
where
    I: Input,
{
    #[inline]
    fn disable(&mut self, id: CorpusId) -> Result<(), Error> {
        self.inner.disable(id)?;
        // Ensure testcase is saved to disk correctly with its new status
        let testcase_cell = &mut self.get_from_all(id).unwrap().borrow_mut();
        self.save_testcase(testcase_cell, Some(id))?;
        Ok(())
    }

    #[inline]
    fn enable(&mut self, id: CorpusId) -> Result<(), Error> {
        self.inner.enable(id)?;
        // Ensure testcase is saved to disk correctly with its new status
        let testcase_cell = &mut self.get_from_all(id).unwrap().borrow_mut();
        self.save_testcase(testcase_cell, Some(id))?;
        Ok(())
    }
}

impl<I> HasTestcase<I> for InMemoryOnDiskCorpus<I>
where
    I: Input,
{
    fn testcase(&self, id: CorpusId) -> Result<Ref<'_, Testcase<I>>, Error> {
        Ok(self.get(id)?.borrow())
    }

    fn testcase_mut(&self, id: CorpusId) -> Result<RefMut<'_, Testcase<I>>, Error> {
        Ok(self.get(id)?.borrow_mut())
    }
}

impl<I> InMemoryOnDiskCorpus<I> {
    /// Creates an [`InMemoryOnDiskCorpus`].
    ///
    /// This corpus stores all testcases to disk, and keeps all of them in memory, as well.
    ///
    /// By default, it stores metadata for each [`Testcase`] as prettified json.
    /// Metadata will be written to a file named `.<testcase>.metadata`
    /// The metadata may include objective reason, specific information for a fuzz job, and more.
    ///
    /// If you don't want metadata, use [`InMemoryOnDiskCorpus::no_meta`].
    /// To pick a different metadata format, use [`InMemoryOnDiskCorpus::with_meta_format`].
    ///
    /// Will error, if [`fs::create_dir_all()`] failed for `dir_path`.
    pub fn new<P>(dir_path: P) -> Result<Self, Error>
    where
        P: AsRef<Path>,
    {
        Self::_new(
            dir_path.as_ref(),
            Some(OnDiskMetadataFormat::JsonPretty),
            None,
            true,
        )
    }

    /// Creates the [`InMemoryOnDiskCorpus`] specifying the format in which `Metadata` will be saved to disk.
    ///
    /// Will error, if [`fs::create_dir_all()`] failed for `dir_path`.
    pub fn with_meta_format<P>(
        dir_path: P,
        meta_format: Option<OnDiskMetadataFormat>,
    ) -> Result<Self, Error>
    where
        P: AsRef<Path>,
    {
        Self::_new(dir_path.as_ref(), meta_format, None, true)
    }

    /// Creates the [`InMemoryOnDiskCorpus`] specifying the format in which `Metadata` will be saved to disk
    /// and the prefix for the filenames.
    ///
    /// Will error, if [`fs::create_dir_all()`] failed for `dir_path`.
    pub fn with_meta_format_and_prefix<P>(
        dir_path: P,
        meta_format: Option<OnDiskMetadataFormat>,
        prefix: Option<String>,
        locking: bool,
    ) -> Result<Self, Error>
    where
        P: AsRef<Path>,
    {
        Self::_new(dir_path.as_ref(), meta_format, prefix, locking)
    }

    /// Creates an [`InMemoryOnDiskCorpus`] that will not store .metadata files
    ///
    /// Will error, if [`fs::create_dir_all()`] failed for `dir_path`.
    pub fn no_meta<P>(dir_path: P) -> Result<Self, Error>
    where
        P: AsRef<Path>,
    {
        Self::_new(dir_path.as_ref(), None, None, true)
    }

    /// Private fn to crate a new corpus at the given (non-generic) path with the given optional `meta_format`
    fn _new(
        dir_path: &Path,
        meta_format: Option<OnDiskMetadataFormat>,
        prefix: Option<String>,
        locking: bool,
    ) -> Result<Self, Error> {
        match fs::create_dir_all(dir_path) {
            Ok(()) => {}
            Err(e) if e.kind() == io::ErrorKind::AlreadyExists => {}
            Err(e) => return Err(e.into()),
        }
        Ok(InMemoryOnDiskCorpus {
            inner: InMemoryCorpus::new(),
            dir_path: dir_path.into(),
            meta_format,
            prefix,
            locking,
        })
    }

    /// Sets the filename for a [`Testcase`].
    /// If an error gets returned from the corpus (i.e., file exists), we'll have to retry with a different filename.
    /// Renaming testcases will most likely cause duplicate testcases to not be handled correctly
    /// if testcases with the same input are not given the same filename.
    /// Only rename when you know what you are doing.
    #[inline]
    pub fn rename_testcase(
        &self,
        testcase: &mut Testcase<I>,
        filename: String,
        id: Option<CorpusId>,
    ) -> Result<(), Error>
    where
        I: Input,
    {
        if testcase.filename().is_some() {
            // We are renaming!

            let old_filename = testcase.filename_mut().take().unwrap();
            let new_filename = filename;

            // Do operations below when new filename is specified
            if old_filename == new_filename {
                *testcase.filename_mut() = Some(old_filename);
                return Ok(());
            }

            let new_file_path = self.dir_path.join(&new_filename);
            self.remove_testcase(testcase)?;
            *testcase.filename_mut() = Some(new_filename);
            self.save_testcase(testcase, id)?;
            *testcase.file_path_mut() = Some(new_file_path);

            Ok(())
        } else {
            Err(Error::illegal_argument(
                "Cannot rename testcase without name!",
            ))
        }
    }

    fn save_testcase(&self, testcase: &mut Testcase<I>, id: Option<CorpusId>) -> Result<(), Error>
    where
        I: Input,
    {
        let file_name = testcase.filename_mut().take().unwrap_or_else(|| {
            // TODO walk entry metadata to ask for pieces of filename (e.g. :havoc in AFL)
            testcase.input().as_ref().unwrap().generate_name(id)
        });

        let mut ctr = 1;
        if self.locking {
            let lockfile_name = format!(".{file_name}");
            let lockfile_path = self.dir_path.join(lockfile_name);

            let mut lockfile = try_create_new(&lockfile_path)?.unwrap_or(
                OpenOptions::new()
                    .write(true)
                    .read(true)
                    .open(&lockfile_path)?,
            );
            lockfile.lock_exclusive()?;

            let mut old_ctr = String::new();
            lockfile.read_to_string(&mut old_ctr)?;
            if !old_ctr.is_empty() {
                ctr = old_ctr.trim().parse::<u32>()? + 1;
            }

            lockfile.seek(SeekFrom::Start(0))?;
            lockfile.write_all(ctr.to_string().as_bytes())?;
        }

        if testcase.file_path().is_none() {
            *testcase.file_path_mut() = Some(self.dir_path.join(&file_name));
        }
        *testcase.filename_mut() = Some(file_name);

        if self.meta_format.is_some() {
            let metafile_name = if self.locking {
                format!(
                    ".{}_{}.metadata",
                    testcase.filename().as_ref().unwrap(),
                    ctr
                )
            } else {
                format!(".{}.metadata", testcase.filename().as_ref().unwrap())
            };
            let metafile_path = self.dir_path.join(&metafile_name);
            let mut tmpfile_path = metafile_path.clone();
            tmpfile_path.set_file_name(format!(".{metafile_name}.tmp",));

            let ondisk_meta = OnDiskMetadata {
                metadata: testcase.metadata_map(),
                exec_time: testcase.exec_time(),
                executions: testcase.executions(),
            };

            let mut tmpfile = File::create(&tmpfile_path)?;

            let json_error =
                |err| Error::serialize(format!("Failed to json-ify metadata: {err:?}"));

            let serialized = match self.meta_format.as_ref().unwrap() {
                OnDiskMetadataFormat::Postcard => postcard::to_allocvec(&ondisk_meta)?,
                OnDiskMetadataFormat::Json => {
                    serde_json::to_vec(&ondisk_meta).map_err(json_error)?
                }
                OnDiskMetadataFormat::JsonPretty => {
                    serde_json::to_vec_pretty(&ondisk_meta).map_err(json_error)?
                }
                #[cfg(feature = "gzip")]
                OnDiskMetadataFormat::JsonGzip => GzipCompressor::new()
                    .compress(&serde_json::to_vec_pretty(&ondisk_meta).map_err(json_error)?),
            };
            tmpfile.write_all(&serialized)?;
            fs::rename(&tmpfile_path, &metafile_path)?;
            *testcase.metadata_path_mut() = Some(metafile_path);
        }

        // Only try to write the data if the counter is 1.
        // Otherwise we already have a file with this name, and
        // we can assume the data has already been written.
        if ctr == 1 {
            if let Err(err) = self.store_input_from(testcase) {
                if self.locking {
                    return Err(err);
                }
                log::error!(
                    "An error occurred when trying to write a testcase without locking: {err}"
                );
            }
        }
        Ok(())
    }

    fn remove_testcase(&self, testcase: &Testcase<I>) -> Result<(), Error> {
        if let Some(filename) = testcase.filename() {
            let mut ctr = String::new();
            if self.locking {
                let lockfile_path = self.dir_path.join(format!(".{filename}"));
                let mut lockfile = OpenOptions::new()
                    .write(true)
                    .read(true)
                    .open(&lockfile_path)?;

                lockfile.lock_exclusive()?;
                lockfile.read_to_string(&mut ctr)?;
                ctr = ctr.trim().to_string();

                if ctr == "1" {
                    FileExt::unlock(&lockfile)?;
                    drop(fs::remove_file(lockfile_path));
                } else {
                    lockfile.seek(SeekFrom::Start(0))?;
                    lockfile.write_all((ctr.parse::<u32>()? - 1).to_string().as_bytes())?;
                    return Ok(());
                }
            }

            fs::remove_file(self.dir_path.join(filename))?;
            if self.meta_format.is_some() {
                if self.locking {
                    fs::remove_file(self.dir_path.join(format!(".{filename}_{ctr}.metadata")))?;
                } else {
                    fs::remove_file(self.dir_path.join(format!(".{filename}.metadata")))?;
                }
            }
        }
        Ok(())
    }

    /// Path to the corpus directory associated with this corpus
    #[must_use]
    pub fn dir_path(&self) -> &PathBuf {
        &self.dir_path
    }
}

#[cfg(test)]
mod tests {
    #[cfg(not(miri))]
    use std::{env, fs, io::Write};

    #[cfg(not(miri))]
    use super::{create_new, try_create_new};

    #[test]
    #[cfg(not(miri))]
    fn test() {
        let tmp = env::temp_dir();
        let path = tmp.join("testfile.tmp");
        _ = fs::remove_file(&path);
        let mut f = create_new(&path).unwrap();
        f.write_all(&[0; 1]).unwrap();

        match try_create_new(&path) {
            Ok(None) => (),
            Ok(_) => panic!(
                "File {} did not exist even though it should have?",
                &path.display()
            ),
            Err(e) => panic!("An unexpected error occurred: {e}"),
        }
        drop(f);
        fs::remove_file(path).unwrap();
    }
}
