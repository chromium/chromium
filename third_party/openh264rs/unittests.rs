// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! GoogleTest integration test harness for `openh264rs`.

#![allow(non_snake_case, non_camel_case_types, non_upper_case_globals)]

#[macro_use]
extern crate openh264rs_noop_test_attribute;

use rust_gtest_interop::prelude::*;

#[path = "src/tests"]
mod integration_tests {
    pub(crate) mod api_lifecycle_test;
    pub(crate) mod api_param_bounds_test;
    pub(crate) mod decoder_conformance_test;
    pub(crate) mod decoder_error_reporting_parity;
    pub(crate) mod decoder_nodelay_parity_test;
    pub(crate) mod decoder_numref_change_test;
    pub(crate) mod decoder_options_parity_test;
    pub(crate) mod decoder_parseonly_parity_test;
    pub(crate) mod decoder_resolution_change_test;
    pub(crate) mod encode_options_test;
    pub(crate) mod encoder_ext_test;
    pub(crate) mod encoder_force_idr_ltr_test;
    pub(crate) mod encoder_output_sha1_test;
    pub(crate) mod encoder_preprocess_plugins_test;
    pub(crate) mod error_concealment_api_test;
    pub(crate) mod kernels_differential_phase2;
    pub(crate) mod loopback_sha1_test;
    pub(crate) mod ltr_test;
    pub(crate) mod malformed_stream_parity;
    pub(crate) mod multithread_decoder_test;
    pub(crate) mod multithread_encoder_test;
    pub(crate) mod safe_bits_differential;
    pub(crate) mod safe_plane_differential;
    pub(crate) mod simple_test;
    pub(crate) mod trace_callback_test;
    pub(crate) mod trace_default_sink_test;
}

macro_rules! openh264rs_gtests {
    ($( $mod:ident : [ $( $test:ident ),+ $(,)? ] ),+ $(,)?) => {
        $(
            mod $mod {
                use super::*;
                $(
                    #[gtest(OpenH264RsTest, $test)]
                    fn $test() {
                        integration_tests::$mod::$test();
                    }
                )+
            }
        )+
    };
}

openh264rs_gtests! {
    api_lifecycle_test: [
        test_c_abi_bool_and_struct_alignment,
        test_decoder_create_and_destroy_lifecycle,
        test_encoder_create_and_destroy_lifecycle,
        test_decoder_reinit_does_not_inherit_reordering_slots,
        test_safe_core_types_are_exported,
        test_output_descriptors_validate_codec_and_bounds,
    ],
    api_param_bounds_test: [
        test_encoder_invalid_resolution_rejected,
        test_encoder_very_large_slices,
        test_encoder_screen_content_scroll_motion_vector_bounds,
        test_decoder_get_set_options_vcl_nal_framenum_idr_isref,
        test_encoder_custom_chroma_plane_strides,
        test_encoder_custom_chroma_plane_strides_invalid_src,
        test_encoder_initialize_ext_rejects_geometry_overflow,
    ],
    decoder_conformance_test: [
        test_sha1_hasher_sanity,
        test_decoder_capability_query,
        test_asset_ba_mw_d,
        test_asset_adobe_pdf_sample_a_1024x768_50frms,
        test_asset_ba1_ft_c,
        test_asset_ba1_sony_d,
        test_asset_ba2_sony_f,
        test_asset_bamq1_jvc_c,
        test_asset_bamq2_jvc_c,
        test_asset_banm_mw_d,
        test_asset_basqp1_sony_c,
        test_asset_caba2_sva_b,
        test_asset_caba3_sva_b,
        test_asset_cabast3_sony_e,
        test_asset_canl1_sony_e,
        test_asset_canl1_toshiba_g,
        test_asset_canl2_sony_e,
        test_asset_ci1_ft_b,
        test_asset_ci_mw_d,
        test_asset_cvbs3_sony_c,
        test_asset_cvfc1_sony_c,
        test_asset_ls_sva_d,
        test_asset_midr_mw_d,
        test_asset_mps_mw_a,
        test_asset_mr1_bt_a,
        test_asset_mr1_mw_a,
        test_asset_mr2_mw_a,
        test_asset_mr2_tandberg_e,
        test_asset_nl1_sony_d,
        test_asset_nl2_sony_h,
        test_asset_nlmq1_jvc_c,
        test_asset_nlmq2_jvc_c,
        test_asset_nrf_mw_e,
        test_asset_qcif_2p_i_allipcm,
        test_asset_sva_ba1_b,
        test_asset_sva_ba2_d,
        test_asset_sva_base_b,
        test_asset_sva_cl1_e,
        test_asset_sva_fm1_e,
        test_asset_sva_nl1_b,
        test_asset_sva_nl2_e,
        test_asset_sarvui,
        test_asset_static,
        test_asset_zhling_1280x720,
        test_asset_sps_subsetsps_bothvui,
        test_asset_test_cif_p_cabac_slice,
        test_asset_test_qcif_cabac,
        test_asset_test_scalinglist_jm,
        test_asset_test_vd_1d,
        test_asset_test_vd_rc,
        test_asset_cisco_men_whisper_640x320_cabac_bframe_9,
        test_asset_cisco_men_whisper_640x320_cavlc_bframe_9,
        test_asset_cisco_adobe_pdf_sample_a_1024x768_cavlc_bframe_9,
        test_asset_narrow_16x16,
        test_asset_narrow_24x18,
        test_asset_narrow_16x16_idr_lost,
        test_asset_grid_48x32,
        test_asset_ba_mw_d_idr_lost,
        test_asset_ba_mw_d_p_lost,
        test_asset_fmo_2groups_64x64,
        test_asset_cqm8x8_intra_176x144,
        test_asset_cqm8x8_inter_176x144,
        test_asset_cqm8x8_cabac_inter_176x144,
        test_asset_high_8x8_multislice_176x144,
        test_asset_multislice_weighted_176x144,
        test_asset_cacqp3_sony_d,
    ],
    decoder_error_reporting_parity: [
        test_error_con_idc_out_of_range_is_clamped_at_set_option,
        test_parse_only_disables_error_concealment,
        test_second_initialize_rebuilds_the_context,
        test_avc_bitstream_type_notifies_key_frame_loss_when_ec_is_off,
        test_concealment_statistics_reach_get_statistics,
    ],
    decoder_nodelay_parity_test: [
        decode_frame_no_delay_matches_the_reference_on_every_axis,
    ],
    decoder_numref_change_test: [
        num_ref_frame_change_resizes_the_pool_and_matches_the_reference,
    ],
    decoder_options_parity_test: [
        get_option_per_call_across_streams,
        option_error_codes_match_the_reference,
    ],
    decoder_parseonly_parity_test: [
        decode_parser_matches_expected_output_on_every_asset,
    ],
    decoder_resolution_change_test: [
        resolution_change_stream_matches_the_reference_and_does_not_abort,
    ],
    encode_options_test: [
        test_encoder_set_and_get_options,
        test_set_get_option_matches_cxx_for_every_option,
        set_option_enc_param_base_roundtrip,
        parameter_set_strategy_increasing_id,
        parameter_set_strategy_sps_listing_and_pps_increasing_1,
        parameter_set_strategy_sps_listing_and_pps_increasing_2,
        parameter_set_strategy_sps_listing_and_pps_increasing_3,
        parameter_set_strategy_sps_pps_listing_1_2_3,
        simulcast_svc_roundtrip,
        simulcast_avc_and_sps_pps_listing_roundtrip,
        set_enc_option_size_sizelimited_decode,
        set_enc_option_size_multithread_fixed_slice,
        set_enc_option_size_annex_a_extreme_dimensions,
        simulcast_avc_diff_fps,
        diff_slicing_in_dlayer_and_mixed,
        thread_num_and_slice_num_and_load_balancing,
        profile_level_setting_roundtrip,
        avc_svc_extension_check,
        unsupported_video_size_input_and_screen_ltr_non_lossless,
        temporal_layer_change_during_encoding,
        encoder_option_idr_interval_schedule,
    ],
    encoder_ext_test: [
        encoder_init_test_very_large_slices,
        encoder_init_test_dynamic_adjust_slicing_extreme_aspect_ratio,
        encoder_init_test_per_frame_psnr,
        encoder_interface_test_encoder_option_set,
        encoder_interface_test_memory_check,
        encoder_interface_test_basic_initialize_and_base_param_setting,
        encoder_interface_test_basic_initialize_false,
        encoder_interface_test_basic_initialize_auto_adjustment,
        encoder_interface_test_force_intra_frame_simulcast_avc,
        encoder_interface_test_force_intra_frame_and_temporal,
        encoder_interface_test_encode_parameter_sets,
        encoder_interface_test_get_statistics,
        encoder_interface_test_frame_size_check,
        encoder_interface_test_skip_frame_check,
        encoder_interface_test_diff_resolution_check,
        encoder_interface_test_nal_size_checking,
    ],
    encoder_force_idr_ltr_test: [
        force_intra_frame_with_ltr_gives_an_idr_every_time,
        test_high_bitrate_idr_target_bits,
    ],
    encoder_output_sha1_test: [
        camera_320x192_single,
        camera_160x96_single,
        static_152x100_single,
        camera_320x192_raster,
        camera_320x192_denoise,
        camera_320x192_2layers,
        camera_1280x720_sizelimited,
        camera_1280x720_4layers,
        screen_320x192_single,
        screen_160x96_single,
        screen_152x100_single,
        screen_1280x720_sizelimited,
        screen_1280x720_lossless_ltr,
        camera_320x192_cabac_single,
        camera_1280x720_cabac_szlim,
        camera_1280x720_cabac_fixed,
        screen_320x192_scrolling,
        camera_320x192_temporal4,
        camera_320x192_rc_bitrate_skip,
        camera_320x192_rc_off_qp24,
        camera_320x192_deblock_off,
        camera_320x192_deblock_slice,
        camera_320x192_high_complexity,
        screen_320x192_high_comp_ltr,
    ],
    encoder_preprocess_plugins_test: [
        screen_content_is_accepted_at_init,
        screen_content_with_lossless_ltr_is_accepted_at_init,
        screen_content_ltr_without_lossless_link_is_accepted_at_init,
        screen_content_encodes_a_sequence,
        the_gates_own_single_layer_configuration_is_still_accepted,
        denoise_is_accepted_and_changes_the_output,
        a_downsampled_spatial_layer_is_accepted_and_encoded,
        four_spatial_layers_at_720p_are_accepted_and_encoded,
        a_layer_rounded_up_to_a_macroblock_multiple_is_not_a_downsample,
        screen_content_scrolling_source_codes_smaller_than_camera,
        source_larger_than_max_mbs_per_frame,
    ],
    error_concealment_api_test: [
        test_decoder_error_concealment_modes,
        test_error_con_idc_set_after_initialize_reaches_the_decoder,
    ],
    kernels_differential_phase2: [
        idct_four_res_add_pred_transforms_a_dc_only_block_with_zero_nzc,
        sad_kernels_stay_inside_the_spans_they_declare,
        vaa_shims_stay_inside_the_spans_they_declare,
        vaa_shim_reads_each_quadrant_where_its_contract_says_it_does,
        sample_variance_16x16_accumulators_cannot_wrap,
        expand_picture_writes_every_padding_byte_and_reads_none,
        encode_mb_aux_shims_stay_inside_the_spans_they_declare,
        encoder_recon_shims_stay_inside_the_spans_they_declare,
        satd_kernels_stay_inside_the_spans_they_declare,
        nonzero_count_duplicates_agree,
    ],
    loopback_sha1_test: [
        test_loopback_encode_and_decode_pipeline,
        test_decode_encode_full_cycle_sha1_parity,
    ],
    ltr_test: [
        test_ltr_encoder_configuration,
        get_option_ltr_all_ltr_case_0,
        get_option_ltr_all_ltr_case_1,
        get_option_ltr_all_ltr_case_2,
        get_option_ltr_engine_case_0,
        get_option_ltr_engine_case_1,
        get_option_ltr_engine_case_2,
        engine_svc_switch_i,
        engine_svc_switch_p,
        set_option_ec_flag_error_con_disable_case_0,
        set_option_ec_flag_error_con_disable_case_1,
        set_option_ec_flag_error_con_disable_case_2,
    ],
    malformed_stream_parity: [
        start_code_scan_agrees_with_split_annexb_units,
        malformed_degenerate_nals,
        malformed_sarvui_parameter_sets,
        mid_au_sps_dimension_increase_before_need_error_con_does_not_panic,
    ],
    multithread_decoder_test: [
        test_multithread_decoder_initialization,
    ],
    multithread_encoder_test: [
        test_multithread_encoder,
    ],
    safe_bits_differential: [
        leading_zero_bits_matches_the_table,
        trailing_bits_matches_for_every_byte,
        cavlc_mode_matches_the_raw_pair_from_prng_cursor_states,
        cavlc_mode_matches_at_every_bit_phase,
        cavlc_mode_matches_where_the_prime_leans_on_the_slop,
        exp_golomb_sizes_match_the_table_driven_versions,
        written_streams_read_back_through_the_old_reader,
    ],
    safe_plane_differential: [
        plane_samples_match_raw_mid_pointer_arithmetic,
        plane_cursors_match_the_roving_macroblock_pointer,
        plane_writes_land_where_the_raw_pointer_would_have_put_them,
    ],
    simple_test: [
        test_simple_encode_init_and_encode_param_sets,
    ],
    trace_callback_test: [
        test_encoder_trace_callback_receives_the_init_line,
        test_encoder_trace_level_filters,
        test_decoder_trace_callback_and_the_error_throttle,
    ],
    trace_default_sink_test: [
        the_child_case,
        a_fresh_decoder_writes_its_error_to_stderr_with_no_callback_installed,
        a_fresh_encoder_writes_its_error_to_stderr_with_no_callback_installed,
        an_installed_callback_replaces_the_default_sink_on_both_codecs,
        the_decoder_defaults_to_error_level_so_its_warnings_are_silent,
    ],
}
