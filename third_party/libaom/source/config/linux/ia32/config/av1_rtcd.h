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

enum { NONE, RELU, SOFTSIGN, SIGMOID } UENUM1BYTE(ACTIVATION);
#if CONFIG_NN_V2
enum { SOFTMAX_CROSS_ENTROPY } UENUM1BYTE(LOSS);
struct NN_CONFIG_V2;
typedef struct NN_CONFIG_V2 NN_CONFIG_V2;
struct FC_LAYER;
typedef struct FC_LAYER FC_LAYER;
#endif  // CONFIG_NN_V2

struct CNN_CONFIG;
typedef struct CNN_CONFIG CNN_CONFIG;
struct CNN_LAYER_CONFIG;
typedef struct CNN_LAYER_CONFIG CNN_LAYER_CONFIG;
struct CNN_THREAD_DATA;
typedef struct CNN_THREAD_DATA CNN_THREAD_DATA;
struct CNN_BRANCH_CONFIG;
typedef struct CNN_BRANCH_CONFIG CNN_BRANCH_CONFIG;
struct CNN_MULTI_OUT;
typedef struct CNN_MULTI_OUT CNN_MULTI_OUT;

/* Function pointers return by CfL functions */
typedef void (*cfl_subsample_lbd_fn)(const uint8_t* input,
                                     int input_stride,
                                     uint16_t* output_q3);

#if CONFIG_AV1_HIGHBITDEPTH
typedef void (*cfl_subsample_hbd_fn)(const uint16_t* input,
                                     int input_stride,
                                     uint16_t* output_q3);

typedef void (*cfl_predict_hbd_fn)(const int16_t* src,
                                   uint16_t* dst,
                                   int dst_stride,
                                   int alpha_q3,
                                   int bd);
#endif

typedef void (*cfl_subtract_average_fn)(const uint16_t* src, int16_t* dst);

typedef void (*cfl_predict_lbd_fn)(const int16_t* src,
                                   uint8_t* dst,
                                   int dst_stride,
                                   int alpha_q3);

#ifdef __cplusplus
extern "C" {
#endif

void aom_quantize_b_helper_c(const tran_low_t* coeff_ptr,
                             intptr_t n_coeffs,
                             const int16_t* zbin_ptr,
                             const int16_t* round_ptr,
                             const int16_t* quant_ptr,
                             const int16_t* quant_shift_ptr,
                             tran_low_t* qcoeff_ptr,
                             tran_low_t* dqcoeff_ptr,
                             const int16_t* dequant_ptr,
                             uint16_t* eob_ptr,
                             const int16_t* scan,
                             const int16_t* iscan,
                             const qm_val_t* qm_ptr,
                             const qm_val_t* iqm_ptr,
                             const int log_scale);
#define aom_quantize_b_helper aom_quantize_b_helper_c

int64_t av1_block_error_c(const tran_low_t* coeff,
                          const tran_low_t* dqcoeff,
                          intptr_t block_size,
                          int64_t* ssz);
int64_t av1_block_error_sse2(const tran_low_t* coeff,
                             const tran_low_t* dqcoeff,
                             intptr_t block_size,
                             int64_t* ssz);
int64_t av1_block_error_avx2(const tran_low_t* coeff,
                             const tran_low_t* dqcoeff,
                             intptr_t block_size,
                             int64_t* ssz);
RTCD_EXTERN int64_t (*av1_block_error)(const tran_low_t* coeff,
                                       const tran_low_t* dqcoeff,
                                       intptr_t block_size,
                                       int64_t* ssz);

int64_t av1_block_error_lp_c(const int16_t* coeff,
                             const int16_t* dqcoeff,
                             intptr_t block_size);
int64_t av1_block_error_lp_avx2(const int16_t* coeff,
                                const int16_t* dqcoeff,
                                intptr_t block_size);
RTCD_EXTERN int64_t (*av1_block_error_lp)(const int16_t* coeff,
                                          const int16_t* dqcoeff,
                                          intptr_t block_size);

void av1_build_compound_diffwtd_mask_c(uint8_t* mask,
                                       DIFFWTD_MASK_TYPE mask_type,
                                       const uint8_t* src0,
                                       int src0_stride,
                                       const uint8_t* src1,
                                       int src1_stride,
                                       int h,
                                       int w);
void av1_build_compound_diffwtd_mask_sse4_1(uint8_t* mask,
                                            DIFFWTD_MASK_TYPE mask_type,
                                            const uint8_t* src0,
                                            int src0_stride,
                                            const uint8_t* src1,
                                            int src1_stride,
                                            int h,
                                            int w);
void av1_build_compound_diffwtd_mask_avx2(uint8_t* mask,
                                          DIFFWTD_MASK_TYPE mask_type,
                                          const uint8_t* src0,
                                          int src0_stride,
                                          const uint8_t* src1,
                                          int src1_stride,
                                          int h,
                                          int w);
RTCD_EXTERN void (*av1_build_compound_diffwtd_mask)(uint8_t* mask,
                                                    DIFFWTD_MASK_TYPE mask_type,
                                                    const uint8_t* src0,
                                                    int src0_stride,
                                                    const uint8_t* src1,
                                                    int src1_stride,
                                                    int h,
                                                    int w);

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
void av1_build_compound_diffwtd_mask_d16_sse4_1(uint8_t* mask,
                                                DIFFWTD_MASK_TYPE mask_type,
                                                const CONV_BUF_TYPE* src0,
                                                int src0_stride,
                                                const CONV_BUF_TYPE* src1,
                                                int src1_stride,
                                                int h,
                                                int w,
                                                ConvolveParams* conv_params,
                                                int bd);
void av1_build_compound_diffwtd_mask_d16_avx2(uint8_t* mask,
                                              DIFFWTD_MASK_TYPE mask_type,
                                              const CONV_BUF_TYPE* src0,
                                              int src0_stride,
                                              const CONV_BUF_TYPE* src1,
                                              int src1_stride,
                                              int h,
                                              int w,
                                              ConvolveParams* conv_params,
                                              int bd);
RTCD_EXTERN void (*av1_build_compound_diffwtd_mask_d16)(
    uint8_t* mask,
    DIFFWTD_MASK_TYPE mask_type,
    const CONV_BUF_TYPE* src0,
    int src0_stride,
    const CONV_BUF_TYPE* src1,
    int src1_stride,
    int h,
    int w,
    ConvolveParams* conv_params,
    int bd);

void av1_build_compound_diffwtd_mask_highbd_c(uint8_t* mask,
                                              DIFFWTD_MASK_TYPE mask_type,
                                              const uint8_t* src0,
                                              int src0_stride,
                                              const uint8_t* src1,
                                              int src1_stride,
                                              int h,
                                              int w,
                                              int bd);
void av1_build_compound_diffwtd_mask_highbd_ssse3(uint8_t* mask,
                                                  DIFFWTD_MASK_TYPE mask_type,
                                                  const uint8_t* src0,
                                                  int src0_stride,
                                                  const uint8_t* src1,
                                                  int src1_stride,
                                                  int h,
                                                  int w,
                                                  int bd);
void av1_build_compound_diffwtd_mask_highbd_avx2(uint8_t* mask,
                                                 DIFFWTD_MASK_TYPE mask_type,
                                                 const uint8_t* src0,
                                                 int src0_stride,
                                                 const uint8_t* src1,
                                                 int src1_stride,
                                                 int h,
                                                 int w,
                                                 int bd);
RTCD_EXTERN void (*av1_build_compound_diffwtd_mask_highbd)(
    uint8_t* mask,
    DIFFWTD_MASK_TYPE mask_type,
    const uint8_t* src0,
    int src0_stride,
    const uint8_t* src1,
    int src1_stride,
    int h,
    int w,
    int bd);

void av1_calc_indices_dim1_c(const int* data,
                             const int* centroids,
                             uint8_t* indices,
                             int n,
                             int k);
void av1_calc_indices_dim1_sse2(const int* data,
                                const int* centroids,
                                uint8_t* indices,
                                int n,
                                int k);
void av1_calc_indices_dim1_avx2(const int* data,
                                const int* centroids,
                                uint8_t* indices,
                                int n,
                                int k);
RTCD_EXTERN void (*av1_calc_indices_dim1)(const int* data,
                                          const int* centroids,
                                          uint8_t* indices,
                                          int n,
                                          int k);

void av1_calc_indices_dim2_c(const int* data,
                             const int* centroids,
                             uint8_t* indices,
                             int n,
                             int k);
void av1_calc_indices_dim2_avx2(const int* data,
                                const int* centroids,
                                uint8_t* indices,
                                int n,
                                int k);
RTCD_EXTERN void (*av1_calc_indices_dim2)(const int* data,
                                          const int* centroids,
                                          uint8_t* indices,
                                          int n,
                                          int k);

double av1_compute_cross_correlation_c(unsigned char* im1,
                                       int stride1,
                                       int x1,
                                       int y1,
                                       unsigned char* im2,
                                       int stride2,
                                       int x2,
                                       int y2);
double av1_compute_cross_correlation_sse4_1(unsigned char* im1,
                                            int stride1,
                                            int x1,
                                            int y1,
                                            unsigned char* im2,
                                            int stride2,
                                            int x2,
                                            int y2);
double av1_compute_cross_correlation_avx2(unsigned char* im1,
                                          int stride1,
                                          int x1,
                                          int y1,
                                          unsigned char* im2,
                                          int stride2,
                                          int x2,
                                          int y2);
RTCD_EXTERN double (*av1_compute_cross_correlation)(unsigned char* im1,
                                                    int stride1,
                                                    int x1,
                                                    int y1,
                                                    unsigned char* im2,
                                                    int stride2,
                                                    int x2,
                                                    int y2);

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
                             const int subpel_y_qn,
                             const int y_step_qn,
                             ConvolveParams* conv_params);
void av1_convolve_2d_scale_sse4_1(const uint8_t* src,
                                  int src_stride,
                                  uint8_t* dst,
                                  int dst_stride,
                                  int w,
                                  int h,
                                  const InterpFilterParams* filter_params_x,
                                  const InterpFilterParams* filter_params_y,
                                  const int subpel_x_qn,
                                  const int x_step_qn,
                                  const int subpel_y_qn,
                                  const int y_step_qn,
                                  ConvolveParams* conv_params);
RTCD_EXTERN void (*av1_convolve_2d_scale)(
    const uint8_t* src,
    int src_stride,
    uint8_t* dst,
    int dst_stride,
    int w,
    int h,
    const InterpFilterParams* filter_params_x,
    const InterpFilterParams* filter_params_y,
    const int subpel_x_qn,
    const int x_step_qn,
    const int subpel_y_qn,
    const int y_step_qn,
    ConvolveParams* conv_params);

void av1_convolve_2d_sr_c(const uint8_t* src,
                          int src_stride,
                          uint8_t* dst,
                          int dst_stride,
                          int w,
                          int h,
                          const InterpFilterParams* filter_params_x,
                          const InterpFilterParams* filter_params_y,
                          const int subpel_x_qn,
                          const int subpel_y_qn,
                          ConvolveParams* conv_params);
void av1_convolve_2d_sr_sse2(const uint8_t* src,
                             int src_stride,
                             uint8_t* dst,
                             int dst_stride,
                             int w,
                             int h,
                             const InterpFilterParams* filter_params_x,
                             const InterpFilterParams* filter_params_y,
                             const int subpel_x_qn,
                             const int subpel_y_qn,
                             ConvolveParams* conv_params);
void av1_convolve_2d_sr_avx2(const uint8_t* src,
                             int src_stride,
                             uint8_t* dst,
                             int dst_stride,
                             int w,
                             int h,
                             const InterpFilterParams* filter_params_x,
                             const InterpFilterParams* filter_params_y,
                             const int subpel_x_qn,
                             const int subpel_y_qn,
                             ConvolveParams* conv_params);
RTCD_EXTERN void (*av1_convolve_2d_sr)(
    const uint8_t* src,
    int src_stride,
    uint8_t* dst,
    int dst_stride,
    int w,
    int h,
    const InterpFilterParams* filter_params_x,
    const InterpFilterParams* filter_params_y,
    const int subpel_x_qn,
    const int subpel_y_qn,
    ConvolveParams* conv_params);

void av1_convolve_horiz_rs_c(const uint8_t* src,
                             int src_stride,
                             uint8_t* dst,
                             int dst_stride,
                             int w,
                             int h,
                             const int16_t* x_filters,
                             int x0_qn,
                             int x_step_qn);
void av1_convolve_horiz_rs_sse4_1(const uint8_t* src,
                                  int src_stride,
                                  uint8_t* dst,
                                  int dst_stride,
                                  int w,
                                  int h,
                                  const int16_t* x_filters,
                                  int x0_qn,
                                  int x_step_qn);
RTCD_EXTERN void (*av1_convolve_horiz_rs)(const uint8_t* src,
                                          int src_stride,
                                          uint8_t* dst,
                                          int dst_stride,
                                          int w,
                                          int h,
                                          const int16_t* x_filters,
                                          int x0_qn,
                                          int x_step_qn);

void av1_convolve_x_sr_c(const uint8_t* src,
                         int src_stride,
                         uint8_t* dst,
                         int dst_stride,
                         int w,
                         int h,
                         const InterpFilterParams* filter_params_x,
                         const int subpel_x_qn,
                         ConvolveParams* conv_params);
void av1_convolve_x_sr_sse2(const uint8_t* src,
                            int src_stride,
                            uint8_t* dst,
                            int dst_stride,
                            int w,
                            int h,
                            const InterpFilterParams* filter_params_x,
                            const int subpel_x_qn,
                            ConvolveParams* conv_params);
void av1_convolve_x_sr_avx2(const uint8_t* src,
                            int src_stride,
                            uint8_t* dst,
                            int dst_stride,
                            int w,
                            int h,
                            const InterpFilterParams* filter_params_x,
                            const int subpel_x_qn,
                            ConvolveParams* conv_params);
RTCD_EXTERN void (*av1_convolve_x_sr)(const uint8_t* src,
                                      int src_stride,
                                      uint8_t* dst,
                                      int dst_stride,
                                      int w,
                                      int h,
                                      const InterpFilterParams* filter_params_x,
                                      const int subpel_x_qn,
                                      ConvolveParams* conv_params);

void av1_convolve_y_sr_c(const uint8_t* src,
                         int src_stride,
                         uint8_t* dst,
                         int dst_stride,
                         int w,
                         int h,
                         const InterpFilterParams* filter_params_y,
                         const int subpel_y_qn);
void av1_convolve_y_sr_sse2(const uint8_t* src,
                            int src_stride,
                            uint8_t* dst,
                            int dst_stride,
                            int w,
                            int h,
                            const InterpFilterParams* filter_params_y,
                            const int subpel_y_qn);
void av1_convolve_y_sr_avx2(const uint8_t* src,
                            int src_stride,
                            uint8_t* dst,
                            int dst_stride,
                            int w,
                            int h,
                            const InterpFilterParams* filter_params_y,
                            const int subpel_y_qn);
RTCD_EXTERN void (*av1_convolve_y_sr)(const uint8_t* src,
                                      int src_stride,
                                      uint8_t* dst,
                                      int dst_stride,
                                      int w,
                                      int h,
                                      const InterpFilterParams* filter_params_y,
                                      const int subpel_y_qn);

int av1_denoiser_filter_c(const uint8_t* sig,
                          int sig_stride,
                          const uint8_t* mc_avg,
                          int mc_avg_stride,
                          uint8_t* avg,
                          int avg_stride,
                          int increase_denoising,
                          BLOCK_SIZE bs,
                          int motion_magnitude);
int av1_denoiser_filter_sse2(const uint8_t* sig,
                             int sig_stride,
                             const uint8_t* mc_avg,
                             int mc_avg_stride,
                             uint8_t* avg,
                             int avg_stride,
                             int increase_denoising,
                             BLOCK_SIZE bs,
                             int motion_magnitude);
#define av1_denoiser_filter av1_denoiser_filter_sse2

void av1_dist_wtd_convolve_2d_c(const uint8_t* src,
                                int src_stride,
                                uint8_t* dst,
                                int dst_stride,
                                int w,
                                int h,
                                const InterpFilterParams* filter_params_x,
                                const InterpFilterParams* filter_params_y,
                                const int subpel_x_qn,
                                const int subpel_y_qn,
                                ConvolveParams* conv_params);
void av1_dist_wtd_convolve_2d_sse2(const uint8_t* src,
                                   int src_stride,
                                   uint8_t* dst,
                                   int dst_stride,
                                   int w,
                                   int h,
                                   const InterpFilterParams* filter_params_x,
                                   const InterpFilterParams* filter_params_y,
                                   const int subpel_x_qn,
                                   const int subpel_y_qn,
                                   ConvolveParams* conv_params);
void av1_dist_wtd_convolve_2d_ssse3(const uint8_t* src,
                                    int src_stride,
                                    uint8_t* dst,
                                    int dst_stride,
                                    int w,
                                    int h,
                                    const InterpFilterParams* filter_params_x,
                                    const InterpFilterParams* filter_params_y,
                                    const int subpel_x_qn,
                                    const int subpel_y_qn,
                                    ConvolveParams* conv_params);
void av1_dist_wtd_convolve_2d_avx2(const uint8_t* src,
                                   int src_stride,
                                   uint8_t* dst,
                                   int dst_stride,
                                   int w,
                                   int h,
                                   const InterpFilterParams* filter_params_x,
                                   const InterpFilterParams* filter_params_y,
                                   const int subpel_x_qn,
                                   const int subpel_y_qn,
                                   ConvolveParams* conv_params);
RTCD_EXTERN void (*av1_dist_wtd_convolve_2d)(
    const uint8_t* src,
    int src_stride,
    uint8_t* dst,
    int dst_stride,
    int w,
    int h,
    const InterpFilterParams* filter_params_x,
    const InterpFilterParams* filter_params_y,
    const int subpel_x_qn,
    const int subpel_y_qn,
    ConvolveParams* conv_params);

void av1_dist_wtd_convolve_2d_copy_c(const uint8_t* src,
                                     int src_stride,
                                     uint8_t* dst,
                                     int dst_stride,
                                     int w,
                                     int h,
                                     ConvolveParams* conv_params);
void av1_dist_wtd_convolve_2d_copy_sse2(const uint8_t* src,
                                        int src_stride,
                                        uint8_t* dst,
                                        int dst_stride,
                                        int w,
                                        int h,
                                        ConvolveParams* conv_params);
void av1_dist_wtd_convolve_2d_copy_avx2(const uint8_t* src,
                                        int src_stride,
                                        uint8_t* dst,
                                        int dst_stride,
                                        int w,
                                        int h,
                                        ConvolveParams* conv_params);
RTCD_EXTERN void (*av1_dist_wtd_convolve_2d_copy)(const uint8_t* src,
                                                  int src_stride,
                                                  uint8_t* dst,
                                                  int dst_stride,
                                                  int w,
                                                  int h,
                                                  ConvolveParams* conv_params);

void av1_dist_wtd_convolve_x_c(const uint8_t* src,
                               int src_stride,
                               uint8_t* dst,
                               int dst_stride,
                               int w,
                               int h,
                               const InterpFilterParams* filter_params_x,
                               const int subpel_x_qn,
                               ConvolveParams* conv_params);
void av1_dist_wtd_convolve_x_sse2(const uint8_t* src,
                                  int src_stride,
                                  uint8_t* dst,
                                  int dst_stride,
                                  int w,
                                  int h,
                                  const InterpFilterParams* filter_params_x,
                                  const int subpel_x_qn,
                                  ConvolveParams* conv_params);
void av1_dist_wtd_convolve_x_avx2(const uint8_t* src,
                                  int src_stride,
                                  uint8_t* dst,
                                  int dst_stride,
                                  int w,
                                  int h,
                                  const InterpFilterParams* filter_params_x,
                                  const int subpel_x_qn,
                                  ConvolveParams* conv_params);
RTCD_EXTERN void (*av1_dist_wtd_convolve_x)(
    const uint8_t* src,
    int src_stride,
    uint8_t* dst,
    int dst_stride,
    int w,
    int h,
    const InterpFilterParams* filter_params_x,
    const int subpel_x_qn,
    ConvolveParams* conv_params);

void av1_dist_wtd_convolve_y_c(const uint8_t* src,
                               int src_stride,
                               uint8_t* dst,
                               int dst_stride,
                               int w,
                               int h,
                               const InterpFilterParams* filter_params_y,
                               const int subpel_y_qn,
                               ConvolveParams* conv_params);
void av1_dist_wtd_convolve_y_sse2(const uint8_t* src,
                                  int src_stride,
                                  uint8_t* dst,
                                  int dst_stride,
                                  int w,
                                  int h,
                                  const InterpFilterParams* filter_params_y,
                                  const int subpel_y_qn,
                                  ConvolveParams* conv_params);
void av1_dist_wtd_convolve_y_avx2(const uint8_t* src,
                                  int src_stride,
                                  uint8_t* dst,
                                  int dst_stride,
                                  int w,
                                  int h,
                                  const InterpFilterParams* filter_params_y,
                                  const int subpel_y_qn,
                                  ConvolveParams* conv_params);
RTCD_EXTERN void (*av1_dist_wtd_convolve_y)(
    const uint8_t* src,
    int src_stride,
    uint8_t* dst,
    int dst_stride,
    int w,
    int h,
    const InterpFilterParams* filter_params_y,
    const int subpel_y_qn,
    ConvolveParams* conv_params);

void av1_dr_prediction_z1_c(uint8_t* dst,
                            ptrdiff_t stride,
                            int bw,
                            int bh,
                            const uint8_t* above,
                            const uint8_t* left,
                            int upsample_above,
                            int dx,
                            int dy);
void av1_dr_prediction_z1_avx2(uint8_t* dst,
                               ptrdiff_t stride,
                               int bw,
                               int bh,
                               const uint8_t* above,
                               const uint8_t* left,
                               int upsample_above,
                               int dx,
                               int dy);
RTCD_EXTERN void (*av1_dr_prediction_z1)(uint8_t* dst,
                                         ptrdiff_t stride,
                                         int bw,
                                         int bh,
                                         const uint8_t* above,
                                         const uint8_t* left,
                                         int upsample_above,
                                         int dx,
                                         int dy);

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
void av1_dr_prediction_z2_avx2(uint8_t* dst,
                               ptrdiff_t stride,
                               int bw,
                               int bh,
                               const uint8_t* above,
                               const uint8_t* left,
                               int upsample_above,
                               int upsample_left,
                               int dx,
                               int dy);
RTCD_EXTERN void (*av1_dr_prediction_z2)(uint8_t* dst,
                                         ptrdiff_t stride,
                                         int bw,
                                         int bh,
                                         const uint8_t* above,
                                         const uint8_t* left,
                                         int upsample_above,
                                         int upsample_left,
                                         int dx,
                                         int dy);

void av1_dr_prediction_z3_c(uint8_t* dst,
                            ptrdiff_t stride,
                            int bw,
                            int bh,
                            const uint8_t* above,
                            const uint8_t* left,
                            int upsample_left,
                            int dx,
                            int dy);
void av1_dr_prediction_z3_avx2(uint8_t* dst,
                               ptrdiff_t stride,
                               int bw,
                               int bh,
                               const uint8_t* above,
                               const uint8_t* left,
                               int upsample_left,
                               int dx,
                               int dy);
RTCD_EXTERN void (*av1_dr_prediction_z3)(uint8_t* dst,
                                         ptrdiff_t stride,
                                         int bw,
                                         int bh,
                                         const uint8_t* above,
                                         const uint8_t* left,
                                         int upsample_left,
                                         int dx,
                                         int dy);

void av1_filter_intra_edge_c(uint8_t* p, int sz, int strength);
void av1_filter_intra_edge_sse4_1(uint8_t* p, int sz, int strength);
RTCD_EXTERN void (*av1_filter_intra_edge)(uint8_t* p, int sz, int strength);

void av1_filter_intra_edge_high_c(uint16_t* p, int sz, int strength);
void av1_filter_intra_edge_high_sse4_1(uint16_t* p, int sz, int strength);
RTCD_EXTERN void (*av1_filter_intra_edge_high)(uint16_t* p,
                                               int sz,
                                               int strength);

void av1_filter_intra_predictor_c(uint8_t* dst,
                                  ptrdiff_t stride,
                                  TX_SIZE tx_size,
                                  const uint8_t* above,
                                  const uint8_t* left,
                                  int mode);
void av1_filter_intra_predictor_sse4_1(uint8_t* dst,
                                       ptrdiff_t stride,
                                       TX_SIZE tx_size,
                                       const uint8_t* above,
                                       const uint8_t* left,
                                       int mode);
RTCD_EXTERN void (*av1_filter_intra_predictor)(uint8_t* dst,
                                               ptrdiff_t stride,
                                               TX_SIZE tx_size,
                                               const uint8_t* above,
                                               const uint8_t* left,
                                               int mode);

void av1_fwd_txfm2d_16x16_c(const int16_t* input,
                            int32_t* output,
                            int stride,
                            TX_TYPE tx_type,
                            int bd);
void av1_fwd_txfm2d_16x16_sse4_1(const int16_t* input,
                                 int32_t* output,
                                 int stride,
                                 TX_TYPE tx_type,
                                 int bd);
void av1_fwd_txfm2d_16x16_avx2(const int16_t* input,
                               int32_t* output,
                               int stride,
                               TX_TYPE tx_type,
                               int bd);
RTCD_EXTERN void (*av1_fwd_txfm2d_16x16)(const int16_t* input,
                                         int32_t* output,
                                         int stride,
                                         TX_TYPE tx_type,
                                         int bd);

void av1_fwd_txfm2d_16x32_c(const int16_t* input,
                            int32_t* output,
                            int stride,
                            TX_TYPE tx_type,
                            int bd);
void av1_fwd_txfm2d_16x32_sse4_1(const int16_t* input,
                                 int32_t* output,
                                 int stride,
                                 TX_TYPE tx_type,
                                 int bd);
RTCD_EXTERN void (*av1_fwd_txfm2d_16x32)(const int16_t* input,
                                         int32_t* output,
                                         int stride,
                                         TX_TYPE tx_type,
                                         int bd);

void av1_fwd_txfm2d_16x4_c(const int16_t* input,
                           int32_t* output,
                           int stride,
                           TX_TYPE tx_type,
                           int bd);
void av1_fwd_txfm2d_16x4_sse4_1(const int16_t* input,
                                int32_t* output,
                                int stride,
                                TX_TYPE tx_type,
                                int bd);
RTCD_EXTERN void (*av1_fwd_txfm2d_16x4)(const int16_t* input,
                                        int32_t* output,
                                        int stride,
                                        TX_TYPE tx_type,
                                        int bd);

void av1_fwd_txfm2d_16x8_c(const int16_t* input,
                           int32_t* output,
                           int stride,
                           TX_TYPE tx_type,
                           int bd);
void av1_fwd_txfm2d_16x8_sse4_1(const int16_t* input,
                                int32_t* output,
                                int stride,
                                TX_TYPE tx_type,
                                int bd);
void av1_fwd_txfm2d_16x8_avx2(const int16_t* input,
                              int32_t* output,
                              int stride,
                              TX_TYPE tx_type,
                              int bd);
RTCD_EXTERN void (*av1_fwd_txfm2d_16x8)(const int16_t* input,
                                        int32_t* output,
                                        int stride,
                                        TX_TYPE tx_type,
                                        int bd);

void av1_fwd_txfm2d_32x16_c(const int16_t* input,
                            int32_t* output,
                            int stride,
                            TX_TYPE tx_type,
                            int bd);
void av1_fwd_txfm2d_32x16_sse4_1(const int16_t* input,
                                 int32_t* output,
                                 int stride,
                                 TX_TYPE tx_type,
                                 int bd);
RTCD_EXTERN void (*av1_fwd_txfm2d_32x16)(const int16_t* input,
                                         int32_t* output,
                                         int stride,
                                         TX_TYPE tx_type,
                                         int bd);

void av1_fwd_txfm2d_32x32_c(const int16_t* input,
                            int32_t* output,
                            int stride,
                            TX_TYPE tx_type,
                            int bd);
void av1_fwd_txfm2d_32x32_sse4_1(const int16_t* input,
                                 int32_t* output,
                                 int stride,
                                 TX_TYPE tx_type,
                                 int bd);
void av1_fwd_txfm2d_32x32_avx2(const int16_t* input,
                               int32_t* output,
                               int stride,
                               TX_TYPE tx_type,
                               int bd);
RTCD_EXTERN void (*av1_fwd_txfm2d_32x32)(const int16_t* input,
                                         int32_t* output,
                                         int stride,
                                         TX_TYPE tx_type,
                                         int bd);

void av1_fwd_txfm2d_32x64_c(const int16_t* input,
                            int32_t* output,
                            int stride,
                            TX_TYPE tx_type,
                            int bd);
void av1_fwd_txfm2d_32x64_sse4_1(const int16_t* input,
                                 int32_t* output,
                                 int stride,
                                 TX_TYPE tx_type,
                                 int bd);
RTCD_EXTERN void (*av1_fwd_txfm2d_32x64)(const int16_t* input,
                                         int32_t* output,
                                         int stride,
                                         TX_TYPE tx_type,
                                         int bd);

void av1_fwd_txfm2d_4x4_c(const int16_t* input,
                          int32_t* output,
                          int stride,
                          TX_TYPE tx_type,
                          int bd);
void av1_fwd_txfm2d_4x4_sse4_1(const int16_t* input,
                               int32_t* output,
                               int stride,
                               TX_TYPE tx_type,
                               int bd);
RTCD_EXTERN void (*av1_fwd_txfm2d_4x4)(const int16_t* input,
                                       int32_t* output,
                                       int stride,
                                       TX_TYPE tx_type,
                                       int bd);

void av1_fwd_txfm2d_4x8_c(const int16_t* input,
                          int32_t* output,
                          int stride,
                          TX_TYPE tx_type,
                          int bd);
void av1_fwd_txfm2d_4x8_sse4_1(const int16_t* input,
                               int32_t* output,
                               int stride,
                               TX_TYPE tx_type,
                               int bd);
RTCD_EXTERN void (*av1_fwd_txfm2d_4x8)(const int16_t* input,
                                       int32_t* output,
                                       int stride,
                                       TX_TYPE tx_type,
                                       int bd);

void av1_fwd_txfm2d_64x32_c(const int16_t* input,
                            int32_t* output,
                            int stride,
                            TX_TYPE tx_type,
                            int bd);
void av1_fwd_txfm2d_64x32_sse4_1(const int16_t* input,
                                 int32_t* output,
                                 int stride,
                                 TX_TYPE tx_type,
                                 int bd);
RTCD_EXTERN void (*av1_fwd_txfm2d_64x32)(const int16_t* input,
                                         int32_t* output,
                                         int stride,
                                         TX_TYPE tx_type,
                                         int bd);

void av1_fwd_txfm2d_64x64_c(const int16_t* input,
                            int32_t* output,
                            int stride,
                            TX_TYPE tx_type,
                            int bd);
void av1_fwd_txfm2d_64x64_sse4_1(const int16_t* input,
                                 int32_t* output,
                                 int stride,
                                 TX_TYPE tx_type,
                                 int bd);
void av1_fwd_txfm2d_64x64_avx2(const int16_t* input,
                               int32_t* output,
                               int stride,
                               TX_TYPE tx_type,
                               int bd);
RTCD_EXTERN void (*av1_fwd_txfm2d_64x64)(const int16_t* input,
                                         int32_t* output,
                                         int stride,
                                         TX_TYPE tx_type,
                                         int bd);

void av1_fwd_txfm2d_8x16_c(const int16_t* input,
                           int32_t* output,
                           int stride,
                           TX_TYPE tx_type,
                           int bd);
void av1_fwd_txfm2d_8x16_sse4_1(const int16_t* input,
                                int32_t* output,
                                int stride,
                                TX_TYPE tx_type,
                                int bd);
void av1_fwd_txfm2d_8x16_avx2(const int16_t* input,
                              int32_t* output,
                              int stride,
                              TX_TYPE tx_type,
                              int bd);
RTCD_EXTERN void (*av1_fwd_txfm2d_8x16)(const int16_t* input,
                                        int32_t* output,
                                        int stride,
                                        TX_TYPE tx_type,
                                        int bd);

void av1_fwd_txfm2d_8x4_c(const int16_t* input,
                          int32_t* output,
                          int stride,
                          TX_TYPE tx_type,
                          int bd);
void av1_fwd_txfm2d_8x4_sse4_1(const int16_t* input,
                               int32_t* output,
                               int stride,
                               TX_TYPE tx_type,
                               int bd);
RTCD_EXTERN void (*av1_fwd_txfm2d_8x4)(const int16_t* input,
                                       int32_t* output,
                                       int stride,
                                       TX_TYPE tx_type,
                                       int bd);

void av1_fwd_txfm2d_8x8_c(const int16_t* input,
                          int32_t* output,
                          int stride,
                          TX_TYPE tx_type,
                          int bd);
void av1_fwd_txfm2d_8x8_sse4_1(const int16_t* input,
                               int32_t* output,
                               int stride,
                               TX_TYPE tx_type,
                               int bd);
void av1_fwd_txfm2d_8x8_avx2(const int16_t* input,
                             int32_t* output,
                             int stride,
                             TX_TYPE tx_type,
                             int bd);
RTCD_EXTERN void (*av1_fwd_txfm2d_8x8)(const int16_t* input,
                                       int32_t* output,
                                       int stride,
                                       TX_TYPE tx_type,
                                       int bd);

void av1_fwht4x4_c(const int16_t* input, tran_low_t* output, int stride);
#define av1_fwht4x4 av1_fwht4x4_c

uint32_t av1_get_crc32c_value_c(void* crc_calculator,
                                uint8_t* p,
                                size_t length);
uint32_t av1_get_crc32c_value_sse4_2(void* crc_calculator,
                                     uint8_t* p,
                                     size_t length);
RTCD_EXTERN uint32_t (*av1_get_crc32c_value)(void* crc_calculator,
                                             uint8_t* p,
                                             size_t length);

void av1_get_horver_correlation_full_c(const int16_t* diff,
                                       int stride,
                                       int w,
                                       int h,
                                       float* hcorr,
                                       float* vcorr);
void av1_get_horver_correlation_full_sse4_1(const int16_t* diff,
                                            int stride,
                                            int w,
                                            int h,
                                            float* hcorr,
                                            float* vcorr);
void av1_get_horver_correlation_full_avx2(const int16_t* diff,
                                          int stride,
                                          int w,
                                          int h,
                                          float* hcorr,
                                          float* vcorr);
RTCD_EXTERN void (*av1_get_horver_correlation_full)(const int16_t* diff,
                                                    int stride,
                                                    int w,
                                                    int h,
                                                    float* hcorr,
                                                    float* vcorr);

void av1_get_nz_map_contexts_c(const uint8_t* const levels,
                               const int16_t* const scan,
                               const uint16_t eob,
                               const TX_SIZE tx_size,
                               const TX_CLASS tx_class,
                               int8_t* const coeff_contexts);
void av1_get_nz_map_contexts_sse2(const uint8_t* const levels,
                                  const int16_t* const scan,
                                  const uint16_t eob,
                                  const TX_SIZE tx_size,
                                  const TX_CLASS tx_class,
                                  int8_t* const coeff_contexts);
#define av1_get_nz_map_contexts av1_get_nz_map_contexts_sse2

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

void av1_highbd_fwht4x4_c(const int16_t* input, tran_low_t* output, int stride);
#define av1_highbd_fwht4x4 av1_highbd_fwht4x4_c

void av1_highbd_inv_txfm_add_c(const tran_low_t* input,
                               uint8_t* dest,
                               int stride,
                               const TxfmParam* txfm_param);
void av1_highbd_inv_txfm_add_sse4_1(const tran_low_t* input,
                                    uint8_t* dest,
                                    int stride,
                                    const TxfmParam* txfm_param);
void av1_highbd_inv_txfm_add_avx2(const tran_low_t* input,
                                  uint8_t* dest,
                                  int stride,
                                  const TxfmParam* txfm_param);
RTCD_EXTERN void (*av1_highbd_inv_txfm_add)(const tran_low_t* input,
                                            uint8_t* dest,
                                            int stride,
                                            const TxfmParam* txfm_param);

void av1_highbd_inv_txfm_add_16x32_c(const tran_low_t* input,
                                     uint8_t* dest,
                                     int stride,
                                     const TxfmParam* txfm_param);
#define av1_highbd_inv_txfm_add_16x32 av1_highbd_inv_txfm_add_16x32_c

void av1_highbd_inv_txfm_add_16x4_c(const tran_low_t* input,
                                    uint8_t* dest,
                                    int stride,
                                    const TxfmParam* txfm_param);
void av1_highbd_inv_txfm_add_16x4_sse4_1(const tran_low_t* input,
                                         uint8_t* dest,
                                         int stride,
                                         const TxfmParam* txfm_param);
RTCD_EXTERN void (*av1_highbd_inv_txfm_add_16x4)(const tran_low_t* input,
                                                 uint8_t* dest,
                                                 int stride,
                                                 const TxfmParam* txfm_param);

void av1_highbd_inv_txfm_add_16x64_c(const tran_low_t* input,
                                     uint8_t* dest,
                                     int stride,
                                     const TxfmParam* txfm_param);
#define av1_highbd_inv_txfm_add_16x64 av1_highbd_inv_txfm_add_16x64_c

void av1_highbd_inv_txfm_add_16x8_c(const tran_low_t* input,
                                    uint8_t* dest,
                                    int stride,
                                    const TxfmParam* txfm_param);
#define av1_highbd_inv_txfm_add_16x8 av1_highbd_inv_txfm_add_16x8_c

void av1_highbd_inv_txfm_add_32x16_c(const tran_low_t* input,
                                     uint8_t* dest,
                                     int stride,
                                     const TxfmParam* txfm_param);
#define av1_highbd_inv_txfm_add_32x16 av1_highbd_inv_txfm_add_32x16_c

void av1_highbd_inv_txfm_add_32x32_c(const tran_low_t* input,
                                     uint8_t* dest,
                                     int stride,
                                     const TxfmParam* txfm_param);
#define av1_highbd_inv_txfm_add_32x32 av1_highbd_inv_txfm_add_32x32_c

void av1_highbd_inv_txfm_add_32x64_c(const tran_low_t* input,
                                     uint8_t* dest,
                                     int stride,
                                     const TxfmParam* txfm_param);
#define av1_highbd_inv_txfm_add_32x64 av1_highbd_inv_txfm_add_32x64_c

void av1_highbd_inv_txfm_add_32x8_c(const tran_low_t* input,
                                    uint8_t* dest,
                                    int stride,
                                    const TxfmParam* txfm_param);
#define av1_highbd_inv_txfm_add_32x8 av1_highbd_inv_txfm_add_32x8_c

void av1_highbd_inv_txfm_add_4x16_c(const tran_low_t* input,
                                    uint8_t* dest,
                                    int stride,
                                    const TxfmParam* txfm_param);
void av1_highbd_inv_txfm_add_4x16_sse4_1(const tran_low_t* input,
                                         uint8_t* dest,
                                         int stride,
                                         const TxfmParam* txfm_param);
RTCD_EXTERN void (*av1_highbd_inv_txfm_add_4x16)(const tran_low_t* input,
                                                 uint8_t* dest,
                                                 int stride,
                                                 const TxfmParam* txfm_param);

void av1_highbd_inv_txfm_add_4x4_c(const tran_low_t* input,
                                   uint8_t* dest,
                                   int stride,
                                   const TxfmParam* txfm_param);
void av1_highbd_inv_txfm_add_4x4_sse4_1(const tran_low_t* input,
                                        uint8_t* dest,
                                        int stride,
                                        const TxfmParam* txfm_param);
RTCD_EXTERN void (*av1_highbd_inv_txfm_add_4x4)(const tran_low_t* input,
                                                uint8_t* dest,
                                                int stride,
                                                const TxfmParam* txfm_param);

void av1_highbd_inv_txfm_add_4x8_c(const tran_low_t* input,
                                   uint8_t* dest,
                                   int stride,
                                   const TxfmParam* txfm_param);
void av1_highbd_inv_txfm_add_4x8_sse4_1(const tran_low_t* input,
                                        uint8_t* dest,
                                        int stride,
                                        const TxfmParam* txfm_param);
RTCD_EXTERN void (*av1_highbd_inv_txfm_add_4x8)(const tran_low_t* input,
                                                uint8_t* dest,
                                                int stride,
                                                const TxfmParam* txfm_param);

void av1_highbd_inv_txfm_add_64x16_c(const tran_low_t* input,
                                     uint8_t* dest,
                                     int stride,
                                     const TxfmParam* txfm_param);
#define av1_highbd_inv_txfm_add_64x16 av1_highbd_inv_txfm_add_64x16_c

void av1_highbd_inv_txfm_add_64x32_c(const tran_low_t* input,
                                     uint8_t* dest,
                                     int stride,
                                     const TxfmParam* txfm_param);
#define av1_highbd_inv_txfm_add_64x32 av1_highbd_inv_txfm_add_64x32_c

void av1_highbd_inv_txfm_add_64x64_c(const tran_low_t* input,
                                     uint8_t* dest,
                                     int stride,
                                     const TxfmParam* txfm_param);
#define av1_highbd_inv_txfm_add_64x64 av1_highbd_inv_txfm_add_64x64_c

void av1_highbd_inv_txfm_add_8x16_c(const tran_low_t* input,
                                    uint8_t* dest,
                                    int stride,
                                    const TxfmParam* txfm_param);
#define av1_highbd_inv_txfm_add_8x16 av1_highbd_inv_txfm_add_8x16_c

void av1_highbd_inv_txfm_add_8x32_c(const tran_low_t* input,
                                    uint8_t* dest,
                                    int stride,
                                    const TxfmParam* txfm_param);
#define av1_highbd_inv_txfm_add_8x32 av1_highbd_inv_txfm_add_8x32_c

void av1_highbd_inv_txfm_add_8x4_c(const tran_low_t* input,
                                   uint8_t* dest,
                                   int stride,
                                   const TxfmParam* txfm_param);
void av1_highbd_inv_txfm_add_8x4_sse4_1(const tran_low_t* input,
                                        uint8_t* dest,
                                        int stride,
                                        const TxfmParam* txfm_param);
RTCD_EXTERN void (*av1_highbd_inv_txfm_add_8x4)(const tran_low_t* input,
                                                uint8_t* dest,
                                                int stride,
                                                const TxfmParam* txfm_param);

void av1_highbd_inv_txfm_add_8x8_c(const tran_low_t* input,
                                   uint8_t* dest,
                                   int stride,
                                   const TxfmParam* txfm_param);
void av1_highbd_inv_txfm_add_8x8_sse4_1(const tran_low_t* input,
                                        uint8_t* dest,
                                        int stride,
                                        const TxfmParam* txfm_param);
RTCD_EXTERN void (*av1_highbd_inv_txfm_add_8x8)(const tran_low_t* input,
                                                uint8_t* dest,
                                                int stride,
                                                const TxfmParam* txfm_param);

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
void av1_inv_txfm2d_add_4x4_sse4_1(const int32_t* input,
                                   uint16_t* output,
                                   int stride,
                                   TX_TYPE tx_type,
                                   int bd);
RTCD_EXTERN void (*av1_inv_txfm2d_add_4x4)(const int32_t* input,
                                           uint16_t* output,
                                           int stride,
                                           TX_TYPE tx_type,
                                           int bd);

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
void av1_inv_txfm2d_add_8x8_sse4_1(const int32_t* input,
                                   uint16_t* output,
                                   int stride,
                                   TX_TYPE tx_type,
                                   int bd);
RTCD_EXTERN void (*av1_inv_txfm2d_add_8x8)(const int32_t* input,
                                           uint16_t* output,
                                           int stride,
                                           TX_TYPE tx_type,
                                           int bd);

void av1_inv_txfm_add_c(const tran_low_t* dqcoeff,
                        uint8_t* dst,
                        int stride,
                        const TxfmParam* txfm_param);
void av1_inv_txfm_add_ssse3(const tran_low_t* dqcoeff,
                            uint8_t* dst,
                            int stride,
                            const TxfmParam* txfm_param);
void av1_inv_txfm_add_avx2(const tran_low_t* dqcoeff,
                           uint8_t* dst,
                           int stride,
                           const TxfmParam* txfm_param);
RTCD_EXTERN void (*av1_inv_txfm_add)(const tran_low_t* dqcoeff,
                                     uint8_t* dst,
                                     int stride,
                                     const TxfmParam* txfm_param);

void av1_lowbd_fwd_txfm_c(const int16_t* src_diff,
                          tran_low_t* coeff,
                          int diff_stride,
                          TxfmParam* txfm_param);
void av1_lowbd_fwd_txfm_sse2(const int16_t* src_diff,
                             tran_low_t* coeff,
                             int diff_stride,
                             TxfmParam* txfm_param);
void av1_lowbd_fwd_txfm_sse4_1(const int16_t* src_diff,
                               tran_low_t* coeff,
                               int diff_stride,
                               TxfmParam* txfm_param);
void av1_lowbd_fwd_txfm_avx2(const int16_t* src_diff,
                             tran_low_t* coeff,
                             int diff_stride,
                             TxfmParam* txfm_param);
RTCD_EXTERN void (*av1_lowbd_fwd_txfm)(const int16_t* src_diff,
                                       tran_low_t* coeff,
                                       int diff_stride,
                                       TxfmParam* txfm_param);

void av1_nn_predict_c(const float* input_nodes,
                      const NN_CONFIG* const nn_config,
                      int reduce_prec,
                      float* const output);
void av1_nn_predict_sse3(const float* input_nodes,
                         const NN_CONFIG* const nn_config,
                         int reduce_prec,
                         float* const output);
RTCD_EXTERN void (*av1_nn_predict)(const float* input_nodes,
                                   const NN_CONFIG* const nn_config,
                                   int reduce_prec,
                                   float* const output);

void av1_quantize_b_c(const tran_low_t* coeff_ptr,
                      intptr_t n_coeffs,
                      const int16_t* zbin_ptr,
                      const int16_t* round_ptr,
                      const int16_t* quant_ptr,
                      const int16_t* quant_shift_ptr,
                      tran_low_t* qcoeff_ptr,
                      tran_low_t* dqcoeff_ptr,
                      const int16_t* dequant_ptr,
                      uint16_t* eob_ptr,
                      const int16_t* scan,
                      const int16_t* iscan,
                      const qm_val_t* qm_ptr,
                      const qm_val_t* iqm_ptr,
                      int log_scale);
#define av1_quantize_b av1_quantize_b_c

void av1_quantize_fp_c(const tran_low_t* coeff_ptr,
                       intptr_t n_coeffs,
                       const int16_t* zbin_ptr,
                       const int16_t* round_ptr,
                       const int16_t* quant_ptr,
                       const int16_t* quant_shift_ptr,
                       tran_low_t* qcoeff_ptr,
                       tran_low_t* dqcoeff_ptr,
                       const int16_t* dequant_ptr,
                       uint16_t* eob_ptr,
                       const int16_t* scan,
                       const int16_t* iscan);
void av1_quantize_fp_sse2(const tran_low_t* coeff_ptr,
                          intptr_t n_coeffs,
                          const int16_t* zbin_ptr,
                          const int16_t* round_ptr,
                          const int16_t* quant_ptr,
                          const int16_t* quant_shift_ptr,
                          tran_low_t* qcoeff_ptr,
                          tran_low_t* dqcoeff_ptr,
                          const int16_t* dequant_ptr,
                          uint16_t* eob_ptr,
                          const int16_t* scan,
                          const int16_t* iscan);
void av1_quantize_fp_avx2(const tran_low_t* coeff_ptr,
                          intptr_t n_coeffs,
                          const int16_t* zbin_ptr,
                          const int16_t* round_ptr,
                          const int16_t* quant_ptr,
                          const int16_t* quant_shift_ptr,
                          tran_low_t* qcoeff_ptr,
                          tran_low_t* dqcoeff_ptr,
                          const int16_t* dequant_ptr,
                          uint16_t* eob_ptr,
                          const int16_t* scan,
                          const int16_t* iscan);
RTCD_EXTERN void (*av1_quantize_fp)(const tran_low_t* coeff_ptr,
                                    intptr_t n_coeffs,
                                    const int16_t* zbin_ptr,
                                    const int16_t* round_ptr,
                                    const int16_t* quant_ptr,
                                    const int16_t* quant_shift_ptr,
                                    tran_low_t* qcoeff_ptr,
                                    tran_low_t* dqcoeff_ptr,
                                    const int16_t* dequant_ptr,
                                    uint16_t* eob_ptr,
                                    const int16_t* scan,
                                    const int16_t* iscan);

void av1_quantize_fp_32x32_c(const tran_low_t* coeff_ptr,
                             intptr_t n_coeffs,
                             const int16_t* zbin_ptr,
                             const int16_t* round_ptr,
                             const int16_t* quant_ptr,
                             const int16_t* quant_shift_ptr,
                             tran_low_t* qcoeff_ptr,
                             tran_low_t* dqcoeff_ptr,
                             const int16_t* dequant_ptr,
                             uint16_t* eob_ptr,
                             const int16_t* scan,
                             const int16_t* iscan);
void av1_quantize_fp_32x32_avx2(const tran_low_t* coeff_ptr,
                                intptr_t n_coeffs,
                                const int16_t* zbin_ptr,
                                const int16_t* round_ptr,
                                const int16_t* quant_ptr,
                                const int16_t* quant_shift_ptr,
                                tran_low_t* qcoeff_ptr,
                                tran_low_t* dqcoeff_ptr,
                                const int16_t* dequant_ptr,
                                uint16_t* eob_ptr,
                                const int16_t* scan,
                                const int16_t* iscan);
RTCD_EXTERN void (*av1_quantize_fp_32x32)(const tran_low_t* coeff_ptr,
                                          intptr_t n_coeffs,
                                          const int16_t* zbin_ptr,
                                          const int16_t* round_ptr,
                                          const int16_t* quant_ptr,
                                          const int16_t* quant_shift_ptr,
                                          tran_low_t* qcoeff_ptr,
                                          tran_low_t* dqcoeff_ptr,
                                          const int16_t* dequant_ptr,
                                          uint16_t* eob_ptr,
                                          const int16_t* scan,
                                          const int16_t* iscan);

void av1_quantize_fp_64x64_c(const tran_low_t* coeff_ptr,
                             intptr_t n_coeffs,
                             const int16_t* zbin_ptr,
                             const int16_t* round_ptr,
                             const int16_t* quant_ptr,
                             const int16_t* quant_shift_ptr,
                             tran_low_t* qcoeff_ptr,
                             tran_low_t* dqcoeff_ptr,
                             const int16_t* dequant_ptr,
                             uint16_t* eob_ptr,
                             const int16_t* scan,
                             const int16_t* iscan);
void av1_quantize_fp_64x64_avx2(const tran_low_t* coeff_ptr,
                                intptr_t n_coeffs,
                                const int16_t* zbin_ptr,
                                const int16_t* round_ptr,
                                const int16_t* quant_ptr,
                                const int16_t* quant_shift_ptr,
                                tran_low_t* qcoeff_ptr,
                                tran_low_t* dqcoeff_ptr,
                                const int16_t* dequant_ptr,
                                uint16_t* eob_ptr,
                                const int16_t* scan,
                                const int16_t* iscan);
RTCD_EXTERN void (*av1_quantize_fp_64x64)(const tran_low_t* coeff_ptr,
                                          intptr_t n_coeffs,
                                          const int16_t* zbin_ptr,
                                          const int16_t* round_ptr,
                                          const int16_t* quant_ptr,
                                          const int16_t* quant_shift_ptr,
                                          tran_low_t* qcoeff_ptr,
                                          tran_low_t* dqcoeff_ptr,
                                          const int16_t* dequant_ptr,
                                          uint16_t* eob_ptr,
                                          const int16_t* scan,
                                          const int16_t* iscan);

void av1_quantize_lp_c(const int16_t* coeff_ptr,
                       intptr_t n_coeffs,
                       const int16_t* round_ptr,
                       const int16_t* quant_ptr,
                       int16_t* qcoeff_ptr,
                       int16_t* dqcoeff_ptr,
                       const int16_t* dequant_ptr,
                       uint16_t* eob_ptr,
                       const int16_t* scan);
void av1_quantize_lp_avx2(const int16_t* coeff_ptr,
                          intptr_t n_coeffs,
                          const int16_t* round_ptr,
                          const int16_t* quant_ptr,
                          int16_t* qcoeff_ptr,
                          int16_t* dqcoeff_ptr,
                          const int16_t* dequant_ptr,
                          uint16_t* eob_ptr,
                          const int16_t* scan);
RTCD_EXTERN void (*av1_quantize_lp)(const int16_t* coeff_ptr,
                                    intptr_t n_coeffs,
                                    const int16_t* round_ptr,
                                    const int16_t* quant_ptr,
                                    int16_t* qcoeff_ptr,
                                    int16_t* dqcoeff_ptr,
                                    const int16_t* dequant_ptr,
                                    uint16_t* eob_ptr,
                                    const int16_t* scan);

void av1_resize_and_extend_frame_c(const YV12_BUFFER_CONFIG* src,
                                   YV12_BUFFER_CONFIG* dst,
                                   const InterpFilter filter,
                                   const int phase,
                                   const int num_planes);
void av1_resize_and_extend_frame_ssse3(const YV12_BUFFER_CONFIG* src,
                                       YV12_BUFFER_CONFIG* dst,
                                       const InterpFilter filter,
                                       const int phase,
                                       const int num_planes);
RTCD_EXTERN void (*av1_resize_and_extend_frame)(const YV12_BUFFER_CONFIG* src,
                                                YV12_BUFFER_CONFIG* dst,
                                                const InterpFilter filter,
                                                const int phase,
                                                const int num_planes);

void av1_round_shift_array_c(int32_t* arr, int size, int bit);
void av1_round_shift_array_sse4_1(int32_t* arr, int size, int bit);
RTCD_EXTERN void (*av1_round_shift_array)(int32_t* arr, int size, int bit);

void av1_txb_init_levels_c(const tran_low_t* const coeff,
                           const int width,
                           const int height,
                           uint8_t* const levels);
void av1_txb_init_levels_sse4_1(const tran_low_t* const coeff,
                                const int width,
                                const int height,
                                uint8_t* const levels);
void av1_txb_init_levels_avx2(const tran_low_t* const coeff,
                              const int width,
                              const int height,
                              uint8_t* const levels);
RTCD_EXTERN void (*av1_txb_init_levels)(const tran_low_t* const coeff,
                                        const int width,
                                        const int height,
                                        uint8_t* const levels);

void av1_upsample_intra_edge_c(uint8_t* p, int sz);
void av1_upsample_intra_edge_sse4_1(uint8_t* p, int sz);
RTCD_EXTERN void (*av1_upsample_intra_edge)(uint8_t* p, int sz);

void av1_upsample_intra_edge_high_c(uint16_t* p, int sz, int bd);
void av1_upsample_intra_edge_high_sse4_1(uint16_t* p, int sz, int bd);
RTCD_EXTERN void (*av1_upsample_intra_edge_high)(uint16_t* p, int sz, int bd);

void av1_wedge_compute_delta_squares_c(int16_t* d,
                                       const int16_t* a,
                                       const int16_t* b,
                                       int N);
void av1_wedge_compute_delta_squares_sse2(int16_t* d,
                                          const int16_t* a,
                                          const int16_t* b,
                                          int N);
void av1_wedge_compute_delta_squares_avx2(int16_t* d,
                                          const int16_t* a,
                                          const int16_t* b,
                                          int N);
RTCD_EXTERN void (*av1_wedge_compute_delta_squares)(int16_t* d,
                                                    const int16_t* a,
                                                    const int16_t* b,
                                                    int N);

int8_t av1_wedge_sign_from_residuals_c(const int16_t* ds,
                                       const uint8_t* m,
                                       int N,
                                       int64_t limit);
int8_t av1_wedge_sign_from_residuals_sse2(const int16_t* ds,
                                          const uint8_t* m,
                                          int N,
                                          int64_t limit);
int8_t av1_wedge_sign_from_residuals_avx2(const int16_t* ds,
                                          const uint8_t* m,
                                          int N,
                                          int64_t limit);
RTCD_EXTERN int8_t (*av1_wedge_sign_from_residuals)(const int16_t* ds,
                                                    const uint8_t* m,
                                                    int N,
                                                    int64_t limit);

uint64_t av1_wedge_sse_from_residuals_c(const int16_t* r1,
                                        const int16_t* d,
                                        const uint8_t* m,
                                        int N);
uint64_t av1_wedge_sse_from_residuals_sse2(const int16_t* r1,
                                           const int16_t* d,
                                           const uint8_t* m,
                                           int N);
uint64_t av1_wedge_sse_from_residuals_avx2(const int16_t* r1,
                                           const int16_t* d,
                                           const uint8_t* m,
                                           int N);
RTCD_EXTERN uint64_t (*av1_wedge_sse_from_residuals)(const int16_t* r1,
                                                     const int16_t* d,
                                                     const uint8_t* m,
                                                     int N);

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
void av1_wiener_convolve_add_src_sse2(const uint8_t* src,
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
void av1_wiener_convolve_add_src_avx2(const uint8_t* src,
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
RTCD_EXTERN void (*av1_wiener_convolve_add_src)(
    const uint8_t* src,
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

void cdef_copy_rect8_16bit_to_16bit_c(uint16_t* dst,
                                      int dstride,
                                      const uint16_t* src,
                                      int sstride,
                                      int v,
                                      int h);
void cdef_copy_rect8_16bit_to_16bit_sse2(uint16_t* dst,
                                         int dstride,
                                         const uint16_t* src,
                                         int sstride,
                                         int v,
                                         int h);
void cdef_copy_rect8_16bit_to_16bit_ssse3(uint16_t* dst,
                                          int dstride,
                                          const uint16_t* src,
                                          int sstride,
                                          int v,
                                          int h);
void cdef_copy_rect8_16bit_to_16bit_sse4_1(uint16_t* dst,
                                           int dstride,
                                           const uint16_t* src,
                                           int sstride,
                                           int v,
                                           int h);
void cdef_copy_rect8_16bit_to_16bit_avx2(uint16_t* dst,
                                         int dstride,
                                         const uint16_t* src,
                                         int sstride,
                                         int v,
                                         int h);
RTCD_EXTERN void (*cdef_copy_rect8_16bit_to_16bit)(uint16_t* dst,
                                                   int dstride,
                                                   const uint16_t* src,
                                                   int sstride,
                                                   int v,
                                                   int h);

void cdef_copy_rect8_8bit_to_16bit_c(uint16_t* dst,
                                     int dstride,
                                     const uint8_t* src,
                                     int sstride,
                                     int v,
                                     int h);
void cdef_copy_rect8_8bit_to_16bit_sse2(uint16_t* dst,
                                        int dstride,
                                        const uint8_t* src,
                                        int sstride,
                                        int v,
                                        int h);
void cdef_copy_rect8_8bit_to_16bit_ssse3(uint16_t* dst,
                                         int dstride,
                                         const uint8_t* src,
                                         int sstride,
                                         int v,
                                         int h);
void cdef_copy_rect8_8bit_to_16bit_sse4_1(uint16_t* dst,
                                          int dstride,
                                          const uint8_t* src,
                                          int sstride,
                                          int v,
                                          int h);
void cdef_copy_rect8_8bit_to_16bit_avx2(uint16_t* dst,
                                        int dstride,
                                        const uint8_t* src,
                                        int sstride,
                                        int v,
                                        int h);
RTCD_EXTERN void (*cdef_copy_rect8_8bit_to_16bit)(uint16_t* dst,
                                                  int dstride,
                                                  const uint8_t* src,
                                                  int sstride,
                                                  int v,
                                                  int h);

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
void cdef_filter_block_sse2(uint8_t* dst8,
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
void cdef_filter_block_ssse3(uint8_t* dst8,
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
void cdef_filter_block_sse4_1(uint8_t* dst8,
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
void cdef_filter_block_avx2(uint8_t* dst8,
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
RTCD_EXTERN void (*cdef_filter_block)(uint8_t* dst8,
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

int cdef_find_dir_c(const uint16_t* img,
                    int stride,
                    int32_t* var,
                    int coeff_shift);
int cdef_find_dir_sse2(const uint16_t* img,
                       int stride,
                       int32_t* var,
                       int coeff_shift);
int cdef_find_dir_ssse3(const uint16_t* img,
                        int stride,
                        int32_t* var,
                        int coeff_shift);
int cdef_find_dir_sse4_1(const uint16_t* img,
                         int stride,
                         int32_t* var,
                         int coeff_shift);
int cdef_find_dir_avx2(const uint16_t* img,
                       int stride,
                       int32_t* var,
                       int coeff_shift);
RTCD_EXTERN int (*cdef_find_dir)(const uint16_t* img,
                                 int stride,
                                 int32_t* var,
                                 int coeff_shift);

cfl_subsample_lbd_fn cfl_get_luma_subsampling_420_lbd_c(TX_SIZE tx_size);
cfl_subsample_lbd_fn cfl_get_luma_subsampling_420_lbd_ssse3(TX_SIZE tx_size);
cfl_subsample_lbd_fn cfl_get_luma_subsampling_420_lbd_avx2(TX_SIZE tx_size);
RTCD_EXTERN cfl_subsample_lbd_fn (*cfl_get_luma_subsampling_420_lbd)(
    TX_SIZE tx_size);

cfl_subsample_lbd_fn cfl_get_luma_subsampling_422_lbd_c(TX_SIZE tx_size);
cfl_subsample_lbd_fn cfl_get_luma_subsampling_422_lbd_ssse3(TX_SIZE tx_size);
cfl_subsample_lbd_fn cfl_get_luma_subsampling_422_lbd_avx2(TX_SIZE tx_size);
RTCD_EXTERN cfl_subsample_lbd_fn (*cfl_get_luma_subsampling_422_lbd)(
    TX_SIZE tx_size);

cfl_subsample_lbd_fn cfl_get_luma_subsampling_444_lbd_c(TX_SIZE tx_size);
cfl_subsample_lbd_fn cfl_get_luma_subsampling_444_lbd_ssse3(TX_SIZE tx_size);
cfl_subsample_lbd_fn cfl_get_luma_subsampling_444_lbd_avx2(TX_SIZE tx_size);
RTCD_EXTERN cfl_subsample_lbd_fn (*cfl_get_luma_subsampling_444_lbd)(
    TX_SIZE tx_size);

cfl_predict_lbd_fn cfl_get_predict_lbd_fn_c(TX_SIZE tx_size);
cfl_predict_lbd_fn cfl_get_predict_lbd_fn_ssse3(TX_SIZE tx_size);
cfl_predict_lbd_fn cfl_get_predict_lbd_fn_avx2(TX_SIZE tx_size);
RTCD_EXTERN cfl_predict_lbd_fn (*cfl_get_predict_lbd_fn)(TX_SIZE tx_size);

cfl_subtract_average_fn cfl_get_subtract_average_fn_c(TX_SIZE tx_size);
cfl_subtract_average_fn cfl_get_subtract_average_fn_sse2(TX_SIZE tx_size);
cfl_subtract_average_fn cfl_get_subtract_average_fn_avx2(TX_SIZE tx_size);
RTCD_EXTERN cfl_subtract_average_fn (*cfl_get_subtract_average_fn)(
    TX_SIZE tx_size);

void av1_rtcd(void);

#ifdef RTCD_C
#include "aom_ports/x86.h"
static void setup_rtcd_internal(void) {
  int flags = x86_simd_caps();

  (void)flags;

  av1_block_error = av1_block_error_sse2;
  if (flags & HAS_AVX2)
    av1_block_error = av1_block_error_avx2;
  av1_block_error_lp = av1_block_error_lp_c;
  if (flags & HAS_AVX2)
    av1_block_error_lp = av1_block_error_lp_avx2;
  av1_build_compound_diffwtd_mask = av1_build_compound_diffwtd_mask_c;
  if (flags & HAS_SSE4_1)
    av1_build_compound_diffwtd_mask = av1_build_compound_diffwtd_mask_sse4_1;
  if (flags & HAS_AVX2)
    av1_build_compound_diffwtd_mask = av1_build_compound_diffwtd_mask_avx2;
  av1_build_compound_diffwtd_mask_d16 = av1_build_compound_diffwtd_mask_d16_c;
  if (flags & HAS_SSE4_1)
    av1_build_compound_diffwtd_mask_d16 =
        av1_build_compound_diffwtd_mask_d16_sse4_1;
  if (flags & HAS_AVX2)
    av1_build_compound_diffwtd_mask_d16 =
        av1_build_compound_diffwtd_mask_d16_avx2;
  av1_build_compound_diffwtd_mask_highbd =
      av1_build_compound_diffwtd_mask_highbd_c;
  if (flags & HAS_SSSE3)
    av1_build_compound_diffwtd_mask_highbd =
        av1_build_compound_diffwtd_mask_highbd_ssse3;
  if (flags & HAS_AVX2)
    av1_build_compound_diffwtd_mask_highbd =
        av1_build_compound_diffwtd_mask_highbd_avx2;
  av1_calc_indices_dim1 = av1_calc_indices_dim1_sse2;
  if (flags & HAS_AVX2)
    av1_calc_indices_dim1 = av1_calc_indices_dim1_avx2;
  av1_calc_indices_dim2 = av1_calc_indices_dim2_c;
  if (flags & HAS_AVX2)
    av1_calc_indices_dim2 = av1_calc_indices_dim2_avx2;
  av1_compute_cross_correlation = av1_compute_cross_correlation_c;
  if (flags & HAS_SSE4_1)
    av1_compute_cross_correlation = av1_compute_cross_correlation_sse4_1;
  if (flags & HAS_AVX2)
    av1_compute_cross_correlation = av1_compute_cross_correlation_avx2;
  av1_convolve_2d_scale = av1_convolve_2d_scale_c;
  if (flags & HAS_SSE4_1)
    av1_convolve_2d_scale = av1_convolve_2d_scale_sse4_1;
  av1_convolve_2d_sr = av1_convolve_2d_sr_sse2;
  if (flags & HAS_AVX2)
    av1_convolve_2d_sr = av1_convolve_2d_sr_avx2;
  av1_convolve_horiz_rs = av1_convolve_horiz_rs_c;
  if (flags & HAS_SSE4_1)
    av1_convolve_horiz_rs = av1_convolve_horiz_rs_sse4_1;
  av1_convolve_x_sr = av1_convolve_x_sr_sse2;
  if (flags & HAS_AVX2)
    av1_convolve_x_sr = av1_convolve_x_sr_avx2;
  av1_convolve_y_sr = av1_convolve_y_sr_sse2;
  if (flags & HAS_AVX2)
    av1_convolve_y_sr = av1_convolve_y_sr_avx2;
  av1_dist_wtd_convolve_2d = av1_dist_wtd_convolve_2d_sse2;
  if (flags & HAS_SSSE3)
    av1_dist_wtd_convolve_2d = av1_dist_wtd_convolve_2d_ssse3;
  if (flags & HAS_AVX2)
    av1_dist_wtd_convolve_2d = av1_dist_wtd_convolve_2d_avx2;
  av1_dist_wtd_convolve_2d_copy = av1_dist_wtd_convolve_2d_copy_sse2;
  if (flags & HAS_AVX2)
    av1_dist_wtd_convolve_2d_copy = av1_dist_wtd_convolve_2d_copy_avx2;
  av1_dist_wtd_convolve_x = av1_dist_wtd_convolve_x_sse2;
  if (flags & HAS_AVX2)
    av1_dist_wtd_convolve_x = av1_dist_wtd_convolve_x_avx2;
  av1_dist_wtd_convolve_y = av1_dist_wtd_convolve_y_sse2;
  if (flags & HAS_AVX2)
    av1_dist_wtd_convolve_y = av1_dist_wtd_convolve_y_avx2;
  av1_dr_prediction_z1 = av1_dr_prediction_z1_c;
  if (flags & HAS_AVX2)
    av1_dr_prediction_z1 = av1_dr_prediction_z1_avx2;
  av1_dr_prediction_z2 = av1_dr_prediction_z2_c;
  if (flags & HAS_AVX2)
    av1_dr_prediction_z2 = av1_dr_prediction_z2_avx2;
  av1_dr_prediction_z3 = av1_dr_prediction_z3_c;
  if (flags & HAS_AVX2)
    av1_dr_prediction_z3 = av1_dr_prediction_z3_avx2;
  av1_filter_intra_edge = av1_filter_intra_edge_c;
  if (flags & HAS_SSE4_1)
    av1_filter_intra_edge = av1_filter_intra_edge_sse4_1;
  av1_filter_intra_edge_high = av1_filter_intra_edge_high_c;
  if (flags & HAS_SSE4_1)
    av1_filter_intra_edge_high = av1_filter_intra_edge_high_sse4_1;
  av1_filter_intra_predictor = av1_filter_intra_predictor_c;
  if (flags & HAS_SSE4_1)
    av1_filter_intra_predictor = av1_filter_intra_predictor_sse4_1;
  av1_fwd_txfm2d_16x16 = av1_fwd_txfm2d_16x16_c;
  if (flags & HAS_SSE4_1)
    av1_fwd_txfm2d_16x16 = av1_fwd_txfm2d_16x16_sse4_1;
  if (flags & HAS_AVX2)
    av1_fwd_txfm2d_16x16 = av1_fwd_txfm2d_16x16_avx2;
  av1_fwd_txfm2d_16x32 = av1_fwd_txfm2d_16x32_c;
  if (flags & HAS_SSE4_1)
    av1_fwd_txfm2d_16x32 = av1_fwd_txfm2d_16x32_sse4_1;
  av1_fwd_txfm2d_16x4 = av1_fwd_txfm2d_16x4_c;
  if (flags & HAS_SSE4_1)
    av1_fwd_txfm2d_16x4 = av1_fwd_txfm2d_16x4_sse4_1;
  av1_fwd_txfm2d_16x8 = av1_fwd_txfm2d_16x8_c;
  if (flags & HAS_SSE4_1)
    av1_fwd_txfm2d_16x8 = av1_fwd_txfm2d_16x8_sse4_1;
  if (flags & HAS_AVX2)
    av1_fwd_txfm2d_16x8 = av1_fwd_txfm2d_16x8_avx2;
  av1_fwd_txfm2d_32x16 = av1_fwd_txfm2d_32x16_c;
  if (flags & HAS_SSE4_1)
    av1_fwd_txfm2d_32x16 = av1_fwd_txfm2d_32x16_sse4_1;
  av1_fwd_txfm2d_32x32 = av1_fwd_txfm2d_32x32_c;
  if (flags & HAS_SSE4_1)
    av1_fwd_txfm2d_32x32 = av1_fwd_txfm2d_32x32_sse4_1;
  if (flags & HAS_AVX2)
    av1_fwd_txfm2d_32x32 = av1_fwd_txfm2d_32x32_avx2;
  av1_fwd_txfm2d_32x64 = av1_fwd_txfm2d_32x64_c;
  if (flags & HAS_SSE4_1)
    av1_fwd_txfm2d_32x64 = av1_fwd_txfm2d_32x64_sse4_1;
  av1_fwd_txfm2d_4x4 = av1_fwd_txfm2d_4x4_c;
  if (flags & HAS_SSE4_1)
    av1_fwd_txfm2d_4x4 = av1_fwd_txfm2d_4x4_sse4_1;
  av1_fwd_txfm2d_4x8 = av1_fwd_txfm2d_4x8_c;
  if (flags & HAS_SSE4_1)
    av1_fwd_txfm2d_4x8 = av1_fwd_txfm2d_4x8_sse4_1;
  av1_fwd_txfm2d_64x32 = av1_fwd_txfm2d_64x32_c;
  if (flags & HAS_SSE4_1)
    av1_fwd_txfm2d_64x32 = av1_fwd_txfm2d_64x32_sse4_1;
  av1_fwd_txfm2d_64x64 = av1_fwd_txfm2d_64x64_c;
  if (flags & HAS_SSE4_1)
    av1_fwd_txfm2d_64x64 = av1_fwd_txfm2d_64x64_sse4_1;
  if (flags & HAS_AVX2)
    av1_fwd_txfm2d_64x64 = av1_fwd_txfm2d_64x64_avx2;
  av1_fwd_txfm2d_8x16 = av1_fwd_txfm2d_8x16_c;
  if (flags & HAS_SSE4_1)
    av1_fwd_txfm2d_8x16 = av1_fwd_txfm2d_8x16_sse4_1;
  if (flags & HAS_AVX2)
    av1_fwd_txfm2d_8x16 = av1_fwd_txfm2d_8x16_avx2;
  av1_fwd_txfm2d_8x4 = av1_fwd_txfm2d_8x4_c;
  if (flags & HAS_SSE4_1)
    av1_fwd_txfm2d_8x4 = av1_fwd_txfm2d_8x4_sse4_1;
  av1_fwd_txfm2d_8x8 = av1_fwd_txfm2d_8x8_c;
  if (flags & HAS_SSE4_1)
    av1_fwd_txfm2d_8x8 = av1_fwd_txfm2d_8x8_sse4_1;
  if (flags & HAS_AVX2)
    av1_fwd_txfm2d_8x8 = av1_fwd_txfm2d_8x8_avx2;
  av1_get_crc32c_value = av1_get_crc32c_value_c;
  if (flags & HAS_SSE4_2)
    av1_get_crc32c_value = av1_get_crc32c_value_sse4_2;
  av1_get_horver_correlation_full = av1_get_horver_correlation_full_c;
  if (flags & HAS_SSE4_1)
    av1_get_horver_correlation_full = av1_get_horver_correlation_full_sse4_1;
  if (flags & HAS_AVX2)
    av1_get_horver_correlation_full = av1_get_horver_correlation_full_avx2;
  av1_highbd_inv_txfm_add = av1_highbd_inv_txfm_add_c;
  if (flags & HAS_SSE4_1)
    av1_highbd_inv_txfm_add = av1_highbd_inv_txfm_add_sse4_1;
  if (flags & HAS_AVX2)
    av1_highbd_inv_txfm_add = av1_highbd_inv_txfm_add_avx2;
  av1_highbd_inv_txfm_add_16x4 = av1_highbd_inv_txfm_add_16x4_c;
  if (flags & HAS_SSE4_1)
    av1_highbd_inv_txfm_add_16x4 = av1_highbd_inv_txfm_add_16x4_sse4_1;
  av1_highbd_inv_txfm_add_4x16 = av1_highbd_inv_txfm_add_4x16_c;
  if (flags & HAS_SSE4_1)
    av1_highbd_inv_txfm_add_4x16 = av1_highbd_inv_txfm_add_4x16_sse4_1;
  av1_highbd_inv_txfm_add_4x4 = av1_highbd_inv_txfm_add_4x4_c;
  if (flags & HAS_SSE4_1)
    av1_highbd_inv_txfm_add_4x4 = av1_highbd_inv_txfm_add_4x4_sse4_1;
  av1_highbd_inv_txfm_add_4x8 = av1_highbd_inv_txfm_add_4x8_c;
  if (flags & HAS_SSE4_1)
    av1_highbd_inv_txfm_add_4x8 = av1_highbd_inv_txfm_add_4x8_sse4_1;
  av1_highbd_inv_txfm_add_8x4 = av1_highbd_inv_txfm_add_8x4_c;
  if (flags & HAS_SSE4_1)
    av1_highbd_inv_txfm_add_8x4 = av1_highbd_inv_txfm_add_8x4_sse4_1;
  av1_highbd_inv_txfm_add_8x8 = av1_highbd_inv_txfm_add_8x8_c;
  if (flags & HAS_SSE4_1)
    av1_highbd_inv_txfm_add_8x8 = av1_highbd_inv_txfm_add_8x8_sse4_1;
  av1_inv_txfm2d_add_4x4 = av1_inv_txfm2d_add_4x4_c;
  if (flags & HAS_SSE4_1)
    av1_inv_txfm2d_add_4x4 = av1_inv_txfm2d_add_4x4_sse4_1;
  av1_inv_txfm2d_add_8x8 = av1_inv_txfm2d_add_8x8_c;
  if (flags & HAS_SSE4_1)
    av1_inv_txfm2d_add_8x8 = av1_inv_txfm2d_add_8x8_sse4_1;
  av1_inv_txfm_add = av1_inv_txfm_add_c;
  if (flags & HAS_SSSE3)
    av1_inv_txfm_add = av1_inv_txfm_add_ssse3;
  if (flags & HAS_AVX2)
    av1_inv_txfm_add = av1_inv_txfm_add_avx2;
  av1_lowbd_fwd_txfm = av1_lowbd_fwd_txfm_sse2;
  if (flags & HAS_SSE4_1)
    av1_lowbd_fwd_txfm = av1_lowbd_fwd_txfm_sse4_1;
  if (flags & HAS_AVX2)
    av1_lowbd_fwd_txfm = av1_lowbd_fwd_txfm_avx2;
  av1_nn_predict = av1_nn_predict_c;
  if (flags & HAS_SSE3)
    av1_nn_predict = av1_nn_predict_sse3;
  av1_quantize_fp = av1_quantize_fp_sse2;
  if (flags & HAS_AVX2)
    av1_quantize_fp = av1_quantize_fp_avx2;
  av1_quantize_fp_32x32 = av1_quantize_fp_32x32_c;
  if (flags & HAS_AVX2)
    av1_quantize_fp_32x32 = av1_quantize_fp_32x32_avx2;
  av1_quantize_fp_64x64 = av1_quantize_fp_64x64_c;
  if (flags & HAS_AVX2)
    av1_quantize_fp_64x64 = av1_quantize_fp_64x64_avx2;
  av1_quantize_lp = av1_quantize_lp_c;
  if (flags & HAS_AVX2)
    av1_quantize_lp = av1_quantize_lp_avx2;
  av1_resize_and_extend_frame = av1_resize_and_extend_frame_c;
  if (flags & HAS_SSSE3)
    av1_resize_and_extend_frame = av1_resize_and_extend_frame_ssse3;
  av1_round_shift_array = av1_round_shift_array_c;
  if (flags & HAS_SSE4_1)
    av1_round_shift_array = av1_round_shift_array_sse4_1;
  av1_txb_init_levels = av1_txb_init_levels_c;
  if (flags & HAS_SSE4_1)
    av1_txb_init_levels = av1_txb_init_levels_sse4_1;
  if (flags & HAS_AVX2)
    av1_txb_init_levels = av1_txb_init_levels_avx2;
  av1_upsample_intra_edge = av1_upsample_intra_edge_c;
  if (flags & HAS_SSE4_1)
    av1_upsample_intra_edge = av1_upsample_intra_edge_sse4_1;
  av1_upsample_intra_edge_high = av1_upsample_intra_edge_high_c;
  if (flags & HAS_SSE4_1)
    av1_upsample_intra_edge_high = av1_upsample_intra_edge_high_sse4_1;
  av1_wedge_compute_delta_squares = av1_wedge_compute_delta_squares_sse2;
  if (flags & HAS_AVX2)
    av1_wedge_compute_delta_squares = av1_wedge_compute_delta_squares_avx2;
  av1_wedge_sign_from_residuals = av1_wedge_sign_from_residuals_sse2;
  if (flags & HAS_AVX2)
    av1_wedge_sign_from_residuals = av1_wedge_sign_from_residuals_avx2;
  av1_wedge_sse_from_residuals = av1_wedge_sse_from_residuals_sse2;
  if (flags & HAS_AVX2)
    av1_wedge_sse_from_residuals = av1_wedge_sse_from_residuals_avx2;
  av1_wiener_convolve_add_src = av1_wiener_convolve_add_src_sse2;
  if (flags & HAS_AVX2)
    av1_wiener_convolve_add_src = av1_wiener_convolve_add_src_avx2;
  cdef_copy_rect8_16bit_to_16bit = cdef_copy_rect8_16bit_to_16bit_sse2;
  if (flags & HAS_SSSE3)
    cdef_copy_rect8_16bit_to_16bit = cdef_copy_rect8_16bit_to_16bit_ssse3;
  if (flags & HAS_SSE4_1)
    cdef_copy_rect8_16bit_to_16bit = cdef_copy_rect8_16bit_to_16bit_sse4_1;
  if (flags & HAS_AVX2)
    cdef_copy_rect8_16bit_to_16bit = cdef_copy_rect8_16bit_to_16bit_avx2;
  cdef_copy_rect8_8bit_to_16bit = cdef_copy_rect8_8bit_to_16bit_sse2;
  if (flags & HAS_SSSE3)
    cdef_copy_rect8_8bit_to_16bit = cdef_copy_rect8_8bit_to_16bit_ssse3;
  if (flags & HAS_SSE4_1)
    cdef_copy_rect8_8bit_to_16bit = cdef_copy_rect8_8bit_to_16bit_sse4_1;
  if (flags & HAS_AVX2)
    cdef_copy_rect8_8bit_to_16bit = cdef_copy_rect8_8bit_to_16bit_avx2;
  cdef_filter_block = cdef_filter_block_sse2;
  if (flags & HAS_SSSE3)
    cdef_filter_block = cdef_filter_block_ssse3;
  if (flags & HAS_SSE4_1)
    cdef_filter_block = cdef_filter_block_sse4_1;
  if (flags & HAS_AVX2)
    cdef_filter_block = cdef_filter_block_avx2;
  cdef_find_dir = cdef_find_dir_sse2;
  if (flags & HAS_SSSE3)
    cdef_find_dir = cdef_find_dir_ssse3;
  if (flags & HAS_SSE4_1)
    cdef_find_dir = cdef_find_dir_sse4_1;
  if (flags & HAS_AVX2)
    cdef_find_dir = cdef_find_dir_avx2;
  cfl_get_luma_subsampling_420_lbd = cfl_get_luma_subsampling_420_lbd_c;
  if (flags & HAS_SSSE3)
    cfl_get_luma_subsampling_420_lbd = cfl_get_luma_subsampling_420_lbd_ssse3;
  if (flags & HAS_AVX2)
    cfl_get_luma_subsampling_420_lbd = cfl_get_luma_subsampling_420_lbd_avx2;
  cfl_get_luma_subsampling_422_lbd = cfl_get_luma_subsampling_422_lbd_c;
  if (flags & HAS_SSSE3)
    cfl_get_luma_subsampling_422_lbd = cfl_get_luma_subsampling_422_lbd_ssse3;
  if (flags & HAS_AVX2)
    cfl_get_luma_subsampling_422_lbd = cfl_get_luma_subsampling_422_lbd_avx2;
  cfl_get_luma_subsampling_444_lbd = cfl_get_luma_subsampling_444_lbd_c;
  if (flags & HAS_SSSE3)
    cfl_get_luma_subsampling_444_lbd = cfl_get_luma_subsampling_444_lbd_ssse3;
  if (flags & HAS_AVX2)
    cfl_get_luma_subsampling_444_lbd = cfl_get_luma_subsampling_444_lbd_avx2;
  cfl_get_predict_lbd_fn = cfl_get_predict_lbd_fn_c;
  if (flags & HAS_SSSE3)
    cfl_get_predict_lbd_fn = cfl_get_predict_lbd_fn_ssse3;
  if (flags & HAS_AVX2)
    cfl_get_predict_lbd_fn = cfl_get_predict_lbd_fn_avx2;
  cfl_get_subtract_average_fn = cfl_get_subtract_average_fn_sse2;
  if (flags & HAS_AVX2)
    cfl_get_subtract_average_fn = cfl_get_subtract_average_fn_avx2;
}
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif
