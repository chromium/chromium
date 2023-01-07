use core::fmt::{self, Debug, Display};

use std::error::Error as StdError;

use crate::{Diagnostic, LabeledSpan, Report, SourceCode};

use crate as miette;

#[repr(transparent)]
pub(crate) struct DisplayError<M>(pub(crate) M);

#[repr(transparent)]
pub(crate) struct MessageError<M>(pub(crate) M);

pub(crate) struct NoneError;

impl<M> Debug for DisplayError<M>
where
    M: Display,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        Display::fmt(&self.0, f)
    }
}

impl<M> Display for DisplayError<M>
where
    M: Display,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        Display::fmt(&self.0, f)
    }
}

impl<M> StdError for DisplayError<M> where M: Display + 'static {}
impl<M> Diagnostic for DisplayError<M> where M: Display + 'static {}

impl<M> Debug for MessageError<M>
where
    M: Display + Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        Debug::fmt(&self.0, f)
    }
}

impl<M> Display for MessageError<M>
where
    M: Display + Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        Display::fmt(&self.0, f)
    }
}

impl<M> StdError for MessageError<M> where M: Display + Debug + 'static {}
impl<M> Diagnostic for MessageError<M> where M: Display + Debug + 'static {}

impl Debug for NoneError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        Debug::fmt("Option was None", f)
    }
}

impl Display for NoneError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        Display::fmt("Option was None", f)
    }
}

impl StdError for NoneError {}
impl Diagnostic for NoneError {}

#[repr(transparent)]
pub(crate) struct BoxedError(pub(crate) Box<dyn Diagnostic + Send + Sync>);

impl Diagnostic for BoxedError {
    fn code<'a>(&'a self) -> Option<Box<dyn Display + 'a>> {
        self.0.code()
    }

    fn severity(&self) -> Option<miette::Severity> {
        self.0.severity()
    }

    fn help<'a>(&'a self) -> Option<Box<dyn Display + 'a>> {
        self.0.help()
    }

    fn url<'a>(&'a self) -> Option<Box<dyn Display + 'a>> {
        self.0.url()
    }

    fn labels<'a>(&'a self) -> Option<Box<dyn Iterator<Item = LabeledSpan> + 'a>> {
        self.0.labels()
    }

    fn source_code(&self) -> Option<&dyn miette::SourceCode> {
        self.0.source_code()
    }

    fn related<'a>(&'a self) -> Option<Box<dyn Iterator<Item = &'a dyn Diagnostic> + 'a>> {
        self.0.related()
    }
}

impl Debug for BoxedError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        Debug::fmt(&self.0, f)
    }
}

impl Display for BoxedError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        Display::fmt(&self.0, f)
    }
}

impl StdError for BoxedError {}

pub(crate) struct WithSourceCode<E, C> {
    pub(crate) error: E,
    pub(crate) source_code: C,
}

impl<E: Diagnostic, C: SourceCode> Diagnostic for WithSourceCode<E, C> {
    fn code<'a>(&'a self) -> Option<Box<dyn Display + 'a>> {
        self.error.code()
    }

    fn severity(&self) -> Option<miette::Severity> {
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

    fn source_code(&self) -> Option<&dyn miette::SourceCode> {
        Some(&self.source_code)
    }

    fn related<'a>(&'a self) -> Option<Box<dyn Iterator<Item = &'a dyn Diagnostic> + 'a>> {
        self.error.related()
    }
}

impl<C: SourceCode> Diagnostic for WithSourceCode<Report, C> {
    fn code<'a>(&'a self) -> Option<Box<dyn Display + 'a>> {
        self.error.code()
    }

    fn severity(&self) -> Option<miette::Severity> {
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

    fn source_code(&self) -> Option<&dyn miette::SourceCode> {
        Some(&self.source_code)
    }

    fn related<'a>(&'a self) -> Option<Box<dyn Iterator<Item = &'a dyn Diagnostic> + 'a>> {
        self.error.related()
    }
}

impl<E: Debug, C> Debug for WithSourceCode<E, C> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        Debug::fmt(&self.error, f)
    }
}

impl<E: Display, C> Display for WithSourceCode<E, C> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        Display::fmt(&self.error, f)
    }
}

impl<E: StdError, C> StdError for WithSourceCode<E, C> {
    fn source(&self) -> Option<&(dyn StdError + 'static)> {
        self.error.source()
    }
}

impl<C> StdError for WithSourceCode<Report, C> {
    fn source(&self) -> Option<&(dyn StdError + 'static)> {
        self.error.source()
    }
}
