// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::grapheme::GraphemeClusterSegmenter;
use crate::provider::*;
use alloc::vec::Vec;
use core::char::{decode_utf16, REPLACEMENT_CHARACTER};
use potential_utf::PotentialUtf8;
use zerovec::maps::ZeroMapBorrowed;

mod matrix;
use matrix::*;

// A word break iterator using LSTM model. Input string have to be same language.

struct LstmSegmenterIterator<'s> {
    input: &'s str,
    pos_utf8: usize,
    bies: BiesIterator<'s>,
}

impl Iterator for LstmSegmenterIterator<'_> {
    type Item = usize;

    fn next(&mut self) -> Option<Self::Item> {
        #[allow(clippy::indexing_slicing)] // pos_utf8 in range
        loop {
            let is_e = self.bies.next()?;
            self.pos_utf8 += self.input[self.pos_utf8..].chars().next()?.len_utf8();
            if is_e || self.bies.len() == 0 {
                return Some(self.pos_utf8);
            }
        }
    }
}

struct LstmSegmenterIteratorUtf16<'s> {
    bies: BiesIterator<'s>,
    pos: usize,
}

impl Iterator for LstmSegmenterIteratorUtf16<'_> {
    type Item = usize;

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            self.pos += 1;
            if self.bies.next()? || self.bies.len() == 0 {
                return Some(self.pos);
            }
        }
    }
}

pub(super) struct LstmSegmenter<'l> {
    dic: ZeroMapBorrowed<'l, PotentialUtf8, u16>,
    embedding: MatrixZero<'l, 2>,
    fw_w: MatrixZero<'l, 3>,
    fw_u: MatrixZero<'l, 3>,
    fw_b: MatrixZero<'l, 2>,
    bw_w: MatrixZero<'l, 3>,
    bw_u: MatrixZero<'l, 3>,
    bw_b: MatrixZero<'l, 2>,
    timew_fw: MatrixZero<'l, 2>,
    timew_bw: MatrixZero<'l, 2>,
    time_b: MatrixZero<'l, 1>,
    grapheme: Option<&'l RuleBreakDataV2<'l>>,
}

impl<'l> LstmSegmenter<'l> {
    /// Returns `Err` if grapheme data is required but not present
    pub(super) fn new(lstm: &'l LstmDataV1<'l>, grapheme: &'l RuleBreakDataV2<'l>) -> Self {
        let LstmDataV1::Float32(lstm) = lstm;
        let time_w = MatrixZero::from(&lstm.time_w);
        #[allow(clippy::unwrap_used)] // shape (2, 4, hunits)
        let timew_fw = time_w.submatrix(0).unwrap();
        #[allow(clippy::unwrap_used)] // shape (2, 4, hunits)
        let timew_bw = time_w.submatrix(1).unwrap();
        Self {
            dic: lstm.dic.as_borrowed(),
            embedding: MatrixZero::from(&lstm.embedding),
            fw_w: MatrixZero::from(&lstm.fw_w),
            fw_u: MatrixZero::from(&lstm.fw_u),
            fw_b: MatrixZero::from(&lstm.fw_b),
            bw_w: MatrixZero::from(&lstm.bw_w),
            bw_u: MatrixZero::from(&lstm.bw_u),
            bw_b: MatrixZero::from(&lstm.bw_b),
            timew_fw,
            timew_bw,
            time_b: MatrixZero::from(&lstm.time_b),
            grapheme: (lstm.model == ModelType::GraphemeClusters).then_some(grapheme),
        }
    }

    /// Create an LSTM based break iterator for an `str` (a UTF-8 string).
    pub(super) fn segment_str(&'l self, input: &'l str) -> impl Iterator<Item = usize> + 'l {
        self.segment_str_p(input)
    }

    // For unit testing as we cannot inspect the opaque type's bies
    fn segment_str_p(&'l self, input: &'l str) -> LstmSegmenterIterator<'l> {
        let input_seq = if let Some(grapheme) = self.grapheme {
            GraphemeClusterSegmenter::new_and_segment_str(input, grapheme)
                .collect::<Vec<usize>>()
                .windows(2)
                .map(|chunk| {
                    let range = if let [first, second, ..] = chunk {
                        *first..*second
                    } else {
                        unreachable!()
                    };
                    let grapheme_cluster = if let Some(grapheme_cluster) = input.get(range) {
                        grapheme_cluster
                    } else {
                        return self.dic.len() as u16;
                    };

                    self.dic
                        .get_copied(PotentialUtf8::from_str(grapheme_cluster))
                        .unwrap_or_else(|| self.dic.len() as u16)
                })
                .collect()
        } else {
            input
                .chars()
                .map(|c| {
                    self.dic
                        .get_copied(PotentialUtf8::from_str(c.encode_utf8(&mut [0; 4])))
                        .unwrap_or_else(|| self.dic.len() as u16)
                })
                .collect()
        };
        LstmSegmenterIterator {
            input,
            pos_utf8: 0,
            bies: BiesIterator::new(self, input_seq),
        }
    }

    /// Create an LSTM based break iterator for a UTF-16 string.
    pub(super) fn segment_utf16(&'l self, input: &[u16]) -> impl Iterator<Item = usize> + 'l {
        let input_seq = if let Some(grapheme) = self.grapheme {
            GraphemeClusterSegmenter::new_and_segment_utf16(input, grapheme)
                .collect::<Vec<usize>>()
                .windows(2)
                .map(|chunk| {
                    let range = if let [first, second, ..] = chunk {
                        *first..*second
                    } else {
                        unreachable!()
                    };
                    let grapheme_cluster = if let Some(grapheme_cluster) = input.get(range) {
                        grapheme_cluster
                    } else {
                        return self.dic.len() as u16;
                    };

                    self.dic
                        .get_copied_by(|key| {
                            key.as_bytes().iter().copied().cmp(
                                decode_utf16(grapheme_cluster.iter().copied()).flat_map(|c| {
                                    let mut buf = [0; 4];
                                    let len = c
                                        .unwrap_or(REPLACEMENT_CHARACTER)
                                        .encode_utf8(&mut buf)
                                        .len();
                                    buf.into_iter().take(len)
                                }),
                            )
                        })
                        .unwrap_or_else(|| self.dic.len() as u16)
                })
                .collect()
        } else {
            decode_utf16(input.iter().copied())
                .map(|c| c.unwrap_or(REPLACEMENT_CHARACTER))
                .map(|c| {
                    self.dic
                        .get_copied(PotentialUtf8::from_str(c.encode_utf8(&mut [0; 4])))
                        .unwrap_or_else(|| self.dic.len() as u16)
                })
                .collect()
        };
        LstmSegmenterIteratorUtf16 {
            bies: BiesIterator::new(self, input_seq),
            pos: 0,
        }
    }
}

struct BiesIterator<'l> {
    segmenter: &'l LstmSegmenter<'l>,
    input_seq: core::iter::Enumerate<alloc::vec::IntoIter<u16>>,
    h_bw: MatrixOwned<2>,
    curr_fw: MatrixOwned<1>,
    c_fw: MatrixOwned<1>,
}

impl<'l> BiesIterator<'l> {
    // input_seq is a sequence of id numbers that represents grapheme clusters or code points in the input line. These ids are used later
    // in the embedding layer of the model.
    fn new(segmenter: &'l LstmSegmenter<'l>, input_seq: Vec<u16>) -> Self {
        let hunits = segmenter.fw_u.dim().1;

        // Backward LSTM
        let mut c_bw = MatrixOwned::<1>::new_zero([hunits]);
        let mut h_bw = MatrixOwned::<2>::new_zero([input_seq.len(), hunits]);
        for (i, &g_id) in input_seq.iter().enumerate().rev() {
            if i + 1 < input_seq.len() {
                h_bw.as_mut().copy_submatrix::<1>(i + 1, i);
            }
            #[allow(clippy::unwrap_used)]
            compute_hc(
                segmenter.embedding.submatrix::<1>(g_id as usize).unwrap(), /* shape (dict.len() + 1, hunit), g_id is at most dict.len() */
                h_bw.submatrix_mut(i).unwrap(), // shape (input_seq.len(), hunits)
                c_bw.as_mut(),
                segmenter.bw_w,
                segmenter.bw_u,
                segmenter.bw_b,
            );
        }

        Self {
            input_seq: input_seq.into_iter().enumerate(),
            h_bw,
            c_fw: MatrixOwned::<1>::new_zero([hunits]),
            curr_fw: MatrixOwned::<1>::new_zero([hunits]),
            segmenter,
        }
    }
}

impl ExactSizeIterator for BiesIterator<'_> {
    fn len(&self) -> usize {
        self.input_seq.len()
    }
}

impl Iterator for BiesIterator<'_> {
    type Item = bool;

    fn next(&mut self) -> Option<Self::Item> {
        let (i, g_id) = self.input_seq.next()?;

        #[allow(clippy::unwrap_used)]
        compute_hc(
            self.segmenter
                .embedding
                .submatrix::<1>(g_id as usize)
                .unwrap(), // shape (dict.len() + 1, hunit), g_id is at most dict.len()
            self.curr_fw.as_mut(),
            self.c_fw.as_mut(),
            self.segmenter.fw_w,
            self.segmenter.fw_u,
            self.segmenter.fw_b,
        );

        #[allow(clippy::unwrap_used)] // shape (input_seq.len(), hunits)
        let curr_bw = self.h_bw.submatrix::<1>(i).unwrap();
        let mut weights = [0.0; 4];
        let mut curr_est = MatrixBorrowedMut {
            data: &mut weights,
            dims: [4],
        };
        curr_est.add_dot_2d(self.curr_fw.as_borrowed(), self.segmenter.timew_fw);
        curr_est.add_dot_2d(curr_bw, self.segmenter.timew_bw);
        #[allow(clippy::unwrap_used)] // both shape (4)
        curr_est.add(self.segmenter.time_b).unwrap();
        // For correct BIES weight calculation we'd now have to apply softmax, however
        // we're only doing a naive argmax, so a monotonic function doesn't make a difference.

        Some(weights[2] > weights[0] && weights[2] > weights[1] && weights[2] > weights[3])
    }
}

/// `compute_hc1` implemens the evaluation of one LSTM layer.
fn compute_hc<'a>(
    x_t: MatrixZero<'a, 1>,
    mut h_tm1: MatrixBorrowedMut<'a, 1>,
    mut c_tm1: MatrixBorrowedMut<'a, 1>,
    w: MatrixZero<'a, 3>,
    u: MatrixZero<'a, 3>,
    b: MatrixZero<'a, 2>,
) {
    #[cfg(debug_assertions)]
    {
        let hunits = h_tm1.dim();
        let embedd_dim = x_t.dim();
        c_tm1.as_borrowed().debug_assert_dims([hunits]);
        w.debug_assert_dims([4, hunits, embedd_dim]);
        u.debug_assert_dims([4, hunits, hunits]);
        b.debug_assert_dims([4, hunits]);
    }

    let mut s_t = b.to_owned();

    s_t.as_mut().add_dot_3d_2(x_t, w);
    s_t.as_mut().add_dot_3d_1(h_tm1.as_borrowed(), u);

    #[allow(clippy::unwrap_used)] // first dimension is 4
    s_t.submatrix_mut::<1>(0).unwrap().sigmoid_transform();
    #[allow(clippy::unwrap_used)] // first dimension is 4
    s_t.submatrix_mut::<1>(1).unwrap().sigmoid_transform();
    #[allow(clippy::unwrap_used)] // first dimension is 4
    s_t.submatrix_mut::<1>(2).unwrap().tanh_transform();
    #[allow(clippy::unwrap_used)] // first dimension is 4
    s_t.submatrix_mut::<1>(3).unwrap().sigmoid_transform();

    #[allow(clippy::unwrap_used)] // first dimension is 4
    c_tm1.convolve(
        s_t.as_borrowed().submatrix(0).unwrap(),
        s_t.as_borrowed().submatrix(2).unwrap(),
        s_t.as_borrowed().submatrix(1).unwrap(),
    );

    #[allow(clippy::unwrap_used)] // first dimension is 4
    h_tm1.mul_tanh(s_t.as_borrowed().submatrix(3).unwrap(), c_tm1.as_borrowed());
}

#[cfg(test)]
mod tests {
    use super::*;
    use icu_provider::prelude::*;
    use serde::Deserialize;

    /// `TestCase` is a struct used to store a single test case.
    /// Each test case has two attributes: `unseg` which denotes the unsegmented line, and `true_bies` which indicates the Bies
    /// sequence representing the true segmentation.
    #[derive(PartialEq, Debug, Deserialize)]
    struct TestCase {
        unseg: String,
        expected_bies: String,
        true_bies: String,
    }

    /// `TestTextData` is a struct to store a vector of `TestCase` that represents a test text.
    #[derive(PartialEq, Debug, Deserialize)]
    struct TestTextData {
        testcases: Vec<TestCase>,
    }

    #[derive(Debug)]
    struct TestText {
        data: TestTextData,
    }

    #[test]
    fn segment_file_by_lstm() {
        let lstm: DataResponse<LstmForWordLineAutoV1Marker> = crate::provider::Baked
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_marker_attributes(
                    DataMarkerAttributes::from_str_or_panic(
                        "Thai_codepoints_exclusive_model4_heavy",
                    ),
                ),
                ..Default::default()
            })
            .unwrap();
        let lstm = LstmSegmenter::new(
            lstm.payload.get(),
            crate::provider::Baked::SINGLETON_GRAPHEME_CLUSTER_BREAK_DATA_V2_MARKER,
        );

        // Importing the test data
        let test_text_data = serde_json::from_str(if lstm.grapheme.is_some() {
            include_str!("../../../tests/testdata/test_text_graphclust.json")
        } else {
            include_str!("../../../tests/testdata/test_text_codepoints.json")
        })
        .expect("JSON syntax error");
        let test_text = TestText {
            data: test_text_data,
        };

        // Testing
        for test_case in &test_text.data.testcases {
            let lstm_output = lstm
                .segment_str_p(&test_case.unseg)
                .bies
                .map(|is_e| if is_e { 'e' } else { '?' })
                .collect::<String>();
            println!("Test case      : {}", test_case.unseg);
            println!("Expected bies  : {}", test_case.expected_bies);
            println!("Estimated bies : {lstm_output}");
            println!("True bies      : {}", test_case.true_bies);
            println!("****************************************************");
            assert_eq!(
                test_case.expected_bies.replace(['b', 'i', 's'], "?"),
                lstm_output
            );
        }
    }
}
