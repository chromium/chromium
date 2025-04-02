use super::*;
use crate::ComInterface;
use std::marker::PhantomData;
use std::sync::atomic::{AtomicPtr, Ordering};

#[doc(hidden)]
pub struct FactoryCache<C, I> {
    shared: AtomicPtr<std::ffi::c_void>,
    _c: PhantomData<C>,
    _i: PhantomData<I>,
}

impl<C, I> FactoryCache<C, I> {
    pub const fn new() -> Self {
        Self { shared: AtomicPtr::new(std::ptr::null_mut()), _c: PhantomData, _i: PhantomData }
    }
}

impl<C: crate::RuntimeName, I: crate::ComInterface> FactoryCache<C, I> {
    pub fn call<R, F: FnOnce(&I) -> crate::Result<R>>(&self, callback: F) -> crate::Result<R> {
        loop {
            // Attempt to load a previously cached factory pointer.
            let ptr = self.shared.load(Ordering::Relaxed);

            // If a pointer is found, the cache is primed and we're good to go.
            if !ptr.is_null() {
                return callback(unsafe { std::mem::transmute(&ptr) });
            }

            // Otherwise, we load the factory the usual way.
            let factory = factory::<C, I>()?;

            // If the factory is agile, we can safely cache it.
            if factory.cast::<IAgileObject>().is_ok() {
                if self.shared.compare_exchange_weak(std::ptr::null_mut(), factory.as_raw(), Ordering::Relaxed, Ordering::Relaxed).is_ok() {
                    std::mem::forget(factory);
                }
            } else {
                // Otherwise, for non-agile factories we simply use the factory
                // and discard after use as it is not safe to cache.
                return callback(&factory);
            }
        }
    }
}

// This is safe because `FactoryCache` only holds agile factory pointers, which are safe to cache and share between threads.
unsafe impl<C, I> std::marker::Sync for FactoryCache<C, I> {}

/// Attempts to load the factory object for the given WinRT class.
/// This can be used to access COM interfaces implemented on a Windows Runtime class factory.
pub fn factory<C: crate::RuntimeName, I: crate::ComInterface>() -> crate::Result<I> {
    let mut factory: Option<I> = None;
    let name = crate::HSTRING::from(C::NAME);

    let code = if let Some(function) = unsafe { delay_load::<RoGetActivationFactory>(crate::s!("combase.dll"), crate::s!("RoGetActivationFactory")) } {
        unsafe {
            let mut code = function(std::mem::transmute_copy(&name), &I::IID, &mut factory as *mut _ as *mut _);

            // If RoGetActivationFactory fails because combase hasn't been loaded yet then load combase
            // automatically so that it "just works" for apartment-agnostic code.
            if code == CO_E_NOTINITIALIZED {
                if let Some(mta) = delay_load::<CoIncrementMTAUsage>(crate::s!("ole32.dll"), crate::s!("CoIncrementMTAUsage")) {
                    let mut cookie = std::ptr::null_mut();
                    let _ = mta(&mut cookie);
                }

                // Now try a second time to get the activation factory via the OS.
                code = function(std::mem::transmute_copy(&name), &I::IID, &mut factory as *mut _ as *mut _);
            }

            code
        }
    } else {
        CLASS_E_CLASSNOTAVAILABLE
    };

    // If this succeeded then return the resulting factory interface.
    if code.is_ok() {
        return code.and_some(factory);
    }

    // If not, first capture the error information from the failure above so that we
    // can ultimately return this error information if all else fails.
    let original: crate::Error = code.into();

    // Now attempt to find the factory's implementation heuristically.
    if let Some(i) = search_path(C::NAME, |library| unsafe { get_activation_factory(library, &name) }) {
        i.cast()
    } else {
        Err(original)
    }
}

// Remove the suffix until a match is found appending `.dll\0` at the end
///
/// For example, if the class name is
/// "A.B.TypeName" then the attempted load order will be:
///   1. A.B.dll
///   2. A.dll
fn search_path<F, R>(mut path: &str, mut callback: F) -> Option<R>
where
    F: FnMut(crate::PCSTR) -> crate::Result<R>,
{
    let suffix = b".dll\0";
    let mut library = vec![0; path.len() + suffix.len()];
    while let Some(pos) = path.rfind('.') {
        path = &path[..pos];
        library.truncate(path.len() + suffix.len());
        library[..path.len()].copy_from_slice(path.as_bytes());
        library[path.len()..].copy_from_slice(suffix);

        if let Ok(r) = callback(crate::PCSTR::from_raw(library.as_ptr())) {
            return Some(r);
        }
    }

    None
}

unsafe fn get_activation_factory(library: crate::PCSTR, name: &crate::HSTRING) -> crate::Result<IGenericFactory> {
    let function = delay_load::<DllGetActivationFactory>(library, crate::s!("DllGetActivationFactory")).ok_or_else(crate::Error::from_win32)?;
    let mut abi = std::ptr::null_mut();
    function(std::mem::transmute_copy(name), &mut abi).from_abi(abi)
}

type CoIncrementMTAUsage = extern "system" fn(cookie: *mut *mut std::ffi::c_void) -> crate::HRESULT;
type RoGetActivationFactory = extern "system" fn(hstring: *mut std::ffi::c_void, interface: &crate::GUID, result: *mut *mut std::ffi::c_void) -> crate::HRESULT;
type DllGetActivationFactory = extern "system" fn(name: *mut std::ffi::c_void, factory: *mut *mut std::ffi::c_void) -> crate::HRESULT;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn dll_search() {
        let path = "A.B.TypeName";

        // Test library successfully found
        let mut results = Vec::new();
        let end_result = search_path(path, |library| {
            results.push(unsafe { library.to_string().unwrap() });
            if unsafe { library.as_bytes() } == &b"A.dll"[..] {
                Ok(42)
            } else {
                Err(crate::Error::OK)
            }
        });
        assert!(matches!(end_result, Some(42)));
        assert_eq!(results, vec!["A.B.dll", "A.dll"]);

        // Test library never successfully found
        let mut results = Vec::new();
        let end_result = search_path(path, |library| {
            results.push(unsafe { library.to_string().unwrap() });
            crate::Result::<()>::Err(crate::Error::OK)
        });
        assert!(end_result.is_none());
        assert_eq!(results, vec!["A.B.dll", "A.dll"]);
    }
}
