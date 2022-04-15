use super::error::ContextError;
use super::{Report, WrapErr};
use core::fmt::{self, Debug, Display, Write};

use std::error::Error as StdError;

use crate::{Diagnostic, LabeledSpan};

mod ext {
    use super::*;

    pub trait Diag {
        #[cfg_attr(track_caller, track_caller)]
        fn ext_report<D>(self, msg: D) -> Report
        where
            D: Display + Send + Sync + 'static;
    }

    impl<E> Diag for E
    where
        E: Diagnostic + Send + Sync + 'static,
    {
        fn ext_report<D>(self, msg: D) -> Report
        where
            D: Display + Send + Sync + 'static,
        {
            Report::from_msg(msg, self)
        }
    }

    impl Diag for Report {
        fn ext_report<D>(self, msg: D) -> Report
        where
            D: Display + Send + Sync + 'static,
        {
            self.wrap_err(msg)
        }
    }
}

impl<T, E> WrapErr<T, E> for Result<T, E>
where
    E: ext::Diag + Send + Sync + 'static,
{
    fn wrap_err<D>(self, msg: D) -> Result<T, Report>
    where
        D: Display + Send + Sync + 'static,
    {
        match self {
            Ok(t) => Ok(t),
            Err(e) => Err(e.ext_report(msg)),
        }
    }

    fn wrap_err_with<D, F>(self, msg: F) -> Result<T, Report>
    where
        D: Display + Send + Sync + 'static,
        F: FnOnce() -> D,
    {
        match self {
            Ok(t) => Ok(t),
            Err(e) => Err(e.ext_report(msg())),
        }
    }

    fn context<D>(self, msg: D) -> Result<T, Report>
    where
        D: Display + Send + Sync + 'static,
    {
        self.wrap_err(msg)
    }

    fn with_context<D, F>(self, msg: F) -> Result<T, Report>
    where
        D: Display + Send + Sync + 'static,
        F: FnOnce() -> D,
    {
        self.wrap_err_with(msg)
    }
}

impl<D, E> Debug for ContextError<D, E>
where
    D: Display,
    E: Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Error")
            .field("msg", &Quoted(&self.msg))
            .field("source", &self.error)
            .finish()
    }
}

impl<D, E> Display for ContextError<D, E>
where
    D: Display,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        Display::fmt(&self.msg, f)
    }
}

impl<D, E> StdError for ContextError<D, E>
where
    D: Display,
    E: StdError + 'static,
{
    fn source(&self) -> Option<&(dyn StdError + 'static)> {
        Some(&self.error)
    }
}

impl<D> StdError for ContextError<D, Report>
where
    D: Display,
{
    fn source(&self) -> Option<&(dyn StdError + 'static)> {
        Some(self.error.inner.error())
    }
}

impl<D, E> Diagnostic for ContextError<D, E>
where
    D: Display,
    E: Diagnostic + 'static,
{
    fn code<'a>(&'a self) -> Option<Box<dyn Display + 'a>> {
        self.error.code()
    }

    fn severity(&self) -> Option<crate::Severity> {
        self.error.severity()
    }

    fn help<'a>(&'a self) -> Option<Box<dyn Display + 'a>> {
        self.error.help()
    }

    fn url<'a>(&'a self) -> Option<Box<dyn Display + 'a>> {
        self.error.url()
    }

    fn labels<'a>(&'a self) -> Option<Box<dyn Iterator<Item = LabeledSpan> + 'a>> {
        self.error.labels()
    }

    fn source_code(&self) -> Option<&dyn crate::SourceCode> {
        self.error.source_code()
    }

    fn related<'a>(&'a self) -> Option<Box<dyn Iterator<Item = &'a dyn Diagnostic> + 'a>> {
        self.error.related()
    }
}

impl<D> Diagnostic for ContextError<D, Report>
where
    D: Display,
{
    fn code<'a>(&'a self) -> Option<Box<dyn Display + 'a>> {
        self.error.inner.diagnostic().code()
    }

    fn severity(&self) -> Option<crate::Severity> {
        self.error.inner.diagnostic().severity()
    }

    fn help<'a>(&'a self) -> Option<Box<dyn Display + 'a>> {
        self.error.inner.diagnostic().help()
    }

    fn url<'a>(&'a self) -> Option<Box<dyn Display + 'a>> {
        self.error.inner.diagnostic().url()
    }

    fn labels<'a>(&'a self) -> Option<Box<dyn Iterator<Item = LabeledSpan> + 'a>> {
        self.error.inner.diagnostic().labels()
    }

    fn source_code(&self) -> Option<&dyn crate::SourceCode> {
        self.error.source_code()
    }

    fn related<'a>(&'a self) -> Option<Box<dyn Iterator<Item = &'a dyn Diagnostic> + 'a>> {
        self.error.related()
    }
}

struct Quoted<D>(D);

impl<D> Debug for Quoted<D>
where
    D: Display,
{
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter.write_char('"')?;
        Quoted(&mut *formatter).write_fmt(format_args!("{}", self.0))?;
        formatter.write_char('"')?;
        Ok(())
    }
}

impl Write for Quoted<&mut fmt::Formatter<'_>> {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        Display::fmt(&s.escape_debug(), self.0)
    }
}

pub(crate) mod private {
    use super::*;

    pub trait Sealed {}

    impl<T, E> Sealed for Result<T, E> where E: ext::Diag {}
    impl<T> Sealed for Option<T> {}
}
