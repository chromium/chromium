use super::{tail_byte_array, StartAndBytes};

#[test]
fn tail_byte_array_eq_gt_tests() {
    assert_eq!(
        tail_byte_array::<0>(0, &[3]),
        StartAndBytes {
            bytes: [],
            start: 0
        }
    );
    assert_eq!(
        tail_byte_array::<0>(0, &[3, 5]),
        StartAndBytes {
            bytes: [],
            start: 0
        }
    );
    assert_eq!(
        tail_byte_array::<1>(0, &[3, 5]),
        StartAndBytes {
            bytes: [0],
            start: 1
        }
    );
    assert_eq!(
        tail_byte_array::<1>(1, &[3, 5]),
        StartAndBytes {
            bytes: [3],
            start: 0
        }
    );
    assert_eq!(
        tail_byte_array::<2>(0, &[3, 5]),
        StartAndBytes {
            bytes: [0, 0],
            start: 2
        }
    );
    assert_eq!(
        tail_byte_array::<2>(1, &[3, 5]),
        StartAndBytes {
            bytes: [0, 3],
            start: 1
        }
    );
    assert_eq!(
        tail_byte_array::<2>(2, &[3, 5]),
        StartAndBytes {
            bytes: [3, 5],
            start: 0
        }
    );

    assert_eq!(
        tail_byte_array::<1>(0, &[]),
        StartAndBytes {
            bytes: [0],
            start: 1
        }
    );
    assert_eq!(
        tail_byte_array::<2>(0, &[]),
        StartAndBytes {
            bytes: [0, 0],
            start: 2
        }
    );

    assert_eq!(
        tail_byte_array::<1>(1, &[3]),
        StartAndBytes {
            bytes: [3],
            start: 0
        }
    );
    assert_eq!(
        tail_byte_array::<2>(1, &[3]),
        StartAndBytes {
            bytes: [0, 3],
            start: 1
        }
    );
    assert_eq!(
        tail_byte_array::<3>(1, &[3]),
        StartAndBytes {
            bytes: [0, 0, 3],
            start: 2
        }
    );

    assert_eq!(
        tail_byte_array::<2>(1, &[3, 5]),
        StartAndBytes {
            bytes: [0, 3],
            start: 1
        }
    );
    assert_eq!(
        tail_byte_array::<3>(1, &[3, 5]),
        StartAndBytes {
            bytes: [0, 0, 3],
            start: 2
        }
    );
    assert_eq!(
        tail_byte_array::<4>(1, &[3, 5]),
        StartAndBytes {
            bytes: [0, 0, 0, 3],
            start: 3
        }
    );

    assert_eq!(
        tail_byte_array::<2>(2, &[3, 5]),
        StartAndBytes {
            bytes: [3, 5],
            start: 0
        }
    );
    assert_eq!(
        tail_byte_array::<3>(2, &[3, 5]),
        StartAndBytes {
            bytes: [0, 3, 5],
            start: 1
        }
    );
    assert_eq!(
        tail_byte_array::<4>(2, &[3, 5]),
        StartAndBytes {
            bytes: [0, 0, 3, 5],
            start: 2
        }
    );
}

#[test]
#[should_panic]
fn tail_byte_array_smaller_test_0() {
    let _: StartAndBytes<0> = tail_byte_array(2, &[1]);
}

#[test]
#[should_panic]
fn tail_byte_array_smaller_test_1() {
    let _: StartAndBytes<1> = tail_byte_array(3, &[1, 2]);
}

#[test]
#[should_panic]
fn tail_byte_array_smaller_test_2() {
    let _: StartAndBytes<1> = tail_byte_array(3, &[1, 2, 3]);
}

#[test]
#[should_panic]
fn tail_byte_array_smaller_test_3() {
    let _: StartAndBytes<2> = tail_byte_array(3, &[1, 2, 3, 4, 5, 6]);
}
