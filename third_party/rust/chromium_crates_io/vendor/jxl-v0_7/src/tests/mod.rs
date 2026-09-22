// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#[macro_use]
mod macros;

mod api;
mod compare_incremental;
mod compare_modular;
#[cfg(not(any(target_family = "wasm", target_arch = "wasm32")))]
mod compare_parallel;
mod compare_prefix;
pub(crate) mod decode;
#[cfg(not(any(target_family = "wasm", target_arch = "wasm32")))]
pub(crate) mod parallel_runner;

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
        (20172, [0.0, 0.0, 0.000232147, 294.999]),
        (23001, [0.0, 0.0, 0.000206628, 294.999]),
        (39237, [0.0, 0.0, 0.00000142328, 294.999]),
        (64083, [0.0, 0.0, 0.0, 0.328222]),
        (111315, [0.0, 0.0, 0.0, 0.25]),
        (125706, [0.0, 0.0, 0.0, 0.118729]),
        (149814, [0.0, 0.0, 0.0, 0.0]),
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
        (7872, [0.0, 0.0, 0.0, 0.00000129912]),
        (8118, [0.0, 0.0, 0.0, 0.00000129912]),
        (8364, [0.0, 0.0, 0.0, 0.00000129912]),
        (8610, [0.0, 0.0, 0.0, 0.00000139172]),
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
        (12915, [0.00730742, 0.011339, 0.0142074, 0.025776]),
        (45387, [0.000781533, 0.0100636, 0.0136229, 0.025776]),
        (112176, [0.0, 0.0, 0.0102296, 0.025776]),
        (168141, [0.0, 0.0, 0.0, 0.0176678]),
    ]
);
declare_test_file!(
    conformance_test_images_bench_oriented_brg_5,
    "conformance_test_images/bench_oriented_brg_5.jxl",
    checkpoints: &[
        (12915, [0.00730742, 0.011339, 0.0142074, 0.025776]),
        (45387, [0.000781533, 0.0100636, 0.0136229, 0.025776]),
        (112176, [0.0, 0.0, 0.0102296, 0.025776]),
        (168141, [0.0, 0.0, 0.0, 0.0176678]),
    ]
);
declare_test_file!(
    conformance_test_images_bicycles,
    "conformance_test_images/bicycles.jxl",
    checkpoints: &[
        (1476, [0.0218885, 0.0375235, 0.0488227, 0.102123]),
        (5043, [0.0105726, 0.0186481, 0.0294646, 0.0617708]),
        (11439, [0.00782226, 0.0138347, 0.0224417, 0.0450609]),
        (25830, [0.00430748, 0.00737679, 0.0116605, 0.0293743]),
        (43419, [0.00166895, 0.0030007, 0.00458033, 0.0200634]),
        (51906, [0.0, 0.00119415, 0.00374154, 0.0200634]),
        (62730, [0.0, 0.0, 0.00000724414, 0.00630335]),
        (69741, [0.0, 0.0, 0.00000132704, 0.00369121]),
    ]
);
declare_test_file!(
    conformance_test_images_bike,
    "conformance_test_images/bike.jxl",
    checkpoints: &[
        (107256, [0.00125166, 0.00650685, 0.0172958, 0.174956]),
        (132840, [0.000202455, 0.00447611, 0.0157836, 0.174956]),
        (160023, [0.0, 0.00281062, 0.0121744, 0.174956]),
        (187821, [0.0, 0.000862913, 0.0087662, 0.12724]),
        (224106, [0.0, 0.0, 0.00509617, 0.0906076]),
        (270600, [0.0, 0.0, 0.00253048, 0.040986]),
        (302580, [0.0, 0.0, 0.00028238, 0.040986]),
        (381177, [0.0, 0.0, 0.0, 0.0140465]),
    ]
);
declare_test_file!(
    conformance_test_images_bike_5,
    "conformance_test_images/bike_5.jxl",
    checkpoints: &[
        (107256, [0.00125166, 0.00650685, 0.0172958, 0.174956]),
        (132840, [0.000202455, 0.00447611, 0.0157836, 0.174956]),
        (160023, [0.0, 0.00281062, 0.0121744, 0.174956]),
        (187821, [0.0, 0.000862913, 0.0087662, 0.12724]),
        (224106, [0.0, 0.0, 0.00509617, 0.0906076]),
        (270600, [0.0, 0.0, 0.00253048, 0.040986]),
        (302580, [0.0, 0.0, 0.00028238, 0.040986]),
        (381177, [0.0, 0.0, 0.0, 0.0140465]),
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
        (32103, [0.0166978, 0.0313812, 0.044541, 0.102195]),
        (69249, [0.00814571, 0.0300422, 0.0444275, 0.102195]),
        (108363, [0.0, 0.027175, 0.0437801, 0.102195]),
        (162852, [0.0, 0.012543, 0.0407327, 0.102195]),
        (212421, [0.0, 0.0, 0.034416, 0.0893503]),
        (278841, [0.0, 0.0, 0.00832674, 0.0730412]),
        (334683, [0.0, 0.0, 0.0, 0.065682]),
        (373674, [0.0, 0.0, 0.0, 0.0518601]),
    ]
);
declare_test_file!(
    conformance_test_images_cafe_5,
    "conformance_test_images/cafe_5.jxl",
    checkpoints: &[
        (32103, [0.0166978, 0.0313812, 0.044541, 0.102195]),
        (69249, [0.00814571, 0.0300422, 0.0444275, 0.102195]),
        (108363, [0.0, 0.027175, 0.0437801, 0.102195]),
        (162852, [0.0, 0.012543, 0.0407327, 0.102195]),
        (212421, [0.0, 0.0, 0.034416, 0.0893503]),
        (278841, [0.0, 0.0, 0.00832674, 0.0730412]),
        (334683, [0.0, 0.0, 0.0, 0.065682]),
        (373674, [0.0, 0.0, 0.0, 0.0518601]),
    ]
);
declare_test_file!(
    conformance_test_images_cmyk_layers,
    "conformance_test_images/cmyk_layers.jxl"
);
declare_test_file!(
    conformance_test_images_delta_palette,
    "conformance_test_images/delta_palette.jxl",
    checkpoints: &[
        (36654, [0.0, 0.248164, 0.311546, 0.487548]),
        (79212, [0.0, 0.0, 0.149023, 0.359282]),
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
        (1107, [0.00876732, 0.0163436, 0.0301204, 0.22971]),
        (45756, [0.000289947, 0.000982232, 0.00253717, 0.0191193]),
        (49692, [0.000129187, 0.000520256, 0.00187197, 0.0191193]),
        (52767, [0.00000441362, 0.00028804, 0.0012267, 0.0191193]),
        (55719, [0.0, 0.000124816, 0.000671951, 0.0191193]),
        (58179, [0.0, 0.00000543873, 0.000331689, 0.0191193]),
        (60024, [0.0, 0.0, 0.000157027, 0.0190378]),
        (62976, [0.0, 0.0, 0.0, 0.0179678]),
        (63345, [0.0, 0.0, 0.0, 0.0179672]),
    ]
);
declare_test_file!(
    conformance_test_images_lossless_pfm,
    "conformance_test_images/lossless_pfm.jxl",
    checkpoints: &[
        (215004, [0.0, 0.356932, 0.810473, 1.58484]),
        (400734, [0.0, 0.0, 0.466985, 1.17089]),
        (586341, [0.0, 0.0, 0.0, 1.062]),
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
        (11685, [0.00763281, 0.0117922, 0.0152014, 0.0256782]),
        (32103, [0.000970023, 0.0104656, 0.0147687, 0.0256782]),
        (73431, [0.0, 0.0, 0.0111172, 0.0256782]),
        (107994, [0.0, 0.0, 0.0, 0.0190241]),
    ]
);
declare_test_file!(
    conformance_test_images_noise_5,
    "conformance_test_images/noise_5.jxl",
    checkpoints: &[
        (11685, [0.00763281, 0.0117922, 0.0152014, 0.0256782]),
        (32103, [0.000970023, 0.0104656, 0.0147687, 0.0256782]),
        (73431, [0.0, 0.0, 0.0111172, 0.0256782]),
        (107994, [0.0, 0.0, 0.0, 0.0190241]),
    ]
);
declare_test_file!(
    conformance_test_images_opsin_inverse,
    "conformance_test_images/opsin_inverse.jxl",
    checkpoints: &[
        (11685, [0.0111216, 0.0215356, 0.0312848, 0.0599849]),
        (32103, [0.000786853, 0.0164581, 0.0296952, 0.0599849]),
        (73431, [0.0, 0.0, 0.0196077, 0.0599849]),
        (107994, [0.0, 0.0, 0.0, 0.0424316]),
    ]
);
declare_test_file!(
    conformance_test_images_opsin_inverse_5,
    "conformance_test_images/opsin_inverse_5.jxl",
    checkpoints: &[
        (11685, [0.0111216, 0.0215356, 0.0312848, 0.0599849]),
        (32103, [0.000786853, 0.0164581, 0.0296952, 0.0599849]),
        (73431, [0.0, 0.0, 0.0196077, 0.0599849]),
        (107994, [0.0, 0.0, 0.0, 0.0424316]),
    ]
);
declare_test_file!(
    conformance_test_images_patches,
    "conformance_test_images/patches.jxl",
    checkpoints: &[
        (43911, [0.0000418253, 0.0000463952, 0.0000839527, 0.0237002]),
        (50676, [0.0, 0.0000416539, 0.0000601838, 0.0237002]),
        (57072, [0.0, 0.0, 0.0000437617, 0.0237002]),
        (64452, [0.0, 0.0, 0.0, 0.0117261]),
    ]
);
declare_test_file!(
    conformance_test_images_patches_5,
    "conformance_test_images/patches_5.jxl",
    checkpoints: &[
        (43911, [0.0000418253, 0.0000463952, 0.0000839527, 0.0237002]),
        (50676, [0.0, 0.0000416539, 0.0000601838, 0.0237002]),
        (57072, [0.0, 0.0, 0.0000437617, 0.0237002]),
        (64452, [0.0, 0.0, 0.0, 0.0117261]),
    ]
);
declare_test_file!(
    conformance_test_images_progressive,
    "conformance_test_images/progressive.jxl",
    checkpoints: &[
        (127674, [0.000082287, 0.00121099, 0.00743871, 0.0407392]),
        (172692, [0.0000561893, 0.000789923, 0.00568393, 0.0290658]),
        (201228, [0.0000358902, 0.000472265, 0.00378548, 0.0290658]),
        (227673, [0.0000219523, 0.000354972, 0.0023623, 0.0290658]),
        (253749, [0.0000157334, 0.000224483, 0.00149931, 0.0247794]),
        (277242, [0.00000425047, 0.000127316, 0.00105421, 0.00942424]),
        (316110, [0.0, 0.0000212103, 0.000522969, 0.00941023]),
        (372075, [0.0, 0.0, 0.000100646, 0.00791961]),
        (466662, [0.0, 0.0, 0.0, 0.00215178]),
    ]
);
declare_test_file!(
    conformance_test_images_progressive_5,
    "conformance_test_images/progressive_5.jxl",
    checkpoints: &[
        (127674, [0.000082287, 0.00121099, 0.00743871, 0.0407392]),
        (172692, [0.0000561893, 0.000789923, 0.00568393, 0.0290658]),
        (201228, [0.0000358902, 0.000472265, 0.00378548, 0.0290658]),
        (227673, [0.0000219523, 0.000354972, 0.0023623, 0.0290658]),
        (253749, [0.0000157334, 0.000224483, 0.00149931, 0.0247794]),
        (277242, [0.00000425047, 0.000127316, 0.00105421, 0.00942424]),
        (316110, [0.0, 0.0000212103, 0.000522969, 0.00941023]),
        (372075, [0.0, 0.0, 0.000100646, 0.00791961]),
        (466662, [0.0, 0.0, 0.0, 0.00215178]),
    ]
);
declare_test_file!(
    conformance_test_images_spot,
    "conformance_test_images/spot.jxl",
    checkpoints: &[
        (391017, [0.0, 0.0127997, 0.195033, 0.563661]),
        (391878, [0.0, 0.0, 0.0, 0.563661]),
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
declare_test_file!(ec_upsampling8_multi_group, "ec_upsampling8_multi_group.jxl");
declare_test_file!(
    dice,
    "dice.jxl",
    checkpoints: &[
        (20664, [0.00000865394, 0.000303827, 0.00241439, 0.00991502]),
        (32595, [0.0, 0.00000940565, 0.000557175, 0.00550891]),
        (44895, [0.0, 0.0, 0.0000020706, 0.00381681]),
        (46371, [0.0, 0.0, 0.0, 0.000542174]),
    ]
);
declare_test_file!(
    efb,
    "efb.jxl",
    checkpoints: &[
        (2460, [0.0010807, 0.00329146, 0.00890474, 0.084241]),
        (4305, [0.000322628, 0.00101129, 0.00373566, 0.0286792]),
        (8610, [0.000130922, 0.000506364, 0.00174055, 0.0117385]),
        (12423, [0.0000872397, 0.000334025, 0.0012309, 0.00734448]),
        (18081, [0.0000173074, 0.0000716683, 0.000285197, 0.00167398]),
        (19188, [0.0, 0.0, 0.0000298308, 0.00109322]),
        (19680, [0.0, 0.0, 0.0, 0.000560946]),
    ]
);
declare_test_file!(extra_channels, "extra_channels.jxl");
declare_test_file!(gray_alpha_lossless, "gray_alpha_lossless.jxl");
declare_test_file!(
    grayscale_patches_modular,
    "grayscale_patches_modular.jxl",
    checkpoints: &[
        (4182, [0.0, 0.0, 0.0, 0.00496795]),
        (4674, [0.0, 0.0, 0.0, 0.00496795]),
        (5043, [0.0, 0.0, 0.0, 0.00496795]),
        (5412, [0.0, 0.0, 0.0, 0.0]),
    ]
);
declare_test_file!(
    grayscale_patches_var_dct,
    "grayscale_patches_var_dct.jxl",
    checkpoints: &[
        (6396, [0.0000181674, 0.0000808498, 0.000155331, 0.00294215]),
        (8856, [0.0, 0.0000179203, 0.000113907, 0.00294215]),
        (11931, [0.0, 0.0, 0.0, 0.00294215]),
        (12546, [0.0, 0.0, 0.0, 0.0]),
    ]
);
declare_test_file!(
    green_queen_modular_e3,
    "green_queen_modular_e3.jxl",
    checkpoints: &[
        (80442, [0.0, 0.140004, 0.186681, 0.381643]),
        (141450, [0.0, 0.115699, 0.162135, 0.381643]),
        (279702, [0.0, 0.0, 0.0, 0.23466]),
        (300981, [0.0, 0.0, 0.0, 0.129488]),
    ]
);
declare_test_file!(
    green_queen_vardct_e3,
    "green_queen_vardct_e3.jxl",
    checkpoints: &[
        (9348, [0.00827082, 0.0118848, 0.0166202, 0.0226725]),
        (27675, [0.0, 0.0108419, 0.015412, 0.0226725]),
        (61008, [0.0, 0.0, 0.0000678705, 0.0226725]),
        (84255, [0.0, 0.0, 0.0, 0.0201893]),
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
        (673302, [0.0, 0.0, 0.0, 0.0]),
    ]
);
declare_test_file!(
    issue728_minimal,
    "issue728_minimal.jxl",
    checkpoints: &[
        (1107, [0.0, 0.0, 0.0, 0.0124522]),
        (1230, [0.0, 0.0, 0.0, 0.00253025]),
        (2829, [0.0, 0.0, 0.0, 0.0]),
    ]
);
declare_test_file!(
    issue772_blendbug,
    "issue772_blendbug.jxl",
    checkpoints: &[
        (10332, [0.0, 0.0, 0.0, 0.0171032]),
        (11193, [0.0, 0.0, 0.0, 0.0132924]),
    ]
);
declare_test_file!(issue865_large_toc, "issue865_large_toc.jxl");
declare_test_file!(large_header, "large_header.jxl");
declare_test_file!(lossy_with_icc, "lossy_with_icc.jxl");
declare_test_file!(
    multiple_layers_noise_spline,
    "multiple_layers_noise_spline.jxl",
    checkpoints: &[
        (246, [0.397928, 0.481508, 0.633433, 1.42151]),
    ]
);
declare_test_file!(multiple_lf_420, "multiple_lf_420.jxl");
declare_test_file!(named_frame_test, "named_frame_test.jxl");
declare_test_file!(
    narrow_edge_group,
    "narrow_edge_group.jxl",
    checkpoints: &[
        (2706, [0.144355, 0.146256, 0.14813, 0.148141]),
        (3075, [0.0, 0.144355, 0.14813, 0.148132]),
        (3444, [0.0, 0.0, 0.0, 0.148132]),
        (3567, [0.0, 0.0, 0.0, 0.148131]),
    ]
);
declare_test_file!(oddsize_ups, "oddsize_ups.jxl");
declare_test_file!(
    red_420,
    "red_420.jxl",
    checkpoints: &[
        (6150, [0.000906281, 0.000935127, 0.00096611, 0.00103002]),
        (18204, [0.000897192, 0.000924399, 0.000961584, 0.00103002]),
        (30135, [0.000862845, 0.000913901, 0.000958284, 0.00103002]),
        (42312, [0.0, 0.000900331, 0.000943442, 0.00103002]),
        (54243, [0.0, 0.000879116, 0.000940313, 0.00103002]),
        (66297, [0.0, 0.0, 0.000919767, 0.00103002]),
        (78351, [0.0, 0.0, 0.000891372, 0.00103002]),
        (90405, [0.0, 0.0, 0.0, 0.0010086]),
    ]
);
declare_test_file!(
    red_422,
    "red_422.jxl",
    checkpoints: &[
        (7995, [0.00100694, 0.0010449, 0.00106196, 0.0011467]),
        (21648, [0.00099264, 0.0010377, 0.00106096, 0.0011467]),
        (35301, [0.000953013, 0.0010253, 0.00105661, 0.0011467]),
        (48954, [0.0, 0.00100186, 0.00105151, 0.0011467]),
        (62730, [0.0, 0.000979137, 0.00104896, 0.0011467]),
        (76383, [0.0, 0.0, 0.00104071, 0.0011467]),
        (90159, [0.0, 0.0, 0.00099512, 0.0011467]),
        (103812, [0.0, 0.0, 0.0, 0.00111499]),
    ]
);
declare_test_file!(
    red_440,
    "red_440.jxl",
    checkpoints: &[
        (8610, [0.00101258, 0.00103663, 0.00104906, 0.00110723]),
        (21771, [0.00101238, 0.00103254, 0.00104589, 0.00110612]),
        (35055, [0.0009981, 0.00101308, 0.00104589, 0.00110612]),
        (48339, [0.0, 0.00101258, 0.00104176, 0.00110612]),
        (61623, [0.0, 0.0009981, 0.00104039, 0.00110612]),
        (74907, [0.0, 0.0, 0.00103254, 0.00110612]),
        (88191, [0.0, 0.0, 0.00101258, 0.00110612]),
        (101352, [0.0, 0.0, 0.0, 0.00110612]),
    ]
);
declare_test_file!(
    ooo_jxlp_empty_dc_group_boxes,
    "ooo_jxlp_empty_dc_group_boxes.jxl"
);
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
        (127797, [0.000134775, 0.00090547, 0.00268865, 0.0107587]),
        (165435, [0.0000645153, 0.000415859, 0.00251687, 0.0107587]),
        (214389, [0.0000376167, 0.000264739, 0.00082058, 0.0107587]),
        (256701, [0.0000297243, 0.000174719, 0.000652265, 0.00501807]),
        (291018, [0.0000218225, 0.0000756224, 0.000469406, 0.00501807]),
        (328779, [0.0000159321, 0.0000561574, 0.000161076, 0.00501807]),
        (368262, [0.00000765154, 0.0000332722, 0.000145919, 0.00238661]),
        (413034, [0.0, 0.0, 0.0000862298, 0.00238661]),
        (495075, [0.0, 0.0, 0.0, 0.0000485131]),
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
        (1599, [0.00217423, 0.0110131, 0.0213114, 0.0424601]),
    ]
);
declare_test_file!(strategic_solid_blue, "strategic_solid_blue.jxl");
declare_test_file!(
    tirr_photo,
    "tirr_photo.jxl",
    checkpoints: &[
        (427425, [0.0000907563, 0.000416052, 0.00152326, 0.00679655]),
        (704790, [0.0000470144, 0.000174987, 0.000547695, 0.00401015]),
        (827667, [0.0000263605, 0.0000783926, 0.000272644, 0.00181453]),
        (1203801, [0.0, 0.0, 0.0000396942, 0.00107394]),
    ],
    skip_shuttle
);
declare_test_file!(tree_max_property_20, "tree_max_property_20.jxl");
declare_test_file!(upsampled_alpha, "upsampled_alpha.jxl");
declare_test_file!(upsampling2_permuted_toc, "upsampling2_permuted_toc.jxl");
declare_test_file!(with_icc, "with_icc.jxl");
declare_test_file!(with_preview, "with_preview.jxl");
declare_test_file!(
    zoltan_tasi_unsplash,
    "zoltan_tasi_unsplash.jxl",
    checkpoints: &[
        (38253, [0.00477679, 0.0121947, 0.0230068, 0.07555]),
        (159531, [0.0, 0.00435373, 0.0187669, 0.07555]),
        (293847, [0.0, 0.0, 0.00355882, 0.0667573]),
        (396429, [0.0, 0.0, 0.0, 0.0275518]),
    ]
);
