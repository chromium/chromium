//! A generic shared memory region to be used by any functions (queues or feedbacks
//! too.)

#[cfg(feature = "alloc")]
use alloc::{rc::Rc, string::ToString, vec::Vec};
#[cfg(feature = "alloc")]
use core::{cell::RefCell, fmt, fmt::Display, mem::ManuallyDrop};
use core::{
    fmt::Debug,
    mem::size_of,
    ops::{Deref, DerefMut},
};
#[cfg(feature = "std")]
use std::env;
#[cfg(all(unix, feature = "std", not(target_os = "haiku")))]
use std::io::Read;
#[cfg(all(feature = "std", not(target_os = "haiku")))]
use std::io::Write;

use serde::{Deserialize, Serialize};
#[cfg(all(
    feature = "std",
    unix,
    not(any(target_os = "android", target_os = "haiku"))
))]
pub use unix_shmem::{MmapShMem, MmapShMemProvider};
#[cfg(all(feature = "std", unix, not(target_os = "haiku")))]
pub use unix_shmem::{UnixShMem, UnixShMemProvider};
#[cfg(all(windows, feature = "std"))]
pub use win32_shmem::{Win32ShMem, Win32ShMemProvider};

use crate::Error;
#[cfg(all(unix, feature = "std", not(target_os = "haiku")))]
use crate::os::pipes::Pipe;
#[cfg(all(feature = "std", unix, not(target_os = "haiku")))]
pub use crate::os::unix_shmem_server::{ServedShMem, ServedShMemProvider, ShMemService};

/// The standard sharedmem provider
#[cfg(all(windows, feature = "std"))]
pub type StdShMemProvider = Win32ShMemProvider;
/// The standard sharedmem
#[cfg(all(windows, feature = "std"))]
pub type StdShMem = Win32ShMem;

/// The standard sharedmem
#[cfg(all(target_os = "android", feature = "std"))]
pub type StdShMem = RcShMem<
    ServedShMem<unix_shmem::ashmem::AshmemShMem>,
    ServedShMemProvider<unix_shmem::ashmem::AshmemShMemProvider>,
>;

/// The standard sharedmem provider
#[cfg(all(target_os = "android", feature = "std"))]
pub type StdShMemProvider =
    RcShMemProvider<ServedShMemProvider<unix_shmem::ashmem::AshmemShMemProvider>>;

/// The standard sharedmem service
#[cfg(all(target_os = "android", feature = "std"))]
pub type StdShMemService = ShMemService<unix_shmem::ashmem::AshmemShMemProvider>;

/// The standard sharedmem
#[cfg(all(feature = "std", target_vendor = "apple"))]
pub type StdShMem = RcShMem<ServedShMem<MmapShMem>, ServedShMemProvider<MmapShMemProvider>>;

/// The standard sharedmem provider
#[cfg(all(feature = "std", target_vendor = "apple"))]
pub type StdShMemProvider = RcShMemProvider<ServedShMemProvider<MmapShMemProvider>>;
#[cfg(all(feature = "std", target_vendor = "apple"))]
/// The standard sharedmem service
pub type StdShMemService = ShMemService<MmapShMemProvider>;

/// The default [`ShMem`].
#[cfg(all(
    feature = "std",
    unix,
    not(any(target_os = "android", target_vendor = "apple", target_os = "haiku"))
))]
pub type StdShMem = UnixShMem;
/// The default [`ShMemProvider`] for this os.
#[cfg(all(
    feature = "std",
    unix,
    not(any(target_os = "android", target_vendor = "apple", target_os = "haiku"))
))]
pub type StdShMemProvider = UnixShMemProvider;
/// The standard sharedmem service
#[cfg(any(
    not(any(target_os = "android", target_vendor = "apple", target_os = "haiku")),
    not(feature = "std")
))]
pub type StdShMemService = DummyShMemService;

// for unix only
/// The standard served shmem provider
#[cfg(all(target_os = "android", feature = "std"))]
pub type StdServedShMemProvider =
    RcShMemProvider<ServedShMemProvider<unix_shmem::ashmem::AshmemShMemProvider>>;
/// The standard served shmem provider
#[cfg(all(feature = "std", target_vendor = "apple"))]
pub type StdServedShMemProvider = RcShMemProvider<ServedShMemProvider<MmapShMemProvider>>;
/// The standard served shmem provider
#[cfg(all(
    feature = "std",
    unix,
    not(any(target_os = "android", target_vendor = "apple", target_os = "haiku"))
))]
pub type StdServedShMemProvider = RcShMemProvider<ServedShMemProvider<MmapShMemProvider>>;

/// Description of a shared map.
/// May be used to restore the map by id.
#[derive(Debug, Copy, Clone, Serialize, Deserialize)]
pub struct ShMemDescription {
    /// Size of this map
    pub size: usize,
    /// Id of this map
    pub id: ShMemId,
}

impl ShMemDescription {
    /// Create a description from a `id_str` and a `size`.
    #[must_use]
    pub fn from_string_and_size(id_str: &str, size: usize) -> Self {
        Self {
            size,
            id: ShMemId::from_string(id_str),
        }
    }
}

/// The id describing shared memory for the current provider
///
/// An id associated with a given shared memory mapping ([`ShMem`]), which can be used to
/// establish shared-mappings between processes.
/// Id is a file descriptor if you use `MmapShMem` or `AshmemShMem`.
/// That means you have to use shmem server to access to the shmem segment from other processes in these cases.
/// On the other hand, id is a unique identifier if you use `CommonUnixShMem` or `Win32ShMem`.
/// In these two cases, you can use shmat(id) or `OpenFileMappingA`(id) to gain access to the shmem
#[derive(Debug, Copy, Clone, Serialize, Deserialize, PartialEq, Eq, Hash, Default)]
pub struct ShMemId {
    id: [u8; 20],
}

impl ShMemId {
    /// Create a new id from a fixed-size string/bytes array
    /// It should contain a valid cstring.
    #[must_use]
    pub fn from_array(array: &[u8; 20]) -> Self {
        Self { id: *array }
    }

    /// Try to create a new id from a bytes string.
    /// The slice must have a length of at least 20 bytes and contain a valid cstring.
    pub fn try_from_slice(slice: &[u8]) -> Result<Self, Error> {
        Ok(Self::from_array(&slice[0..20].try_into()?))
    }

    /// Create a new id from an int
    #[cfg(feature = "alloc")]
    #[must_use]
    pub fn from_int(val: i32) -> Self {
        Self::from_string(&val.to_string())
    }

    /// Create a new id from a string
    #[must_use]
    pub fn from_string(val: &str) -> Self {
        let mut slice: [u8; 20] = [0; 20];
        for (i, val) in val.as_bytes().iter().enumerate() {
            slice[i] = *val;
        }
        Self { id: slice }
    }

    /// Returns `true` if this `ShMemId` has an empty backing slice.
    /// If this is the case something went wrong, and this `ShMemId` may not be read from.
    #[must_use]
    pub const fn is_empty(&self) -> bool {
        self.id[0] == 0
    }

    /// Get the id as a fixed-length slice
    #[must_use]
    pub const fn as_array(&self) -> &[u8; 20] {
        &self.id
    }

    /// Returns the first null-byte in or the end of the buffer
    #[must_use]
    pub fn null_pos(&self) -> usize {
        self.id.iter().position(|&c| c == 0).unwrap()
    }

    /// Returns a `str` representation of this [`ShMemId`]
    #[cfg(feature = "alloc")]
    #[must_use]
    pub fn as_str(&self) -> &str {
        core::str::from_utf8(&self.id[..self.null_pos()]).unwrap()
    }
}

impl Deref for ShMemId {
    type Target = [u8];
    fn deref(&self) -> &[u8] {
        &self.id
    }
}

#[cfg(feature = "alloc")]
impl From<ShMemId> for i32 {
    fn from(id: ShMemId) -> i32 {
        id.as_str().parse().unwrap()
    }
}

#[cfg(feature = "alloc")]
impl Display for ShMemId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.as_str())
    }
}

/// A [`ShMem`] is an interface to shared maps.
///
/// They are the backbone of [`crate::llmp`] for inter-process communication.
/// All you need for scaling on a new target is to implement this interface, as well as the respective [`ShMemProvider`].
pub trait ShMem: Sized + Debug + Clone + DerefMut<Target = [u8]> {
    /// Get the id of this shared memory mapping
    fn id(&self) -> ShMemId;

    /// Convert to a ptr of a given type, checking the size.
    /// If the map is too small, returns `None`
    fn as_ptr_of<T: Sized>(&self) -> Option<*const T> {
        if self.len() >= size_of::<T>() {
            Some(self.as_ptr() as *const T)
        } else {
            None
        }
    }

    /// Convert to a mut ptr of a given type, checking the size.
    /// If the map is too small, returns `None`
    fn as_mut_ptr_of<T: Sized>(&mut self) -> Option<*mut T> {
        if self.len() >= size_of::<T>() {
            Some(self.as_mut_ptr() as *mut T)
        } else {
            None
        }
    }

    /// Get the description of the shared memory mapping
    fn description(&self) -> ShMemDescription {
        ShMemDescription {
            size: self.len(),
            id: self.id(),
        }
    }

    /// Write this map's config to env
    ///
    /// # Safety
    /// Writes to env variables and may only be done single-threaded.
    #[cfg(feature = "std")]
    unsafe fn write_to_env(&self, env_name: &str) -> Result<(), Error> {
        let map_size = self.len();
        let map_size_env = format!("{env_name}_SIZE");
        // TODO: Audit that the environment access only happens in single-threaded code.
        unsafe { env::set_var(env_name, self.id().to_string()) };
        // TODO: Audit that the environment access only happens in single-threaded code.
        unsafe { env::set_var(map_size_env, format!("{map_size}")) };
        Ok(())
    }
}

/// A [`ShMemProvider`] provides access to shared maps.
///
/// They are the backbone of [`crate::llmp`] for inter-process communication.
/// All you need for scaling on a new target is to implement this interface, as well as the respective [`ShMem`].
pub trait ShMemProvider: Clone + Default + Debug {
    /// The actual shared map handed out by this [`ShMemProvider`].
    type ShMem: ShMem;

    /// Create a new instance of the provider
    fn new() -> Result<Self, Error>;

    /// Create a new shared memory mapping
    fn new_shmem(&mut self, map_size: usize) -> Result<Self::ShMem, Error>;

    /// Get a mapping given its id and size
    fn shmem_from_id_and_size(&mut self, id: ShMemId, size: usize) -> Result<Self::ShMem, Error>;

    /// Create a new shared memory mapping to hold an object of the given type, and initializes it with the given value.
    fn new_on_shmem<T: Sized + 'static>(&mut self, value: T) -> Result<Self::ShMem, Error> {
        self.uninit_on_shmem::<T>().map(|mut shmem| {
            // # Safety
            // The map has been created at this point in time, and is large enough.
            // The map is fresh from the OS and, hence, the pointer should be properly aligned for any object.
            unsafe { shmem.as_mut_ptr_of::<T>().unwrap().write_volatile(value) };
            shmem
        })
    }

    /// Create a new shared memory mapping to hold an object of the given type, and initializes it with the given value.
    fn uninit_on_shmem<T: Sized + 'static>(&mut self) -> Result<Self::ShMem, Error> {
        self.new_shmem(size_of::<T>())
    }

    /// Get a mapping given a description
    fn shmem_from_description(
        &mut self,
        description: ShMemDescription,
    ) -> Result<Self::ShMem, Error> {
        self.shmem_from_id_and_size(description.id, description.size)
    }

    /// Create a new sharedmap reference from an existing `id` and `len`
    fn clone_ref(&mut self, mapping: &Self::ShMem) -> Result<Self::ShMem, Error> {
        self.shmem_from_id_and_size(mapping.id(), mapping.len())
    }

    /// Reads an existing map config from env vars, then maps it
    #[cfg(feature = "std")]
    fn existing_from_env(&mut self, env_name: &str) -> Result<Self::ShMem, Error> {
        let map_shm_str = env::var(env_name)?;
        let map_size = str::parse::<usize>(&env::var(format!("{env_name}_SIZE"))?)?;
        self.shmem_from_description(ShMemDescription::from_string_and_size(
            &map_shm_str,
            map_size,
        ))
    }

    /// This method should be called before a fork or a thread creation event, allowing the [`ShMemProvider`] to
    /// get ready for a potential reset of thread specific info, and for potential reconnects.
    /// Make sure to call [`Self::post_fork()`] after threading!
    fn pre_fork(&mut self) -> Result<(), Error> {
        // do nothing
        Ok(())
    }

    /// This method should be called after a fork or after cloning/a thread creation event, allowing the [`ShMemProvider`] to
    /// reset thread specific info, and potentially reconnect.
    /// Make sure to call [`Self::pre_fork()`] before threading!
    fn post_fork(&mut self, _is_child: bool) -> Result<(), Error> {
        // do nothing
        Ok(())
    }

    /// Release the resources associated with the given [`ShMem`]
    fn release_shmem(&mut self, _shmem: &mut Self::ShMem) {
        // do nothing
    }
}

/// An [`ShMemProvider`] that does not provide any [`ShMem`].
///
/// This is mainly for testing and type magic.
/// The resulting [`NopShMem`] is backed by a simple byte buffer to do some simple non-shared things with.
/// Calling [`NopShMemProvider::shmem_from_id_and_size`] will return new maps for the same id every time.
///
/// # Note
/// If you just want a simple shared memory implementation, use [`StdShMemProvider`] instead.
#[cfg(feature = "alloc")]
#[derive(Debug, Copy, Clone, Default)]
pub struct NopShMemProvider;

#[cfg(feature = "alloc")]
impl ShMemProvider for NopShMemProvider {
    type ShMem = NopShMem;

    fn new() -> Result<Self, Error> {
        Ok(Self)
    }

    fn new_shmem(&mut self, map_size: usize) -> Result<Self::ShMem, Error> {
        self.shmem_from_id_and_size(ShMemId::default(), map_size)
    }

    fn shmem_from_id_and_size(
        &mut self,
        id: ShMemId,
        map_size: usize,
    ) -> Result<Self::ShMem, Error> {
        Ok(NopShMem {
            id,
            buf: vec![0; map_size],
        })
    }
}

/// An [`ShMem]`] that does not have any mem nor share anything.
#[cfg(feature = "alloc")]
#[derive(Debug, Clone, Default)]
pub struct NopShMem {
    id: ShMemId,
    buf: Vec<u8>,
}

#[cfg(feature = "alloc")]
impl ShMem for NopShMem {
    fn id(&self) -> ShMemId {
        self.id
    }
}

#[cfg(feature = "alloc")]
impl DerefMut for NopShMem {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.buf
    }
}

#[cfg(feature = "alloc")]
impl Deref for NopShMem {
    type Target = [u8];

    fn deref(&self) -> &Self::Target {
        &self.buf
    }
}

/// A Handle Counted shared map,
/// that can use internal mutability.
/// Useful if the `ShMemProvider` needs to keep local state.
#[cfg(feature = "alloc")]
#[derive(Debug, Clone, Default)]
pub struct RcShMem<SHM, SP>
where
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    internal: ManuallyDrop<SHM>,
    provider: Rc<RefCell<SP>>,
}

#[cfg(feature = "alloc")]
impl<SP> ShMem for RcShMem<SP::ShMem, SP>
where
    SP: ShMemProvider,
{
    fn id(&self) -> ShMemId {
        self.internal.id()
    }
}

#[cfg(feature = "alloc")]
impl<SHM, SP> Deref for RcShMem<SHM, SP>
where
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    type Target = [u8];

    fn deref(&self) -> &[u8] {
        &self.internal
    }
}

#[cfg(feature = "alloc")]
impl<SHM, SP> DerefMut for RcShMem<SHM, SP>
where
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    fn deref_mut(&mut self) -> &mut [u8] {
        &mut self.internal
    }
}

#[cfg(feature = "alloc")]
impl<SHM, SP> Drop for RcShMem<SHM, SP>
where
    SHM: ShMem,
    SP: ShMemProvider<ShMem = SHM>,
{
    fn drop(&mut self) {
        self.provider.borrow_mut().release_shmem(&mut self.internal);
    }
}

/// A Handle Counted `ShMemProvider`,
/// that can use internal mutability.
/// Useful if the `ShMemProvider` needs to keep local state.
#[derive(Debug, Clone)]
#[cfg(all(unix, feature = "std", not(target_os = "haiku")))]
pub struct RcShMemProvider<SP> {
    /// The wrapped [`ShMemProvider`].
    internal: Rc<RefCell<SP>>,
    /// A pipe the child uses to communicate progress to the parent after fork.
    /// This prevents a potential race condition when using the [`ShMemService`].
    #[cfg(unix)]
    child_parent_pipe: Option<Pipe>,
    #[cfg(unix)]
    /// A pipe the parent uses to communicate progress to the child after fork.
    /// This prevents a potential race condition when using the [`ShMemService`].
    parent_child_pipe: Option<Pipe>,
}

#[cfg(all(unix, feature = "std", not(target_os = "haiku")))]
impl<SP> ShMemProvider for RcShMemProvider<SP>
where
    SP: ShMemProvider + Debug,
{
    type ShMem = RcShMem<SP::ShMem, SP>;

    fn new() -> Result<Self, Error> {
        Ok(Self {
            internal: Rc::new(RefCell::new(SP::new()?)),
            child_parent_pipe: None,
            parent_child_pipe: None,
        })
    }

    fn new_shmem(&mut self, map_size: usize) -> Result<Self::ShMem, Error> {
        Ok(Self::ShMem {
            internal: ManuallyDrop::new(self.internal.borrow_mut().new_shmem(map_size)?),
            provider: self.internal.clone(),
        })
    }

    fn shmem_from_id_and_size(&mut self, id: ShMemId, size: usize) -> Result<Self::ShMem, Error> {
        Ok(Self::ShMem {
            internal: ManuallyDrop::new(
                self.internal
                    .borrow_mut()
                    .shmem_from_id_and_size(id, size)?,
            ),
            provider: self.internal.clone(),
        })
    }

    fn release_shmem(&mut self, map: &mut Self::ShMem) {
        self.internal.borrow_mut().release_shmem(&mut map.internal);
    }

    fn clone_ref(&mut self, mapping: &Self::ShMem) -> Result<Self::ShMem, Error> {
        Ok(Self::ShMem {
            internal: ManuallyDrop::new(self.internal.borrow_mut().clone_ref(&mapping.internal)?),
            provider: self.internal.clone(),
        })
    }

    /// This method should be called before a fork or a thread creation event, allowing the [`ShMemProvider`] to
    /// get ready for a potential reset of thread specific info, and for potential reconnects.
    fn pre_fork(&mut self) -> Result<(), Error> {
        // Set up the pipes to communicate progress over, later.
        self.child_parent_pipe = Some(Pipe::new()?);
        self.parent_child_pipe = Some(Pipe::new()?);
        self.internal.borrow_mut().pre_fork()
    }

    /// After fork, make sure everything gets set up correctly internally.
    fn post_fork(&mut self, is_child: bool) -> Result<(), Error> {
        if is_child {
            self.await_parent_done()?;
            //let child_shmem = self.internal.borrow_mut().clone();
            //self.internal = Rc::new(RefCell::new(child_shmem));
        }
        self.internal.borrow_mut().post_fork(is_child)?;
        if is_child {
            self.set_child_done()?;
        } else {
            self.set_parent_done()?;
            self.await_child_done()?;
        }

        self.parent_child_pipe = None;
        self.child_parent_pipe = None;
        Ok(())
    }
}

#[cfg(all(unix, feature = "std", not(target_os = "haiku")))]
impl<SP> RcShMemProvider<SP> {
    /// "set" the "latch"
    /// (we abuse `pipes` as `semaphores`, as they don't need an additional shared mem region.)
    fn pipe_set(pipe: &mut Option<Pipe>) -> Result<(), Error> {
        match pipe {
            Some(pipe) => {
                let ok = [0_u8; 4];
                pipe.write_all(&ok)?;
                Ok(())
            }
            None => Err(Error::illegal_state(
                "Unexpected `None` Pipe in RcShMemProvider! Missing post_fork()?".to_string(),
            )),
        }
    }

    /// "await" the "latch"
    fn pipe_await(pipe: &mut Option<Pipe>) -> Result<(), Error> {
        match pipe {
            Some(pipe) => {
                let ok = [0_u8; 4];
                let mut ret = ok;
                pipe.read_exact(&mut ret)?;
                if ret == ok {
                    Ok(())
                } else {
                    Err(Error::unknown(format!(
                        "Wrong result read from pipe! Expected 0, got {ret:?}"
                    )))
                }
            }
            None => Err(Error::illegal_state(
                "Unexpected `None` Pipe in RcShMemProvider! Missing post_fork()?".to_string(),
            )),
        }
    }

    /// After fork, wait for the parent to write to our pipe :)
    fn await_parent_done(&mut self) -> Result<(), Error> {
        Self::pipe_await(&mut self.parent_child_pipe)
    }

    /// After fork, inform the new child we're done
    fn set_parent_done(&mut self) -> Result<(), Error> {
        Self::pipe_set(&mut self.parent_child_pipe)
    }

    /// After fork, wait for the child to write to our pipe :)
    fn await_child_done(&mut self) -> Result<(), Error> {
        Self::pipe_await(&mut self.child_parent_pipe)
    }

    /// After fork, inform the new child we're done
    fn set_child_done(&mut self) -> Result<(), Error> {
        Self::pipe_set(&mut self.child_parent_pipe)
    }
}

#[cfg(all(unix, feature = "std", not(target_os = "haiku")))]
impl<SP> Default for RcShMemProvider<SP>
where
    SP: ShMemProvider,
{
    fn default() -> Self {
        Self::new().unwrap()
    }
}

#[cfg(all(unix, feature = "std", not(target_os = "haiku")))]
impl<SP> RcShMemProvider<ServedShMemProvider<SP>> {
    /// Forward to `ServedShMemProvider::on_restart`
    pub fn on_restart(&mut self) {
        self.internal.borrow_mut().on_restart();
    }
}

/// A Unix sharedmem implementation.
///
/// On Android, this is partially reused to wrap `AshmemShMem`,
/// Although for an [`ServedShMemProvider`] using a unix domain socket
/// Is needed on top.
#[cfg(all(unix, feature = "std", not(target_os = "haiku")))]
pub mod unix_shmem {
    /// Mmap [`ShMem`] for Unix
    #[cfg(not(target_os = "android"))]
    pub use default::{MAX_MMAP_FILENAME_LEN, MmapShMem, MmapShMemProvider};

    #[cfg(doc)]
    use crate::shmem::{ShMem, ShMemProvider};

    /// Shared memory provider for Android, allocating and forwarding maps over unix domain sockets.
    #[cfg(target_os = "android")]
    pub type UnixShMemProvider = ashmem::AshmemShMemProvider;
    /// Shared memory for Android
    #[cfg(target_os = "android")]
    pub type UnixShMem = ashmem::AshmemShMem;
    /// Shared memory Provider for Unix
    #[cfg(not(target_os = "android"))]
    pub type UnixShMemProvider = default::CommonUnixShMemProvider;
    /// Shared memory for Unix
    #[cfg(not(target_os = "android"))]
    pub type UnixShMem = default::CommonUnixShMem;

    #[cfg(all(unix, feature = "std", not(target_os = "android")))]
    mod default {
        use alloc::string::ToString;
        use core::{
            ops::{Deref, DerefMut},
            ptr, slice,
        };
        use std::{io, path::Path, process};

        use libc::{
            c_int, c_uchar, close, fcntl, ftruncate, mmap, munmap, shm_open, shm_unlink, shmat,
            shmctl, shmdt, shmget,
        };

        use crate::{
            Error,
            rands::{Rand, StdRand},
            shmem::{ShMem, ShMemId, ShMemProvider},
        };

        /// The max number of bytes used when generating names for [`MmapShMem`]s.
        pub const MAX_MMAP_FILENAME_LEN: usize = 20;

        /// Mmap-based The sharedmap impl for unix using [`shm_open`] and [`mmap`].
        /// Default on `MacOS` and `iOS`, where we need a central point to unmap
        /// shared mem segments for dubious Mach kernel reasons.
        #[derive(Debug, Clone)]
        pub struct MmapShMem {
            /// The path of this shared memory segment.
            /// None in case we didn't [`shm_open`] this ourselves, but someone sent us the FD.
            filename_path: Option<[u8; MAX_MMAP_FILENAME_LEN]>,
            /// The size of this map
            map_size: usize,
            /// The map ptr
            map: *mut u8,
            /// The shmem id, containing the file descriptor and size, to send over the wire
            id: ShMemId,
            /// The file descriptor of the shmem
            shm_fd: c_int,
        }

        unsafe impl Send for MmapShMem {}

        impl MmapShMem {
            /// Create a new [`MmapShMem`]
            ///
            /// At most [`MAX_MMAP_FILENAME_LEN`] - 2 bytes from filename will be used.
            ///
            /// This will *NOT* automatically delete the shmem files, meaning that it's user's responsibility to delete them after fuzzing
            pub fn new(
                map_size: usize,
                filename: impl AsRef<Path>,
                use_fd_as_id: bool,
            ) -> Result<Self, Error> {
                let filename_bytes = filename.as_ref().as_os_str().as_encoded_bytes();

                let mut filename_path: [u8; 20] = [0_u8; MAX_MMAP_FILENAME_LEN];
                // Keep room for the leading slash and trailing NULL.
                let max_copy = usize::min(filename_bytes.len(), MAX_MMAP_FILENAME_LEN - 2);
                filename_path[0] = b'/';
                filename_path[1..=max_copy].copy_from_slice(&filename_bytes[..max_copy]);

                log::info!(
                    "{} Creating shmem {} {:?}",
                    map_size,
                    process::id(),
                    filename_path
                );

                // # Safety
                // No user-provided potentially unsafe parameters.
                // FFI Calls.
                unsafe {
                    /* create the shared memory segment as if it was a file */
                    let shm_fd = shm_open(
                        filename_path.as_ptr() as *const _,
                        libc::O_CREAT | libc::O_RDWR | libc::O_EXCL,
                        0o600,
                    );

                    if shm_fd == -1 {
                        return Err(Error::last_os_error(format!(
                            "Failed to shm_open map with id {filename_path:?}",
                        )));
                    }

                    /* configure the size of the shared memory segment */
                    if ftruncate(shm_fd, map_size.try_into()?) != 0 {
                        shm_unlink(filename_path.as_ptr() as *const _);
                        return Err(Error::last_os_error(format!(
                            "setup_shm(): ftruncate() failed for map with id {filename_path:?}",
                        )));
                    }

                    /* map the shared memory segment to the address space of the process */
                    let map = mmap(
                        ptr::null_mut(),
                        map_size,
                        libc::PROT_READ | libc::PROT_WRITE,
                        libc::MAP_SHARED,
                        shm_fd,
                        0,
                    );
                    if ptr::addr_eq(map, libc::MAP_FAILED) {
                        close(shm_fd);
                        shm_unlink(filename_path.as_ptr() as *const _);
                        return Err(Error::last_os_error(format!(
                            "mmap() failed for map with id {filename_path:?}",
                        )));
                    }

                    let id = if use_fd_as_id {
                        ShMemId::from_string(&format!("{shm_fd}"))
                    } else {
                        ShMemId::from_string(core::str::from_utf8(&filename_path)?)
                    };

                    Ok(Self {
                        filename_path: Some(filename_path),
                        map: map as *mut u8,
                        map_size,
                        shm_fd,
                        id,
                    })
                }
            }

            #[allow(clippy::unnecessary_wraps)] // cfg dependent
            fn shmem_from_id_and_size(
                id: ShMemId,
                map_size: usize,
                use_fd_as_id: bool,
            ) -> Result<Self, Error> {
                // # Safety
                // No user-provided potentially unsafe parameters.
                // FFI Calls.
                unsafe {
                    /* map the shared memory segment to the address space of the process */
                    let (map, shm_fd) = if use_fd_as_id {
                        let shm_fd: i32 = id.to_string().parse().unwrap();
                        let map = mmap(
                            ptr::null_mut(),
                            map_size,
                            libc::PROT_READ | libc::PROT_WRITE,
                            libc::MAP_SHARED,
                            shm_fd,
                            0,
                        );
                        (map, shm_fd)
                    } else {
                        let mut filename_path = [0_u8; MAX_MMAP_FILENAME_LEN];
                        filename_path.copy_from_slice(&id.id);

                        /* attach to the shared memory segment as if it was a file */
                        let shm_fd =
                            shm_open(filename_path.as_ptr() as *const _, libc::O_RDWR, 0o600);
                        if shm_fd == -1 {
                            log::info!(
                                "Trying to attach to {:#?} but failed {}",
                                filename_path,
                                process::id()
                            );
                            return Err(Error::last_os_error(format!(
                                "Failed to shm_open map with id {filename_path:?}",
                            )));
                        }
                        /* map the shared memory segment to the address space of the process */
                        let map = mmap(
                            ptr::null_mut(),
                            map_size,
                            libc::PROT_READ | libc::PROT_WRITE,
                            libc::MAP_SHARED,
                            shm_fd,
                            0,
                        );
                        if ptr::addr_eq(map, libc::MAP_FAILED) {
                            close(shm_fd);
                            return Err(Error::last_os_error(format!(
                                "mmap() failed for map with fd {shm_fd:?}"
                            )));
                        }
                        (map, shm_fd)
                    };

                    Ok(Self {
                        filename_path: None,
                        map: map as *mut u8,
                        map_size,
                        shm_fd,
                        id,
                    })
                }
            }

            /// Get `filename_path`
            #[must_use]
            pub fn filename_path(&self) -> &Option<[u8; MAX_MMAP_FILENAME_LEN]> {
                &self.filename_path
            }

            /// Makes a shared memory mapping available in other processes.
            ///
            /// Only available on UNIX systems at the moment.
            ///
            /// You likely want to pass the [`crate::shmem::ShMemDescription`] of the returned [`ShMem`]
            /// and reopen the shared memory in the child process using [`crate::shmem::ShMemProvider::shmem_from_description`].
            ///
            /// # Errors
            ///
            /// This function will return an error if the appropriate flags could not be extracted or set.
            #[cfg(any(unix, doc))]
            pub fn persist(self) -> Result<Self, Error> {
                let fd = self.shm_fd;

                // # Safety
                // No user-provided potentially unsafe parameters.
                // FFI Calls.
                unsafe {
                    let flags = fcntl(fd, libc::F_GETFD);

                    if flags == -1 {
                        return Err(Error::os_error(
                            io::Error::last_os_error(),
                            "Failed to retrieve FD flags",
                        ));
                    }

                    if fcntl(fd, libc::F_SETFD, flags & !libc::FD_CLOEXEC) == -1 {
                        return Err(Error::os_error(
                            io::Error::last_os_error(),
                            "Failed to set FD flags",
                        ));
                    }
                }
                Ok(self)
            }
        }

        impl ShMem for MmapShMem {
            fn id(&self) -> ShMemId {
                self.id
            }
        }

        impl Deref for MmapShMem {
            type Target = [u8];

            fn deref(&self) -> &[u8] {
                // # Safety
                // No user-provided potentially unsafe parameters.
                unsafe { slice::from_raw_parts(self.map, self.map_size) }
            }
        }

        impl DerefMut for MmapShMem {
            fn deref_mut(&mut self) -> &mut [u8] {
                // # Safety
                // No user-provided potentially unsafe parameters.
                unsafe { slice::from_raw_parts_mut(self.map, self.map_size) }
            }
        }

        impl Drop for MmapShMem {
            fn drop(&mut self) {
                // # Safety
                // No user-provided potentially unsafe parameters.
                // Mutable borrow so no possible race.
                unsafe {
                    assert!(
                        !self.map.is_null(),
                        "Map should never be null for MmapShMem (on Drop)"
                    );

                    munmap(self.map as *mut _, self.map_size);
                    self.map = ptr::null_mut();

                    assert!(
                        self.shm_fd != -1,
                        "FD should never be -1 for MmapShMem (on Drop)"
                    );

                    // None in case we didn't [`shm_open`] this ourselves, but someone sent us the FD.
                    // log::info!("Dropping {:#?}", self.filename_path);
                    // if let Some(filename_path) = self.filename_path {
                    // shm_unlink(filename_path.as_ptr() as *const _);
                    // }
                    // We cannot shm_unlink here!
                    // unlike unix common shmem we don't have refcounter.
                    // so there's no guarantee that there's no other process still using it.
                }
            }
        }

        /// A [`ShMemProvider`] which uses [`shm_open`] and [`mmap`] to provide shared memory mappings.
        #[cfg(unix)]
        #[derive(Debug, Clone)]
        pub struct MmapShMemProvider {
            /// True if should use the FD as an id (good when sending FD over a socket, otherwise use the filename)
            use_fd_as_id: bool,
        }

        impl MmapShMemProvider {
            /// Create a [`MmapShMem`] with the specified size and id.
            ///
            /// At most [`MAX_MMAP_FILENAME_LEN`] - 2 bytes from id will be used.
            #[cfg(any(unix, doc))]
            pub fn new_shmem_with_id(
                &mut self,
                map_size: usize,
                id: impl AsRef<Path>,
            ) -> Result<MmapShMem, Error> {
                MmapShMem::new(map_size, id, self.use_fd_as_id)
            }

            /// Create a new [`MmapShMemProvider`] where filename is used as the shmem id.
            #[must_use]
            pub fn with_filename_as_id() -> Self {
                Self {
                    use_fd_as_id: false,
                }
            }
        }

        unsafe impl Send for MmapShMemProvider {}

        #[cfg(unix)]
        impl Default for MmapShMemProvider {
            fn default() -> Self {
                Self::new().unwrap()
            }
        }

        /// Implement [`ShMemProvider`] for [`MmapShMemProvider`].
        #[cfg(unix)]
        impl ShMemProvider for MmapShMemProvider {
            type ShMem = MmapShMem;

            fn new() -> Result<Self, Error> {
                // Apple uses it for served shmem provider, which uses fd
                // Others will just use filename
                let use_fd_as_id = cfg!(target_vendor = "apple");
                Ok(Self { use_fd_as_id })
            }

            fn new_shmem(&mut self, map_size: usize) -> Result<Self::ShMem, Error> {
                let mut rand = StdRand::with_seed(crate::rands::random_seed());
                let id = rand.next() as u32;
                let mut full_file_name = format!("libafl_{}_{}", process::id(), id);
                // leave one byte space for the null byte.
                full_file_name.truncate(MAX_MMAP_FILENAME_LEN - 1);
                MmapShMem::new(map_size, full_file_name, self.use_fd_as_id)
            }

            fn shmem_from_id_and_size(
                &mut self,
                id: ShMemId,
                size: usize,
            ) -> Result<Self::ShMem, Error> {
                MmapShMem::shmem_from_id_and_size(id, size, self.use_fd_as_id)
            }

            fn release_shmem(&mut self, shmem: &mut Self::ShMem) {
                unsafe { close(shmem.shm_fd) };
            }
        }

        /// The default sharedmap impl for unix using shmctl & shmget
        #[derive(Debug, Clone)]
        pub struct CommonUnixShMem {
            id: ShMemId,
            map: *mut u8,
            map_size: usize,
        }

        unsafe impl Send for CommonUnixShMem {}

        impl CommonUnixShMem {
            /// Create a new shared memory mapping, using shmget/shmat
            pub fn new(map_size: usize) -> Result<Self, Error> {
                #[cfg(any(target_os = "solaris", target_os = "illumos"))]
                const SHM_R: c_int = 0o400;
                #[cfg(not(any(target_os = "solaris", target_os = "illumos")))]
                const SHM_R: c_int = libc::SHM_R;
                #[cfg(any(target_os = "solaris", target_os = "illumos"))]
                const SHM_W: c_int = 0o200;
                #[cfg(not(any(target_os = "solaris", target_os = "illumos")))]
                const SHM_W: c_int = libc::SHM_W;

                unsafe {
                    let os_id = shmget(
                        libc::IPC_PRIVATE,
                        map_size,
                        libc::IPC_CREAT | libc::IPC_EXCL | SHM_R | SHM_W,
                    );

                    if os_id < 0_i32 {
                        return Err(Error::unknown(format!(
                            "Failed to allocate a shared mapping of size {map_size} - check OS limits (i.e shmall, shmmax)"
                        )));
                    }

                    let map = shmat(os_id, ptr::null(), 0) as *mut c_uchar;

                    if map as c_int == -1 || map.is_null() {
                        return Err(Error::last_os_error("Failed to map the shared mapping"));
                    }

                    Ok(Self {
                        id: ShMemId::from_int(os_id),
                        map,
                        map_size,
                    })
                }
            }

            /// Get a [`UnixShMem`] of the existing shared memory mapping identified by id
            pub fn shmem_from_id_and_size(id: ShMemId, map_size: usize) -> Result<Self, Error> {
                unsafe {
                    let id_int: i32 = id.into();
                    let map = shmat(id_int, ptr::null(), 0) as *mut c_uchar;

                    if ptr::addr_eq(map, ptr::null_mut::<c_uchar>().wrapping_sub(1)) {
                        return Err(Error::last_os_error(format!(
                            "Failed to map the shared mapping with id {id_int}"
                        )));
                    }

                    Ok(Self { id, map, map_size })
                }
            }
        }

        #[cfg(unix)]
        impl ShMem for CommonUnixShMem {
            fn id(&self) -> ShMemId {
                self.id
            }
        }

        impl Deref for CommonUnixShMem {
            type Target = [u8];

            fn deref(&self) -> &[u8] {
                unsafe { slice::from_raw_parts(self.map, self.map_size) }
            }
        }

        impl DerefMut for CommonUnixShMem {
            fn deref_mut(&mut self) -> &mut [u8] {
                unsafe { slice::from_raw_parts_mut(self.map, self.map_size) }
            }
        }

        /// [`Drop`] implementation for [`UnixShMem`], which detaches the memory and cleans up the mapping.
        #[cfg(unix)]
        impl Drop for CommonUnixShMem {
            fn drop(&mut self) {
                unsafe {
                    let id_int: i32 = self.id.into();
                    shmctl(id_int, libc::IPC_RMID, ptr::null_mut());

                    shmdt(self.map as *mut _);
                }
            }
        }

        /// A [`ShMemProvider`] which uses `shmget`/`shmat`/`shmctl` to provide shared memory mappings.
        #[cfg(unix)]
        #[derive(Debug, Clone)]
        pub struct CommonUnixShMemProvider {}

        unsafe impl Send for CommonUnixShMemProvider {}

        #[cfg(unix)]
        impl Default for CommonUnixShMemProvider {
            fn default() -> Self {
                Self::new().unwrap()
            }
        }

        /// Implement [`ShMemProvider`] for [`UnixShMemProvider`].
        #[cfg(unix)]
        impl ShMemProvider for CommonUnixShMemProvider {
            type ShMem = CommonUnixShMem;

            fn new() -> Result<Self, Error> {
                Ok(Self {})
            }
            fn new_shmem(&mut self, map_size: usize) -> Result<Self::ShMem, Error> {
                CommonUnixShMem::new(map_size)
            }

            fn shmem_from_id_and_size(
                &mut self,
                id: ShMemId,
                size: usize,
            ) -> Result<Self::ShMem, Error> {
                CommonUnixShMem::shmem_from_id_and_size(id, size)
            }
        }
    }

    /// Module containing `ashmem` shared memory support, commonly used on Android.
    #[cfg(all(any(target_os = "linux", target_os = "android"), feature = "std"))]
    pub mod ashmem {
        use alloc::{ffi::CString, string::ToString};
        use core::{
            ops::{Deref, DerefMut},
            ptr, slice,
        };

        use libc::{
            MAP_SHARED, O_RDWR, PROT_READ, PROT_WRITE, c_uint, c_ulong, c_void, close, ioctl, mmap,
            open,
        };

        use crate::{
            Error,
            shmem::{ShMem, ShMemId, ShMemProvider},
        };

        /// An ashmem based impl for linux/android
        #[derive(Debug, Clone)]
        pub struct AshmemShMem {
            id: ShMemId,
            map: *mut u8,
            map_size: usize,
        }

        unsafe impl Send for AshmemShMem {}

        #[allow(non_camel_case_types)] // expect somehow breaks here
        #[derive(Copy, Clone)]
        #[repr(C)]
        struct ashmem_pin {
            pub offset: c_uint,
            pub len: c_uint,
        }

        const ASHMEM_GET_SIZE: c_ulong = 0x00007704;
        const ASHMEM_UNPIN: c_ulong = 0x40087708;
        //const ASHMEM_SET_NAME: c_long = 0x41007701;
        const ASHMEM_SET_SIZE: c_ulong = 0x40087703;

        impl AshmemShMem {
            /// Create a new shared memory mapping, using shmget/shmat
            pub fn new(map_size: usize) -> Result<Self, Error> {
                unsafe {
                    let device_path = CString::new(
                        if let Ok(boot_id) =
                            std::fs::read_to_string("/proc/sys/kernel/random/boot_id")
                        {
                            let path_str = format!("/dev/ashmem{boot_id}").trim().to_string();
                            if std::path::Path::new(&path_str).exists() {
                                path_str
                            } else {
                                "/dev/ashmem".to_string()
                            }
                        } else {
                            "/dev/ashmem".to_string()
                        },
                    )
                    .unwrap();

                    let fd = open(device_path.as_ptr(), O_RDWR);
                    if fd == -1 {
                        return Err(Error::unknown(format!(
                            "Failed to open the ashmem device at {device_path:?}"
                        )));
                    }

                    //if ioctl(fd, ASHMEM_SET_NAME, name) != 0 {
                    //close(fd);
                    //return Err(Error::unknown("Failed to set the ashmem mapping's name".to_string()));
                    //};

                    #[allow(trivial_numeric_casts)]
                    if ioctl(fd, ASHMEM_SET_SIZE as _, map_size) != 0 {
                        close(fd);
                        return Err(Error::unknown(
                            "Failed to set the ashmem mapping's size".to_string(),
                        ));
                    }

                    let map = mmap(
                        ptr::null_mut(),
                        map_size,
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED,
                        fd,
                        0,
                    );
                    if ptr::addr_eq(map, usize::MAX as *mut c_void) {
                        close(fd);
                        return Err(Error::unknown(
                            "Failed to map the ashmem mapping".to_string(),
                        ));
                    }

                    Ok(Self {
                        id: ShMemId::from_string(&format!("{fd}")),
                        map: map as *mut u8,
                        map_size,
                    })
                }
            }

            /// Get a [`crate::shmem::unix_shmem::UnixShMem`] of the existing [`ShMem`] mapping identified by id.
            pub fn shmem_from_id_and_size(id: ShMemId, map_size: usize) -> Result<Self, Error> {
                unsafe {
                    let fd: i32 = id.to_string().parse().unwrap();
                    #[allow(trivial_numeric_casts)]
                    #[expect(clippy::cast_sign_loss)]
                    if ioctl(fd, ASHMEM_GET_SIZE as _) as u32 as usize != map_size {
                        return Err(Error::unknown(
                            "The mapping's size differs from the requested size".to_string(),
                        ));
                    }

                    let map = mmap(
                        ptr::null_mut(),
                        map_size,
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED,
                        fd,
                        0,
                    );
                    if ptr::addr_eq(map, usize::MAX as *mut c_void) {
                        close(fd);
                        return Err(Error::unknown(
                            "Failed to map the ashmem mapping".to_string(),
                        ));
                    }

                    Ok(Self {
                        id,
                        map: map as *mut u8,
                        map_size,
                    })
                }
            }
        }

        impl ShMem for AshmemShMem {
            fn id(&self) -> ShMemId {
                self.id
            }
        }

        impl Deref for AshmemShMem {
            type Target = [u8];

            fn deref(&self) -> &[u8] {
                unsafe { slice::from_raw_parts(self.map, self.map_size) }
            }
        }

        impl DerefMut for AshmemShMem {
            fn deref_mut(&mut self) -> &mut [u8] {
                unsafe { slice::from_raw_parts_mut(self.map, self.map_size) }
            }
        }

        /// [`Drop`] implementation for [`AshmemShMem`], which cleans up the mapping.
        impl Drop for AshmemShMem {
            #[allow(trivial_numeric_casts)]
            fn drop(&mut self) {
                unsafe {
                    let fd: i32 = self.id.to_string().parse().unwrap();

                    #[allow(trivial_numeric_casts)]
                    #[expect(clippy::cast_sign_loss)]
                    let length = ioctl(fd, ASHMEM_GET_SIZE as _) as u32;

                    let ap = ashmem_pin {
                        offset: 0,
                        len: length,
                    };

                    ioctl(fd, ASHMEM_UNPIN as _, &ap);
                    close(fd);
                }
            }
        }

        /// A [`ShMemProvider`] which uses ashmem to provide shared memory mappings.
        #[derive(Debug, Clone)]
        pub struct AshmemShMemProvider {}

        unsafe impl Send for AshmemShMemProvider {}

        impl Default for AshmemShMemProvider {
            fn default() -> Self {
                Self::new().unwrap()
            }
        }

        /// Implement [`ShMemProvider`] for [`AshmemShMemProvider`], for the Android `ShMem`.
        impl ShMemProvider for AshmemShMemProvider {
            type ShMem = AshmemShMem;

            fn new() -> Result<Self, Error> {
                Ok(Self {})
            }

            fn new_shmem(&mut self, map_size: usize) -> Result<Self::ShMem, Error> {
                let mapping = AshmemShMem::new(map_size)?;
                Ok(mapping)
            }

            fn shmem_from_id_and_size(
                &mut self,
                id: ShMemId,
                size: usize,
            ) -> Result<Self::ShMem, Error> {
                AshmemShMem::shmem_from_id_and_size(id, size)
            }
        }
    }

    /// Module containing `memfd` shared memory support, usable on Linux and Android.
    #[cfg(all(
        unix,
        feature = "std",
        any(target_os = "linux", target_os = "android", target_os = "freebsd")
    ))]
    pub mod memfd {
        use alloc::{ffi::CString, string::ToString};
        use core::{
            ops::{Deref, DerefMut},
            ptr, slice,
        };
        use std::{fs::File, os::fd::IntoRawFd};

        use libc::{MAP_SHARED, PROT_READ, PROT_WRITE, close, fstat, ftruncate, mmap, munmap};
        use nix::sys::memfd::{MFdFlags, memfd_create};

        use crate::{
            Error,
            shmem::{ShMem, ShMemId, ShMemProvider},
        };

        /// An memfd based impl for linux/android
        #[cfg(unix)]
        #[derive(Debug, Clone)]
        pub struct MemfdShMem {
            id: ShMemId,
            map: *mut u8,
            map_size: usize,
        }

        unsafe impl Send for MemfdShMem {}

        impl MemfdShMem {
            /// Create a new shared memory mapping, using shmget/shmat
            pub fn new(map_size: usize) -> Result<Self, Error> {
                unsafe {
                    let c_str = CString::new("LibAFL").unwrap();
                    let Ok(fd) = memfd_create(c_str.as_c_str(), MFdFlags::empty()) else {
                        return Err(Error::last_os_error("Failed to create memfd".to_string()));
                    };
                    let fd = fd.into_raw_fd();

                    #[expect(clippy::cast_possible_wrap)]
                    if ftruncate(fd, map_size as libc::off_t) == -1 {
                        close(fd);
                        return Err(Error::last_os_error(format!(
                            "Failed to ftruncate memfd to {map_size}"
                        )));
                    }
                    let map = mmap(
                        ptr::null_mut(),
                        map_size,
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED,
                        fd,
                        0,
                    );
                    if ptr::addr_eq(map, libc::MAP_FAILED) {
                        close(fd);
                        return Err(Error::unknown(
                            "Failed to map the memfd mapping".to_string(),
                        ));
                    }
                    Ok(Self {
                        id: ShMemId::from_int(fd),
                        map: map as *mut u8,
                        map_size,
                    })
                }
            }

            fn shmem_from_id_and_size(id: ShMemId, map_size: usize) -> Result<Self, Error> {
                let fd = i32::from(id);
                unsafe {
                    let mut stat = core::mem::zeroed();
                    if fstat(fd, &raw mut stat) == -1 {
                        return Err(Error::unknown(
                            "Failed to map the memfd mapping".to_string(),
                        ));
                    }
                    #[expect(clippy::cast_sign_loss)]
                    if stat.st_size as usize != map_size {
                        return Err(Error::unknown(
                            "The mapping's size differs from the requested size".to_string(),
                        ));
                    }
                    let map = mmap(
                        ptr::null_mut(),
                        map_size,
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED,
                        fd,
                        0,
                    );
                    if ptr::addr_eq(map, libc::MAP_FAILED) {
                        return Err(Error::last_os_error(format!(
                            "mmap() failed for map with fd {fd:?}"
                        )));
                    }
                    Ok(Self {
                        id: ShMemId::from_int(fd),
                        map: map as *mut u8,
                        map_size,
                    })
                }
            }
        }

        #[cfg(unix)]
        impl ShMem for MemfdShMem {
            fn id(&self) -> ShMemId {
                self.id
            }
        }

        impl Deref for MemfdShMem {
            type Target = [u8];

            fn deref(&self) -> &[u8] {
                unsafe { slice::from_raw_parts(self.map, self.map_size) }
            }
        }

        impl DerefMut for MemfdShMem {
            fn deref_mut(&mut self) -> &mut [u8] {
                unsafe { slice::from_raw_parts_mut(self.map, self.map_size) }
            }
        }

        /// [`Drop`] implementation for [`MemfdShMem`], which cleans up the mapping.
        #[cfg(unix)]
        impl Drop for MemfdShMem {
            fn drop(&mut self) {
                let fd = i32::from(self.id);

                unsafe {
                    munmap(self.map as *mut _, self.map_size);
                    close(fd);
                }
            }
        }

        /// A [`ShMemProvider`] which uses memfd to provide shared memory mappings.
        #[cfg(unix)]
        #[derive(Debug, Clone)]
        pub struct MemfdShMemProvider {}

        unsafe impl Send for MemfdShMemProvider {}

        #[cfg(unix)]
        impl Default for MemfdShMemProvider {
            fn default() -> Self {
                Self::new().unwrap()
            }
        }

        /// Dedicated Implementation to yield a [`std::fs::File`]
        #[cfg(unix)]
        impl MemfdShMemProvider {
            /// Unlike [`MemfdShMemProvider::new`], this returns a file instead, without any mmap and truncate.
            /// By default, the file size is capped by the tmpfs installed by the operating system, which is big
            /// enough to hold all output and avoid spurious read/write errors from children. However, you are free
            /// to set the size via [`std::fs::File::set_len`]
            pub fn new_file() -> Result<File, Error> {
                Ok(File::from(memfd_create(c"libafl_file", MFdFlags::empty())?))
            }
        }

        /// Implement [`ShMemProvider`] for [`MemfdShMemProvider`]
        #[cfg(unix)]
        impl ShMemProvider for MemfdShMemProvider {
            type ShMem = MemfdShMem;

            fn new() -> Result<Self, Error> {
                Ok(Self {})
            }

            fn new_shmem(&mut self, map_size: usize) -> Result<Self::ShMem, Error> {
                let mapping = MemfdShMem::new(map_size)?;
                Ok(mapping)
            }

            fn shmem_from_id_and_size(
                &mut self,
                id: ShMemId,
                size: usize,
            ) -> Result<Self::ShMem, Error> {
                MemfdShMem::shmem_from_id_and_size(id, size)
            }
        }
    }
}

/// Then `win32` implementation for shared memory.
#[cfg(all(feature = "std", windows))]
pub mod win32_shmem {
    use alloc::string::String;
    use core::{
        ffi::c_void,
        fmt::{self, Debug, Formatter},
        ops::{Deref, DerefMut},
        slice,
    };

    use uuid::Uuid;
    use windows::{
        Win32::{
            Foundation::{CloseHandle, HANDLE},
            System::Memory::{
                CreateFileMappingA, FILE_MAP_ALL_ACCESS, MEMORY_MAPPED_VIEW_ADDRESS, MapViewOfFile,
                OpenFileMappingA, PAGE_READWRITE, UnmapViewOfFile,
            },
        },
        core::PCSTR,
    };

    use crate::{
        Error,
        shmem::{ShMem, ShMemId, ShMemProvider},
    };

    const INVALID_HANDLE_VALUE: *mut c_void = -1isize as *mut c_void;

    /// The default [`ShMem`] impl for Windows using `shmctl` & `shmget`
    #[derive(Clone)]
    pub struct Win32ShMem {
        id: ShMemId,
        handle: HANDLE,
        map: *mut u8,
        map_size: usize,
    }

    unsafe impl Send for Win32ShMem {}

    impl Debug for Win32ShMem {
        fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
            f.debug_struct("Win32ShMem")
                .field("id", &self.id)
                .field("handle", &self.handle.0)
                .field("map", &self.map)
                .field("map_size", &self.map_size)
                .finish()
        }
    }

    impl Win32ShMem {
        fn new_shmem(map_size: usize) -> Result<Self, Error> {
            unsafe {
                let uuid = Uuid::new_v4();
                let mut map_str = format!("libafl_{}", uuid.simple());
                let map_str_bytes = map_str.as_mut_vec();
                map_str_bytes[19] = 0; // Trucate to size 20
                let handle = CreateFileMappingA(
                    HANDLE(INVALID_HANDLE_VALUE),
                    None,
                    PAGE_READWRITE,
                    0,
                    map_size as u32,
                    PCSTR(map_str_bytes.as_mut_ptr()),
                )?;

                let map =
                    MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, map_size).Value as *mut u8;
                if map.is_null() {
                    return Err(Error::unknown(format!(
                        "Cannot map shared memory {}",
                        String::from_utf8_lossy(map_str_bytes)
                    )));
                }

                Ok(Self {
                    id: ShMemId::try_from_slice(map_str_bytes).unwrap(),
                    handle,
                    map,
                    map_size,
                })
            }
        }

        fn shmem_from_id_and_size(id: ShMemId, map_size: usize) -> Result<Self, Error> {
            unsafe {
                let map_str_bytes = id.id;
                // Unlike MapViewOfFile this one needs u32
                let handle = OpenFileMappingA(
                    FILE_MAP_ALL_ACCESS.0,
                    false,
                    PCSTR(map_str_bytes.as_ptr().cast_mut()),
                )?;

                let map =
                    MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, map_size).Value as *mut u8;
                if map.is_null() {
                    return Err(Error::unknown(format!(
                        "Cannot map shared memory {}",
                        String::from_utf8_lossy(&map_str_bytes)
                    )));
                }
                Ok(Self {
                    id,
                    handle,
                    map,
                    map_size,
                })
            }
        }
    }

    impl ShMem for Win32ShMem {
        fn id(&self) -> ShMemId {
            self.id
        }
    }

    impl Deref for Win32ShMem {
        type Target = [u8];
        fn deref(&self) -> &[u8] {
            unsafe { slice::from_raw_parts(self.map, self.map_size) }
        }
    }
    impl DerefMut for Win32ShMem {
        fn deref_mut(&mut self) -> &mut [u8] {
            unsafe { slice::from_raw_parts_mut(self.map, self.map_size) }
        }
    }

    /// Deinit sharedmaps on [`Drop`]
    impl Drop for Win32ShMem {
        fn drop(&mut self) {
            unsafe {
                let res = UnmapViewOfFile(MEMORY_MAPPED_VIEW_ADDRESS {
                    Value: self.map as *mut c_void,
                });
                if let Err(err) = res {
                    // ignore result: nothing we can do if this goes wrong..
                    log::warn!("Failed to unmap memory at {:?}: {err}", self.map);
                }
                let res = CloseHandle(self.handle);
                if let Err(err) = res {
                    // ignore result: nothing we can do if this goes wrong..
                    log::warn!("Failed to close mem handle {:?}: {err}", self.handle);
                }
            }
        }
    }

    /// A [`ShMemProvider`] which uses `win32` functions to provide shared memory mappings.
    #[derive(Debug, Clone)]
    pub struct Win32ShMemProvider {}

    impl Default for Win32ShMemProvider {
        fn default() -> Self {
            Self::new().unwrap()
        }
    }

    /// Implement [`ShMemProvider`] for [`Win32ShMemProvider`]
    impl ShMemProvider for Win32ShMemProvider {
        type ShMem = Win32ShMem;

        fn new() -> Result<Self, Error> {
            Ok(Self {})
        }
        fn new_shmem(&mut self, map_size: usize) -> Result<Self::ShMem, Error> {
            Win32ShMem::new_shmem(map_size)
        }

        fn shmem_from_id_and_size(
            &mut self,
            id: ShMemId,
            size: usize,
        ) -> Result<Self::ShMem, Error> {
            Win32ShMem::shmem_from_id_and_size(id, size)
        }
    }
}

/// A `ShMemService` dummy, that does nothing on start.
/// Drop in for targets that don't need a server for ref counting and page creation.
#[derive(Debug)]
pub struct DummyShMemService;

impl DummyShMemService {
    /// Create a new [`DummyShMemService`] that does nothing.
    /// Useful only to have the same API for [`StdShMemService`] on Operating Systems that don't need it.
    #[inline]
    pub fn start() -> Result<Self, Error> {
        Ok(Self {})
    }
}

/// A cursor around [`ShMem`] that immitates [`std::io::Cursor`]. Notably, this implements [`Write`] for [`ShMem`] in std environments.
#[cfg(feature = "std")]
#[derive(Debug)]
pub struct ShMemCursor<SHM> {
    inner: SHM,
    pos: usize,
}

#[cfg(all(feature = "std", not(target_os = "haiku")))]
impl<SHM> ShMemCursor<SHM> {
    /// Create a new [`ShMemCursor`] around [`ShMem`]
    pub fn new(shmem: SHM) -> Self {
        Self {
            inner: shmem,
            pos: 0,
        }
    }

    /// Slice from the current location on this map to the end, mutable
    fn empty_slice_mut(&mut self) -> &mut [u8]
    where
        SHM: DerefMut<Target = [u8]>,
    {
        use crate::AsSliceMut;
        &mut (self.inner.as_slice_mut()[self.pos..])
    }
}

#[cfg(all(feature = "std", not(target_os = "haiku")))]
impl<SHM> Write for ShMemCursor<SHM>
where
    SHM: DerefMut<Target = [u8]>,
{
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        match self.empty_slice_mut().write(buf) {
            Ok(w) => {
                self.pos += w;
                Ok(w)
            }
            Err(e) => Err(e),
        }
    }

    fn write_vectored(&mut self, bufs: &[std::io::IoSlice<'_>]) -> std::io::Result<usize> {
        match self.empty_slice_mut().write_vectored(bufs) {
            Ok(w) => {
                self.pos += w;
                Ok(w)
            }
            Err(e) => Err(e),
        }
    }

    fn flush(&mut self) -> std::io::Result<()> {
        Ok(())
    }

    fn write_all(&mut self, buf: &[u8]) -> std::io::Result<()> {
        match self.empty_slice_mut().write_all(buf) {
            Ok(w) => {
                self.pos += buf.len();
                Ok(w)
            }
            Err(e) => Err(e),
        }
    }
}

#[cfg(feature = "std")]
impl<SHM> std::io::Seek for ShMemCursor<SHM>
where
    SHM: DerefMut<Target = [u8]>,
{
    fn seek(&mut self, pos: std::io::SeekFrom) -> std::io::Result<u64> {
        let effective_new_pos = match pos {
            std::io::SeekFrom::Start(s) => s,
            std::io::SeekFrom::End(offset) => {
                use crate::AsSlice;
                let map_len = self.inner.as_slice().len();
                let signed_pos = i64::try_from(map_len).unwrap();
                let effective = signed_pos.checked_add(offset).unwrap();
                assert!(effective >= 0);
                effective.try_into().unwrap()
            }
            std::io::SeekFrom::Current(offset) => {
                let current_pos = self.pos;
                i64::try_from(current_pos).unwrap();
                let signed_pos = i64::try_from(current_pos).unwrap();
                let effective = signed_pos.checked_add(offset).unwrap();
                assert!(effective >= 0);
                effective.try_into().unwrap()
            }
        };
        usize::try_from(effective_new_pos).unwrap();
        self.pos = effective_new_pos as usize;
        Ok(effective_new_pos)
    }
}

#[cfg(all(feature = "std", not(target_os = "haiku")))]
#[cfg(test)]
mod tests {
    use serial_test::serial;

    use crate::{
        AsSlice, AsSliceMut, Error,
        shmem::{ShMemProvider, StdShMemProvider},
    };

    #[test]
    #[serial]
    #[cfg_attr(miri, ignore)]
    fn test_shmem_service() -> Result<(), Error> {
        let mut provider = StdShMemProvider::new()?;
        let mut map = provider.new_shmem(1024)?;
        map.as_slice_mut()[0] = 1;
        assert_eq!(1, map.as_slice()[0]);
        Ok(())
    }

    #[test]
    #[cfg(unix)]
    #[cfg_attr(miri, ignore)]
    fn test_persist_shmem() -> Result<(), Error> {
        use alloc::string::ToString;
        use core::ffi::CStr;
        use std::{
            env,
            process::{Command, Stdio},
        };

        use crate::shmem::{MmapShMemProvider, ShMem as _, ShMemId};

        // relies on the fact that the ID in a ShMemDescription is always a string for MmapShMem
        match env::var("SHMEM_SIZE") {
            Ok(size) => {
                let mut provider = MmapShMemProvider::new()?;
                let id = ShMemId::from_string(&env::var("SHMEM_ID").unwrap());
                let size = size.parse().unwrap();
                let mut shmem = provider.shmem_from_id_and_size(id, size)?;
                shmem[0] = 1;
            }
            Err(env::VarError::NotPresent) => {
                let mut provider = MmapShMemProvider::new()?;
                let mut shmem = provider.new_shmem(1)?.persist()?;
                shmem.fill(0);
                let description = shmem.description();

                // call the test binary again
                // with certain env variables set to prevent infinite loops
                // and with an added arg to only run this test
                //
                // a command is necessary to create the required distance between the two processes
                // with threads/fork it works without the additional steps to persist the ShMem regardless
                let status = Command::new(env::current_exe().unwrap())
                    .env(
                        "SHMEM_ID",
                        CStr::from_bytes_until_nul(description.id.as_array())
                            .unwrap()
                            .to_str()
                            .unwrap(),
                    )
                    .env("SHMEM_SIZE", description.size.to_string())
                    .arg("shmem::tests::test_persist_shmem")
                    .stdout(Stdio::null())
                    .stderr(Stdio::null())
                    .status()
                    .unwrap();

                assert!(status.success());
                assert_eq!(shmem[0], 1);
            }
            Err(e) => panic!("{e}"),
        }

        Ok(())
    }

    #[test]
    #[cfg_attr(miri, ignore)]
    fn test_shmem_release() -> Result<(), Error> {
        let mut provider = StdShMemProvider::new()?;
        let mut shmem = provider.new_shmem(1024)?;
        provider.release_shmem(&mut shmem);
        drop(shmem);
        Ok(())
    }

    #[test]
    #[cfg_attr(miri, ignore)]
    #[cfg(unix)]
    fn test_mmap_shmem_release() -> Result<(), Error> {
        use crate::shmem::MmapShMemProvider;

        let mut provider = MmapShMemProvider::new()?;
        let mut shmem = provider.new_shmem(1024)?;
        provider.release_shmem(&mut shmem);
        drop(shmem);
        Ok(())
    }
}
