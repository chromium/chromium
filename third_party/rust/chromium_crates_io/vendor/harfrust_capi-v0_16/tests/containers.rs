//! Container semantics exercised through the C entry points.

use core::ffi::{c_uint, c_void};
use std::collections::BTreeMap;
use std::ptr;

use crate::*;

const INVALID: u32 = HR_CODEPOINT_INVALID;

unsafe fn members(set: *const hr_set_t) -> Vec<u32> {
    let mut values = Vec::new();
    let mut value = INVALID;
    while unsafe { hr_set_next(set, &raw mut value) } != 0 {
        values.push(value);
    }
    assert_eq!(value, INVALID);
    values
}

unsafe fn ranges(set: *const hr_set_t, backwards: bool) -> Vec<(u32, u32)> {
    let (mut first, mut last) = (INVALID, INVALID);
    let mut result = Vec::new();
    let next = if backwards {
        hr_set_previous_range
    } else {
        hr_set_next_range
    };
    while unsafe { next(set, &raw mut first, &raw mut last) } != 0 {
        result.push((first, last));
    }
    assert_eq!((first, last), (INVALID, INVALID));
    result
}

#[test]
fn set_ranges_and_iteration() {
    unsafe {
        let set = hr_set_create();
        assert_eq!(hr_set_allocation_successful(set), 1);
        hr_set_add_range(set, 0, 2);
        hr_set_add_range(set, 510, 514); // spans a bitset page boundary
        hr_set_add_range(set, INVALID - 3, INVALID - 1);
        hr_set_add_range(set, 8, 7);
        hr_set_add_range(set, 20, INVALID); // invalid endpoints reject addition
        let sorted = [2, 9, 9, 12, INVALID];
        hr_set_add_sorted_array(set, sorted.as_ptr(), sorted.len() as c_uint);
        hr_set_add_sorted_array(set, ptr::null(), 0);
        hr_set_del(set, 9);
        hr_set_del_range(set, 511, 513);
        hr_set_del_range(set, 8, 7);
        hr_set_del(set, INVALID);
        let expected = [0, 1, 2, 12, 510, 514, INVALID - 3, INVALID - 2, INVALID - 1];
        assert_eq!(members(set), expected);
        assert_eq!(hr_set_get_population(set), expected.len() as c_uint);
        assert_eq!(hr_set_get_min(set), 0);
        assert_eq!(hr_set_get_max(set), INVALID - 1);
        let mut previous = INVALID;
        for &value in expected.iter().rev() {
            assert_eq!(hr_set_previous(set, &raw mut previous), 1);
            assert_eq!(previous, value);
        }
        assert_eq!(hr_set_previous(set, &raw mut previous), 0);
        assert_eq!(previous, INVALID);
        let expected_ranges = [
            (0, 2),
            (12, 12),
            (510, 510),
            (514, 514),
            (INVALID - 3, INVALID - 1),
        ];
        assert_eq!(ranges(set, false), expected_ranges);
        assert_eq!(
            ranges(set, true),
            expected_ranges.into_iter().rev().collect::<Vec<_>>()
        );
        // Cursors need not point to a member or the end of a complete range.
        let (mut first, mut last) = (99, 0);
        assert_eq!(hr_set_next_range(set, &raw mut first, &raw mut last), 1);
        assert_eq!((first, last), (1, 2));
        first = INVALID - 2;
        assert_eq!(hr_set_previous_range(set, &raw mut first, &raw mut last), 1);
        assert_eq!((first, last), (INVALID - 3, INVALID - 3));
        let mut out = [77; 12];
        assert_eq!(hr_set_next_many(set, INVALID, out.as_mut_ptr(), 12), 9);
        assert_eq!(&out[..9], &expected);
        assert_eq!(&out[9..], &[77; 3]);
        assert_eq!(hr_set_next_many(set, 12, out.as_mut_ptr(), 2), 2);
        assert_eq!(&out[..2], &[510, 514]);
        assert_eq!(hr_set_next_many(set, INVALID - 1, out.as_mut_ptr(), 12), 0);
        assert_eq!(hr_set_next_many(set, INVALID, ptr::null_mut(), 0), 0);
        hr_set_del_range(set, 510, INVALID);
        assert_eq!(members(set), [0, 1, 2, 12]);
        hr_set_clear(set);
        assert_eq!(hr_set_get_population(set), 0);
        assert_eq!(hr_set_get_min(set), INVALID);
        assert_eq!(hr_set_get_max(set), INVALID);
        assert_eq!(ranges(set, false), [] as [(u32, u32); 0]);
        assert_eq!(ranges(set, true), [] as [(u32, u32); 0]);
        hr_set_destroy(set);
    }
}

#[test]
fn inverted_sets_cover_the_full_codepoint_domain() {
    unsafe {
        let set = hr_set_create();
        hr_set_invert(set);
        assert_eq!(hr_set_is_inverted(set), 1);
        assert_eq!(hr_set_get_population(set), INVALID);
        assert_eq!(hr_set_get_min(set), 0);
        assert_eq!(hr_set_get_max(set), INVALID - 1);
        assert_eq!(hr_set_has(set, INVALID), 1); // HarfBuzz's inverted sentinel behavior
        assert_eq!(ranges(set, false), [(0, INVALID - 1)]);
        hr_set_del_range(set, 0, 3);
        hr_set_del_range(set, 510, 514);
        hr_set_del_range(set, INVALID - 3, INVALID - 1);
        hr_set_del_range(set, 20, INVALID); // rejected when inverted
        hr_set_del(set, INVALID); // never stored in the underlying bitset
        assert_eq!(hr_set_get_population(set), INVALID - 12);
        assert_eq!(hr_set_get_min(set), 4);
        assert_eq!(hr_set_get_max(set), INVALID - 4);
        assert_eq!(ranges(set, false), [(4, 509), (515, INVALID - 4)]);
        assert_eq!(ranges(set, true), [(515, INVALID - 4), (4, 509)]);
        let mut out = [0; 4];
        assert_eq!(hr_set_next_many(set, 508, out.as_mut_ptr(), 4), 4);
        assert_eq!(out, [509, 515, 516, 517]);
        hr_set_add_range(set, INVALID - 3, INVALID); // restores the upper tail
        assert_eq!(hr_set_get_max(set), INVALID - 1);
        hr_set_add_range(set, 0, 3);
        hr_set_add_sorted_array(set, [510, 511, 512, 513, 514, INVALID].as_ptr(), 6);
        assert_eq!(hr_set_get_population(set), INVALID);
        hr_set_invert(set);
        assert_eq!(hr_set_is_empty(set), 1);
        assert_eq!(hr_set_has(set, INVALID), 0);
        hr_set_invert(set);
        hr_set_clear(set);
        assert_eq!(hr_set_is_empty(set), 1);
        assert_eq!(hr_set_is_inverted(set), 0);
        hr_set_destroy(set);
    }
}

#[test]
fn set_batch_iteration_seeks_across_missing_pages() {
    unsafe {
        let set = hr_set_create();
        hr_set_add(set, 512);
        hr_set_add(set, 1024);
        for _ in 0..2 {
            for cursor in [0, 2, 255, 510, 511, 512, 700, 1023, 1024] {
                let mut out = [77; 8];
                let count = hr_set_next_many(set, cursor, out.as_mut_ptr(), 8) as usize;
                let mut expected = Vec::new();
                let mut value = cursor;
                while expected.len() < 8 && hr_set_next(set, &raw mut value) != 0 {
                    expected.push(value);
                }
                assert_eq!(&out[..count], &expected);
                assert!(out[count..].iter().all(|&v| v == 77));
            }
            hr_set_invert(set);
        }
        hr_set_destroy(set);
    }
}

#[test]
fn set_iteration_does_not_rescan_the_preceding_runs() {
    // Every iteration call used to scan the ranges from the first page, so
    // walking a set of many separate runs was quadratic: the inverted walks
    // below took tens of seconds on a debug build.
    const LIMIT: u32 = 1 << 15;
    unsafe {
        for inverted in [false, true] {
            let set = hr_set_create();
            if inverted {
                hr_set_invert(set);
            }
            for value in (1..LIMIT).step_by(2) {
                if inverted {
                    hr_set_del(set, value);
                } else {
                    hr_set_add(set, value);
                }
            }
            let expected: Vec<u32> = (0..LIMIT).filter(|v| (v % 2 == 1) != inverted).collect();
            let expected_ranges: Vec<(u32, u32)> = expected.iter().map(|&v| (v, v)).collect();

            let mut forwards = Vec::new();
            let mut value = INVALID;
            while hr_set_next(set, &raw mut value) != 0 && value < LIMIT {
                forwards.push(value);
            }
            assert_eq!(forwards, expected);
            let mut backwards = Vec::new();
            value = LIMIT;
            while hr_set_previous(set, &raw mut value) != 0 {
                backwards.push(value);
            }
            backwards.reverse();
            assert_eq!(backwards, expected);

            let mut forwards = Vec::new();
            let (mut first, mut last) = (INVALID, INVALID);
            while hr_set_next_range(set, &raw mut first, &raw mut last) != 0 && first < LIMIT {
                forwards.push((first, last));
            }
            assert_eq!(forwards, expected_ranges);
            let mut backwards = Vec::new();
            first = LIMIT;
            while hr_set_previous_range(set, &raw mut first, &raw mut last) != 0 {
                backwards.push((first, last));
            }
            backwards.reverse();
            assert_eq!(backwards, expected_ranges);

            let mut batched = Vec::new();
            let mut out = [0; 16];
            value = INVALID;
            while batched.len() < expected.len() {
                let count = hr_set_next_many(set, value, out.as_mut_ptr(), 16) as usize;
                assert_ne!(count, 0);
                batched.extend_from_slice(&out[..count]);
                value = out[count - 1];
            }
            assert_eq!(batched, expected);
            hr_set_destroy(set);
        }
    }
}

#[test]
fn set_algebra_in_all_inversion_combinations() {
    type Op = unsafe extern "C" fn(*mut hr_set_t, *const hr_set_t);
    type Expected = fn(bool, bool) -> bool;
    let operations: [(Op, Expected); 5] = [
        (hr_set_set, |_, b| b),
        (hr_set_union, |a, b| a || b),
        (hr_set_intersect, |a, b| a && b),
        (hr_set_subtract, |a, b| a && !b),
        (hr_set_symmetric_difference, |a, b| a ^ b),
    ];
    unsafe {
        for a_inverted in [false, true] {
            for b_inverted in [false, true] {
                let a = hr_set_create();
                let b = hr_set_create();
                hr_set_add_range(a, 1, 9);
                hr_set_add_range(b, 7, 16);
                if a_inverted {
                    hr_set_invert(a);
                }
                if b_inverted {
                    hr_set_invert(b);
                }
                let has_a = |v| (1..=9).contains(&v) ^ a_inverted;
                let has_b = |v| (7..=16).contains(&v) ^ b_inverted;
                assert_eq!(hr_set_is_equal(a, b), 0);
                assert_eq!(
                    hr_set_intersects(a, b) != 0,
                    (0..32).any(|v| has_a(v) && has_b(v))
                );
                assert_eq!(
                    hr_set_is_subset(a, b) != 0,
                    (0..32).all(|v| !has_a(v) || has_b(v))
                );
                for &(op, expected) in &operations {
                    let result = hr_set_copy(a);
                    op(result, b);
                    let inverted = expected(a_inverted, b_inverted);
                    assert_eq!(hr_set_is_inverted(result) != 0, inverted);
                    let mut count = 0;
                    for value in 0..32 {
                        let member = expected(has_a(value), has_b(value));
                        count += u32::from(member);
                        assert_eq!(hr_set_has(result, value) != 0, member);
                    }
                    assert_eq!(hr_set_has(result, INVALID - 1) != 0, inverted);
                    assert_eq!(
                        hr_set_get_population(result),
                        if inverted {
                            INVALID - (32 - count)
                        } else {
                            count
                        }
                    );
                    let copy = hr_set_copy(result);
                    assert_eq!(hr_set_is_equal(result, copy), 1);
                    assert_eq!(hr_set_hash(result), hr_set_hash(copy));
                    hr_set_destroy(copy);
                    hr_set_destroy(result);
                }
                // Self operations must not create aliased Rust references.
                let copy = hr_set_copy(a);
                hr_set_set(a, a);
                hr_set_union(a, a);
                hr_set_intersect(a, a);
                assert_eq!(hr_set_is_equal(a, copy), 1);
                hr_set_subtract(a, a);
                assert_eq!(hr_set_is_empty(a), 1);
                hr_set_set(a, copy);
                hr_set_symmetric_difference(a, a);
                assert_eq!(hr_set_is_empty(a), 1);
                hr_set_destroy(copy);
                hr_set_destroy(a);
                hr_set_destroy(b);
            }
        }
    }
}

#[test]
fn set_equality_checks_the_whole_set() {
    unsafe {
        let prefix = hr_set_create();
        let full = hr_set_create();
        hr_set_invert(full);
        assert_eq!(hr_set_is_equal(prefix, full), 0);
        assert_eq!(hr_set_is_equal(full, prefix), 0);
        hr_set_add_range(prefix, 0, 10);
        assert_eq!(hr_set_is_equal(prefix, full), 0);
        assert_eq!(hr_set_is_equal(full, prefix), 0);
        hr_set_destroy(prefix);
        hr_set_destroy(full);
    }
}

#[test]
fn map_entries_copies_updates_and_iteration() {
    unsafe {
        let map = hr_map_create();
        assert_eq!(hr_map_allocation_successful(map), 1);
        assert_eq!(hr_map_is_empty(map), 1);
        assert_eq!(hr_map_get(map, 1), INVALID);
        assert_eq!(hr_map_has(map, 1), 0);
        let expected = BTreeMap::from([(0, 7), (1, 10), (512, 7), (INVALID, INVALID)]);
        for (&key, &value) in &expected {
            hr_map_set(map, key, value);
        }
        hr_map_set(map, 1, 11);
        assert_eq!(hr_map_get_population(map), 4);
        assert_eq!(hr_map_get(map, 1), 11);
        hr_map_set(map, 1, 10);
        assert_eq!(hr_map_has(map, INVALID), 1);
        let copy = hr_map_copy(map);
        let reversed = hr_map_create();
        for (&key, &value) in expected.iter().rev() {
            hr_map_set(reversed, key, value);
        }
        assert_eq!(hr_map_is_equal(map, copy), 1);
        assert_eq!(hr_map_is_equal(map, reversed), 1);
        assert_eq!(hr_map_hash(map), hr_map_hash(copy));
        assert_eq!(hr_map_hash(map), hr_map_hash(reversed));
        let mut entries = BTreeMap::new();
        let (mut idx, mut key, mut value) = (-1, 100, 200);
        while hr_map_next(map, &raw mut idx, &raw mut key, &raw mut value) != 0 {
            assert!(entries.insert(key, value).is_none());
        }
        assert_eq!(idx, -1);
        assert_eq!(entries, expected);
        let last_entry = (key, value);
        idx = i32::MAX;
        assert_eq!(
            hr_map_next(map, &raw mut idx, &raw mut key, &raw mut value),
            0
        );
        assert_eq!(idx, -1);
        assert_eq!((key, value), last_entry);
        // Deletion moves a dense entry; both the moved and remaining indices
        // must continue to resolve to their original keys.
        hr_map_del(map, 1);
        hr_map_del(map, 1);
        assert_eq!(hr_map_get_population(map), 3);
        assert_eq!(hr_map_has(map, 1), 0);
        for &key in &[0, 512, INVALID] {
            assert_eq!(hr_map_get(map, key), expected[&key]);
        }
        hr_map_set(map, 999, 42);
        hr_map_set(copy, 0, 88);
        hr_map_update(map, copy);
        assert_eq!(hr_map_get_population(map), 5);
        assert_eq!(hr_map_get(map, 0), 88);
        assert_eq!(hr_map_get(map, 999), 42);
        assert_eq!(hr_map_is_equal(map, copy), 0);
        hr_map_update(map, map);
        assert_eq!(hr_map_get_population(map), 5);
        let keys = hr_set_create();
        let values = hr_set_create();
        hr_set_add(keys, 123);
        hr_set_add(values, 456);
        hr_map_keys(map, keys);
        hr_map_values(map, values);
        assert_eq!(members(keys), [0, 1, 123, 512, 999]);
        assert_eq!(members(values), [7, 10, 42, 88, 456]);
        hr_set_invert(keys);
        hr_map_keys(map, keys);
        assert_eq!(hr_set_has(keys, 0), 1);
        hr_map_clear(map);
        assert_eq!(hr_map_get_population(map), 0);
        assert_eq!(hr_map_has(map, 0), 0);
        hr_map_set(map, 7, 8);
        assert_eq!(hr_map_get(map, 7), 8);
        hr_set_destroy(keys);
        hr_set_destroy(values);
        hr_map_destroy(map);
        hr_map_destroy(copy);
        hr_map_destroy(reversed);
    }
}

#[test]
fn container_empty_singletons_and_null_arguments() {
    unsafe {
        for set in [hr_set_get_empty(), ptr::null_mut()] {
            assert_eq!(hr_set_allocation_successful(set), 0);
            hr_set_add(set, 1);
            hr_set_add_range(set, 0, 5);
            hr_set_add_sorted_array(set, [1, 2].as_ptr(), 2);
            hr_set_del(set, 1);
            hr_set_del_range(set, 0, INVALID);
            hr_set_invert(set);
            hr_set_clear(set);
            for other in [hr_set_get_empty().cast_const(), ptr::null()] {
                hr_set_set(set, other);
                hr_set_union(set, other);
                hr_set_intersect(set, other);
                hr_set_subtract(set, other);
                hr_set_symmetric_difference(set, other);
                assert_eq!(hr_set_is_equal(set, other), 1);
                assert_eq!(hr_set_is_subset(set, other), 1);
                assert_eq!(hr_set_intersects(set, other), 0);
            }
            assert_eq!(hr_set_is_empty(set), 1);
            assert_eq!(hr_set_is_inverted(set), 0);
            assert_eq!(hr_set_has(set, 1), 0);
            assert_eq!(hr_set_get_population(set), 0);
            assert_eq!(hr_set_get_min(set), INVALID);
            assert_eq!(hr_set_get_max(set), INVALID);
            assert_eq!(members(set), [] as [u32; 0]);
            assert_eq!(ranges(set, false), [] as [(u32, u32); 0]);
            assert_eq!(ranges(set, true), [] as [(u32, u32); 0]);
            let copy = hr_set_copy(set);
            assert_eq!(hr_set_allocation_successful(copy), 1);
            hr_set_add(copy, 1);
            assert_eq!(hr_set_has(copy, 1), 1);
            hr_set_destroy(copy);
            assert_eq!(hr_set_reference(set), set);
            hr_set_destroy(set);
        }
        for map in [hr_map_get_empty(), ptr::null_mut()] {
            assert_eq!(hr_map_allocation_successful(map), 0);
            hr_map_set(map, 1, 2);
            hr_map_del(map, 1);
            hr_map_clear(map);
            hr_map_update(map, map);
            hr_map_update(map, ptr::null());
            assert_eq!(hr_map_is_empty(map), 1);
            assert_eq!(hr_map_get_population(map), 0);
            assert_eq!(hr_map_get(map, 1), INVALID);
            assert_eq!(hr_map_has(map, 1), 0);
            assert_eq!(hr_map_is_equal(map, ptr::null()), 1);
            let (mut idx, mut key, mut value) = (-1, 123, 456);
            assert_eq!(
                hr_map_next(map, &raw mut idx, &raw mut key, &raw mut value),
                0
            );
            assert_eq!((idx, key, value), (-1, 123, 456));
            hr_map_keys(map, hr_set_get_empty());
            hr_map_values(map, ptr::null_mut());
            let copy = hr_map_copy(map);
            assert_eq!(hr_map_allocation_successful(copy), 1);
            hr_map_set(copy, 1, 2);
            assert_eq!(hr_map_get(copy, 1), 2);
            hr_map_destroy(copy);
            assert_eq!(hr_map_reference(map), map);
            hr_map_destroy(map);
        }
        // A NULL source empties set assignment/intersection but leaves a map
        // update and a set union/subtraction unchanged.
        let set = hr_set_create();
        hr_set_add(set, 1);
        hr_set_union(set, ptr::null());
        hr_set_subtract(set, ptr::null());
        hr_set_symmetric_difference(set, ptr::null());
        assert_eq!(members(set), [1]);
        hr_set_intersect(set, ptr::null());
        assert_eq!(hr_set_is_empty(set), 1);
        hr_set_add(set, 1);
        hr_set_set(set, ptr::null());
        assert_eq!(hr_set_is_empty(set), 1);
        hr_set_destroy(set);
        let map = hr_map_create();
        hr_map_set(map, 1, 2);
        hr_map_update(map, ptr::null());
        assert_eq!(hr_map_get(map, 1), 2);
        hr_map_destroy(map);
    }
}

unsafe extern "C" fn count_destroy(data: *mut c_void) {
    unsafe {
        *data.cast::<u32>() += 1;
    }
}

#[test]
fn container_user_data_and_reference_lifetimes() {
    unsafe {
        let key = hr_user_data_key_t { unused: 0 };
        let mut destroyed = 0u32;
        let data = ptr::from_mut(&mut destroyed).cast();
        let set = hr_set_create();
        let map = hr_map_create();
        assert_eq!(
            hr_set_set_user_data(set, &raw const key, data, Some(count_destroy), 0),
            1
        );
        assert_eq!(
            hr_map_set_user_data(map, &raw const key, data, Some(count_destroy), 0),
            1
        );
        assert_eq!(hr_set_set_user_data(set, &raw const key, data, None, 0), 0);
        assert_eq!(hr_map_set_user_data(map, &raw const key, data, None, 0), 0);
        assert_eq!(hr_set_get_user_data(set, &raw const key), data);
        assert_eq!(hr_map_get_user_data(map, &raw const key), data);
        assert_eq!(
            hr_set_set_user_data(hr_set_get_empty(), &raw const key, data, None, 1),
            0
        );
        assert_eq!(
            hr_map_set_user_data(hr_map_get_empty(), &raw const key, data, None, 1),
            0
        );
        assert_eq!(
            hr_set_set_user_data(ptr::null_mut(), &raw const key, data, None, 1),
            0
        );
        assert_eq!(
            hr_map_set_user_data(ptr::null_mut(), &raw const key, data, None, 1),
            0
        );
        let set_copy = hr_set_copy(set);
        let map_copy = hr_map_copy(map);
        assert!(hr_set_get_user_data(set_copy, &raw const key).is_null());
        assert!(hr_map_get_user_data(map_copy, &raw const key).is_null());
        hr_set_set(set, set_copy);
        hr_map_update(map, map_copy);
        hr_set_clear(set);
        hr_map_clear(map);
        assert_eq!(hr_set_get_user_data(set, &raw const key), data);
        assert_eq!(hr_map_get_user_data(map, &raw const key), data);
        hr_set_destroy(set_copy);
        hr_map_destroy(map_copy);
        assert_eq!(hr_set_reference(set), set);
        assert_eq!(hr_map_reference(map), map);
        hr_set_destroy(set);
        hr_map_destroy(map);
        assert_eq!(destroyed, 0);
        hr_set_destroy(set);
        assert_eq!(destroyed, 1);
        hr_map_destroy(map);
        assert_eq!(destroyed, 2);
    }
}

#[test]
fn set_range_iteration_can_delete_ranges_as_it_walks() {
    unsafe {
        for (width, count) in [(1, 16000), (65, 1024)] {
            let stride = width + 1;
            for inverted in [false, true] {
                for backwards in [false, true] {
                    for subtract in [false, true] {
                        let set = hr_set_create();
                        let operand = hr_set_create();
                        if inverted {
                            hr_set_invert(set);
                        }
                        for index in 0..count {
                            let first = index * stride;
                            if inverted {
                                hr_set_del(set, first + width);
                            } else {
                                hr_set_add_range(set, first, first + width - 1);
                            }
                        }
                        let (mut first, mut last) =
                            (if backwards { count * stride } else { INVALID }, INVALID);
                        let query = if backwards {
                            hr_set_previous_range
                        } else {
                            hr_set_next_range
                        };
                        for index in 0..count {
                            assert_eq!(query(set, &raw mut first, &raw mut last), 1);
                            let expected =
                                if backwards { count - index - 1 } else { index } * stride;
                            assert_eq!((first, last), (expected, expected + width - 1));
                            if subtract {
                                hr_set_clear(operand);
                                hr_set_add_range(operand, first, last);
                                hr_set_subtract(set, operand);
                            } else if width == 1 {
                                hr_set_del(set, first);
                            } else {
                                hr_set_del_range(set, first, last);
                            }
                        }
                        assert_eq!(hr_set_has(set, count * stride), i32::from(inverted));
                        hr_set_destroy(operand);
                        hr_set_destroy(set);
                    }
                }
            }
        }
    }
}

#[test]
fn indexed_set_mutations_match_intset_in_all_inversion_combinations() {
    use read_fonts::collections::IntSet;

    unsafe fn check_ranges(set: *const hr_set_t, expected: &IntSet<u32>) {
        let expected: Vec<_> = expected
            .iter_ranges()
            .filter_map(|range| {
                let (first, last) = (*range.start(), (*range.end()).min(INVALID - 1));
                (first <= last).then_some((first, last))
            })
            .collect();
        assert_eq!(unsafe { ranges(set, false) }, expected);
        assert_eq!(
            unsafe { ranges(set, true) },
            expected.into_iter().rev().collect::<Vec<_>>()
        );
    }

    type Op = unsafe extern "C" fn(*mut hr_set_t, *const hr_set_t);
    type ModelOp = fn(&mut IntSet<u32>, &IntSet<u32>);
    let operations: [(Op, ModelOp); 5] = [
        (hr_set_set, IntSet::clone_from),
        (hr_set_union, IntSet::union),
        (hr_set_intersect, IntSet::intersect),
        (hr_set_subtract, IntSet::subtract),
        (hr_set_symmetric_difference, |a, b| {
            let mut right = b.clone();
            right.subtract(a);
            a.subtract(b);
            a.union(&right);
        }),
    ];
    unsafe {
        for a_inverted in [false, true] {
            for b_inverted in [false, true] {
                for (op, model_op) in operations {
                    let a = hr_set_create();
                    let b = hr_set_create();
                    let mut model_a = IntSet::empty();
                    let mut model_b = IntSet::empty();
                    for (first, last) in [
                        (0, 31),
                        (40, 100),
                        (109, 127),
                        (255, 260),
                        (INVALID - 32, INVALID - 1),
                    ] {
                        hr_set_add_range(a, first, last);
                        model_a.insert_range(first..=last);
                    }
                    for (first, last) in [
                        (17, 39),
                        (110, 130),
                        (259, 259),
                        (INVALID - 16, INVALID - 1),
                    ] {
                        hr_set_add_range(b, first, last);
                        model_b.insert_range(first..=last);
                    }
                    if a_inverted {
                        hr_set_invert(a);
                        model_a.invert();
                    }
                    if b_inverted {
                        hr_set_invert(b);
                        model_b.invert();
                    }
                    // Warm the range index before each mutation, then validate
                    // through both directions rather than only bit membership.
                    check_ranges(a, &model_a);
                    op(a, b);
                    model_op(&mut model_a, &model_b);
                    check_ranges(a, &model_a);
                    for (first, last, include) in [
                        (50, 70, true),
                        (60, 65, false),
                        (66, 75, true),
                        (0, 0, false),
                        (INVALID - 1, INVALID - 1, true),
                    ] {
                        if include {
                            hr_set_add_range(a, first, last);
                            model_a.insert_range(first..=last);
                        } else {
                            hr_set_del_range(a, first, last);
                            model_a.remove_range(first..=last);
                        }
                        check_ranges(a, &model_a);
                    }
                    let added = [5, 12, 200];
                    hr_set_add_sorted_array(a, added.as_ptr(), added.len() as c_uint);
                    model_a.extend(added);
                    check_ranges(a, &model_a);
                    hr_set_invert(a);
                    model_a.invert();
                    check_ranges(a, &model_a);
                    let copy = hr_set_copy(a);
                    check_ranges(copy, &model_a);
                    hr_set_destroy(copy);
                    hr_set_destroy(a);
                    hr_set_destroy(b);
                }
            }
        }
    }
}
