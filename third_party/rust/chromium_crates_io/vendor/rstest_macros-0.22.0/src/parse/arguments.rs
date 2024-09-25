use std::collections::HashMap;

use quote::format_ident;
use syn::{FnArg, Ident, Pat};

use crate::{
    refident::{IntoPat, MaybeIdent, MaybePatType, MaybePatTypeMut},
    resolver::pat_invert_mutability,
};

#[derive(PartialEq, Debug, Clone, Copy)]
#[allow(dead_code)]
#[derive(Default)]
pub(crate) enum FutureArg {
    #[default]
    None,
    Define,
    Await,
}

#[derive(Clone, PartialEq, Default, Debug)]
pub(crate) struct ArgumentInfo {
    future: FutureArg,
    by_ref: bool,
    ignore: bool,
    inner_pat: Option<Pat>, // Optional pat used to inject data and call test function
}

impl ArgumentInfo {
    fn future(future: FutureArg) -> Self {
        Self {
            future,
            ..Default::default()
        }
    }

    fn by_ref() -> Self {
        Self {
            by_ref: true,
            ..Default::default()
        }
    }

    fn ignore() -> Self {
        Self {
            ignore: true,
            ..Default::default()
        }
    }

    fn inner_pat(pat: Pat) -> Self {
        Self {
            inner_pat: Some(pat),
            ..Default::default()
        }
    }

    fn is_future(&self) -> bool {
        use FutureArg::*;

        matches!(self.future, Define | Await)
    }

    fn is_future_await(&self) -> bool {
        use FutureArg::*;

        matches!(self.future, Await)
    }

    fn is_by_ref(&self) -> bool {
        self.by_ref
    }

    fn is_ignore(&self) -> bool {
        self.ignore
    }
}

#[derive(Clone, PartialEq, Default, Debug)]
struct Args {
    args: HashMap<Pat, ArgumentInfo>,
}

impl Args {
    fn get(&self, pat: &Pat) -> Option<&ArgumentInfo> {
        self.args
            .get(pat)
            .or_else(|| self.args.get(&pat_invert_mutability(pat)))
    }

    fn entry(&mut self, pat: Pat) -> std::collections::hash_map::Entry<Pat, ArgumentInfo> {
        self.args.entry(pat)
    }
}

#[derive(Clone, PartialEq, Default, Debug)]
pub(crate) struct ArgumentsInfo {
    args: Args,
    is_global_await: bool,
    once: Option<syn::Attribute>,
}

impl ArgumentsInfo {
    pub(crate) fn set_future(&mut self, pat: Pat, kind: FutureArg) {
        self.args
            .entry(pat)
            .and_modify(|v| v.future = kind)
            .or_insert_with(|| ArgumentInfo::future(kind));
    }

    pub(crate) fn set_futures(&mut self, futures: impl Iterator<Item = (Pat, FutureArg)>) {
        futures.for_each(|(pat, k)| self.set_future(pat, k));
    }

    pub(crate) fn set_global_await(&mut self, is_global_await: bool) {
        self.is_global_await = is_global_await;
    }

    #[allow(dead_code)]
    pub(crate) fn add_future(&mut self, pat: Pat) {
        self.set_future(pat, FutureArg::Define);
    }

    pub(crate) fn is_future(&self, pat: &Pat) -> bool {
        self.args
            .get(pat)
            .map(|arg| arg.is_future())
            .unwrap_or_default()
    }

    pub(crate) fn is_future_await(&self, pat: &Pat) -> bool {
        match self.args.get(pat) {
            Some(arg) => arg.is_future_await() || (arg.is_future() && self.is_global_await()),
            None => false,
        }
    }

    pub(crate) fn is_global_await(&self) -> bool {
        self.is_global_await
    }

    pub(crate) fn set_once(&mut self, once: Option<syn::Attribute>) {
        self.once = once
    }

    pub(crate) fn get_once(&self) -> Option<&syn::Attribute> {
        self.once.as_ref()
    }

    pub(crate) fn is_once(&self) -> bool {
        self.get_once().is_some()
    }

    pub(crate) fn set_by_ref(&mut self, pat: Pat) {
        self.args
            .entry(pat)
            .and_modify(|v| v.by_ref = true)
            .or_insert_with(ArgumentInfo::by_ref);
    }

    pub(crate) fn set_ignore(&mut self, pat: Pat) {
        self.args
            .entry(pat)
            .and_modify(|v| v.ignore = true)
            .or_insert_with(ArgumentInfo::ignore);
    }

    pub(crate) fn set_by_refs(&mut self, by_refs: impl Iterator<Item = Pat>) {
        by_refs.for_each(|pat| self.set_by_ref(pat));
    }

    pub(crate) fn set_ignores(&mut self, ignores: impl Iterator<Item = Pat>) {
        ignores.for_each(|pat| self.set_ignore(pat));
    }

    pub(crate) fn is_by_refs(&self, id: &Pat) -> bool {
        self.args
            .get(id)
            .map(|arg| arg.is_by_ref())
            .unwrap_or_default()
    }

    pub(crate) fn is_ignore(&self, pat: &Pat) -> bool {
        self.args
            .get(pat)
            .map(|arg| arg.is_ignore())
            .unwrap_or_default()
    }

    pub(crate) fn set_inner_pat(&mut self, pat: Pat, inner: Pat) {
        self.args
            .entry(pat)
            .and_modify(|v| v.inner_pat = Some(inner.clone()))
            .or_insert_with(|| ArgumentInfo::inner_pat(inner));
    }

    pub(crate) fn set_inner_ident(&mut self, pat: Pat, ident: Ident) {
        self.set_inner_pat(pat, ident.into_pat());
    }

    pub(crate) fn inner_pat<'arguments: 'pat_ref, 'pat_ref>(
        &'arguments self,
        id: &'pat_ref Pat,
    ) -> &'pat_ref Pat {
        self.args
            .get(id)
            .and_then(|arg| arg.inner_pat.as_ref())
            .unwrap_or(id)
    }

    pub(crate) fn register_inner_destructored_idents_names(&mut self, item_fn: &syn::ItemFn) {
        let mut anonymous_destruct = 0_usize;
        // On the signature we remove all destruct arguments and replace them with `__destruct_{id}`
        // This is just to define the new arguments and local variable that we use in the test
        // and coll the original signature that should preserve the destruct arguments.
        for arg in item_fn.sig.inputs.iter() {
            if let Some(pt) = arg.maybe_pat_type() {
                if pt.maybe_ident().is_none() {
                    anonymous_destruct += 1;
                    let ident = format_ident!("__destruct_{}", anonymous_destruct);
                    self.set_inner_ident(pt.pat.as_ref().clone(), ident);
                }
            }
        }
    }

    pub(crate) fn replace_fn_args_with_related_inner_pat<'a>(
        &'a self,
        fn_args: impl Iterator<Item = FnArg> + 'a,
    ) -> impl Iterator<Item = FnArg> + 'a {
        fn_args.map(|mut fn_arg| {
            if let Some(p) = fn_arg.maybe_pat_type_mut() {
                p.pat = Box::new(self.inner_pat(p.pat.as_ref()).clone());
            }
            fn_arg
        })
    }
}

#[cfg(test)]
mod should_implement_is_future_await_logic {
    use super::*;
    use crate::test::*;

    #[fixture]
    fn info() -> ArgumentsInfo {
        let mut a = ArgumentsInfo::default();
        a.set_future(pat("simple"), FutureArg::Define);
        a.set_future(pat("other_simple"), FutureArg::Define);
        a.set_future(pat("awaited"), FutureArg::Await);
        a.set_future(pat("other_awaited"), FutureArg::Await);
        a.set_future(pat("none"), FutureArg::None);
        a
    }

    #[rstest]
    fn no_matching_ident(info: ArgumentsInfo) {
        assert!(!info.is_future_await(&pat("some")));
        assert!(!info.is_future_await(&pat("simple")));
        assert!(!info.is_future_await(&pat("none")));
    }

    #[rstest]
    fn matching_ident(info: ArgumentsInfo) {
        assert!(info.is_future_await(&pat("awaited")));
        assert!(info.is_future_await(&pat("other_awaited")));
    }

    #[rstest]
    fn global_matching_future_ident(mut info: ArgumentsInfo) {
        info.set_global_await(true);
        assert!(info.is_future_await(&pat("simple")));
        assert!(info.is_future_await(&pat("other_simple")));
        assert!(info.is_future_await(&pat("awaited")));

        assert!(!info.is_future_await(&pat("some")));
        assert!(!info.is_future_await(&pat("none")));
    }
}

#[cfg(test)]
mod should_register_inner_destructored_idents_names {
    use super::*;
    use crate::test::{assert_eq, *};

    #[test]
    fn implement_the_correct_pat_reolver() {
        let item_fn = "fn test_function(A(a,b): A, (c,d,e): (u32, u32, u32), none: u32, B{s,d} : B, clean: C) {}".ast();

        let mut arguments = ArgumentsInfo::default();

        arguments.register_inner_destructored_idents_names(&item_fn);

        assert_eq!(arguments.inner_pat(&pat("A(a,b)")), &pat("__destruct_1"));
        assert_eq!(arguments.inner_pat(&pat("(c,d,e)")), &pat("__destruct_2"));
        assert_eq!(arguments.inner_pat(&pat("none")), &pat("none"));
        assert_eq!(arguments.inner_pat(&pat("B{s,d}")), &pat("__destruct_3"));
        assert_eq!(arguments.inner_pat(&pat("clean")), &pat("clean"));
    }

    #[test]
    fn and_replace_them_correctly() {
        let item_fn = "fn test_function(A(a,b): A, (c,d,e): (u32, u32, u32), none: u32, B{s,d} : B, clean: C) {}".ast();

        let mut arguments = ArgumentsInfo::default();

        arguments.register_inner_destructored_idents_names(&item_fn);

        let new_args = arguments
            .replace_fn_args_with_related_inner_pat(item_fn.sig.inputs.into_iter())
            .filter_map(|f| f.maybe_ident().cloned())
            .map(|id| id.to_string())
            .collect::<Vec<_>>()
            .join(" | ");

        assert_eq!(
            new_args,
            "__destruct_1 | __destruct_2 | none | __destruct_3 | clean"
        );
    }
}
