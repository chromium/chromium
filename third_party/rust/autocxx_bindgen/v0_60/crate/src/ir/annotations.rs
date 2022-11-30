//! Types and functions related to bindgen annotation comments.
//!
//! Users can add annotations in doc comments to types that they would like to
//! replace other types with, mark as opaque, etc. This module deals with all of
//! that stuff.

use crate::clang;

/// What kind of accessor should we provide for a field?
#[derive(Copy, PartialEq, Clone, Debug)]
pub enum FieldAccessorKind {
    /// No accessor.
    None,
    /// Plain accessor.
    Regular,
    /// Unsafe accessor.
    Unsafe,
    /// Immutable accessor.
    Immutable,
}

/// Annotations for a given item, or a field.
///
/// You can see the kind of comments that are accepted in the Doxygen
/// documentation:
///
/// http://www.stack.nl/~dimitri/doxygen/manual/docblocks.html
#[derive(Default, Clone, PartialEq, Debug)]
pub struct Annotations {
    /// Whether this item is marked as opaque. Only applies to types.
    opaque: bool,
    /// Whether this item should be hidden from the output. Only applies to
    /// types, or enum variants.
    hide: bool,
    /// Whether this type should be replaced by another. The name is a
    /// namespace-aware path.
    use_instead_of: Option<Vec<String>>,
    /// Manually disable deriving copy/clone on this type. Only applies to
    /// struct or union types.
    disallow_copy: bool,
    /// Manually disable deriving debug on this type.
    disallow_debug: bool,
    /// Manually disable deriving/implement default on this type.
    disallow_default: bool,
    /// Whether to add a #[must_use] annotation to this type.
    must_use_type: bool,
    /// Whether fields should be marked as private or not. You can set this on
    /// structs (it will apply to all the fields), or individual fields.
    private_fields: Option<bool>,
    /// The kind of accessor this field will have. Also can be applied to
    /// structs so all the fields inside share it by default.
    accessor_kind: Option<FieldAccessorKind>,
    /// Whether this enum variant should be constified.
    ///
    /// This is controlled by the `constant` attribute, this way:
    ///
    /// ```cpp
    /// enum Foo {
    ///     Bar = 0, /**< <div rustbindgen constant></div> */
    ///     Baz = 0,
    /// };
    /// ```
    ///
    /// In that case, bindgen will generate a constant for `Bar` instead of
    /// `Baz`.
    constify_enum_variant: bool,
    /// List of explicit derives for this type.
    derives: Vec<String>,
}

fn parse_accessor(s: &str) -> FieldAccessorKind {
    match s {
        "false" => FieldAccessorKind::None,
        "unsafe" => FieldAccessorKind::Unsafe,
        "immutable" => FieldAccessorKind::Immutable,
        _ => FieldAccessorKind::Regular,
    }
}

impl Annotations {
    /// Construct new annotations for the given cursor and its bindgen comments
    /// (if any).
    pub fn new(cursor: &clang::Cursor) -> Option<Annotations> {
        let mut anno = Annotations::default();
        let mut matched_one = false;
        anno.parse(&cursor.comment(), &mut matched_one);

        if matched_one {
            Some(anno)
        } else {
            None
        }
    }

    /// Should this type be hidden?
    pub fn hide(&self) -> bool {
        self.hide
    }

    /// Should this type be opaque?
    pub fn opaque(&self) -> bool {
        self.opaque
    }

    /// For a given type, indicates the type it should replace.
    ///
    /// For example, in the following code:
    ///
    /// ```cpp
    ///
    /// /** <div rustbindgen replaces="Bar"></div> */
    /// struct Foo { int x; };
    ///
    /// struct Bar { char foo; };
    /// ```
    ///
    /// the generated code would look something like:
    ///
    /// ```
    /// /** <div rustbindgen replaces="Bar"></div> */
    /// struct Bar {
    ///     x: ::std::os::raw::c_int,
    /// };
    /// ```
    ///
    /// That is, code for `Foo` is used to generate `Bar`.
    pub fn use_instead_of(&self) -> Option<&[String]> {
        self.use_instead_of.as_deref()
    }

    /// The list of derives that have been specified in this annotation.
    pub fn derives(&self) -> &[String] {
        &self.derives
    }

    /// Should we avoid implementing the `Copy` trait?
    pub fn disallow_copy(&self) -> bool {
        self.disallow_copy
    }

    /// Should we avoid implementing the `Debug` trait?
    pub fn disallow_debug(&self) -> bool {
        self.disallow_debug
    }

    /// Should we avoid implementing the `Default` trait?
    pub fn disallow_default(&self) -> bool {
        self.disallow_default
    }

    /// Should this type get a `#[must_use]` annotation?
    pub fn must_use_type(&self) -> bool {
        self.must_use_type
    }

    /// Should the fields be private?
    pub fn private_fields(&self) -> Option<bool> {
        self.private_fields
    }

    /// What kind of accessors should we provide for this type's fields?
    pub fn accessor_kind(&self) -> Option<FieldAccessorKind> {
        self.accessor_kind
    }

    fn parse(&mut self, comment: &clang::Comment, matched: &mut bool) {
        use clang_sys::CXComment_HTMLStartTag;
        if comment.kind() == CXComment_HTMLStartTag &&
            comment.get_tag_name() == "div" &&
            comment
                .get_tag_attrs()
                .next()
                .map_or(false, |attr| attr.name == "rustbindgen")
        {
            *matched = true;
            for attr in comment.get_tag_attrs() {
                match attr.name.as_str() {
                    "opaque" => self.opaque = true,
                    "hide" => self.hide = true,
                    "nocopy" => self.disallow_copy = true,
                    "nodebug" => self.disallow_debug = true,
                    "nodefault" => self.disallow_default = true,
                    "mustusetype" => self.must_use_type = true,
                    "replaces" => {
                        self.use_instead_of = Some(
                            attr.value.split("::").map(Into::into).collect(),
                        )
                    }
                    "derive" => self.derives.push(attr.value),
                    "private" => {
                        self.private_fields = Some(attr.value != "false")
                    }
                    "accessor" => {
                        self.accessor_kind = Some(parse_accessor(&attr.value))
                    }
                    "constant" => self.constify_enum_variant = true,
                    _ => {}
                }
            }
        }

        for child in comment.get_children() {
            self.parse(&child, matched);
        }
    }

    /// Returns whether we've parsed a "constant" attribute.
    pub fn constify_enum_variant(&self) -> bool {
        self.constify_enum_variant
    }
}
