use super::*;

/// A BSTR string ([BSTR](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/automat/string-manipulation-functions))
/// is a length-prefixed wide string.
#[repr(transparent)]
pub struct BSTR(*const u16);

impl BSTR {
    /// Create an empty `BSTR`.
    ///
    /// This function does not allocate memory.
    pub const fn new() -> Self {
        Self(std::ptr::null_mut())
    }

    /// Returns `true` if the string is empty.
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Returns the length of the string.
    pub fn len(&self) -> usize {
        if self.0.is_null() {
            0
        } else {
            unsafe { crate::imp::SysStringLen(self.0) as usize }
        }
    }

    /// Get the string as 16-bit wide characters (wchars).
    pub fn as_wide(&self) -> &[u16] {
        if self.0.is_null() {
            return &[];
        }

        unsafe { std::slice::from_raw_parts(self.0, self.len()) }
    }

    /// Create a `BSTR` from a slice of 16 bit characters (wchars).
    pub fn from_wide(value: &[u16]) -> Result<Self> {
        if value.is_empty() {
            return Ok(Self::new());
        }

        let result = unsafe { Self(crate::imp::SysAllocStringLen(value.as_ptr(), value.len().try_into()?)) };

        if result.is_empty() {
            Err(crate::imp::E_OUTOFMEMORY.into())
        } else {
            Ok(result)
        }
    }

    /// # Safety
    #[doc(hidden)]
    pub unsafe fn from_raw(raw: *const u16) -> Self {
        Self(raw)
    }

    /// # Safety
    #[doc(hidden)]
    pub fn into_raw(self) -> *const u16 {
        unsafe { std::mem::transmute(self) }
    }
}

impl std::clone::Clone for BSTR {
    fn clone(&self) -> Self {
        Self::from_wide(self.as_wide()).unwrap()
    }
}

impl std::convert::From<&str> for BSTR {
    fn from(value: &str) -> Self {
        let value: std::vec::Vec<u16> = value.encode_utf16().collect();
        Self::from_wide(&value).unwrap()
    }
}

impl std::convert::From<std::string::String> for BSTR {
    fn from(value: std::string::String) -> Self {
        value.as_str().into()
    }
}

impl std::convert::From<&std::string::String> for BSTR {
    fn from(value: &std::string::String) -> Self {
        value.as_str().into()
    }
}

impl<'a> std::convert::TryFrom<&'a BSTR> for std::string::String {
    type Error = std::string::FromUtf16Error;

    fn try_from(value: &BSTR) -> std::result::Result<Self, Self::Error> {
        std::string::String::from_utf16(value.as_wide())
    }
}

impl std::convert::TryFrom<BSTR> for std::string::String {
    type Error = std::string::FromUtf16Error;

    fn try_from(value: BSTR) -> std::result::Result<Self, Self::Error> {
        std::string::String::try_from(&value)
    }
}

impl std::default::Default for BSTR {
    fn default() -> Self {
        Self(std::ptr::null_mut())
    }
}

impl std::fmt::Display for BSTR {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        std::write!(f, "{}", crate::Decode(|| std::char::decode_utf16(self.as_wide().iter().cloned())))
    }
}

impl std::fmt::Debug for BSTR {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        std::write!(f, "{}", self)
    }
}

impl std::cmp::PartialEq for BSTR {
    fn eq(&self, other: &Self) -> bool {
        self.as_wide() == other.as_wide()
    }
}

impl std::cmp::Eq for BSTR {}

impl std::cmp::PartialEq<BSTR> for &str {
    fn eq(&self, other: &BSTR) -> bool {
        other == self
    }
}

impl std::cmp::PartialEq<BSTR> for String {
    fn eq(&self, other: &BSTR) -> bool {
        other == self
    }
}

impl<T: AsRef<str> + ?Sized> std::cmp::PartialEq<T> for BSTR {
    fn eq(&self, other: &T) -> bool {
        self.as_wide().iter().copied().eq(other.as_ref().encode_utf16())
    }
}

impl std::ops::Drop for BSTR {
    fn drop(&mut self) {
        if !self.0.is_null() {
            unsafe { crate::imp::SysFreeString(self.0) }
        }
    }
}

impl TypeKind for BSTR {
    type TypeKind = ValueType;
}
