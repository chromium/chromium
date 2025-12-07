// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Utilities for editing `.toml` files.

use itertools::Itertools;
use std::collections::HashMap;
use toml_edit::visit_mut::VisitMut;
use toml_edit::{DocumentMut, Table};

pub struct FormatOptions {
    pub toplevel_table_order: &'static [&'static str],
}

pub static GNRT_CONFIG_FORMAT_OPTIONS: FormatOptions =
    FormatOptions { toplevel_table_order: &["gn", "resolve", "all-crates", "crate"] };

pub fn format(doc: &mut DocumentMut, format_options: &FormatOptions) {
    // Sort top-level tables based on `format_options.toplevel_table_order`.
    let key_to_desired_position: HashMap<&'static str, usize> = format_options
        .toplevel_table_order
        .iter()
        .copied()
        .enumerate()
        .map(|(index, key)| (key, index))
        .collect();
    let sorted_tables = doc
        .iter_mut()
        .filter_map(|(key, item)| item.as_table_mut().map(|table| (key, table)))
        .sorted_by_key(|(key, _table)| {
            key_to_desired_position.get(key.get()).copied().unwrap_or(usize::MAX)
        })
        .map(|(_key, table)| table);

    struct Sorter {
        next_position: isize,
    }

    impl VisitMut for Sorter {
        fn visit_table_mut(&mut self, table: &mut Table) {
            table.set_position(self.next_position);
            self.next_position += 1;

            // Sort nested tables alphabetically.
            let sorted_nested_tables = table
                .iter_mut()
                .sorted_by(|(key1, _), (key2, _)| Ord::cmp(key1.get(), key2.get()))
                .filter_map(|(_key, value)| value.as_table_mut());
            for nested_table in sorted_nested_tables {
                self.visit_table_mut(nested_table);
            }
        }
    }

    let mut sorter = Sorter { next_position: 1 };
    for table in sorted_tables {
        table.sort_values();
        sorter.visit_table_mut(table)
    }
}

#[cfg(test)]
mod test {
    use super::{format, FormatOptions};
    use pretty_assertions::assert_eq;
    use std::fmt::Write;
    use toml_edit::DocumentMut;

    fn trim(s: &str) -> String {
        let mut result = String::with_capacity(s.len());
        for line in s.lines() {
            writeln!(&mut result, "{}", line.trim_ascii()).unwrap();
        }
        result
    }

    fn sort(input: &str, toplevel_table_order: &'static [&'static str]) -> String {
        let mut doc = input.parse::<DocumentMut>().unwrap();
        format(&mut doc, &FormatOptions { toplevel_table_order });
        doc.to_string()
    }

    #[test]
    fn test_sort_simple_keys_in_toplevel_tables() {
        let input = trim(
            r#"
            [table]
            a = 1
            c = 3
            b = 2
            "#,
        );
        let expected = trim(
            r#"
            [table]
            a = 1
            b = 2
            c = 3
            "#,
        );
        assert_eq!(sort(&input, &[]), expected);
    }

    #[test]
    fn test_sort_simple_keys_in_toplevel_tables_preserving_comments() {
        let input = trim(
            r#"
            [table]
            a = 1
            c = 3 # C comment
            # B comment
            b = 2
            "#,
        );
        let expected = trim(
            r#"
            [table]
            a = 1
            # B comment
            b = 2
            c = 3 # C comment
            "#,
        );
        assert_eq!(sort(&input, &[]), expected);
    }

    /// Sort only the values in top-level tables, but not values in
    /// nested tables.
    ///
    /// Rationale:
    /// * Sorting simple keys of nested tables results in quite a bit of churn.
    /// * `chromium_crates_io/run_presubmits.py` doesn't enforce order inside
    ///   nested tables (although it also doesn't enforce the order of
    ///   `[crate.foo]` vs `[crate.foo.extra_kv]` so maybe this is not a good
    ///   argument)
    #[test]
    fn test_dont_sort_simple_keys_in_nested_tables() {
        let input = trim(
            r#"
            [table]
            x = 1
            z = 3
            y = 2

            [table.foo]
            a = 1
            c = 3
            b = 2
            "#,
        );
        let expected = trim(
            r#"
            [table]
            x = 1
            y = 2
            z = 3

            [table.foo]
            a = 1
            c = 3
            b = 2
            "#,
        );
        assert_eq!(sort(&input, &[]), expected);
    }

    #[test]
    fn test_dont_sort_inline_tables() {
        let input = trim(
            r#"
            [table]
            a = 1
            c = { z = 3, y = 2, x = 1 }
            b = 2
            "#,
        );
        let expected = trim(
            r#"
            [table]
            a = 1
            b = 2
            c = { z = 3, y = 2, x = 1 }
            "#,
        );
        assert_eq!(sort(&input, &[]), expected);
    }

    #[test]
    fn test_sort_toplevel_table_with_nested_tables() {
        let input = trim(
            r#"
            [table.foo]
            a = 1

            [table.baz.b]
            baz_b = 1

            [table.baz]
            baz = 1

            [table.baz.a]
            baz_a = 1

            [table.bar]
            b = 2
            "#,
        );
        let expected = trim(
            r#"
            [table.bar]
            b = 2

            [table.baz]
            baz = 1

            [table.baz.a]
            baz_a = 1

            [table.baz.b]
            baz_b = 1

            [table.foo]
            a = 1
            "#,
        );
        assert_eq!(sort(&input, &[]), expected);
    }

    #[test]
    fn test_sort_toplevel_table_with_nested_tables_preserving_comment() {
        let input = trim(
            r#"
            [table.foo]
            a = 1

            [table.baz] # Baz comment
            baz = 1

            # Bar comment
            [table.bar]
            b = 2
            "#,
        );
        let expected = trim(
            r#"
            # Bar comment
            [table.bar]
            b = 2

            [table.baz] # Baz comment
            baz = 1

            [table.foo]
            a = 1
            "#,
        );
        assert_eq!(sort(&input, &[]), expected);
    }

    #[test]
    fn test_sort_to_enforce_toplevel_order() {
        let input = trim(
            r#"
            [table.foo]
            a = 1

            [zzz]
            p = 0

            [table.bar]
            b = 2
            "#,
        );
        let expected = trim(
            r#"
            [zzz]
            p = 0

            [table.bar]
            b = 2

            [table.foo]
            a = 1
            "#,
        );
        assert_eq!(sort(&input, &["zzz", "table"]), expected);
    }

    #[test]
    fn test_sort_when_no_order_specified_for_some_toplevel_tables() {
        let input = trim(
            r#"
            [table.foo]
            a = 1

            [zzz]
            p = 0

            [yyy]
            p = 0

            [ppp]
            p = 0

            [xxx]
            p = 0

            [table.bar]
            b = 2
            "#,
        );
        let expected = trim(
            r#"
            [xxx]
            p = 0

            [zzz]
            p = 0

            [table.bar]
            b = 2

            [table.foo]
            a = 1

            [yyy]
            p = 0

            [ppp]
            p = 0
            "#,
        );
        // `ppp` and `yyy` are not covered by `toplevel_table_order` passed to `sort`,
        // and so: 1) they move to the end, but 2) their relative order is preserved.
        assert_eq!(sort(&input, &["xxx", "zzz", "table"]), expected);
    }
}
