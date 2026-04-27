use crate::{
    iter::{IntoIterKind, IsIteratorKind},
    string::{self, str_from, str_up_to},
};

use konst_macro_rules::iterator_shared;

/// Const equivalent of [`str::split_terminator`], which only takes a `&str` delimiter.
///
/// The same as [`split`](crate::string::split),
/// except that, if the string after the last delimiter is empty, it is skipped.
///
/// # Version compatibility
///
/// This requires the `"rust_1_64"` feature.
///
/// # Example
///
/// ```rust
/// use konst::string;
/// use konst::iter::for_each;
///
/// const STRS: &[&str] = &{
///     let mut arr = [""; 3];
///     for_each!{(i, sub) in string::split_terminator("foo,bar,baz,", ","),enumerate() =>
///         arr[i] = sub;
///     }
///     arr
/// };
///
/// assert_eq!(STRS, ["foo", "bar", "baz"]);
/// ```
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_64")))]
pub const fn split_terminator<'a, 'b>(this: &'a str, delim: &'b str) -> SplitTerminator<'a, 'b> {
    SplitTerminator {
        this,
        state: if delim.is_empty() {
            State::Empty(EmptyState::Start)
        } else {
            State::Normal { delim }
        },
    }
}

/// Const equivalent of [`str::rsplit_terminator`], which only takes a `&str` delimiter.
///
/// The same as [`rsplit`](crate::string::rsplit),
/// except that, if the string before the first delimiter is empty, it is skipped.
///
/// # Version compatibility
///
/// This requires the `"rust_1_64"` feature.
///
/// # Example
///
/// ```rust
/// use konst::string;
/// use konst::iter::for_each;
///
/// const STRS: &[&str] = &{
///     let mut arr = [""; 3];
///     for_each!{(i, sub) in string::rsplit_terminator(":foo:bar:baz", ":"),enumerate() =>
///         arr[i] = sub;
///     }
///     arr
/// };
///
/// assert_eq!(STRS, ["baz", "bar", "foo"]);
/// ```
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_64")))]
pub const fn rsplit_terminator<'a, 'b>(this: &'a str, delim: &'b str) -> RSplitTerminator<'a, 'b> {
    let SplitTerminator { this, state } = split_terminator(this, delim);
    RSplitTerminator { this, state }
}

#[derive(Copy, Clone)]
enum State<'a> {
    Normal { delim: &'a str },
    Empty(EmptyState),
}

#[derive(Copy, Clone)]
enum EmptyState {
    Start,
    Continue,
}

/// Const equivalent of `core::str::SplitTerminator<'a, &'b str>`
///
/// This is constructed with [`split_terminator`] like this:
/// ```rust
/// # let string = "";
/// # let delim = "";
/// # let _: konst::string::SplitTerminator<'_, '_> =
/// konst::string::split_terminator(string, delim)
/// # ;
/// ```
///
/// # Version compatibility
///
/// This requires the `"rust_1_64"` feature.
///
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_64")))]
pub struct SplitTerminator<'a, 'b> {
    this: &'a str,
    state: State<'b>,
}
impl IntoIterKind for SplitTerminator<'_, '_> {
    type Kind = IsIteratorKind;
}

impl<'a, 'b> SplitTerminator<'a, 'b> {
    iterator_shared! {
        is_forward = true,
        item = &'a str,
        iter_forward = SplitTerminator<'a, 'b>,
        next(self){
            let Self {
                this,
                state,
            } = self;

            match state {
                State::Empty(EmptyState::Start) => {
                    self.state = State::Empty(EmptyState::Continue);
                    Some(("", self))
                }
                _ if this.is_empty() => {
                    None
                }
                State::Normal{delim} => {
                    let (next, ret) = match string::find(this, delim, 0) {
                        Some(pos) => (pos + delim.len(), pos),
                        None => (this.len(), this.len()),
                    };
                    self.this = str_from(this, next);
                    Some((str_up_to(this, ret), self))
                }
                State::Empty(EmptyState::Continue) => {
                    let next_char = string::find_next_char_boundary(self.this.as_bytes(), 0);
                    let (next_char, rem) = string::split_at(self.this, next_char);
                    self.this = rem;
                    Some((next_char, self))
                }
            }
        },
        fields = {this, state},
    }

    /// Gets the remainder of the string.
    ///
    /// # Example
    ///
    /// ```rust
    /// let iter = konst::string::split_terminator("foo,bar,baz,", ",");
    /// assert_eq!(iter.remainder(), "foo,bar,baz,");
    ///
    /// let (elem, iter) = iter.next().unwrap();
    /// assert_eq!(elem, "foo");
    /// assert_eq!(iter.remainder(), "bar,baz,");
    ///
    /// let (elem, iter) = iter.next().unwrap();
    /// assert_eq!(elem, "bar");
    /// assert_eq!(iter.remainder(), "baz,");
    ///
    /// let (elem, iter) = iter.next().unwrap();
    /// assert_eq!(elem, "baz");
    /// assert_eq!(iter.remainder(), "");
    ///
    /// ```
    pub const fn remainder(&self) -> &'a str {
        self.this
    }
}

/// Const equivalent of `core::str::RSplitTerminator<'a, &'b str>`
///
/// This is constructed with [`rsplit_terminator`] like this:
/// ```rust
/// # let string = "";
/// # let delim = "";
/// # let _: konst::string::RSplitTerminator<'_, '_> =
/// konst::string::rsplit_terminator(string, delim)
/// # ;
/// ```
///
/// # Version compatibility
///
/// This requires the `"rust_1_64"` feature.
///
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_64")))]
pub struct RSplitTerminator<'a, 'b> {
    this: &'a str,
    state: State<'b>,
}
impl IntoIterKind for RSplitTerminator<'_, '_> {
    type Kind = IsIteratorKind;
}

impl<'a, 'b> RSplitTerminator<'a, 'b> {
    iterator_shared! {
        is_forward = true,
        item = &'a str,
        iter_forward = RSplitTerminator<'a, 'b>,
        next(self){
            let Self {
                this,
                state,
            } = self;

            match state {
                State::Empty(EmptyState::Start) => {
                    self.state = State::Empty(EmptyState::Continue);
                    Some(("", self))
                }
                _ if this.is_empty() => {
                    None
                }
                State::Normal{delim} => {
                    let (next, ret) = match string::rfind(this, delim, this.len()) {
                        Some(pos) => (pos, pos + delim.len()),
                        None => (0, 0),
                    };
                    self.this = str_up_to(this, next);
                    Some((str_from(this, ret), self))
                }
                State::Empty(EmptyState::Continue) => {
                    let bytes = self.this.as_bytes();
                    let next_char = string::find_prev_char_boundary(bytes, bytes.len());
                    let (rem, next_char) = string::split_at(self.this, next_char);
                    self.this = rem;
                    Some((next_char, self))
                }
            }
        },
        fields = {this, state},
    }

    /// Gets the remainder of the string.
    ///
    /// # Example
    ///
    /// ```rust
    /// let iter = konst::string::rsplit_terminator("=foo=bar=baz", "=");
    /// assert_eq!(iter.remainder(), "=foo=bar=baz");
    ///
    /// let (elem, iter) = iter.next().unwrap();
    /// assert_eq!(elem, "baz");
    /// assert_eq!(iter.remainder(), "=foo=bar");
    ///
    /// let (elem, iter) = iter.next().unwrap();
    /// assert_eq!(elem, "bar");
    /// assert_eq!(iter.remainder(), "=foo");
    ///
    /// let (elem, iter) = iter.next().unwrap();
    /// assert_eq!(elem, "foo");
    /// assert_eq!(iter.remainder(), "");
    ///
    /// ```
    pub const fn remainder(&self) -> &'a str {
        self.this
    }
}
