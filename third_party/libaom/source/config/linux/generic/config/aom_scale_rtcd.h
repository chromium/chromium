// This file is generated. Do not edit.
#ifndef AOM_SCALE_RTCD_H_
#define AOM_SCALE_RTCD_H_

#ifdef RTCD_C
#define RTCD_EXTERN
#else
#define RTCD_EXTERN extern
#endif

struct yv12_buffer_config;

#ifdef __cplusplus
extern "C" {
#endif

void aom_extend_frame_borders_c(struct yv12_buffer_config* ybf,
                                const int num_planes);
#define aom_extend_frame_borders aom_extend_frame_borders_c

void aom_extend_frame_borders_y_c(struct yv12_buffer_config* ybf);
#define aom_extend_frame_borders_y aom_extend_frame_borders_y_c

void aom_extend_frame_inner_borders_c(struct yv12_buffer_config* ybf,
                                      const int num_planes);
#define aom_extend_frame_inner_borders aom_extend_frame_inner_borders_c

void aom_horizontal_line_2_1_scale_c(const unsigned char* source,
                                     unsigned int source_width,
                                     unsigned char* dest,
                                     unsigned int dest_width);
#define aom_horizontal_line_2_1_scale aom_horizontal_line_2_1_scale_c

void aom_horizontal_line_5_3_scale_c(const unsigned char* source,
                                     unsigned int source_width,
                                     unsigned char* dest,
                                     unsigned int dest_width);
#define aom_horizontal_line_5_3_scale aom_horizontal_line_5_3_scale_c

void aom_horizontal_line_5_4_scale_c(const unsigned char* source,
                                     unsigned int source_width,
                                     unsigned char* dest,
                                     unsigned int dest_width);
#define aom_horizontal_line_5_4_scale aom_horizontal_line_5_4_scale_c

void aom_vertical_band_2_1_scale_c(unsigned char* source,
                                   int src_pitch,
                                   unsigned char* dest,
                                   int dest_pitch,
                                   unsigned int dest_width);
#define aom_vertical_band_2_1_scale aom_vertical_band_2_1_scale_c

void aom_vertical_band_2_1_scale_i_c(unsigned char* source,
                                     int src_pitch,
                                     unsigned char* dest,
                                     int dest_pitch,
                                     unsigned int dest_width);
#define aom_vertical_band_2_1_scale_i aom_vertical_band_2_1_scale_i_c

void aom_vertical_band_5_3_scale_c(unsigned char* source,
                                   int src_pitch,
                                   unsigned char* dest,
                                   int dest_pitch,
                                   unsigned int dest_width);
#define aom_vertical_band_5_3_scale aom_vertical_band_5_3_scale_c

void aom_vertical_band_5_4_scale_c(unsigned char* source,
                                   int src_pitch,
                                   unsigned char* dest,
                                   int dest_pitch,
                                   unsigned int dest_width);
#define aom_vertical_band_5_4_scale aom_vertical_band_5_4_scale_c

void aom_yv12_copy_frame_c(const struct yv12_buffer_config* src_bc,
                           struct yv12_buffer_config* dst_bc,
                           const int num_planes);
#define aom_yv12_copy_frame aom_yv12_copy_frame_c

void aom_yv12_copy_u_c(const struct yv12_buffer_config* src_bc,
                       struct yv12_buffer_config* dst_bc);
#define aom_yv12_copy_u aom_yv12_copy_u_c

void aom_yv12_copy_v_c(const struct yv12_buffer_config* src_bc,
                       struct yv12_buffer_config* dst_bc);
#define aom_yv12_copy_v aom_yv12_copy_v_c

void aom_yv12_copy_y_c(const struct yv12_buffer_config* src_ybc,
                       struct yv12_buffer_config* dst_ybc);
#define aom_yv12_copy_y aom_yv12_copy_y_c

void aom_yv12_extend_frame_borders_c(struct yv12_buffer_config* ybf,
                                     const int num_planes);
#define aom_yv12_extend_frame_borders aom_yv12_extend_frame_borders_c

void aom_yv12_partial_coloc_copy_u_c(const struct yv12_buffer_config* src_bc,
                                     struct yv12_buffer_config* dst_bc,
                                     int hstart,
                                     int hend,
                                     int vstart,
                                     int vend);
#define aom_yv12_partial_coloc_copy_u aom_yv12_partial_coloc_copy_u_c

void aom_yv12_partial_coloc_copy_v_c(const struct yv12_buffer_config* src_bc,
                                     struct yv12_buffer_config* dst_bc,
                                     int hstart,
                                     int hend,
                                     int vstart,
                                     int vend);
#define aom_yv12_partial_coloc_copy_v aom_yv12_partial_coloc_copy_v_c

void aom_yv12_partial_coloc_copy_y_c(const struct yv12_buffer_config* src_ybc,
                                     struct yv12_buffer_config* dst_ybc,
                                     int hstart,
                                     int hend,
                                     int vstart,
                                     int vend);
#define aom_yv12_partial_coloc_copy_y aom_yv12_partial_coloc_copy_y_c

void aom_yv12_partial_copy_u_c(const struct yv12_buffer_config* src_bc,
                               int hstart1,
                               int hend1,
                               int vstart1,
                               int vend1,
                               struct yv12_buffer_config* dst_bc,
                               int hstart2,
                               int vstart2);
#define aom_yv12_partial_copy_u aom_yv12_partial_copy_u_c

void aom_yv12_partial_copy_v_c(const struct yv12_buffer_config* src_bc,
                               int hstart1,
                               int hend1,
                               int vstart1,
                               int vend1,
                               struct yv12_buffer_config* dst_bc,
                               int hstart2,
                               int vstart2);
#define aom_yv12_partial_copy_v aom_yv12_partial_copy_v_c

void aom_yv12_partial_copy_y_c(const struct yv12_buffer_config* src_ybc,
                               int hstart1,
                               int hend1,
                               int vstart1,
                               int vend1,
                               struct yv12_buffer_config* dst_ybc,
                               int hstart2,
                               int vstart2);
#define aom_yv12_partial_copy_y aom_yv12_partial_copy_y_c

int aom_yv12_realloc_with_new_border_c(struct yv12_buffer_config* ybf,
                                       int new_border,
                                       int byte_alignment,
                                       int num_planes);
#define aom_yv12_realloc_with_new_border aom_yv12_realloc_with_new_border_c

void aom_scale_rtcd(void);

#include "config/aom_config.h"

#ifdef RTCD_C
static void setup_rtcd_internal(void) {}
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif
