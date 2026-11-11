use std::borrow::BorrowMut;

#[cfg(feature = "parsed-types")]
use crate::{Item, ListEntry};
use crate::{KeyRef, RefBareItem, serializer};

/// Serializes `Item` field value components incrementally.
///
/// Note: The serialization conforms to [RFC 9651], meaning that
/// [`Dates`][crate::Date] and [`Display Strings`][RefBareItem::DisplayString],
/// which cause parsing errors under [RFC 8941], will be serialized
/// unconditionally. The consumer of this API is responsible for determining
/// whether it is valid to serialize these bare items for any specific field.
///
/// [RFC 8941]: <https://httpwg.org/specs/rfc8941.html>
/// [RFC 9651]: <https://httpwg.org/specs/rfc9651.html>
/// ```
/// use sfv::{KeyRef, ItemSerializer};
///
/// # fn main() -> Result<(), sfv::Error> {
/// let serialized_item = ItemSerializer::new()
///     .bare_item(11)
///     .parameter(KeyRef::from_str("foo")?, true)
///     .finish();
///
/// assert_eq!(serialized_item, "11;foo");
/// # Ok(())
/// # }
/// ```
// https://httpwg.org/specs/rfc9651.html#ser-item
#[derive(Debug)]
#[must_use]
pub struct ItemSerializer<W> {
    buffer: W,
}

impl Default for ItemSerializer<String> {
    fn default() -> Self {
        Self::new()
    }
}

impl ItemSerializer<String> {
    /// Creates a serializer that writes into a new string.
    pub fn new() -> Self {
        Self {
            buffer: String::new(),
        }
    }
}

impl<'a> ItemSerializer<&'a mut String> {
    /// Creates a serializer that writes into the given string.
    pub fn with_buffer(buffer: &'a mut String) -> Self {
        Self { buffer }
    }
}

impl<W: BorrowMut<String>> ItemSerializer<W> {
    /// Serializes the given bare item.
    ///
    /// Returns a serializer for the item's parameters.
    pub fn bare_item<'b>(
        mut self,
        bare_item: impl Into<RefBareItem<'b>>,
    ) -> ParameterSerializer<W> {
        serializer::serialize_bare_item(bare_item, self.buffer.borrow_mut());
        ParameterSerializer {
            buffer: self.buffer,
        }
    }
}

/// Serializes parameters incrementally.
#[derive(Debug)]
pub struct ParameterSerializer<W> {
    buffer: W,
}

impl<W: BorrowMut<String>> ParameterSerializer<W> {
    /// Serializes a parameter with the given name and value.
    ///
    /// Returns the serializer.
    #[allow(clippy::return_self_not_must_use)]
    pub fn parameter<'b>(mut self, name: &KeyRef, value: impl Into<RefBareItem<'b>>) -> Self {
        serializer::serialize_parameter(name, value, self.buffer.borrow_mut());
        self
    }

    /// Serializes the given parameters.
    ///
    /// Returns the serializer.
    #[allow(clippy::return_self_not_must_use)]
    pub fn parameters<'b>(
        mut self,
        params: impl IntoIterator<Item = (impl AsRef<KeyRef>, impl Into<RefBareItem<'b>>)>,
    ) -> Self {
        for (name, value) in params {
            serializer::serialize_parameter(name.as_ref(), value, self.buffer.borrow_mut());
        }
        self
    }

    /// Finishes parameter serialization and returns the serializer's output.
    #[must_use]
    pub fn finish(self) -> W {
        self.buffer
    }
}

fn maybe_write_separator(buffer: &mut String, first: &mut bool) {
    if *first {
        *first = false;
    } else {
        buffer.push_str(", ");
    }
}

/// Serializes `List` field value components incrementally.
///
/// Note: The serialization conforms to [RFC 9651], meaning that
/// [`Dates`][crate::Date] and [`Display Strings`][RefBareItem::DisplayString],
/// which cause parsing errors under [RFC 8941], will be serialized
/// unconditionally. The consumer of this API is responsible for determining
/// whether it is valid to serialize these bare items for any specific field.
///
/// [RFC 8941]: <https://httpwg.org/specs/rfc8941.html>
/// [RFC 9651]: <https://httpwg.org/specs/rfc9651.html>
/// ```
/// use sfv::{KeyRef, StringRef, TokenRef, ListSerializer};
///
/// # fn main() -> Result<(), sfv::Error> {
/// let mut ser = ListSerializer::new();
///
/// ser.bare_item(11)
///     .parameter(KeyRef::from_str("foo")?, true);
///
/// {
///     let mut ser = ser.inner_list();
///
///     ser.bare_item(TokenRef::from_str("abc")?)
///         .parameter(KeyRef::from_str("abc_param")?, false);
///
///     ser.bare_item(TokenRef::from_str("def")?);
///
///     ser.finish()
///         .parameter(KeyRef::from_str("bar")?, StringRef::from_str("val")?);
/// }
///
/// assert_eq!(
///     ser.finish().as_deref(),
///     Some(r#"11;foo, (abc;abc_param=?0 def);bar="val""#),
/// );
/// # Ok(())
/// # }
/// ```
// https://httpwg.org/specs/rfc9651.html#ser-list
#[derive(Debug)]
#[must_use]
pub struct ListSerializer<W> {
    buffer: W,
    first: bool,
}

impl Default for ListSerializer<String> {
    fn default() -> Self {
        Self::new()
    }
}

impl ListSerializer<String> {
    /// Creates a serializer that writes into a new string.
    pub fn new() -> Self {
        Self {
            buffer: String::new(),
            first: true,
        }
    }
}

impl<'a> ListSerializer<&'a mut String> {
    /// Creates a serializer that writes into the given string.
    pub fn with_buffer(buffer: &'a mut String) -> Self {
        Self {
            buffer,
            first: true,
        }
    }
}

impl<W: BorrowMut<String>> ListSerializer<W> {
    /// Serializes the given bare item as a member of the list.
    ///
    /// Returns a serializer for the item's parameters.
    pub fn bare_item<'b>(
        &mut self,
        bare_item: impl Into<RefBareItem<'b>>,
    ) -> ParameterSerializer<&mut String> {
        let buffer = self.buffer.borrow_mut();
        maybe_write_separator(buffer, &mut self.first);
        serializer::serialize_bare_item(bare_item, buffer);
        ParameterSerializer { buffer }
    }

    /// Opens an inner list, returning a serializer to be used for its items and
    /// parameters.
    pub fn inner_list(&mut self) -> InnerListSerializer<'_> {
        let buffer = self.buffer.borrow_mut();
        maybe_write_separator(buffer, &mut self.first);
        buffer.push('(');
        InnerListSerializer {
            buffer: Some(buffer),
        }
    }

    /// Serializes the given members of the list.
    #[cfg(feature = "parsed-types")]
    pub fn members<'b>(&mut self, members: impl IntoIterator<Item = &'b ListEntry>) {
        for value in members {
            match value {
                ListEntry::Item(value) => {
                    self.bare_item(&value.bare_item).parameters(&value.params);
                }
                ListEntry::InnerList(value) => {
                    let mut ser = self.inner_list();
                    ser.items(&value.items);
                    ser.finish().parameters(&value.params);
                }
            }
        }
    }

    /// Finishes serialization of the list and returns the underlying output.
    ///
    /// Returns `None` if and only if no members were serialized, as [empty
    /// lists are not meant to be serialized at
    /// all](https://httpwg.org/specs/rfc9651.html#text-serialize).
    #[must_use]
    pub fn finish(self) -> Option<W> {
        if self.first { None } else { Some(self.buffer) }
    }
}

/// Serializes `Dictionary` field value components incrementally.
///
/// Note: The serialization conforms to [RFC 9651], meaning that
/// [`Dates`][crate::Date] and [`Display Strings`][RefBareItem::DisplayString],
/// which cause parsing errors under [RFC 8941], will be serialized
/// unconditionally. The consumer of this API is responsible for determining
/// whether it is valid to serialize these bare items for any specific field.
///
/// [RFC 8941]: <https://httpwg.org/specs/rfc8941.html>
/// [RFC 9651]: <https://httpwg.org/specs/rfc9651.html>
///
/// ```
/// use sfv::{KeyRef, StringRef, TokenRef, DictSerializer, Decimal};
///
/// # fn main() -> Result<(), sfv::Error> {
/// let mut ser = DictSerializer::new();
///
/// ser.bare_item(KeyRef::from_str("member1")?, 11)
///     .parameter(KeyRef::from_str("foo")?, true);
///
/// {
///   let mut ser = ser.inner_list(KeyRef::from_str("member2")?);
///
///   ser.bare_item(TokenRef::from_str("abc")?)
///       .parameter(KeyRef::from_str("abc_param")?, false);
///
///   ser.bare_item(TokenRef::from_str("def")?);
///
///   ser.finish()
///      .parameter(KeyRef::from_str("bar")?, StringRef::from_str("val")?);
/// }
///
/// ser.bare_item(KeyRef::from_str("member3")?, Decimal::try_from(12.34566)?);
///
/// assert_eq!(
///     ser.finish().as_deref(),
///     Some(r#"member1=11;foo, member2=(abc;abc_param=?0 def);bar="val", member3=12.346"#),
/// );
/// # Ok(())
/// # }
/// ```
// https://httpwg.org/specs/rfc9651.html#ser-dictionary
#[derive(Debug)]
#[must_use]
pub struct DictSerializer<W> {
    buffer: W,
    first: bool,
}

impl Default for DictSerializer<String> {
    fn default() -> Self {
        Self::new()
    }
}

impl DictSerializer<String> {
    /// Creates a serializer that writes into a new string.
    pub fn new() -> Self {
        Self {
            buffer: String::new(),
            first: true,
        }
    }
}

impl<'a> DictSerializer<&'a mut String> {
    /// Creates a serializer that writes into the given string.
    pub fn with_buffer(buffer: &'a mut String) -> Self {
        Self {
            buffer,
            first: true,
        }
    }
}

impl<W: BorrowMut<String>> DictSerializer<W> {
    /// Serializes the given bare item as a member of the dictionary with the
    /// given key.
    ///
    /// Returns a serializer for the item's parameters.
    pub fn bare_item<'b>(
        &mut self,
        name: &KeyRef,
        value: impl Into<RefBareItem<'b>>,
    ) -> ParameterSerializer<&mut String> {
        let buffer = self.buffer.borrow_mut();
        maybe_write_separator(buffer, &mut self.first);
        serializer::serialize_key(name, buffer);
        let value = value.into();
        if value != RefBareItem::Boolean(true) {
            buffer.push('=');
            serializer::serialize_bare_item(value, buffer);
        }
        ParameterSerializer { buffer }
    }

    /// Opens an inner list with the given key, returning a serializer to be
    /// used for its items and parameters.
    pub fn inner_list<'a>(&'a mut self, name: &KeyRef) -> InnerListSerializer<'a> {
        let buffer = self.buffer.borrow_mut();
        maybe_write_separator(buffer, &mut self.first);
        serializer::serialize_key(name, buffer);
        buffer.push_str("=(");
        InnerListSerializer {
            buffer: Some(buffer),
        }
    }

    /// Serializes the given members of the dictionary.
    #[cfg(feature = "parsed-types")]
    pub fn members<'b>(
        &mut self,
        members: impl IntoIterator<Item = (impl AsRef<KeyRef>, &'b ListEntry)>,
    ) {
        for (name, value) in members {
            match value {
                ListEntry::Item(value) => {
                    self.bare_item(name.as_ref(), &value.bare_item)
                        .parameters(&value.params);
                }
                ListEntry::InnerList(value) => {
                    let mut ser = self.inner_list(name.as_ref());
                    ser.items(&value.items);
                    ser.finish().parameters(&value.params);
                }
            }
        }
    }

    /// Finishes serialization of the dictionary and returns the underlying output.
    ///
    /// Returns `None` if and only if no members were serialized, as [empty
    /// dictionaries are not meant to be serialized at
    /// all](https://httpwg.org/specs/rfc9651.html#text-serialize).
    #[must_use]
    pub fn finish(self) -> Option<W> {
        if self.first { None } else { Some(self.buffer) }
    }
}

/// Serializes inner lists incrementally.
///
/// The inner list will be closed automatically when the serializer is dropped.
/// To set the inner list's parameters, call [`InnerListSerializer::finish`].
///
/// Failing to drop the serializer or call its `finish` method will result in
/// an invalid serialization that lacks a closing `)` character.
// https://httpwg.org/specs/rfc9651.html#ser-innerlist
#[derive(Debug)]
#[must_use]
pub struct InnerListSerializer<'a> {
    buffer: Option<&'a mut String>,
}

impl Drop for InnerListSerializer<'_> {
    fn drop(&mut self) {
        if let Some(ref mut buffer) = self.buffer {
            buffer.push(')');
        }
    }
}

impl<'a> InnerListSerializer<'a> {
    /// Serializes the given bare item as a member of the inner list.
    ///
    /// Returns a serializer for the item's parameters.
    #[allow(clippy::missing_panics_doc)] // The unwrap is safe by construction.
    pub fn bare_item<'b>(
        &mut self,
        bare_item: impl Into<RefBareItem<'b>>,
    ) -> ParameterSerializer<&mut String> {
        let buffer = self.buffer.as_mut().unwrap();
        if !buffer.is_empty() && !buffer.ends_with('(') {
            buffer.push(' ');
        }
        serializer::serialize_bare_item(bare_item, buffer);
        ParameterSerializer { buffer }
    }

    /// Serializes the given items as members of the inner list.
    #[cfg(feature = "parsed-types")]
    pub fn items<'b>(&mut self, items: impl IntoIterator<Item = &'b Item>) {
        for item in items {
            self.bare_item(&item.bare_item).parameters(&item.params);
        }
    }

    /// Closes the inner list and returns a serializer for its parameters.
    #[must_use]
    #[allow(clippy::missing_panics_doc)]
    pub fn finish(mut self) -> ParameterSerializer<&'a mut String> {
        let buffer = self.buffer.take().unwrap();
        buffer.push(')');
        ParameterSerializer { buffer }
    }
}
