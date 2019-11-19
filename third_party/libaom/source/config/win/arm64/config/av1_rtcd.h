// This file is generated. Do not edit.
#ifndef AV1_RTCD_H_
#define AV1_RTCD_H_

#ifdef RTCD_C
#define RTCD_EXTERN
#else
#define RTCD_EXTERN extern
#endif

/*
 * AV1
 */

#include "aom/aom_integer.h"
#include "aom_dsp/txfm_common.h"
#include "av1/common/av1_txfm.h"
#include "av1/common/common.h"
#include "av1/common/convolve.h"
#include "av1/common/enums.h"
#include "av1/common/filter.h"
#include "av1/common/odintrin.h"
#include "av1/common/quant_common.h"
#include "av1/common/restoration.h"

struct macroblockd;

/* Encoder forward decls */
struct macroblock;
struct txfm_param;
struct aom_variance_vtable;
struct search_site_config;
struct yv12_buffer_config;
struct NN_CONFIG;
typedef struct NN_CONFIG NN_CONFIG;

/* Function pointers return by CfL functions */
typedef void (*cfl_subsample_lbd_fn)(const uint8_t* input,
                                     int input_stride,
                                     uint16_t* output_q3);

typedef void (*cfl_subsample_hbd_fn)(const uint16_t* input,
                                     int input_stride,
                                     uint16_t* output_q3);

typedef void (*cfl_subtract_average_fn)(const uint16_t* src, int16_t* dst);

typedef void (*cfl_predict_lbd_fn)(const int16_t* src,
                                   uint8_t* dst,
                                   int dst_stride,
                                   int alpha_q3);

typedef void (*cfl_predict_hbd_fn)(const int16_t* src,
                                   uint16_t* dst,
                                   int dst_stride,
                                   int alpha_q3,
                                   int bd);

#ifdef __cplusplus
extern "C" {
#endif

void apply_selfguided_restoration_c(const uint8_t* dat,
                                    int width,
                                    int height,
                                    int stride,
                                    int eps,
                                    const int* xqd,
                                    uint8_t* dst,
                                    int dst_stride,
                                    int32_t* tmpbuf,
                                    int bit_depth,
                                    int highbd);
void apply_selfguided_restoration_neon(const uint8_t* dat,
                                       int width,
                                       int height,
                                       int stride,
                                       int eps,
                                       const int* xqd,
                                       uint8_t* dst,
                                       int dst_stride,
                                       int32_t* tmpbuf,
                                       int bit_depth,
                                       int highbd);
#define apply_selfguided_restoration apply_selfguided_restoration_neon

void av1_build_compound_diffwtd_mask_c(uint8_t* mask,
                                       DIFFWTD_MASK_TYPE mask_type,
                                       const uint8_t* src0,
                                       int src0_stride,
                                       const uint8_t* src1,
                                       int src1_stride,
                                       int h,
                                       int w);
#define av1_build_compound_diffwtd_mask av1_build_compound_diffwtd_mask_c

void av1_build_compound_diffwtd_mask_d16_c(uint8_t* mask,
                                           DIFFWTD_MASK_TYPE mask_type,
                                           const CONV_BUF_TYPE* src0,
                                           int src0_stride,
                                           const CONV_BUF_TYPE* src1,
                                           int src1_stride,
                                           int h,
                                           int w,
                                           ConvolveParams* conv_params,
                                           int bd);
void av1_build_compound_diffwtd_mask_d16_neon(uint8_t* mask,
                                              DIFFWTD_MASK_TYPE mask_type,
                                              const CONV_BUF_TYPE* src0,
                                              int src0_stride,
                                              const CONV_BUF_TYPE* src1,
                                              int src1_stride,
                                              int h,
                                              int w,
                                              ConvolveParams* conv_params,
                                              int bd);
#define av1_build_compound_diffwtd_mask_d16 \
  av1_build_compound_diffwtd_mask_d16_neon

void av1_build_compound_diffwtd_mask_highbd_c(uint8_t* mask,
                                              DIFFWTD_MASK_TYPE mask_type,
                                              const uint8_t* src0,
                                              int src0_stride,
                                              const uint8_t* src1,
                                              int src1_stride,
                                              int h,
                                              int w,
                                              int bd);
#define av1_build_compound_diffwtd_mask_highbd \
  av1_build_compound_diffwtd_mask_highbd_c

void av1_convolve_2d_copy_sr_c(const uint8_t* src,
                               int src_stride,
                               uint8_t* dst,
                               int dst_stride,
                               int w,
                               int h,
                               const InterpFilterParams* filter_params_x,
                               const InterpFilterParams* filter_params_y,
                               const int subpel_x_q4,
                               const int subpel_y_q4,
                               ConvolveParams* conv_params);
void av1_convolve_2d_copy_sr_neon(const uint8_t* src,
                                  int src_stride,
                                  uint8_t* dst,
                                  int dst_stride,
                                  int w,
                                  int h,
                                  const InterpFilterParams* filter_params_x,
                                  const InterpFilterParams* filter_params_y,
                                  const int subpel_x_q4,
                                  const int subpel_y_q4,
                                  ConvolveParams* conv_params);
#define av1_convolve_2d_copy_sr av1_convolve_2d_copy_sr_neon

void av1_convolve_2d_scale_c(const uint8_t* src,
                             int src_stride,
                             uint8_t* dst,
                             int dst_stride,
                             int w,
                             int h,
                             const InterpFilterParams* filter_params_x,
                             const InterpFilterParams* filter_params_y,
                             const int subpel_x_qn,
                             const int x_step_qn,
                             const int subpel_y_q4,
                             const int y_step_qn,
                             ConvolveParams* conv_params);
#define av1_convolve_2d_scale av1_convolve_2d_scale_c

void av1_convolve_2d_sr_c(const uint8_t* src,
                          int src_stride,
                          uint8_t* dst,
                          int dst_stride,
                          int w,
                          int h,
                          const InterpFilterParams* filter_params_x,
                          const InterpFilterParams* filter_params_y,
                          const int subpel_x_q4,
                          const int subpel_y_q4,
                          ConvolveParams* conv_params);
void av1_convolve_2d_sr_neon(const uint8_t* src,
                             int src_stride,
                             uint8_t* dst,
                             int dst_stride,
                             int w,
                             int h,
                             const InterpFilterParams* filter_params_x,
                             const InterpFilterParams* filter_params_y,
                             const int subpel_x_q4,
                             const int subpel_y_q4,
                             ConvolveParams* conv_params);
#define av1_convolve_2d_sr av1_convolve_2d_sr_neon

void av1_convolve_horiz_rs_c(const uint8_t* src,
                             int src_stride,
                             uint8_t* dst,
                             int dst_stride,
                             int w,
                             int h,
                             const int16_t* x_filters,
                             int x0_qn,
                             int x_step_qn);
#define av1_convolve_horiz_rs av1_convolve_horiz_rs_c

void av1_convolve_x_sr_c(const uint8_t* src,
                         int src_stride,
                         uint8_t* dst,
                         int dst_stride,
                         int w,
                         int h,
                         const InterpFilterParams* filter_params_x,
                         const InterpFilterParams* filter_params_y,
                         const int subpel_x_q4,
                         const int subpel_y_q4,
                         ConvolveParams* conv_params);
void av1_convolve_x_sr_neon(const uint8_t* src,
                            int src_stride,
                            uint8_t* dst,
                            int dst_stride,
                            int w,
                            int h,
                            const InterpFilterParams* filter_params_x,
                            const InterpFilterParams* filter_params_y,
                            const int subpel_x_q4,
                            const int subpel_y_q4,
                            ConvolveParams* conv_params);
#define av1_convolve_x_sr av1_convolve_x_sr_neon

void av1_convolve_y_sr_c(const uint8_t* src,
                         int src_stride,
                         uint8_t* dst,
                         int dst_stride,
                         int w,
                         int h,
                         const InterpFilterParams* filter_params_x,
                         const InterpFilterParams* filter_params_y,
                         const int subpel_x_q4,
                         const int subpel_y_q4,
                         ConvolveParams* conv_params);
void av1_convolve_y_sr_neon(const uint8_t* src,
                            int src_stride,
                            uint8_t* dst,
                            int dst_stride,
                            int w,
                            int h,
                            const InterpFilterParams* filter_params_x,
                            const InterpFilterParams* filter_params_y,
                            const int subpel_x_q4,
                            const int subpel_y_q4,
                            ConvolveParams* conv_params);
#define av1_convolve_y_sr av1_convolve_y_sr_neon

void av1_dist_wtd_convolve_2d_c(const uint8_t* src,
                                int src_stride,
                                uint8_t* dst,
                                int dst_stride,
                                int w,
                                int h,
                                const InterpFilterParams* filter_params_x,
                                const InterpFilterParams* filter_params_y,
                                const int subpel_x_q4,
                                const int subpel_y_q4,
                                ConvolveParams* conv_params);
void av1_dist_wtd_convolve_2d_neon(const uint8_t* src,
                                   int src_stride,
                                   uint8_t* dst,
                                   int dst_stride,
                                   int w,
                                   int h,
                                   const InterpFilterParams* filter_params_x,
                                   const InterpFilterParams* filter_params_y,
                                   const int subpel_x_q4,
                                   const int subpel_y_q4,
                                   ConvolveParams* conv_params);
#define av1_dist_wtd_convolve_2d av1_dist_wtd_convolve_2d_neon

void av1_dist_wtd_convolve_2d_copy_c(const uint8_t* src,
                                     int src_stride,
                                     uint8_t* dst,
                                     int dst_stride,
                                     int w,
                                     int h,
                                     const InterpFilterParams* filter_params_x,
                                     const InterpFilterParams* filter_params_y,
                                     const int subpel_x_q4,
                                     const int subpel_y_q4,
                                     ConvolveParams* conv_params);
void av1_dist_wtd_convolve_2d_copy_neon(
    const uint8_t* src,
    int src_stride,
    uint8_t* dst,
    int dst_stride,
    int w,
    int h,
    const InterpFilterParams* filter_params_x,
    const InterpFilterParams* filter_params_y,
    const int subpel_x_q4,
    const int subpel_y_q4,
    ConvolveParams* conv_params);
#define av1_dist_wtd_convolve_2d_copy av1_dist_wtd_convolve_2d_copy_neon

void av1_dist_wtd_convolve_x_c(const uint8_t* src,
                               int src_stride,
                               uint8_t* dst,
                               int dst_stride,
                               int w,
                               int h,
                               const InterpFilterParams* filter_params_x,
                               const InterpFilterParams* filter_params_y,
                               const int subpel_x_q4,
                               const int subpel_y_q4,
                               ConvolveParams* conv_params);
void av1_dist_wtd_convolve_x_neon(const uint8_t* src,
                                  int src_stride,
                                  uint8_t* dst,
                                  int dst_stride,
                                  int w,
                                  int h,
                                  const InterpFilterParams* filter_params_x,
                                  const InterpFilterParams* filter_params_y,
                                  const int subpel_x_q4,
                                  const int subpel_y_q4,
                                  ConvolveParams* conv_params);
#define av1_dist_wtd_convolve_x av1_dist_wtd_convolve_x_neon

void av1_dist_wtd_convolve_y_c(const uint8_t* src,
                               int src_stride,
                               uint8_t* dst,
                               int dst_stride,
                               int w,
                               int h,
                               const InterpFilterParams* filter_params_x,
                               const InterpFilterParams* filter_params_y,
                               const int subpel_x_q4,
                               const int subpel_y_q4,
                               ConvolveParams* conv_params);
void av1_dist_wtd_convolve_y_neon(const uint8_t* src,
                                  int src_stride,
                                  uint8_t* dst,
                                  int dst_stride,
                                  int w,
                                  int h,
                                  const InterpFilterParams* filter_params_x,
                                  const InterpFilterParams* filter_params_y,
                                  const int subpel_x_q4,
                                  const int subpel_y_q4,
                                  ConvolveParams* conv_params);
#define av1_dist_wtd_convolve_y av1_dist_wtd_convolve_y_neon

void av1_dr_prediction_z1_c(uint8_t* dst,
                            ptrdiff_t stride,
                            int bw,
                            int bh,
                            const uint8_t* above,
                            const uint8_t* left,
                            int upsample_above,
                            int dx,
                            int dy);
#define av1_dr_prediction_z1 av1_dr_prediction_z1_c

void av1_dr_prediction_z2_c(uint8_t* dst,
                            ptrdiff_t stride,
                            int bw,
                            int bh,
                            const uint8_t* above,
                            const uint8_t* left,
                            int upsample_above,
                            int upsample_left,
                            int dx,
                            int dy);
#define av1_dr_prediction_z2 av1_dr_prediction_z2_c

void av1_dr_prediction_z3_c(uint8_t* dst,
                            ptrdiff_t stride,
                            int bw,
                            int bh,
                            const uint8_t* above,
                            const uint8_t* left,
                            int upsample_left,
                            int dx,
                            int dy);
#define av1_dr_prediction_z3 av1_dr_prediction_z3_c

void av1_filter_intra_edge_c(uint8_t* p, int sz, int strength);
#define av1_filter_intra_edge av1_filter_intra_edge_c

void av1_filter_intra_edge_high_c(uint16_t* p, int sz, int strength);
#define av1_filter_intra_edge_high av1_filter_intra_edge_high_c

void av1_filter_intra_predictor_c(uint8_t* dst,
                                  ptrdiff_t stride,
                                  TX_SIZE tx_size,
                                  const uint8_t* above,
                                  const uint8_t* left,
                                  int mode);
#define av1_filter_intra_predictor av1_filter_intra_predictor_c

void av1_highbd_convolve8_c(const uint8_t* src,
                            ptrdiff_t src_stride,
                            uint8_t* dst,
                            ptrdiff_t dst_stride,
                            const int16_t* filter_x,
                            int x_step_q4,
                            const int16_t* filter_y,
                            int y_step_q4,
                            int w,
                            int h,
                            int bps);
#define av1_highbd_convolve8 av1_highbd_convolve8_c

void av1_highbd_convolve8_horiz_c(const uint8_t* src,
                                  ptrdiff_t src_stride,
                                  uint8_t* dst,
                                  ptrdiff_t dst_stride,
                                  const int16_t* filter_x,
                                  int x_step_q4,
                                  const int16_t* filter_y,
                                  int y_step_q4,
                                  int w,
                                  int h,
                                  int bps);
#define av1_highbd_convolve8_horiz av1_highbd_convolve8_horiz_c

void av1_highbd_convolve8_vert_c(const uint8_t* src,
                                 ptrdiff_t src_stride,
                                 uint8_t* dst,
                                 ptrdiff_t dst_stride,
                                 const int16_t* filter_x,
                                 int x_step_q4,
                                 const int16_t* filter_y,
                                 int y_step_q4,
                                 int w,
                                 int h,
                                 int bps);
#define av1_highbd_convolve8_vert av1_highbd_convolve8_vert_c

void av1_highbd_convolve_2d_copy_sr_c(const uint16_t* src,
                                      int src_stride,
                                      uint16_t* dst,
                                      int dst_stride,
                                      int w,
                                      int h,
                                      const InterpFilterParams* filter_params_x,
                                      const InterpFilterParams* filter_params_y,
                                      const int subpel_x_q4,
                                      const int subpel_y_q4,
                                      ConvolveParams* conv_params,
                                      int bd);
#define av1_highbd_convolve_2d_copy_sr av1_highbd_convolve_2d_copy_sr_c

void av1_highbd_convolve_2d_scale_c(const uint16_t* src,
                                    int src_stride,
                                    uint16_t* dst,
                                    int dst_stride,
                                    int w,
                                    int h,
                                    const InterpFilterParams* filter_params_x,
                                    const InterpFilterParams* filter_params_y,
                                    const int subpel_x_q4,
                                    const int x_step_qn,
                                    const int subpel_y_q4,
                                    const int y_step_qn,
                                    ConvolveParams* conv_params,
                                    int bd);
#define av1_highbd_convolve_2d_scale av1_highbd_convolve_2d_scale_c

void av1_highbd_convolve_2d_sr_c(const uint16_t* src,
                                 int src_stride,
                                 uint16_t* dst,
                                 int dst_stride,
                                 int w,
                                 int h,
                                 const InterpFilterParams* filter_params_x,
                                 const InterpFilterParams* filter_params_y,
                                 const int subpel_x_q4,
                                 const int subpel_y_q4,
                                 ConvolveParams* conv_params,
                                 int bd);
#define av1_highbd_convolve_2d_sr av1_highbd_convolve_2d_sr_c

void av1_highbd_convolve_avg_c(const uint8_t* src,
                               ptrdiff_t src_stride,
                               uint8_t* dst,
                               ptrdiff_t dst_stride,
                               const int16_t* filter_x,
                               int x_step_q4,
                               const int16_t* filter_y,
                               int y_step_q4,
                               int w,
                               int h,
                               int bps);
#define av1_highbd_convolve_avg av1_highbd_convolve_avg_c

void av1_highbd_convolve_copy_c(const uint8_t* src,
                                ptrdiff_t src_stride,
                                uint8_t* dst,
                                ptrdiff_t dst_stride,
                                const int16_t* filter_x,
                                int x_step_q4,
                                const int16_t* filter_y,
                                int y_step_q4,
                                int w,
                                int h,
                                int bps);
#define av1_highbd_convolve_copy av1_highbd_convolve_copy_c

void av1_highbd_convolve_horiz_rs_c(const uint16_t* src,
                                    int src_stride,
                                    uint16_t* dst,
                                    int dst_stride,
                                    int w,
                                    int h,
                                    const int16_t* x_filters,
                                    int x0_qn,
                                    int x_step_qn,
                                    int bd);
#define av1_highbd_convolve_horiz_rs av1_highbd_convolve_horiz_rs_c

void av1_highbd_convolve_x_sr_c(const uint16_t* src,
                                int src_stride,
                                uint16_t* dst,
                                int dst_stride,
                                int w,
                                int h,
                                const InterpFilterParams* filter_params_x,
                                const InterpFilterParams* filter_params_y,
                                const int subpel_x_q4,
                                const int subpel_y_q4,
                                ConvolveParams* conv_params,
                                int bd);
#define av1_highbd_convolve_x_sr av1_highbd_convolve_x_sr_c

void av1_highbd_convolve_y_sr_c(const uint16_t* src,
                                int src_stride,
                                uint16_t* dst,
                                int dst_stride,
                                int w,
                                int h,
                                const InterpFilterParams* filter_params_x,
                                const InterpFilterParams* filter_params_y,
                                const int subpel_x_q4,
                                const int subpel_y_q4,
                                ConvolveParams* conv_params,
                                int bd);
#define av1_highbd_convolve_y_sr av1_highbd_convolve_y_sr_c

void av1_highbd_dist_wtd_convolve_2d_c(
    const uint16_t* src,
    int src_stride,
    uint16_t* dst,
    int dst_stride,
    int w,
    int h,
    const InterpFilterParams* filter_params_x,
    const InterpFilterParams* filter_params_y,
    const int subpel_x_q4,
    const int subpel_y_q4,
    ConvolveParams* conv_params,
    int bd);
#define av1_highbd_dist_wtd_convolve_2d av1_highbd_dist_wtd_convolve_2d_c

void av1_highbd_dist_wtd_convolve_2d_copy_c(
    const uint16_t* src,
    int src_stride,
    uint16_t* dst,
    int dst_stride,
    int w,
    int h,
    const InterpFilterParams* filter_params_x,
    const InterpFilterParams* filter_params_y,
    const int subpel_x_q4,
    const int subpel_y_q4,
    ConvolveParams* conv_params,
    int bd);
#define av1_highbd_dist_wtd_convolve_2d_copy \
  av1_highbd_dist_wtd_convolve_2d_copy_c

void av1_highbd_dist_wtd_convolve_x_c(const uint16_t* src,
                                      int src_stride,
                                      uint16_t* dst,
                                      int dst_stride,
                                      int w,
                                      int h,
                                      const InterpFilterParams* filter_params_x,
                                      const InterpFilterParams* filter_params_y,
                                      const int subpel_x_q4,
                                      const int subpel_y_q4,
                                      ConvolveParams* conv_params,
                                      int bd);
#define av1_highbd_dist_wtd_convolve_x av1_highbd_dist_wtd_convolve_x_c

void av1_highbd_dist_wtd_convolve_y_c(const uint16_t* src,
                                      int src_stride,
                                      uint16_t* dst,
                                      int dst_stride,
                                      int w,
                                      int h,
                                      const InterpFilterParams* filter_params_x,
                                      const InterpFilterParams* filter_params_y,
                                      const int subpel_x_q4,
                                      const int subpel_y_q4,
                                      ConvolveParams* conv_params,
                                      int bd);
#define av1_highbd_dist_wtd_convolve_y av1_highbd_dist_wtd_convolve_y_c

void av1_highbd_dr_prediction_z1_c(uint16_t* dst,
                                   ptrdiff_t stride,
                                   int bw,
                                   int bh,
                                   const uint16_t* above,
                                   const uint16_t* left,
                                   int upsample_above,
                                   int dx,
                                   int dy,
                                   int bd);
#define av1_highbd_dr_prediction_z1 av1_highbd_dr_prediction_z1_c

void av1_highbd_dr_prediction_z2_c(uint16_t* dst,
                                   ptrdiff_t stride,
                                   int bw,
                                   int bh,
                                   const uint16_t* above,
                                   const uint16_t* left,
                                   int upsample_above,
                                   int upsample_left,
                                   int dx,
                                   int dy,
                                   int bd);
#define av1_highbd_dr_prediction_z2 av1_highbd_dr_prediction_z2_c

void av1_highbd_dr_prediction_z3_c(uint16_t* dst,
                                   ptrdiff_t stride,
                                   int bw,
                                   int bh,
                                   const uint16_t* above,
                                   const uint16_t* left,
                                   int upsample_left,
                                   int dx,
                                   int dy,
                                   int bd);
#define av1_highbd_dr_prediction_z3 av1_highbd_dr_prediction_z3_c

void av1_highbd_inv_txfm_add_c(const tran_low_t* dqcoeff,
                               uint8_t* dst,
                               int stride,
                               const TxfmParam* txfm_param);
#define av1_highbd_inv_txfm_add av1_highbd_inv_txfm_add_c

void av1_highbd_inv_txfm_add_16x4_c(const tran_low_t* dqcoeff,
                                    uint8_t* dst,
                                    int stride,
                                    const TxfmParam* txfm_param);
#define av1_highbd_inv_txfm_add_16x4 av1_highbd_inv_txfm_add_16x4_c

void av1_highbd_inv_txfm_add_4x16_c(const tran_low_t* dqcoeff,
                                    uint8_t* dst,
                                    int stride,
                                    const TxfmParam* txfm_param);
#define av1_highbd_inv_txfm_add_4x16 av1_highbd_inv_txfm_add_4x16_c

void av1_highbd_inv_txfm_add_4x4_c(const tran_low_t* dqcoeff,
                                   uint8_t* dst,
                                   int stride,
                                   const TxfmParam* txfm_param);
#define av1_highbd_inv_txfm_add_4x4 av1_highbd_inv_txfm_add_4x4_c

void av1_highbd_inv_txfm_add_4x8_c(const tran_low_t* dqcoeff,
                                   uint8_t* dst,
                                   int stride,
                                   const TxfmParam* txfm_param);
#define av1_highbd_inv_txfm_add_4x8 av1_highbd_inv_txfm_add_4x8_c

void av1_highbd_inv_txfm_add_8x4_c(const tran_low_t* dqcoeff,
                                   uint8_t* dst,
                                   int stride,
                                   const TxfmParam* txfm_param);
#define av1_highbd_inv_txfm_add_8x4 av1_highbd_inv_txfm_add_8x4_c

void av1_highbd_inv_txfm_add_8x8_c(const tran_low_t* dqcoeff,
                                   uint8_t* dst,
                                   int stride,
                                   const TxfmParam* txfm_param);
#define av1_highbd_inv_txfm_add_8x8 av1_highbd_inv_txfm_add_8x8_c

void av1_highbd_iwht4x4_16_add_c(const tran_low_t* input,
                                 uint8_t* dest,
                                 int dest_stride,
                                 int bd);
#define av1_highbd_iwht4x4_16_add av1_highbd_iwht4x4_16_add_c

void av1_highbd_iwht4x4_1_add_c(const tran_low_t* input,
                                uint8_t* dest,
                                int dest_stride,
                                int bd);
#define av1_highbd_iwht4x4_1_add av1_highbd_iwht4x4_1_add_c

void av1_highbd_warp_affine_c(const int32_t* mat,
                              const uint16_t* ref,
                              int width,
                              int height,
                              int stride,
                              uint16_t* pred,
                              int p_col,
                              int p_row,
                              int p_width,
                              int p_height,
                              int p_stride,
                              int subsampling_x,
                              int subsampling_y,
                              int bd,
                              ConvolveParams* conv_params,
                              int16_t alpha,
                              int16_t beta,
                              int16_t gamma,
                              int16_t delta);
#define av1_highbd_warp_affine av1_highbd_warp_affine_c

void av1_highbd_wiener_convolve_add_src_c(const uint8_t* src,
                                          ptrdiff_t src_stride,
                                          uint8_t* dst,
                                          ptrdiff_t dst_stride,
                                          const int16_t* filter_x,
                                          int x_step_q4,
                                          const int16_t* filter_y,
                                          int y_step_q4,
                                          int w,
                                          int h,
                                          const ConvolveParams* conv_params,
                                          int bps);
#define av1_highbd_wiener_convolve_add_src av1_highbd_wiener_convolve_add_src_c

void av1_inv_txfm2d_add_16x16_c(const int32_t* input,
                                uint16_t* output,
                                int stride,
                                TX_TYPE tx_type,
                                int bd);
#define av1_inv_txfm2d_add_16x16 av1_inv_txfm2d_add_16x16_c

void av1_inv_txfm2d_add_16x32_c(const int32_t* input,
                                uint16_t* output,
                                int stride,
                                TX_TYPE tx_type,
                                int bd);
#define av1_inv_txfm2d_add_16x32 av1_inv_txfm2d_add_16x32_c

void av1_inv_txfm2d_add_16x4_c(const int32_t* input,
                               uint16_t* output,
                               int stride,
                               TX_TYPE tx_type,
                               int bd);
#define av1_inv_txfm2d_add_16x4 av1_inv_txfm2d_add_16x4_c

void av1_inv_txfm2d_add_16x64_c(const int32_t* input,
                                uint16_t* output,
                                int stride,
                                TX_TYPE tx_type,
                                int bd);
#define av1_inv_txfm2d_add_16x64 av1_inv_txfm2d_add_16x64_c

void av1_inv_txfm2d_add_16x8_c(const int32_t* input,
                               uint16_t* output,
                               int stride,
                               TX_TYPE tx_type,
                               int bd);
#define av1_inv_txfm2d_add_16x8 av1_inv_txfm2d_add_16x8_c

void av1_inv_txfm2d_add_32x16_c(const int32_t* input,
                                uint16_t* output,
                                int stride,
                                TX_TYPE tx_type,
                                int bd);
#define av1_inv_txfm2d_add_32x16 av1_inv_txfm2d_add_32x16_c

void av1_inv_txfm2d_add_32x32_c(const int32_t* input,
                                uint16_t* output,
                                int stride,
                                TX_TYPE tx_type,
                                int bd);
#define av1_inv_txfm2d_add_32x32 av1_inv_txfm2d_add_32x32_c

void av1_inv_txfm2d_add_32x64_c(const int32_t* input,
                                uint16_t* output,
                                int stride,
                                TX_TYPE tx_type,
                                int bd);
#define av1_inv_txfm2d_add_32x64 av1_inv_txfm2d_add_32x64_c

void av1_inv_txfm2d_add_32x8_c(const int32_t* input,
                               uint16_t* output,
                               int stride,
                               TX_TYPE tx_type,
                               int bd);
#define av1_inv_txfm2d_add_32x8 av1_inv_txfm2d_add_32x8_c

void av1_inv_txfm2d_add_4x16_c(const int32_t* input,
                               uint16_t* output,
                               int stride,
                               TX_TYPE tx_type,
                               int bd);
#define av1_inv_txfm2d_add_4x16 av1_inv_txfm2d_add_4x16_c

void av1_inv_txfm2d_add_4x4_c(const int32_t* input,
                              uint16_t* output,
                              int stride,
                              TX_TYPE tx_type,
                              int bd);
#define av1_inv_txfm2d_add_4x4 av1_inv_txfm2d_add_4x4_c

void av1_inv_txfm2d_add_4x8_c(const int32_t* input,
                              uint16_t* output,
                              int stride,
                              TX_TYPE tx_type,
                              int bd);
#define av1_inv_txfm2d_add_4x8 av1_inv_txfm2d_add_4x8_c

void av1_inv_txfm2d_add_64x16_c(const int32_t* input,
                                uint16_t* output,
                                int stride,
                                TX_TYPE tx_type,
                                int bd);
#define av1_inv_txfm2d_add_64x16 av1_inv_txfm2d_add_64x16_c

void av1_inv_txfm2d_add_64x32_c(const int32_t* input,
                                uint16_t* output,
                                int stride,
                                TX_TYPE tx_type,
                                int bd);
#define av1_inv_txfm2d_add_64x32 av1_inv_txfm2d_add_64x32_c

void av1_inv_txfm2d_add_64x64_c(const int32_t* input,
                                uint16_t* output,
                                int stride,
                                TX_TYPE tx_type,
                                int bd);
#define av1_inv_txfm2d_add_64x64 av1_inv_txfm2d_add_64x64_c

void av1_inv_txfm2d_add_8x16_c(const int32_t* input,
                               uint16_t* output,
                               int stride,
                               TX_TYPE tx_type,
                               int bd);
#define av1_inv_txfm2d_add_8x16 av1_inv_txfm2d_add_8x16_c

void av1_inv_txfm2d_add_8x32_c(const int32_t* input,
                               uint16_t* output,
                               int stride,
                               TX_TYPE tx_type,
                               int bd);
#define av1_inv_txfm2d_add_8x32 av1_inv_txfm2d_add_8x32_c

void av1_inv_txfm2d_add_8x4_c(const int32_t* input,
                              uint16_t* output,
                              int stride,
                              TX_TYPE tx_type,
                              int bd);
#define av1_inv_txfm2d_add_8x4 av1_inv_txfm2d_add_8x4_c

void av1_inv_txfm2d_add_8x8_c(const int32_t* input,
                              uint16_t* output,
                              int stride,
                              TX_TYPE tx_type,
                              int bd);
#define av1_inv_txfm2d_add_8x8 av1_inv_txfm2d_add_8x8_c

void av1_inv_txfm_add_c(const tran_low_t* dqcoeff,
                        uint8_t* dst,
                        int stride,
                        const TxfmParam* txfm_param);
void av1_inv_txfm_add_neon(const tran_low_t* dqcoeff,
                           uint8_t* dst,
                           int stride,
                           const TxfmParam* txfm_param);
#define av1_inv_txfm_add av1_inv_txfm_add_neon

void av1_round_shift_array_c(int32_t* arr, int size, int bit);
void av1_round_shift_array_neon(int32_t* arr, int size, int bit);
#define av1_round_shift_array av1_round_shift_array_neon

int av1_selfguided_restoration_c(const uint8_t* dgd8,
                                 int width,
                                 int height,
                                 int dgd_stride,
                                 int32_t* flt0,
                                 int32_t* flt1,
                                 int flt_stride,
                                 int sgr_params_idx,
                                 int bit_depth,
                                 int highbd);
int av1_selfguided_restoration_neon(const uint8_t* dgd8,
                                    int width,
                                    int height,
                                    int dgd_stride,
                                    int32_t* flt0,
                                    int32_t* flt1,
                                    int flt_stride,
                                    int sgr_params_idx,
                                    int bit_depth,
                                    int highbd);
#define av1_selfguided_restoration av1_selfguided_restoration_neon

void av1_upsample_intra_edge_c(uint8_t* p, int sz);
#define av1_upsample_intra_edge av1_upsample_intra_edge_c

void av1_upsample_intra_edge_high_c(uint16_t* p, int sz, int bd);
#define av1_upsample_intra_edge_high av1_upsample_intra_edge_high_c

void av1_warp_affine_c(const int32_t* mat,
                       const uint8_t* ref,
                       int width,
                       int height,
                       int stride,
                       uint8_t* pred,
                       int p_col,
                       int p_row,
                       int p_width,
                       int p_height,
                       int p_stride,
                       int subsampling_x,
                       int subsampling_y,
                       ConvolveParams* conv_params,
                       int16_t alpha,
                       int16_t beta,
                       int16_t gamma,
                       int16_t delta);
void av1_warp_affine_neon(const int32_t* mat,
                          const uint8_t* ref,
                          int width,
                          int height,
                          int stride,
                          uint8_t* pred,
                          int p_col,
                          int p_row,
                          int p_width,
                          int p_height,
                          int p_stride,
                          int subsampling_x,
                          int subsampling_y,
                          ConvolveParams* conv_params,
                          int16_t alpha,
                          int16_t beta,
                          int16_t gamma,
                          int16_t delta);
#define av1_warp_affine av1_warp_affine_neon

void av1_wiener_convolve_add_src_c(const uint8_t* src,
                                   ptrdiff_t src_stride,
                                   uint8_t* dst,
                                   ptrdiff_t dst_stride,
                                   const int16_t* filter_x,
                                   int x_step_q4,
                                   const int16_t* filter_y,
                                   int y_step_q4,
                                   int w,
                                   int h,
                                   const ConvolveParams* conv_params);
void av1_wiener_convolve_add_src_neon(const uint8_t* src,
                                      ptrdiff_t src_stride,
                                      uint8_t* dst,
                                      ptrdiff_t dst_stride,
                                      const int16_t* filter_x,
                                      int x_step_q4,
                                      const int16_t* filter_y,
                                      int y_step_q4,
                                      int w,
                                      int h,
                                      const ConvolveParams* conv_params);
#define av1_wiener_convolve_add_src av1_wiener_convolve_add_src_neon

void cdef_filter_block_c(uint8_t* dst8,
                         uint16_t* dst16,
                         int dstride,
                         const uint16_t* in,
                         int pri_strength,
                         int sec_strength,
                         int dir,
                         int pri_damping,
                         int sec_damping,
                         int bsize,
                         int coeff_shift);
void cdef_filter_block_neon(uint8_t* dst8,
                            uint16_t* dst16,
                            int dstride,
                            const uint16_t* in,
                            int pri_strength,
                            int sec_strength,
                            int dir,
                            int pri_damping,
                            int sec_damping,
                            int bsize,
                            int coeff_shift);
#define cdef_filter_block cdef_filter_block_neon

int cdef_find_dir_c(const uint16_t* img,
                    int stride,
                    int32_t* var,
                    int coeff_shift);
int cdef_find_dir_neon(const uint16_t* img,
                       int stride,
                       int32_t* var,
                       int coeff_shift);
#define cdef_find_dir cdef_find_dir_neon

cfl_subsample_hbd_fn cfl_get_luma_subsampling_420_hbd_c(TX_SIZE tx_size);
cfl_subsample_hbd_fn cfl_get_luma_subsampling_420_hbd_neon(TX_SIZE tx_size);
#define cfl_get_luma_subsampling_420_hbd cfl_get_luma_subsampling_420_hbd_neon

cfl_subsample_lbd_fn cfl_get_luma_subsampling_420_lbd_c(TX_SIZE tx_size);
cfl_subsample_lbd_fn cfl_get_luma_subsampling_420_lbd_neon(TX_SIZE tx_size);
#define cfl_get_luma_subsampling_420_lbd cfl_get_luma_subsampling_420_lbd_neon

cfl_subsample_hbd_fn cfl_get_luma_subsampling_422_hbd_c(TX_SIZE tx_size);
cfl_subsample_hbd_fn cfl_get_luma_subsampling_422_hbd_neon(TX_SIZE tx_size);
#define cfl_get_luma_subsampling_422_hbd cfl_get_luma_subsampling_422_hbd_neon

cfl_subsample_lbd_fn cfl_get_luma_subsampling_422_lbd_c(TX_SIZE tx_size);
cfl_subsample_lbd_fn cfl_get_luma_subsampling_422_lbd_neon(TX_SIZE tx_size);
#define cfl_get_luma_subsampling_422_lbd cfl_get_luma_subsampling_422_lbd_neon

cfl_subsample_hbd_fn cfl_get_luma_subsampling_444_hbd_c(TX_SIZE tx_size);
cfl_subsample_hbd_fn cfl_get_luma_subsampling_444_hbd_neon(TX_SIZE tx_size);
#define cfl_get_luma_subsampling_444_hbd cfl_get_luma_subsampling_444_hbd_neon

cfl_subsample_lbd_fn cfl_get_luma_subsampling_444_lbd_c(TX_SIZE tx_size);
cfl_subsample_lbd_fn cfl_get_luma_subsampling_444_lbd_neon(TX_SIZE tx_size);
#define cfl_get_luma_subsampling_444_lbd cfl_get_luma_subsampling_444_lbd_neon

void copy_rect8_16bit_to_16bit_c(uint16_t* dst,
                                 int dstride,
                                 const uint16_t* src,
                                 int sstride,
                                 int v,
                                 int h);
void copy_rect8_16bit_to_16bit_neon(uint16_t* dst,
                                    int dstride,
                                    const uint16_t* src,
                                    int sstride,
                                    int v,
                                    int h);
#define copy_rect8_16bit_to_16bit copy_rect8_16bit_to_16bit_neon

void copy_rect8_8bit_to_16bit_c(uint16_t* dst,
                                int dstride,
                                const uint8_t* src,
                                int sstride,
                                int v,
                                int h);
void copy_rect8_8bit_to_16bit_neon(uint16_t* dst,
                                   int dstride,
                                   const uint8_t* src,
                                   int sstride,
                                   int v,
                                   int h);
#define copy_rect8_8bit_to_16bit copy_rect8_8bit_to_16bit_neon

cfl_predict_hbd_fn get_predict_hbd_fn_c(TX_SIZE tx_size);
cfl_predict_hbd_fn get_predict_hbd_fn_neon(TX_SIZE tx_size);
#define get_predict_hbd_fn get_predict_hbd_fn_neon

cfl_predict_lbd_fn get_predict_lbd_fn_c(TX_SIZE tx_size);
cfl_predict_lbd_fn get_predict_lbd_fn_neon(TX_SIZE tx_size);
#define get_predict_lbd_fn get_predict_lbd_fn_neon

cfl_subtract_average_fn get_subtract_average_fn_c(TX_SIZE tx_size);
cfl_subtract_average_fn get_subtract_average_fn_neon(TX_SIZE tx_size);
#define get_subtract_average_fn get_subtract_average_fn_neon

void av1_rtcd(void);

#include "config/aom_config.h"

#ifdef RTCD_C
#include "aom_ports/arm.h"
static void setup_rtcd_internal(void) {
  int flags = aom_arm_cpu_caps();

  (void)flags;
}
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif
