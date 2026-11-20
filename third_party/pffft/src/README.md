PFFFT: a pretty fast FFT.

## TL;DR

PFFFT does 1D Fast Fourier Transforms, of single precision real and
complex vectors. It tries do it fast, it tries to be correct, and it
tries to be small. Computations do take advantage of SSE1 instructions
on x86 cpus, Altivec on powerpc cpus, and NEON on ARM cpus. The
license is BSD-like.


## Why does it exist:

I was in search of a good performing FFT library , preferably very
small and with a very liberal license.

When one says "fft library", FFTW ("Fastest Fourier Transform in the
West") is probably the first name that comes to mind -- I guess that
99% of open-source projects that need a FFT do use FFTW, and are happy
with it. However, it is quite a large library , which does everything
fft related (2d transforms, 3d transforms, other transformations such
as discrete cosine , or fast hartley). And it is licensed under the
GNU GPL , which means that it cannot be used in non open-source
products.

An alternative to FFTW that is really small, is the venerable FFTPACK
v4, which is available on NETLIB. A more recent version (v5) exists,
but it is larger as it deals with multi-dimensional transforms. This
is a library that is written in FORTRAN 77, a language that is now
considered as a bit antiquated by many. FFTPACKv4 was written in 1985,
by Dr Paul Swarztrauber of NCAR, more than 25 years ago ! And despite
its age, benchmarks show it that it still a very good performing FFT
library, see for example the 1d single precision benchmarks here:
http://www.fftw.org/speed/opteron-2.2GHz-32bit/ . It is however not
competitive with the fastest ones, such as FFTW, Intel MKL, AMD ACML,
Apple vDSP. The reason for that is that those libraries do take
advantage of the SSE SIMD instructions available on Intel CPUs,
available since the days of the Pentium III. These instructions deal
with small vectors of 4 floats at a time, instead of a single float
for a traditionnal FPU, so when using these instructions one may expect
a 4-fold performance improvement.

The idea was to take this fortran fftpack v4 code, translate to C,
modify it to deal with those SSE instructions, and check that the
final performance is not completely ridiculous when compared to other
SIMD FFT libraries. Translation to C was performed with f2c (
http://www.netlib.org/f2c/ ). The resulting file was a bit edited in
order to remove the thousands of gotos that were introduced by
f2c. You will find the fftpack.h and fftpack.c sources in the
repository, this a complete translation of
http://www.netlib.org/fftpack/ , with the discrete cosine transform
and the test program. There is no license information in the netlib
repository, but it was confirmed to me by the fftpack v5 curators that
the same terms do apply to fftpack v4:
http://www.cisl.ucar.edu/css/software/fftpack5/ftpk.html . This is a
"BSD-like" license, it is compatible with proprietary projects.

Adapting fftpack to deal with the SIMD 4-element vectors instead of
scalar single precision numbers was more complex than I originally
thought, especially with the real transforms, and I ended up writing
more code than I planned..


## The code:

Only two files, in good old C, pffft.c and pffft.h . The API is very
very simple, just make sure that you read the comments in pffft.h.


## Comparison with other FFTs:

The idea was not to break speed records, but to get a decently fast
fft that is at least 50% as fast as the fastest FFT -- especially on
slowest computers . I'm more focused on getting the best performance
on slow cpus (Atom, Intel Core 1, old Athlons, ARM Cortex-A9...), than
on getting top performance on today fastest cpus.

It can be used in a real-time context as the fft functions do not
perform any memory allocation -- that is why they accept a 'work'
array in their arguments.

It is also a bit focused on performing 1D convolutions, that is why it
provides "unordered" FFTs , and a fourier domain convolution
operation.


Benchmark results
--

The benchmark shows the performance of various fft implementations measured in 
MFlops, with the number of floating point operations being defined as 5Nlog2(N)
for a length N complex fft, and 2.5*Nlog2(N) for a real fft. 
See http://www.fftw.org/speed/method.html for an explanation of these formulas.

2021 update: I'm now including Intel MKL, and I'm removing old benchmarks results for cpu that have
not been relevant for a very long time.. Now that most Intel cpus have AVX enabled (most but not
all, Intel is still selling cpus with no AVX), the performance of pffft vs MKL or VDSP is a bit
behind since pffft is only SSE2.

However, performance of pffft on the Apple M1 cpu is very good (especially for the real fft), so I'll put it first :-)


MacOS Big Sur, XCode 12, M1 cpu on a 2020 mac mini. I'm not including fftw results as they are very
bad here, most likely Homebrew did not enable neon when building the lib.

    clang -o test_pffft -DHAVE_VECLIB -O3 -Wall -W pffft.c test_pffft.c fftpack.c -framework Accelerate

| input len  |real FFTPack|  real vDSP | real PFFFT |cplx FFTPack|  cplx vDSP | cplx PFFFT |
|-----------:|-----------:|-----------:|-----------:|-----------:|-----------:|-----------:|
|       64   |     4835   |    15687   |    23704   |    12935   |    36222   |    33046   |
|       96   |     9539   |      n/a   |    25957   |    11068   |      n/a   |    32703   |
|      128   |    11119   |    33087   |    30143   |    12329   |    50866   |    36363   |
|      160   |    11062   |      n/a   |    32938   |    11641   |      n/a   |    34945   |
|      192   |    11784   |      n/a   |    35726   |    12945   |      n/a   |    39258   |
|      256   |    13370   |    44880   |    40683   |    14214   |    64654   |    42522   |
|      384   |    11059   |      n/a   |    40038   |    11655   |      n/a   |    39565   |
|      480   |    10977   |      n/a   |    40895   |    10802   |      n/a   |    36943   |
|      512   |    12197   |    34830   |    43353   |    12357   |    78441   |    41450   |
|      640   |    11685   |      n/a   |    43393   |    11775   |      n/a   |    39302   |
|      768   |    12314   |      n/a   |    45836   |    12756   |      n/a   |    43058   |
|      800   |    11213   |      n/a   |    41321   |    10842   |      n/a   |    37382   |
|     1024   |    13354   |    45214   |    50039   |    13759   |    93269   |    45210   |
|     2048   |    12806   |    57047   |    49519   |    12908   |    99361   |    43719   |
|     2400   |    10972   |      n/a   |    43399   |    10928   |      n/a   |    37574   |
|     4096   |    13957   |    65233   |    52845   |    13851   |   105734   |    46274   |
|     8192   |    12772   |    70108   |    49830   |    12560   |    85238   |    40252   |
|     9216   |    12281   |      n/a   |    48929   |    12114   |      n/a   |    39202   |
|    16384   |    13363   |    62150   |    48260   |    12910   |    79073   |    38742   |
|    32768   |    11622   |    60809   |    32801   |    11145   |    71570   |    35607   |
|   262144   |    11525   |    53651   |    34988   |    10818   |    63198   |    36742   |
|  1048576   |    11167   |    46119   |    34437   |     9202   |    38823   |    31788   |

So yes, the perf of the M1 cpu on the complex is really impressive...

Windows 10, Ryzen 7 4800HS @ 2.9GHz, Visual c++ 2019 and Intel MKL 2018

Build with:

    cl /Ox -D_USE_MATH_DEFINES -DHAVE_MKL /arch:SSE test_pffft.c pffft.c fftpack.c /I c:/MKL/include c:/MKL/lib/intel64/mkl_intel_lp64.lib c:/MKL/lib/intel64/mkl_sequential.lib c:/MKL/lib/intel64/mkl_core.lib

| input len  |real FFTPack|  real MKL  | real PFFFT |cplx FFTPack|  cplx MKL  | cplx PFFFT |
|-----------:|-----------:|-----------:|-----------:|-----------:|-----------:|-----------:|
|       64   |     3938   |     7877   |    14629   |     7314   |    25600   |    19200   |
|       96   |     5108   |    14984   |    13761   |     7329   |    20128   |    20748   |
|      128   |     5973   |    18379   |    19911   |     7626   |    29257   |    23123   |
|      160   |     6694   |    18287   |    19731   |     7976   |    22720   |    21120   |
|      192   |     6472   |    16525   |    20439   |     6813   |    27252   |    25054   |
|      256   |     7585   |    23406   |    25600   |     8715   |    33437   |    26006   |
|      384   |     6279   |    21441   |    23759   |     7206   |    25855   |    25481   |
|      480   |     6514   |    20267   |    22800   |     7238   |    26435   |    21976   |
|      512   |     6776   |    26332   |    29729   |     6312   |    34777   |    25961   |
|      640   |     7019   |    21695   |    27273   |     7232   |    26889   |    25120   |
|      768   |     6815   |    21809   |    28865   |     7667   |    31658   |    27645   |
|      800   |     7261   |    23513   |    25988   |     6764   |    27056   |    25001   |
|     1024   |     7529   |    30118   |    31030   |     8127   |    38641   |    28055   |
|     2048   |     7411   |    31289   |    33129   |     8533   |    38841   |    27812   |
|     2400   |     7768   |    22993   |    26128   |     7563   |    26429   |    24992   |
|     4096   |     8533   |    33211   |    34134   |     8777   |    38400   |    27007   |
|     8192   |     6525   |    32468   |    30254   |     7924   |    39737   |    28025   |
|     9216   |     7322   |    22835   |    28068   |     7322   |    29939   |    26945   |
|    16384   |     7455   |    31807   |    30453   |     8132   |    37177   |    27525   |
|    32768   |     8157   |    31949   |    30671   |     8334   |    29210   |    26214   |
|   262144   |     7349   |    25255   |    24904   |     6844   |    22413   |    16996   |
|  1048576   |     5115   |    19284   |     8347   |     6079   |    12906   |     9244   |

(Note: MKL is not using AVX on AMD cpus)


MacOS Catalina, Xcode 12, fftw 3.3.9 and MKL 2020.1, cpu is "i7-6920HQ CPU @ 2.90GHz"

Built with:

    clang -o test_pffft -DHAVE_MKL -I /opt/intel/mkl/include -DHAVE_FFTW -DHAVE_VECLIB -O3 -Wall -W pffft.c test_pffft.c fftpack.c -L/usr/local/lib -I/usr/local/include/ -lfftw3f -framework Accelerate /opt/intel/mkl/lib/libmkl_{intel_lp64,sequential,core}.

| input len  |real FFTPack|  real vDSP |  real MKL  |  real FFTW | real PFFFT |cplx FFTPack|  cplx vDSP |  cplx MKL  |  cplx FFTW | cplx PFFFT |
|------------|------------|------------|------------|------------|------------|------------|------------|------------|------------|------------|
|       64   |     4528   |    12550   |    18214   |     8237   |    10097   |     5962   |    25748   |    40865   |    32233   |    15807   |
|       96   |     4738   |      n/a   |    15844   |    10749   |    11026   |     5344   |      n/a   |    21086   |    34678   |    15493   |
|      128   |     5464   |    20419   |    25739   |    12371   |    13338   |     6060   |    28659   |    42419   |    38868   |    17694   |
|      160   |     5517   |      n/a   |    18644   |    12361   |    13765   |     6002   |      n/a   |    21633   |    37726   |    17969   |
|      192   |     5904   |      n/a   |    18861   |    12480   |    15134   |     6271   |      n/a   |    26074   |    33216   |    18525   |
|      256   |     6618   |    24944   |    26063   |    14646   |    16895   |     6965   |    34332   |    52182   |    44496   |    20980   |
|      384   |     5685   |      n/a   |    22307   |    14682   |    16969   |     5853   |      n/a   |    27363   |    35805   |    19841   |
|      480   |     5757   |      n/a   |    21122   |    14572   |    16765   |     5836   |      n/a   |    26259   |    26340   |    18852   |
|      512   |     6245   |    28100   |    27224   |    16546   |    18502   |     6240   |    37098   |    51679   |    43444   |    21519   |
|      640   |     6110   |      n/a   |    22565   |    14691   |    18573   |     6376   |      n/a   |    29219   |    34327   |    20708   |
|      768   |     6424   |      n/a   |    21496   |    15999   |    19900   |     6358   |      n/a   |    30168   |    36437   |    21657   |
|      800   |     5747   |      n/a   |    24857   |    15068   |    18842   |     5698   |      n/a   |    26891   |    20249   |    18497   |
|     1024   |     6397   |    28477   |    27520   |    13399   |    18491   |     5558   |    33632   |    44366   |    35960   |    23421   |
|     2048   |     6563   |    37379   |    34743   |    14204   |    20854   |     5828   |    41758   |    40301   |    36469   |    18504   |
|     2400   |     5594   |      n/a   |    24631   |    15496   |    16732   |     4128   |      n/a   |    16997   |    23421   |    16710   |
|     4096   |     6262   |    36417   |    28150   |    17742   |    19356   |     6272   |    33534   |    31632   |    33524   |    16995   |
|     8192   |     4142   |    24923   |    26571   |    17102   |    10104   |     5681   |    29504   |    33221   |    21803   |    15212   |
|     9216   |     5762   |      n/a   |    17305   |    14870   |    14464   |     5781   |      n/a   |    21579   |    22174   |    17358   |
|    16384   |     5650   |    29395   |    27201   |    15857   |    11748   |     5932   |    26534   |    31708   |    21161   |    16173   |
|    32768   |     5441   |    23931   |    26261   |    15394   |    10334   |     5679   |    23278   |    31162   |    19966   |    14624   |
|   262144   |     4946   |    19081   |    23591   |     9612   |     9544   |     4958   |    16362   |    20196   |    10419   |    12575   |
|  1048576   |     3752   |    14873   |    15469   |     6673   |     6653   |     4048   |     9563   |    16681   |     4298   |     7852   |


MacOS Lion, gcc 4.2, 64-bit, fftw 3.3 on a 3.4 GHz core i7 2600

Built with:

     gcc-4.2 -o test_pffft -arch x86_64 -O3 -Wall -W pffft.c test_pffft.c fftpack.c -L/usr/local/lib -I/usr/local/include/ -DHAVE_VECLIB -framework veclib -DHAVE_FFTW -lfftw3f

| input len |real FFTPack|  real vDSP |  real FFTW | real PFFFT |cplx FFTPack|  cplx vDSP |  cplx FFTW | cplx PFFFT |
|----------:|-----------:|-----------:|-----------:|-----------:|-----------:|-----------:|-----------:|-----------:|
|       64  |     2816   |     8596   |     7329   |     8187   |     2887   |    14898   |    14668   |    11108   |
|       96  |     3298   |      n/a   |     8378   |     7727   |     3953   |      n/a   |    15680   |    10878   |
|      128  |     3507   |    11575   |     9266   |    10108   |     4233   |    17598   |    16427   |    12000   |
|      160  |     3391   |      n/a   |     9838   |    10711   |     4220   |      n/a   |    16653   |    11187   |
|      192  |     3919   |      n/a   |     9868   |    10956   |     4297   |      n/a   |    15770   |    12540   |
|      256  |     4283   |    13179   |    10694   |    13128   |     4545   |    19550   |    16350   |    13822   |
|      384  |     3136   |      n/a   |    10810   |    12061   |     3600   |      n/a   |    16103   |    13240   |
|      480  |     3477   |      n/a   |    10632   |    12074   |     3536   |      n/a   |    11630   |    12522   |
|      512  |     3783   |    15141   |    11267   |    13838   |     3649   |    20002   |    16560   |    13580   |
|      640  |     3639   |      n/a   |    11164   |    13946   |     3695   |      n/a   |    15416   |    13890   |
|      768  |     3800   |      n/a   |    11245   |    13495   |     3590   |      n/a   |    15802   |    14552   |
|      800  |     3440   |      n/a   |    10499   |    13301   |     3659   |      n/a   |    12056   |    13268   |
|     1024  |     3924   |    15605   |    11450   |    15339   |     3769   |    20963   |    13941   |    15467   |
|     2048  |     4518   |    16195   |    11551   |    15532   |     4258   |    20413   |    13723   |    15042   |
|     2400  |     4294   |      n/a   |    10685   |    13078   |     4093   |      n/a   |    12777   |    13119   |
|     4096  |     4750   |    16596   |    11672   |    15817   |     4157   |    19662   |    14316   |    14336   |
|     8192  |     3820   |    16227   |    11084   |    12555   |     3691   |    18132   |    12102   |    13813   |
|     9216  |     3864   |      n/a   |    10254   |    12870   |     3586   |      n/a   |    12119   |    13994   |
|    16384  |     3822   |    15123   |    10454   |    12822   |     3613   |    16874   |    12370   |    13881   |
|    32768  |     4175   |    14512   |    10662   |    11095   |     3881   |    14702   |    11619   |    11524   |
|   262144  |     3317   |    11429   |     6269   |     9517   |     2810   |    11729   |     7757   |    10179   |
|  1048576  |     2913   |    10551   |     4730   |     5867   |     2661   |     7881   |     3520   |     5350   |


Ubuntu 11.04, gcc 4.5, 32-bit, fftw 3.2 on a 2.66 core 2 quad

Built with:

    gcc -o test_pffft -DHAVE_FFTW -msse -mfpmath=sse -O3 -Wall -W pffft.c test_pffft.c fftpack.c -L/usr/local/lib -I/usr/local/include/ -lfftw3f -lm

| input len |real FFTPack|  real FFTW | real PFFFT |cplx FFTPack|  cplx FFTW | cplx PFFFT |
|----------:|-----------:|-----------:|-----------:|-----------:|-----------:|-----------:|
|       64  |     1920   |     3614   |     5120   |     2194   |     7680   |     6467   |
|       96  |     1873   |     3549   |     5187   |     2107   |     8429   |     5863   |
|      128  |     2240   |     3773   |     5514   |     2560   |     7964   |     6827   |
|      192  |     1765   |     4569   |     7767   |     2284   |     9137   |     7061   |
|      256  |     2048   |     5461   |     7447   |     2731   |     9638   |     7802   |
|      384  |     1998   |     5861   |     6762   |     2313   |     9253   |     7644   |
|      512  |     2095   |     6144   |     7680   |     2194   |    10240   |     7089   |
|      768  |     2230   |     5773   |     7549   |     2045   |    10331   |     7010   |
|     1024  |     2133   |     6400   |     8533   |     2133   |    10779   |     7877   |
|     2048  |     2011   |     7040   |     8665   |     1942   |    10240   |     7768   |
|     4096  |     2194   |     6827   |     8777   |     1755   |     9452   |     6827   |
|     8192  |     1849   |     6656   |     6656   |     1752   |     7831   |     6827   |
|     9216  |     1871   |     5858   |     6416   |     1643   |     6909   |     6266   |
|    16384  |     1883   |     6223   |     6506   |     1664   |     7340   |     6982   |
|    32768  |     1826   |     6390   |     6667   |     1631   |     7481   |     6971   |
|   262144  |     1546   |     4075   |     5977   |     1299   |     3415   |     3551   |
|  1048576  |     1104   |     2071   |     1730   |     1104   |     1149   |     1834   |


NVIDIA Jetson TK1 board, gcc-4.8.2. The cpu is a 2.3GHz cortex A15 (Tegra K1).

Built with:

    gcc -O3 -march=armv7-a -mtune=native -mfloat-abi=hard -mfpu=neon -ffast-math test_pffft.c pffft.c -o test_pffft_arm fftpack.c -lm

| input len |real FFTPack| real PFFFT |cplx FFTPack| cplx PFFFT |
|----------:|-----------:|-----------:|-----------:|-----------:|
|       64  |     1735   |     3308   |     1994   |     3744   |
|       96  |     1596   |     3448   |     1987   |     3572   |
|      128  |     1807   |     4076   |     2255   |     3960   |
|      160  |     1769   |     4083   |     2071   |     3845   |
|      192  |     1990   |     4233   |     2017   |     3939   |
|      256  |     2191   |     4882   |     2254   |     4346   |
|      384  |     1878   |     4492   |     2073   |     4012   |
|      480  |     1748   |     4398   |     1923   |     3951   |
|      512  |     2030   |     5064   |     2267   |     4195   |
|      640  |     1918   |     4756   |     2094   |     4184   |
|      768  |     2099   |     4907   |     2048   |     4297   |
|      800  |     1822   |     4555   |     1880   |     4063   |
|     1024  |     2232   |     5355   |     2187   |     4420   |
|     2048  |     2176   |     4983   |     2027   |     3602   |
|     2400  |     1741   |     4256   |     1710   |     3344   |
|     4096  |     1816   |     3914   |     1851   |     3349   |
|     8192  |     1716   |     3481   |     1700   |     3255   |
|     9216  |     1735   |     3589   |     1653   |     3094   |
|    16384  |     1567   |     3483   |     1637   |     3244   |
|    32768  |     1624   |     3240   |     1655   |     3156   |
|   262144  |     1012   |     1898   |      983   |     1503   |
|  1048576  |      876   |     1154   |      868   |     1341   |


iPad Air 2 with iOS9, xcode 8.0, arm64. The cpu is an Apple A8X, supposedly running at 1.5GHz.

| input len |real FFTPack|  real vDSP | real PFFFT |cplx FFTPack|  cplx vDSP | cplx PFFFT |
|----------:|-----------:|-----------:|-----------:|-----------:|-----------:|-----------:|
|       64  |     2517   |     7995   |     6086   |     2725   |    13006   |     8495   |
|       96  |     2442   |      n/a   |     6691   |     2256   |      n/a   |     7991   |
|      128  |     2664   |    10186   |     7877   |     2575   |    15115   |     9115   |
|      160  |     2638   |      n/a   |     8283   |     2682   |      n/a   |     8806   |
|      192  |     2903   |      n/a   |     9083   |     2634   |      n/a   |     8980   |
|      256  |     3184   |    11452   |    10039   |     3026   |    15410   |    10199   |
|      384  |     2665   |      n/a   |    10100   |     2275   |      n/a   |     9247   |
|      480  |     2546   |      n/a   |     9863   |     2341   |      n/a   |     8892   |
|      512  |     2832   |    12197   |    10989   |     2547   |    16768   |    10154   |
|      640  |     2755   |      n/a   |    10461   |     2569   |      n/a   |     9666   |
|      768  |     2998   |      n/a   |    11355   |     2585   |      n/a   |     9813   |
|      800  |     2516   |      n/a   |    10332   |     2433   |      n/a   |     9164   |
|     1024  |     3109   |    12965   |    12114   |     2869   |    16448   |    10519   |
|     2048  |     3027   |    12996   |    12023   |     2648   |    17304   |    10307   |
|     2400  |     2515   |      n/a   |    10372   |     2355   |      n/a   |     8443   |
|     4096  |     3204   |    13603   |    12359   |     2814   |    16570   |     9780   |
|     8192  |     2759   |    13422   |    10824   |     2153   |    15652   |     7884   |
|     9216  |     2700   |      n/a   |     9938   |     2241   |      n/a   |     7900   |
|    16384  |     2280   |    13057   |     7976   |      593   |     4272   |     2534   |
|    32768  |      768   |     4269   |     2882   |      606   |     4405   |     2604   |
|   262144  |      724   |     3527   |     2630   |      534   |     2418   |     2157   |
|  1048576  |      674   |     1467   |     2135   |      530   |     1621   |     2055   |



