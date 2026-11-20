/*
  Copyright (c) 2013 Julien Pommier.

  Small test & bench for PFFFT, comparing its performance with the scalar FFTPACK, FFTW, Intel MKL, and Apple vDSP

  How to build: 

  on linux, with fftw3:
  gcc -o test_pffft -DHAVE_FFTW -msse -mfpmath=sse -O3 -Wall -W pffft.c test_pffft.c fftpack.c -L/usr/local/lib -I/usr/local/include/ -lfftw3f -lm

  on macos, without fftw3:
  clang -o test_pffft -DHAVE_VECLIB -O3 -Wall -W pffft.c test_pffft.c fftpack.c -L/usr/local/lib -I/usr/local/include/ -framework Accelerate

  on macos, with fftw3:
  clang -o test_pffft -DHAVE_FFTW -DHAVE_VECLIB -O3 -Wall -W pffft.c test_pffft.c fftpack.c -L/usr/local/lib -I/usr/local/include/ -lfftw3f -framework Accelerate

  on macos, with fftw3 and Intel MKL:
  clang -o test_pffft -I /opt/intel/mkl/include -DHAVE_FFTW -DHAVE_VECLIB -DHAVE_MKL  -O3 -Wall -W pffft.c test_pffft.c fftpack.c -L/usr/local/lib -I/usr/local/include/ -lfftw3f -framework Accelerate /opt/intel/mkl/lib/libmkl_{intel_lp64,sequential,core}.a

  on windows, with visual c++:
  cl /Ox -D_USE_MATH_DEFINES /arch:SSE test_pffft.c pffft.c fftpack.c
  
  build without SIMD instructions:
  gcc -o test_pffft -DPFFFT_SIMD_DISABLE -O3 -Wall -W pffft.c test_pffft.c fftpack.c -lm

 */

// define this to test large FFT size (2^24, 2^25..) (test is a bit slower, of course)
// #define TEST_LARGE_FFTS

#include "pffft.h"
#include "fftpack.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

#ifdef HAVE_SYS_TIMES
#  include <sys/times.h>
#  include <unistd.h>
#endif

#ifdef HAVE_VECLIB
#  include <Accelerate/Accelerate.h>
#endif

#ifdef HAVE_FFTW
#  include <fftw3.h>
#endif

#ifdef HAVE_MKL
#  include <mkl_dfti.h>
#endif

#define MAX_OF(x,y) ((x)>(y)?(x):(y))

double frand() {
  return rand()/(double)RAND_MAX;
}

#if defined(HAVE_SYS_TIMES)
  inline double uclock_sec(void) {
    static double ttclk = 0.;
    if (ttclk == 0.) ttclk = sysconf(_SC_CLK_TCK);
    struct tms t; return ((double)times(&t)) / ttclk;
  }
# else
  double uclock_sec(void)
{ return (double)clock()/(double)CLOCKS_PER_SEC; }
#endif

float norm_inf_rel(float *v, float *w, int N) {
  float max_w = 0, max_diff = 0;
  int k;
  for (k=0; k < N; ++k) {
    max_w = MAX_OF(max_w, fabs(w[k]));
    max_diff = MAX_OF(max_diff, fabs(w[k] - v[k]));
  }
  assert(max_w > 0);
  return max_diff / max_w;
}

size_t FFTPACK_IFAC_MAX_SIZE=25; // room for small factor decompositon of fftsize, you don't want this to overflow..

/* compare results with the regular fftpack */
void pffft_validate_N(int N, int cplx) {
  int Nfloat = N*(cplx?2:1);
  int Nbytes = Nfloat * sizeof(float);
  float *ref, *ref2, *in, *out, *tmp, *tmp2, *scratch;
  PFFFT_Setup *s = NULL; 
  int pass;

  assert(Nbytes > 0);
  s = pffft_new_setup(N, cplx ? PFFFT_COMPLEX : PFFFT_REAL);

  if (!s) { printf("Skipping N=%d, not supported\n", N); return; }
  ref = pffft_aligned_malloc(Nbytes);
  ref2 = pffft_aligned_malloc(Nbytes);
  in = pffft_aligned_malloc(Nbytes);
  out = pffft_aligned_malloc(Nbytes);
  tmp = pffft_aligned_malloc(Nbytes);
  tmp2 = pffft_aligned_malloc(Nbytes);
  if (N < 2000) {
    scratch = 0;
  } else {
    scratch = pffft_aligned_malloc(Nbytes);
  }

  for (pass=0; pass < 2; ++pass) {
    float ref_max = 0;
    int k;
    //printf("N=%d pass=%d cplx=%d\n", N, pass, cplx);
    // compute reference solution with FFTPACK
    if (pass == 0) {
      // compute forward transform with fftpack
      float *wrk = malloc(2*Nbytes+FFTPACK_IFAC_MAX_SIZE*sizeof(float));
      for (k=0; k < Nfloat; ++k) {
        ref[k] = in[k] = frand()*2-1;
        out[k] = 1e30;
      }
      if (!cplx) {
        rffti(N, wrk);
        rfftf(N, ref, wrk);
        memcpy(ref2, ref, Nbytes);
        rfftb(N, ref2, wrk);
        // use our ordering for real ffts instead of the one of fftpack
        {
          float refN=ref[N-1];
          for (k=N-2; k >= 1; --k) ref[k+1] = ref[k];
          ref[1] = refN;
        }
      } else {
        cffti(N, wrk);
        cfftf(N, ref, wrk);
        memcpy(ref2, ref, Nbytes);
        cfftb(N, ref2, wrk);
      }

      for (k = 0; k < Nfloat; ++k) {
        ref2[k] /= N;
      }
      float fftpack_back_and_forth_error = norm_inf_rel(ref2, in, Nfloat);
      assert(fftpack_back_and_forth_error < 1e-3);

      free(wrk);


    }

    for (k = 0; k < Nfloat; ++k) ref_max = MAX_OF(ref_max, fabs(ref[k]));

      
    // pass 0 : non canonical ordering of transform coefficients  
    if (pass == 0) {
      // test forward transform, with different input / output
      pffft_transform(s, in, tmp, scratch, PFFFT_FORWARD);
      memcpy(tmp2, tmp, Nbytes);
      memcpy(tmp, in, Nbytes);
      pffft_transform(s, tmp, tmp, scratch, PFFFT_FORWARD);
      for (k = 0; k < Nfloat; ++k) {
        assert(tmp2[k] == tmp[k]);
      }

      // test reordering
      pffft_zreorder(s, tmp, out, PFFFT_FORWARD);
      pffft_zreorder(s, out, tmp, PFFFT_BACKWARD);
      for (k = 0; k < Nfloat; ++k) {
        assert(tmp2[k] == tmp[k]);
      }
      pffft_zreorder(s, tmp, out, PFFFT_FORWARD);
    } else {
      // pass 1 : canonical ordering of transform coeffs.
      pffft_transform_ordered(s, in, tmp, scratch, PFFFT_FORWARD);
      memcpy(tmp2, tmp, Nbytes);
      memcpy(tmp, in, Nbytes);
      pffft_transform_ordered(s, tmp, tmp, scratch, PFFFT_FORWARD);
      for (k = 0; k < Nfloat; ++k) {
        assert(tmp2[k] == tmp[k]);
      }
      memcpy(out, tmp, Nbytes);
    }

    {
      // error for the forward transform when compared with fftpack
      float max_forward_transform_error = norm_inf_rel(out, ref, Nfloat);
      if (!(max_forward_transform_error < 1e-3)) {
        printf("%s forward PFFFT mismatch found for N=%d relative error=%g\n",
               (cplx?"CPLX":"REAL"), N, max_forward_transform_error);
        exit(1);
      }

      if (pass == 0) pffft_transform(s, tmp, out, scratch, PFFFT_BACKWARD);
      else   pffft_transform_ordered(s, tmp, out, scratch, PFFFT_BACKWARD);
      memcpy(tmp2, out, Nbytes);
      memcpy(out, tmp, Nbytes);
      if (pass == 0) pffft_transform(s, out, out, scratch, PFFFT_BACKWARD);
      else   pffft_transform_ordered(s, out, out, scratch, PFFFT_BACKWARD);
      for (k = 0; k < Nfloat; ++k) {
        assert(tmp2[k] == out[k]);
        out[k] *= 1.f/N;
      }

      // error when transformed back to the original vector
      float max_final_error_rel = norm_inf_rel(out, in, Nfloat);
      //printf("  N=%d pass=%d max_forward_transform_error=%g max_final_error_rel=%g\n", N, pass, max_forward_transform_error, max_final_error_rel);
      if (max_final_error_rel > 1e-3) {
        printf("pass=%d, %s IFFFT does not match for N=%d, relative error=%g\n",
               pass, (cplx?"CPLX":"REAL"), N, max_final_error_rel); break;
        exit(1);
      }
    }

    // quick test of the circular convolution in fft domain
    {
      float conv_err = 0, conv_max = 0;

      pffft_zreorder(s, ref, tmp, PFFFT_FORWARD);
      memset(out, 0, Nbytes);
      pffft_zconvolve_accumulate(s, ref, ref, out, 1.0);
      pffft_zreorder(s, out, tmp2, PFFFT_FORWARD);
      
      for (k=0; k < Nfloat; k += 2) {
        float ar = tmp[k], ai=tmp[k+1];
        if (cplx || k > 0) {
          tmp[k] = ar*ar - ai*ai;
          tmp[k+1] = 2*ar*ai;
        } else {
          tmp[0] = ar*ar;
          tmp[1] = ai*ai;
        }
      }
      
      for (k=0; k < Nfloat; ++k) {
        float d = fabs(tmp[k] - tmp2[k]), e = fabs(tmp[k]);
        if (d > conv_err) conv_err = d;
        if (e > conv_max) conv_max = e;
      }
      if (conv_err > 1e-5*conv_max) {
        printf("zconvolve error ? %g %g\n", conv_err, conv_max); exit(1);
      }
    }

  }

  printf("%s PFFFT is OK for N=%d\n", (cplx?"CPLX":"REAL"), N); fflush(stdout);
  
  pffft_destroy_setup(s);
  pffft_aligned_free(ref);
  pffft_aligned_free(in);
  pffft_aligned_free(out);
  pffft_aligned_free(tmp);
  pffft_aligned_free(tmp2);
}

void pffft_validate(int cplx) {
  static int Ntest[] = { 16, 32, 64, 96, 128, 160, 192, 256, 288, 384, 5*96, 512, 576, 5*128, 800, 864, 1024, 2048, 2592, 4000, 4096, 12000, 36864, 0 };
#ifdef TEST_LARGE_FFTS
  static int Ntest_large[] = { 4000000, 7558272, 1600000, 20000000, 47185920, 2<<24, 2<<25, 0};
#endif
  int k;

  for (k = 0; Ntest[k]; ++k) {
    int N = Ntest[k];
    if (N == 16 && !cplx) continue;
    pffft_validate_N(N, cplx);
  }
#ifdef TEST_LARGE_FFTS
  for (k = 0; Ntest_large[k]; ++k) {
    int N = Ntest_large[k];
    pffft_validate_N(N, cplx);
  }
#endif
}

int array_output_format = 1;

void show_output(const char *name, int N, int cplx, float flops, float t0, float t1, int max_iter) {
  float mflops = flops/1e6/(t1 - t0 + 1e-16);
  if (array_output_format) {
    if (flops != -1) {
      printf("|%9.0f   ", mflops);
    } else printf("|      n/a   ");
  } else {
    if (flops != -1) {
      printf("N=%5d, %s %16s : %6.0f MFlops [t=%6.0f ns, %d runs]\n", N, (cplx?"CPLX":"REAL"), name, mflops, (t1-t0)/2/max_iter * 1e9, max_iter);
    }
  }
  fflush(stdout);
}

void benchmark_ffts(int N, int cplx) {
  int Nfloat = (cplx ? N*2 : N);
  int Nbytes = Nfloat * sizeof(float);
  float *X = pffft_aligned_malloc(Nbytes), *Y = pffft_aligned_malloc(Nbytes), *Z = pffft_aligned_malloc(Nbytes);

  double t0, t1, flops;

  int k;
  int max_iter = 5120000/N*4;
#if defined(__arm__) || defined(__arm64__) || defined(__aarch64__)
  max_iter /= 8;
#endif
  if (max_iter == 0) max_iter = 1;
  int iter;

  for (k = 0; k < Nfloat; ++k) {
    X[k] = 0; //sqrtf(k+1);
  }

  // FFTPack benchmark
  {
    float *wrk = malloc(2*Nbytes + FFTPACK_IFAC_MAX_SIZE*sizeof(float));
    int max_iter_ = max_iter/pffft_simd_size(); if (max_iter_ == 0) max_iter_ = 1;
    if (cplx) cffti(N, wrk);
    else      rffti(N, wrk);
    t0 = uclock_sec();  
    for (iter = 0; iter < max_iter_; ++iter) {
      if (cplx) {
        cfftf(N, X, wrk);
        cfftb(N, X, wrk);
      } else {
        rfftf(N, X, wrk);
        rfftb(N, X, wrk);
      }
    }
    t1 = uclock_sec();
    free(wrk);
    
    flops = (max_iter_*2) * ((cplx ? 5 : 2.5)*N*log((double)N)/M_LN2); // see http://www.fftw.org/speed/method.html
    show_output("FFTPack", N, cplx, flops, t0, t1, max_iter_);
  }

#ifdef HAVE_VECLIB
  int log2N = (int)(log(N)/log(2) + 0.5f);
  if (N == (1<<log2N)) {
    FFTSetup setup;

    setup = vDSP_create_fftsetup(log2N, FFT_RADIX2);
    DSPSplitComplex zsamples;
    zsamples.realp = &X[0];
    zsamples.imagp = &X[Nfloat/2];
    t0 = uclock_sec();  
    for (iter = 0; iter < max_iter; ++iter) {
      if (cplx) {
        vDSP_fft_zip(setup, &zsamples, 1, log2N, kFFTDirection_Forward);
        vDSP_fft_zip(setup, &zsamples, 1, log2N, kFFTDirection_Inverse);
      } else {
        vDSP_fft_zrip(setup, &zsamples, 1, log2N, kFFTDirection_Forward); 
        vDSP_fft_zrip(setup, &zsamples, 1, log2N, kFFTDirection_Inverse);
      }
    }
    t1 = uclock_sec();
    vDSP_destroy_fftsetup(setup);

    flops = (max_iter*2) * ((cplx ? 5 : 2.5)*N*log((double)N)/M_LN2); // see http://www.fftw.org/speed/method.html
    show_output("vDSP", N, cplx, flops, t0, t1, max_iter);
  } else {
    show_output("vDSP", N, cplx, -1, -1, -1, -1);
  }
#endif

#ifdef HAVE_MKL
  {
    DFTI_DESCRIPTOR_HANDLE fft_handle;
    if (DftiCreateDescriptor(&fft_handle, DFTI_SINGLE, (cplx ? DFTI_COMPLEX : DFTI_REAL), 1, N) == 0) {
      assert(DftiSetValue(fft_handle, DFTI_PLACEMENT, DFTI_INPLACE) == 0);
      assert(DftiCommitDescriptor(fft_handle) == 0);

      t0 = uclock_sec();
      for (iter = 0; iter < max_iter; ++iter) {
        DftiComputeForward(fft_handle, &X[0]);
        DftiComputeBackward(fft_handle, &X[0]);
      }
      t1 = uclock_sec();


      DftiFreeDescriptor(&fft_handle);
      flops = (max_iter*2) * ((cplx ? 5 : 2.5)*N*log((double)N)/M_LN2); // see http://www.fftw.org/speed/method.html
      show_output("MKL ", N, cplx, flops, t0, t1, max_iter);
    } else {
      show_output(" MKL", N, cplx, -1, -1, -1, -1);
    }
  }
#endif

#ifdef HAVE_FFTW
  {
    fftwf_plan planf, planb;
    fftw_complex *in = (fftw_complex*) fftwf_malloc(sizeof(fftw_complex) * N);
    fftw_complex *out = (fftw_complex*) fftwf_malloc(sizeof(fftw_complex) * N);
    memset(in, 0, sizeof(fftw_complex) * N);
    int flags = (N < 40000 ? FFTW_MEASURE : FFTW_ESTIMATE);  // measure takes a lot of time on largest ffts
    //int flags = FFTW_ESTIMATE;
    if (cplx) {
      planf = fftwf_plan_dft_1d(N, (fftwf_complex*)in, (fftwf_complex*)out, FFTW_FORWARD, flags);
      planb = fftwf_plan_dft_1d(N, (fftwf_complex*)in, (fftwf_complex*)out, FFTW_BACKWARD, flags);
    } else {
      planf = fftwf_plan_dft_r2c_1d(N, (float*)in, (fftwf_complex*)out, flags);
      planb = fftwf_plan_dft_c2r_1d(N, (fftwf_complex*)in, (float*)out, flags);
    }

    t0 = uclock_sec();  
    for (iter = 0; iter < max_iter; ++iter) {
      fftwf_execute(planf);
      fftwf_execute(planb);
    }
    t1 = uclock_sec();

    fftwf_destroy_plan(planf);
    fftwf_destroy_plan(planb);
    fftwf_free(in); fftwf_free(out);

    flops = (max_iter*2) * ((cplx ? 5 : 2.5)*N*log((double)N)/M_LN2); // see http://www.fftw.org/speed/method.html
    show_output((flags == FFTW_MEASURE ? "FFTW (meas.)" : " FFTW (estim)"), N, cplx, flops, t0, t1, max_iter);
  }
#endif  

  // PFFFT benchmark
  {
    PFFFT_Setup *s = pffft_new_setup(N, cplx ? PFFFT_COMPLEX : PFFFT_REAL);
    if (s) {
      t0 = uclock_sec();  
      for (iter = 0; iter < max_iter; ++iter) {
        pffft_transform(s, X, Z, Y, PFFFT_FORWARD);
        pffft_transform(s, X, Z, Y, PFFFT_BACKWARD);
      }
      t1 = uclock_sec();
      pffft_destroy_setup(s);
    
      flops = (max_iter*2) * ((cplx ? 5 : 2.5)*N*log((double)N)/M_LN2); // see http://www.fftw.org/speed/method.html
      show_output("PFFFT", N, cplx, flops, t0, t1, max_iter);
    }
  }

  if (!array_output_format) {
    printf("--\n");
  }

  pffft_aligned_free(X);
  pffft_aligned_free(Y);
  pffft_aligned_free(Z);
}

int main(int argc, char **argv) {
  int Nvalues[] = { 64, 96, 128, 160, 192, 256, 384, 5*96, 512, 5*128, 3*256, 800, 1024, 2048, 2400, 4096, 8192, 9*1024, 16384, 32768, 256*1024, 1024*1024, 2<<24, 2<<25, -1 };
  int Nmax = 1014*1024;
  int i;

  if (argc > 1 && strcmp(argv[1], "--no-array-format") == 0) {
    array_output_format = 0;
  }

#ifdef TEST_LARGE_FFTS
  Nmax = 2000000000;
#endif

#ifndef PFFFT_SIMD_DISABLE
  validate_pffft_simd();
#endif
  pffft_validate(1);
  pffft_validate(0);
  if (!array_output_format) {
    // display a nice markdown array
    for (i=0; Nvalues[i] > 0; ++i) {
      if (Nvalues[i] < Nmax) {
        benchmark_ffts(Nvalues[i], 0 /* real fft */);
      }
    }
    for (i=0; Nvalues[i] > 0; ++i) {
      if (Nvalues[i] < Nmax) {
        benchmark_ffts(Nvalues[i], 1 /* cplx fft */);
      }
    }
  } else {
    int columns = 0;
    printf("| input len  "); ++columns;
    printf("|real FFTPack"); ++columns;
#ifdef HAVE_VECLIB
    printf("|  real vDSP "); ++columns;
#endif
#ifdef HAVE_MKL
    printf("|  real MKL  "); ++columns;
#endif
#ifdef HAVE_FFTW
    printf("|  real FFTW "); ++columns;
#endif
    printf("| real PFFFT "); ++columns;

    printf("|cplx FFTPack"); ++columns;
#ifdef HAVE_VECLIB
    printf("|  cplx vDSP "); ++columns;
#endif
#ifdef HAVE_MKL
    printf("|  cplx MKL  "); ++columns;
#endif
#ifdef HAVE_FFTW
    printf("|  cplx FFTW "); ++columns;
#endif
    printf("| cplx PFFFT |\n"); ++columns;
    for (i=0; i < columns; ++i) {
      printf("|-----------:");
    }
    printf("|\n");

    for (i=0; Nvalues[i] > 0; ++i) {
      if (Nvalues[i] < Nmax) {
        printf("|%9d   ", Nvalues[i]);
        benchmark_ffts(Nvalues[i], 0);
        //printf("| ");
        benchmark_ffts(Nvalues[i], 1);
        printf("|\n");
      }
    }
    printf(" (numbers are given in MFlops)\n");
  }


  return 0;
}
