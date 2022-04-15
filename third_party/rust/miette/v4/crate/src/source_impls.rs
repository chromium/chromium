/*!
Default trait implementations for [`SourceCode`].
*/
use std::{
    borrow::{Cow, ToOwned},
    fmt::Debug,
    sync::Arc,
};

use crate::{MietteError, MietteSpanContents, SourceCode, SourceSpan, SpanContents};

fn context_info<'a>(
    input: &'a [u8],
    span: &SourceSpan,
    context_lines_before: usize,
    context_lines_after: usize,
) -> Result<MietteSpanContents<'a>, MietteError> {
    let mut offset = 0usize;
    let mut line_count = 0usize;
    let mut start_line = 0usize;
    let mut start_column = 0usize;
    let mut before_lines_starts = Vec::new();
    let mut current_line_start = 0usize;
    let mut end_lines = 0usize;
    let mut post_span = false;
    let mut post_span_got_newline = false;
    let mut iter = input.iter().copied().peekable();
    while let Some(char) = iter.next() {
        if matches!(char, b'\r' | b'\n') {
            line_count += 1;
            if char == b'\r' && iter.next_if_eq(&b'\n').is_some() {
                offset += 1;
            }
            if offset < span.offset() {
                // We're before the start of the span.
                start_column = 0;
                before_lines_starts.push(current_line_start);
                if before_lines_starts.len() > context_lines_before {
                    start_line += 1;
                    before_lines_starts.remove(0);
                }
            } else if offset >= span.offset() + span.len().saturating_sub(1) {
                // We're after the end of the span, but haven't necessarily
                // started collecting end lines yet (we might still be
                // collecting context lines).
                if post_span {
                    start_column = 0;
                    if post_span_got_newline {
                        end_lines += 1;
                    } else {
                        post_span_got_newline = true;
                    }
                    if end_lines >= context_lines_after {
                        offset += 1;
                        break;
                    }
                }
            }
            current_line_start = offset + 1;
        } else if offset < span.offset() {
            start_column += 1;
        }

        if offset >= (span.offset() + span.len()).saturating_sub(1) {
            post_span = true;
            if end_lines >= context_lines_after {
                offset += 1;
                break;
            }
        }

        offset += 1;
    }

    if offset >= (span.offset() + span.len()).saturating_sub(1) {
        let starting_offset = before_lines_starts.get(0).copied().unwrap_or_else(|| {
            if context_lines_before == 0 {
                span.offset()
            } else {
                0
            }
        });
        Ok(MietteSpanContents::new(
            &input[starting_offset..offset],
            (starting_offset, offset - starting_offset).into(),
            start_line,
            if context_lines_before == 0 {
                start_column
            } else {
                0
            },
            line_count,
        ))
    } else {
        Err(MietteError::OutOfBounds)
    }
}

impl SourceCode for [u8] {
    fn read_span<'a>(
        &'a self,
        span: &SourceSpan,
        context_lines_before: usize,
        context_lines_after: usize,
    ) -> Result<Box<dyn SpanContents<'a> + 'a>, MietteError> {
        let contents = context_info(self, span, context_lines_before, context_lines_after)?;
        Ok(Box::new(contents))
    }
}

impl<'src> SourceCode for &'src [u8] {
    fn read_span<'a>(
        &'a self,
        span: &SourceSpan,
        context_lines_before: usize,
        context_lines_after: usize,
    ) -> Result<Box<dyn SpanContents<'a> + 'a>, MietteError> {
        <[u8] as SourceCode>::read_span(self, span, context_lines_before, context_lines_after)
    }
}

impl SourceCode for str {
    fn read_span<'a>(
        &'a self,
        span: &SourceSpan,
        context_lines_before: usize,
        context_lines_after: usize,
    ) -> Result<Box<dyn SpanContents<'a> + 'a>, MietteError> {
        <[u8] as SourceCode>::read_span(
            self.as_bytes(),
            span,
            context_lines_before,
            context_lines_after,
        )
    }
}

/// Makes `src: &'static str` or `struct S<'a> { src: &'a str }` usable.
impl<'s> SourceCode for &'s str {
    fn read_span<'a>(
        &'a self,
        span: &SourceSpan,
        context_lines_before: usize,
        context_lines_after: usize,
    ) -> Result<Box<dyn SpanContents<'a> + 'a>, MietteError> {
        <str as SourceCode>::read_span(self, span, context_lines_before, context_lines_after)
    }
}

impl SourceCode for String {
    fn read_span<'a>(
        &'a self,
        span: &SourceSpan,
        context_lines_before: usize,
        context_lines_after: usize,
    ) -> Result<Box<dyn SpanContents<'a> + 'a>, MietteError> {
        <str as SourceCode>::read_span(self, span, context_lines_before, context_lines_after)
    }
}

impl<T: SourceCode> SourceCode for Arc<T> {
    fn read_span<'a>(
        &'a self,
        span: &SourceSpan,
        context_lines_before: usize,
        context_lines_after: usize,
    ) -> Result<Box<dyn SpanContents<'a> + 'a>, MietteError> {
        self.as_ref()
            .read_span(span, context_lines_before, context_lines_after)
    }
}

impl<T: ?Sized + SourceCode + ToOwned> SourceCode for Cow<'_, T>
where
    // The minimal bounds are used here.
    // `T::Owned` need not be
    // `SourceCode`, because `&T`
    // can always be obtained from
    // `Cow<'_, T>`.
    T::Owned: Debug + Send + Sync,
{
    fn read_span<'a>(
        &'a self,
        span: &SourceSpan,
        context_lines_before: usize,
        context_lines_after: usize,
    ) -> Result<Box<dyn SpanContents<'a> + 'a>, MietteError> {
        self.as_ref()
            .read_span(span, context_lines_before, context_lines_after)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn basic() -> Result<(), MietteError> {
        let src = String::from("foo\n");
        let contents = src.read_span(&(0, 4).into(), 0, 0)?;
        assert_eq!("foo\n", std::str::from_utf8(contents.data()).unwrap());
        assert_eq!(0, contents.line());
        assert_eq!(0, contents.column());
        Ok(())
    }

    #[test]
    fn shifted() -> Result<(), MietteError> {
        let src = String::from("foobar");
        let contents = src.read_span(&(3, 3).into(), 1, 1)?;
        assert_eq!("foobar", std::str::from_utf8(contents.data()).unwrap());
        assert_eq!(0, contents.line());
        assert_eq!(0, contents.column());
        Ok(())
    }

    #[test]
    fn middle() -> Result<(), MietteError> {
        let src = String::from("foo\nbar\nbaz\n");
        let contents = src.read_span(&(4, 4).into(), 0, 0)?;
        assert_eq!("bar\n", std::str::from_utf8(contents.data()).unwrap());
        assert_eq!(1, contents.line());
        assert_eq!(0, contents.column());
        Ok(())
    }

    #[test]
    fn middle_of_line() -> Result<(), MietteError> {
        let src = String::from("foo\nbarbar\nbaz\n");
        let contents = src.read_span(&(7, 4).into(), 0, 0)?;
        assert_eq!("bar\n", std::str::from_utf8(contents.data()).unwrap());
        assert_eq!(1, contents.line());
        assert_eq!(3, contents.column());
        Ok(())
    }

    #[test]
    fn with_crlf() -> Result<(), MietteError> {
        let src = String::from("foo\r\nbar\r\nbaz\r\n");
        let contents = src.read_span(&(5, 5).into(), 0, 0)?;
        assert_eq!("bar\r\n", std::str::from_utf8(contents.data()).unwrap());
        assert_eq!(1, contents.line());
        assert_eq!(0, contents.column());
        Ok(())
    }

    #[test]
    fn with_context() -> Result<(), MietteError> {
        let src = String::from("xxx\nfoo\nbar\nbaz\n\nyyy\n");
        let contents = src.read_span(&(8, 3).into(), 1, 1)?;
        assert_eq!(
            "foo\nbar\nbaz\n",
            std::str::from_utf8(contents.data()).unwrap()
        );
        assert_eq!(1, contents.line());
        assert_eq!(0, contents.column());
        Ok(())
    }

    #[test]
    fn multiline_with_context() -> Result<(), MietteError> {
        let src = String::from("aaa\nxxx\n\nfoo\nbar\nbaz\n\nyyy\nbbb\n");
        let contents = src.read_span(&(9, 11).into(), 1, 1)?;
        assert_eq!(
            "\nfoo\nbar\nbaz\n\n",
            std::str::from_utf8(contents.data()).unwrap()
        );
        assert_eq!(2, contents.line());
        assert_eq!(0, contents.column());
        let span: SourceSpan = (8, 14).into();
        assert_eq!(&span, contents.span());
        Ok(())
    }

    #[test]
    fn multiline_with_context_line_start() -> Result<(), MietteError> {
        let src = String::from("one\ntwo\n\nthree\nfour\nfive\n\nsix\nseven\n");
        let contents = src.read_span(&(2, 0).into(), 2, 2)?;
        assert_eq!(
            "one\ntwo\n\n",
            std::str::from_utf8(contents.data()).unwrap()
        );
        assert_eq!(0, contents.line());
        assert_eq!(0, contents.column());
        let span: SourceSpan = (0, 9).into();
        assert_eq!(&span, contents.span());
        Ok(())
    }
}
