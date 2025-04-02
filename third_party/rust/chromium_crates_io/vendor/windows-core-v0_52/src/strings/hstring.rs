use super::*;

/// A WinRT string ([HSTRING](https://docs.microsoft.com/en-us/windows/win32/winrt/hstring))
/// is reference-counted and immutable.
#[repr(transparent)]
pub struct HSTRING(Option<std::ptr::NonNull<Header>>);

impl HSTRING {
    /// Create an empty `HSTRING`.
    ///
    /// This function does not allocate memory.
    pub const fn new() -> Self {
        Self(None)
    }

    /// Returns `true` if the string is empty.
    pub const fn is_empty(&self) -> bool {
        // An empty HSTRING is represented by a null pointer.
        self.0.is_none()
    }

    /// Returns the length of the string.
    pub fn len(&self) -> usize {
        if let Some(header) = self.get_header() {
            header.len as usize
        } else {
            0
        }
    }

    /// Get the string as 16-bit wide characters (wchars).
    pub fn as_wide(&self) -> &[u16] {
        unsafe { std::slice::from_raw_parts(self.as_ptr(), self.len()) }
    }

    /// Returns a raw pointer to the `HSTRING` buffer.
    pub fn as_ptr(&self) -> *const u16 {
        if let Some(header) = self.get_header() {
            header.data
        } else {
            const EMPTY: [u16; 1] = [0];
            EMPTY.as_ptr()
        }
    }

    /// Create a `HSTRING` from a slice of 16 bit characters (wchars).
    pub fn from_wide(value: &[u16]) -> Result<Self> {
        unsafe { Self::from_wide_iter(value.iter().copied(), value.len()) }
    }

    /// Get the contents of this `HSTRING` as a String lossily.
    pub fn to_string_lossy(&self) -> std::string::String {
        std::string::String::from_utf16_lossy(self.as_wide())
    }

    /// Get the contents of this `HSTRING` as a OsString.
    #[cfg(windows)]
    pub fn to_os_string(&self) -> std::ffi::OsString {
        std::os::windows::ffi::OsStringExt::from_wide(self.as_wide())
    }

    /// # Safety
    /// len must not be less than the number of items in the iterator.
    unsafe fn from_wide_iter<I: Iterator<Item = u16>>(iter: I, len: usize) -> Result<Self> {
        if len == 0 {
            return Ok(Self::new());
        }

        let ptr = Header::alloc(len.try_into()?)?;

        // Place each utf-16 character into the buffer and
        // increase len as we go along.
        for (index, wide) in iter.enumerate() {
            debug_assert!(index < len);

            std::ptr::write((*ptr).data.add(index), wide);
            (*ptr).len = index as u32 + 1;
        }

        // Write a 0 byte to the end of the buffer.
        std::ptr::write((*ptr).data.offset((*ptr).len as isize), 0);
        Ok(Self(std::ptr::NonNull::new(ptr)))
    }

    fn get_header(&self) -> Option<&Header> {
        if let Some(header) = &self.0 {
            unsafe { Some(header.as_ref()) }
        } else {
            None
        }
    }
}

impl RuntimeType for HSTRING {
    const SIGNATURE: crate::imp::ConstBuffer = crate::imp::ConstBuffer::from_slice(b"string");
}

impl TypeKind for HSTRING {
    type TypeKind = ValueType;
}

impl Default for HSTRING {
    fn default() -> Self {
        Self::new()
    }
}

impl Clone for HSTRING {
    fn clone(&self) -> Self {
        if let Some(header) = self.get_header() {
            Self(std::ptr::NonNull::new(header.duplicate().unwrap()))
        } else {
            Self::new()
        }
    }
}

impl Drop for HSTRING {
    fn drop(&mut self) {
        if self.is_empty() {
            return;
        }

        if let Some(header) = self.0.take() {
            // REFERENCE_FLAG indicates a string backed by static or stack memory that is
            // thus not reference-counted and does not need to be freed.
            unsafe {
                let header = header.as_ref();
                if header.flags & REFERENCE_FLAG == 0 && header.count.release() == 0 {
                    crate::imp::heap_free(header as *const _ as *mut _);
                }
            }
        }
    }
}

unsafe impl Send for HSTRING {}
unsafe impl Sync for HSTRING {}

impl std::fmt::Display for HSTRING {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", Decode(|| std::char::decode_utf16(self.as_wide().iter().cloned())))
    }
}

impl std::fmt::Debug for HSTRING {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "\"{}\"", self)
    }
}

impl std::convert::From<&str> for HSTRING {
    fn from(value: &str) -> Self {
        unsafe { Self::from_wide_iter(value.encode_utf16(), value.len()).unwrap() }
    }
}

impl std::convert::From<std::string::String> for HSTRING {
    fn from(value: std::string::String) -> Self {
        value.as_str().into()
    }
}

impl std::convert::From<&std::string::String> for HSTRING {
    fn from(value: &std::string::String) -> Self {
        value.as_str().into()
    }
}

#[cfg(windows)]
impl std::convert::From<&std::path::Path> for HSTRING {
    fn from(value: &std::path::Path) -> Self {
        value.as_os_str().into()
    }
}

#[cfg(windows)]
impl std::convert::From<&std::ffi::OsStr> for HSTRING {
    fn from(value: &std::ffi::OsStr) -> Self {
        unsafe { Self::from_wide_iter(std::os::windows::ffi::OsStrExt::encode_wide(value), value.len()).unwrap() }
    }
}

#[cfg(windows)]
impl std::convert::From<std::ffi::OsString> for HSTRING {
    fn from(value: std::ffi::OsString) -> Self {
        value.as_os_str().into()
    }
}

#[cfg(windows)]
impl std::convert::From<&std::ffi::OsString> for HSTRING {
    fn from(value: &std::ffi::OsString) -> Self {
        value.as_os_str().into()
    }
}

impl Eq for HSTRING {}

impl Ord for HSTRING {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.as_wide().cmp(other.as_wide())
    }
}

impl PartialOrd for HSTRING {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl PartialEq for HSTRING {
    fn eq(&self, other: &Self) -> bool {
        *self.as_wide() == *other.as_wide()
    }
}

impl PartialEq<std::string::String> for HSTRING {
    fn eq(&self, other: &std::string::String) -> bool {
        *self == **other
    }
}

impl PartialEq<std::string::String> for &HSTRING {
    fn eq(&self, other: &std::string::String) -> bool {
        **self == **other
    }
}

impl PartialEq<&std::string::String> for HSTRING {
    fn eq(&self, other: &&std::string::String) -> bool {
        *self == ***other
    }
}

impl PartialEq<str> for HSTRING {
    fn eq(&self, other: &str) -> bool {
        self.as_wide().iter().copied().eq(other.encode_utf16())
    }
}

impl PartialEq<str> for &HSTRING {
    fn eq(&self, other: &str) -> bool {
        **self == *other
    }
}

impl PartialEq<&str> for HSTRING {
    fn eq(&self, other: &&str) -> bool {
        *self == **other
    }
}

impl PartialEq<HSTRING> for str {
    fn eq(&self, other: &HSTRING) -> bool {
        *other == *self
    }
}

impl PartialEq<HSTRING> for &str {
    fn eq(&self, other: &HSTRING) -> bool {
        *other == **self
    }
}

impl PartialEq<&HSTRING> for str {
    fn eq(&self, other: &&HSTRING) -> bool {
        **other == *self
    }
}

impl PartialEq<HSTRING> for std::string::String {
    fn eq(&self, other: &HSTRING) -> bool {
        *other == **self
    }
}

impl PartialEq<HSTRING> for &std::string::String {
    fn eq(&self, other: &HSTRING) -> bool {
        *other == ***self
    }
}

impl PartialEq<&HSTRING> for std::string::String {
    fn eq(&self, other: &&HSTRING) -> bool {
        **other == **self
    }
}

#[cfg(windows)]
impl PartialEq<std::ffi::OsString> for HSTRING {
    fn eq(&self, other: &std::ffi::OsString) -> bool {
        *self == **other
    }
}

#[cfg(windows)]
impl PartialEq<std::ffi::OsString> for &HSTRING {
    fn eq(&self, other: &std::ffi::OsString) -> bool {
        **self == **other
    }
}

#[cfg(windows)]
impl PartialEq<&std::ffi::OsString> for HSTRING {
    fn eq(&self, other: &&std::ffi::OsString) -> bool {
        *self == ***other
    }
}

#[cfg(windows)]
impl PartialEq<std::ffi::OsStr> for HSTRING {
    fn eq(&self, other: &std::ffi::OsStr) -> bool {
        self.as_wide().iter().copied().eq(std::os::windows::ffi::OsStrExt::encode_wide(other))
    }
}

#[cfg(windows)]
impl PartialEq<std::ffi::OsStr> for &HSTRING {
    fn eq(&self, other: &std::ffi::OsStr) -> bool {
        **self == *other
    }
}

#[cfg(windows)]
impl PartialEq<&std::ffi::OsStr> for HSTRING {
    fn eq(&self, other: &&std::ffi::OsStr) -> bool {
        *self == **other
    }
}

#[cfg(windows)]
impl PartialEq<HSTRING> for std::ffi::OsStr {
    fn eq(&self, other: &HSTRING) -> bool {
        *other == *self
    }
}

#[cfg(windows)]
impl PartialEq<HSTRING> for &std::ffi::OsStr {
    fn eq(&self, other: &HSTRING) -> bool {
        *other == **self
    }
}

#[cfg(windows)]
impl PartialEq<&HSTRING> for std::ffi::OsStr {
    fn eq(&self, other: &&HSTRING) -> bool {
        **other == *self
    }
}

#[cfg(windows)]
impl PartialEq<HSTRING> for std::ffi::OsString {
    fn eq(&self, other: &HSTRING) -> bool {
        *other == **self
    }
}

#[cfg(windows)]
impl PartialEq<HSTRING> for &std::ffi::OsString {
    fn eq(&self, other: &HSTRING) -> bool {
        *other == ***self
    }
}

#[cfg(windows)]
impl PartialEq<&HSTRING> for std::ffi::OsString {
    fn eq(&self, other: &&HSTRING) -> bool {
        **other == **self
    }
}

impl<'a> std::convert::TryFrom<&'a HSTRING> for std::string::String {
    type Error = std::string::FromUtf16Error;

    fn try_from(hstring: &HSTRING) -> std::result::Result<Self, Self::Error> {
        std::string::String::from_utf16(hstring.as_wide())
    }
}

impl std::convert::TryFrom<HSTRING> for std::string::String {
    type Error = std::string::FromUtf16Error;

    fn try_from(hstring: HSTRING) -> std::result::Result<Self, Self::Error> {
        std::string::String::try_from(&hstring)
    }
}

#[cfg(windows)]
impl<'a> std::convert::From<&'a HSTRING> for std::ffi::OsString {
    fn from(hstring: &HSTRING) -> Self {
        hstring.to_os_string()
    }
}

#[cfg(windows)]
impl std::convert::From<HSTRING> for std::ffi::OsString {
    fn from(hstring: HSTRING) -> Self {
        Self::from(&hstring)
    }
}

impl IntoParam<PCWSTR> for &HSTRING {
    fn into_param(self) -> Param<PCWSTR> {
        Param::Owned(PCWSTR(self.as_ptr()))
    }
}

const REFERENCE_FLAG: u32 = 1;

#[repr(C)]
struct Header {
    flags: u32,
    len: u32,
    _0: u32,
    _1: u32,
    data: *mut u16,
    count: crate::imp::RefCount,
    buffer_start: u16,
}

impl Header {
    fn alloc(len: u32) -> Result<*mut Header> {
        debug_assert!(len != 0);
        // Allocate enough space for header and two bytes per character.
        // The space for the terminating null character is already accounted for inside of `Header`.
        let alloc_size = std::mem::size_of::<Header>() + 2 * len as usize;

        let header = crate::imp::heap_alloc(alloc_size)? as *mut Header;

        // SAFETY: uses `std::ptr::write` (since `header` is unintialized). `Header` is safe to be all zeros.
        unsafe {
            header.write(std::mem::MaybeUninit::<Header>::zeroed().assume_init());
            (*header).len = len;
            (*header).count = crate::imp::RefCount::new(1);
            (*header).data = &mut (*header).buffer_start;
        }
        Ok(header)
    }

    fn duplicate(&self) -> Result<*mut Header> {
        if self.flags & REFERENCE_FLAG == 0 {
            // If this is not a "fast pass" string then simply increment the reference count.
            self.count.add_ref();
            Ok(self as *const Header as *mut Header)
        } else {
            // Otherwise, allocate a new string and copy the value into the new string.
            let copy = Header::alloc(self.len)?;
            // SAFETY: since we are duplicating the string it is safe to copy all data from self to the initialized `copy`.
            // We copy `len + 1` characters since `len` does not account for the terminating null character.
            unsafe {
                std::ptr::copy_nonoverlapping(self.data, (*copy).data, self.len as usize + 1);
            }
            Ok(copy)
        }
    }
}
