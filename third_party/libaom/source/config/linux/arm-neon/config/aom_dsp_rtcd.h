// This file is generated. Do not edit.
#ifndef AOM_DSP_RTCD_H_
#define AOM_DSP_RTCD_H_

#ifdef RTCD_C
#define RTCD_EXTERN
#else
#define RTCD_EXTERN extern
#endif

/*
 * DSP
 */

#include "aom/aom_integer.h"
#include "aom_dsp/aom_dsp_common.h"
#include "av1/common/blockd.h"
#include "av1/common/enums.h"

#ifdef __cplusplus
extern "C" {
#endif

unsigned int aom_avg_4x4_c(const uint8_t*, int p);
unsigned int aom_avg_4x4_neon(const uint8_t*, int p);
#define aom_avg_4x4 aom_avg_4x4_neon

unsigned int aom_avg_8x8_c(const uint8_t*, int p);
unsigned int aom_avg_8x8_neon(const uint8_t*, int p);
#define aom_avg_8x8 aom_avg_8x8_neon

void aom_avg_8x8_quad_c(const uint8_t* s,
                        int p,
                        int x16_idx,
                        int y16_idx,
                        int* avg);
void aom_avg_8x8_quad_neon(const uint8_t* s,
                           int p,
                           int x16_idx,
                           int y16_idx,
                           int* avg);
#define aom_avg_8x8_quad aom_avg_8x8_quad_neon

void aom_blend_a64_hmask_c(uint8_t* dst,
                           uint32_t dst_stride,
                           const uint8_t* src0,
                           uint32_t src0_stride,
                           const uint8_t* src1,
                           uint32_t src1_stride,
                           const uint8_t* mask,
                           int w,
                           int h);
void aom_blend_a64_hmask_neon(uint8_t* dst,
                              uint32_t dst_stride,
                              const uint8_t* src0,
                              uint32_t src0_stride,
                              const uint8_t* src1,
                              uint32_t src1_stride,
                              const uint8_t* mask,
                              int w,
                              int h);
#define aom_blend_a64_hmask aom_blend_a64_hmask_neon

void aom_blend_a64_mask_c(uint8_t* dst,
                          uint32_t dst_stride,
                          const uint8_t* src0,
                          uint32_t src0_stride,
                          const uint8_t* src1,
                          uint32_t src1_stride,
                          const uint8_t* mask,
                          uint32_t mask_stride,
                          int w,
                          int h,
                          int subw,
                          int subh);
#define aom_blend_a64_mask aom_blend_a64_mask_c

void aom_blend_a64_vmask_c(uint8_t* dst,
                           uint32_t dst_stride,
                           const uint8_t* src0,
                           uint32_t src0_stride,
                           const uint8_t* src1,
                           uint32_t src1_stride,
                           const uint8_t* mask,
                           int w,
                           int h);
void aom_blend_a64_vmask_neon(uint8_t* dst,
                              uint32_t dst_stride,
                              const uint8_t* src0,
                              uint32_t src0_stride,
                              const uint8_t* src1,
                              uint32_t src1_stride,
                              const uint8_t* mask,
                              int w,
                              int h);
#define aom_blend_a64_vmask aom_blend_a64_vmask_neon

void aom_comp_avg_pred_c(uint8_t* comp_pred,
                         const uint8_t* pred,
                         int width,
                         int height,
                         const uint8_t* ref,
                         int ref_stride);
#define aom_comp_avg_pred aom_comp_avg_pred_c

void aom_comp_mask_pred_c(uint8_t* comp_pred,
                          const uint8_t* pred,
                          int width,
                          int height,
                          const uint8_t* ref,
                          int ref_stride,
                          const uint8_t* mask,
                          int mask_stride,
                          int invert_mask);
#define aom_comp_mask_pred aom_comp_mask_pred_c

void aom_convolve8_c(const uint8_t* src,
                     ptrdiff_t src_stride,
                     uint8_t* dst,
                     ptrdiff_t dst_stride,
                     const InterpKernel* filter,
                     int x0_q4,
                     int x_step_q4,
                     int y0_q4,
                     int y_step_q4,
                     int w,
                     int h);
#define aom_convolve8 aom_convolve8_c

void aom_convolve8_horiz_c(const uint8_t* src,
                           ptrdiff_t src_stride,
                           uint8_t* dst,
                           ptrdiff_t dst_stride,
                           const int16_t* filter_x,
                           int x_step_q4,
                           const int16_t* filter_y,
                           int y_step_q4,
                           int w,
                           int h);
#define aom_convolve8_horiz aom_convolve8_horiz_c

void aom_convolve8_vert_c(const uint8_t* src,
                          ptrdiff_t src_stride,
                          uint8_t* dst,
                          ptrdiff_t dst_stride,
                          const int16_t* filter_x,
                          int x_step_q4,
                          const int16_t* filter_y,
                          int y_step_q4,
                          int w,
                          int h);
#define aom_convolve8_vert aom_convolve8_vert_c

void aom_convolve_copy_c(const uint8_t* src,
                         ptrdiff_t src_stride,
                         uint8_t* dst,
                         ptrdiff_t dst_stride,
                         int w,
                         int h);
void aom_convolve_copy_neon(const uint8_t* src,
                            ptrdiff_t src_stride,
                            uint8_t* dst,
                            ptrdiff_t dst_stride,
                            int w,
                            int h);
#define aom_convolve_copy aom_convolve_copy_neon

void aom_dc_128_predictor_16x16_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void aom_dc_128_predictor_16x16_neon(uint8_t* dst,
                                     ptrdiff_t y_stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
#define aom_dc_128_predictor_16x16 aom_dc_128_predictor_16x16_neon

void aom_dc_128_predictor_16x32_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
#define aom_dc_128_predictor_16x32 aom_dc_128_predictor_16x32_c

void aom_dc_128_predictor_16x4_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
#define aom_dc_128_predictor_16x4 aom_dc_128_predictor_16x4_c

void aom_dc_128_predictor_16x64_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
#define aom_dc_128_predictor_16x64 aom_dc_128_predictor_16x64_c

void aom_dc_128_predictor_16x8_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
#define aom_dc_128_predictor_16x8 aom_dc_128_predictor_16x8_c

void aom_dc_128_predictor_32x16_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
#define aom_dc_128_predictor_32x16 aom_dc_128_predictor_32x16_c

void aom_dc_128_predictor_32x32_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void aom_dc_128_predictor_32x32_neon(uint8_t* dst,
                                     ptrdiff_t y_stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
#define aom_dc_128_predictor_32x32 aom_dc_128_predictor_32x32_neon

void aom_dc_128_predictor_32x64_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
#define aom_dc_128_predictor_32x64 aom_dc_128_predictor_32x64_c

void aom_dc_128_predictor_32x8_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
#define aom_dc_128_predictor_32x8 aom_dc_128_predictor_32x8_c

void aom_dc_128_predictor_4x16_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
#define aom_dc_128_predictor_4x16 aom_dc_128_predictor_4x16_c

void aom_dc_128_predictor_4x4_c(uint8_t* dst,
                                ptrdiff_t y_stride,
                                const uint8_t* above,
                                const uint8_t* left);
void aom_dc_128_predictor_4x4_neon(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
#define aom_dc_128_predictor_4x4 aom_dc_128_predictor_4x4_neon

void aom_dc_128_predictor_4x8_c(uint8_t* dst,
                                ptrdiff_t y_stride,
                                const uint8_t* above,
                                const uint8_t* left);
#define aom_dc_128_predictor_4x8 aom_dc_128_predictor_4x8_c

void aom_dc_128_predictor_64x16_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
#define aom_dc_128_predictor_64x16 aom_dc_128_predictor_64x16_c

void aom_dc_128_predictor_64x32_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
#define aom_dc_128_predictor_64x32 aom_dc_128_predictor_64x32_c

void aom_dc_128_predictor_64x64_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
#define aom_dc_128_predictor_64x64 aom_dc_128_predictor_64x64_c

void aom_dc_128_predictor_8x16_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
#define aom_dc_128_predictor_8x16 aom_dc_128_predictor_8x16_c

void aom_dc_128_predictor_8x32_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
#define aom_dc_128_predictor_8x32 aom_dc_128_predictor_8x32_c

void aom_dc_128_predictor_8x4_c(uint8_t* dst,
                                ptrdiff_t y_stride,
                                const uint8_t* above,
                                const uint8_t* left);
#define aom_dc_128_predictor_8x4 aom_dc_128_predictor_8x4_c

void aom_dc_128_predictor_8x8_c(uint8_t* dst,
                                ptrdiff_t y_stride,
                                const uint8_t* above,
                                const uint8_t* left);
void aom_dc_128_predictor_8x8_neon(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
#define aom_dc_128_predictor_8x8 aom_dc_128_predictor_8x8_neon

void aom_dc_left_predictor_16x16_c(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
void aom_dc_left_predictor_16x16_neon(uint8_t* dst,
                                      ptrdiff_t y_stride,
                                      const uint8_t* above,
                                      const uint8_t* left);
#define aom_dc_left_predictor_16x16 aom_dc_left_predictor_16x16_neon

void aom_dc_left_predictor_16x32_c(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
#define aom_dc_left_predictor_16x32 aom_dc_left_predictor_16x32_c

void aom_dc_left_predictor_16x4_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
#define aom_dc_left_predictor_16x4 aom_dc_left_predictor_16x4_c

void aom_dc_left_predictor_16x64_c(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
#define aom_dc_left_predictor_16x64 aom_dc_left_predictor_16x64_c

void aom_dc_left_predictor_16x8_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
#define aom_dc_left_predictor_16x8 aom_dc_left_predictor_16x8_c

void aom_dc_left_predictor_32x16_c(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
#define aom_dc_left_predictor_32x16 aom_dc_left_predictor_32x16_c

void aom_dc_left_predictor_32x32_c(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
void aom_dc_left_predictor_32x32_neon(uint8_t* dst,
                                      ptrdiff_t y_stride,
                                      const uint8_t* above,
                                      const uint8_t* left);
#define aom_dc_left_predictor_32x32 aom_dc_left_predictor_32x32_neon

void aom_dc_left_predictor_32x64_c(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
#define aom_dc_left_predictor_32x64 aom_dc_left_predictor_32x64_c

void aom_dc_left_predictor_32x8_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
#define aom_dc_left_predictor_32x8 aom_dc_left_predictor_32x8_c

void aom_dc_left_predictor_4x16_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
#define aom_dc_left_predictor_4x16 aom_dc_left_predictor_4x16_c

void aom_dc_left_predictor_4x4_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
void aom_dc_left_predictor_4x4_neon(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
#define aom_dc_left_predictor_4x4 aom_dc_left_predictor_4x4_neon

void aom_dc_left_predictor_4x8_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
#define aom_dc_left_predictor_4x8 aom_dc_left_predictor_4x8_c

void aom_dc_left_predictor_64x16_c(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
#define aom_dc_left_predictor_64x16 aom_dc_left_predictor_64x16_c

void aom_dc_left_predictor_64x32_c(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
#define aom_dc_left_predictor_64x32 aom_dc_left_predictor_64x32_c

void aom_dc_left_predictor_64x64_c(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
#define aom_dc_left_predictor_64x64 aom_dc_left_predictor_64x64_c

void aom_dc_left_predictor_8x16_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
#define aom_dc_left_predictor_8x16 aom_dc_left_predictor_8x16_c

void aom_dc_left_predictor_8x32_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
#define aom_dc_left_predictor_8x32 aom_dc_left_predictor_8x32_c

void aom_dc_left_predictor_8x4_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
#define aom_dc_left_predictor_8x4 aom_dc_left_predictor_8x4_c

void aom_dc_left_predictor_8x8_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
void aom_dc_left_predictor_8x8_neon(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
#define aom_dc_left_predictor_8x8 aom_dc_left_predictor_8x8_neon

void aom_dc_predictor_16x16_c(uint8_t* dst,
                              ptrdiff_t y_stride,
                              const uint8_t* above,
                              const uint8_t* left);
void aom_dc_predictor_16x16_neon(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
#define aom_dc_predictor_16x16 aom_dc_predictor_16x16_neon

void aom_dc_predictor_16x32_c(uint8_t* dst,
                              ptrdiff_t y_stride,
                              const uint8_t* above,
                              const uint8_t* left);
#define aom_dc_predictor_16x32 aom_dc_predictor_16x32_c

void aom_dc_predictor_16x4_c(uint8_t* dst,
                             ptrdiff_t y_stride,
                             const uint8_t* above,
                             const uint8_t* left);
#define aom_dc_predictor_16x4 aom_dc_predictor_16x4_c

void aom_dc_predictor_16x64_c(uint8_t* dst,
                              ptrdiff_t y_stride,
                              const uint8_t* above,
                              const uint8_t* left);
#define aom_dc_predictor_16x64 aom_dc_predictor_16x64_c

void aom_dc_predictor_16x8_c(uint8_t* dst,
                             ptrdiff_t y_stride,
                             const uint8_t* above,
                             const uint8_t* left);
#define aom_dc_predictor_16x8 aom_dc_predictor_16x8_c

void aom_dc_predictor_32x16_c(uint8_t* dst,
                              ptrdiff_t y_stride,
                              const uint8_t* above,
                              const uint8_t* left);
#define aom_dc_predictor_32x16 aom_dc_predictor_32x16_c

void aom_dc_predictor_32x32_c(uint8_t* dst,
                              ptrdiff_t y_stride,
                              const uint8_t* above,
                              const uint8_t* left);
void aom_dc_predictor_32x32_neon(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
#define aom_dc_predictor_32x32 aom_dc_predictor_32x32_neon

void aom_dc_predictor_32x64_c(uint8_t* dst,
                              ptrdiff_t y_stride,
                              const uint8_t* above,
                              const uint8_t* left);
#define aom_dc_predictor_32x64 aom_dc_predictor_32x64_c

void aom_dc_predictor_32x8_c(uint8_t* dst,
                             ptrdiff_t y_stride,
                             const uint8_t* above,
                             const uint8_t* left);
#define aom_dc_predictor_32x8 aom_dc_predictor_32x8_c

void aom_dc_predictor_4x16_c(uint8_t* dst,
                             ptrdiff_t y_stride,
                             const uint8_t* above,
                             const uint8_t* left);
#define aom_dc_predictor_4x16 aom_dc_predictor_4x16_c

void aom_dc_predictor_4x4_c(uint8_t* dst,
                            ptrdiff_t y_stride,
                            const uint8_t* above,
                            const uint8_t* left);
void aom_dc_predictor_4x4_neon(uint8_t* dst,
                               ptrdiff_t y_stride,
                               const uint8_t* above,
                               const uint8_t* left);
#define aom_dc_predictor_4x4 aom_dc_predictor_4x4_neon

void aom_dc_predictor_4x8_c(uint8_t* dst,
                            ptrdiff_t y_stride,
                            const uint8_t* above,
                            const uint8_t* left);
#define aom_dc_predictor_4x8 aom_dc_predictor_4x8_c

void aom_dc_predictor_64x16_c(uint8_t* dst,
                              ptrdiff_t y_stride,
                              const uint8_t* above,
                              const uint8_t* left);
#define aom_dc_predictor_64x16 aom_dc_predictor_64x16_c

void aom_dc_predictor_64x32_c(uint8_t* dst,
                              ptrdiff_t y_stride,
                              const uint8_t* above,
                              const uint8_t* left);
#define aom_dc_predictor_64x32 aom_dc_predictor_64x32_c

void aom_dc_predictor_64x64_c(uint8_t* dst,
                              ptrdiff_t y_stride,
                              const uint8_t* above,
                              const uint8_t* left);
#define aom_dc_predictor_64x64 aom_dc_predictor_64x64_c

void aom_dc_predictor_8x16_c(uint8_t* dst,
                             ptrdiff_t y_stride,
                             const uint8_t* above,
                             const uint8_t* left);
#define aom_dc_predictor_8x16 aom_dc_predictor_8x16_c

void aom_dc_predictor_8x32_c(uint8_t* dst,
                             ptrdiff_t y_stride,
                             const uint8_t* above,
                             const uint8_t* left);
#define aom_dc_predictor_8x32 aom_dc_predictor_8x32_c

void aom_dc_predictor_8x4_c(uint8_t* dst,
                            ptrdiff_t y_stride,
                            const uint8_t* above,
                            const uint8_t* left);
#define aom_dc_predictor_8x4 aom_dc_predictor_8x4_c

void aom_dc_predictor_8x8_c(uint8_t* dst,
                            ptrdiff_t y_stride,
                            const uint8_t* above,
                            const uint8_t* left);
void aom_dc_predictor_8x8_neon(uint8_t* dst,
                               ptrdiff_t y_stride,
                               const uint8_t* above,
                               const uint8_t* left);
#define aom_dc_predictor_8x8 aom_dc_predictor_8x8_neon

void aom_dc_top_predictor_16x16_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void aom_dc_top_predictor_16x16_neon(uint8_t* dst,
                                     ptrdiff_t y_stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
#define aom_dc_top_predictor_16x16 aom_dc_top_predictor_16x16_neon

void aom_dc_top_predictor_16x32_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
#define aom_dc_top_predictor_16x32 aom_dc_top_predictor_16x32_c

void aom_dc_top_predictor_16x4_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
#define aom_dc_top_predictor_16x4 aom_dc_top_predictor_16x4_c

void aom_dc_top_predictor_16x64_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
#define aom_dc_top_predictor_16x64 aom_dc_top_predictor_16x64_c

void aom_dc_top_predictor_16x8_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
#define aom_dc_top_predictor_16x8 aom_dc_top_predictor_16x8_c

void aom_dc_top_predictor_32x16_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
#define aom_dc_top_predictor_32x16 aom_dc_top_predictor_32x16_c

void aom_dc_top_predictor_32x32_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void aom_dc_top_predictor_32x32_neon(uint8_t* dst,
                                     ptrdiff_t y_stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
#define aom_dc_top_predictor_32x32 aom_dc_top_predictor_32x32_neon

void aom_dc_top_predictor_32x64_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
#define aom_dc_top_predictor_32x64 aom_dc_top_predictor_32x64_c

void aom_dc_top_predictor_32x8_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
#define aom_dc_top_predictor_32x8 aom_dc_top_predictor_32x8_c

void aom_dc_top_predictor_4x16_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
#define aom_dc_top_predictor_4x16 aom_dc_top_predictor_4x16_c

void aom_dc_top_predictor_4x4_c(uint8_t* dst,
                                ptrdiff_t y_stride,
                                const uint8_t* above,
                                const uint8_t* left);
void aom_dc_top_predictor_4x4_neon(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
#define aom_dc_top_predictor_4x4 aom_dc_top_predictor_4x4_neon

void aom_dc_top_predictor_4x8_c(uint8_t* dst,
                                ptrdiff_t y_stride,
                                const uint8_t* above,
                                const uint8_t* left);
#define aom_dc_top_predictor_4x8 aom_dc_top_predictor_4x8_c

void aom_dc_top_predictor_64x16_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
#define aom_dc_top_predictor_64x16 aom_dc_top_predictor_64x16_c

void aom_dc_top_predictor_64x32_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
#define aom_dc_top_predictor_64x32 aom_dc_top_predictor_64x32_c

void aom_dc_top_predictor_64x64_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
#define aom_dc_top_predictor_64x64 aom_dc_top_predictor_64x64_c

void aom_dc_top_predictor_8x16_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
#define aom_dc_top_predictor_8x16 aom_dc_top_predictor_8x16_c

void aom_dc_top_predictor_8x32_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
#define aom_dc_top_predictor_8x32 aom_dc_top_predictor_8x32_c

void aom_dc_top_predictor_8x4_c(uint8_t* dst,
                                ptrdiff_t y_stride,
                                const uint8_t* above,
                                const uint8_t* left);
#define aom_dc_top_predictor_8x4 aom_dc_top_predictor_8x4_c

void aom_dc_top_predictor_8x8_c(uint8_t* dst,
                                ptrdiff_t y_stride,
                                const uint8_t* above,
                                const uint8_t* left);
void aom_dc_top_predictor_8x8_neon(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
#define aom_dc_top_predictor_8x8 aom_dc_top_predictor_8x8_neon

void aom_dist_wtd_comp_avg_pred_c(uint8_t* comp_pred,
                                  const uint8_t* pred,
                                  int width,
                                  int height,
                                  const uint8_t* ref,
                                  int ref_stride,
                                  const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_comp_avg_pred aom_dist_wtd_comp_avg_pred_c

unsigned int aom_dist_wtd_sad128x128_avg_c(
    const uint8_t* src_ptr,
    int src_stride,
    const uint8_t* ref_ptr,
    int ref_stride,
    const uint8_t* second_pred,
    const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sad128x128_avg aom_dist_wtd_sad128x128_avg_c

unsigned int aom_dist_wtd_sad128x64_avg_c(
    const uint8_t* src_ptr,
    int src_stride,
    const uint8_t* ref_ptr,
    int ref_stride,
    const uint8_t* second_pred,
    const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sad128x64_avg aom_dist_wtd_sad128x64_avg_c

unsigned int aom_dist_wtd_sad16x16_avg_c(const uint8_t* src_ptr,
                                         int src_stride,
                                         const uint8_t* ref_ptr,
                                         int ref_stride,
                                         const uint8_t* second_pred,
                                         const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sad16x16_avg aom_dist_wtd_sad16x16_avg_c

unsigned int aom_dist_wtd_sad16x32_avg_c(const uint8_t* src_ptr,
                                         int src_stride,
                                         const uint8_t* ref_ptr,
                                         int ref_stride,
                                         const uint8_t* second_pred,
                                         const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sad16x32_avg aom_dist_wtd_sad16x32_avg_c

unsigned int aom_dist_wtd_sad16x8_avg_c(const uint8_t* src_ptr,
                                        int src_stride,
                                        const uint8_t* ref_ptr,
                                        int ref_stride,
                                        const uint8_t* second_pred,
                                        const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sad16x8_avg aom_dist_wtd_sad16x8_avg_c

unsigned int aom_dist_wtd_sad32x16_avg_c(const uint8_t* src_ptr,
                                         int src_stride,
                                         const uint8_t* ref_ptr,
                                         int ref_stride,
                                         const uint8_t* second_pred,
                                         const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sad32x16_avg aom_dist_wtd_sad32x16_avg_c

unsigned int aom_dist_wtd_sad32x32_avg_c(const uint8_t* src_ptr,
                                         int src_stride,
                                         const uint8_t* ref_ptr,
                                         int ref_stride,
                                         const uint8_t* second_pred,
                                         const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sad32x32_avg aom_dist_wtd_sad32x32_avg_c

unsigned int aom_dist_wtd_sad32x64_avg_c(const uint8_t* src_ptr,
                                         int src_stride,
                                         const uint8_t* ref_ptr,
                                         int ref_stride,
                                         const uint8_t* second_pred,
                                         const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sad32x64_avg aom_dist_wtd_sad32x64_avg_c

unsigned int aom_dist_wtd_sad4x4_avg_c(const uint8_t* src_ptr,
                                       int src_stride,
                                       const uint8_t* ref_ptr,
                                       int ref_stride,
                                       const uint8_t* second_pred,
                                       const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sad4x4_avg aom_dist_wtd_sad4x4_avg_c

unsigned int aom_dist_wtd_sad4x8_avg_c(const uint8_t* src_ptr,
                                       int src_stride,
                                       const uint8_t* ref_ptr,
                                       int ref_stride,
                                       const uint8_t* second_pred,
                                       const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sad4x8_avg aom_dist_wtd_sad4x8_avg_c

unsigned int aom_dist_wtd_sad64x128_avg_c(
    const uint8_t* src_ptr,
    int src_stride,
    const uint8_t* ref_ptr,
    int ref_stride,
    const uint8_t* second_pred,
    const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sad64x128_avg aom_dist_wtd_sad64x128_avg_c

unsigned int aom_dist_wtd_sad64x32_avg_c(const uint8_t* src_ptr,
                                         int src_stride,
                                         const uint8_t* ref_ptr,
                                         int ref_stride,
                                         const uint8_t* second_pred,
                                         const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sad64x32_avg aom_dist_wtd_sad64x32_avg_c

unsigned int aom_dist_wtd_sad64x64_avg_c(const uint8_t* src_ptr,
                                         int src_stride,
                                         const uint8_t* ref_ptr,
                                         int ref_stride,
                                         const uint8_t* second_pred,
                                         const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sad64x64_avg aom_dist_wtd_sad64x64_avg_c

unsigned int aom_dist_wtd_sad8x16_avg_c(const uint8_t* src_ptr,
                                        int src_stride,
                                        const uint8_t* ref_ptr,
                                        int ref_stride,
                                        const uint8_t* second_pred,
                                        const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sad8x16_avg aom_dist_wtd_sad8x16_avg_c

unsigned int aom_dist_wtd_sad8x4_avg_c(const uint8_t* src_ptr,
                                       int src_stride,
                                       const uint8_t* ref_ptr,
                                       int ref_stride,
                                       const uint8_t* second_pred,
                                       const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sad8x4_avg aom_dist_wtd_sad8x4_avg_c

unsigned int aom_dist_wtd_sad8x8_avg_c(const uint8_t* src_ptr,
                                       int src_stride,
                                       const uint8_t* ref_ptr,
                                       int ref_stride,
                                       const uint8_t* second_pred,
                                       const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sad8x8_avg aom_dist_wtd_sad8x8_avg_c

uint32_t aom_dist_wtd_sub_pixel_avg_variance128x128_c(
    const uint8_t* src_ptr,
    int source_stride,
    int xoffset,
    int yoffset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred,
    const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sub_pixel_avg_variance128x128 \
  aom_dist_wtd_sub_pixel_avg_variance128x128_c

uint32_t aom_dist_wtd_sub_pixel_avg_variance128x64_c(
    const uint8_t* src_ptr,
    int source_stride,
    int xoffset,
    int yoffset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred,
    const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sub_pixel_avg_variance128x64 \
  aom_dist_wtd_sub_pixel_avg_variance128x64_c

uint32_t aom_dist_wtd_sub_pixel_avg_variance16x16_c(
    const uint8_t* src_ptr,
    int source_stride,
    int xoffset,
    int yoffset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred,
    const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sub_pixel_avg_variance16x16 \
  aom_dist_wtd_sub_pixel_avg_variance16x16_c

uint32_t aom_dist_wtd_sub_pixel_avg_variance16x32_c(
    const uint8_t* src_ptr,
    int source_stride,
    int xoffset,
    int yoffset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred,
    const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sub_pixel_avg_variance16x32 \
  aom_dist_wtd_sub_pixel_avg_variance16x32_c

uint32_t aom_dist_wtd_sub_pixel_avg_variance16x8_c(
    const uint8_t* src_ptr,
    int source_stride,
    int xoffset,
    int yoffset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred,
    const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sub_pixel_avg_variance16x8 \
  aom_dist_wtd_sub_pixel_avg_variance16x8_c

uint32_t aom_dist_wtd_sub_pixel_avg_variance32x16_c(
    const uint8_t* src_ptr,
    int source_stride,
    int xoffset,
    int yoffset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred,
    const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sub_pixel_avg_variance32x16 \
  aom_dist_wtd_sub_pixel_avg_variance32x16_c

uint32_t aom_dist_wtd_sub_pixel_avg_variance32x32_c(
    const uint8_t* src_ptr,
    int source_stride,
    int xoffset,
    int yoffset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred,
    const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sub_pixel_avg_variance32x32 \
  aom_dist_wtd_sub_pixel_avg_variance32x32_c

uint32_t aom_dist_wtd_sub_pixel_avg_variance32x64_c(
    const uint8_t* src_ptr,
    int source_stride,
    int xoffset,
    int yoffset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred,
    const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sub_pixel_avg_variance32x64 \
  aom_dist_wtd_sub_pixel_avg_variance32x64_c

uint32_t aom_dist_wtd_sub_pixel_avg_variance4x4_c(
    const uint8_t* src_ptr,
    int source_stride,
    int xoffset,
    int yoffset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred,
    const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sub_pixel_avg_variance4x4 \
  aom_dist_wtd_sub_pixel_avg_variance4x4_c

uint32_t aom_dist_wtd_sub_pixel_avg_variance4x8_c(
    const uint8_t* src_ptr,
    int source_stride,
    int xoffset,
    int yoffset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred,
    const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sub_pixel_avg_variance4x8 \
  aom_dist_wtd_sub_pixel_avg_variance4x8_c

uint32_t aom_dist_wtd_sub_pixel_avg_variance64x128_c(
    const uint8_t* src_ptr,
    int source_stride,
    int xoffset,
    int yoffset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred,
    const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sub_pixel_avg_variance64x128 \
  aom_dist_wtd_sub_pixel_avg_variance64x128_c

uint32_t aom_dist_wtd_sub_pixel_avg_variance64x32_c(
    const uint8_t* src_ptr,
    int source_stride,
    int xoffset,
    int yoffset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred,
    const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sub_pixel_avg_variance64x32 \
  aom_dist_wtd_sub_pixel_avg_variance64x32_c

uint32_t aom_dist_wtd_sub_pixel_avg_variance64x64_c(
    const uint8_t* src_ptr,
    int source_stride,
    int xoffset,
    int yoffset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred,
    const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sub_pixel_avg_variance64x64 \
  aom_dist_wtd_sub_pixel_avg_variance64x64_c

uint32_t aom_dist_wtd_sub_pixel_avg_variance8x16_c(
    const uint8_t* src_ptr,
    int source_stride,
    int xoffset,
    int yoffset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred,
    const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sub_pixel_avg_variance8x16 \
  aom_dist_wtd_sub_pixel_avg_variance8x16_c

uint32_t aom_dist_wtd_sub_pixel_avg_variance8x4_c(
    const uint8_t* src_ptr,
    int source_stride,
    int xoffset,
    int yoffset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred,
    const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sub_pixel_avg_variance8x4 \
  aom_dist_wtd_sub_pixel_avg_variance8x4_c

uint32_t aom_dist_wtd_sub_pixel_avg_variance8x8_c(
    const uint8_t* src_ptr,
    int source_stride,
    int xoffset,
    int yoffset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred,
    const DIST_WTD_COMP_PARAMS* jcp_param);
#define aom_dist_wtd_sub_pixel_avg_variance8x8 \
  aom_dist_wtd_sub_pixel_avg_variance8x8_c

void aom_fdct4x4_c(const int16_t* input, tran_low_t* output, int stride);
void aom_fdct4x4_neon(const int16_t* input, tran_low_t* output, int stride);
#define aom_fdct4x4 aom_fdct4x4_neon

void aom_fdct4x4_lp_c(const int16_t* input, int16_t* output, int stride);
void aom_fdct4x4_lp_neon(const int16_t* input, int16_t* output, int stride);
#define aom_fdct4x4_lp aom_fdct4x4_lp_neon

void aom_fdct8x8_c(const int16_t* input, tran_low_t* output, int stride);
void aom_fdct8x8_neon(const int16_t* input, tran_low_t* output, int stride);
#define aom_fdct8x8 aom_fdct8x8_neon

void aom_fft16x16_float_c(const float* input, float* temp, float* output);
#define aom_fft16x16_float aom_fft16x16_float_c

void aom_fft2x2_float_c(const float* input, float* temp, float* output);
#define aom_fft2x2_float aom_fft2x2_float_c

void aom_fft32x32_float_c(const float* input, float* temp, float* output);
#define aom_fft32x32_float aom_fft32x32_float_c

void aom_fft4x4_float_c(const float* input, float* temp, float* output);
#define aom_fft4x4_float aom_fft4x4_float_c

void aom_fft8x8_float_c(const float* input, float* temp, float* output);
#define aom_fft8x8_float aom_fft8x8_float_c

void aom_get16x16var_c(const uint8_t* src_ptr,
                       int source_stride,
                       const uint8_t* ref_ptr,
                       int ref_stride,
                       unsigned int* sse,
                       int* sum);
void aom_get16x16var_neon(const uint8_t* src_ptr,
                          int source_stride,
                          const uint8_t* ref_ptr,
                          int ref_stride,
                          unsigned int* sse,
                          int* sum);
#define aom_get16x16var aom_get16x16var_neon

unsigned int aom_get4x4sse_cs_c(const unsigned char* src_ptr,
                                int source_stride,
                                const unsigned char* ref_ptr,
                                int ref_stride);
unsigned int aom_get4x4sse_cs_neon(const unsigned char* src_ptr,
                                   int source_stride,
                                   const unsigned char* ref_ptr,
                                   int ref_stride);
#define aom_get4x4sse_cs aom_get4x4sse_cs_neon

void aom_get8x8var_c(const uint8_t* src_ptr,
                     int source_stride,
                     const uint8_t* ref_ptr,
                     int ref_stride,
                     unsigned int* sse,
                     int* sum);
void aom_get8x8var_neon(const uint8_t* src_ptr,
                        int source_stride,
                        const uint8_t* ref_ptr,
                        int ref_stride,
                        unsigned int* sse,
                        int* sum);
#define aom_get8x8var aom_get8x8var_neon

void aom_get_blk_sse_sum_c(const int16_t* data,
                           int stride,
                           int bw,
                           int bh,
                           int* x_sum,
                           int64_t* x2_sum);
#define aom_get_blk_sse_sum aom_get_blk_sse_sum_c

unsigned int aom_get_mb_ss_c(const int16_t*);
#define aom_get_mb_ss aom_get_mb_ss_c

void aom_get_sse_sum_8x8_quad_c(const uint8_t* src_ptr,
                                int source_stride,
                                const uint8_t* ref_ptr,
                                int ref_stride,
                                unsigned int* sse,
                                int* sum);
void aom_get_sse_sum_8x8_quad_neon(const uint8_t* src_ptr,
                                   int source_stride,
                                   const uint8_t* ref_ptr,
                                   int ref_stride,
                                   unsigned int* sse,
                                   int* sum);
#define aom_get_sse_sum_8x8_quad aom_get_sse_sum_8x8_quad_neon

void aom_h_predictor_16x16_c(uint8_t* dst,
                             ptrdiff_t y_stride,
                             const uint8_t* above,
                             const uint8_t* left);
void aom_h_predictor_16x16_neon(uint8_t* dst,
                                ptrdiff_t y_stride,
                                const uint8_t* above,
                                const uint8_t* left);
#define aom_h_predictor_16x16 aom_h_predictor_16x16_neon

void aom_h_predictor_16x32_c(uint8_t* dst,
                             ptrdiff_t y_stride,
                             const uint8_t* above,
                             const uint8_t* left);
#define aom_h_predictor_16x32 aom_h_predictor_16x32_c

void aom_h_predictor_16x4_c(uint8_t* dst,
                            ptrdiff_t y_stride,
                            const uint8_t* above,
                            const uint8_t* left);
#define aom_h_predictor_16x4 aom_h_predictor_16x4_c

void aom_h_predictor_16x64_c(uint8_t* dst,
                             ptrdiff_t y_stride,
                             const uint8_t* above,
                             const uint8_t* left);
#define aom_h_predictor_16x64 aom_h_predictor_16x64_c

void aom_h_predictor_16x8_c(uint8_t* dst,
                            ptrdiff_t y_stride,
                            const uint8_t* above,
                            const uint8_t* left);
#define aom_h_predictor_16x8 aom_h_predictor_16x8_c

void aom_h_predictor_32x16_c(uint8_t* dst,
                             ptrdiff_t y_stride,
                             const uint8_t* above,
                             const uint8_t* left);
#define aom_h_predictor_32x16 aom_h_predictor_32x16_c

void aom_h_predictor_32x32_c(uint8_t* dst,
                             ptrdiff_t y_stride,
                             const uint8_t* above,
                             const uint8_t* left);
void aom_h_predictor_32x32_neon(uint8_t* dst,
                                ptrdiff_t y_stride,
                                const uint8_t* above,
                                const uint8_t* left);
#define aom_h_predictor_32x32 aom_h_predictor_32x32_neon

void aom_h_predictor_32x64_c(uint8_t* dst,
                             ptrdiff_t y_stride,
                             const uint8_t* above,
                             const uint8_t* left);
#define aom_h_predictor_32x64 aom_h_predictor_32x64_c

void aom_h_predictor_32x8_c(uint8_t* dst,
                            ptrdiff_t y_stride,
                            const uint8_t* above,
                            const uint8_t* left);
#define aom_h_predictor_32x8 aom_h_predictor_32x8_c

void aom_h_predictor_4x16_c(uint8_t* dst,
                            ptrdiff_t y_stride,
                            const uint8_t* above,
                            const uint8_t* left);
#define aom_h_predictor_4x16 aom_h_predictor_4x16_c

void aom_h_predictor_4x4_c(uint8_t* dst,
                           ptrdiff_t y_stride,
                           const uint8_t* above,
                           const uint8_t* left);
void aom_h_predictor_4x4_neon(uint8_t* dst,
                              ptrdiff_t y_stride,
                              const uint8_t* above,
                              const uint8_t* left);
#define aom_h_predictor_4x4 aom_h_predictor_4x4_neon

void aom_h_predictor_4x8_c(uint8_t* dst,
                           ptrdiff_t y_stride,
                           const uint8_t* above,
                           const uint8_t* left);
#define aom_h_predictor_4x8 aom_h_predictor_4x8_c

void aom_h_predictor_64x16_c(uint8_t* dst,
                             ptrdiff_t y_stride,
                             const uint8_t* above,
                             const uint8_t* left);
#define aom_h_predictor_64x16 aom_h_predictor_64x16_c

void aom_h_predictor_64x32_c(uint8_t* dst,
                             ptrdiff_t y_stride,
                             const uint8_t* above,
                             const uint8_t* left);
#define aom_h_predictor_64x32 aom_h_predictor_64x32_c

void aom_h_predictor_64x64_c(uint8_t* dst,
                             ptrdiff_t y_stride,
                             const uint8_t* above,
                             const uint8_t* left);
#define aom_h_predictor_64x64 aom_h_predictor_64x64_c

void aom_h_predictor_8x16_c(uint8_t* dst,
                            ptrdiff_t y_stride,
                            const uint8_t* above,
                            const uint8_t* left);
#define aom_h_predictor_8x16 aom_h_predictor_8x16_c

void aom_h_predictor_8x32_c(uint8_t* dst,
                            ptrdiff_t y_stride,
                            const uint8_t* above,
                            const uint8_t* left);
#define aom_h_predictor_8x32 aom_h_predictor_8x32_c

void aom_h_predictor_8x4_c(uint8_t* dst,
                           ptrdiff_t y_stride,
                           const uint8_t* above,
                           const uint8_t* left);
#define aom_h_predictor_8x4 aom_h_predictor_8x4_c

void aom_h_predictor_8x8_c(uint8_t* dst,
                           ptrdiff_t y_stride,
                           const uint8_t* above,
                           const uint8_t* left);
void aom_h_predictor_8x8_neon(uint8_t* dst,
                              ptrdiff_t y_stride,
                              const uint8_t* above,
                              const uint8_t* left);
#define aom_h_predictor_8x8 aom_h_predictor_8x8_neon

void aom_hadamard_16x16_c(const int16_t* src_diff,
                          ptrdiff_t src_stride,
                          tran_low_t* coeff);
void aom_hadamard_16x16_neon(const int16_t* src_diff,
                             ptrdiff_t src_stride,
                             tran_low_t* coeff);
#define aom_hadamard_16x16 aom_hadamard_16x16_neon

void aom_hadamard_32x32_c(const int16_t* src_diff,
                          ptrdiff_t src_stride,
                          tran_low_t* coeff);
#define aom_hadamard_32x32 aom_hadamard_32x32_c

void aom_hadamard_4x4_c(const int16_t* src_diff,
                        ptrdiff_t src_stride,
                        tran_low_t* coeff);
#define aom_hadamard_4x4 aom_hadamard_4x4_c

void aom_hadamard_8x8_c(const int16_t* src_diff,
                        ptrdiff_t src_stride,
                        tran_low_t* coeff);
void aom_hadamard_8x8_neon(const int16_t* src_diff,
                           ptrdiff_t src_stride,
                           tran_low_t* coeff);
#define aom_hadamard_8x8 aom_hadamard_8x8_neon

void aom_hadamard_lp_16x16_c(const int16_t* src_diff,
                             ptrdiff_t src_stride,
                             int16_t* coeff);
void aom_hadamard_lp_16x16_neon(const int16_t* src_diff,
                                ptrdiff_t src_stride,
                                int16_t* coeff);
#define aom_hadamard_lp_16x16 aom_hadamard_lp_16x16_neon

void aom_hadamard_lp_8x8_c(const int16_t* src_diff,
                           ptrdiff_t src_stride,
                           int16_t* coeff);
void aom_hadamard_lp_8x8_neon(const int16_t* src_diff,
                              ptrdiff_t src_stride,
                              int16_t* coeff);
#define aom_hadamard_lp_8x8 aom_hadamard_lp_8x8_neon

void aom_hadamard_lp_8x8_dual_c(const int16_t* src_diff,
                                ptrdiff_t src_stride,
                                int16_t* coeff);
void aom_hadamard_lp_8x8_dual_neon(const int16_t* src_diff,
                                   ptrdiff_t src_stride,
                                   int16_t* coeff);
#define aom_hadamard_lp_8x8_dual aom_hadamard_lp_8x8_dual_neon

void aom_ifft16x16_float_c(const float* input, float* temp, float* output);
#define aom_ifft16x16_float aom_ifft16x16_float_c

void aom_ifft2x2_float_c(const float* input, float* temp, float* output);
#define aom_ifft2x2_float aom_ifft2x2_float_c

void aom_ifft32x32_float_c(const float* input, float* temp, float* output);
#define aom_ifft32x32_float aom_ifft32x32_float_c

void aom_ifft4x4_float_c(const float* input, float* temp, float* output);
#define aom_ifft4x4_float aom_ifft4x4_float_c

void aom_ifft8x8_float_c(const float* input, float* temp, float* output);
#define aom_ifft8x8_float aom_ifft8x8_float_c

int16_t aom_int_pro_col_c(const uint8_t* ref, const int width);
int16_t aom_int_pro_col_neon(const uint8_t* ref, const int width);
#define aom_int_pro_col aom_int_pro_col_neon

void aom_int_pro_row_c(int16_t hbuf[16],
                       const uint8_t* ref,
                       const int ref_stride,
                       const int height);
void aom_int_pro_row_neon(int16_t hbuf[16],
                          const uint8_t* ref,
                          const int ref_stride,
                          const int height);
#define aom_int_pro_row aom_int_pro_row_neon

void aom_lowbd_blend_a64_d16_mask_c(uint8_t* dst,
                                    uint32_t dst_stride,
                                    const CONV_BUF_TYPE* src0,
                                    uint32_t src0_stride,
                                    const CONV_BUF_TYPE* src1,
                                    uint32_t src1_stride,
                                    const uint8_t* mask,
                                    uint32_t mask_stride,
                                    int w,
                                    int h,
                                    int subw,
                                    int subh,
                                    ConvolveParams* conv_params);
void aom_lowbd_blend_a64_d16_mask_neon(uint8_t* dst,
                                       uint32_t dst_stride,
                                       const CONV_BUF_TYPE* src0,
                                       uint32_t src0_stride,
                                       const CONV_BUF_TYPE* src1,
                                       uint32_t src1_stride,
                                       const uint8_t* mask,
                                       uint32_t mask_stride,
                                       int w,
                                       int h,
                                       int subw,
                                       int subh,
                                       ConvolveParams* conv_params);
#define aom_lowbd_blend_a64_d16_mask aom_lowbd_blend_a64_d16_mask_neon

void aom_lpf_horizontal_14_c(uint8_t* s,
                             int pitch,
                             const uint8_t* blimit,
                             const uint8_t* limit,
                             const uint8_t* thresh);
void aom_lpf_horizontal_14_neon(uint8_t* s,
                                int pitch,
                                const uint8_t* blimit,
                                const uint8_t* limit,
                                const uint8_t* thresh);
#define aom_lpf_horizontal_14 aom_lpf_horizontal_14_neon

void aom_lpf_horizontal_14_dual_c(uint8_t* s,
                                  int pitch,
                                  const uint8_t* blimit0,
                                  const uint8_t* limit0,
                                  const uint8_t* thresh0,
                                  const uint8_t* blimit1,
                                  const uint8_t* limit1,
                                  const uint8_t* thresh1);
void aom_lpf_horizontal_14_dual_neon(uint8_t* s,
                                     int pitch,
                                     const uint8_t* blimit0,
                                     const uint8_t* limit0,
                                     const uint8_t* thresh0,
                                     const uint8_t* blimit1,
                                     const uint8_t* limit1,
                                     const uint8_t* thresh1);
#define aom_lpf_horizontal_14_dual aom_lpf_horizontal_14_dual_neon

void aom_lpf_horizontal_14_quad_c(uint8_t* s,
                                  int pitch,
                                  const uint8_t* blimit0,
                                  const uint8_t* limit0,
                                  const uint8_t* thresh0);
void aom_lpf_horizontal_14_quad_neon(uint8_t* s,
                                     int pitch,
                                     const uint8_t* blimit0,
                                     const uint8_t* limit0,
                                     const uint8_t* thresh0);
#define aom_lpf_horizontal_14_quad aom_lpf_horizontal_14_quad_neon

void aom_lpf_horizontal_4_c(uint8_t* s,
                            int pitch,
                            const uint8_t* blimit,
                            const uint8_t* limit,
                            const uint8_t* thresh);
void aom_lpf_horizontal_4_neon(uint8_t* s,
                               int pitch,
                               const uint8_t* blimit,
                               const uint8_t* limit,
                               const uint8_t* thresh);
#define aom_lpf_horizontal_4 aom_lpf_horizontal_4_neon

void aom_lpf_horizontal_4_dual_c(uint8_t* s,
                                 int pitch,
                                 const uint8_t* blimit0,
                                 const uint8_t* limit0,
                                 const uint8_t* thresh0,
                                 const uint8_t* blimit1,
                                 const uint8_t* limit1,
                                 const uint8_t* thresh1);
void aom_lpf_horizontal_4_dual_neon(uint8_t* s,
                                    int pitch,
                                    const uint8_t* blimit0,
                                    const uint8_t* limit0,
                                    const uint8_t* thresh0,
                                    const uint8_t* blimit1,
                                    const uint8_t* limit1,
                                    const uint8_t* thresh1);
#define aom_lpf_horizontal_4_dual aom_lpf_horizontal_4_dual_neon

void aom_lpf_horizontal_4_quad_c(uint8_t* s,
                                 int pitch,
                                 const uint8_t* blimit0,
                                 const uint8_t* limit0,
                                 const uint8_t* thresh0);
void aom_lpf_horizontal_4_quad_neon(uint8_t* s,
                                    int pitch,
                                    const uint8_t* blimit0,
                                    const uint8_t* limit0,
                                    const uint8_t* thresh0);
#define aom_lpf_horizontal_4_quad aom_lpf_horizontal_4_quad_neon

void aom_lpf_horizontal_6_c(uint8_t* s,
                            int pitch,
                            const uint8_t* blimit,
                            const uint8_t* limit,
                            const uint8_t* thresh);
void aom_lpf_horizontal_6_neon(uint8_t* s,
                               int pitch,
                               const uint8_t* blimit,
                               const uint8_t* limit,
                               const uint8_t* thresh);
#define aom_lpf_horizontal_6 aom_lpf_horizontal_6_neon

void aom_lpf_horizontal_6_dual_c(uint8_t* s,
                                 int pitch,
                                 const uint8_t* blimit0,
                                 const uint8_t* limit0,
                                 const uint8_t* thresh0,
                                 const uint8_t* blimit1,
                                 const uint8_t* limit1,
                                 const uint8_t* thresh1);
void aom_lpf_horizontal_6_dual_neon(uint8_t* s,
                                    int pitch,
                                    const uint8_t* blimit0,
                                    const uint8_t* limit0,
                                    const uint8_t* thresh0,
                                    const uint8_t* blimit1,
                                    const uint8_t* limit1,
                                    const uint8_t* thresh1);
#define aom_lpf_horizontal_6_dual aom_lpf_horizontal_6_dual_neon

void aom_lpf_horizontal_6_quad_c(uint8_t* s,
                                 int pitch,
                                 const uint8_t* blimit0,
                                 const uint8_t* limit0,
                                 const uint8_t* thresh0);
void aom_lpf_horizontal_6_quad_neon(uint8_t* s,
                                    int pitch,
                                    const uint8_t* blimit0,
                                    const uint8_t* limit0,
                                    const uint8_t* thresh0);
#define aom_lpf_horizontal_6_quad aom_lpf_horizontal_6_quad_neon

void aom_lpf_horizontal_8_c(uint8_t* s,
                            int pitch,
                            const uint8_t* blimit,
                            const uint8_t* limit,
                            const uint8_t* thresh);
void aom_lpf_horizontal_8_neon(uint8_t* s,
                               int pitch,
                               const uint8_t* blimit,
                               const uint8_t* limit,
                               const uint8_t* thresh);
#define aom_lpf_horizontal_8 aom_lpf_horizontal_8_neon

void aom_lpf_horizontal_8_dual_c(uint8_t* s,
                                 int pitch,
                                 const uint8_t* blimit0,
                                 const uint8_t* limit0,
                                 const uint8_t* thresh0,
                                 const uint8_t* blimit1,
                                 const uint8_t* limit1,
                                 const uint8_t* thresh1);
void aom_lpf_horizontal_8_dual_neon(uint8_t* s,
                                    int pitch,
                                    const uint8_t* blimit0,
                                    const uint8_t* limit0,
                                    const uint8_t* thresh0,
                                    const uint8_t* blimit1,
                                    const uint8_t* limit1,
                                    const uint8_t* thresh1);
#define aom_lpf_horizontal_8_dual aom_lpf_horizontal_8_dual_neon

void aom_lpf_horizontal_8_quad_c(uint8_t* s,
                                 int pitch,
                                 const uint8_t* blimit0,
                                 const uint8_t* limit0,
                                 const uint8_t* thresh0);
void aom_lpf_horizontal_8_quad_neon(uint8_t* s,
                                    int pitch,
                                    const uint8_t* blimit0,
                                    const uint8_t* limit0,
                                    const uint8_t* thresh0);
#define aom_lpf_horizontal_8_quad aom_lpf_horizontal_8_quad_neon

void aom_lpf_vertical_14_c(uint8_t* s,
                           int pitch,
                           const uint8_t* blimit,
                           const uint8_t* limit,
                           const uint8_t* thresh);
void aom_lpf_vertical_14_neon(uint8_t* s,
                              int pitch,
                              const uint8_t* blimit,
                              const uint8_t* limit,
                              const uint8_t* thresh);
#define aom_lpf_vertical_14 aom_lpf_vertical_14_neon

void aom_lpf_vertical_14_dual_c(uint8_t* s,
                                int pitch,
                                const uint8_t* blimit0,
                                const uint8_t* limit0,
                                const uint8_t* thresh0,
                                const uint8_t* blimit1,
                                const uint8_t* limit1,
                                const uint8_t* thresh1);
void aom_lpf_vertical_14_dual_neon(uint8_t* s,
                                   int pitch,
                                   const uint8_t* blimit0,
                                   const uint8_t* limit0,
                                   const uint8_t* thresh0,
                                   const uint8_t* blimit1,
                                   const uint8_t* limit1,
                                   const uint8_t* thresh1);
#define aom_lpf_vertical_14_dual aom_lpf_vertical_14_dual_neon

void aom_lpf_vertical_14_quad_c(uint8_t* s,
                                int pitch,
                                const uint8_t* blimit0,
                                const uint8_t* limit0,
                                const uint8_t* thresh0);
void aom_lpf_vertical_14_quad_neon(uint8_t* s,
                                   int pitch,
                                   const uint8_t* blimit0,
                                   const uint8_t* limit0,
                                   const uint8_t* thresh0);
#define aom_lpf_vertical_14_quad aom_lpf_vertical_14_quad_neon

void aom_lpf_vertical_4_c(uint8_t* s,
                          int pitch,
                          const uint8_t* blimit,
                          const uint8_t* limit,
                          const uint8_t* thresh);
void aom_lpf_vertical_4_neon(uint8_t* s,
                             int pitch,
                             const uint8_t* blimit,
                             const uint8_t* limit,
                             const uint8_t* thresh);
#define aom_lpf_vertical_4 aom_lpf_vertical_4_neon

void aom_lpf_vertical_4_dual_c(uint8_t* s,
                               int pitch,
                               const uint8_t* blimit0,
                               const uint8_t* limit0,
                               const uint8_t* thresh0,
                               const uint8_t* blimit1,
                               const uint8_t* limit1,
                               const uint8_t* thresh1);
void aom_lpf_vertical_4_dual_neon(uint8_t* s,
                                  int pitch,
                                  const uint8_t* blimit0,
                                  const uint8_t* limit0,
                                  const uint8_t* thresh0,
                                  const uint8_t* blimit1,
                                  const uint8_t* limit1,
                                  const uint8_t* thresh1);
#define aom_lpf_vertical_4_dual aom_lpf_vertical_4_dual_neon

void aom_lpf_vertical_4_quad_c(uint8_t* s,
                               int pitch,
                               const uint8_t* blimit0,
                               const uint8_t* limit0,
                               const uint8_t* thresh0);
void aom_lpf_vertical_4_quad_neon(uint8_t* s,
                                  int pitch,
                                  const uint8_t* blimit0,
                                  const uint8_t* limit0,
                                  const uint8_t* thresh0);
#define aom_lpf_vertical_4_quad aom_lpf_vertical_4_quad_neon

void aom_lpf_vertical_6_c(uint8_t* s,
                          int pitch,
                          const uint8_t* blimit,
                          const uint8_t* limit,
                          const uint8_t* thresh);
void aom_lpf_vertical_6_neon(uint8_t* s,
                             int pitch,
                             const uint8_t* blimit,
                             const uint8_t* limit,
                             const uint8_t* thresh);
#define aom_lpf_vertical_6 aom_lpf_vertical_6_neon

void aom_lpf_vertical_6_dual_c(uint8_t* s,
                               int pitch,
                               const uint8_t* blimit0,
                               const uint8_t* limit0,
                               const uint8_t* thresh0,
                               const uint8_t* blimit1,
                               const uint8_t* limit1,
                               const uint8_t* thresh1);
void aom_lpf_vertical_6_dual_neon(uint8_t* s,
                                  int pitch,
                                  const uint8_t* blimit0,
                                  const uint8_t* limit0,
                                  const uint8_t* thresh0,
                                  const uint8_t* blimit1,
                                  const uint8_t* limit1,
                                  const uint8_t* thresh1);
#define aom_lpf_vertical_6_dual aom_lpf_vertical_6_dual_neon

void aom_lpf_vertical_6_quad_c(uint8_t* s,
                               int pitch,
                               const uint8_t* blimit0,
                               const uint8_t* limit0,
                               const uint8_t* thresh0);
void aom_lpf_vertical_6_quad_neon(uint8_t* s,
                                  int pitch,
                                  const uint8_t* blimit0,
                                  const uint8_t* limit0,
                                  const uint8_t* thresh0);
#define aom_lpf_vertical_6_quad aom_lpf_vertical_6_quad_neon

void aom_lpf_vertical_8_c(uint8_t* s,
                          int pitch,
                          const uint8_t* blimit,
                          const uint8_t* limit,
                          const uint8_t* thresh);
void aom_lpf_vertical_8_neon(uint8_t* s,
                             int pitch,
                             const uint8_t* blimit,
                             const uint8_t* limit,
                             const uint8_t* thresh);
#define aom_lpf_vertical_8 aom_lpf_vertical_8_neon

void aom_lpf_vertical_8_dual_c(uint8_t* s,
                               int pitch,
                               const uint8_t* blimit0,
                               const uint8_t* limit0,
                               const uint8_t* thresh0,
                               const uint8_t* blimit1,
                               const uint8_t* limit1,
                               const uint8_t* thresh1);
void aom_lpf_vertical_8_dual_neon(uint8_t* s,
                                  int pitch,
                                  const uint8_t* blimit0,
                                  const uint8_t* limit0,
                                  const uint8_t* thresh0,
                                  const uint8_t* blimit1,
                                  const uint8_t* limit1,
                                  const uint8_t* thresh1);
#define aom_lpf_vertical_8_dual aom_lpf_vertical_8_dual_neon

void aom_lpf_vertical_8_quad_c(uint8_t* s,
                               int pitch,
                               const uint8_t* blimit0,
                               const uint8_t* limit0,
                               const uint8_t* thresh0);
void aom_lpf_vertical_8_quad_neon(uint8_t* s,
                                  int pitch,
                                  const uint8_t* blimit0,
                                  const uint8_t* limit0,
                                  const uint8_t* thresh0);
#define aom_lpf_vertical_8_quad aom_lpf_vertical_8_quad_neon

unsigned int aom_masked_sad128x128_c(const uint8_t* src,
                                     int src_stride,
                                     const uint8_t* ref,
                                     int ref_stride,
                                     const uint8_t* second_pred,
                                     const uint8_t* msk,
                                     int msk_stride,
                                     int invert_mask);
#define aom_masked_sad128x128 aom_masked_sad128x128_c

void aom_masked_sad128x128x4d_c(const uint8_t* src,
                                int src_stride,
                                const uint8_t* ref[4],
                                int ref_stride,
                                const uint8_t* second_pred,
                                const uint8_t* msk,
                                int msk_stride,
                                int invert_mask,
                                unsigned sads[4]);
#define aom_masked_sad128x128x4d aom_masked_sad128x128x4d_c

unsigned int aom_masked_sad128x64_c(const uint8_t* src,
                                    int src_stride,
                                    const uint8_t* ref,
                                    int ref_stride,
                                    const uint8_t* second_pred,
                                    const uint8_t* msk,
                                    int msk_stride,
                                    int invert_mask);
#define aom_masked_sad128x64 aom_masked_sad128x64_c

void aom_masked_sad128x64x4d_c(const uint8_t* src,
                               int src_stride,
                               const uint8_t* ref[4],
                               int ref_stride,
                               const uint8_t* second_pred,
                               const uint8_t* msk,
                               int msk_stride,
                               int invert_mask,
                               unsigned sads[4]);
#define aom_masked_sad128x64x4d aom_masked_sad128x64x4d_c

unsigned int aom_masked_sad16x16_c(const uint8_t* src,
                                   int src_stride,
                                   const uint8_t* ref,
                                   int ref_stride,
                                   const uint8_t* second_pred,
                                   const uint8_t* msk,
                                   int msk_stride,
                                   int invert_mask);
#define aom_masked_sad16x16 aom_masked_sad16x16_c

void aom_masked_sad16x16x4d_c(const uint8_t* src,
                              int src_stride,
                              const uint8_t* ref[4],
                              int ref_stride,
                              const uint8_t* second_pred,
                              const uint8_t* msk,
                              int msk_stride,
                              int invert_mask,
                              unsigned sads[4]);
#define aom_masked_sad16x16x4d aom_masked_sad16x16x4d_c

unsigned int aom_masked_sad16x32_c(const uint8_t* src,
                                   int src_stride,
                                   const uint8_t* ref,
                                   int ref_stride,
                                   const uint8_t* second_pred,
                                   const uint8_t* msk,
                                   int msk_stride,
                                   int invert_mask);
#define aom_masked_sad16x32 aom_masked_sad16x32_c

void aom_masked_sad16x32x4d_c(const uint8_t* src,
                              int src_stride,
                              const uint8_t* ref[4],
                              int ref_stride,
                              const uint8_t* second_pred,
                              const uint8_t* msk,
                              int msk_stride,
                              int invert_mask,
                              unsigned sads[4]);
#define aom_masked_sad16x32x4d aom_masked_sad16x32x4d_c

unsigned int aom_masked_sad16x8_c(const uint8_t* src,
                                  int src_stride,
                                  const uint8_t* ref,
                                  int ref_stride,
                                  const uint8_t* second_pred,
                                  const uint8_t* msk,
                                  int msk_stride,
                                  int invert_mask);
#define aom_masked_sad16x8 aom_masked_sad16x8_c

void aom_masked_sad16x8x4d_c(const uint8_t* src,
                             int src_stride,
                             const uint8_t* ref[4],
                             int ref_stride,
                             const uint8_t* second_pred,
                             const uint8_t* msk,
                             int msk_stride,
                             int invert_mask,
                             unsigned sads[4]);
#define aom_masked_sad16x8x4d aom_masked_sad16x8x4d_c

unsigned int aom_masked_sad32x16_c(const uint8_t* src,
                                   int src_stride,
                                   const uint8_t* ref,
                                   int ref_stride,
                                   const uint8_t* second_pred,
                                   const uint8_t* msk,
                                   int msk_stride,
                                   int invert_mask);
#define aom_masked_sad32x16 aom_masked_sad32x16_c

void aom_masked_sad32x16x4d_c(const uint8_t* src,
                              int src_stride,
                              const uint8_t* ref[4],
                              int ref_stride,
                              const uint8_t* second_pred,
                              const uint8_t* msk,
                              int msk_stride,
                              int invert_mask,
                              unsigned sads[4]);
#define aom_masked_sad32x16x4d aom_masked_sad32x16x4d_c

unsigned int aom_masked_sad32x32_c(const uint8_t* src,
                                   int src_stride,
                                   const uint8_t* ref,
                                   int ref_stride,
                                   const uint8_t* second_pred,
                                   const uint8_t* msk,
                                   int msk_stride,
                                   int invert_mask);
#define aom_masked_sad32x32 aom_masked_sad32x32_c

void aom_masked_sad32x32x4d_c(const uint8_t* src,
                              int src_stride,
                              const uint8_t* ref[4],
                              int ref_stride,
                              const uint8_t* second_pred,
                              const uint8_t* msk,
                              int msk_stride,
                              int invert_mask,
                              unsigned sads[4]);
#define aom_masked_sad32x32x4d aom_masked_sad32x32x4d_c

unsigned int aom_masked_sad32x64_c(const uint8_t* src,
                                   int src_stride,
                                   const uint8_t* ref,
                                   int ref_stride,
                                   const uint8_t* second_pred,
                                   const uint8_t* msk,
                                   int msk_stride,
                                   int invert_mask);
#define aom_masked_sad32x64 aom_masked_sad32x64_c

void aom_masked_sad32x64x4d_c(const uint8_t* src,
                              int src_stride,
                              const uint8_t* ref[4],
                              int ref_stride,
                              const uint8_t* second_pred,
                              const uint8_t* msk,
                              int msk_stride,
                              int invert_mask,
                              unsigned sads[4]);
#define aom_masked_sad32x64x4d aom_masked_sad32x64x4d_c

unsigned int aom_masked_sad4x4_c(const uint8_t* src,
                                 int src_stride,
                                 const uint8_t* ref,
                                 int ref_stride,
                                 const uint8_t* second_pred,
                                 const uint8_t* msk,
                                 int msk_stride,
                                 int invert_mask);
#define aom_masked_sad4x4 aom_masked_sad4x4_c

void aom_masked_sad4x4x4d_c(const uint8_t* src,
                            int src_stride,
                            const uint8_t* ref[4],
                            int ref_stride,
                            const uint8_t* second_pred,
                            const uint8_t* msk,
                            int msk_stride,
                            int invert_mask,
                            unsigned sads[4]);
#define aom_masked_sad4x4x4d aom_masked_sad4x4x4d_c

unsigned int aom_masked_sad4x8_c(const uint8_t* src,
                                 int src_stride,
                                 const uint8_t* ref,
                                 int ref_stride,
                                 const uint8_t* second_pred,
                                 const uint8_t* msk,
                                 int msk_stride,
                                 int invert_mask);
#define aom_masked_sad4x8 aom_masked_sad4x8_c

void aom_masked_sad4x8x4d_c(const uint8_t* src,
                            int src_stride,
                            const uint8_t* ref[4],
                            int ref_stride,
                            const uint8_t* second_pred,
                            const uint8_t* msk,
                            int msk_stride,
                            int invert_mask,
                            unsigned sads[4]);
#define aom_masked_sad4x8x4d aom_masked_sad4x8x4d_c

unsigned int aom_masked_sad64x128_c(const uint8_t* src,
                                    int src_stride,
                                    const uint8_t* ref,
                                    int ref_stride,
                                    const uint8_t* second_pred,
                                    const uint8_t* msk,
                                    int msk_stride,
                                    int invert_mask);
#define aom_masked_sad64x128 aom_masked_sad64x128_c

void aom_masked_sad64x128x4d_c(const uint8_t* src,
                               int src_stride,
                               const uint8_t* ref[4],
                               int ref_stride,
                               const uint8_t* second_pred,
                               const uint8_t* msk,
                               int msk_stride,
                               int invert_mask,
                               unsigned sads[4]);
#define aom_masked_sad64x128x4d aom_masked_sad64x128x4d_c

unsigned int aom_masked_sad64x32_c(const uint8_t* src,
                                   int src_stride,
                                   const uint8_t* ref,
                                   int ref_stride,
                                   const uint8_t* second_pred,
                                   const uint8_t* msk,
                                   int msk_stride,
                                   int invert_mask);
#define aom_masked_sad64x32 aom_masked_sad64x32_c

void aom_masked_sad64x32x4d_c(const uint8_t* src,
                              int src_stride,
                              const uint8_t* ref[4],
                              int ref_stride,
                              const uint8_t* second_pred,
                              const uint8_t* msk,
                              int msk_stride,
                              int invert_mask,
                              unsigned sads[4]);
#define aom_masked_sad64x32x4d aom_masked_sad64x32x4d_c

unsigned int aom_masked_sad64x64_c(const uint8_t* src,
                                   int src_stride,
                                   const uint8_t* ref,
                                   int ref_stride,
                                   const uint8_t* second_pred,
                                   const uint8_t* msk,
                                   int msk_stride,
                                   int invert_mask);
#define aom_masked_sad64x64 aom_masked_sad64x64_c

void aom_masked_sad64x64x4d_c(const uint8_t* src,
                              int src_stride,
                              const uint8_t* ref[4],
                              int ref_stride,
                              const uint8_t* second_pred,
                              const uint8_t* msk,
                              int msk_stride,
                              int invert_mask,
                              unsigned sads[4]);
#define aom_masked_sad64x64x4d aom_masked_sad64x64x4d_c

unsigned int aom_masked_sad8x16_c(const uint8_t* src,
                                  int src_stride,
                                  const uint8_t* ref,
                                  int ref_stride,
                                  const uint8_t* second_pred,
                                  const uint8_t* msk,
                                  int msk_stride,
                                  int invert_mask);
#define aom_masked_sad8x16 aom_masked_sad8x16_c

void aom_masked_sad8x16x4d_c(const uint8_t* src,
                             int src_stride,
                             const uint8_t* ref[4],
                             int ref_stride,
                             const uint8_t* second_pred,
                             const uint8_t* msk,
                             int msk_stride,
                             int invert_mask,
                             unsigned sads[4]);
#define aom_masked_sad8x16x4d aom_masked_sad8x16x4d_c

unsigned int aom_masked_sad8x4_c(const uint8_t* src,
                                 int src_stride,
                                 const uint8_t* ref,
                                 int ref_stride,
                                 const uint8_t* second_pred,
                                 const uint8_t* msk,
                                 int msk_stride,
                                 int invert_mask);
#define aom_masked_sad8x4 aom_masked_sad8x4_c

void aom_masked_sad8x4x4d_c(const uint8_t* src,
                            int src_stride,
                            const uint8_t* ref[4],
                            int ref_stride,
                            const uint8_t* second_pred,
                            const uint8_t* msk,
                            int msk_stride,
                            int invert_mask,
                            unsigned sads[4]);
#define aom_masked_sad8x4x4d aom_masked_sad8x4x4d_c

unsigned int aom_masked_sad8x8_c(const uint8_t* src,
                                 int src_stride,
                                 const uint8_t* ref,
                                 int ref_stride,
                                 const uint8_t* second_pred,
                                 const uint8_t* msk,
                                 int msk_stride,
                                 int invert_mask);
#define aom_masked_sad8x8 aom_masked_sad8x8_c

void aom_masked_sad8x8x4d_c(const uint8_t* src,
                            int src_stride,
                            const uint8_t* ref[4],
                            int ref_stride,
                            const uint8_t* second_pred,
                            const uint8_t* msk,
                            int msk_stride,
                            int invert_mask,
                            unsigned sads[4]);
#define aom_masked_sad8x8x4d aom_masked_sad8x8x4d_c

unsigned int aom_masked_sub_pixel_variance128x128_c(const uint8_t* src,
                                                    int src_stride,
                                                    int xoffset,
                                                    int yoffset,
                                                    const uint8_t* ref,
                                                    int ref_stride,
                                                    const uint8_t* second_pred,
                                                    const uint8_t* msk,
                                                    int msk_stride,
                                                    int invert_mask,
                                                    unsigned int* sse);
#define aom_masked_sub_pixel_variance128x128 \
  aom_masked_sub_pixel_variance128x128_c

unsigned int aom_masked_sub_pixel_variance128x64_c(const uint8_t* src,
                                                   int src_stride,
                                                   int xoffset,
                                                   int yoffset,
                                                   const uint8_t* ref,
                                                   int ref_stride,
                                                   const uint8_t* second_pred,
                                                   const uint8_t* msk,
                                                   int msk_stride,
                                                   int invert_mask,
                                                   unsigned int* sse);
#define aom_masked_sub_pixel_variance128x64 \
  aom_masked_sub_pixel_variance128x64_c

unsigned int aom_masked_sub_pixel_variance16x16_c(const uint8_t* src,
                                                  int src_stride,
                                                  int xoffset,
                                                  int yoffset,
                                                  const uint8_t* ref,
                                                  int ref_stride,
                                                  const uint8_t* second_pred,
                                                  const uint8_t* msk,
                                                  int msk_stride,
                                                  int invert_mask,
                                                  unsigned int* sse);
#define aom_masked_sub_pixel_variance16x16 aom_masked_sub_pixel_variance16x16_c

unsigned int aom_masked_sub_pixel_variance16x32_c(const uint8_t* src,
                                                  int src_stride,
                                                  int xoffset,
                                                  int yoffset,
                                                  const uint8_t* ref,
                                                  int ref_stride,
                                                  const uint8_t* second_pred,
                                                  const uint8_t* msk,
                                                  int msk_stride,
                                                  int invert_mask,
                                                  unsigned int* sse);
#define aom_masked_sub_pixel_variance16x32 aom_masked_sub_pixel_variance16x32_c

unsigned int aom_masked_sub_pixel_variance16x8_c(const uint8_t* src,
                                                 int src_stride,
                                                 int xoffset,
                                                 int yoffset,
                                                 const uint8_t* ref,
                                                 int ref_stride,
                                                 const uint8_t* second_pred,
                                                 const uint8_t* msk,
                                                 int msk_stride,
                                                 int invert_mask,
                                                 unsigned int* sse);
#define aom_masked_sub_pixel_variance16x8 aom_masked_sub_pixel_variance16x8_c

unsigned int aom_masked_sub_pixel_variance32x16_c(const uint8_t* src,
                                                  int src_stride,
                                                  int xoffset,
                                                  int yoffset,
                                                  const uint8_t* ref,
                                                  int ref_stride,
                                                  const uint8_t* second_pred,
                                                  const uint8_t* msk,
                                                  int msk_stride,
                                                  int invert_mask,
                                                  unsigned int* sse);
#define aom_masked_sub_pixel_variance32x16 aom_masked_sub_pixel_variance32x16_c

unsigned int aom_masked_sub_pixel_variance32x32_c(const uint8_t* src,
                                                  int src_stride,
                                                  int xoffset,
                                                  int yoffset,
                                                  const uint8_t* ref,
                                                  int ref_stride,
                                                  const uint8_t* second_pred,
                                                  const uint8_t* msk,
                                                  int msk_stride,
                                                  int invert_mask,
                                                  unsigned int* sse);
#define aom_masked_sub_pixel_variance32x32 aom_masked_sub_pixel_variance32x32_c

unsigned int aom_masked_sub_pixel_variance32x64_c(const uint8_t* src,
                                                  int src_stride,
                                                  int xoffset,
                                                  int yoffset,
                                                  const uint8_t* ref,
                                                  int ref_stride,
                                                  const uint8_t* second_pred,
                                                  const uint8_t* msk,
                                                  int msk_stride,
                                                  int invert_mask,
                                                  unsigned int* sse);
#define aom_masked_sub_pixel_variance32x64 aom_masked_sub_pixel_variance32x64_c

unsigned int aom_masked_sub_pixel_variance4x4_c(const uint8_t* src,
                                                int src_stride,
                                                int xoffset,
                                                int yoffset,
                                                const uint8_t* ref,
                                                int ref_stride,
                                                const uint8_t* second_pred,
                                                const uint8_t* msk,
                                                int msk_stride,
                                                int invert_mask,
                                                unsigned int* sse);
#define aom_masked_sub_pixel_variance4x4 aom_masked_sub_pixel_variance4x4_c

unsigned int aom_masked_sub_pixel_variance4x8_c(const uint8_t* src,
                                                int src_stride,
                                                int xoffset,
                                                int yoffset,
                                                const uint8_t* ref,
                                                int ref_stride,
                                                const uint8_t* second_pred,
                                                const uint8_t* msk,
                                                int msk_stride,
                                                int invert_mask,
                                                unsigned int* sse);
#define aom_masked_sub_pixel_variance4x8 aom_masked_sub_pixel_variance4x8_c

unsigned int aom_masked_sub_pixel_variance64x128_c(const uint8_t* src,
                                                   int src_stride,
                                                   int xoffset,
                                                   int yoffset,
                                                   const uint8_t* ref,
                                                   int ref_stride,
                                                   const uint8_t* second_pred,
                                                   const uint8_t* msk,
                                                   int msk_stride,
                                                   int invert_mask,
                                                   unsigned int* sse);
#define aom_masked_sub_pixel_variance64x128 \
  aom_masked_sub_pixel_variance64x128_c

unsigned int aom_masked_sub_pixel_variance64x32_c(const uint8_t* src,
                                                  int src_stride,
                                                  int xoffset,
                                                  int yoffset,
                                                  const uint8_t* ref,
                                                  int ref_stride,
                                                  const uint8_t* second_pred,
                                                  const uint8_t* msk,
                                                  int msk_stride,
                                                  int invert_mask,
                                                  unsigned int* sse);
#define aom_masked_sub_pixel_variance64x32 aom_masked_sub_pixel_variance64x32_c

unsigned int aom_masked_sub_pixel_variance64x64_c(const uint8_t* src,
                                                  int src_stride,
                                                  int xoffset,
                                                  int yoffset,
                                                  const uint8_t* ref,
                                                  int ref_stride,
                                                  const uint8_t* second_pred,
                                                  const uint8_t* msk,
                                                  int msk_stride,
                                                  int invert_mask,
                                                  unsigned int* sse);
#define aom_masked_sub_pixel_variance64x64 aom_masked_sub_pixel_variance64x64_c

unsigned int aom_masked_sub_pixel_variance8x16_c(const uint8_t* src,
                                                 int src_stride,
                                                 int xoffset,
                                                 int yoffset,
                                                 const uint8_t* ref,
                                                 int ref_stride,
                                                 const uint8_t* second_pred,
                                                 const uint8_t* msk,
                                                 int msk_stride,
                                                 int invert_mask,
                                                 unsigned int* sse);
#define aom_masked_sub_pixel_variance8x16 aom_masked_sub_pixel_variance8x16_c

unsigned int aom_masked_sub_pixel_variance8x4_c(const uint8_t* src,
                                                int src_stride,
                                                int xoffset,
                                                int yoffset,
                                                const uint8_t* ref,
                                                int ref_stride,
                                                const uint8_t* second_pred,
                                                const uint8_t* msk,
                                                int msk_stride,
                                                int invert_mask,
                                                unsigned int* sse);
#define aom_masked_sub_pixel_variance8x4 aom_masked_sub_pixel_variance8x4_c

unsigned int aom_masked_sub_pixel_variance8x8_c(const uint8_t* src,
                                                int src_stride,
                                                int xoffset,
                                                int yoffset,
                                                const uint8_t* ref,
                                                int ref_stride,
                                                const uint8_t* second_pred,
                                                const uint8_t* msk,
                                                int msk_stride,
                                                int invert_mask,
                                                unsigned int* sse);
#define aom_masked_sub_pixel_variance8x8 aom_masked_sub_pixel_variance8x8_c

void aom_minmax_8x8_c(const uint8_t* s,
                      int p,
                      const uint8_t* d,
                      int dp,
                      int* min,
                      int* max);
#define aom_minmax_8x8 aom_minmax_8x8_c

unsigned int aom_mse16x16_c(const uint8_t* src_ptr,
                            int source_stride,
                            const uint8_t* ref_ptr,
                            int recon_stride,
                            unsigned int* sse);
unsigned int aom_mse16x16_neon(const uint8_t* src_ptr,
                               int source_stride,
                               const uint8_t* ref_ptr,
                               int recon_stride,
                               unsigned int* sse);
#define aom_mse16x16 aom_mse16x16_neon

unsigned int aom_mse16x8_c(const uint8_t* src_ptr,
                           int source_stride,
                           const uint8_t* ref_ptr,
                           int recon_stride,
                           unsigned int* sse);
#define aom_mse16x8 aom_mse16x8_c

unsigned int aom_mse8x16_c(const uint8_t* src_ptr,
                           int source_stride,
                           const uint8_t* ref_ptr,
                           int recon_stride,
                           unsigned int* sse);
#define aom_mse8x16 aom_mse8x16_c

unsigned int aom_mse8x8_c(const uint8_t* src_ptr,
                          int source_stride,
                          const uint8_t* ref_ptr,
                          int recon_stride,
                          unsigned int* sse);
#define aom_mse8x8 aom_mse8x8_c

uint64_t aom_mse_wxh_16bit_c(uint8_t* dst,
                             int dstride,
                             uint16_t* src,
                             int sstride,
                             int w,
                             int h);
#define aom_mse_wxh_16bit aom_mse_wxh_16bit_c

void aom_paeth_predictor_16x16_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
void aom_paeth_predictor_16x16_neon(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
#define aom_paeth_predictor_16x16 aom_paeth_predictor_16x16_neon

void aom_paeth_predictor_16x32_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
void aom_paeth_predictor_16x32_neon(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
#define aom_paeth_predictor_16x32 aom_paeth_predictor_16x32_neon

void aom_paeth_predictor_16x4_c(uint8_t* dst,
                                ptrdiff_t y_stride,
                                const uint8_t* above,
                                const uint8_t* left);
void aom_paeth_predictor_16x4_neon(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
#define aom_paeth_predictor_16x4 aom_paeth_predictor_16x4_neon

void aom_paeth_predictor_16x64_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
void aom_paeth_predictor_16x64_neon(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
#define aom_paeth_predictor_16x64 aom_paeth_predictor_16x64_neon

void aom_paeth_predictor_16x8_c(uint8_t* dst,
                                ptrdiff_t y_stride,
                                const uint8_t* above,
                                const uint8_t* left);
void aom_paeth_predictor_16x8_neon(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
#define aom_paeth_predictor_16x8 aom_paeth_predictor_16x8_neon

void aom_paeth_predictor_32x16_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
void aom_paeth_predictor_32x16_neon(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
#define aom_paeth_predictor_32x16 aom_paeth_predictor_32x16_neon

void aom_paeth_predictor_32x32_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
void aom_paeth_predictor_32x32_neon(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
#define aom_paeth_predictor_32x32 aom_paeth_predictor_32x32_neon

void aom_paeth_predictor_32x64_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
void aom_paeth_predictor_32x64_neon(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
#define aom_paeth_predictor_32x64 aom_paeth_predictor_32x64_neon

void aom_paeth_predictor_32x8_c(uint8_t* dst,
                                ptrdiff_t y_stride,
                                const uint8_t* above,
                                const uint8_t* left);
void aom_paeth_predictor_32x8_neon(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
#define aom_paeth_predictor_32x8 aom_paeth_predictor_32x8_neon

void aom_paeth_predictor_4x16_c(uint8_t* dst,
                                ptrdiff_t y_stride,
                                const uint8_t* above,
                                const uint8_t* left);
void aom_paeth_predictor_4x16_neon(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
#define aom_paeth_predictor_4x16 aom_paeth_predictor_4x16_neon

void aom_paeth_predictor_4x4_c(uint8_t* dst,
                               ptrdiff_t y_stride,
                               const uint8_t* above,
                               const uint8_t* left);
void aom_paeth_predictor_4x4_neon(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
#define aom_paeth_predictor_4x4 aom_paeth_predictor_4x4_neon

void aom_paeth_predictor_4x8_c(uint8_t* dst,
                               ptrdiff_t y_stride,
                               const uint8_t* above,
                               const uint8_t* left);
void aom_paeth_predictor_4x8_neon(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
#define aom_paeth_predictor_4x8 aom_paeth_predictor_4x8_neon

void aom_paeth_predictor_64x16_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
void aom_paeth_predictor_64x16_neon(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
#define aom_paeth_predictor_64x16 aom_paeth_predictor_64x16_neon

void aom_paeth_predictor_64x32_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
void aom_paeth_predictor_64x32_neon(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
#define aom_paeth_predictor_64x32 aom_paeth_predictor_64x32_neon

void aom_paeth_predictor_64x64_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
void aom_paeth_predictor_64x64_neon(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
#define aom_paeth_predictor_64x64 aom_paeth_predictor_64x64_neon

void aom_paeth_predictor_8x16_c(uint8_t* dst,
                                ptrdiff_t y_stride,
                                const uint8_t* above,
                                const uint8_t* left);
void aom_paeth_predictor_8x16_neon(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
#define aom_paeth_predictor_8x16 aom_paeth_predictor_8x16_neon

void aom_paeth_predictor_8x32_c(uint8_t* dst,
                                ptrdiff_t y_stride,
                                const uint8_t* above,
                                const uint8_t* left);
void aom_paeth_predictor_8x32_neon(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
#define aom_paeth_predictor_8x32 aom_paeth_predictor_8x32_neon

void aom_paeth_predictor_8x4_c(uint8_t* dst,
                               ptrdiff_t y_stride,
                               const uint8_t* above,
                               const uint8_t* left);
void aom_paeth_predictor_8x4_neon(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
#define aom_paeth_predictor_8x4 aom_paeth_predictor_8x4_neon

void aom_paeth_predictor_8x8_c(uint8_t* dst,
                               ptrdiff_t y_stride,
                               const uint8_t* above,
                               const uint8_t* left);
void aom_paeth_predictor_8x8_neon(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
#define aom_paeth_predictor_8x8 aom_paeth_predictor_8x8_neon

void aom_pixel_scale_c(const int16_t* src_diff,
                       ptrdiff_t src_stride,
                       int16_t* coeff,
                       int log_scale,
                       int h8,
                       int w8);
#define aom_pixel_scale aom_pixel_scale_c

void aom_quantize_b_c(const tran_low_t* coeff_ptr,
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
void aom_quantize_b_neon(const tran_low_t* coeff_ptr,
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
#define aom_quantize_b aom_quantize_b_neon

void aom_quantize_b_32x32_c(const tran_low_t* coeff_ptr,
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
void aom_quantize_b_32x32_neon(const tran_low_t* coeff_ptr,
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
#define aom_quantize_b_32x32 aom_quantize_b_32x32_neon

void aom_quantize_b_64x64_c(const tran_low_t* coeff_ptr,
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
void aom_quantize_b_64x64_neon(const tran_low_t* coeff_ptr,
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
#define aom_quantize_b_64x64 aom_quantize_b_64x64_neon

unsigned int aom_sad128x128_c(const uint8_t* src_ptr,
                              int src_stride,
                              const uint8_t* ref_ptr,
                              int ref_stride);
unsigned int aom_sad128x128_neon(const uint8_t* src_ptr,
                                 int src_stride,
                                 const uint8_t* ref_ptr,
                                 int ref_stride);
#define aom_sad128x128 aom_sad128x128_neon

unsigned int aom_sad128x128_avg_c(const uint8_t* src_ptr,
                                  int src_stride,
                                  const uint8_t* ref_ptr,
                                  int ref_stride,
                                  const uint8_t* second_pred);
#define aom_sad128x128_avg aom_sad128x128_avg_c

void aom_sad128x128x4d_c(const uint8_t* src_ptr,
                         int src_stride,
                         const uint8_t* const ref_ptr[4],
                         int ref_stride,
                         uint32_t sad_array[4]);
#define aom_sad128x128x4d aom_sad128x128x4d_c

void aom_sad128x128x4d_avg_c(const uint8_t* src_ptr,
                             int src_stride,
                             const uint8_t* const ref_ptr[4],
                             int ref_stride,
                             const uint8_t* second_pred,
                             uint32_t sad_array[4]);
#define aom_sad128x128x4d_avg aom_sad128x128x4d_avg_c

unsigned int aom_sad128x64_c(const uint8_t* src_ptr,
                             int src_stride,
                             const uint8_t* ref_ptr,
                             int ref_stride);
#define aom_sad128x64 aom_sad128x64_c

unsigned int aom_sad128x64_avg_c(const uint8_t* src_ptr,
                                 int src_stride,
                                 const uint8_t* ref_ptr,
                                 int ref_stride,
                                 const uint8_t* second_pred);
#define aom_sad128x64_avg aom_sad128x64_avg_c

void aom_sad128x64x4d_c(const uint8_t* src_ptr,
                        int src_stride,
                        const uint8_t* const ref_ptr[4],
                        int ref_stride,
                        uint32_t sad_array[4]);
#define aom_sad128x64x4d aom_sad128x64x4d_c

void aom_sad128x64x4d_avg_c(const uint8_t* src_ptr,
                            int src_stride,
                            const uint8_t* const ref_ptr[4],
                            int ref_stride,
                            const uint8_t* second_pred,
                            uint32_t sad_array[4]);
#define aom_sad128x64x4d_avg aom_sad128x64x4d_avg_c

unsigned int aom_sad128xh_c(const uint8_t* a,
                            int a_stride,
                            const uint8_t* b,
                            int b_stride,
                            int width,
                            int height);
#define aom_sad128xh aom_sad128xh_c

unsigned int aom_sad16x16_c(const uint8_t* src_ptr,
                            int src_stride,
                            const uint8_t* ref_ptr,
                            int ref_stride);
unsigned int aom_sad16x16_neon(const uint8_t* src_ptr,
                               int src_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride);
#define aom_sad16x16 aom_sad16x16_neon

unsigned int aom_sad16x16_avg_c(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* ref_ptr,
                                int ref_stride,
                                const uint8_t* second_pred);
#define aom_sad16x16_avg aom_sad16x16_avg_c

void aom_sad16x16x4d_c(const uint8_t* src_ptr,
                       int src_stride,
                       const uint8_t* const ref_ptr[4],
                       int ref_stride,
                       uint32_t sad_array[4]);
void aom_sad16x16x4d_neon(const uint8_t* src_ptr,
                          int src_stride,
                          const uint8_t* const ref_ptr[4],
                          int ref_stride,
                          uint32_t sad_array[4]);
#define aom_sad16x16x4d aom_sad16x16x4d_neon

void aom_sad16x16x4d_avg_c(const uint8_t* src_ptr,
                           int src_stride,
                           const uint8_t* const ref_ptr[4],
                           int ref_stride,
                           const uint8_t* second_pred,
                           uint32_t sad_array[4]);
#define aom_sad16x16x4d_avg aom_sad16x16x4d_avg_c

unsigned int aom_sad16x32_c(const uint8_t* src_ptr,
                            int src_stride,
                            const uint8_t* ref_ptr,
                            int ref_stride);
#define aom_sad16x32 aom_sad16x32_c

unsigned int aom_sad16x32_avg_c(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* ref_ptr,
                                int ref_stride,
                                const uint8_t* second_pred);
#define aom_sad16x32_avg aom_sad16x32_avg_c

void aom_sad16x32x4d_c(const uint8_t* src_ptr,
                       int src_stride,
                       const uint8_t* const ref_ptr[4],
                       int ref_stride,
                       uint32_t sad_array[4]);
#define aom_sad16x32x4d aom_sad16x32x4d_c

void aom_sad16x32x4d_avg_c(const uint8_t* src_ptr,
                           int src_stride,
                           const uint8_t* const ref_ptr[4],
                           int ref_stride,
                           const uint8_t* second_pred,
                           uint32_t sad_array[4]);
#define aom_sad16x32x4d_avg aom_sad16x32x4d_avg_c

unsigned int aom_sad16x8_c(const uint8_t* src_ptr,
                           int src_stride,
                           const uint8_t* ref_ptr,
                           int ref_stride);
unsigned int aom_sad16x8_neon(const uint8_t* src_ptr,
                              int src_stride,
                              const uint8_t* ref_ptr,
                              int ref_stride);
#define aom_sad16x8 aom_sad16x8_neon

unsigned int aom_sad16x8_avg_c(const uint8_t* src_ptr,
                               int src_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride,
                               const uint8_t* second_pred);
#define aom_sad16x8_avg aom_sad16x8_avg_c

void aom_sad16x8x4d_c(const uint8_t* src_ptr,
                      int src_stride,
                      const uint8_t* const ref_ptr[4],
                      int ref_stride,
                      uint32_t sad_array[4]);
#define aom_sad16x8x4d aom_sad16x8x4d_c

void aom_sad16x8x4d_avg_c(const uint8_t* src_ptr,
                          int src_stride,
                          const uint8_t* const ref_ptr[4],
                          int ref_stride,
                          const uint8_t* second_pred,
                          uint32_t sad_array[4]);
#define aom_sad16x8x4d_avg aom_sad16x8x4d_avg_c

unsigned int aom_sad16xh_c(const uint8_t* a,
                           int a_stride,
                           const uint8_t* b,
                           int b_stride,
                           int width,
                           int height);
#define aom_sad16xh aom_sad16xh_c

unsigned int aom_sad32x16_c(const uint8_t* src_ptr,
                            int src_stride,
                            const uint8_t* ref_ptr,
                            int ref_stride);
#define aom_sad32x16 aom_sad32x16_c

unsigned int aom_sad32x16_avg_c(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* ref_ptr,
                                int ref_stride,
                                const uint8_t* second_pred);
#define aom_sad32x16_avg aom_sad32x16_avg_c

void aom_sad32x16x4d_c(const uint8_t* src_ptr,
                       int src_stride,
                       const uint8_t* const ref_ptr[4],
                       int ref_stride,
                       uint32_t sad_array[4]);
#define aom_sad32x16x4d aom_sad32x16x4d_c

void aom_sad32x16x4d_avg_c(const uint8_t* src_ptr,
                           int src_stride,
                           const uint8_t* const ref_ptr[4],
                           int ref_stride,
                           const uint8_t* second_pred,
                           uint32_t sad_array[4]);
#define aom_sad32x16x4d_avg aom_sad32x16x4d_avg_c

unsigned int aom_sad32x32_c(const uint8_t* src_ptr,
                            int src_stride,
                            const uint8_t* ref_ptr,
                            int ref_stride);
unsigned int aom_sad32x32_neon(const uint8_t* src_ptr,
                               int src_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride);
#define aom_sad32x32 aom_sad32x32_neon

unsigned int aom_sad32x32_avg_c(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* ref_ptr,
                                int ref_stride,
                                const uint8_t* second_pred);
#define aom_sad32x32_avg aom_sad32x32_avg_c

void aom_sad32x32x4d_c(const uint8_t* src_ptr,
                       int src_stride,
                       const uint8_t* const ref_ptr[4],
                       int ref_stride,
                       uint32_t sad_array[4]);
void aom_sad32x32x4d_neon(const uint8_t* src_ptr,
                          int src_stride,
                          const uint8_t* const ref_ptr[4],
                          int ref_stride,
                          uint32_t sad_array[4]);
#define aom_sad32x32x4d aom_sad32x32x4d_neon

void aom_sad32x32x4d_avg_c(const uint8_t* src_ptr,
                           int src_stride,
                           const uint8_t* const ref_ptr[4],
                           int ref_stride,
                           const uint8_t* second_pred,
                           uint32_t sad_array[4]);
#define aom_sad32x32x4d_avg aom_sad32x32x4d_avg_c

unsigned int aom_sad32x64_c(const uint8_t* src_ptr,
                            int src_stride,
                            const uint8_t* ref_ptr,
                            int ref_stride);
#define aom_sad32x64 aom_sad32x64_c

unsigned int aom_sad32x64_avg_c(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* ref_ptr,
                                int ref_stride,
                                const uint8_t* second_pred);
#define aom_sad32x64_avg aom_sad32x64_avg_c

void aom_sad32x64x4d_c(const uint8_t* src_ptr,
                       int src_stride,
                       const uint8_t* const ref_ptr[4],
                       int ref_stride,
                       uint32_t sad_array[4]);
#define aom_sad32x64x4d aom_sad32x64x4d_c

void aom_sad32x64x4d_avg_c(const uint8_t* src_ptr,
                           int src_stride,
                           const uint8_t* const ref_ptr[4],
                           int ref_stride,
                           const uint8_t* second_pred,
                           uint32_t sad_array[4]);
#define aom_sad32x64x4d_avg aom_sad32x64x4d_avg_c

unsigned int aom_sad32xh_c(const uint8_t* a,
                           int a_stride,
                           const uint8_t* b,
                           int b_stride,
                           int width,
                           int height);
#define aom_sad32xh aom_sad32xh_c

unsigned int aom_sad4x4_c(const uint8_t* src_ptr,
                          int src_stride,
                          const uint8_t* ref_ptr,
                          int ref_stride);
unsigned int aom_sad4x4_neon(const uint8_t* src_ptr,
                             int src_stride,
                             const uint8_t* ref_ptr,
                             int ref_stride);
#define aom_sad4x4 aom_sad4x4_neon

unsigned int aom_sad4x4_avg_c(const uint8_t* src_ptr,
                              int src_stride,
                              const uint8_t* ref_ptr,
                              int ref_stride,
                              const uint8_t* second_pred);
#define aom_sad4x4_avg aom_sad4x4_avg_c

void aom_sad4x4x4d_c(const uint8_t* src_ptr,
                     int src_stride,
                     const uint8_t* const ref_ptr[4],
                     int ref_stride,
                     uint32_t sad_array[4]);
#define aom_sad4x4x4d aom_sad4x4x4d_c

void aom_sad4x4x4d_avg_c(const uint8_t* src_ptr,
                         int src_stride,
                         const uint8_t* const ref_ptr[4],
                         int ref_stride,
                         const uint8_t* second_pred,
                         uint32_t sad_array[4]);
#define aom_sad4x4x4d_avg aom_sad4x4x4d_avg_c

unsigned int aom_sad4x8_c(const uint8_t* src_ptr,
                          int src_stride,
                          const uint8_t* ref_ptr,
                          int ref_stride);
#define aom_sad4x8 aom_sad4x8_c

unsigned int aom_sad4x8_avg_c(const uint8_t* src_ptr,
                              int src_stride,
                              const uint8_t* ref_ptr,
                              int ref_stride,
                              const uint8_t* second_pred);
#define aom_sad4x8_avg aom_sad4x8_avg_c

void aom_sad4x8x4d_c(const uint8_t* src_ptr,
                     int src_stride,
                     const uint8_t* const ref_ptr[4],
                     int ref_stride,
                     uint32_t sad_array[4]);
#define aom_sad4x8x4d aom_sad4x8x4d_c

void aom_sad4x8x4d_avg_c(const uint8_t* src_ptr,
                         int src_stride,
                         const uint8_t* const ref_ptr[4],
                         int ref_stride,
                         const uint8_t* second_pred,
                         uint32_t sad_array[4]);
#define aom_sad4x8x4d_avg aom_sad4x8x4d_avg_c

unsigned int aom_sad4xh_c(const uint8_t* a,
                          int a_stride,
                          const uint8_t* b,
                          int b_stride,
                          int width,
                          int height);
#define aom_sad4xh aom_sad4xh_c

unsigned int aom_sad64x128_c(const uint8_t* src_ptr,
                             int src_stride,
                             const uint8_t* ref_ptr,
                             int ref_stride);
#define aom_sad64x128 aom_sad64x128_c

unsigned int aom_sad64x128_avg_c(const uint8_t* src_ptr,
                                 int src_stride,
                                 const uint8_t* ref_ptr,
                                 int ref_stride,
                                 const uint8_t* second_pred);
#define aom_sad64x128_avg aom_sad64x128_avg_c

void aom_sad64x128x4d_c(const uint8_t* src_ptr,
                        int src_stride,
                        const uint8_t* const ref_ptr[4],
                        int ref_stride,
                        uint32_t sad_array[4]);
#define aom_sad64x128x4d aom_sad64x128x4d_c

void aom_sad64x128x4d_avg_c(const uint8_t* src_ptr,
                            int src_stride,
                            const uint8_t* const ref_ptr[4],
                            int ref_stride,
                            const uint8_t* second_pred,
                            uint32_t sad_array[4]);
#define aom_sad64x128x4d_avg aom_sad64x128x4d_avg_c

unsigned int aom_sad64x32_c(const uint8_t* src_ptr,
                            int src_stride,
                            const uint8_t* ref_ptr,
                            int ref_stride);
#define aom_sad64x32 aom_sad64x32_c

unsigned int aom_sad64x32_avg_c(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* ref_ptr,
                                int ref_stride,
                                const uint8_t* second_pred);
#define aom_sad64x32_avg aom_sad64x32_avg_c

void aom_sad64x32x4d_c(const uint8_t* src_ptr,
                       int src_stride,
                       const uint8_t* const ref_ptr[4],
                       int ref_stride,
                       uint32_t sad_array[4]);
#define aom_sad64x32x4d aom_sad64x32x4d_c

void aom_sad64x32x4d_avg_c(const uint8_t* src_ptr,
                           int src_stride,
                           const uint8_t* const ref_ptr[4],
                           int ref_stride,
                           const uint8_t* second_pred,
                           uint32_t sad_array[4]);
#define aom_sad64x32x4d_avg aom_sad64x32x4d_avg_c

unsigned int aom_sad64x64_c(const uint8_t* src_ptr,
                            int src_stride,
                            const uint8_t* ref_ptr,
                            int ref_stride);
unsigned int aom_sad64x64_neon(const uint8_t* src_ptr,
                               int src_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride);
#define aom_sad64x64 aom_sad64x64_neon

unsigned int aom_sad64x64_avg_c(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* ref_ptr,
                                int ref_stride,
                                const uint8_t* second_pred);
#define aom_sad64x64_avg aom_sad64x64_avg_c

void aom_sad64x64x4d_c(const uint8_t* src_ptr,
                       int src_stride,
                       const uint8_t* const ref_ptr[4],
                       int ref_stride,
                       uint32_t sad_array[4]);
void aom_sad64x64x4d_neon(const uint8_t* src_ptr,
                          int src_stride,
                          const uint8_t* const ref_ptr[4],
                          int ref_stride,
                          uint32_t sad_array[4]);
#define aom_sad64x64x4d aom_sad64x64x4d_neon

void aom_sad64x64x4d_avg_c(const uint8_t* src_ptr,
                           int src_stride,
                           const uint8_t* const ref_ptr[4],
                           int ref_stride,
                           const uint8_t* second_pred,
                           uint32_t sad_array[4]);
#define aom_sad64x64x4d_avg aom_sad64x64x4d_avg_c

unsigned int aom_sad64xh_c(const uint8_t* a,
                           int a_stride,
                           const uint8_t* b,
                           int b_stride,
                           int width,
                           int height);
#define aom_sad64xh aom_sad64xh_c

unsigned int aom_sad8x16_c(const uint8_t* src_ptr,
                           int src_stride,
                           const uint8_t* ref_ptr,
                           int ref_stride);
unsigned int aom_sad8x16_neon(const uint8_t* src_ptr,
                              int src_stride,
                              const uint8_t* ref_ptr,
                              int ref_stride);
#define aom_sad8x16 aom_sad8x16_neon

unsigned int aom_sad8x16_avg_c(const uint8_t* src_ptr,
                               int src_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride,
                               const uint8_t* second_pred);
#define aom_sad8x16_avg aom_sad8x16_avg_c

void aom_sad8x16x4d_c(const uint8_t* src_ptr,
                      int src_stride,
                      const uint8_t* const ref_ptr[4],
                      int ref_stride,
                      uint32_t sad_array[4]);
#define aom_sad8x16x4d aom_sad8x16x4d_c

void aom_sad8x16x4d_avg_c(const uint8_t* src_ptr,
                          int src_stride,
                          const uint8_t* const ref_ptr[4],
                          int ref_stride,
                          const uint8_t* second_pred,
                          uint32_t sad_array[4]);
#define aom_sad8x16x4d_avg aom_sad8x16x4d_avg_c

unsigned int aom_sad8x4_c(const uint8_t* src_ptr,
                          int src_stride,
                          const uint8_t* ref_ptr,
                          int ref_stride);
#define aom_sad8x4 aom_sad8x4_c

unsigned int aom_sad8x4_avg_c(const uint8_t* src_ptr,
                              int src_stride,
                              const uint8_t* ref_ptr,
                              int ref_stride,
                              const uint8_t* second_pred);
#define aom_sad8x4_avg aom_sad8x4_avg_c

void aom_sad8x4x4d_c(const uint8_t* src_ptr,
                     int src_stride,
                     const uint8_t* const ref_ptr[4],
                     int ref_stride,
                     uint32_t sad_array[4]);
#define aom_sad8x4x4d aom_sad8x4x4d_c

void aom_sad8x4x4d_avg_c(const uint8_t* src_ptr,
                         int src_stride,
                         const uint8_t* const ref_ptr[4],
                         int ref_stride,
                         const uint8_t* second_pred,
                         uint32_t sad_array[4]);
#define aom_sad8x4x4d_avg aom_sad8x4x4d_avg_c

unsigned int aom_sad8x8_c(const uint8_t* src_ptr,
                          int src_stride,
                          const uint8_t* ref_ptr,
                          int ref_stride);
unsigned int aom_sad8x8_neon(const uint8_t* src_ptr,
                             int src_stride,
                             const uint8_t* ref_ptr,
                             int ref_stride);
#define aom_sad8x8 aom_sad8x8_neon

unsigned int aom_sad8x8_avg_c(const uint8_t* src_ptr,
                              int src_stride,
                              const uint8_t* ref_ptr,
                              int ref_stride,
                              const uint8_t* second_pred);
#define aom_sad8x8_avg aom_sad8x8_avg_c

void aom_sad8x8x4d_c(const uint8_t* src_ptr,
                     int src_stride,
                     const uint8_t* const ref_ptr[4],
                     int ref_stride,
                     uint32_t sad_array[4]);
#define aom_sad8x8x4d aom_sad8x8x4d_c

void aom_sad8x8x4d_avg_c(const uint8_t* src_ptr,
                         int src_stride,
                         const uint8_t* const ref_ptr[4],
                         int ref_stride,
                         const uint8_t* second_pred,
                         uint32_t sad_array[4]);
#define aom_sad8x8x4d_avg aom_sad8x8x4d_avg_c

unsigned int aom_sad8xh_c(const uint8_t* a,
                          int a_stride,
                          const uint8_t* b,
                          int b_stride,
                          int width,
                          int height);
#define aom_sad8xh aom_sad8xh_c

unsigned int aom_sad_skip_128x128_c(const uint8_t* src_ptr,
                                    int src_stride,
                                    const uint8_t* ref_ptr,
                                    int ref_stride);
unsigned int aom_sad_skip_128x128_neon(const uint8_t* src_ptr,
                                       int src_stride,
                                       const uint8_t* ref_ptr,
                                       int ref_stride);
#define aom_sad_skip_128x128 aom_sad_skip_128x128_neon

void aom_sad_skip_128x128x4d_c(const uint8_t* src_ptr,
                               int src_stride,
                               const uint8_t* const ref_ptr[4],
                               int ref_stride,
                               uint32_t sad_array[4]);
void aom_sad_skip_128x128x4d_neon(const uint8_t* src_ptr,
                                  int src_stride,
                                  const uint8_t* const ref_ptr[4],
                                  int ref_stride,
                                  uint32_t sad_array[4]);
#define aom_sad_skip_128x128x4d aom_sad_skip_128x128x4d_neon

unsigned int aom_sad_skip_128x64_c(const uint8_t* src_ptr,
                                   int src_stride,
                                   const uint8_t* ref_ptr,
                                   int ref_stride);
unsigned int aom_sad_skip_128x64_neon(const uint8_t* src_ptr,
                                      int src_stride,
                                      const uint8_t* ref_ptr,
                                      int ref_stride);
#define aom_sad_skip_128x64 aom_sad_skip_128x64_neon

void aom_sad_skip_128x64x4d_c(const uint8_t* src_ptr,
                              int src_stride,
                              const uint8_t* const ref_ptr[4],
                              int ref_stride,
                              uint32_t sad_array[4]);
void aom_sad_skip_128x64x4d_neon(const uint8_t* src_ptr,
                                 int src_stride,
                                 const uint8_t* const ref_ptr[4],
                                 int ref_stride,
                                 uint32_t sad_array[4]);
#define aom_sad_skip_128x64x4d aom_sad_skip_128x64x4d_neon

unsigned int aom_sad_skip_16x16_c(const uint8_t* src_ptr,
                                  int src_stride,
                                  const uint8_t* ref_ptr,
                                  int ref_stride);
unsigned int aom_sad_skip_16x16_neon(const uint8_t* src_ptr,
                                     int src_stride,
                                     const uint8_t* ref_ptr,
                                     int ref_stride);
#define aom_sad_skip_16x16 aom_sad_skip_16x16_neon

void aom_sad_skip_16x16x4d_c(const uint8_t* src_ptr,
                             int src_stride,
                             const uint8_t* const ref_ptr[4],
                             int ref_stride,
                             uint32_t sad_array[4]);
void aom_sad_skip_16x16x4d_neon(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* const ref_ptr[4],
                                int ref_stride,
                                uint32_t sad_array[4]);
#define aom_sad_skip_16x16x4d aom_sad_skip_16x16x4d_neon

unsigned int aom_sad_skip_16x32_c(const uint8_t* src_ptr,
                                  int src_stride,
                                  const uint8_t* ref_ptr,
                                  int ref_stride);
unsigned int aom_sad_skip_16x32_neon(const uint8_t* src_ptr,
                                     int src_stride,
                                     const uint8_t* ref_ptr,
                                     int ref_stride);
#define aom_sad_skip_16x32 aom_sad_skip_16x32_neon

void aom_sad_skip_16x32x4d_c(const uint8_t* src_ptr,
                             int src_stride,
                             const uint8_t* const ref_ptr[4],
                             int ref_stride,
                             uint32_t sad_array[4]);
void aom_sad_skip_16x32x4d_neon(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* const ref_ptr[4],
                                int ref_stride,
                                uint32_t sad_array[4]);
#define aom_sad_skip_16x32x4d aom_sad_skip_16x32x4d_neon

unsigned int aom_sad_skip_16x8_c(const uint8_t* src_ptr,
                                 int src_stride,
                                 const uint8_t* ref_ptr,
                                 int ref_stride);
unsigned int aom_sad_skip_16x8_neon(const uint8_t* src_ptr,
                                    int src_stride,
                                    const uint8_t* ref_ptr,
                                    int ref_stride);
#define aom_sad_skip_16x8 aom_sad_skip_16x8_neon

void aom_sad_skip_16x8x4d_c(const uint8_t* src_ptr,
                            int src_stride,
                            const uint8_t* const ref_ptr[4],
                            int ref_stride,
                            uint32_t sad_array[4]);
void aom_sad_skip_16x8x4d_neon(const uint8_t* src_ptr,
                               int src_stride,
                               const uint8_t* const ref_ptr[4],
                               int ref_stride,
                               uint32_t sad_array[4]);
#define aom_sad_skip_16x8x4d aom_sad_skip_16x8x4d_neon

unsigned int aom_sad_skip_32x16_c(const uint8_t* src_ptr,
                                  int src_stride,
                                  const uint8_t* ref_ptr,
                                  int ref_stride);
unsigned int aom_sad_skip_32x16_neon(const uint8_t* src_ptr,
                                     int src_stride,
                                     const uint8_t* ref_ptr,
                                     int ref_stride);
#define aom_sad_skip_32x16 aom_sad_skip_32x16_neon

void aom_sad_skip_32x16x4d_c(const uint8_t* src_ptr,
                             int src_stride,
                             const uint8_t* const ref_ptr[4],
                             int ref_stride,
                             uint32_t sad_array[4]);
void aom_sad_skip_32x16x4d_neon(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* const ref_ptr[4],
                                int ref_stride,
                                uint32_t sad_array[4]);
#define aom_sad_skip_32x16x4d aom_sad_skip_32x16x4d_neon

unsigned int aom_sad_skip_32x32_c(const uint8_t* src_ptr,
                                  int src_stride,
                                  const uint8_t* ref_ptr,
                                  int ref_stride);
unsigned int aom_sad_skip_32x32_neon(const uint8_t* src_ptr,
                                     int src_stride,
                                     const uint8_t* ref_ptr,
                                     int ref_stride);
#define aom_sad_skip_32x32 aom_sad_skip_32x32_neon

void aom_sad_skip_32x32x4d_c(const uint8_t* src_ptr,
                             int src_stride,
                             const uint8_t* const ref_ptr[4],
                             int ref_stride,
                             uint32_t sad_array[4]);
void aom_sad_skip_32x32x4d_neon(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* const ref_ptr[4],
                                int ref_stride,
                                uint32_t sad_array[4]);
#define aom_sad_skip_32x32x4d aom_sad_skip_32x32x4d_neon

unsigned int aom_sad_skip_32x64_c(const uint8_t* src_ptr,
                                  int src_stride,
                                  const uint8_t* ref_ptr,
                                  int ref_stride);
unsigned int aom_sad_skip_32x64_neon(const uint8_t* src_ptr,
                                     int src_stride,
                                     const uint8_t* ref_ptr,
                                     int ref_stride);
#define aom_sad_skip_32x64 aom_sad_skip_32x64_neon

void aom_sad_skip_32x64x4d_c(const uint8_t* src_ptr,
                             int src_stride,
                             const uint8_t* const ref_ptr[4],
                             int ref_stride,
                             uint32_t sad_array[4]);
void aom_sad_skip_32x64x4d_neon(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* const ref_ptr[4],
                                int ref_stride,
                                uint32_t sad_array[4]);
#define aom_sad_skip_32x64x4d aom_sad_skip_32x64x4d_neon

unsigned int aom_sad_skip_4x4_c(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* ref_ptr,
                                int ref_stride);
#define aom_sad_skip_4x4 aom_sad_skip_4x4_c

void aom_sad_skip_4x4x4d_c(const uint8_t* src_ptr,
                           int src_stride,
                           const uint8_t* const ref_ptr[4],
                           int ref_stride,
                           uint32_t sad_array[4]);
#define aom_sad_skip_4x4x4d aom_sad_skip_4x4x4d_c

unsigned int aom_sad_skip_4x8_c(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* ref_ptr,
                                int ref_stride);
unsigned int aom_sad_skip_4x8_neon(const uint8_t* src_ptr,
                                   int src_stride,
                                   const uint8_t* ref_ptr,
                                   int ref_stride);
#define aom_sad_skip_4x8 aom_sad_skip_4x8_neon

void aom_sad_skip_4x8x4d_c(const uint8_t* src_ptr,
                           int src_stride,
                           const uint8_t* const ref_ptr[4],
                           int ref_stride,
                           uint32_t sad_array[4]);
void aom_sad_skip_4x8x4d_neon(const uint8_t* src_ptr,
                              int src_stride,
                              const uint8_t* const ref_ptr[4],
                              int ref_stride,
                              uint32_t sad_array[4]);
#define aom_sad_skip_4x8x4d aom_sad_skip_4x8x4d_neon

unsigned int aom_sad_skip_64x128_c(const uint8_t* src_ptr,
                                   int src_stride,
                                   const uint8_t* ref_ptr,
                                   int ref_stride);
unsigned int aom_sad_skip_64x128_neon(const uint8_t* src_ptr,
                                      int src_stride,
                                      const uint8_t* ref_ptr,
                                      int ref_stride);
#define aom_sad_skip_64x128 aom_sad_skip_64x128_neon

void aom_sad_skip_64x128x4d_c(const uint8_t* src_ptr,
                              int src_stride,
                              const uint8_t* const ref_ptr[4],
                              int ref_stride,
                              uint32_t sad_array[4]);
void aom_sad_skip_64x128x4d_neon(const uint8_t* src_ptr,
                                 int src_stride,
                                 const uint8_t* const ref_ptr[4],
                                 int ref_stride,
                                 uint32_t sad_array[4]);
#define aom_sad_skip_64x128x4d aom_sad_skip_64x128x4d_neon

unsigned int aom_sad_skip_64x32_c(const uint8_t* src_ptr,
                                  int src_stride,
                                  const uint8_t* ref_ptr,
                                  int ref_stride);
unsigned int aom_sad_skip_64x32_neon(const uint8_t* src_ptr,
                                     int src_stride,
                                     const uint8_t* ref_ptr,
                                     int ref_stride);
#define aom_sad_skip_64x32 aom_sad_skip_64x32_neon

void aom_sad_skip_64x32x4d_c(const uint8_t* src_ptr,
                             int src_stride,
                             const uint8_t* const ref_ptr[4],
                             int ref_stride,
                             uint32_t sad_array[4]);
void aom_sad_skip_64x32x4d_neon(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* const ref_ptr[4],
                                int ref_stride,
                                uint32_t sad_array[4]);
#define aom_sad_skip_64x32x4d aom_sad_skip_64x32x4d_neon

unsigned int aom_sad_skip_64x64_c(const uint8_t* src_ptr,
                                  int src_stride,
                                  const uint8_t* ref_ptr,
                                  int ref_stride);
unsigned int aom_sad_skip_64x64_neon(const uint8_t* src_ptr,
                                     int src_stride,
                                     const uint8_t* ref_ptr,
                                     int ref_stride);
#define aom_sad_skip_64x64 aom_sad_skip_64x64_neon

void aom_sad_skip_64x64x4d_c(const uint8_t* src_ptr,
                             int src_stride,
                             const uint8_t* const ref_ptr[4],
                             int ref_stride,
                             uint32_t sad_array[4]);
void aom_sad_skip_64x64x4d_neon(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* const ref_ptr[4],
                                int ref_stride,
                                uint32_t sad_array[4]);
#define aom_sad_skip_64x64x4d aom_sad_skip_64x64x4d_neon

unsigned int aom_sad_skip_8x16_c(const uint8_t* src_ptr,
                                 int src_stride,
                                 const uint8_t* ref_ptr,
                                 int ref_stride);
unsigned int aom_sad_skip_8x16_neon(const uint8_t* src_ptr,
                                    int src_stride,
                                    const uint8_t* ref_ptr,
                                    int ref_stride);
#define aom_sad_skip_8x16 aom_sad_skip_8x16_neon

void aom_sad_skip_8x16x4d_c(const uint8_t* src_ptr,
                            int src_stride,
                            const uint8_t* const ref_ptr[4],
                            int ref_stride,
                            uint32_t sad_array[4]);
void aom_sad_skip_8x16x4d_neon(const uint8_t* src_ptr,
                               int src_stride,
                               const uint8_t* const ref_ptr[4],
                               int ref_stride,
                               uint32_t sad_array[4]);
#define aom_sad_skip_8x16x4d aom_sad_skip_8x16x4d_neon

unsigned int aom_sad_skip_8x4_c(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* ref_ptr,
                                int ref_stride);
#define aom_sad_skip_8x4 aom_sad_skip_8x4_c

void aom_sad_skip_8x4x4d_c(const uint8_t* src_ptr,
                           int src_stride,
                           const uint8_t* const ref_ptr[4],
                           int ref_stride,
                           uint32_t sad_array[4]);
#define aom_sad_skip_8x4x4d aom_sad_skip_8x4x4d_c

unsigned int aom_sad_skip_8x8_c(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* ref_ptr,
                                int ref_stride);
unsigned int aom_sad_skip_8x8_neon(const uint8_t* src_ptr,
                                   int src_stride,
                                   const uint8_t* ref_ptr,
                                   int ref_stride);
#define aom_sad_skip_8x8 aom_sad_skip_8x8_neon

void aom_sad_skip_8x8x4d_c(const uint8_t* src_ptr,
                           int src_stride,
                           const uint8_t* const ref_ptr[4],
                           int ref_stride,
                           uint32_t sad_array[4]);
void aom_sad_skip_8x8x4d_neon(const uint8_t* src_ptr,
                              int src_stride,
                              const uint8_t* const ref_ptr[4],
                              int ref_stride,
                              uint32_t sad_array[4]);
#define aom_sad_skip_8x8x4d aom_sad_skip_8x8x4d_neon

int aom_satd_c(const tran_low_t* coeff, int length);
int aom_satd_neon(const tran_low_t* coeff, int length);
#define aom_satd aom_satd_neon

int aom_satd_lp_c(const int16_t* coeff, int length);
int aom_satd_lp_neon(const int16_t* coeff, int length);
#define aom_satd_lp aom_satd_lp_neon

void aom_scaled_2d_c(const uint8_t* src,
                     ptrdiff_t src_stride,
                     uint8_t* dst,
                     ptrdiff_t dst_stride,
                     const InterpKernel* filter,
                     int x0_q4,
                     int x_step_q4,
                     int y0_q4,
                     int y_step_q4,
                     int w,
                     int h);
void aom_scaled_2d_neon(const uint8_t* src,
                        ptrdiff_t src_stride,
                        uint8_t* dst,
                        ptrdiff_t dst_stride,
                        const InterpKernel* filter,
                        int x0_q4,
                        int x_step_q4,
                        int y0_q4,
                        int y_step_q4,
                        int w,
                        int h);
#define aom_scaled_2d aom_scaled_2d_neon

void aom_smooth_h_predictor_16x16_c(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
void aom_smooth_h_predictor_16x16_neon(uint8_t* dst,
                                       ptrdiff_t y_stride,
                                       const uint8_t* above,
                                       const uint8_t* left);
#define aom_smooth_h_predictor_16x16 aom_smooth_h_predictor_16x16_neon

void aom_smooth_h_predictor_16x32_c(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
void aom_smooth_h_predictor_16x32_neon(uint8_t* dst,
                                       ptrdiff_t y_stride,
                                       const uint8_t* above,
                                       const uint8_t* left);
#define aom_smooth_h_predictor_16x32 aom_smooth_h_predictor_16x32_neon

void aom_smooth_h_predictor_16x4_c(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
void aom_smooth_h_predictor_16x4_neon(uint8_t* dst,
                                      ptrdiff_t y_stride,
                                      const uint8_t* above,
                                      const uint8_t* left);
#define aom_smooth_h_predictor_16x4 aom_smooth_h_predictor_16x4_neon

void aom_smooth_h_predictor_16x64_c(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
void aom_smooth_h_predictor_16x64_neon(uint8_t* dst,
                                       ptrdiff_t y_stride,
                                       const uint8_t* above,
                                       const uint8_t* left);
#define aom_smooth_h_predictor_16x64 aom_smooth_h_predictor_16x64_neon

void aom_smooth_h_predictor_16x8_c(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
void aom_smooth_h_predictor_16x8_neon(uint8_t* dst,
                                      ptrdiff_t y_stride,
                                      const uint8_t* above,
                                      const uint8_t* left);
#define aom_smooth_h_predictor_16x8 aom_smooth_h_predictor_16x8_neon

void aom_smooth_h_predictor_32x16_c(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
void aom_smooth_h_predictor_32x16_neon(uint8_t* dst,
                                       ptrdiff_t y_stride,
                                       const uint8_t* above,
                                       const uint8_t* left);
#define aom_smooth_h_predictor_32x16 aom_smooth_h_predictor_32x16_neon

void aom_smooth_h_predictor_32x32_c(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
void aom_smooth_h_predictor_32x32_neon(uint8_t* dst,
                                       ptrdiff_t y_stride,
                                       const uint8_t* above,
                                       const uint8_t* left);
#define aom_smooth_h_predictor_32x32 aom_smooth_h_predictor_32x32_neon

void aom_smooth_h_predictor_32x64_c(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
void aom_smooth_h_predictor_32x64_neon(uint8_t* dst,
                                       ptrdiff_t y_stride,
                                       const uint8_t* above,
                                       const uint8_t* left);
#define aom_smooth_h_predictor_32x64 aom_smooth_h_predictor_32x64_neon

void aom_smooth_h_predictor_32x8_c(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
void aom_smooth_h_predictor_32x8_neon(uint8_t* dst,
                                      ptrdiff_t y_stride,
                                      const uint8_t* above,
                                      const uint8_t* left);
#define aom_smooth_h_predictor_32x8 aom_smooth_h_predictor_32x8_neon

void aom_smooth_h_predictor_4x16_c(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
void aom_smooth_h_predictor_4x16_neon(uint8_t* dst,
                                      ptrdiff_t y_stride,
                                      const uint8_t* above,
                                      const uint8_t* left);
#define aom_smooth_h_predictor_4x16 aom_smooth_h_predictor_4x16_neon

void aom_smooth_h_predictor_4x4_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void aom_smooth_h_predictor_4x4_neon(uint8_t* dst,
                                     ptrdiff_t y_stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
#define aom_smooth_h_predictor_4x4 aom_smooth_h_predictor_4x4_neon

void aom_smooth_h_predictor_4x8_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void aom_smooth_h_predictor_4x8_neon(uint8_t* dst,
                                     ptrdiff_t y_stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
#define aom_smooth_h_predictor_4x8 aom_smooth_h_predictor_4x8_neon

void aom_smooth_h_predictor_64x16_c(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
void aom_smooth_h_predictor_64x16_neon(uint8_t* dst,
                                       ptrdiff_t y_stride,
                                       const uint8_t* above,
                                       const uint8_t* left);
#define aom_smooth_h_predictor_64x16 aom_smooth_h_predictor_64x16_neon

void aom_smooth_h_predictor_64x32_c(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
void aom_smooth_h_predictor_64x32_neon(uint8_t* dst,
                                       ptrdiff_t y_stride,
                                       const uint8_t* above,
                                       const uint8_t* left);
#define aom_smooth_h_predictor_64x32 aom_smooth_h_predictor_64x32_neon

void aom_smooth_h_predictor_64x64_c(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
void aom_smooth_h_predictor_64x64_neon(uint8_t* dst,
                                       ptrdiff_t y_stride,
                                       const uint8_t* above,
                                       const uint8_t* left);
#define aom_smooth_h_predictor_64x64 aom_smooth_h_predictor_64x64_neon

void aom_smooth_h_predictor_8x16_c(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
void aom_smooth_h_predictor_8x16_neon(uint8_t* dst,
                                      ptrdiff_t y_stride,
                                      const uint8_t* above,
                                      const uint8_t* left);
#define aom_smooth_h_predictor_8x16 aom_smooth_h_predictor_8x16_neon

void aom_smooth_h_predictor_8x32_c(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
void aom_smooth_h_predictor_8x32_neon(uint8_t* dst,
                                      ptrdiff_t y_stride,
                                      const uint8_t* above,
                                      const uint8_t* left);
#define aom_smooth_h_predictor_8x32 aom_smooth_h_predictor_8x32_neon

void aom_smooth_h_predictor_8x4_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void aom_smooth_h_predictor_8x4_neon(uint8_t* dst,
                                     ptrdiff_t y_stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
#define aom_smooth_h_predictor_8x4 aom_smooth_h_predictor_8x4_neon

void aom_smooth_h_predictor_8x8_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void aom_smooth_h_predictor_8x8_neon(uint8_t* dst,
                                     ptrdiff_t y_stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
#define aom_smooth_h_predictor_8x8 aom_smooth_h_predictor_8x8_neon

void aom_smooth_predictor_16x16_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void aom_smooth_predictor_16x16_neon(uint8_t* dst,
                                     ptrdiff_t y_stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
#define aom_smooth_predictor_16x16 aom_smooth_predictor_16x16_neon

void aom_smooth_predictor_16x32_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void aom_smooth_predictor_16x32_neon(uint8_t* dst,
                                     ptrdiff_t y_stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
#define aom_smooth_predictor_16x32 aom_smooth_predictor_16x32_neon

void aom_smooth_predictor_16x4_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
void aom_smooth_predictor_16x4_neon(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
#define aom_smooth_predictor_16x4 aom_smooth_predictor_16x4_neon

void aom_smooth_predictor_16x64_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void aom_smooth_predictor_16x64_neon(uint8_t* dst,
                                     ptrdiff_t y_stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
#define aom_smooth_predictor_16x64 aom_smooth_predictor_16x64_neon

void aom_smooth_predictor_16x8_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
void aom_smooth_predictor_16x8_neon(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
#define aom_smooth_predictor_16x8 aom_smooth_predictor_16x8_neon

void aom_smooth_predictor_32x16_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void aom_smooth_predictor_32x16_neon(uint8_t* dst,
                                     ptrdiff_t y_stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
#define aom_smooth_predictor_32x16 aom_smooth_predictor_32x16_neon

void aom_smooth_predictor_32x32_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void aom_smooth_predictor_32x32_neon(uint8_t* dst,
                                     ptrdiff_t y_stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
#define aom_smooth_predictor_32x32 aom_smooth_predictor_32x32_neon

void aom_smooth_predictor_32x64_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void aom_smooth_predictor_32x64_neon(uint8_t* dst,
                                     ptrdiff_t y_stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
#define aom_smooth_predictor_32x64 aom_smooth_predictor_32x64_neon

void aom_smooth_predictor_32x8_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
void aom_smooth_predictor_32x8_neon(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
#define aom_smooth_predictor_32x8 aom_smooth_predictor_32x8_neon

void aom_smooth_predictor_4x16_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
void aom_smooth_predictor_4x16_neon(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
#define aom_smooth_predictor_4x16 aom_smooth_predictor_4x16_neon

void aom_smooth_predictor_4x4_c(uint8_t* dst,
                                ptrdiff_t y_stride,
                                const uint8_t* above,
                                const uint8_t* left);
void aom_smooth_predictor_4x4_neon(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
#define aom_smooth_predictor_4x4 aom_smooth_predictor_4x4_neon

void aom_smooth_predictor_4x8_c(uint8_t* dst,
                                ptrdiff_t y_stride,
                                const uint8_t* above,
                                const uint8_t* left);
void aom_smooth_predictor_4x8_neon(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
#define aom_smooth_predictor_4x8 aom_smooth_predictor_4x8_neon

void aom_smooth_predictor_64x16_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void aom_smooth_predictor_64x16_neon(uint8_t* dst,
                                     ptrdiff_t y_stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
#define aom_smooth_predictor_64x16 aom_smooth_predictor_64x16_neon

void aom_smooth_predictor_64x32_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void aom_smooth_predictor_64x32_neon(uint8_t* dst,
                                     ptrdiff_t y_stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
#define aom_smooth_predictor_64x32 aom_smooth_predictor_64x32_neon

void aom_smooth_predictor_64x64_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void aom_smooth_predictor_64x64_neon(uint8_t* dst,
                                     ptrdiff_t y_stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
#define aom_smooth_predictor_64x64 aom_smooth_predictor_64x64_neon

void aom_smooth_predictor_8x16_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
void aom_smooth_predictor_8x16_neon(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
#define aom_smooth_predictor_8x16 aom_smooth_predictor_8x16_neon

void aom_smooth_predictor_8x32_c(uint8_t* dst,
                                 ptrdiff_t y_stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
void aom_smooth_predictor_8x32_neon(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
#define aom_smooth_predictor_8x32 aom_smooth_predictor_8x32_neon

void aom_smooth_predictor_8x4_c(uint8_t* dst,
                                ptrdiff_t y_stride,
                                const uint8_t* above,
                                const uint8_t* left);
void aom_smooth_predictor_8x4_neon(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
#define aom_smooth_predictor_8x4 aom_smooth_predictor_8x4_neon

void aom_smooth_predictor_8x8_c(uint8_t* dst,
                                ptrdiff_t y_stride,
                                const uint8_t* above,
                                const uint8_t* left);
void aom_smooth_predictor_8x8_neon(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
#define aom_smooth_predictor_8x8 aom_smooth_predictor_8x8_neon

void aom_smooth_v_predictor_16x16_c(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
void aom_smooth_v_predictor_16x16_neon(uint8_t* dst,
                                       ptrdiff_t y_stride,
                                       const uint8_t* above,
                                       const uint8_t* left);
#define aom_smooth_v_predictor_16x16 aom_smooth_v_predictor_16x16_neon

void aom_smooth_v_predictor_16x32_c(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
void aom_smooth_v_predictor_16x32_neon(uint8_t* dst,
                                       ptrdiff_t y_stride,
                                       const uint8_t* above,
                                       const uint8_t* left);
#define aom_smooth_v_predictor_16x32 aom_smooth_v_predictor_16x32_neon

void aom_smooth_v_predictor_16x4_c(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
void aom_smooth_v_predictor_16x4_neon(uint8_t* dst,
                                      ptrdiff_t y_stride,
                                      const uint8_t* above,
                                      const uint8_t* left);
#define aom_smooth_v_predictor_16x4 aom_smooth_v_predictor_16x4_neon

void aom_smooth_v_predictor_16x64_c(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
void aom_smooth_v_predictor_16x64_neon(uint8_t* dst,
                                       ptrdiff_t y_stride,
                                       const uint8_t* above,
                                       const uint8_t* left);
#define aom_smooth_v_predictor_16x64 aom_smooth_v_predictor_16x64_neon

void aom_smooth_v_predictor_16x8_c(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
void aom_smooth_v_predictor_16x8_neon(uint8_t* dst,
                                      ptrdiff_t y_stride,
                                      const uint8_t* above,
                                      const uint8_t* left);
#define aom_smooth_v_predictor_16x8 aom_smooth_v_predictor_16x8_neon

void aom_smooth_v_predictor_32x16_c(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
void aom_smooth_v_predictor_32x16_neon(uint8_t* dst,
                                       ptrdiff_t y_stride,
                                       const uint8_t* above,
                                       const uint8_t* left);
#define aom_smooth_v_predictor_32x16 aom_smooth_v_predictor_32x16_neon

void aom_smooth_v_predictor_32x32_c(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
void aom_smooth_v_predictor_32x32_neon(uint8_t* dst,
                                       ptrdiff_t y_stride,
                                       const uint8_t* above,
                                       const uint8_t* left);
#define aom_smooth_v_predictor_32x32 aom_smooth_v_predictor_32x32_neon

void aom_smooth_v_predictor_32x64_c(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
void aom_smooth_v_predictor_32x64_neon(uint8_t* dst,
                                       ptrdiff_t y_stride,
                                       const uint8_t* above,
                                       const uint8_t* left);
#define aom_smooth_v_predictor_32x64 aom_smooth_v_predictor_32x64_neon

void aom_smooth_v_predictor_32x8_c(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
void aom_smooth_v_predictor_32x8_neon(uint8_t* dst,
                                      ptrdiff_t y_stride,
                                      const uint8_t* above,
                                      const uint8_t* left);
#define aom_smooth_v_predictor_32x8 aom_smooth_v_predictor_32x8_neon

void aom_smooth_v_predictor_4x16_c(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
void aom_smooth_v_predictor_4x16_neon(uint8_t* dst,
                                      ptrdiff_t y_stride,
                                      const uint8_t* above,
                                      const uint8_t* left);
#define aom_smooth_v_predictor_4x16 aom_smooth_v_predictor_4x16_neon

void aom_smooth_v_predictor_4x4_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void aom_smooth_v_predictor_4x4_neon(uint8_t* dst,
                                     ptrdiff_t y_stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
#define aom_smooth_v_predictor_4x4 aom_smooth_v_predictor_4x4_neon

void aom_smooth_v_predictor_4x8_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void aom_smooth_v_predictor_4x8_neon(uint8_t* dst,
                                     ptrdiff_t y_stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
#define aom_smooth_v_predictor_4x8 aom_smooth_v_predictor_4x8_neon

void aom_smooth_v_predictor_64x16_c(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
void aom_smooth_v_predictor_64x16_neon(uint8_t* dst,
                                       ptrdiff_t y_stride,
                                       const uint8_t* above,
                                       const uint8_t* left);
#define aom_smooth_v_predictor_64x16 aom_smooth_v_predictor_64x16_neon

void aom_smooth_v_predictor_64x32_c(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
void aom_smooth_v_predictor_64x32_neon(uint8_t* dst,
                                       ptrdiff_t y_stride,
                                       const uint8_t* above,
                                       const uint8_t* left);
#define aom_smooth_v_predictor_64x32 aom_smooth_v_predictor_64x32_neon

void aom_smooth_v_predictor_64x64_c(uint8_t* dst,
                                    ptrdiff_t y_stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
void aom_smooth_v_predictor_64x64_neon(uint8_t* dst,
                                       ptrdiff_t y_stride,
                                       const uint8_t* above,
                                       const uint8_t* left);
#define aom_smooth_v_predictor_64x64 aom_smooth_v_predictor_64x64_neon

void aom_smooth_v_predictor_8x16_c(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
void aom_smooth_v_predictor_8x16_neon(uint8_t* dst,
                                      ptrdiff_t y_stride,
                                      const uint8_t* above,
                                      const uint8_t* left);
#define aom_smooth_v_predictor_8x16 aom_smooth_v_predictor_8x16_neon

void aom_smooth_v_predictor_8x32_c(uint8_t* dst,
                                   ptrdiff_t y_stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
void aom_smooth_v_predictor_8x32_neon(uint8_t* dst,
                                      ptrdiff_t y_stride,
                                      const uint8_t* above,
                                      const uint8_t* left);
#define aom_smooth_v_predictor_8x32 aom_smooth_v_predictor_8x32_neon

void aom_smooth_v_predictor_8x4_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void aom_smooth_v_predictor_8x4_neon(uint8_t* dst,
                                     ptrdiff_t y_stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
#define aom_smooth_v_predictor_8x4 aom_smooth_v_predictor_8x4_neon

void aom_smooth_v_predictor_8x8_c(uint8_t* dst,
                                  ptrdiff_t y_stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void aom_smooth_v_predictor_8x8_neon(uint8_t* dst,
                                     ptrdiff_t y_stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
#define aom_smooth_v_predictor_8x8 aom_smooth_v_predictor_8x8_neon

int64_t aom_sse_c(const uint8_t* a,
                  int a_stride,
                  const uint8_t* b,
                  int b_stride,
                  int width,
                  int height);
int64_t aom_sse_neon(const uint8_t* a,
                     int a_stride,
                     const uint8_t* b,
                     int b_stride,
                     int width,
                     int height);
#define aom_sse aom_sse_neon

void aom_ssim_parms_8x8_c(const uint8_t* s,
                          int sp,
                          const uint8_t* r,
                          int rp,
                          uint32_t* sum_s,
                          uint32_t* sum_r,
                          uint32_t* sum_sq_s,
                          uint32_t* sum_sq_r,
                          uint32_t* sum_sxr);
#define aom_ssim_parms_8x8 aom_ssim_parms_8x8_c

uint32_t aom_sub_pixel_avg_variance128x128_c(const uint8_t* src_ptr,
                                             int source_stride,
                                             int xoffset,
                                             int yoffset,
                                             const uint8_t* ref_ptr,
                                             int ref_stride,
                                             uint32_t* sse,
                                             const uint8_t* second_pred);
#define aom_sub_pixel_avg_variance128x128 aom_sub_pixel_avg_variance128x128_c

uint32_t aom_sub_pixel_avg_variance128x64_c(const uint8_t* src_ptr,
                                            int source_stride,
                                            int xoffset,
                                            int yoffset,
                                            const uint8_t* ref_ptr,
                                            int ref_stride,
                                            uint32_t* sse,
                                            const uint8_t* second_pred);
#define aom_sub_pixel_avg_variance128x64 aom_sub_pixel_avg_variance128x64_c

uint32_t aom_sub_pixel_avg_variance16x16_c(const uint8_t* src_ptr,
                                           int source_stride,
                                           int xoffset,
                                           int yoffset,
                                           const uint8_t* ref_ptr,
                                           int ref_stride,
                                           uint32_t* sse,
                                           const uint8_t* second_pred);
#define aom_sub_pixel_avg_variance16x16 aom_sub_pixel_avg_variance16x16_c

uint32_t aom_sub_pixel_avg_variance16x32_c(const uint8_t* src_ptr,
                                           int source_stride,
                                           int xoffset,
                                           int yoffset,
                                           const uint8_t* ref_ptr,
                                           int ref_stride,
                                           uint32_t* sse,
                                           const uint8_t* second_pred);
#define aom_sub_pixel_avg_variance16x32 aom_sub_pixel_avg_variance16x32_c

uint32_t aom_sub_pixel_avg_variance16x8_c(const uint8_t* src_ptr,
                                          int source_stride,
                                          int xoffset,
                                          int yoffset,
                                          const uint8_t* ref_ptr,
                                          int ref_stride,
                                          uint32_t* sse,
                                          const uint8_t* second_pred);
#define aom_sub_pixel_avg_variance16x8 aom_sub_pixel_avg_variance16x8_c

uint32_t aom_sub_pixel_avg_variance32x16_c(const uint8_t* src_ptr,
                                           int source_stride,
                                           int xoffset,
                                           int yoffset,
                                           const uint8_t* ref_ptr,
                                           int ref_stride,
                                           uint32_t* sse,
                                           const uint8_t* second_pred);
#define aom_sub_pixel_avg_variance32x16 aom_sub_pixel_avg_variance32x16_c

uint32_t aom_sub_pixel_avg_variance32x32_c(const uint8_t* src_ptr,
                                           int source_stride,
                                           int xoffset,
                                           int yoffset,
                                           const uint8_t* ref_ptr,
                                           int ref_stride,
                                           uint32_t* sse,
                                           const uint8_t* second_pred);
#define aom_sub_pixel_avg_variance32x32 aom_sub_pixel_avg_variance32x32_c

uint32_t aom_sub_pixel_avg_variance32x64_c(const uint8_t* src_ptr,
                                           int source_stride,
                                           int xoffset,
                                           int yoffset,
                                           const uint8_t* ref_ptr,
                                           int ref_stride,
                                           uint32_t* sse,
                                           const uint8_t* second_pred);
#define aom_sub_pixel_avg_variance32x64 aom_sub_pixel_avg_variance32x64_c

uint32_t aom_sub_pixel_avg_variance4x4_c(const uint8_t* src_ptr,
                                         int source_stride,
                                         int xoffset,
                                         int yoffset,
                                         const uint8_t* ref_ptr,
                                         int ref_stride,
                                         uint32_t* sse,
                                         const uint8_t* second_pred);
#define aom_sub_pixel_avg_variance4x4 aom_sub_pixel_avg_variance4x4_c

uint32_t aom_sub_pixel_avg_variance4x8_c(const uint8_t* src_ptr,
                                         int source_stride,
                                         int xoffset,
                                         int yoffset,
                                         const uint8_t* ref_ptr,
                                         int ref_stride,
                                         uint32_t* sse,
                                         const uint8_t* second_pred);
#define aom_sub_pixel_avg_variance4x8 aom_sub_pixel_avg_variance4x8_c

uint32_t aom_sub_pixel_avg_variance64x128_c(const uint8_t* src_ptr,
                                            int source_stride,
                                            int xoffset,
                                            int yoffset,
                                            const uint8_t* ref_ptr,
                                            int ref_stride,
                                            uint32_t* sse,
                                            const uint8_t* second_pred);
#define aom_sub_pixel_avg_variance64x128 aom_sub_pixel_avg_variance64x128_c

uint32_t aom_sub_pixel_avg_variance64x32_c(const uint8_t* src_ptr,
                                           int source_stride,
                                           int xoffset,
                                           int yoffset,
                                           const uint8_t* ref_ptr,
                                           int ref_stride,
                                           uint32_t* sse,
                                           const uint8_t* second_pred);
#define aom_sub_pixel_avg_variance64x32 aom_sub_pixel_avg_variance64x32_c

uint32_t aom_sub_pixel_avg_variance64x64_c(const uint8_t* src_ptr,
                                           int source_stride,
                                           int xoffset,
                                           int yoffset,
                                           const uint8_t* ref_ptr,
                                           int ref_stride,
                                           uint32_t* sse,
                                           const uint8_t* second_pred);
#define aom_sub_pixel_avg_variance64x64 aom_sub_pixel_avg_variance64x64_c

uint32_t aom_sub_pixel_avg_variance8x16_c(const uint8_t* src_ptr,
                                          int source_stride,
                                          int xoffset,
                                          int yoffset,
                                          const uint8_t* ref_ptr,
                                          int ref_stride,
                                          uint32_t* sse,
                                          const uint8_t* second_pred);
#define aom_sub_pixel_avg_variance8x16 aom_sub_pixel_avg_variance8x16_c

uint32_t aom_sub_pixel_avg_variance8x4_c(const uint8_t* src_ptr,
                                         int source_stride,
                                         int xoffset,
                                         int yoffset,
                                         const uint8_t* ref_ptr,
                                         int ref_stride,
                                         uint32_t* sse,
                                         const uint8_t* second_pred);
#define aom_sub_pixel_avg_variance8x4 aom_sub_pixel_avg_variance8x4_c

uint32_t aom_sub_pixel_avg_variance8x8_c(const uint8_t* src_ptr,
                                         int source_stride,
                                         int xoffset,
                                         int yoffset,
                                         const uint8_t* ref_ptr,
                                         int ref_stride,
                                         uint32_t* sse,
                                         const uint8_t* second_pred);
#define aom_sub_pixel_avg_variance8x8 aom_sub_pixel_avg_variance8x8_c

uint32_t aom_sub_pixel_variance128x128_c(const uint8_t* src_ptr,
                                         int source_stride,
                                         int xoffset,
                                         int yoffset,
                                         const uint8_t* ref_ptr,
                                         int ref_stride,
                                         uint32_t* sse);
uint32_t aom_sub_pixel_variance128x128_neon(const uint8_t* src_ptr,
                                            int source_stride,
                                            int xoffset,
                                            int yoffset,
                                            const uint8_t* ref_ptr,
                                            int ref_stride,
                                            uint32_t* sse);
#define aom_sub_pixel_variance128x128 aom_sub_pixel_variance128x128_neon

uint32_t aom_sub_pixel_variance128x64_c(const uint8_t* src_ptr,
                                        int source_stride,
                                        int xoffset,
                                        int yoffset,
                                        const uint8_t* ref_ptr,
                                        int ref_stride,
                                        uint32_t* sse);
uint32_t aom_sub_pixel_variance128x64_neon(const uint8_t* src_ptr,
                                           int source_stride,
                                           int xoffset,
                                           int yoffset,
                                           const uint8_t* ref_ptr,
                                           int ref_stride,
                                           uint32_t* sse);
#define aom_sub_pixel_variance128x64 aom_sub_pixel_variance128x64_neon

uint32_t aom_sub_pixel_variance16x16_c(const uint8_t* src_ptr,
                                       int source_stride,
                                       int xoffset,
                                       int yoffset,
                                       const uint8_t* ref_ptr,
                                       int ref_stride,
                                       uint32_t* sse);
uint32_t aom_sub_pixel_variance16x16_neon(const uint8_t* src_ptr,
                                          int source_stride,
                                          int xoffset,
                                          int yoffset,
                                          const uint8_t* ref_ptr,
                                          int ref_stride,
                                          uint32_t* sse);
#define aom_sub_pixel_variance16x16 aom_sub_pixel_variance16x16_neon

uint32_t aom_sub_pixel_variance16x32_c(const uint8_t* src_ptr,
                                       int source_stride,
                                       int xoffset,
                                       int yoffset,
                                       const uint8_t* ref_ptr,
                                       int ref_stride,
                                       uint32_t* sse);
uint32_t aom_sub_pixel_variance16x32_neon(const uint8_t* src_ptr,
                                          int source_stride,
                                          int xoffset,
                                          int yoffset,
                                          const uint8_t* ref_ptr,
                                          int ref_stride,
                                          uint32_t* sse);
#define aom_sub_pixel_variance16x32 aom_sub_pixel_variance16x32_neon

uint32_t aom_sub_pixel_variance16x8_c(const uint8_t* src_ptr,
                                      int source_stride,
                                      int xoffset,
                                      int yoffset,
                                      const uint8_t* ref_ptr,
                                      int ref_stride,
                                      uint32_t* sse);
uint32_t aom_sub_pixel_variance16x8_neon(const uint8_t* src_ptr,
                                         int source_stride,
                                         int xoffset,
                                         int yoffset,
                                         const uint8_t* ref_ptr,
                                         int ref_stride,
                                         uint32_t* sse);
#define aom_sub_pixel_variance16x8 aom_sub_pixel_variance16x8_neon

uint32_t aom_sub_pixel_variance32x16_c(const uint8_t* src_ptr,
                                       int source_stride,
                                       int xoffset,
                                       int yoffset,
                                       const uint8_t* ref_ptr,
                                       int ref_stride,
                                       uint32_t* sse);
uint32_t aom_sub_pixel_variance32x16_neon(const uint8_t* src_ptr,
                                          int source_stride,
                                          int xoffset,
                                          int yoffset,
                                          const uint8_t* ref_ptr,
                                          int ref_stride,
                                          uint32_t* sse);
#define aom_sub_pixel_variance32x16 aom_sub_pixel_variance32x16_neon

uint32_t aom_sub_pixel_variance32x32_c(const uint8_t* src_ptr,
                                       int source_stride,
                                       int xoffset,
                                       int yoffset,
                                       const uint8_t* ref_ptr,
                                       int ref_stride,
                                       uint32_t* sse);
uint32_t aom_sub_pixel_variance32x32_neon(const uint8_t* src_ptr,
                                          int source_stride,
                                          int xoffset,
                                          int yoffset,
                                          const uint8_t* ref_ptr,
                                          int ref_stride,
                                          uint32_t* sse);
#define aom_sub_pixel_variance32x32 aom_sub_pixel_variance32x32_neon

uint32_t aom_sub_pixel_variance32x64_c(const uint8_t* src_ptr,
                                       int source_stride,
                                       int xoffset,
                                       int yoffset,
                                       const uint8_t* ref_ptr,
                                       int ref_stride,
                                       uint32_t* sse);
uint32_t aom_sub_pixel_variance32x64_neon(const uint8_t* src_ptr,
                                          int source_stride,
                                          int xoffset,
                                          int yoffset,
                                          const uint8_t* ref_ptr,
                                          int ref_stride,
                                          uint32_t* sse);
#define aom_sub_pixel_variance32x64 aom_sub_pixel_variance32x64_neon

uint32_t aom_sub_pixel_variance4x4_c(const uint8_t* src_ptr,
                                     int source_stride,
                                     int xoffset,
                                     int yoffset,
                                     const uint8_t* ref_ptr,
                                     int ref_stride,
                                     uint32_t* sse);
uint32_t aom_sub_pixel_variance4x4_neon(const uint8_t* src_ptr,
                                        int source_stride,
                                        int xoffset,
                                        int yoffset,
                                        const uint8_t* ref_ptr,
                                        int ref_stride,
                                        uint32_t* sse);
#define aom_sub_pixel_variance4x4 aom_sub_pixel_variance4x4_neon

uint32_t aom_sub_pixel_variance4x8_c(const uint8_t* src_ptr,
                                     int source_stride,
                                     int xoffset,
                                     int yoffset,
                                     const uint8_t* ref_ptr,
                                     int ref_stride,
                                     uint32_t* sse);
uint32_t aom_sub_pixel_variance4x8_neon(const uint8_t* src_ptr,
                                        int source_stride,
                                        int xoffset,
                                        int yoffset,
                                        const uint8_t* ref_ptr,
                                        int ref_stride,
                                        uint32_t* sse);
#define aom_sub_pixel_variance4x8 aom_sub_pixel_variance4x8_neon

uint32_t aom_sub_pixel_variance64x128_c(const uint8_t* src_ptr,
                                        int source_stride,
                                        int xoffset,
                                        int yoffset,
                                        const uint8_t* ref_ptr,
                                        int ref_stride,
                                        uint32_t* sse);
uint32_t aom_sub_pixel_variance64x128_neon(const uint8_t* src_ptr,
                                           int source_stride,
                                           int xoffset,
                                           int yoffset,
                                           const uint8_t* ref_ptr,
                                           int ref_stride,
                                           uint32_t* sse);
#define aom_sub_pixel_variance64x128 aom_sub_pixel_variance64x128_neon

uint32_t aom_sub_pixel_variance64x32_c(const uint8_t* src_ptr,
                                       int source_stride,
                                       int xoffset,
                                       int yoffset,
                                       const uint8_t* ref_ptr,
                                       int ref_stride,
                                       uint32_t* sse);
uint32_t aom_sub_pixel_variance64x32_neon(const uint8_t* src_ptr,
                                          int source_stride,
                                          int xoffset,
                                          int yoffset,
                                          const uint8_t* ref_ptr,
                                          int ref_stride,
                                          uint32_t* sse);
#define aom_sub_pixel_variance64x32 aom_sub_pixel_variance64x32_neon

uint32_t aom_sub_pixel_variance64x64_c(const uint8_t* src_ptr,
                                       int source_stride,
                                       int xoffset,
                                       int yoffset,
                                       const uint8_t* ref_ptr,
                                       int ref_stride,
                                       uint32_t* sse);
uint32_t aom_sub_pixel_variance64x64_neon(const uint8_t* src_ptr,
                                          int source_stride,
                                          int xoffset,
                                          int yoffset,
                                          const uint8_t* ref_ptr,
                                          int ref_stride,
                                          uint32_t* sse);
#define aom_sub_pixel_variance64x64 aom_sub_pixel_variance64x64_neon

uint32_t aom_sub_pixel_variance8x16_c(const uint8_t* src_ptr,
                                      int source_stride,
                                      int xoffset,
                                      int yoffset,
                                      const uint8_t* ref_ptr,
                                      int ref_stride,
                                      uint32_t* sse);
uint32_t aom_sub_pixel_variance8x16_neon(const uint8_t* src_ptr,
                                         int source_stride,
                                         int xoffset,
                                         int yoffset,
                                         const uint8_t* ref_ptr,
                                         int ref_stride,
                                         uint32_t* sse);
#define aom_sub_pixel_variance8x16 aom_sub_pixel_variance8x16_neon

uint32_t aom_sub_pixel_variance8x4_c(const uint8_t* src_ptr,
                                     int source_stride,
                                     int xoffset,
                                     int yoffset,
                                     const uint8_t* ref_ptr,
                                     int ref_stride,
                                     uint32_t* sse);
uint32_t aom_sub_pixel_variance8x4_neon(const uint8_t* src_ptr,
                                        int source_stride,
                                        int xoffset,
                                        int yoffset,
                                        const uint8_t* ref_ptr,
                                        int ref_stride,
                                        uint32_t* sse);
#define aom_sub_pixel_variance8x4 aom_sub_pixel_variance8x4_neon

uint32_t aom_sub_pixel_variance8x8_c(const uint8_t* src_ptr,
                                     int source_stride,
                                     int xoffset,
                                     int yoffset,
                                     const uint8_t* ref_ptr,
                                     int ref_stride,
                                     uint32_t* sse);
uint32_t aom_sub_pixel_variance8x8_neon(const uint8_t* src_ptr,
                                        int source_stride,
                                        int xoffset,
                                        int yoffset,
                                        const uint8_t* ref_ptr,
                                        int ref_stride,
                                        uint32_t* sse);
#define aom_sub_pixel_variance8x8 aom_sub_pixel_variance8x8_neon

void aom_subtract_block_c(int rows,
                          int cols,
                          int16_t* diff_ptr,
                          ptrdiff_t diff_stride,
                          const uint8_t* src_ptr,
                          ptrdiff_t src_stride,
                          const uint8_t* pred_ptr,
                          ptrdiff_t pred_stride);
void aom_subtract_block_neon(int rows,
                             int cols,
                             int16_t* diff_ptr,
                             ptrdiff_t diff_stride,
                             const uint8_t* src_ptr,
                             ptrdiff_t src_stride,
                             const uint8_t* pred_ptr,
                             ptrdiff_t pred_stride);
#define aom_subtract_block aom_subtract_block_neon

uint64_t aom_sum_squares_2d_i16_c(const int16_t* src,
                                  int stride,
                                  int width,
                                  int height);
uint64_t aom_sum_squares_2d_i16_neon(const int16_t* src,
                                     int stride,
                                     int width,
                                     int height);
#define aom_sum_squares_2d_i16 aom_sum_squares_2d_i16_neon

uint64_t aom_sum_squares_i16_c(const int16_t* src, uint32_t N);
#define aom_sum_squares_i16 aom_sum_squares_i16_c

uint64_t aom_sum_sse_2d_i16_c(const int16_t* src,
                              int src_stride,
                              int width,
                              int height,
                              int* sum);
#define aom_sum_sse_2d_i16 aom_sum_sse_2d_i16_c

void aom_v_predictor_16x16_c(uint8_t* dst,
                             ptrdiff_t y_stride,
                             const uint8_t* above,
                             const uint8_t* left);
void aom_v_predictor_16x16_neon(uint8_t* dst,
                                ptrdiff_t y_stride,
                                const uint8_t* above,
                                const uint8_t* left);
#define aom_v_predictor_16x16 aom_v_predictor_16x16_neon

void aom_v_predictor_16x32_c(uint8_t* dst,
                             ptrdiff_t y_stride,
                             const uint8_t* above,
                             const uint8_t* left);
#define aom_v_predictor_16x32 aom_v_predictor_16x32_c

void aom_v_predictor_16x4_c(uint8_t* dst,
                            ptrdiff_t y_stride,
                            const uint8_t* above,
                            const uint8_t* left);
#define aom_v_predictor_16x4 aom_v_predictor_16x4_c

void aom_v_predictor_16x64_c(uint8_t* dst,
                             ptrdiff_t y_stride,
                             const uint8_t* above,
                             const uint8_t* left);
#define aom_v_predictor_16x64 aom_v_predictor_16x64_c

void aom_v_predictor_16x8_c(uint8_t* dst,
                            ptrdiff_t y_stride,
                            const uint8_t* above,
                            const uint8_t* left);
#define aom_v_predictor_16x8 aom_v_predictor_16x8_c

void aom_v_predictor_32x16_c(uint8_t* dst,
                             ptrdiff_t y_stride,
                             const uint8_t* above,
                             const uint8_t* left);
#define aom_v_predictor_32x16 aom_v_predictor_32x16_c

void aom_v_predictor_32x32_c(uint8_t* dst,
                             ptrdiff_t y_stride,
                             const uint8_t* above,
                             const uint8_t* left);
void aom_v_predictor_32x32_neon(uint8_t* dst,
                                ptrdiff_t y_stride,
                                const uint8_t* above,
                                const uint8_t* left);
#define aom_v_predictor_32x32 aom_v_predictor_32x32_neon

void aom_v_predictor_32x64_c(uint8_t* dst,
                             ptrdiff_t y_stride,
                             const uint8_t* above,
                             const uint8_t* left);
#define aom_v_predictor_32x64 aom_v_predictor_32x64_c

void aom_v_predictor_32x8_c(uint8_t* dst,
                            ptrdiff_t y_stride,
                            const uint8_t* above,
                            const uint8_t* left);
#define aom_v_predictor_32x8 aom_v_predictor_32x8_c

void aom_v_predictor_4x16_c(uint8_t* dst,
                            ptrdiff_t y_stride,
                            const uint8_t* above,
                            const uint8_t* left);
#define aom_v_predictor_4x16 aom_v_predictor_4x16_c

void aom_v_predictor_4x4_c(uint8_t* dst,
                           ptrdiff_t y_stride,
                           const uint8_t* above,
                           const uint8_t* left);
void aom_v_predictor_4x4_neon(uint8_t* dst,
                              ptrdiff_t y_stride,
                              const uint8_t* above,
                              const uint8_t* left);
#define aom_v_predictor_4x4 aom_v_predictor_4x4_neon

void aom_v_predictor_4x8_c(uint8_t* dst,
                           ptrdiff_t y_stride,
                           const uint8_t* above,
                           const uint8_t* left);
#define aom_v_predictor_4x8 aom_v_predictor_4x8_c

void aom_v_predictor_64x16_c(uint8_t* dst,
                             ptrdiff_t y_stride,
                             const uint8_t* above,
                             const uint8_t* left);
#define aom_v_predictor_64x16 aom_v_predictor_64x16_c

void aom_v_predictor_64x32_c(uint8_t* dst,
                             ptrdiff_t y_stride,
                             const uint8_t* above,
                             const uint8_t* left);
#define aom_v_predictor_64x32 aom_v_predictor_64x32_c

void aom_v_predictor_64x64_c(uint8_t* dst,
                             ptrdiff_t y_stride,
                             const uint8_t* above,
                             const uint8_t* left);
#define aom_v_predictor_64x64 aom_v_predictor_64x64_c

void aom_v_predictor_8x16_c(uint8_t* dst,
                            ptrdiff_t y_stride,
                            const uint8_t* above,
                            const uint8_t* left);
#define aom_v_predictor_8x16 aom_v_predictor_8x16_c

void aom_v_predictor_8x32_c(uint8_t* dst,
                            ptrdiff_t y_stride,
                            const uint8_t* above,
                            const uint8_t* left);
#define aom_v_predictor_8x32 aom_v_predictor_8x32_c

void aom_v_predictor_8x4_c(uint8_t* dst,
                           ptrdiff_t y_stride,
                           const uint8_t* above,
                           const uint8_t* left);
#define aom_v_predictor_8x4 aom_v_predictor_8x4_c

void aom_v_predictor_8x8_c(uint8_t* dst,
                           ptrdiff_t y_stride,
                           const uint8_t* above,
                           const uint8_t* left);
void aom_v_predictor_8x8_neon(uint8_t* dst,
                              ptrdiff_t y_stride,
                              const uint8_t* above,
                              const uint8_t* left);
#define aom_v_predictor_8x8 aom_v_predictor_8x8_neon

uint64_t aom_var_2d_u16_c(uint8_t* src, int src_stride, int width, int height);
#define aom_var_2d_u16 aom_var_2d_u16_c

uint64_t aom_var_2d_u8_c(uint8_t* src, int src_stride, int width, int height);
#define aom_var_2d_u8 aom_var_2d_u8_c

unsigned int aom_variance128x128_c(const uint8_t* src_ptr,
                                   int source_stride,
                                   const uint8_t* ref_ptr,
                                   int ref_stride,
                                   unsigned int* sse);
unsigned int aom_variance128x128_neon(const uint8_t* src_ptr,
                                      int source_stride,
                                      const uint8_t* ref_ptr,
                                      int ref_stride,
                                      unsigned int* sse);
#define aom_variance128x128 aom_variance128x128_neon

unsigned int aom_variance128x64_c(const uint8_t* src_ptr,
                                  int source_stride,
                                  const uint8_t* ref_ptr,
                                  int ref_stride,
                                  unsigned int* sse);
unsigned int aom_variance128x64_neon(const uint8_t* src_ptr,
                                     int source_stride,
                                     const uint8_t* ref_ptr,
                                     int ref_stride,
                                     unsigned int* sse);
#define aom_variance128x64 aom_variance128x64_neon

unsigned int aom_variance16x16_c(const uint8_t* src_ptr,
                                 int source_stride,
                                 const uint8_t* ref_ptr,
                                 int ref_stride,
                                 unsigned int* sse);
unsigned int aom_variance16x16_neon(const uint8_t* src_ptr,
                                    int source_stride,
                                    const uint8_t* ref_ptr,
                                    int ref_stride,
                                    unsigned int* sse);
#define aom_variance16x16 aom_variance16x16_neon

unsigned int aom_variance16x32_c(const uint8_t* src_ptr,
                                 int source_stride,
                                 const uint8_t* ref_ptr,
                                 int ref_stride,
                                 unsigned int* sse);
unsigned int aom_variance16x32_neon(const uint8_t* src_ptr,
                                    int source_stride,
                                    const uint8_t* ref_ptr,
                                    int ref_stride,
                                    unsigned int* sse);
#define aom_variance16x32 aom_variance16x32_neon

unsigned int aom_variance16x8_c(const uint8_t* src_ptr,
                                int source_stride,
                                const uint8_t* ref_ptr,
                                int ref_stride,
                                unsigned int* sse);
unsigned int aom_variance16x8_neon(const uint8_t* src_ptr,
                                   int source_stride,
                                   const uint8_t* ref_ptr,
                                   int ref_stride,
                                   unsigned int* sse);
#define aom_variance16x8 aom_variance16x8_neon

unsigned int aom_variance2x2_c(const uint8_t* src_ptr,
                               int source_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride,
                               unsigned int* sse);
#define aom_variance2x2 aom_variance2x2_c

unsigned int aom_variance2x4_c(const uint8_t* src_ptr,
                               int source_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride,
                               unsigned int* sse);
#define aom_variance2x4 aom_variance2x4_c

unsigned int aom_variance32x16_c(const uint8_t* src_ptr,
                                 int source_stride,
                                 const uint8_t* ref_ptr,
                                 int ref_stride,
                                 unsigned int* sse);
unsigned int aom_variance32x16_neon(const uint8_t* src_ptr,
                                    int source_stride,
                                    const uint8_t* ref_ptr,
                                    int ref_stride,
                                    unsigned int* sse);
#define aom_variance32x16 aom_variance32x16_neon

unsigned int aom_variance32x32_c(const uint8_t* src_ptr,
                                 int source_stride,
                                 const uint8_t* ref_ptr,
                                 int ref_stride,
                                 unsigned int* sse);
unsigned int aom_variance32x32_neon(const uint8_t* src_ptr,
                                    int source_stride,
                                    const uint8_t* ref_ptr,
                                    int ref_stride,
                                    unsigned int* sse);
#define aom_variance32x32 aom_variance32x32_neon

unsigned int aom_variance32x64_c(const uint8_t* src_ptr,
                                 int source_stride,
                                 const uint8_t* ref_ptr,
                                 int ref_stride,
                                 unsigned int* sse);
unsigned int aom_variance32x64_neon(const uint8_t* src_ptr,
                                    int source_stride,
                                    const uint8_t* ref_ptr,
                                    int ref_stride,
                                    unsigned int* sse);
#define aom_variance32x64 aom_variance32x64_neon

unsigned int aom_variance4x2_c(const uint8_t* src_ptr,
                               int source_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride,
                               unsigned int* sse);
#define aom_variance4x2 aom_variance4x2_c

unsigned int aom_variance4x4_c(const uint8_t* src_ptr,
                               int source_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride,
                               unsigned int* sse);
unsigned int aom_variance4x4_neon(const uint8_t* src_ptr,
                                  int source_stride,
                                  const uint8_t* ref_ptr,
                                  int ref_stride,
                                  unsigned int* sse);
#define aom_variance4x4 aom_variance4x4_neon

unsigned int aom_variance4x8_c(const uint8_t* src_ptr,
                               int source_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride,
                               unsigned int* sse);
unsigned int aom_variance4x8_neon(const uint8_t* src_ptr,
                                  int source_stride,
                                  const uint8_t* ref_ptr,
                                  int ref_stride,
                                  unsigned int* sse);
#define aom_variance4x8 aom_variance4x8_neon

unsigned int aom_variance64x128_c(const uint8_t* src_ptr,
                                  int source_stride,
                                  const uint8_t* ref_ptr,
                                  int ref_stride,
                                  unsigned int* sse);
unsigned int aom_variance64x128_neon(const uint8_t* src_ptr,
                                     int source_stride,
                                     const uint8_t* ref_ptr,
                                     int ref_stride,
                                     unsigned int* sse);
#define aom_variance64x128 aom_variance64x128_neon

unsigned int aom_variance64x32_c(const uint8_t* src_ptr,
                                 int source_stride,
                                 const uint8_t* ref_ptr,
                                 int ref_stride,
                                 unsigned int* sse);
unsigned int aom_variance64x32_neon(const uint8_t* src_ptr,
                                    int source_stride,
                                    const uint8_t* ref_ptr,
                                    int ref_stride,
                                    unsigned int* sse);
#define aom_variance64x32 aom_variance64x32_neon

unsigned int aom_variance64x64_c(const uint8_t* src_ptr,
                                 int source_stride,
                                 const uint8_t* ref_ptr,
                                 int ref_stride,
                                 unsigned int* sse);
unsigned int aom_variance64x64_neon(const uint8_t* src_ptr,
                                    int source_stride,
                                    const uint8_t* ref_ptr,
                                    int ref_stride,
                                    unsigned int* sse);
#define aom_variance64x64 aom_variance64x64_neon

unsigned int aom_variance8x16_c(const uint8_t* src_ptr,
                                int source_stride,
                                const uint8_t* ref_ptr,
                                int ref_stride,
                                unsigned int* sse);
unsigned int aom_variance8x16_neon(const uint8_t* src_ptr,
                                   int source_stride,
                                   const uint8_t* ref_ptr,
                                   int ref_stride,
                                   unsigned int* sse);
#define aom_variance8x16 aom_variance8x16_neon

unsigned int aom_variance8x4_c(const uint8_t* src_ptr,
                               int source_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride,
                               unsigned int* sse);
unsigned int aom_variance8x4_neon(const uint8_t* src_ptr,
                                  int source_stride,
                                  const uint8_t* ref_ptr,
                                  int ref_stride,
                                  unsigned int* sse);
#define aom_variance8x4 aom_variance8x4_neon

unsigned int aom_variance8x8_c(const uint8_t* src_ptr,
                               int source_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride,
                               unsigned int* sse);
unsigned int aom_variance8x8_neon(const uint8_t* src_ptr,
                                  int source_stride,
                                  const uint8_t* ref_ptr,
                                  int ref_stride,
                                  unsigned int* sse);
#define aom_variance8x8 aom_variance8x8_neon

int aom_vector_var_c(const int16_t* ref, const int16_t* src, const int bwl);
int aom_vector_var_neon(const int16_t* ref, const int16_t* src, const int bwl);
#define aom_vector_var aom_vector_var_neon

void aom_dsp_rtcd(void);

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
