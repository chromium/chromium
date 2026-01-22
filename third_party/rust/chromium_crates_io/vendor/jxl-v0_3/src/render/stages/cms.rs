// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use std::any::Any;

use crate::api::JxlCmsTransformer;
use crate::error::Result;
use crate::render::RenderPipelineInPlaceStage;

use crate::render::simd_utils::{
    deinterleave_2_dispatch, deinterleave_3_dispatch, deinterleave_4_dispatch,
    interleave_2_dispatch, interleave_3_dispatch, interleave_4_dispatch,
};
use crate::util::AtomicRefCell;

/// Thread-local state for CMS transform.
struct CmsLocalState {
    transformer_idx: usize,
    /// Buffer for interleaved input pixels (always used).
    input_buffer: Vec<f32>,
    /// Buffer for interleaved output pixels (only used when in_channels != out_channels).
    output_buffer: Vec<f32>,
}

/// Applies CMS color transform between color profiles.
///
/// The stage receives channels re-indexed from 0 in `process_row_chunk`:
/// - row[0], row[1], row[2] are color channels (always present)
/// - row[3] is the black (K) channel if `black_channel` is `Some`
///
/// Output is written to row[0..out_channels].
pub struct CmsStage {
    transformers: Vec<AtomicRefCell<Box<dyn JxlCmsTransformer + Send>>>,
    /// Number of input channels (3 for RGB, 4 for CMYK).
    in_channels: usize,
    /// Number of output channels (typically 3 for RGB output).
    out_channels: usize,
    /// Pipeline index of the black (K) channel, if present.
    /// Used by `uses_channel` to request the K channel from the pipeline.
    black_channel: Option<usize>,
    input_buffer_size: usize,
    output_buffer_size: usize,
}

impl CmsStage {
    /// Creates a new CMS stage.
    ///
    /// # Arguments
    /// * `transformers` - CMS transformer instances (one per thread recommended)
    /// * `in_channels` - Number of input channels (3 for RGB, 4 for CMYK)
    /// * `out_channels` - Number of output channels (must be <= in_channels)
    /// * `black_channel` - Pipeline index of K channel if present (for CMYK)
    /// * `max_pixels` - Maximum pixels per row chunk
    ///
    /// When input and output channel counts match, uses in-place transform.
    /// When they differ, uses separate input/output buffers.
    ///
    /// # Example
    /// ```ignore
    /// // RGB -> RGB (in-place)
    /// CmsStage::new(transformers, 3, 3, None, max_pixels);
    ///
    /// // CMYK -> RGB where K is at pipeline channel 5
    /// CmsStage::new(transformers, 4, 3, Some(5), max_pixels);
    /// ```
    pub fn new(
        transformers: Vec<Box<dyn JxlCmsTransformer + Send>>,
        in_channels: usize,
        out_channels: usize,
        black_channel: Option<usize>,
        max_pixels: usize,
    ) -> Self {
        // Validate ranges first for clearer error messages
        assert!(
            (1..=4).contains(&in_channels),
            "CMS stage only supports 1-4 input channels, got {in_channels}"
        );
        assert!(
            (1..=4).contains(&out_channels),
            "CMS stage only supports 1-4 output channels, got {out_channels}"
        );
        assert!(
            out_channels <= in_channels,
            "out_channels ({out_channels}) must be <= in_channels ({in_channels})"
        );
        assert!(
            black_channel.is_some() == (in_channels == 4),
            "black_channel must be Some iff in_channels == 4"
        );
        // Pad buffer to SIMD alignment (max vector length is 16)
        let padded_pixels = max_pixels.next_multiple_of(16);
        Self {
            transformers: transformers.into_iter().map(AtomicRefCell::new).collect(),
            in_channels,
            out_channels,
            black_channel,
            input_buffer_size: padded_pixels
                .checked_mul(in_channels)
                .expect("CMS input buffer size overflow"),
            output_buffer_size: padded_pixels
                .checked_mul(out_channels)
                .expect("CMS output buffer size overflow"),
        }
    }
}

impl std::fmt::Display for CmsStage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if let Some(k) = self.black_channel {
            write!(
                f,
                "CMS transform: {} channels (K at {}) -> {} channels",
                self.in_channels, k, self.out_channels
            )
        } else {
            write!(
                f,
                "CMS transform: {} channels -> {} channels",
                self.in_channels, self.out_channels
            )
        }
    }
}

impl RenderPipelineInPlaceStage for CmsStage {
    type Type = f32;

    fn uses_channel(&self, c: usize) -> bool {
        // Color channels (0..min(in_channels, 3)) plus black channel if present
        c < self.in_channels.min(3) || self.black_channel == Some(c)
    }

    fn init_local_state(&self, thread_index: usize) -> Result<Option<Box<dyn Any>>> {
        if self.transformers.is_empty() {
            return Ok(None);
        }
        // Use thread index modulo transformer count to assign transformer to this thread
        let idx = thread_index % self.transformers.len();

        // When channel counts differ, we need separate input and output buffers.
        // When they're the same, we use in-place transform (output_buffer unused).
        let output_buffer = if self.in_channels != self.out_channels {
            vec![0.0f32; self.output_buffer_size]
        } else {
            Vec::new()
        };

        Ok(Some(Box::new(CmsLocalState {
            transformer_idx: idx,
            input_buffer: vec![0.0f32; self.input_buffer_size],
            output_buffer,
        })))
    }

    fn process_row_chunk(
        &self,
        _position: (usize, usize),
        xsize: usize,
        row: &mut [&mut [f32]],
        state: Option<&mut dyn Any>,
    ) {
        let Some(state) = state else {
            return;
        };
        let state: &mut CmsLocalState = state.downcast_mut().unwrap();
        let same_channels = self.in_channels == self.out_channels;

        debug_assert!(
            xsize * self.in_channels <= state.input_buffer.len(),
            "xsize {} exceeds buffer capacity",
            xsize
        );

        // Single channel: transform directly in place without interleaving
        if self.in_channels == 1 && self.out_channels == 1 {
            let mut transformer = self.transformers[state.transformer_idx].borrow_mut();
            transformer
                .do_transform_inplace(&mut row[0][..xsize])
                .expect("CMS transform failed");
            return;
        }

        // Pad to SIMD alignment (pipeline rows are already padded)
        let xsize_padded = xsize.next_multiple_of(16);

        // Interleave planar -> packed using SIMD
        // row[0..in_channels] are our input channels (re-indexed from 0 by pipeline)
        match self.in_channels {
            2 => {
                interleave_2_dispatch(
                    &row[0][..xsize_padded],
                    &row[1][..xsize_padded],
                    &mut state.input_buffer[..xsize_padded * 2],
                );
            }
            3 => {
                interleave_3_dispatch(
                    &row[0][..xsize_padded],
                    &row[1][..xsize_padded],
                    &row[2][..xsize_padded],
                    &mut state.input_buffer[..xsize_padded * 3],
                );
            }
            4 => {
                interleave_4_dispatch(
                    &row[0][..xsize_padded],
                    &row[1][..xsize_padded],
                    &row[2][..xsize_padded],
                    &row[3][..xsize_padded],
                    &mut state.input_buffer[..xsize_padded * 4],
                );
            }
            _ => unreachable!("CMS stage only supports 2-4 input channels here"),
        }

        // Apply transform (only on actual pixels, not padding)
        let mut transformer = self.transformers[state.transformer_idx].borrow_mut();
        if same_channels {
            // In-place transform when channel counts match
            transformer
                .do_transform_inplace(&mut state.input_buffer[..xsize * self.in_channels])
                .expect("CMS transform failed");
        } else {
            // Separate buffer transform when channel counts differ
            transformer
                .do_transform(
                    &state.input_buffer[..xsize * self.in_channels],
                    &mut state.output_buffer[..xsize * self.out_channels],
                )
                .expect("CMS transform failed");
        }

        // Select source buffer for deinterleaving
        let output_buf = if same_channels {
            &state.input_buffer
        } else {
            &state.output_buffer
        };

        // De-interleave packed -> planar
        // Output goes to row[0..out_channels]
        match self.out_channels {
            1 => {
                // Single output channel: copy directly from output buffer (no deinterleaving)
                row[0][..xsize].copy_from_slice(&output_buf[..xsize]);
            }
            2 => {
                let (r0, r1) = row.split_at_mut(1);
                deinterleave_2_dispatch(
                    &output_buf[..xsize_padded * 2],
                    &mut r0[0][..xsize_padded],
                    &mut r1[0][..xsize_padded],
                );
            }
            3 => {
                let (r0, rest) = row.split_at_mut(1);
                let (r1, r2) = rest.split_at_mut(1);
                deinterleave_3_dispatch(
                    &output_buf[..xsize_padded * 3],
                    &mut r0[0][..xsize_padded],
                    &mut r1[0][..xsize_padded],
                    &mut r2[0][..xsize_padded],
                );
            }
            4 => {
                let (r0, rest) = row.split_at_mut(1);
                let (r1, rest) = rest.split_at_mut(1);
                let (r2, r3) = rest.split_at_mut(1);
                deinterleave_4_dispatch(
                    &output_buf[..xsize_padded * 4],
                    &mut r0[0][..xsize_padded],
                    &mut r1[0][..xsize_padded],
                    &mut r2[0][..xsize_padded],
                    &mut r3[0][..xsize_padded],
                );
            }
            _ => unreachable!("CMS stage only supports 1-4 output channels"),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Mock transformer that copies input to output (for same channel count).
    struct IdentityTransformer;

    impl JxlCmsTransformer for IdentityTransformer {
        fn do_transform(&mut self, input: &[f32], output: &mut [f32]) -> Result<()> {
            output.copy_from_slice(input);
            Ok(())
        }

        fn do_transform_inplace(&mut self, _inout: &mut [f32]) -> Result<()> {
            // Identity - no change needed
            Ok(())
        }
    }

    /// Mock transformer that scales values by 2 (for testing in-place).
    struct ScaleTransformer;

    impl JxlCmsTransformer for ScaleTransformer {
        fn do_transform(&mut self, input: &[f32], output: &mut [f32]) -> Result<()> {
            for (o, i) in output.iter_mut().zip(input.iter()) {
                *o = *i * 2.0;
            }
            Ok(())
        }

        fn do_transform_inplace(&mut self, inout: &mut [f32]) -> Result<()> {
            for v in inout.iter_mut() {
                *v *= 2.0;
            }
            Ok(())
        }
    }

    /// Mock transformer that converts 4 channels to 3 (CMYK -> RGB style).
    /// Simply drops the 4th channel and passes through the first 3.
    struct FourToThreeTransformer;

    impl JxlCmsTransformer for FourToThreeTransformer {
        fn do_transform(&mut self, input: &[f32], output: &mut [f32]) -> Result<()> {
            // Input: CMYK interleaved (4 values per pixel)
            // Output: RGB interleaved (3 values per pixel)
            let num_pixels = input.len() / 4;
            for i in 0..num_pixels {
                // Simple conversion: R = 1-C, G = 1-M, B = 1-Y (ignoring K for simplicity)
                output[i * 3] = 1.0 - input[i * 4]; // C -> R
                output[i * 3 + 1] = 1.0 - input[i * 4 + 1]; // M -> G
                output[i * 3 + 2] = 1.0 - input[i * 4 + 2]; // Y -> B
            }
            Ok(())
        }

        fn do_transform_inplace(&mut self, _inout: &mut [f32]) -> Result<()> {
            // Cannot do 4->3 in place
            panic!("FourToThreeTransformer does not support in-place transform");
        }
    }

    #[test]
    fn test_cms_stage_rgb_inplace() {
        // Test 3->3 channel transform (uses in-place)
        let transformers: Vec<Box<dyn JxlCmsTransformer + Send>> = vec![Box::new(ScaleTransformer)];
        let stage = CmsStage::new(transformers, 3, 3, None, 16);

        // Initialize state for thread 0
        let state = stage.init_local_state(0).unwrap().unwrap();
        let mut state_ref: Box<dyn Any> = state;

        // Create test data: 3 channels, 4 pixels
        let mut ch0 = vec![1.0, 2.0, 3.0, 4.0];
        let mut ch1 = vec![0.5, 0.5, 0.5, 0.5];
        let mut ch2 = vec![0.1, 0.2, 0.3, 0.4];

        // Pad to 16 for SIMD alignment
        ch0.resize(16, 0.0);
        ch1.resize(16, 0.0);
        ch2.resize(16, 0.0);

        let mut rows: Vec<&mut [f32]> = vec![&mut ch0, &mut ch1, &mut ch2];
        stage.process_row_chunk((0, 0), 4, &mut rows, Some(state_ref.as_mut()));

        // Values should be scaled by 2
        assert_eq!(ch0[0], 2.0);
        assert_eq!(ch0[1], 4.0);
        assert_eq!(ch1[0], 1.0);
        assert_eq!(ch2[0], 0.2);
    }

    #[test]
    fn test_cms_stage_cmyk_to_rgb() {
        // Test 4->3 channel transform (CMYK to RGB)
        // The pipeline passes row[0..4] as CMY + K (re-indexed from 0)
        let transformers: Vec<Box<dyn JxlCmsTransformer + Send>> =
            vec![Box::new(FourToThreeTransformer)];
        // K is at pipeline index 5, but we get it as row[3]
        let stage = CmsStage::new(transformers, 4, 3, Some(5), 16);

        let state = stage.init_local_state(0).unwrap().unwrap();
        let mut state_ref: Box<dyn Any> = state;

        // Create test data: 4 channels as the pipeline would pass them (re-indexed)
        // row[0]=C, row[1]=M, row[2]=Y, row[3]=K
        let mut ch0 = vec![0.2, 0.5]; // C
        let mut ch1 = vec![0.3, 0.5]; // M
        let mut ch2 = vec![0.4, 0.5]; // Y
        let mut ch3 = vec![0.1, 0.5]; // K

        // Pad to 16 for SIMD alignment
        ch0.resize(16, 0.0);
        ch1.resize(16, 0.0);
        ch2.resize(16, 0.0);
        ch3.resize(16, 0.0);

        let mut rows: Vec<&mut [f32]> = vec![&mut ch0, &mut ch1, &mut ch2, &mut ch3];
        stage.process_row_chunk((0, 0), 2, &mut rows, Some(state_ref.as_mut()));

        // Output should be RGB: R = 1-C, G = 1-M, B = 1-Y
        assert!((ch0[0] - 0.8).abs() < 0.001);
        assert!((ch1[0] - 0.7).abs() < 0.001);
        assert!((ch2[0] - 0.6).abs() < 0.001);
    }

    #[test]
    fn test_cms_stage_single_channel() {
        // Test 1->1 channel transform (grayscale)
        let transformers: Vec<Box<dyn JxlCmsTransformer + Send>> = vec![Box::new(ScaleTransformer)];
        let stage = CmsStage::new(transformers, 1, 1, None, 16);

        let state = stage.init_local_state(0).unwrap().unwrap();
        let mut state_ref: Box<dyn Any> = state;

        let mut ch0 = vec![1.0, 2.0, 3.0, 4.0];
        ch0.resize(16, 0.0);

        let mut rows: Vec<&mut [f32]> = vec![&mut ch0];
        stage.process_row_chunk((0, 0), 4, &mut rows, Some(state_ref.as_mut()));

        // Values should be scaled by 2
        assert_eq!(ch0[0], 2.0);
        assert_eq!(ch0[1], 4.0);
        assert_eq!(ch0[2], 6.0);
        assert_eq!(ch0[3], 8.0);
    }

    #[test]
    fn test_cms_stage_no_transformers() {
        // Test with empty transformers - should do nothing
        let transformers: Vec<Box<dyn JxlCmsTransformer + Send>> = vec![];
        let stage = CmsStage::new(transformers, 3, 3, None, 16);

        // init_local_state should return None when no transformers
        let state = stage.init_local_state(0).unwrap();
        assert!(state.is_none());

        // process_row_chunk should be a no-op with None state
        let mut ch0 = vec![1.0, 2.0, 3.0, 4.0];
        ch0.resize(16, 0.0);
        let original = ch0.clone();

        let mut rows: Vec<&mut [f32]> = vec![&mut ch0];
        stage.process_row_chunk((0, 0), 4, &mut rows, None);

        // Values should be unchanged
        assert_eq!(ch0, original);
    }

    #[test]
    fn test_cms_stage_display() {
        let transformers: Vec<Box<dyn JxlCmsTransformer + Send>> =
            vec![Box::new(IdentityTransformer)];
        let stage_rgb = CmsStage::new(transformers, 3, 3, None, 16);
        let display = format!("{}", stage_rgb);
        assert!(display.contains("3 channels -> 3 channels"));

        let transformers: Vec<Box<dyn JxlCmsTransformer + Send>> =
            vec![Box::new(IdentityTransformer)];
        let stage_cmyk = CmsStage::new(transformers, 4, 3, Some(5), 16);
        let display = format!("{}", stage_cmyk);
        assert!(display.contains("4 channels"));
        assert!(display.contains("K at 5"));
        assert!(display.contains("-> 3 channels"));
    }

    #[test]
    fn test_cms_stage_uses_channel() {
        // RGB only (no black channel)
        let transformers: Vec<Box<dyn JxlCmsTransformer + Send>> =
            vec![Box::new(IdentityTransformer)];
        let stage_rgb = CmsStage::new(transformers, 3, 3, None, 16);
        assert!(stage_rgb.uses_channel(0));
        assert!(stage_rgb.uses_channel(1));
        assert!(stage_rgb.uses_channel(2));
        assert!(!stage_rgb.uses_channel(3));
        assert!(!stage_rgb.uses_channel(5));

        // CMYK with K at pipeline index 5
        let transformers: Vec<Box<dyn JxlCmsTransformer + Send>> =
            vec![Box::new(IdentityTransformer)];
        let stage_cmyk = CmsStage::new(transformers, 4, 3, Some(5), 16);
        assert!(stage_cmyk.uses_channel(0));
        assert!(stage_cmyk.uses_channel(1));
        assert!(stage_cmyk.uses_channel(2));
        assert!(!stage_cmyk.uses_channel(3)); // Not K
        assert!(!stage_cmyk.uses_channel(4)); // Not K
        assert!(stage_cmyk.uses_channel(5)); // K channel
        assert!(!stage_cmyk.uses_channel(6));
    }

    #[test]
    fn test_stage_consistency_cms() -> crate::error::Result<()> {
        // Test consistency for RGB -> RGB (3-channel identity transform)
        // max_pixels must be >= test image width (500)
        crate::render::test::test_stage_consistency(
            || CmsStage::new(vec![Box::new(IdentityTransformer)], 3, 3, None, 512),
            (500, 500),
            3,
        )
    }
}
