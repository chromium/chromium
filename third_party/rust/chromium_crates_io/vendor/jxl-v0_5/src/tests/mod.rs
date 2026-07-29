// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#[macro_use]
mod macros;

mod api;
mod compare_incremental;
pub(crate) mod decode;

#[allow(clippy::single_component_path_imports)]
pub(crate) use assert_close;
#[allow(clippy::single_component_path_imports)]
pub(crate) use assert_image_eq;

declare_test_file!(jpeg_recompression_3x3, "3x3_jpeg_recompression.jxl");
declare_test_file!(srgb_lossless_3x3, "3x3_srgb_lossless.jxl");
declare_test_file!(srgb_lossy_3x3, "3x3_srgb_lossy.jxl");
declare_test_file!(srgb_lossless_3x3a, "3x3a_srgb_lossless.jxl");
declare_test_file!(srgb_lossy_3x3a, "3x3a_srgb_lossy.jxl");
declare_test_file!(noise_8x8, "8x8_noise.jxl");
declare_test_file!(basic, "basic.jxl");
declare_test_file!(
    candle,
    "candle.jxl",
    checkpoints: &[
        (20172, 1.5539095),
        (39237, 1.5526553),
        (111315, 0.0188495),
        (146247, 0.0055011)
    ]
);
declare_test_file!(
    conformance_test_images_alpha_nonpremultiplied,
    "conformance_test_images/alpha_nonpremultiplied.jxl"
);
declare_test_file!(
    conformance_test_images_alpha_premultiplied,
    "conformance_test_images/alpha_premultiplied.jxl",
    checkpoints: &[
        (7872, 0.0000015),
        (8118, 0.0000012),
        (8364, 0.0000011),
        (8610, 0.0000010)
    ]
);
declare_test_file!(
    conformance_test_images_alpha_triangles,
    "conformance_test_images/alpha_triangles.jxl"
);
declare_test_file!(
    conformance_test_images_animation_icos4d,
    "conformance_test_images/animation_icos4d.jxl"
);
declare_test_file!(
    conformance_test_images_animation_icos4d_5,
    "conformance_test_images/animation_icos4d_5.jxl"
);
declare_test_file!(
    conformance_test_images_animation_newtons_cradle,
    "conformance_test_images/animation_newtons_cradle.jxl"
);
declare_test_file!(
    conformance_test_images_animation_spline,
    "conformance_test_images/animation_spline.jxl"
);
declare_test_file!(
    conformance_test_images_animation_spline_5,
    "conformance_test_images/animation_spline_5.jxl"
);
declare_test_file!(
    conformance_test_images_bench_oriented_brg,
    "conformance_test_images/bench_oriented_brg.jxl",
    checkpoints: &[
        (12915, 0.0116237),
        (45387, 0.0097723),
        (112176, 0.0058762),
        (168141, 0.0013063)
    ]
);
declare_test_file!(
    conformance_test_images_bench_oriented_brg_5,
    "conformance_test_images/bench_oriented_brg_5.jxl",
    checkpoints: &[
        (12915, 0.0116237),
        (45387, 0.0097723),
        (112176, 0.0058762),
        (168141, 0.0013063)
    ]
);
declare_test_file!(
    conformance_test_images_bicycles,
    "conformance_test_images/bicycles.jxl",
    checkpoints: &[
        (1476, 0.0402024),
        (11439, 0.0192258),
        (47109, 0.0044628),
        (62730, 0.0012438)
    ]
);
declare_test_file!(
    conformance_test_images_bike,
    "conformance_test_images/bike.jxl",
    checkpoints: &[
        (107256, 0.0134448),
        (140589, 0.0116619),
        (218940, 0.0043241),
        (306762, 0.0014917)
    ]
);
declare_test_file!(
    conformance_test_images_bike_5,
    "conformance_test_images/bike_5.jxl",
    checkpoints: &[
        (107256, 0.0134448),
        (140589, 0.0116619),
        (218940, 0.0043241),
        (306762, 0.0014917)
    ]
);
declare_test_file!(
    conformance_test_images_blendmodes,
    "conformance_test_images/blendmodes.jxl"
);
declare_test_file!(
    conformance_test_images_blendmodes_5,
    "conformance_test_images/blendmodes_5.jxl"
);
declare_test_file!(
    conformance_test_images_cafe,
    "conformance_test_images/cafe.jxl",
    checkpoints: &[
        (32103, 0.0321261),
        (108363, 0.0263731),
        (212421, 0.0166692),
        (334683, 0.0039590)
    ]
);
declare_test_file!(
    conformance_test_images_cafe_5,
    "conformance_test_images/cafe_5.jxl",
    checkpoints: &[
        (32103, 0.0321261),
        (108363, 0.0263731),
        (212421, 0.0166692),
        (334683, 0.0039590)
    ]
);
declare_test_file!(
    conformance_test_images_cmyk_layers,
    "conformance_test_images/cmyk_layers.jxl",
    checkpoints: &[
        (379578, 0.9753281),
        (382407, 0.9753281),
        (384621, 0.9753281),
        (386097, 0.9753281)
    ]
);
declare_test_file!(
    conformance_test_images_delta_palette,
    "conformance_test_images/delta_palette.jxl",
    checkpoints: &[
        (36654, 0.1900889),
        (79212, 0.0846460)
    ]
);
declare_test_file!(
    conformance_test_images_grayscale,
    "conformance_test_images/grayscale.jxl"
);
declare_test_file!(
    conformance_test_images_grayscale_5,
    "conformance_test_images/grayscale_5.jxl"
);
declare_test_file!(
    conformance_test_images_grayscale_jpeg,
    "conformance_test_images/grayscale_jpeg.jxl"
);
declare_test_file!(
    conformance_test_images_grayscale_jpeg_5,
    "conformance_test_images/grayscale_jpeg_5.jxl"
);
declare_test_file!(
    conformance_test_images_grayscale_public_university,
    "conformance_test_images/grayscale_public_university.jxl",
    checkpoints: &[
        (1107, 0.0266236),
        (49692, 0.0024040),
        (56334, 0.0013935),
        (60516, 0.0007346)
    ]
);
declare_test_file!(
    conformance_test_images_lossless_pfm,
    "conformance_test_images/lossless_pfm.jxl",
    checkpoints: &[
        (215004, 0.632265),
        (400734, 0.4235403),
        (586341, 0.2072522)
    ]
);
declare_test_file!(
    conformance_test_images_lz77_flower,
    "conformance_test_images/lz77_flower.jxl"
);
declare_test_file!(
    conformance_test_images_noise,
    "conformance_test_images/noise.jxl",
    checkpoints: &[
        (11685, 0.0121489),
        (32103, 0.0101523),
        (73431, 0.0060286),
        (107994, 0.0013624)
    ]
);
declare_test_file!(
    conformance_test_images_noise_5,
    "conformance_test_images/noise_5.jxl",
    checkpoints: &[
        (11685, 0.0121489),
        (32103, 0.0101523),
        (73431, 0.0060286),
        (107994, 0.0013624)
    ]
);
declare_test_file!(
    conformance_test_images_opsin_inverse,
    "conformance_test_images/opsin_inverse.jxl",
    checkpoints: &[
        (11685, 0.0237428),
        (32103, 0.0206120),
        (73431, 0.0133140),
        (107994, 0.0030593)
    ]
);
declare_test_file!(
    conformance_test_images_opsin_inverse_5,
    "conformance_test_images/opsin_inverse_5.jxl",
    checkpoints: &[
        (11685, 0.0237428),
        (32103, 0.0206120),
        (73431, 0.0133140),
        (107994, 0.0030593)
    ]
);
declare_test_file!(
    conformance_test_images_patches,
    "conformance_test_images/patches.jxl",
    checkpoints: &[
        (43911, 0.0016077),
        (50676, 0.0011612),
        (57072, 0.0006984),
        (64452, 0.0001858)
    ]
);
declare_test_file!(
    conformance_test_images_patches_5,
    "conformance_test_images/patches_5.jxl",
    checkpoints: &[
        (43911, 0.0016077),
        (50676, 0.0011612),
        (57072, 0.0006984),
        (64452, 0.0001858)
    ]
);
declare_test_file!(
    conformance_test_images_progressive,
    "conformance_test_images/progressive.jxl",
    checkpoints: &[
        (127674, 0.0045291),
        (210945, 0.0026292),
        (264450, 0.0010978),
        (367893, 0.0005386)
    ]
);
declare_test_file!(
    conformance_test_images_progressive_5,
    "conformance_test_images/progressive_5.jxl",
    checkpoints: &[
        (127674, 0.0045291),
        (210945, 0.0026292),
        (264450, 0.0010978),
        (367893, 0.0005386)
    ]
);
declare_test_file!(
    conformance_test_images_spot,
    "conformance_test_images/spot.jxl",
    checkpoints: &[
        (141573, 0.5850735),
        (278349, 0.5850735),
        (376503, 0.5850735),
        (391017, 0.1207217)
    ]
);
declare_test_file!(
    conformance_test_images_sunset_logo,
    "conformance_test_images/sunset_logo.jxl"
);
declare_test_file!(
    conformance_test_images_upsampling,
    "conformance_test_images/upsampling.jxl"
);
declare_test_file!(
    conformance_test_images_upsampling_5,
    "conformance_test_images/upsampling_5.jxl"
);
declare_test_file!(cropped_traffic_light, "cropped_traffic_light.jxl");
declare_test_file!(
    dice,
    "dice.jxl",
    checkpoints: &[
        (20664, 0.0014690),
        (32595, 0.0006832),
        (44895, 0.0000859),
        (46371, 0.0000138)
    ]
);
declare_test_file!(
    efb,
    "efb.jxl",
    checkpoints: &[
        (3198, 0.0054359),
        (12423, 0.0016210),
        (18573, 0.0003477),
        (19311, 0.0001782)
    ]
);
declare_test_file!(extra_channels, "extra_channels.jxl");
declare_test_file!(gray_alpha_lossless, "gray_alpha_lossless.jxl");
declare_test_file!(
    grayscale_patches_modular,
    "grayscale_patches_modular.jxl",
    checkpoints: &[
        (4182, 0.0001557),
        (4674, 0.0000921),
        (5043, 0.0000753),
        (5412, 0.0000010)
    ]
);
declare_test_file!(
    grayscale_patches_var_dct,
    "grayscale_patches_var_dct.jxl",
    checkpoints: &[
        (6396, 0.0001695),
        (8856, 0.0001409),
        (11931, 0.0000513),
        (12546, 0.0000010)
    ]
);
declare_test_file!(
    green_queen_modular_e3,
    "green_queen_modular_e3.jxl",
    checkpoints: &[
        (80442, 0.1442797),
        (141450, 0.106242),
        (279702, 0.0218487),
        (300981, 0.0076177)
    ]
);
declare_test_file!(
    green_queen_vardct_e3,
    "green_queen_vardct_e3.jxl",
    checkpoints: &[
        (9348, 0.0121551),
        (27675, 0.0094501),
        (61008, 0.0046881),
        (84255, 0.0008925)
    ]
);
declare_test_file!(has_permutation, "has_permutation.jxl");
declare_test_file!(
    has_permutation_with_container,
    "has_permutation_with_container.jxl"
);
declare_test_file!(hdr_hlg_test, "hdr_hlg_test.jxl");
declare_test_file!(hdr_pq_test, "hdr_pq_test.jxl");
declare_test_file!(
    issue648_palette0,
    "issue648_palette0.jxl",
    checkpoints: &[
        (673302, 0.0004207)
    ]
);
declare_test_file!(
    issue728_minimal,
    "issue728_minimal.jxl",
    checkpoints: &[
        (1353, 0.0000338),
        (2091, 0.0000338),
        (2952, 0.0000079),
        (3690, 0.0000040)
    ]
);
declare_test_file!(
    issue772_blendbug,
    "issue772_blendbug.jxl",
    checkpoints: &[
        (1107, 0.0539580),
        (2337, 0.0539580),
        (8241, 0.0539580),
        (10332, 0.0004338)
    ]
);
declare_test_file!(large_header, "large_header.jxl");
declare_test_file!(lossy_with_icc, "lossy_with_icc.jxl");
declare_test_file!(
    multiple_layers_noise_spline,
    "multiple_layers_noise_spline.jxl",
    checkpoints: &[
        (246, 0.5443857)
    ]
);
declare_test_file!(multiple_lf_420, "multiple_lf_420.jxl");
declare_test_file!(named_frame_test, "named_frame_test.jxl");
declare_test_file!(oddsize_ups, "oddsize_ups.jxl");
declare_test_file!(orientation1_identity, "orientation1_identity.jxl");
declare_test_file!(
    orientation2_flip_horizontal,
    "orientation2_flip_horizontal.jxl"
);
declare_test_file!(orientation3_rotate_180, "orientation3_rotate_180.jxl");
declare_test_file!(orientation4_flip_vertical, "orientation4_flip_vertical.jxl");
declare_test_file!(orientation5_transpose, "orientation5_transpose.jxl");
declare_test_file!(orientation6_rotate_90_cw, "orientation6_rotate_90_cw.jxl");
declare_test_file!(
    orientation7_anti_transpose,
    "orientation7_anti_transpose.jxl"
);
declare_test_file!(orientation8_rotate_90_ccw, "orientation8_rotate_90_ccw.jxl");
declare_test_file!(patch_y_out_of_bounds, "patch_y_out_of_bounds.jxl");
declare_test_file!(pq_gradient, "pq_gradient.jxl");
declare_test_file!(
    progressive_ac,
    "progressive_ac.jxl",
    checkpoints: &[
        (127797, 0.0017506),
        (217833, 0.0009243),
        (297045, 0.0003563),
        (379947, 0.0001129)
    ]
);
declare_test_file!(
    small_grayscale_patches_modular,
    "small_grayscale_patches_modular.jxl"
);
declare_test_file!(
    small_grayscale_patches_modular_with_icc,
    "small_grayscale_patches_modular_with_icc.jxl"
);
declare_test_file!(spline_on_first_frame, "spline_on_first_frame.jxl");
declare_test_file!(splines, "splines.jxl");
declare_test_file!(squeeze_alpha, "squeeze_alpha.jxl");
declare_test_file!(squeeze_edge, "squeeze_edge.jxl");
declare_test_file!(squeeze_empty_residual, "squeeze_empty_residual.jxl");
declare_test_file!(
    stp2_520x260_d25_e6,
    "stp2_520x260_d25_e6.jxl",
    checkpoints: &[
        (1599, 0.0139440)
    ]
);
declare_test_file!(strategic_solid_blue, "strategic_solid_blue.jxl");
declare_test_file!(
    tirr_photo,
    "tirr_photo.jxl",
    checkpoints: &[
        (427425, 0.0009541),
        (704790, 0.0004241),
        (827667, 0.0001988),
        (1203801, 0.0000516)
    ]
);
declare_test_file!(tree_max_property_20, "tree_max_property_20.jxl");
declare_test_file!(upsampled_alpha, "upsampled_alpha.jxl");
declare_test_file!(with_icc, "with_icc.jxl");
declare_test_file!(with_preview, "with_preview.jxl");
declare_test_file!(
    zoltan_tasi_unsplash,
    "zoltan_tasi_unsplash.jxl",
    checkpoints: &[
        (38253, 0.0164253),
        (159531, 0.0120982),
        (293847, 0.0053874),
        (396429, 0.0009008)
    ]
);
