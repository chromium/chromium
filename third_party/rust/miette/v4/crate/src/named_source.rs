use crate::{MietteError, MietteSpanContents, SourceCode, SpanContents};

/// Utility struct for when you have a regular [`SourceCode`] type that doesn't
/// implement `name`. For example [`String`]. Or if you want to override the
/// `name` returned by the `SourceCode`.
pub struct NamedSource {
    source: Box<dyn SourceCode + 'static>,
    name: String,
}

impl std::fmt::Debug for NamedSource {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("NamedSource")
            .field("name", &self.name)
            .field("source", &"<redacted>");
        Ok(())
    }
}

impl NamedSource {
    /// Create a new `NamedSource` using a regular [`SourceCode`] and giving
    /// its returned [`SpanContents`] a name.
    pub fn new(name: impl AsRef<str>, source: impl SourceCode + Send + Sync + 'static) -> Self {
        Self {
            source: Box::new(source),
            name: name.as_ref().to_string(),
        }
    }

    /// Returns a reference the inner [`SourceCode`] type for this
    /// `NamedSource`.
    pub fn inner(&self) -> &(dyn SourceCode + 'static) {
        &*self.source
    }
}

impl SourceCode for NamedSource {
    fn read_span<'a>(
        &'a self,
        span: &crate::SourceSpan,
        context_lines_before: usize,
        context_lines_after: usize,
    ) -> Result<Box<dyn SpanContents<'a> + 'a>, MietteError> {
        let contents = self
            .inner()
            .read_span(span, context_lines_before, context_lines_after)?;
        Ok(Box::new(MietteSpanContents::new_named(
            self.name.clone(),
            contents.data(),
            contents.span().clone(),
            contents.line(),
            contents.column(),
            contents.line_count(),
        )))
    }
}
