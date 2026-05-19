//! Parse command line argument like AFL, then put it in a C-compatible way
use alloc::{boxed::Box, ffi::CString, vec::Vec};
use core::{
    ffi::{c_char, c_int},
    pin::Pin,
};
use std::os::unix::ffi::OsStrExt;

use crate::{Error, StdTargetArgs, StdTargetArgsInner};

/// For creating an C-compatible argument
#[derive(Debug)]
pub struct CMainArgsBuilder {
    inner: StdTargetArgsInner,
}

impl StdTargetArgs for CMainArgsBuilder {
    fn inner(&self) -> &StdTargetArgsInner {
        &self.inner
    }

    fn inner_mut(&mut self) -> &mut StdTargetArgsInner {
        &mut self.inner
    }
}

impl Default for CMainArgsBuilder {
    fn default() -> Self {
        Self::new()
    }
}

impl CMainArgsBuilder {
    /// Constructor
    #[must_use]
    pub fn new() -> Self {
        Self {
            inner: StdTargetArgsInner::default(),
        }
    }

    /// Build it
    pub fn build(&self) -> Result<CMainArgs, Error> {
        let mut argv: Vec<Pin<Box<CString>>> = Vec::new();

        if let Some(program) = &self.inner().program {
            argv.push(Box::pin(CString::new(program.as_bytes()).unwrap()));
        } else {
            return Err(Error::illegal_argument("Program not specified"));
        }

        for args in &self.inner().arguments {
            argv.push(Box::pin(CString::new(args.as_bytes()).unwrap()));
        }

        let mut argv_ptr: Vec<*const c_char> = argv.iter().map(|arg| arg.as_ptr()).collect();
        argv_ptr.push(core::ptr::null());

        Ok(CMainArgs {
            use_stdin: self.use_stdin(),
            argv,
            argv_ptr,
        })
    }
}

/// For creating an C-compatible argument
#[derive(Debug)]
#[allow(dead_code)]
pub struct CMainArgs {
    use_stdin: bool,
    /// This guys have to sit here, else Rust will free them
    argv: Vec<Pin<Box<CString>>>,
    argv_ptr: Vec<*const c_char>,
}

// From https://gist.github.com/TrinityCoder/793c097b5a4ab25b8fabf5cd67e92f05
impl CMainArgs {
    /// If stdin is used for this or no
    #[must_use]
    pub fn use_stdin(&self) -> bool {
        self.use_stdin
    }

    /// Returns the C language's `argv` (`*const *const c_char`).
    #[must_use]
    pub fn argv(&self) -> *const *const c_char {
        // println!("{:#?}", self.argv_ptr);
        self.argv_ptr.as_ptr()
    }

    /// Returns the C language's `argv[0]` (`*const c_char`).
    /// On x64 you would pass this to Rsi before starting emulation
    /// Like: `qemu.write_reg(Regs::Rsi, main_args.argv() as u64).unwrap();`
    #[must_use]
    pub fn argv0(&self) -> *const c_char {
        self.argv_ptr[0]
    }

    /// Gets total number of args.
    /// On x64 you would pass this to Rdi before starting emulation
    /// Like: `qemu.write_reg(Regs::Rdi, main_args.argc() as u64).unwrap();`
    #[must_use]
    pub fn argc(&self) -> c_int {
        (self.argv_ptr.len() - 1).try_into().unwrap()
    }
}
