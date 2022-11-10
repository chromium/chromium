// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use indexmap::set::IndexSet as HashSet;
use std::collections::VecDeque;
use std::fmt::Debug;

use itertools::Itertools;

use crate::types::QualifiedName;

/// A little like `HasDependencies` but accounts for only direct fiele
/// and bases.
pub(crate) trait HasFieldsAndBases {
    fn name(&self) -> &QualifiedName;
    /// Return field and base class dependencies of this item.
    /// This should only include those items where a definition is required,
    /// not merely a declaration. So if the field type is
    /// `std::unique_ptr<A>`, this should only return `std::unique_ptr`.
    fn field_and_base_deps(&self) -> Box<dyn Iterator<Item = &QualifiedName> + '_>;
}

/// Iterate through APIs in an order such that later APIs have no fields or bases
/// other than those whose types have already been processed.
pub(super) fn fields_and_bases_first<'a, T: HasFieldsAndBases + Debug + 'a>(
    inputs: impl Iterator<Item = &'a T> + 'a,
) -> impl Iterator<Item = &'a T> {
    let queue: VecDeque<_> = inputs.collect();
    let yet_to_do = queue.iter().map(|api| api.name()).collect();
    DepthFirstIter { queue, yet_to_do }
}

struct DepthFirstIter<'a, T: HasFieldsAndBases + Debug> {
    queue: VecDeque<&'a T>,
    yet_to_do: HashSet<&'a QualifiedName>,
}

impl<'a, T: HasFieldsAndBases + Debug> Iterator for DepthFirstIter<'a, T> {
    type Item = &'a T;

    fn next(&mut self) -> Option<Self::Item> {
        let first_candidate = self.queue.get(0).map(|api| api.name());
        while let Some(candidate) = self.queue.pop_front() {
            if !candidate
                .field_and_base_deps()
                .any(|d| self.yet_to_do.contains(&d))
            {
                self.yet_to_do.remove(candidate.name());
                return Some(candidate);
            }
            self.queue.push_back(candidate);
            if self.queue.get(0).map(|api| api.name()) == first_candidate {
                panic!(
                    "Failed to find a candidate; there must be a circular dependency. Queue is {}",
                    self.queue
                        .iter()
                        .map(|item| format!(
                            "{}: {}",
                            item.name(),
                            item.field_and_base_deps().join(",")
                        ))
                        .join("\n")
                );
            }
        }
        None
    }
}

#[cfg(test)]
mod test {
    use crate::types::QualifiedName;

    use super::{fields_and_bases_first, HasFieldsAndBases};

    #[derive(Debug)]
    struct Thing(QualifiedName, Vec<QualifiedName>);

    impl HasFieldsAndBases for Thing {
        fn name(&self) -> &QualifiedName {
            &self.0
        }

        fn field_and_base_deps(&self) -> Box<dyn Iterator<Item = &QualifiedName> + '_> {
            Box::new(self.1.iter())
        }
    }

    #[test]
    fn test() {
        let a = Thing(QualifiedName::new_from_cpp_name("a"), vec![]);
        let b = Thing(
            QualifiedName::new_from_cpp_name("b"),
            vec![
                QualifiedName::new_from_cpp_name("a"),
                QualifiedName::new_from_cpp_name("c"),
            ],
        );
        let c = Thing(
            QualifiedName::new_from_cpp_name("c"),
            vec![QualifiedName::new_from_cpp_name("a")],
        );
        let api_list = vec![a, b, c];
        let mut it = fields_and_bases_first(api_list.iter());
        assert_eq!(it.next().unwrap().0, QualifiedName::new_from_cpp_name("a"));
        assert_eq!(it.next().unwrap().0, QualifiedName::new_from_cpp_name("c"));
        assert_eq!(it.next().unwrap().0, QualifiedName::new_from_cpp_name("b"));
        assert!(it.next().is_none());
    }
}
