use super::*;

/// An error object consists of both an error code as well as detailed error information for debugging.
#[derive(Clone, PartialEq, Eq)]
pub struct Error {
    pub(crate) code: HRESULT,
    pub(crate) info: Option<crate::imp::IRestrictedErrorInfo>,
}

impl Error {
    /// An error object without any failure information.
    pub const OK: Self = Self { code: HRESULT(0), info: None };

    /// This creates a new WinRT error object, capturing the stack and other information about the
    /// point of failure.
    pub fn new(code: HRESULT, message: HSTRING) -> Self {
        unsafe {
            if let Some(function) = crate::imp::delay_load::<RoOriginateError>(s!("combase.dll"), s!("RoOriginateError")) {
                function(code, std::mem::transmute_copy(&message));
            }
            let info = GetErrorInfo().and_then(|e| e.cast()).ok();
            Self { code, info }
        }
    }

    pub fn from_win32() -> Self {
        unsafe { Self { code: HRESULT::from_win32(crate::imp::GetLastError()), info: None } }
    }

    /// The error code describing the error.
    pub const fn code(&self) -> HRESULT {
        self.code
    }

    /// The error information describing the error.
    pub const fn info(&self) -> &Option<crate::imp::IRestrictedErrorInfo> {
        &self.info
    }

    /// The error message describing the error.
    pub fn message(&self) -> HSTRING {
        // First attempt to retrieve the restricted error information.
        if let Some(info) = &self.info {
            let mut fallback = BSTR::default();
            let mut message = BSTR::default();
            let mut code = HRESULT(0);

            unsafe {
                let _ = info.GetErrorDetails(&mut fallback, &mut code, &mut message, &mut BSTR::default());
            }

            if self.code == code {
                let message = if !message.is_empty() { message } else { fallback };
                return HSTRING::from_wide(crate::imp::wide_trim_end(message.as_wide())).unwrap_or_default();
            }
        }

        self.code.message()
    }
}

impl std::convert::From<Error> for HRESULT {
    fn from(error: Error) -> Self {
        let code = error.code;
        let info: Option<crate::imp::IErrorInfo> = error.info.and_then(|info| info.cast().ok());

        unsafe {
            let _ = crate::imp::SetErrorInfo(0, info.as_ref());
        }

        code
    }
}

impl std::convert::From<Error> for std::io::Error {
    fn from(from: Error) -> Self {
        Self::from_raw_os_error(from.code.0)
    }
}

impl std::convert::From<std::string::FromUtf16Error> for Error {
    fn from(_: std::string::FromUtf16Error) -> Self {
        Self { code: HRESULT::from_win32(crate::imp::ERROR_NO_UNICODE_TRANSLATION), info: None }
    }
}

impl std::convert::From<std::string::FromUtf8Error> for Error {
    fn from(_: std::string::FromUtf8Error) -> Self {
        Self { code: HRESULT::from_win32(crate::imp::ERROR_NO_UNICODE_TRANSLATION), info: None }
    }
}

impl std::convert::From<std::num::TryFromIntError> for Error {
    fn from(_: std::num::TryFromIntError) -> Self {
        Self { code: HRESULT(crate::imp::E_INVALIDARG), info: None }
    }
}

// Unfortunately this is needed to make types line up. The Rust type system does
// not know the `Infallible` can never be constructed. This code needs to be here
// to satesify the type checker but it will never be run. Once `!` is stabilizied
// this can be removed.
impl std::convert::From<std::convert::Infallible> for Error {
    fn from(_: std::convert::Infallible) -> Self {
        unreachable!()
    }
}

impl std::convert::From<HRESULT> for Error {
    fn from(code: HRESULT) -> Self {
        let info: Option<crate::imp::IRestrictedErrorInfo> = GetErrorInfo().and_then(|e| e.cast()).ok();

        if let Some(info) = info {
            // If it does (and therefore running on a recent version of Windows)
            // then capture_propagation_context adds a breadcrumb to the error
            // info to make debugging easier.
            if let Ok(capture) = info.cast::<crate::imp::ILanguageExceptionErrorInfo2>() {
                unsafe {
                    let _ = capture.CapturePropagationContext(None);
                }
            }

            return Self { code, info: Some(info) };
        }

        if let Ok(info) = GetErrorInfo() {
            let message = unsafe { info.GetDescription().unwrap_or_default() };
            Self::new(code, HSTRING::from_wide(message.as_wide()).unwrap_or_default())
        } else {
            Self { code, info: None }
        }
    }
}

impl std::fmt::Debug for Error {
    fn fmt(&self, fmt: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut debug = fmt.debug_struct("Error");
        debug.field("code", &self.code).field("message", &self.message()).finish()
    }
}

impl std::fmt::Display for Error {
    fn fmt(&self, fmt: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let message = self.message();
        if message.is_empty() {
            std::write!(fmt, "{}", self.code())
        } else {
            std::write!(fmt, "{} ({})", self.message(), self.code())
        }
    }
}

impl std::error::Error for Error {}

type RoOriginateError = extern "system" fn(code: HRESULT, message: *mut std::ffi::c_void) -> i32;

fn GetErrorInfo() -> Result<crate::imp::IErrorInfo> {
    unsafe { crate::imp::GetErrorInfo(0) }
}
