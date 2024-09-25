use std::marker::PhantomData;

use quote::ToTokens;
use syn::{visit_mut::VisitMut, Attribute, FnArg, ItemFn, Pat};

use crate::{error::ErrorsVec, refident::MaybePat, utils::attr_is};

pub trait AttrBuilder<E> {
    type Out;

    fn build(attr: Attribute, extra: &E) -> syn::Result<Self::Out>;
}

pub trait Validator<T> {
    fn validate(_arg: &T) -> syn::Result<()> {
        Ok(())
    }
}

impl AttrBuilder<Pat> for () {
    type Out = Pat;

    fn build(_attr: Attribute, pat: &Pat) -> syn::Result<Self::Out> {
        Ok(pat.clone())
    }
}

impl AttrBuilder<ItemFn> for () {
    type Out = Attribute;

    fn build(attr: Attribute, _item_fn: &ItemFn) -> syn::Result<Self::Out> {
        Ok(attr.clone())
    }
}

impl<T> Validator<T> for () {}

/// Simple struct used to visit function argument attributes and extract attributes that match
/// the `name`: Only one attribute is allowed for arguments.
pub struct JustOnceFnArgAttributeExtractor<'a, B = ()>
where
    B: AttrBuilder<Pat>,
{
    name: &'a str,
    elements: Vec<B::Out>,
    errors: Vec<syn::Error>,
    _phantom: PhantomData<B>,
}

impl<'a> From<&'a str> for JustOnceFnArgAttributeExtractor<'a, ()> {
    fn from(value: &'a str) -> Self {
        Self::new(value)
    }
}

impl<'a, B> JustOnceFnArgAttributeExtractor<'a, B>
where
    B: AttrBuilder<Pat>,
{
    pub fn new(name: &'a str) -> Self {
        Self {
            name,
            elements: Default::default(),
            errors: Default::default(),
            _phantom: PhantomData,
        }
    }

    pub fn take(self) -> Result<Vec<B::Out>, ErrorsVec> {
        if self.errors.is_empty() {
            Ok(self.elements)
        } else {
            Err(self.errors.into())
        }
    }
}

impl<B> VisitMut for JustOnceFnArgAttributeExtractor<'_, B>
where
    B: AttrBuilder<Pat>,
    B: Validator<FnArg>,
{
    fn visit_fn_arg_mut(&mut self, node: &mut FnArg) {
        let pat = match node.maybe_pat() {
            Some(pat) => pat.clone(),
            None => return,
        };
        if let FnArg::Typed(ref mut arg) = node {
            // Extract interesting attributes
            let attrs = std::mem::take(&mut arg.attrs);
            let (extracted, remain): (Vec<_>, Vec<_>) =
                attrs.into_iter().partition(|a| attr_is(a, self.name));

            arg.attrs = remain;

            let parsed = extracted
                .into_iter()
                .map(|attr| B::build(attr.clone(), &pat).map(|t| (attr, t)))
                .collect::<Result<Vec<_>, _>>();

            match parsed {
                Ok(data) => match data.len() {
                    1 => match B::validate(node) {
                        Ok(_) => self.elements.extend(data.into_iter().map(|(_attr, t)| t)),
                        Err(e) => {
                            self.errors.push(e);
                        }
                    },

                    0 => {}
                    _ => {
                        self.errors
                            .extend(data.into_iter().skip(1).map(|(attr, _t)| {
                                syn::Error::new_spanned(
                                    attr.into_token_stream(),
                                    format!("Cannot use #[{}] more than once.", self.name),
                                )
                            }));
                    }
                },
                Err(e) => {
                    self.errors.push(e);
                }
            }
        }
    }
}

/// Simple struct used to visit function attributes and extract attributes that match
/// the `name`: Only one attribute is allowed for arguments.
pub struct JustOnceFnAttributeExtractor<'a, B = ()>
where
    B: AttrBuilder<ItemFn>,
{
    name: &'a str,
    inner: Result<Option<B::Out>, Vec<syn::Error>>,
    _phantom: PhantomData<B>,
}

impl<'a> From<&'a str> for JustOnceFnAttributeExtractor<'a, ()> {
    fn from(value: &'a str) -> Self {
        Self::new(value)
    }
}

impl<'a, B> JustOnceFnAttributeExtractor<'a, B>
where
    B: AttrBuilder<ItemFn>,
{
    pub fn new(name: &'a str) -> Self {
        Self {
            name,
            inner: Ok(Default::default()),
            _phantom: PhantomData,
        }
    }

    pub fn take(self) -> Result<Option<B::Out>, ErrorsVec> {
        self.inner.map_err(Into::into)
    }
}

impl<B> VisitMut for JustOnceFnAttributeExtractor<'_, B>
where
    B: AttrBuilder<ItemFn>,
    B: Validator<ItemFn>,
{
    fn visit_item_fn_mut(&mut self, item_fn: &mut ItemFn) {
        // Extract interesting attributes
        let attrs = std::mem::take(&mut item_fn.attrs);
        let (extracted, remain): (Vec<_>, Vec<_>) =
            attrs.into_iter().partition(|a| attr_is(a, self.name));

        item_fn.attrs = remain;

        let parsed = extracted
            .into_iter()
            .map(|attr| B::build(attr.clone(), item_fn).map(|t| (attr, t)))
            .collect::<Result<Vec<_>, _>>();
        let mut errors = Vec::default();
        let mut out = None;

        match parsed {
            Ok(data) => match data.len() {
                1 => match B::validate(item_fn) {
                    Ok(_) => {
                        out = data.into_iter().next().map(|(_attr, t)| t);
                    }
                    Err(e) => {
                        errors.push(e);
                    }
                },

                0 => {}
                _ => {
                    errors.extend(data.into_iter().skip(1).map(|(attr, _t)| {
                        syn::Error::new_spanned(
                            attr.into_token_stream(),
                            format!("Cannot use #[{}] more than once.", self.name),
                        )
                    }));
                }
            },
            Err(e) => {
                errors.push(e);
            }
        };
        if errors.is_empty() {
            self.inner = Ok(out);
        } else {
            self.inner = Err(errors);
        }
    }
}
