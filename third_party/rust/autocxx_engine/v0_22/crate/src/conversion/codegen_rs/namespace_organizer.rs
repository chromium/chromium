// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::types::Namespace;
use std::collections::BTreeMap;

pub trait HasNs {
    fn get_namespace(&self) -> &Namespace;
}

pub struct NamespaceEntries<'a, T: HasNs> {
    entries: Vec<&'a T>,
    children: BTreeMap<&'a String, NamespaceEntries<'a, T>>,
}

impl<'a, T: HasNs> NamespaceEntries<'a, T> {
    pub(crate) fn new(apis: &'a [T]) -> Self {
        let api_refs = apis.iter().collect::<Vec<_>>();
        Self::sort_by_inner_namespace(api_refs, 0)
    }

    pub(crate) fn is_empty(&self) -> bool {
        self.entries.is_empty() && self.children.iter().all(|(_, child)| child.is_empty())
    }

    pub(crate) fn entries(&self) -> &[&'a T] {
        &self.entries
    }

    pub(crate) fn children(&self) -> impl Iterator<Item = (&&String, &NamespaceEntries<T>)> {
        self.children.iter()
    }

    fn sort_by_inner_namespace(apis: Vec<&'a T>, depth: usize) -> Self {
        let mut root = NamespaceEntries {
            entries: Vec::new(),
            children: BTreeMap::new(),
        };

        let mut kids_by_child_ns = BTreeMap::new();
        for api in apis {
            let first_ns_elem = api.get_namespace().iter().nth(depth);
            if let Some(first_ns_elem) = first_ns_elem {
                let list = kids_by_child_ns
                    .entry(first_ns_elem)
                    .or_insert_with(Vec::new);
                list.push(api);
                continue;
            }
            root.entries.push(api);
        }

        for (k, v) in kids_by_child_ns.into_iter() {
            root.children
                .insert(k, Self::sort_by_inner_namespace(v, depth + 1));
        }

        root
    }
}

#[cfg(test)]
mod tests {
    use super::{HasNs, NamespaceEntries};
    use crate::types::Namespace;

    struct TestApi(&'static str, Namespace);
    impl HasNs for TestApi {
        fn get_namespace(&self) -> &Namespace {
            &self.1
        }
    }

    #[test]
    fn test_ns_entries_sort() {
        let entries = vec![
            make_api(None, "C"),
            make_api(None, "A"),
            make_api(Some("G"), "E"),
            make_api(Some("D"), "F"),
            make_api(Some("G"), "H"),
            make_api(Some("D::K"), "L"),
            make_api(Some("D::K"), "M"),
            make_api(None, "B"),
            make_api(Some("D"), "I"),
            make_api(Some("D"), "J"),
        ];
        let ns = NamespaceEntries::new(&entries);
        let root_entries = ns.entries();
        assert_eq!(root_entries.len(), 3);
        assert_ident(root_entries[0], "C");
        assert_ident(root_entries[1], "A");
        assert_ident(root_entries[2], "B");
        let mut kids = ns.children();
        let (d_id, d_nse) = kids.next().unwrap();
        assert_eq!(d_id.to_string(), "D");
        let (g_id, g_nse) = kids.next().unwrap();
        assert_eq!(g_id.to_string(), "G");
        assert!(kids.next().is_none());
        let d_nse_entries = d_nse.entries();
        assert_eq!(d_nse_entries.len(), 3);
        assert_ident(d_nse_entries[0], "F");
        assert_ident(d_nse_entries[1], "I");
        assert_ident(d_nse_entries[2], "J");
        let g_nse_entries = g_nse.entries();
        assert_eq!(g_nse_entries.len(), 2);
        assert_ident(g_nse_entries[0], "E");
        assert_ident(g_nse_entries[1], "H");
        let mut g_kids = g_nse.children();
        assert!(g_kids.next().is_none());
        let mut d_kids = d_nse.children();
        let (k_id, k_nse) = d_kids.next().unwrap();
        assert_eq!(k_id.to_string(), "K");
        let k_nse_entries = k_nse.entries();
        assert_eq!(k_nse_entries.len(), 2);
        assert_ident(k_nse_entries[0], "L");
        assert_ident(k_nse_entries[1], "M");
    }

    fn assert_ident(api: &TestApi, expected: &str) {
        assert_eq!(api.0, expected);
    }

    fn make_api(ns: Option<&str>, id: &'static str) -> TestApi {
        let ns = match ns {
            Some(st) => Namespace::from_user_input(st),
            None => Namespace::new(),
        };
        TestApi(id, ns)
    }
}
