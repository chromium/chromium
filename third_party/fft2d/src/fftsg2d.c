/*
Fast Fourier/Cosine/Sine Transform
    dimension   :two
    data length :power of 2
    decimation  :frequency
    radix       :split-radix, row-column
    data        :inplace
    table       :use
functions
    cdft2d: Complex Discrete Fourier Transform
    rdft2d: Real Discrete Fourier Transform
    ddct2d: Discrete Cosine Transform
    ddst2d: Discrete Sine Transform
function prototypes
    void cdft2d(int, int, int, double **, double *, int *, double *);
    void rdft2d(int, int, int, double **, double *, int *, double *);
    void rdft2dsort(int, int, int, double **);
    void ddct2d(int, int, int, double **, double *, int *, double *);
    void ddst2d(int, int, int, double **, double *, int *, double *);
necessary package
    fftsg.c  : 1D-FFT package
macro definitions
    USE_FFT2D_PTHREADS : default=not defined
        FFT2D_MAX_THREADS     : must be 2^N, default=4
        FFT2D_THREADS_BEGIN_N : default=65536
    USE_FFT2D_WINTHREADS : default=not defined
        FFT2D_MAX_THREADS     : must be 2^N, default=4
        FFT2D_THREADS_BEGIN_N : default=131072


-------- Complex DFT (Discrete Fourier Transform) --------
    [definition]
        <case1>
            X[k1][k2] = sum_j1=0^n1-1 sum_j2=0^n2-1 x[j1][j2] * 
                            exp(2*pi*i*j1*k1/n1) * 
                            exp(2*pi*i*j2*k2/n2), 0<=k1<n1, 0<=k2<n2
        <case2>
            X[k1][k2] = sum_j1=0^n1-1 sum_j2=0^n2-1 x[j1][j2] * 
                            exp(-2*pi*i*j1*k1/n1) * 
                            exp(-2*pi*i*j2*k2/n2), 0<=k1<n1, 0<=k2<n2
        (notes: sum_j=0^n-1 is a summation from j=0 to n-1)
    [usage]
        <case1>
            ip[0] = 0; // first time only
            cdft2d(n1, 2*n2, 1, a, t, ip, w);
        <case2>
            ip[0] = 0; // first time only
            cdft2d(n1, 2*n2, -1, a, t, ip, w);
    [parameters]
        n1     :data length (int)
                n1 >= 1, n1 = power of 2
        2*n2   :data length (int)
                n2 >= 1, n2 = power of 2
        a[0...n1-1][0...2*n2-1]
               :input/output data (double **)
                input data
                    a[j1][2*j2] = Re(x[j1][j2]), 
                    a[j1][2*j2+1] = Im(x[j1][j2]), 
                    0<=j1<n1, 0<=j2<n2
                output data
                    a[k1][2*k2] = Re(X[k1][k2]), 
                    a[k1][2*k2+1] = Im(X[k1][k2]), 
                    0<=k1<n1, 0<=k2<n2
        t[0...*]
               :work area (double *)
                length of t >= 8*n1,                   if single thread, 
                length of t >= 8*n1*FFT2D_MAX_THREADS, if multi threads, 
                t is dynamically allocated, if t == NULL.
        ip[0...*]
               :work area for bit reversal (int *)
                length of ip >= 2+sqrt(n)
                (n = max(n1, n2))
                ip[0],ip[1] are pointers of the cos/sin table.
        w[0...*]
               :cos/sin table (double *)
                length of w >= max(n1/2, n2/2)
                w[],ip[] are initialized if ip[0] == 0.
    [remark]
        Inverse of 
            cdft2d(n1, 2*n2, -1, a, t, ip, w);
        is 
            cdft2d(n1, 2*n2, 1, a, t, ip, w);
            for (j1 = 0; j1 <= n1 - 1; j1++) {
                for (j2 = 0; j2 <= 2 * n2 - 1; j2++) {
                    a[j1][j2] *= 1.0 / n1 / n2;
                }
            }
        .


-------- Real DFT / Inverse of Real DFT --------
    [definition]
        <case1> RDFT
            R[k1][k2] = sum_j1=0^n1-1 sum_j2=0^n2-1 a[j1][j2] * 
                            cos(2*pi*j1*k1/n1 + 2*pi*j2*k2/n2), 
                            0<=k1<n1, 0<=k2<n2
            I[k1][k2] = sum_j1=0^n1-1 sum_j2=0^n2-1 a[j1][j2] * 
                            sin(2*pi*j1*k1/n1 + 2*pi*j2*k2/n2), 
                            0<=k1<n1, 0<=k2<n2
        <case2> IRDFT (excluding scale)
            a[k1][k2] = (1/2) * sum_j1=0^n1-1 sum_j2=0^n2-1
                            (R[j1][j2] * 
                            cos(2*pi*j1*k1/n1 + 2*pi*j2*k2/n2) + 
                            I[j1][j2] * 
                            sin(2*pi*j1*k1/n1 + 2*pi*j2*k2/n2)), 
                            0<=k1<n1, 0<=k2<n2
        (notes: R[n1-k1][n2-k2] = R[k1][k2], 
                I[n1-k1][n2-k2] = -I[k1][k2], 
                R[n1-k1][0] = R[k1][0], 
                I[n1-k1][0] = -I[k1][0], 
                R[0][n2-k2] = R[0][k2], 
                I[0][n2-k2] = -I[0][k2], 
                0<k1<n1, 0<k2<n2)
    [usage]
        <case1>
            ip[0] = 0; // first time only
            rdft2d(n1, n2, 1, a, t, ip, w);
        <case2>
            ip[0] = 0; // first time only
            rdft2d(n1, n2, -1, a, t, ip, w);
    [parameters]
        n1     :data length (int)
                n1 >= 2, n1 = power of 2
        n2     :data length (int)
                n2 >= 2, n2 = power of 2
        a[0...n1-1][0...n2-1]
               :input/output data (double **)
                <case1>
                    output data
                        a[k1][2*k2] = R[k1][k2] = R[n1-k1][n2-k2], 
                        a[k1][2*k2+1] = I[k1][k2] = -I[n1-k1][n2-k2], 
                            0<k1<n1, 0<k2<n2/2, 
                        a[0][2*k2] = R[0][k2] = R[0][n2-k2], 
                        a[0][2*k2+1] = I[0][k2] = -I[0][n2-k2], 
                            0<k2<n2/2, 
                        a[k1][0] = R[k1][0] = R[n1-k1][0], 
                        a[k1][1] = I[k1][0] = -I[n1-k1][0], 
                        a[n1-k1][1] = R[k1][n2/2] = R[n1-k1][n2/2], 
                        a[n1-k1][0] = -I[k1][n2/2] = I[n1-k1][n2/2], 
                            0<k1<n1/2, 
                        a[0][0] = R[0][0], 
                        a[0][1] = R[0][n2/2], 
                        a[n1/2][0] = R[n1/2][0], 
                        a[n1/2][1] = R[n1/2][n2/2]
                <case2>
                    input data
                        a[j1][2*j2] = R[j1][j2] = R[n1-j1][n2-j2], 
                        a[j1][2*j2+1] = I[j1][j2] = -I[n1-j1][n2-j2], 
                            0<j1<n1, 0<j2<n2/2, 
                        a[0][2*j2] = R[0][j2] = R[0][n2-j2], 
                        a[0][2*j2+1] = I[0][j2] = -I[0][n2-j2], 
                            0<j2<n2/2, 
                        a[j1][0] = R[j1][0] = R[n1-j1][0], 
                        a[j1][1] = I[j1][0] = -I[n1-j1][0], 
                        a[n1-j1][1] = R[j1][n2/2] = R[n1-j1][n2/2], 
                        a[n1-j1][0] = -I[j1][n2/2] = I[n1-j1][n2/2], 
                            0<j1<n1/2, 
                        a[0][0] = R[0][0], 
                        a[0][1] = R[0][n2/2], 
                        a[n1/2][0] = R[n1/2][0], 
                        a[n1/2][1] = R[n1/2][n2/2]
                ---- output ordering ----
                    rdft2d(n1, n2, 1, a, t, ip, w);
                    rdft2dsort(n1, n2, 1, a);
                    // stored data is a[0...n1-1][0...n2+1]:
                    // a[k1][2*k2] = R[k1][k2], 
                    // a[k1][2*k2+1] = I[k1][k2], 
                    // 0<=k1<n1, 0<=k2<=n2/2.
                    // the stored data is larger than the input data!
                ---- input ordering ----
                    rdft2dsort(n1, n2, -1, a);
                    rdft2d(n1, n2, -1, a, t, ip, w);
        t[0...*]
               :work area (double *)
                length of t >= 8*n1,                   if single thread, 
                length of t >= 8*n1*FFT2D_MAX_THREADS, if multi threads, 
                t is dynamically allocated, if t == NULL.
        ip[0...*]
               :work area for bit reversal (int *)
                length of ip >= 2+sqrt(n)
                (n = max(n1, n2/2))
                ip[0],ip[1] are pointers of the cos/sin table.
        w[0...*]
               :cos/sin table (double *)
                length of w >= max(n1/2, n2/4) + n2/4
                w[],ip[] are initialized if ip[0] == 0.
    [remark]
        Inverse of 
            rdft2d(n1, n2, 1, a, t, ip, w);
        is 
            rdft2d(n1, n2, -1, a, t, ip, w);
            for (j1 = 0; j1 <= n1 - 1; j1++) {
                for (j2 = 0; j2 <= n2 - 1; j2++) {
                    a[j1][j2] *= 2.0 / n1 / n2;
                }
            }
        .


-------- DCT (Discrete Cosine Transform) / Inverse of DCT --------
    [definition]
        <case1> IDCT (excluding scale)
            C[k1][k2] = sum_j1=0^n1-1 sum_j2=0^n2-1 a[j1][j2] * 
                            cos(pi*j1*(k1+1/2)/n1) * 
                            cos(pi*j2*(k2+1/2)/n2), 
                            0<=k1<n1, 0<=k2<n2
        <case2> DCT
            C[k1][k2] = sum_j1=0^n1-1 sum_j2=0^n2-1 a[j1][j2] * 
                            cos(pi*(j1+1/2)*k1/n1) * 
                            cos(pi*(j2+1/2)*k2/n2), 
                            0<=k1<n1, 0<=k2<n2
    [usage]
        <case1>
            ip[0] = 0; // first time only
            ddct2d(n1, n2, 1, a, t, ip, w);
        <case2>
            ip[0] = 0; // first time only
            ddct2d(n1, n2, -1, a, t, ip, w);
    [parameters]
        n1     :data length (int)
                n1 >= 2, n1 = power of 2
        n2     :data length (int)
                n2 >= 2, n2 = power of 2
        a[0...n1-1][0...n2-1]
               :input/output data (double **)
                output data
                    a[k1][k2] = C[k1][k2], 0<=k1<n1, 0<=k2<n2
        t[0...*]
               :work area (double *)
                length of t >= 4*n1,                   if single thread, 
                length of t >= 4*n1*FFT2D_MAX_THREADS, if multi threads, 
                t is dynamically allocated, if t == NULL.
        ip[0...*]
               :work area for bit reversal (int *)
                length of ip >= 2+sqrt(n)
                (n = max(n1/2, n2/2))
                ip[0],ip[1] are pointers of the cos/sin table.
        w[0...*]
               :cos/sin table (double *)
                length of w >= max(n1*3/2, n2*3/2)
                w[],ip[] are initialized if ip[0] == 0.
    [remark]
        Inverse of 
            ddct2d(n1, n2, -1, a, t, ip, w);
        is 
            for (j1 = 0; j1 <= n1 - 1; j1++) {
                a[j1][0] *= 0.5;
            }
            for (j2 = 0; j2 <= n2 - 1; j2++) {
                a[0][j2] *= 0.5;
            }
            ddct2d(n1, n2, 1, a, t, ip, w);
            for (j1 = 0; j1 <= n1 - 1; j1++) {
                for (j2 = 0; j2 <= n2 - 1; j2++) {
                    a[j1][j2] *= 4.0 / n1 / n2;
                }
            }
        .


-------- DST (Discrete Sine Transform) / Inverse of DST --------
    [definition]
        <case1> IDST (excluding scale)
            S[k1][k2] = sum_j1=1^n1 sum_j2=1^n2 A[j1][j2] * 
                            sin(pi*j1*(k1+1/2)/n1) * 
                            sin(pi*j2*(k2+1/2)/n2), 
                            0<=k1<n1, 0<=k2<n2
        <case2> DST
            S[k1][k2] = sum_j1=0^n1-1 sum_j2=0^n2-1 a[j1][j2] * 
                            sin(pi*(j1+1/2)*k1/n1) * 
                            sin(pi*(j2+1/2)*k2/n2), 
                            0<k1<=n1, 0<k2<=n2
    [usage]
        <case1>
            ip[0] = 0; // first time only
            ddst2d(n1, n2, 1, a, t, ip, w);
        <case2>
            ip[0] = 0; // first time only
            ddst2d(n1, n2, -1, a, t, ip, w);
    [parameters]
        n1     :data length (int)
                n1 >= 2, n1 = power of 2
        n2     :data length (int)
                n2 >= 2, n2 = power of 2
        a[0...n1-1][0...n2-1]
               :input/output data (double **)
                <case1>
                    input data
                        a[j1][j2] = A[j1][j2], 0<j1<n1, 0<j2<n2, 
                        a[j1][0] = A[j1][n2], 0<j1<n1, 
                        a[0][j2] = A[n1][j2], 0<j2<n2, 
                        a[0][0] = A[n1][n2]
                        (i.e. A[j1][j2] = a[j1 % n1][j2 % n2])
                    output data
                        a[k1][k2] = S[k1][k2], 0<=k1<n1, 0<=k2<n2
                <case2>
                    output data
                        a[k1][k2] = S[k1][k2], 0<k1<n1, 0<k2<n2, 
                        a[k1][0] = S[k1][n2], 0<k1<n1, 
                        a[0][k2] = S[n1][k2], 0<k2<n2, 
                        a[0][0] = S[n1][n2]
                        (i.e. S[k1][k2] = a[k1 % n1][k2 % n2])
        t[0...*]
               :work area (double *)
                length of t >= 4*n1,                   if single thread, 
                length of t >= 4*n1*FFT2D_MAX_THREADS, if multi threads, 
                t is dynamically allocated, if t == NULL.
        ip[0...*]
               :work area for bit reversal (int *)
                length of ip >= 2+sqrt(n)
                (n = max(n1/2, n2/2))
                ip[0],ip[1] are pointers of the cos/sin table.
        w[0...*]
               :cos/sin table (double *)
                length of w >= max(n1*3/2, n2*3/2)
                w[],ip[] are initialized if ip[0] == 0.
    [remark]
        Inverse of 
            ddst2d(n1, n2, -1, a, t, ip, w);
        is 
            for (j1 = 0; j1 <= n1 - 1; j1++) {
                a[j1][0] *= 0.5;
            }
            for (j2 = 0; j2 <= n2 - 1; j2++) {
                a[0][j2] *= 0.5;
            }
            ddst2d(n1, n2, 1, a, t, ip, w);
            for (j1 = 0; j1 <= n1 - 1; j1++) {
                for (j2 = 0; j2 <= n2 - 1; j2++) {
                    a[j1][j2] *= 4.0 / n1 / n2;
                }
            }
        .
*/


#include <stdio.h>
#include <stdlib.h>
#define fft2d_alloc_error_check(p) { \
    if ((p) == NULL) { \
        fprintf(stderr, "fft2d memory allocation error\n"); \
        exit(1); \
    } \
}


#ifdef USE_FFT2D_PTHREADS
#define USE_FFT2D_THREADS
#ifndef FFT2D_MAX_THREADS
#define FFT2D_MAX_THREADS 4
#endif
#ifndef FFT2D_THREADS_BEGIN_N
#define FFT2D_THREADS_BEGIN_N 65536
#endif
#include <pthread.h>
#define fft2d_thread_t pthread_t
#define fft2d_thread_create(thp,func,argp) { \
    if (pthread_create(thp, NULL, func, (void *) (argp)) != 0) { \
        fprintf(stderr, "fft2d thread error\n"); \
        exit(1); \
    } \
}
#define fft2d_thread_wait(th) { \
    if (pthread_join(th, NULL) != 0) { \
        fprintf(stderr, "fft2d thread error\n"); \
        exit(1); \
    } \
}
#endif /* USE_FFT2D_PTHREADS */


#ifdef USE_FFT2D_WINTHREADS
#define USE_FFT2D_THREADS
#ifndef FFT2D_MAX_THREADS
#define FFT2D_MAX_THREADS 4
#endif
#ifndef FFT2D_THREADS_BEGIN_N
#define FFT2D_THREADS_BEGIN_N 131072
#endif
#include <windows.h>
#define fft2d_thread_t HANDLE
#define fft2d_thread_create(thp,func,argp) { \
    DWORD thid; \
    *(thp) = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) (func), (LPVOID) (argp), 0, &thid); \
    if (*(thp) == 0) { \
        fprintf(stderr, "fft2d thread error\n"); \
        exit(1); \
    } \
}
#define fft2d_thread_wait(th) { \
    WaitForSingleObject(th, INFINITE); \
    CloseHandle(th); \
}
#endif /* USE_FFT2D_WINTHREADS */


void cdft2d(int n1, int n2, int isgn, double **a, double *t, 
    int *ip, double *w)
{
    void makewt(int nw, int *ip, double *w);
    void cdft(int n, int isgn, double *a, int *ip, double *w);
    void cdft2d_sub(int n1, int n2, int isgn, double **a, double *t, 
        int *ip, double *w);
#ifdef USE_FFT2D_THREADS
    void xdft2d0_subth(int n1, int n2, int icr, int isgn, double **a, 
        int *ip, double *w);
    void cdft2d_subth(int n1, int n2, int isgn, double **a, double *t, 
        int *ip, double *w);
#endif /* USE_FFT2D_THREADS */
    int n, itnull, nthread, nt, i;
    
    n = n1 << 1;
    if (n < n2) {
        n = n2;
    }
    if (n > (ip[0] << 2)) {
        makewt(n >> 2, ip, w);
    }
    itnull = 0;
    if (t == NULL) {
        itnull = 1;
        nthread = 1;
#ifdef USE_FFT2D_THREADS
        nthread = FFT2D_MAX_THREADS;
#endif /* USE_FFT2D_THREADS */
        nt = 8 * nthread * n1;
        if (n2 == 4 * nthread) {
            nt >>= 1;
        } else if (n2 < 4 * nthread) {
            nt >>= 2;
        }
        t = (double *) malloc(sizeof(double) * nt);
        fft2d_alloc_error_check(t);
    }
#ifdef USE_FFT2D_THREADS
    if ((double) n1 * n2 >= (double) FFT2D_THREADS_BEGIN_N) {
        xdft2d0_subth(n1, n2, 0, isgn, a, ip, w);
        cdft2d_subth(n1, n2, isgn, a, t, ip, w);
    } else 
#endif /* USE_FFT2D_THREADS */
    {
        for (i = 0; i < n1; i++) {
            cdft(n2, isgn, a[i], ip, w);
        }
        cdft2d_sub(n1, n2, isgn, a, t, ip, w);
    }
    if (itnull != 0) {
        free(t);
    }
}


void rdft2d(int n1, int n2, int isgn, double **a, double *t, 
    int *ip, double *w)
{
    void makewt(int nw, int *ip, double *w);
    void makect(int nc, int *ip, double *c);
    void rdft(int n, int isgn, double *a, int *ip, double *w);
    void cdft2d_sub(int n1, int n2, int isgn, double **a, double *t, 
        int *ip, double *w);
    void rdft2d_sub(int n1, int n2, int isgn, double **a);
#ifdef USE_FFT2D_THREADS
    void xdft2d0_subth(int n1, int n2, int icr, int isgn, double **a, 
        int *ip, double *w);
    void cdft2d_subth(int n1, int n2, int isgn, double **a, double *t, 
        int *ip, double *w);
#endif /* USE_FFT2D_THREADS */
    int n, nw, nc, itnull, nthread, nt, i;
    
    n = n1 << 1;
    if (n < n2) {
        n = n2;
    }
    nw = ip[0];
    if (n > (nw << 2)) {
        nw = n >> 2;
        makewt(nw, ip, w);
    }
    nc = ip[1];
    if (n2 > (nc << 2)) {
        nc = n2 >> 2;
        makect(nc, ip, w + nw);
    }
    itnull = 0;
    if (t == NULL) {
        itnull = 1;
        nthread = 1;
#ifdef USE_FFT2D_THREADS
        nthread = FFT2D_MAX_THREADS;
#endif /* USE_FFT2D_THREADS */
        nt = 8 * nthread * n1;
        if (n2 == 4 * nthread) {
            nt >>= 1;
        } else if (n2 < 4 * nthread) {
            nt >>= 2;
        }
        t = (double *) malloc(sizeof(double) * nt);
        fft2d_alloc_error_check(t);
    }
#ifdef USE_FFT2D_THREADS
    if ((double) n1 * n2 >= (double) FFT2D_THREADS_BEGIN_N) {
        if (isgn < 0) {
            rdft2d_sub(n1, n2, isgn, a);
            cdft2d_subth(n1, n2, isgn, a, t, ip, w);
        }
        xdft2d0_subth(n1, n2, 1, isgn, a, ip, w);
        if (isgn >= 0) {
            cdft2d_subth(n1, n2, isgn, a, t, ip, w);
            rdft2d_sub(n1, n2, isgn, a);
        }
    } else 
#endif /* USE_FFT2D_THREADS */
    {
        if (isgn < 0) {
            rdft2d_sub(n1, n2, isgn, a);
            cdft2d_sub(n1, n2, isgn, a, t, ip, w);
        }
        for (i = 0; i < n1; i++) {
            rdft(n2, isgn, a[i], ip, w);
        }
        if (isgn >= 0) {
            cdft2d_sub(n1, n2, isgn, a, t, ip, w);
            rdft2d_sub(n1, n2, isgn, a);
        }
    }
    if (itnull != 0) {
        free(t);
    }
}


void rdft2dsort(int n1, int n2, int isgn, double **a)
{
    int n1h, i;
    double x, y;
    
    n1h = n1 >> 1;
    if (isgn < 0) {
        for (i = n1h + 1; i < n1; i++) {
            a[i][0] = a[i][n2 + 1];
            a[i][1] = a[i][n2];
        }
        a[0][1] = a[0][n2];
        a[n1h][1] = a[n1h][n2];
    } else {
        for (i = n1h + 1; i < n1; i++) {
            y = a[i][0];
            x = a[i][1];
            a[i][n2] = x;
            a[i][n2 + 1] = y;
            a[n1 - i][n2] = x;
            a[n1 - i][n2 + 1] = -y;
            a[i][0] = a[n1 - i][0];
            a[i][1] = -a[n1 - i][1];
        }
        a[0][n2] = a[0][1];
        a[0][n2 + 1] = 0;
        a[0][1] = 0;
        a[n1h][n2] = a[n1h][1];
        a[n1h][n2 + 1] = 0;
        a[n1h][1] = 0;
    }
}


void ddct2d(int n1, int n2, int isgn, double **a, double *t, 
    int *ip, double *w)
{
    void makewt(int nw, int *ip, double *w);
    void makect(int nc, int *ip, double *c);
    void ddct(int n, int isgn, double *a, int *ip, double *w);
    void ddxt2d_sub(int n1, int n2, int ics, int isgn, double **a, 
        double *t, int *ip, double *w);
#ifdef USE_FFT2D_THREADS
    void ddxt2d0_subth(int n1, int n2, int ics, int isgn, double **a, 
        int *ip, double *w);
    void ddxt2d_subth(int n1, int n2, int ics, int isgn, double **a, 
        double *t, int *ip, double *w);
#endif /* USE_FFT2D_THREADS */
    int n, nw, nc, itnull, nthread, nt, i;
    
    n = n1;
    if (n < n2) {
        n = n2;
    }
    nw = ip[0];
    if (n > (nw << 2)) {
        nw = n >> 2;
        makewt(nw, ip, w);
    }
    nc = ip[1];
    if (n > nc) {
        nc = n;
        makect(nc, ip, w + nw);
    }
    itnull = 0;
    if (t == NULL) {
        itnull = 1;
        nthread = 1;
#ifdef USE_FFT2D_THREADS
        nthread = FFT2D_MAX_THREADS;
#endif /* USE_FFT2D_THREADS */
        nt = 4 * nthread * n1;
        if (n2 == 2 * nthread) {
            nt >>= 1;
        } else if (n2 < 2 * nthread) {
            nt >>= 2;
        }
        t = (double *) malloc(sizeof(double) * nt);
        fft2d_alloc_error_check(t);
    }
#ifdef USE_FFT2D_THREADS
    if ((double) n1 * n2 >= (double) FFT2D_THREADS_BEGIN_N) {
        ddxt2d0_subth(n1, n2, 0, isgn, a, ip, w);
        ddxt2d_subth(n1, n2, 0, isgn, a, t, ip, w);
    } else 
#endif /* USE_FFT2D_THREADS */
    {
        for (i = 0; i < n1; i++) {
            ddct(n2, isgn, a[i], ip, w);
        }
        ddxt2d_sub(n1, n2, 0, isgn, a, t, ip, w);
    }
    if (itnull != 0) {
        free(t);
    }
}


void ddst2d(int n1, int n2, int isgn, double **a, double *t, 
    int *ip, double *w)
{
    void makewt(int nw, int *ip, double *w);
    void makect(int nc, int *ip, double *c);
    void ddst(int n, int isgn, double *a, int *ip, double *w);
    void ddxt2d_sub(int n1, int n2, int ics, int isgn, double **a, 
        double *t, int *ip, double *w);
#ifdef USE_FFT2D_THREADS
    void ddxt2d0_subth(int n1, int n2, int ics, int isgn, double **a, 
        int *ip, double *w);
    void ddxt2d_subth(int n1, int n2, int ics, int isgn, double **a, 
        double *t, int *ip, double *w);
#endif /* USE_FFT2D_THREADS */
    int n, nw, nc, itnull, nthread, nt, i;
    
    n = n1;
    if (n < n2) {
        n = n2;
    }
    nw = ip[0];
    if (n > (nw << 2)) {
        nw = n >> 2;
        makewt(nw, ip, w);
    }
    nc = ip[1];
    if (n > nc) {
        nc = n;
        makect(nc, ip, w + nw);
    }
    itnull = 0;
    if (t == NULL) {
        itnull = 1;
        nthread = 1;
#ifdef USE_FFT2D_THREADS
        nthread = FFT2D_MAX_THREADS;
#endif /* USE_FFT2D_THREADS */
        nt = 4 * nthread * n1;
        if (n2 == 2 * nthread) {
            nt >>= 1;
        } else if (n2 < 2 * nthread) {
            nt >>= 2;
        }
        t = (double *) malloc(sizeof(double) * nt);
        fft2d_alloc_error_check(t);
    }
#ifdef USE_FFT2D_THREADS
    if ((double) n1 * n2 >= (double) FFT2D_THREADS_BEGIN_N) {
        ddxt2d0_subth(n1, n2, 1, isgn, a, ip, w);
        ddxt2d_subth(n1, n2, 1, isgn, a, t, ip, w);
    } else 
#endif /* USE_FFT2D_THREADS */
    {
        for (i = 0; i < n1; i++) {
            ddst(n2, isgn, a[i], ip, w);
        }
        ddxt2d_sub(n1, n2, 1, isgn, a, t, ip, w);
    }
    if (itnull != 0) {
        free(t);
    }
}


/* -------- child routines -------- */


void cdft2d_sub(int n1, int n2, int isgn, double **a, double *t, 
    int *ip, double *w)
{
    void cdft(int n, int isgn, double *a, int *ip, double *w);
    int i, j;
    
    if (n2 > 4) {
        for (j = 0; j < n2; j += 8) {
            for (i = 0; i < n1; i++) {
                t[2 * i] = a[i][j];
                t[2 * i + 1] = a[i][j + 1];
                t[2 * n1 + 2 * i] = a[i][j + 2];
                t[2 * n1 + 2 * i + 1] = a[i][j + 3];
                t[4 * n1 + 2 * i] = a[i][j + 4];
                t[4 * n1 + 2 * i + 1] = a[i][j + 5];
                t[6 * n1 + 2 * i] = a[i][j + 6];
                t[6 * n1 + 2 * i + 1] = a[i][j + 7];
            }
            cdft(2 * n1, isgn, t, ip, w);
            cdft(2 * n1, isgn, &t[2 * n1], ip, w);
            cdft(2 * n1, isgn, &t[4 * n1], ip, w);
            cdft(2 * n1, isgn, &t[6 * n1], ip, w);
            for (i = 0; i < n1; i++) {
                a[i][j] = t[2 * i];
                a[i][j + 1] = t[2 * i + 1];
                a[i][j + 2] = t[2 * n1 + 2 * i];
                a[i][j + 3] = t[2 * n1 + 2 * i + 1];
                a[i][j + 4] = t[4 * n1 + 2 * i];
                a[i][j + 5] = t[4 * n1 + 2 * i + 1];
                a[i][j + 6] = t[6 * n1 + 2 * i];
                a[i][j + 7] = t[6 * n1 + 2 * i + 1];
            }
        }
    } else if (n2 == 4) {
        for (i = 0; i < n1; i++) {
            t[2 * i] = a[i][0];
            t[2 * i + 1] = a[i][1];
            t[2 * n1 + 2 * i] = a[i][2];
            t[2 * n1 + 2 * i + 1] = a[i][3];
        }
        cdft(2 * n1, isgn, t, ip, w);
        cdft(2 * n1, isgn, &t[2 * n1], ip, w);
        for (i = 0; i < n1; i++) {
            a[i][0] = t[2 * i];
            a[i][1] = t[2 * i + 1];
            a[i][2] = t[2 * n1 + 2 * i];
            a[i][3] = t[2 * n1 + 2 * i + 1];
        }
    } else if (n2 == 2) {
        for (i = 0; i < n1; i++) {
            t[2 * i] = a[i][0];
            t[2 * i + 1] = a[i][1];
        }
        cdft(2 * n1, isgn, t, ip, w);
        for (i = 0; i < n1; i++) {
            a[i][0] = t[2 * i];
            a[i][1] = t[2 * i + 1];
        }
    }
}


void rdft2d_sub(int n1, int n2, int isgn, double **a)
{
    int n1h, i, j;
    double xi;
    
    n1h = n1 >> 1;
    if (isgn < 0) {
        for (i = 1; i < n1h; i++) {
            j = n1 - i;
            xi = a[i][0] - a[j][0];
            a[i][0] += a[j][0];
            a[j][0] = xi;
            xi = a[j][1] - a[i][1];
            a[i][1] += a[j][1];
            a[j][1] = xi;
        }
    } else {
        for (i = 1; i < n1h; i++) {
            j = n1 - i;
            a[j][0] = 0.5 * (a[i][0] - a[j][0]);
            a[i][0] -= a[j][0];
            a[j][1] = 0.5 * (a[i][1] + a[j][1]);
            a[i][1] -= a[j][1];
        }
    }
}


void ddxt2d_sub(int n1, int n2, int ics, int isgn, double **a, 
    double *t, int *ip, double *w)
{
    void ddct(int n, int isgn, double *a, int *ip, double *w);
    void ddst(int n, int isgn, double *a, int *ip, double *w);
    int i, j;
    
    if (n2 > 2) {
        for (j = 0; j < n2; j += 4) {
            for (i = 0; i < n1; i++) {
                t[i] = a[i][j];
                t[n1 + i] = a[i][j + 1];
                t[2 * n1 + i] = a[i][j + 2];
                t[3 * n1 + i] = a[i][j + 3];
            }
            if (ics == 0) {
                ddct(n1, isgn, t, ip, w);
                ddct(n1, isgn, &t[n1], ip, w);
                ddct(n1, isgn, &t[2 * n1], ip, w);
                ddct(n1, isgn, &t[3 * n1], ip, w);
            } else {
                ddst(n1, isgn, t, ip, w);
                ddst(n1, isgn, &t[n1], ip, w);
                ddst(n1, isgn, &t[2 * n1], ip, w);
                ddst(n1, isgn, &t[3 * n1], ip, w);
            }
            for (i = 0; i < n1; i++) {
                a[i][j] = t[i];
                a[i][j + 1] = t[n1 + i];
                a[i][j + 2] = t[2 * n1 + i];
                a[i][j + 3] = t[3 * n1 + i];
            }
        }
    } else if (n2 == 2) {
        for (i = 0; i < n1; i++) {
            t[i] = a[i][0];
            t[n1 + i] = a[i][1];
        }
        if (ics == 0) {
            ddct(n1, isgn, t, ip, w);
            ddct(n1, isgn, &t[n1], ip, w);
        } else {
            ddst(n1, isgn, t, ip, w);
            ddst(n1, isgn, &t[n1], ip, w);
        }
        for (i = 0; i < n1; i++) {
            a[i][0] = t[i];
            a[i][1] = t[n1 + i];
        }
    }
}


#ifdef USE_FFT2D_THREADS
struct fft2d_arg_st {
    int nthread;
    int n0;
    int n1;
    int n2;
    int ic;
    int isgn;
    double **a;
    double *t;
    int *ip;
    double *w;
};
typedef struct fft2d_arg_st fft2d_arg_t;


void xdft2d0_subth(int n1, int n2, int icr, int isgn, double **a, 
    int *ip, double *w)
{
    void *xdft2d0_th(void *p);
    fft2d_thread_t th[FFT2D_MAX_THREADS];
    fft2d_arg_t ag[FFT2D_MAX_THREADS];
    int nthread, i;
    
    nthread = FFT2D_MAX_THREADS;
    if (nthread > n1) {
        nthread = n1;
    }
    for (i = 0; i < nthread; i++) {
        ag[i].nthread = nthread;
        ag[i].n0 = i;
        ag[i].n1 = n1;
        ag[i].n2 = n2;
        ag[i].ic = icr;
        ag[i].isgn = isgn;
        ag[i].a = a;
        ag[i].ip = ip;
        ag[i].w = w;
        fft2d_thread_create(&th[i], xdft2d0_th, &ag[i]);
    }
    for (i = 0; i < nthread; i++) {
        fft2d_thread_wait(th[i]);
    }
}


void cdft2d_subth(int n1, int n2, int isgn, double **a, double *t, 
    int *ip, double *w)
{
    void *cdft2d_th(void *p);
    fft2d_thread_t th[FFT2D_MAX_THREADS];
    fft2d_arg_t ag[FFT2D_MAX_THREADS];
    int nthread, nt, i;
    
    nthread = FFT2D_MAX_THREADS;
    nt = 8 * n1;
    if (n2 == 4 * FFT2D_MAX_THREADS) {
        nt >>= 1;
    } else if (n2 < 4 * FFT2D_MAX_THREADS) {
        nthread = n2 >> 1;
        nt >>= 2;
    }
    for (i = 0; i < nthread; i++) {
        ag[i].nthread = nthread;
        ag[i].n0 = i;
        ag[i].n1 = n1;
        ag[i].n2 = n2;
        ag[i].isgn = isgn;
        ag[i].a = a;
        ag[i].t = &t[nt * i];
        ag[i].ip = ip;
        ag[i].w = w;
        fft2d_thread_create(&th[i], cdft2d_th, &ag[i]);
    }
    for (i = 0; i < nthread; i++) {
        fft2d_thread_wait(th[i]);
    }
}


void ddxt2d0_subth(int n1, int n2, int ics, int isgn, double **a, 
    int *ip, double *w)
{
    void *ddxt2d0_th(void *p);
    fft2d_thread_t th[FFT2D_MAX_THREADS];
    fft2d_arg_t ag[FFT2D_MAX_THREADS];
    int nthread, i;
    
    nthread = FFT2D_MAX_THREADS;
    if (nthread > n1) {
        nthread = n1;
    }
    for (i = 0; i < nthread; i++) {
        ag[i].nthread = nthread;
        ag[i].n0 = i;
        ag[i].n1 = n1;
        ag[i].n2 = n2;
        ag[i].ic = ics;
        ag[i].isgn = isgn;
        ag[i].a = a;
        ag[i].ip = ip;
        ag[i].w = w;
        fft2d_thread_create(&th[i], ddxt2d0_th, &ag[i]);
    }
    for (i = 0; i < nthread; i++) {
        fft2d_thread_wait(th[i]);
    }
}


void ddxt2d_subth(int n1, int n2, int ics, int isgn, double **a, 
    double *t, int *ip, double *w)
{
    void *ddxt2d_th(void *p);
    fft2d_thread_t th[FFT2D_MAX_THREADS];
    fft2d_arg_t ag[FFT2D_MAX_THREADS];
    int nthread, nt, i;
    
    nthread = FFT2D_MAX_THREADS;
    nt = 4 * n1;
    if (n2 == 2 * FFT2D_MAX_THREADS) {
        nt >>= 1;
    } else if (n2 < 2 * FFT2D_MAX_THREADS) {
        nthread = n2;
        nt >>= 2;
    }
    for (i = 0; i < nthread; i++) {
        ag[i].nthread = nthread;
        ag[i].n0 = i;
        ag[i].n1 = n1;
        ag[i].n2 = n2;
        ag[i].ic = ics;
        ag[i].isgn = isgn;
        ag[i].a = a;
        ag[i].t = &t[nt * i];
        ag[i].ip = ip;
        ag[i].w = w;
        fft2d_thread_create(&th[i], ddxt2d_th, &ag[i]);
    }
    for (i = 0; i < nthread; i++) {
        fft2d_thread_wait(th[i]);
    }
}


void *xdft2d0_th(void *p)
{
    void cdft(int n, int isgn, double *a, int *ip, double *w);
    void rdft(int n, int isgn, double *a, int *ip, double *w);
    int nthread, n0, n1, n2, icr, isgn, *ip, i;
    double **a, *w;
    
    nthread = ((fft2d_arg_t *) p)->nthread;
    n0 = ((fft2d_arg_t *) p)->n0;
    n1 = ((fft2d_arg_t *) p)->n1;
    n2 = ((fft2d_arg_t *) p)->n2;
    icr = ((fft2d_arg_t *) p)->ic;
    isgn = ((fft2d_arg_t *) p)->isgn;
    a = ((fft2d_arg_t *) p)->a;
    ip = ((fft2d_arg_t *) p)->ip;
    w = ((fft2d_arg_t *) p)->w;
    if (icr == 0) {
        for (i = n0; i < n1; i += nthread) {
            cdft(n2, isgn, a[i], ip, w);
        }
    } else {
        for (i = n0; i < n1; i += nthread) {
            rdft(n2, isgn, a[i], ip, w);
        }
    }
    return (void *) 0;
}


void *cdft2d_th(void *p)
{
    void cdft(int n, int isgn, double *a, int *ip, double *w);
    int nthread, n0, n1, n2, isgn, *ip, i, j;
    double **a, *t, *w;
    
    nthread = ((fft2d_arg_t *) p)->nthread;
    n0 = ((fft2d_arg_t *) p)->n0;
    n1 = ((fft2d_arg_t *) p)->n1;
    n2 = ((fft2d_arg_t *) p)->n2;
    isgn = ((fft2d_arg_t *) p)->isgn;
    a = ((fft2d_arg_t *) p)->a;
    t = ((fft2d_arg_t *) p)->t;
    ip = ((fft2d_arg_t *) p)->ip;
    w = ((fft2d_arg_t *) p)->w;
    if (n2 > 4 * nthread) {
        for (j = 8 * n0; j < n2; j += 8 * nthread) {
            for (i = 0; i < n1; i++) {
                t[2 * i] = a[i][j];
                t[2 * i + 1] = a[i][j + 1];
                t[2 * n1 + 2 * i] = a[i][j + 2];
                t[2 * n1 + 2 * i + 1] = a[i][j + 3];
                t[4 * n1 + 2 * i] = a[i][j + 4];
                t[4 * n1 + 2 * i + 1] = a[i][j + 5];
                t[6 * n1 + 2 * i] = a[i][j + 6];
                t[6 * n1 + 2 * i + 1] = a[i][j + 7];
            }
            cdft(2 * n1, isgn, t, ip, w);
            cdft(2 * n1, isgn, &t[2 * n1], ip, w);
            cdft(2 * n1, isgn, &t[4 * n1], ip, w);
            cdft(2 * n1, isgn, &t[6 * n1], ip, w);
            for (i = 0; i < n1; i++) {
                a[i][j] = t[2 * i];
                a[i][j + 1] = t[2 * i + 1];
                a[i][j + 2] = t[2 * n1 + 2 * i];
                a[i][j + 3] = t[2 * n1 + 2 * i + 1];
                a[i][j + 4] = t[4 * n1 + 2 * i];
                a[i][j + 5] = t[4 * n1 + 2 * i + 1];
                a[i][j + 6] = t[6 * n1 + 2 * i];
                a[i][j + 7] = t[6 * n1 + 2 * i + 1];
            }
        }
    } else if (n2 == 4 * nthread) {
        for (i = 0; i < n1; i++) {
            t[2 * i] = a[i][4 * n0];
            t[2 * i + 1] = a[i][4 * n0 + 1];
            t[2 * n1 + 2 * i] = a[i][4 * n0 + 2];
            t[2 * n1 + 2 * i + 1] = a[i][4 * n0 + 3];
        }
        cdft(2 * n1, isgn, t, ip, w);
        cdft(2 * n1, isgn, &t[2 * n1], ip, w);
        for (i = 0; i < n1; i++) {
            a[i][4 * n0] = t[2 * i];
            a[i][4 * n0 + 1] = t[2 * i + 1];
            a[i][4 * n0 + 2] = t[2 * n1 + 2 * i];
            a[i][4 * n0 + 3] = t[2 * n1 + 2 * i + 1];
        }
    } else if (n2 == 2 * nthread) {
        for (i = 0; i < n1; i++) {
            t[2 * i] = a[i][2 * n0];
            t[2 * i + 1] = a[i][2 * n0 + 1];
        }
        cdft(2 * n1, isgn, t, ip, w);
        for (i = 0; i < n1; i++) {
            a[i][2 * n0] = t[2 * i];
            a[i][2 * n0 + 1] = t[2 * i + 1];
        }
    }
    return (void *) 0;
}


void *ddxt2d0_th(void *p)
{
    void ddct(int n, int isgn, double *a, int *ip, double *w);
    void ddst(int n, int isgn, double *a, int *ip, double *w);
    int nthread, n0, n1, n2, ics, isgn, *ip, i;
    double **a, *w;
    
    nthread = ((fft2d_arg_t *) p)->nthread;
    n0 = ((fft2d_arg_t *) p)->n0;
    n1 = ((fft2d_arg_t *) p)->n1;
    n2 = ((fft2d_arg_t *) p)->n2;
    ics = ((fft2d_arg_t *) p)->ic;
    isgn = ((fft2d_arg_t *) p)->isgn;
    a = ((fft2d_arg_t *) p)->a;
    ip = ((fft2d_arg_t *) p)->ip;
    w = ((fft2d_arg_t *) p)->w;
    if (ics == 0) {
        for (i = n0; i < n1; i += nthread) {
            ddct(n2, isgn, a[i], ip, w);
        }
    } else {
        for (i = n0; i < n1; i += nthread) {
            ddst(n2, isgn, a[i], ip, w);
        }
    }
    return (void *) 0;
}


void *ddxt2d_th(void *p)
{
    void ddct(int n, int isgn, double *a, int *ip, double *w);
    void ddst(int n, int isgn, double *a, int *ip, double *w);
    int nthread, n0, n1, n2, ics, isgn, *ip, i, j;
    double **a, *t, *w;
    
    nthread = ((fft2d_arg_t *) p)->nthread;
    n0 = ((fft2d_arg_t *) p)->n0;
    n1 = ((fft2d_arg_t *) p)->n1;
    n2 = ((fft2d_arg_t *) p)->n2;
    ics = ((fft2d_arg_t *) p)->ic;
    isgn = ((fft2d_arg_t *) p)->isgn;
    a = ((fft2d_arg_t *) p)->a;
    t = ((fft2d_arg_t *) p)->t;
    ip = ((fft2d_arg_t *) p)->ip;
    w = ((fft2d_arg_t *) p)->w;
    if (n2 > 2 * nthread) {
        for (j = 4 * n0; j < n2; j += 4 * nthread) {
            for (i = 0; i < n1; i++) {
                t[i] = a[i][j];
                t[n1 + i] = a[i][j + 1];
                t[2 * n1 + i] = a[i][j + 2];
                t[3 * n1 + i] = a[i][j + 3];
            }
            if (ics == 0) {
                ddct(n1, isgn, t, ip, w);
                ddct(n1, isgn, &t[n1], ip, w);
                ddct(n1, isgn, &t[2 * n1], ip, w);
                ddct(n1, isgn, &t[3 * n1], ip, w);
            } else {
                ddst(n1, isgn, t, ip, w);
                ddst(n1, isgn, &t[n1], ip, w);
                ddst(n1, isgn, &t[2 * n1], ip, w);
                ddst(n1, isgn, &t[3 * n1], ip, w);
            }
            for (i = 0; i < n1; i++) {
                a[i][j] = t[i];
                a[i][j + 1] = t[n1 + i];
                a[i][j + 2] = t[2 * n1 + i];
                a[i][j + 3] = t[3 * n1 + i];
            }
        }
    } else if (n2 == 2 * nthread) {
        for (i = 0; i < n1; i++) {
            t[i] = a[i][2 * n0];
            t[n1 + i] = a[i][2 * n0 + 1];
        }
        if (ics == 0) {
            ddct(n1, isgn, t, ip, w);
            ddct(n1, isgn, &t[n1], ip, w);
        } else {
            ddst(n1, isgn, t, ip, w);
            ddst(n1, isgn, &t[n1], ip, w);
        }
        for (i = 0; i < n1; i++) {
            a[i][2 * n0] = t[i];
            a[i][2 * n0 + 1] = t[n1 + i];
        }
    } else if (n2 == nthread) {
        for (i = 0; i < n1; i++) {
            t[i] = a[i][n0];
        }
        if (ics == 0) {
            ddct(n1, isgn, t, ip, w);
        } else {
            ddst(n1, isgn, t, ip, w);
        }
        for (i = 0; i < n1; i++) {
            a[i][n0] = t[i];
        }
    }
    return (void *) 0;
}
#endif /* USE_FFT2D_THREADS */

