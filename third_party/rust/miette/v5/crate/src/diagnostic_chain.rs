/*!
Iterate over error `.diagnostic_source()` chains.
*/

use crate::protocol::Diagnostic;

/// Iterator of a chain of cause errors.
#[derive(Clone, Default)]
#[allow(missing_debug_implementations)]
pub(crate) struct DiagnosticChain<'a> {
    state: Option<ErrorKind<'a>>,
}

impl<'a> DiagnosticChain<'a> {
    pub(crate) fn from_diagnostic(head: &'a dyn Diagnostic) -> Self {
        DiagnosticChain {
            state: Some(ErrorKind::Diagnostic(head)),
        }
    }

    pub(crate) fn from_stderror(head: &'a (dyn std::error::Error + 'static)) -> Self {
        DiagnosticChain {
            state: Some(ErrorKind::StdError(head)),
        }
    }
}

impl<'a> Iterator for DiagnosticChain<'a> {
    type Item = ErrorKind<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        if let Some(err) = self.state.take() {
            self.state = err.get_nested();
            Some(err)
        } else {
            None
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let len = self.len();
        (len, Some(len))
    }
}

impl ExactSizeIterator for DiagnosticChain<'_> {
    fn len(&self) -> usize {
        fn depth(d: Option<&ErrorKind<'_>>) -> usize {
            match d {
                Some(d) => 1 + depth(d.get_nested().as_ref()),
                None => 0,
            }
        }

        depth(self.state.as_ref())
    }
}

#[derive(Clone)]
pub(crate) enum ErrorKind<'a> {
    Diagnostic(&'a dyn Diagnostic),
    StdError(&'a (dyn std::error::Error + 'static)),
}

impl<'a> ErrorKind<'a> {
    fn get_nested(&self) -> Option<ErrorKind<'a>> {
        match self {
            ErrorKind::Diagnostic(d) => d
                .diagnostic_source()
                .map(ErrorKind::Diagnostic)
                .or_else(|| d.source().map(ErrorKind::StdError)),
            ErrorKind::StdError(e) => e.source().map(ErrorKind::StdError),
        }
    }
}

impl<'a> std::fmt::Debug for ErrorKind<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ErrorKind::Diagnostic(d) => d.fmt(f),
            ErrorKind::StdError(e) => e.fmt(f),
        }
    }
}

impl<'a> std::fmt::Display for ErrorKind<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ErrorKind::Diagnostic(d) => d.fmt(f),
            ErrorKind::StdError(e) => e.fmt(f),
        }
    }
}
