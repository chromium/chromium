//! Find the optimal data mode sequence to encode a piece of data.
use crate::types::{Mode, Version};
use std::borrow::Borrow;
use std::slice::Iter;

//------------------------------------------------------------------------------
//{{{ Segment

/// A segment of data committed to an encoding mode.
#[derive(PartialEq, Eq, Debug, Copy, Clone)]
pub struct Segment {
    /// The encoding mode of the segment of data.
    pub mode: Mode,

    /// The start index of the segment.
    pub begin: usize,

    /// The end index (exclusive) of the segment.
    pub end: usize,
}

impl Segment {
    /// Compute the number of bits (including the size of the mode indicator and
    /// length bits) when this segment is encoded.
    pub fn encoded_len(&self, version: Version) -> usize {
        let byte_size = self.end - self.begin;
        let chars_count = if self.mode == Mode::Kanji {
            byte_size / 2
        } else {
            byte_size
        };

        let mode_bits_count = version.mode_bits_count();
        let length_bits_count = self.mode.length_bits_count(version);
        let data_bits_count = self.mode.data_bits_count(chars_count);

        mode_bits_count + length_bits_count + data_bits_count
    }

    /// Panics if `&self` is not a valid segment.
    fn assert_invariants(&self) {
        // TODO: It would be great if it would be impossible to construct an invalid `Segment` -
        // either by 1) making the fields private and only allowing construction via a public,
        // preconditions-checking API, or 2) keeping the fields public but replacing `end: usize`
        // with `len: NonZeroUsize`.
        assert!(self.begin < self.end);
    }
}

//}}}
//------------------------------------------------------------------------------
//{{{ Parser

/// This iterator is basically equivalent to
///
/// ```ignore
/// data.map(|c| ExclCharSet::from_u8(*c))
///     .chain(Some(ExclCharSet::End).move_iter())
///     .enumerate()
/// ```
///
/// But the type is too hard to write, thus the new type.
///
struct EcsIter<I> {
    base: I,
    index: usize,
    ended: bool,
}

impl<'a, I: Iterator<Item = &'a u8>> Iterator for EcsIter<I> {
    type Item = (usize, ExclCharSet);

    fn next(&mut self) -> Option<(usize, ExclCharSet)> {
        if self.ended {
            return None;
        }

        match self.base.next() {
            None => {
                self.ended = true;
                Some((self.index, ExclCharSet::End))
            }
            Some(c) => {
                let old_index = self.index;
                self.index += 1;
                Some((old_index, ExclCharSet::from_u8(*c)))
            }
        }
    }
}

/// QR code data parser to classify the input into distinct segments.
pub struct Parser<'a> {
    ecs_iter: EcsIter<Iter<'a, u8>>,
    state: State,
    begin: usize,
    pending_single_byte: bool,
}

impl<'a> Parser<'a> {
    /// Creates a new iterator which parse the data into segments that only
    /// contains their exclusive subsets. No optimization is done at this point.
    ///
    ///     use qr_code::optimize::{Parser, Segment};
    ///     use qr_code::types::Mode::{Alphanumeric, Numeric, Byte};
    ///
    ///     let parse_res = Parser::new(b"ABC123abcd").collect::<Vec<Segment>>();
    ///     assert_eq!(parse_res, vec![Segment { mode: Alphanumeric, begin: 0, end: 3 },
    ///                                Segment { mode: Numeric, begin: 3, end: 6 },
    ///                                Segment { mode: Byte, begin: 6, end: 10 }]);
    ///
    pub fn new(data: &[u8]) -> Parser {
        Parser {
            ecs_iter: EcsIter {
                base: data.iter(),
                index: 0,
                ended: false,
            },
            state: State::Init,
            begin: 0,
            pending_single_byte: false,
        }
    }
}

impl<'a> Iterator for Parser<'a> {
    type Item = Segment;

    fn next(&mut self) -> Option<Segment> {
        if self.pending_single_byte {
            self.pending_single_byte = false;
            self.begin += 1;
            return Some(Segment {
                mode: Mode::Byte,
                begin: self.begin - 1,
                end: self.begin,
            });
        }

        loop {
            let (i, ecs) = match self.ecs_iter.next() {
                None => return None,
                Some(a) => a,
            };
            let (next_state, action) = STATE_TRANSITION[self.state as usize + ecs as usize];
            self.state = next_state;

            let old_begin = self.begin;
            let push_mode = match action {
                Action::Idle => continue,
                Action::Numeric => Mode::Numeric,
                Action::Alpha => Mode::Alphanumeric,
                Action::Byte => Mode::Byte,
                Action::Kanji => Mode::Kanji,
                Action::KanjiAndSingleByte => {
                    let next_begin = i - 1;
                    if self.begin == next_begin {
                        Mode::Byte
                    } else {
                        self.pending_single_byte = true;
                        self.begin = next_begin;
                        return Some(Segment {
                            mode: Mode::Kanji,
                            begin: old_begin,
                            end: next_begin,
                        });
                    }
                }
            };

            self.begin = i;
            return Some(Segment {
                mode: push_mode,
                begin: old_begin,
                end: i,
            });
        }
    }
}

#[cfg(test)]
mod parse_tests {
    use crate::optimize::{Parser, Segment};
    use crate::types::Mode;

    fn parse(data: &[u8]) -> Vec<Segment> {
        Parser::new(data).collect()
    }

    #[test]
    fn test_parse_1() {
        let segs = parse(b"01049123451234591597033130128%10ABC123");
        assert_eq!(
            segs,
            vec![
                Segment {
                    mode: Mode::Numeric,
                    begin: 0,
                    end: 29
                },
                Segment {
                    mode: Mode::Alphanumeric,
                    begin: 29,
                    end: 30
                },
                Segment {
                    mode: Mode::Numeric,
                    begin: 30,
                    end: 32
                },
                Segment {
                    mode: Mode::Alphanumeric,
                    begin: 32,
                    end: 35
                },
                Segment {
                    mode: Mode::Numeric,
                    begin: 35,
                    end: 38
                },
            ]
        );
    }

    #[test]
    fn test_parse_shift_jis_example_1() {
        let segs = parse(b"\x82\xa0\x81\x41\x41\xb1\x81\xf0"); // "あ、AｱÅ"
        assert_eq!(
            segs,
            vec![
                Segment {
                    mode: Mode::Kanji,
                    begin: 0,
                    end: 4
                },
                Segment {
                    mode: Mode::Alphanumeric,
                    begin: 4,
                    end: 5
                },
                Segment {
                    mode: Mode::Byte,
                    begin: 5,
                    end: 6
                },
                Segment {
                    mode: Mode::Kanji,
                    begin: 6,
                    end: 8
                },
            ]
        );
    }

    #[test]
    fn test_parse_utf_8() {
        // Mojibake?
        let segs = parse(b"\xe3\x81\x82\xe3\x80\x81A\xef\xbd\xb1\xe2\x84\xab");
        assert_eq!(
            segs,
            vec![
                Segment {
                    mode: Mode::Kanji,
                    begin: 0,
                    end: 4
                },
                Segment {
                    mode: Mode::Byte,
                    begin: 4,
                    end: 5
                },
                Segment {
                    mode: Mode::Kanji,
                    begin: 5,
                    end: 7
                },
                Segment {
                    mode: Mode::Byte,
                    begin: 7,
                    end: 10
                },
                Segment {
                    mode: Mode::Kanji,
                    begin: 10,
                    end: 12
                },
                Segment {
                    mode: Mode::Byte,
                    begin: 12,
                    end: 13
                },
            ]
        );
    }

    #[test]
    fn test_not_kanji_1() {
        let segs = parse(b"\x81\x30");
        assert_eq!(
            segs,
            vec![
                Segment {
                    mode: Mode::Byte,
                    begin: 0,
                    end: 1
                },
                Segment {
                    mode: Mode::Numeric,
                    begin: 1,
                    end: 2
                },
            ]
        );
    }

    #[test]
    fn test_not_kanji_2() {
        // Note that it's implementation detail that the byte seq is split into
        // two. Perhaps adjust the test to check for this.
        let segs = parse(b"\xeb\xc0");
        assert_eq!(
            segs,
            vec![
                Segment {
                    mode: Mode::Byte,
                    begin: 0,
                    end: 1
                },
                Segment {
                    mode: Mode::Byte,
                    begin: 1,
                    end: 2
                },
            ]
        );
    }

    #[test]
    fn test_not_kanji_3() {
        let segs = parse(b"\x81\x7f");
        assert_eq!(
            segs,
            vec![
                Segment {
                    mode: Mode::Byte,
                    begin: 0,
                    end: 1
                },
                Segment {
                    mode: Mode::Byte,
                    begin: 1,
                    end: 2
                },
            ]
        );
    }

    #[test]
    fn test_not_kanji_4() {
        let segs = parse(b"\x81\x40\x81");
        assert_eq!(
            segs,
            vec![
                Segment {
                    mode: Mode::Kanji,
                    begin: 0,
                    end: 2
                },
                Segment {
                    mode: Mode::Byte,
                    begin: 2,
                    end: 3
                },
            ]
        );
    }
}

/// Implementation of a shortest path algorithm in a graph where:
///
/// * Graph nodes represent an encoding that covers an initial slice of segments and ends with a
///   given `Mode`.  For an input of N segments, the graph will contain at most 4 * N nodes (maybe
///   less, because some reencodings are not possible - e.g. it is not possible to reencode a
///   `Byte` segment as `Numeric`).
/// * Graph edges connect encodings that cover N segments with encodings
///   that cover 1 additional segment.  Because of possible `Mode` transitions
///   nodes can have up to 4 incoming edges and up to 4 outgoing edges.
///
/// The algorithm follows the relaxation approach of the
/// https://en.wikipedia.org/wiki/Dijkstra%27s_algorithm, but considers the nodes and edges in a
/// fixed order (rather than using a priority queue).  This is possible because of the constrained,
/// structured shape of the graph we are working with.
mod shortest_path {
    use super::Segment;
    use crate::types::{Mode, Version};
    use std::cmp::{Ordering, PartialOrd};

    #[derive(Clone)]
    struct Predecessor {
        index_of_segment: usize,
        mode: Mode,
    }

    #[derive(Clone)]
    struct ShortestPath {
        length: usize,
        predecessor: Option<Predecessor>,
    }

    pub struct AlgorithmState<'a> {
        /// Stores shortest paths - see `get_shortest_path` and `get_path_index` for more details.
        paths: Vec<Option<ShortestPath>>,

        /// Initial segmentation that we are trying to improve.
        segments: &'a [Segment],

        /// QR code version that determines how many bits are needed to encode a given `Segment`.
        version: Version,
    }

    /// Finds the index into `AlgorithmState::paths` for a path that:
    /// * Includes all the `0..=index_of_segment` segments from `AlgorithmState::segments`
    ///   (this range is inclusive on both ends).
    /// * Encodes the last segment (i.e. the one at `index_of_segment`) using `mode`
    fn get_path_index(index_of_segment: usize, mode: Mode) -> usize {
        index_of_segment * Mode::ALL_MODES.len() + (mode as usize)
    }

    impl<'a> AlgorithmState<'a> {
        /// Gets the shortest path that has been computed earlier.
        ///
        /// * For more details about `ending_mode` and `index_of_segment` see the documentation
        ///   of `get_path_index`
        /// * For more details about the return value see the documentation of
        ///   `compute_shortest_path`.
        fn get_shortest_path(
            &self,
            index_of_segment: usize,
            ending_mode: Mode,
        ) -> &Option<ShortestPath> {
            &self.paths[get_path_index(index_of_segment, ending_mode)]
        }

        fn get_end_of_predecessor(&self, predecessor: &Option<Predecessor>) -> usize {
            match predecessor {
                None => 0,
                Some(predecessor) => self.segments[predecessor.index_of_segment].end,
            }
        }

        /// Computes the shortest path that
        /// * Includes all the segments in `self.segments[0..=index_of_segment]`
        /// * Encodes the last segment (i.e. the one at `index_of_segment`) using `desired_mode`
        ///
        /// Assumes that shortest paths for all earlier segments (i.e. ones before
        /// `index_of_segment) have already been computed and memoized in `self.paths`.
        ///
        /// Returns `None` if the segment at `index_of_segment` can't be encoded using
        /// `desired_mode` (e.g. contents of `Mode::Byte` segment can't be reencoded using
        /// `Mode::Numeric` - see also the documentation of `PartialOrd` impl for `Mode`).
        fn compute_shortest_path(
            &self,
            index_of_segment: usize,
            desired_mode: Mode,
        ) -> Option<ShortestPath> {
            let curr_segment = self.segments[index_of_segment];

            // Check if `curr_segment` can be reencoded using `desired_mode`.
            match curr_segment.mode.partial_cmp(&desired_mode) {
                None | Some(Ordering::Greater) => return None,
                Some(Ordering::Less) | Some(Ordering::Equal) => (),
            }

            let length_of_reencoding_curr_segment_in_desired_mode = {
                let reencoded_segment = Segment {
                    begin: curr_segment.begin,
                    end: curr_segment.end,
                    mode: desired_mode,
                };
                reencoded_segment.encoded_len(self.version)
            };

            // Handle the case when there are no predecessors.
            if index_of_segment == 0 {
                return Some(ShortestPath {
                    length: length_of_reencoding_curr_segment_in_desired_mode,
                    predecessor: None,
                });
            }

            // Find the predecessor with the best mode / compute the shortest path.
            let prev_index = index_of_segment - 1;
            Mode::ALL_MODES
                .iter()
                .filter_map(|&prev_mode| {
                    self.get_shortest_path(prev_index, prev_mode)
                        .as_ref()
                        .map(|prev_path| (prev_mode, prev_path))
                })
                .map(|(prev_mode, prev_path)| {
                    if prev_mode == desired_mode {
                        let merged_length = {
                            let merged_segment = Segment {
                                begin: self.get_end_of_predecessor(&prev_path.predecessor),
                                end: curr_segment.end,
                                mode: desired_mode,
                            };
                            merged_segment.encoded_len(self.version)
                        };
                        let length_up_to_merged_segment = match prev_path.predecessor.as_ref() {
                            None => 0,
                            Some(predecessor) => {
                                self.get_shortest_path(
                                    predecessor.index_of_segment,
                                    predecessor.mode,
                                )
                                .as_ref()
                                .expect("Predecessors should point at a valid path")
                                .length
                            }
                        };
                        ShortestPath {
                            length: length_up_to_merged_segment + merged_length,
                            predecessor: prev_path.predecessor.clone(),
                        }
                    } else {
                        ShortestPath {
                            length: prev_path.length
                                + length_of_reencoding_curr_segment_in_desired_mode,
                            predecessor: Some(Predecessor {
                                index_of_segment: prev_index,
                                mode: prev_mode,
                            }),
                        }
                    }
                })
                .min_by_key(|path| path.length)
        }

        /// Runs the shortest-path algorithm, fully populating `Self::paths`.
        pub fn find_shortest_paths(segments: &'a [Segment], version: Version) -> Self {
            let mut this = AlgorithmState::<'a> {
                segments,
                paths: vec![None; segments.len() * Mode::ALL_MODES.len()],
                version,
            };
            for index_of_segment in 0..segments.len() {
                for &mode in Mode::ALL_MODES {
                    this.paths[get_path_index(index_of_segment, mode)] =
                        this.compute_shortest_path(index_of_segment, mode);
                }
            }
            this
        }

        /// Constructs the best segmentation from prepopulated `self.paths`.
        pub fn construct_best_segmentation(&self) -> Vec<Segment> {
            let mut result = Vec::new();

            // Start at the last segment (since we want the best encoding
            // that covers all of the input segments) and find the mode that
            // results in the shortest path for the last segment.
            let mut curr_index_of_segment = self.segments.len() - 1;
            let (mut best_mode, mut shortest_path) = Mode::ALL_MODES
                .iter()
                .filter_map(|&mode| {
                    self.get_shortest_path(curr_index_of_segment, mode)
                        .as_ref()
                        .map(|shortest_path| (mode, shortest_path))
                })
                .min_by_key(|(_mode, path)| path.length)
                .expect("At least one path should always exist");

            // Work backwards to construct the shortest path based on the predecessor information.
            loop {
                result.push(Segment {
                    begin: self.get_end_of_predecessor(&shortest_path.predecessor),
                    end: self.segments[curr_index_of_segment].end,
                    mode: best_mode,
                });
                match shortest_path.predecessor.as_ref() {
                    None => {
                        result.reverse();
                        return result;
                    }
                    Some(predecessor) => {
                        curr_index_of_segment = predecessor.index_of_segment;
                        best_mode = predecessor.mode;
                        shortest_path = self
                            .get_shortest_path(curr_index_of_segment, best_mode)
                            .as_ref()
                            .expect("Predecessors should point at valid paths");
                    }
                };
            }
        }
    }
}

/// Panics if `segments` are not consecutive (i.e. if there are gaps, or overlaps, or if the
/// segments are not ordered by their `begin` field).
fn assert_valid_segmentation(segments: &[Segment]) {
    if segments.is_empty() {
        return;
    }

    for segment in segments.iter() {
        segment.assert_invariants();
    }

    let consecutive_pairs = segments[..(segments.len() - 1)]
        .iter()
        .zip(segments[1..].iter());
    for (prev, next) in consecutive_pairs {
        assert_eq!(prev.end, next.begin, "Non-adjacent segments");
    }
}

/// Optimize the segments by combining segments when possible.
///
/// Optimization considers all possible segmentations where each of the input `segments`
/// may have been reencoded into a different mode (and where adjacent, same-mode segments are
/// merged).  The shortest of the considered segmentations is returned.  The implementation uses a
/// shortest-path algorithm that runs in O(N) time and uses additional O(N) memory.
///
/// This function may panic if `segments` do not represent a valid segmentation
/// (e.g. if there are gaps between segments, if the segments overlap, etc.).
pub fn optimize_segmentation(segments: &[Segment], version: Version) -> Vec<Segment> {
    assert_valid_segmentation(segments);
    if segments.is_empty() {
        return Vec::new();
    }

    let optimized_segments = shortest_path::AlgorithmState::find_shortest_paths(segments, version)
        .construct_best_segmentation();

    #[cfg(fuzzing)]
    optimize_test_helpers::assert_valid_optimization(&segments, &*optimized_segments, version);

    optimized_segments
}

/// Computes the total encoded length of all segments.
pub fn total_encoded_len<I>(segments: I, version: Version) -> usize
where
    I: IntoIterator,
    I::Item: Borrow<Segment>,
{
    segments
        .into_iter()
        .map(|seg| seg.borrow().encoded_len(version))
        .sum()
}

#[cfg(any(fuzzing, test))]
mod optimize_test_helpers {
    use super::{assert_valid_segmentation, total_encoded_len, Segment};
    use crate::types::{Mode, Version};
    use std::cmp::Ordering;

    /// Panics if there exists an input that can be represented by `given` segments,
    /// but that cannot be represented by `opt` segments.
    pub fn assert_segmentation_equivalence(given: &[Segment], opt: &[Segment]) {
        if given.is_empty() {
            assert!(opt.is_empty());
            return;
        }
        let begin = given.first().unwrap().begin;
        let end = given.last().unwrap().end;

        // Verify that `opt` covers the same range as `given`.
        // (This assumes that contiguous coverage has already been verified by
        // `assert_valid_segmentation`.)
        assert!(!opt.is_empty());
        assert_eq!(begin, opt.first().unwrap().begin);
        assert_eq!(end, opt.last().unwrap().end);

        // Verify that for each character, `opt` can represent all the characters that may be
        // present in `given`.
        for i in begin..end {
            fn find_mode(segments: &[Segment], i: usize) {
                segments
                    .iter()
                    .filter(|s| (s.begin <= i) && (i < s.end))
                    .next()
                    .expect("Expecting exactly one segment")
                    .mode;
            }
            let given_mode = find_mode(&*given, i);
            let opt_mode = find_mode(&*given, i);
            match given_mode.partial_cmp(&opt_mode) {
                Some(Ordering::Less) | Some(Ordering::Equal) => (),
                _ => panic!(
                    "Character #{} is {:?}, which {:?} may not represent",
                    i, given_mode, opt_mode,
                ),
            }
        }
    }

    /// Panics if `optimized` is not an improved representation of `input`.
    pub fn assert_valid_optimization(input: &[Segment], optimized: &[Segment], version: Version) {
        assert_valid_segmentation(input);
        assert_valid_segmentation(optimized);
        assert_segmentation_equivalence(input, optimized);

        let input_len = total_encoded_len(input, version);
        let optimized_len = total_encoded_len(optimized, version);
        assert!(optimized_len <= input_len);

        let single_bytes_segment_len = if input.is_empty() {
            0
        } else {
            Segment {
                begin: input.first().unwrap().begin,
                end: input.last().unwrap().end,
                mode: Mode::Byte,
            }
            .encoded_len(version)
        };
        assert!(optimized_len <= single_bytes_segment_len);
    }
}

#[cfg(test)]
mod optimize_tests {
    use super::optimize_test_helpers::*;
    use super::{assert_valid_segmentation, optimize_segmentation, total_encoded_len, Segment};
    use crate::types::{Mode, Version};
    use std::cmp::Ordering;

    fn test_optimization_result(given: Vec<Segment>, expected: Vec<Segment>, version: Version) {
        // Verify that the test input is valid.
        assert_valid_segmentation(&given);

        // Verify that the test expectations are compatible with the test input.
        assert_valid_segmentation(&expected);
        assert_segmentation_equivalence(&given, &expected);

        let opt_segs = optimize_segmentation(given.as_slice(), version);
        assert_valid_optimization(&given, &opt_segs, version);

        // Verify that optimization produces result that is as short as `expected`.
        if opt_segs != expected {
            let actual_len = total_encoded_len(&opt_segs, version);
            let expected_len = total_encoded_len(&expected, version);
            let msg = match actual_len.cmp(&expected_len) {
                Ordering::Less => "Optimization gave something better than expected",
                Ordering::Equal => "Optimization gave something different, but just as short",
                Ordering::Greater => "Optimization gave something worse than expected",
            };
            panic!(
                "{}: expected_len={}; actual_len={}; opt_segs=({:?})",
                msg, expected_len, actual_len, opt_segs
            );
        }
    }

    #[test]
    fn test_example_1() {
        test_optimization_result(
            vec![
                Segment {
                    mode: Mode::Alphanumeric,
                    begin: 0,
                    end: 3,
                },
                Segment {
                    mode: Mode::Numeric,
                    begin: 3,
                    end: 6,
                },
                Segment {
                    mode: Mode::Byte,
                    begin: 6,
                    end: 10,
                },
            ],
            vec![
                Segment {
                    mode: Mode::Alphanumeric,
                    begin: 0,
                    end: 6,
                },
                Segment {
                    mode: Mode::Byte,
                    begin: 6,
                    end: 10,
                },
            ],
            Version::Normal(1),
        );
    }

    #[test]
    fn test_example_2() {
        test_optimization_result(
            vec![
                Segment {
                    mode: Mode::Numeric,
                    begin: 0,
                    end: 29,
                },
                Segment {
                    mode: Mode::Alphanumeric,
                    begin: 29,
                    end: 30,
                },
                Segment {
                    mode: Mode::Numeric,
                    begin: 30,
                    end: 32,
                },
                Segment {
                    mode: Mode::Alphanumeric,
                    begin: 32,
                    end: 35,
                },
                Segment {
                    mode: Mode::Numeric,
                    begin: 35,
                    end: 38,
                },
            ],
            vec![
                Segment {
                    mode: Mode::Numeric,
                    begin: 0,
                    end: 29,
                },
                Segment {
                    mode: Mode::Alphanumeric,
                    begin: 29,
                    end: 38,
                },
            ],
            Version::Normal(9),
        );
    }

    #[test]
    fn test_example_3() {
        test_optimization_result(
            vec![
                Segment {
                    mode: Mode::Kanji,
                    begin: 0,
                    end: 4,
                },
                Segment {
                    mode: Mode::Alphanumeric,
                    begin: 4,
                    end: 5,
                },
                Segment {
                    mode: Mode::Byte,
                    begin: 5,
                    end: 6,
                },
                Segment {
                    mode: Mode::Kanji,
                    begin: 6,
                    end: 8,
                },
            ],
            vec![Segment {
                mode: Mode::Byte,
                begin: 0,
                end: 8,
            }],
            Version::Normal(1),
        );
    }

    #[test]
    fn test_example_4() {
        test_optimization_result(
            vec![
                Segment {
                    mode: Mode::Kanji,
                    begin: 0,
                    end: 10,
                },
                Segment {
                    mode: Mode::Byte,
                    begin: 10,
                    end: 11,
                },
            ],
            vec![
                Segment {
                    mode: Mode::Kanji,
                    begin: 0,
                    end: 10,
                },
                Segment {
                    mode: Mode::Byte,
                    begin: 10,
                    end: 11,
                },
            ],
            Version::Normal(1),
        );
    }

    #[test]
    fn test_empty() {
        test_optimization_result(vec![], vec![], Version::Normal(1));
    }

    /// This example shows that greedy merging strategy (used by `qr_code` v1.1.0) can give
    /// suboptimal results for the following input: `"bytesbytesA1B2C3D4E5"`.  The greedy strategy
    /// will always consider it (locally) beneficial to merge the initial `Mode::Byte` segment with
    /// the subsequent single-char segment - this will result in representing the whole input
    /// in a single `Mode::Byte` segment.  Better segmentation can be done by first merging all the
    /// `Mode::Alphanumeric` and `Mode::Numeric` segments.
    #[test]
    fn test_example_where_greedy_merging_is_suboptimal() {
        let mut input = vec![Segment {
            mode: Mode::Byte,
            begin: 0,
            end: 10,
        }];
        for _ in 0..5 {
            for &mode in [Mode::Alphanumeric, Mode::Numeric].iter() {
                let begin = input.last().unwrap().end;
                let end = begin + 1;
                input.push(Segment { mode, begin, end });
            }
        }
        test_optimization_result(
            input,
            vec![
                Segment {
                    mode: Mode::Byte,
                    begin: 0,
                    end: 10,
                },
                Segment {
                    mode: Mode::Alphanumeric,
                    begin: 10,
                    end: 20,
                },
            ],
            Version::Normal(40),
        );
    }

    /// In this example merging 2 consecutive segments is always harmful, but merging many segments
    /// may be beneficial.  This is because merging more than 2 segments saves additional header
    /// bytes.
    #[test]
    fn test_example_where_merging_two_consecutive_segments_is_always_harmful() {
        let mut input = vec![];
        for _ in 0..5 {
            for &mode in [Mode::Byte, Mode::Alphanumeric].iter() {
                let begin = input.last().map(|s: &Segment| s.end).unwrap_or_default();
                let end = begin + 10;
                input.push(Segment { mode, begin, end });
            }
        }
        test_optimization_result(
            input,
            vec![
                Segment {
                    mode: Mode::Byte,
                    begin: 0,
                    end: 90,
                },
                Segment {
                    mode: Mode::Alphanumeric,
                    begin: 90,
                    end: 100,
                },
            ],
            Version::Normal(40),
        );
    }

    #[test]
    fn test_optimize_base64() {
        let input: &[u8] = include_bytes!("../test_data/large_base64.in");
        let input: Vec<Segment> = super::Parser::new(input).collect();
        test_optimization_result(
            input,
            vec![
                Segment {
                    mode: Mode::Byte,
                    begin: 0,
                    end: 334,
                },
                Segment {
                    mode: Mode::Alphanumeric,
                    begin: 334,
                    end: 349,
                },
                Segment {
                    mode: Mode::Byte,
                    begin: 349,
                    end: 387,
                },
                Segment {
                    mode: Mode::Alphanumeric,
                    begin: 387,
                    end: 402,
                },
                Segment {
                    mode: Mode::Byte,
                    begin: 402,
                    end: 850,
                },
                Segment {
                    mode: Mode::Alphanumeric,
                    begin: 850,
                    end: 866,
                },
                Segment {
                    mode: Mode::Byte,
                    begin: 866,
                    end: 1146,
                },
                Segment {
                    mode: Mode::Alphanumeric,
                    begin: 1146,
                    end: 1162,
                },
                Segment {
                    mode: Mode::Byte,
                    begin: 1162,
                    end: 2474,
                },
                Segment {
                    mode: Mode::Alphanumeric,
                    begin: 2474,
                    end: 2489,
                },
                Segment {
                    mode: Mode::Byte,
                    begin: 2489,
                    end: 2618,
                },
                Segment {
                    mode: Mode::Alphanumeric,
                    begin: 2618,
                    end: 2641,
                },
                Segment {
                    mode: Mode::Byte,
                    begin: 2641,
                    end: 2707,
                },
                Segment {
                    mode: Mode::Alphanumeric,
                    begin: 2707,
                    end: 2722,
                },
                Segment {
                    mode: Mode::Byte,
                    begin: 2722,
                    end: 2880,
                },
            ],
            Version::Normal(40),
        );
    }

    #[test]
    fn test_annex_j_guideline_1a() {
        test_optimization_result(
            vec![
                Segment {
                    mode: Mode::Numeric,
                    begin: 0,
                    end: 3,
                },
                Segment {
                    mode: Mode::Alphanumeric,
                    begin: 3,
                    end: 4,
                },
            ],
            vec![
                Segment {
                    mode: Mode::Numeric,
                    begin: 0,
                    end: 3,
                },
                Segment {
                    mode: Mode::Alphanumeric,
                    begin: 3,
                    end: 4,
                },
            ],
            Version::Micro(2),
        );
    }

    #[test]
    fn test_annex_j_guideline_1b() {
        test_optimization_result(
            vec![
                Segment {
                    mode: Mode::Numeric,
                    begin: 0,
                    end: 2,
                },
                Segment {
                    mode: Mode::Alphanumeric,
                    begin: 2,
                    end: 4,
                },
            ],
            vec![Segment {
                mode: Mode::Alphanumeric,
                begin: 0,
                end: 4,
            }],
            Version::Micro(2),
        );
    }

    #[test]
    fn test_annex_j_guideline_1c() {
        test_optimization_result(
            vec![
                Segment {
                    mode: Mode::Numeric,
                    begin: 0,
                    end: 3,
                },
                Segment {
                    mode: Mode::Alphanumeric,
                    begin: 3,
                    end: 4,
                },
            ],
            vec![Segment {
                mode: Mode::Alphanumeric,
                begin: 0,
                end: 4,
            }],
            Version::Micro(3),
        );
    }
}

#[cfg(bench)]
mod bench {
    use super::{optimize_segmentation, total_encoded_len, Parser, Segment};
    use crate::Version;

    fn bench_optimize(data: &[u8], version: Version, bencher: &mut test::Bencher) {
        bencher.iter(|| {
            let segments: Vec<Segment> = Parser::new(data).collect();
            let segments = optimize_segmentation(segments.as_slice(), version);
            test::black_box(total_encoded_len(segments, version))
        });
    }

    #[bench]
    fn bench_optimize_example1(bencher: &mut test::Bencher) {
        let data = b"QR\x83R\x81[\x83h\x81i\x83L\x83\x85\x81[\x83A\x81[\x83\x8b\x83R\x81[\x83h\x81j\
                     \x82\xc6\x82\xcd\x81A1994\x94N\x82\xc9\x83f\x83\x93\x83\\\x81[\x82\xcc\x8aJ\
                     \x94\xad\x95\x94\x96\xe5\x81i\x8c\xbb\x8d\xdd\x82\xcd\x95\xaa\x97\xa3\x82\xb5\x83f\
                     \x83\x93\x83\\\x81[\x83E\x83F\x81[\x83u\x81j\x82\xaa\x8aJ\x94\xad\x82\xb5\x82\xbd\
                     \x83}\x83g\x83\x8a\x83b\x83N\x83X\x8c^\x93\xf1\x8e\x9f\x8c\xb3\x83R\x81[\x83h\
                     \x82\xc5\x82\xa0\x82\xe9\x81B\x82\xc8\x82\xa8\x81AQR\x83R\x81[\x83h\x82\xc6\
                     \x82\xa2\x82\xa4\x96\xbc\x8f\xcc\x81i\x82\xa8\x82\xe6\x82\xd1\x92P\x8c\xea\x81j\
                     \x82\xcd\x83f\x83\x93\x83\\\x81[\x83E\x83F\x81[\x83u\x82\xcc\x93o\x98^\x8f\xa4\
                     \x95W\x81i\x91\xe64075066\x8d\x86\x81j\x82\xc5\x82\xa0\x82\xe9\x81BQR\x82\xcd\
                     Quick Response\x82\xc9\x97R\x97\x88\x82\xb5\x81A\x8d\x82\x91\xac\x93\xc7\x82\xdd\
                     \x8e\xe6\x82\xe8\x82\xaa\x82\xc5\x82\xab\x82\xe9\x82\xe6\x82\xa4\x82\xc9\x8aJ\
                     \x94\xad\x82\xb3\x82\xea\x82\xbd\x81B\x93\x96\x8f\x89\x82\xcd\x8e\xa9\x93\xae\
                     \x8e\xd4\x95\x94\x95i\x8dH\x8f\xea\x82\xe2\x94z\x91\x97\x83Z\x83\x93\x83^\x81[\
                     \x82\xc8\x82\xc7\x82\xc5\x82\xcc\x8eg\x97p\x82\xf0\x94O\x93\xaa\x82\xc9\x8aJ\
                     \x94\xad\x82\xb3\x82\xea\x82\xbd\x82\xaa\x81A\x8c\xbb\x8d\xdd\x82\xc5\x82\xcd\x83X\
                     \x83}\x81[\x83g\x83t\x83H\x83\x93\x82\xcc\x95\x81\x8by\x82\xc8\x82\xc7\x82\xc9\
                     \x82\xe6\x82\xe8\x93\xfa\x96{\x82\xc9\x8c\xc0\x82\xe7\x82\xb8\x90\xa2\x8aE\x93I\
                     \x82\xc9\x95\x81\x8by\x82\xb5\x82\xc4\x82\xa2\x82\xe9\x81B";
        bench_optimize(data, Version::Normal(15), bencher);
    }

    #[bench]
    fn bench_optimize_base64(bencher: &mut test::Bencher) {
        let data = include_bytes!("../test_data/large_base64.in");
        bench_optimize(data, Version::Normal(40), bencher);
    }
}

//}}}
//------------------------------------------------------------------------------
//{{{ Internal types and data for parsing

/// All values of `u8` can be split into 9 different character sets when
/// determining which encoding to use. This enum represents these groupings for
/// parsing purpose.
#[derive(Copy, Clone)]
enum ExclCharSet {
    /// The end of string.
    End = 0,

    /// All symbols supported by the Alphanumeric encoding, i.e. space, `$`, `%`,
    /// `*`, `+`, `-`, `.`, `/` and `:`.
    Symbol = 1,

    /// All numbers (0–9).
    Numeric = 2,

    /// All uppercase letters (A–Z). These characters may also appear in the
    /// second byte of a Shift JIS 2-byte encoding.
    Alpha = 3,

    /// The first byte of a Shift JIS 2-byte encoding, in the range 0x81–0x9f.
    KanjiHi1 = 4,

    /// The first byte of a Shift JIS 2-byte encoding, in the range 0xe0–0xea.
    KanjiHi2 = 5,

    /// The first byte of a Shift JIS 2-byte encoding, of value 0xeb. This is
    /// different from the other two range that the second byte has a smaller
    /// range.
    KanjiHi3 = 6,

    /// The second byte of a Shift JIS 2-byte encoding, in the range 0x40–0xbf,
    /// excluding letters (covered by `Alpha`), 0x81–0x9f (covered by `KanjiHi1`),
    /// and the invalid byte 0x7f.
    KanjiLo1 = 7,

    /// The second byte of a Shift JIS 2-byte encoding, in the range 0xc0–0xfc,
    /// excluding the range 0xe0–0xeb (covered by `KanjiHi2` and `KanjiHi3`).
    /// This half of byte-pair cannot appear as the second byte leaded by
    /// `KanjiHi3`.
    KanjiLo2 = 8,

    /// Any other values not covered by the above character sets.
    Byte = 9,
}

impl ExclCharSet {
    /// Determines which character set a byte is in.
    fn from_u8(c: u8) -> Self {
        match c {
            0x20 | 0x24 | 0x25 | 0x2a | 0x2b | 0x2d..=0x2f | 0x3a => ExclCharSet::Symbol,
            0x30..=0x39 => ExclCharSet::Numeric,
            0x41..=0x5a => ExclCharSet::Alpha,
            0x81..=0x9f => ExclCharSet::KanjiHi1,
            0xe0..=0xea => ExclCharSet::KanjiHi2,
            0xeb => ExclCharSet::KanjiHi3,
            0x40 | 0x5b..=0x7e | 0x80 | 0xa0..=0xbf => ExclCharSet::KanjiLo1,
            0xc0..=0xdf | 0xec..=0xfc => ExclCharSet::KanjiLo2,
            _ => ExclCharSet::Byte,
        }
    }
}

/// The current parsing state.
#[derive(Copy, Clone)]
enum State {
    /// Just initialized.
    Init = 0,

    /// Inside a string that can be exclusively encoded as Numeric.
    Numeric = 10,

    /// Inside a string that can be exclusively encoded as Alphanumeric.
    Alpha = 20,

    /// Inside a string that can be exclusively encoded as 8-Bit Byte.
    Byte = 30,

    /// Just encountered the first byte of a Shift JIS 2-byte sequence of the
    /// set `KanjiHi1` or `KanjiHi2`.
    KanjiHi12 = 40,

    /// Just encountered the first byte of a Shift JIS 2-byte sequence of the
    /// set `KanjiHi3`.
    KanjiHi3 = 50,

    /// Inside a string that can be exclusively encoded as Kanji.
    Kanji = 60,
}

/// What should the parser do after a state transition.
#[derive(Copy, Clone)]
enum Action {
    /// The parser should do nothing.
    Idle,

    /// Push the current segment as a Numeric string, and reset the marks.
    Numeric,

    /// Push the current segment as an Alphanumeric string, and reset the marks.
    Alpha,

    /// Push the current segment as a 8-Bit Byte string, and reset the marks.
    Byte,

    /// Push the current segment as a Kanji string, and reset the marks.
    Kanji,

    /// Push the current segment excluding the last byte as a Kanji string, then
    /// push the remaining single byte as a Byte string, and reset the marks.
    KanjiAndSingleByte,
}

static STATE_TRANSITION: [(State, Action); 70] = [
    // STATE_TRANSITION[current_state + next_character] == (next_state, what_to_do)

    // Init state:
    (State::Init, Action::Idle),      // End
    (State::Alpha, Action::Idle),     // Symbol
    (State::Numeric, Action::Idle),   // Numeric
    (State::Alpha, Action::Idle),     // Alpha
    (State::KanjiHi12, Action::Idle), // KanjiHi1
    (State::KanjiHi12, Action::Idle), // KanjiHi2
    (State::KanjiHi3, Action::Idle),  // KanjiHi3
    (State::Byte, Action::Idle),      // KanjiLo1
    (State::Byte, Action::Idle),      // KanjiLo2
    (State::Byte, Action::Idle),      // Byte
    // Numeric state:
    (State::Init, Action::Numeric),      // End
    (State::Alpha, Action::Numeric),     // Symbol
    (State::Numeric, Action::Idle),      // Numeric
    (State::Alpha, Action::Numeric),     // Alpha
    (State::KanjiHi12, Action::Numeric), // KanjiHi1
    (State::KanjiHi12, Action::Numeric), // KanjiHi2
    (State::KanjiHi3, Action::Numeric),  // KanjiHi3
    (State::Byte, Action::Numeric),      // KanjiLo1
    (State::Byte, Action::Numeric),      // KanjiLo2
    (State::Byte, Action::Numeric),      // Byte
    // Alpha state:
    (State::Init, Action::Alpha),      // End
    (State::Alpha, Action::Idle),      // Symbol
    (State::Numeric, Action::Alpha),   // Numeric
    (State::Alpha, Action::Idle),      // Alpha
    (State::KanjiHi12, Action::Alpha), // KanjiHi1
    (State::KanjiHi12, Action::Alpha), // KanjiHi2
    (State::KanjiHi3, Action::Alpha),  // KanjiHi3
    (State::Byte, Action::Alpha),      // KanjiLo1
    (State::Byte, Action::Alpha),      // KanjiLo2
    (State::Byte, Action::Alpha),      // Byte
    // Byte state:
    (State::Init, Action::Byte),      // End
    (State::Alpha, Action::Byte),     // Symbol
    (State::Numeric, Action::Byte),   // Numeric
    (State::Alpha, Action::Byte),     // Alpha
    (State::KanjiHi12, Action::Byte), // KanjiHi1
    (State::KanjiHi12, Action::Byte), // KanjiHi2
    (State::KanjiHi3, Action::Byte),  // KanjiHi3
    (State::Byte, Action::Idle),      // KanjiLo1
    (State::Byte, Action::Idle),      // KanjiLo2
    (State::Byte, Action::Idle),      // Byte
    // KanjiHi12 state:
    (State::Init, Action::KanjiAndSingleByte),    // End
    (State::Alpha, Action::KanjiAndSingleByte),   // Symbol
    (State::Numeric, Action::KanjiAndSingleByte), // Numeric
    (State::Kanji, Action::Idle),                 // Alpha
    (State::Kanji, Action::Idle),                 // KanjiHi1
    (State::Kanji, Action::Idle),                 // KanjiHi2
    (State::Kanji, Action::Idle),                 // KanjiHi3
    (State::Kanji, Action::Idle),                 // KanjiLo1
    (State::Kanji, Action::Idle),                 // KanjiLo2
    (State::Byte, Action::KanjiAndSingleByte),    // Byte
    // KanjiHi3 state:
    (State::Init, Action::KanjiAndSingleByte),      // End
    (State::Alpha, Action::KanjiAndSingleByte),     // Symbol
    (State::Numeric, Action::KanjiAndSingleByte),   // Numeric
    (State::Kanji, Action::Idle),                   // Alpha
    (State::Kanji, Action::Idle),                   // KanjiHi1
    (State::KanjiHi12, Action::KanjiAndSingleByte), // KanjiHi2
    (State::KanjiHi3, Action::KanjiAndSingleByte),  // KanjiHi3
    (State::Kanji, Action::Idle),                   // KanjiLo1
    (State::Byte, Action::KanjiAndSingleByte),      // KanjiLo2
    (State::Byte, Action::KanjiAndSingleByte),      // Byte
    // Kanji state:
    (State::Init, Action::Kanji),     // End
    (State::Alpha, Action::Kanji),    // Symbol
    (State::Numeric, Action::Kanji),  // Numeric
    (State::Alpha, Action::Kanji),    // Alpha
    (State::KanjiHi12, Action::Idle), // KanjiHi1
    (State::KanjiHi12, Action::Idle), // KanjiHi2
    (State::KanjiHi3, Action::Idle),  // KanjiHi3
    (State::Byte, Action::Kanji),     // KanjiLo1
    (State::Byte, Action::Kanji),     // KanjiLo2
    (State::Byte, Action::Kanji),     // Byte
];

//}}}
