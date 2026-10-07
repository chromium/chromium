//! Stores and restores state when a client needs to relaunch.
//! Uses a [`ShMem`] up to a threshold, then write to disk.
use alloc::string::{String, ToString};
use core::{
    hash::{BuildHasher, Hasher},
    marker::PhantomData,
    mem::size_of,
    ptr,
    ptr::read_volatile,
    slice,
};
use std::{
    env::temp_dir,
    fs::{self, File},
    io::{Read, Write},
    path::PathBuf,
};

use ahash::RandomState;
use libafl_core::Error;
use serde::{Serialize, de::DeserializeOwned};

use crate::shmem::{ShMem, ShMemProvider};

/// If the saved page content equals exactly this buf, the restarted child wants to exit cleanly.
const EXITING_MAGIC: &[u8; 16] = b"LIBAFL_EXIT_NOW\0";

/// The struct stored on the shared map, containing either the data, or the filename to read contents from.
#[repr(C)]
struct StateShMemContent {
    is_disk: bool,
    buf_len: usize,
    buf: [u8; 0],
}

impl StateShMemContent {
    /// Gets the (tmp-)filename, if the contents are stored on disk.
    pub fn tmpfile(&self, shmem_size: usize) -> Result<Option<PathBuf>, Error> {
        Ok(if self.is_disk {
            let bytes = unsafe {
                slice::from_raw_parts(self.buf.as_ptr(), self.buf_len_checked(shmem_size)?)
            };
            let filename = postcard::from_bytes::<String>(bytes)?;
            Some(temp_dir().join(filename))
        } else {
            None
        })
    }

    /// Get a length that's safe to deref from this map, or error.
    pub fn buf_len_checked(&self, shmem_size: usize) -> Result<usize, Error> {
        let buf_len = unsafe { read_volatile(&raw const self.buf_len) };
        if size_of::<StateShMemContent>() + buf_len > shmem_size {
            Err(Error::illegal_state(format!(
                "Stored buf_len is larger than the shared map! Shared data corrupted? Expected {shmem_size} bytes max, but got {} (buf_len {buf_len})",
                size_of::<StateShMemContent>() + buf_len
            )))
        } else {
            Ok(buf_len)
        }
    }
}

/// A [`StateRestorer`] saves and restores bytes to a shared map.
///
/// If the state gets larger than the preallocated [`ShMem`] shared map,
/// it will instead write to disk, and store the file name into the map.
/// Writing to [`StateRestorer`] multiple times is not allowed.
#[derive(Debug, Clone)]
pub struct StateRestorer<SHM, SP> {
    shmem: SHM,
    phantom: PhantomData<*const SP>,
}

impl<SHM, SP> StateRestorer<SHM, SP>
where
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    /// Get the map size backing this [`StateRestorer`].
    pub fn mapsize(&self) -> usize {
        self.shmem.len()
    }

    /// Writes this [`StateRestorer`] to env variable, to be restored later
    ///
    /// # Safety
    /// Alters the env. Should only be called from a single thread.
    pub unsafe fn write_to_env(&self, env_name: &str) -> Result<(), Error> {
        unsafe { self.shmem.write_to_env(env_name) }
    }

    /// Create a [`StateRestorer`] from `env` variable name
    pub fn from_env(shmem_provider: &mut SP, env_name: &str) -> Result<Self, Error> {
        Ok(Self {
            shmem: shmem_provider.existing_from_env(env_name)?,
            phantom: PhantomData,
        })
    }

    /// Create a new [`StateRestorer`].
    pub fn new(shmem: SHM) -> Self {
        let mut ret = Self {
            shmem,
            phantom: PhantomData,
        };
        ret.reset();
        ret
    }

    /// Saves a state to the connected [`ShMem`], or a tmpfile, if its serialized size get too large.
    pub fn save<S>(&mut self, state: &S) -> Result<(), Error>
    where
        S: Serialize,
    {
        if self.has_content() {
            return Err(Error::illegal_state(
                "Trying to save state to a non-empty state map".to_string(),
            ));
        }

        let shmem_len = self.shmem.len();
        let content_ptr = ptr::from_mut::<StateShMemContent>(self.content_mut());
        // Calculate available space: shmem.len() - sizeof(StateShMemContent)
        let available = shmem_len
            .checked_sub(size_of::<StateShMemContent>())
            .ok_or(Error::illegal_state("ShMem too small"))?;

        let buf_slice =
            unsafe { slice::from_raw_parts_mut((*content_ptr).buf.as_mut_ptr(), available) };

        // Try to serialize directly to shmem
        match postcard::to_slice(state, buf_slice) {
            Ok(bytes) => {
                // Success!
                let len = bytes.len();
                unsafe {
                    ptr::write_volatile(&raw mut (*content_ptr).buf_len, len);
                    ptr::write_volatile(&raw mut (*content_ptr).is_disk, false);
                }
                Ok(())
            }
            Err(postcard::Error::SerializeBufferFull) => {
                // Buffer full, fall back to allocating vector and file (unsafe in signal handler usually, but best effort)
                let serialized = postcard::to_allocvec(state)?;
                // generate a filename
                let mut hasher = RandomState::with_seeds(0, 0, 0, 0).build_hasher();
                // Using the last few k as randomness for a filename, hoping it's unique.
                hasher.write(&serialized[serialized.len().saturating_sub(4096)..]);

                let filename = format!("{:016x}.libafl_state", hasher.finish());
                let tmpfile = temp_dir().join(&filename);
                File::create(tmpfile)?.write_all(&serialized)?;

                // write the filename to shmem
                let filename_buf = postcard::to_allocvec(&filename)?;

                let len = filename_buf.len();
                if len > available {
                    return Err(Error::illegal_state(format!(
                        "The state restorer map is too small to fit anything, even the filename! 
                            It needs to be at least {} bytes. 
                            The tmpfile was written to {}.",
                        len,
                        temp_dir().join(&filename).display()
                    )));
                }

                unsafe {
                    ptr::copy_nonoverlapping(
                        filename_buf.as_ptr(),
                        (*content_ptr).buf.as_mut_ptr(),
                        len,
                    );
                    ptr::write_volatile(&raw mut (*content_ptr).buf_len, len);
                    ptr::write_volatile(&raw mut (*content_ptr).is_disk, true);
                }
                Ok(())
            }
            Err(e) => Err(Error::serialize(format!("Postcard internal error: {e}"))),
        }
    }

    /// Reset this [`StateRestorer`] to an empty state.
    pub fn reset(&mut self) {
        let mapsize = self.mapsize();
        let content_mut = self.content_mut();
        if let Ok(Some(tmpfile)) = content_mut.tmpfile(mapsize) {
            // Remove tmpfile and ignore result
            drop(fs::remove_file(tmpfile));
        }
        unsafe {
            ptr::write_volatile(&raw mut content_mut.is_disk, false);
            ptr::write_volatile(&raw mut content_mut.buf_len, 0);
        }
    }

    /// When called from a child, informs the restarter/parent process
    /// that it should no longer respawn the child.
    pub fn send_exiting(&mut self) {
        self.reset();

        let len = EXITING_MAGIC.len();

        assert!(size_of::<StateShMemContent>() + len <= self.shmem.len());

        let shmem_content = self.content_mut();
        // # Safety
        // `buf` is large enough to hold `[u8; 16]` and is properly aligned (u8 alignment is 1),
        // and `buf_len` points to a valid field in shared memory.
        unsafe {
            ptr::write_volatile(
                shmem_content.buf.as_mut_ptr().cast::<[u8; 16]>(),
                *EXITING_MAGIC,
            );
            ptr::write_volatile(&raw mut shmem_content.buf_len, len);
        }
    }

    /// Returns true, if [`Self::send_exiting`] was called on this [`StateRestorer`] last.
    /// This should be checked in the parent before deciding to restore the client.
    pub fn wants_to_exit(&self) -> bool {
        let len = EXITING_MAGIC.len();
        assert!(size_of::<StateShMemContent>() + len <= self.shmem.len());
        let content = self.content();
        // # Safety
        // `content.buf_len` points to a valid field in the shared memory map.
        let buf_len = unsafe { read_volatile(&raw const content.buf_len) };
        if buf_len != len {
            return false;
        }
        // # Safety
        // `content.buf` is checked above to contain at least 16 bytes and is properly aligned (u8 alignment is 1).
        let magic = unsafe { read_volatile(content.buf.as_ptr().cast::<[u8; 16]>()) };
        magic == *EXITING_MAGIC
    }

    fn content_mut(&mut self) -> &mut StateShMemContent {
        let ptr = self.shmem.as_ptr();
        debug_assert_eq!(
            ptr.align_offset(size_of::<StateShMemContent>()),
            0,
            "Beginning of the page is not aligned at {ptr:?}!"
        );
        #[expect(clippy::cast_ptr_alignment)] // Beginning of the page will always be aligned
        unsafe {
            &mut *(ptr as *mut StateShMemContent)
        }
    }

    /// The content is either the name of the tmpfile, or the serialized bytes directly, if they fit on a single page.
    fn content(&self) -> &StateShMemContent {
        #[expect(clippy::cast_ptr_alignment)] // Beginning of the page will always be aligned
        let ptr = self.shmem.as_ptr() as *const StateShMemContent;
        unsafe { &*(ptr) }
    }

    /// Returns true, if this [`StateRestorer`] has contents.
    pub fn has_content(&self) -> bool {
        let buf_len = unsafe { read_volatile(&raw const self.content().buf_len) };
        buf_len > 0
    }

    /// Restores the contents saved in this [`StateRestorer`], if any are available.
    /// Can only be read once.
    pub fn restore<S>(&self) -> Result<Option<S>, Error>
    where
        S: DeserializeOwned,
    {
        if !self.has_content() {
            return Ok(None);
        }
        let state_shmem_content = self.content();
        let bytes = unsafe {
            slice::from_raw_parts(
                state_shmem_content.buf.as_ptr(),
                state_shmem_content.buf_len_checked(self.mapsize())?,
            )
        };

        if bytes == EXITING_MAGIC {
            return Err(Error::illegal_state(
                "Trying to restore a state after send_exiting was called.",
            ));
        }

        let mut state = bytes;
        let mut file_content;
        if state_shmem_content.buf_len == 0 {
            return Ok(None);
        } else if state_shmem_content.is_disk {
            let filename: String = postcard::from_bytes(bytes)?;
            let tmpfile = temp_dir().join(&filename);
            file_content = vec![];
            File::open(&tmpfile)?.read_to_end(&mut file_content)?;
            if file_content.is_empty() {
                return Err(Error::illegal_state(format!(
                    "Could not restore state from file {filename}"
                )));
            }
            drop(fs::remove_file(&tmpfile));
            state = &file_content;
        }
        let deserialized = postcard::from_bytes(state)?;
        Ok(Some(deserialized))
    }
}

#[cfg(test)]
mod tests {

    #[cfg(not(target_os = "haiku"))]
    use serial_test::serial;

    #[test]
    #[serial]
    #[cfg_attr(miri, ignore)]
    #[cfg(not(target_os = "haiku"))]
    fn test_state_restore() {
        use alloc::{
            string::{String, ToString},
            vec::Vec,
        };

        use crate::{
            shmem::{ShMemProvider, StdShMem, StdShMemProvider},
            staterestore::StateRestorer,
        };

        const TESTMAP_SIZE: usize = 1024;

        let mut shmem_provider = StdShMemProvider::new().unwrap();
        let shmem = shmem_provider.new_shmem(TESTMAP_SIZE).unwrap();
        let mut state_restorer = StateRestorer::<StdShMem, StdShMemProvider>::new(shmem);

        let state = "hello world".to_string();

        state_restorer.save(&state).unwrap();

        assert!(state_restorer.has_content());
        let restored = state_restorer.restore::<String>().unwrap().unwrap();
        log::info!("Restored {restored}");
        assert_eq!(restored, "hello world");
        assert!(!state_restorer.content().is_disk);

        state_restorer.reset();

        assert!(!state_restorer.has_content());
        assert!(!state_restorer.content().is_disk);
        assert!(state_restorer.restore::<String>().unwrap().is_none());

        let too_large = vec![4u8; TESTMAP_SIZE + 1];
        state_restorer.save(&too_large).unwrap();
        assert!(state_restorer.has_content());

        let large_restored = state_restorer.restore::<Vec<u8>>().unwrap().unwrap();
        assert_eq!(large_restored, too_large);
        assert_eq!(large_restored.len(), too_large.len());
        assert_eq!(large_restored[TESTMAP_SIZE], 4u8);

        assert!(state_restorer.content().is_disk);
        assert_ne!(state_restorer.content().buf_len, 0);

        // Check if file removal works on restore.
        let state_shmem_content = state_restorer.content();
        let tmpfile = state_shmem_content
            .tmpfile(state_restorer.mapsize())
            .unwrap()
            .unwrap();
        assert!(!tmpfile.exists());

        state_restorer.reset();
        assert!(!state_restorer.has_content());
        assert!(!tmpfile.exists());

        // Check if file removal works on reset without restore.
        state_restorer.save(&too_large).unwrap();
        let tmpfile2 = state_restorer
            .content()
            .tmpfile(state_restorer.mapsize())
            .unwrap()
            .unwrap();
        assert!(tmpfile2.exists());
        state_restorer.reset();
        assert!(!state_restorer.has_content());
        assert!(!tmpfile2.exists());
    }

    #[test]
    #[serial]
    #[cfg_attr(miri, ignore)]
    #[cfg(not(target_os = "haiku"))]
    fn test_state_restore_exiting() {
        use crate::{
            shmem::{ShMemProvider, StdShMem, StdShMemProvider},
            staterestore::StateRestorer,
        };

        const TESTMAP_SIZE: usize = 1024;

        let mut shmem_provider = StdShMemProvider::new().unwrap();
        let shmem = shmem_provider.new_shmem(TESTMAP_SIZE).unwrap();
        let mut state_restorer = StateRestorer::<StdShMem, StdShMemProvider>::new(shmem);

        assert!(!state_restorer.wants_to_exit());

        state_restorer.send_exiting();
        assert!(state_restorer.wants_to_exit());
        assert!(state_restorer.has_content());

        state_restorer.reset();
        assert!(!state_restorer.wants_to_exit());
        assert!(!state_restorer.has_content());
    }
}
