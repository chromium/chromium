#ifndef _RAR_OS_
#define _RAR_OS_

#define FALSE 0
#define TRUE  1

#if defined(RARDLL) && !defined(SILENT)
#define SILENT
#endif

#include <new>
#include <string>
#include <vector>
#include <deque>
#include <memory> // For automatic pointers.


#ifdef _WIN_ALL

#define LITTLE_ENDIAN


// We got a report that just "#define STRICT" is incompatible with
// "#define STRICT 1" in Windows 10 SDK minwindef.h and depending on the order
// in which these statements are reached this may cause a compiler warning
// and build break for other projects incorporating this source.
// So we changed it to "#define STRICT 1".
#ifndef STRICT
#define STRICT 1
#endif

#if !defined(CHROMIUM_UNRAR)
// 'ifndef' check here is needed for unrar.dll header to avoid macro
// re-definition warnings in third party projects.
#ifndef UNICODE
#define UNICODE
#define _UNICODE // Set _T() macro to convert from narrow to wide strings.
#endif

#define WINVER _WIN32_WINNT_WINXP
#define _WIN32_WINNT _WIN32_WINNT_WINXP
#endif  // !defined(CHROMIUM_UNRAR)

#if !defined(ZIPSFX) && !defined(CHROMIUM_UNRAR)
#define RAR_SMP
#endif

#if !defined(CHROMIUM_UNRAR)
#define WIN32_LEAN_AND_MEAN
#endif  // CHROMIUM_UNRAR

#include <windows.h>
#include <prsht.h>
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#include <PowrProf.h>
#pragma comment(lib, "PowrProf.lib")
#include <shellapi.h>
#include <shlobj.h>
#include <winioctl.h>
#include <wincrypt.h>
#include <wchar.h>
#include <wctype.h>

// For WMI requests.
#include <comdef.h>
#include <WbemIdl.h>
#pragma comment(lib, "wbemuuid.lib")


#include <sys/types.h>
#include <sys/stat.h>
#include <dos.h>
#include <direct.h>
#include <intrin.h>

#if !defined(CHROMIUM_UNRAR)
// Use SSE only for x86/x64, not ARM Windows.
#if defined(_M_IX86) || defined(_M_X64)
  #define USE_SSE
  #define SSE_ALIGNMENT 16
#endif
#endif  // CHROMIUM_UNRAR

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>
#include <io.h>
#include <time.h>
#include <signal.h>

#define SAVE_LINKS

#define ENABLE_ACCESS

#define DefConfigName  L"rar.ini"
#define DefLogName     L"rar.log"


#define SPATHDIVIDER L"\\"
#define CPATHDIVIDER L'\\'
#define MASKALL      L"*"

#define READBINARY   "rb"
#define READTEXT     "rt"
#define UPDATEBINARY "r+b"
#define CREATEBINARY "w+b"
#define WRITEBINARY  "wb"
#define APPENDTEXT   "at"

#define _stdfunction __cdecl
#define _forceinline __forceinline

#endif // _WIN_ALL

#ifdef _UNIX

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#if defined(__QNXNTO__)
  #include <sys/param.h>
#endif
#ifdef _APPLE
  #include <sys/sysctl.h>
#endif
#ifndef SFX_MODULE
    #include <sys/statvfs.h>
#endif
#include <pwd.h>
#include <grp.h>
#include <wchar.h>
#include <wctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <signal.h>
#include <utime.h>
#include <locale.h>

#if !defined(CHROMIUM_UNRAR)
#ifdef __GNUC__
  #if defined(__i386__) || defined(__x86_64__)
    #include <x86intrin.h>
    
    #define USE_SSE
    #define SSE_ALIGNMENT 16
  #endif
#endif
#endif  // CHROMIUM_UNRAR

#if defined(__aarch64__) && (defined(__ARM_FEATURE_CRYPTO) || defined(__ARM_FEATURE_CRC32))
#include <arm_neon.h>
#ifndef _APPLE
#include <sys/auxv.h>
#include <asm/hwcap.h>
#endif
#ifdef __ARM_FEATURE_CRYPTO
#define USE_NEON_AES
#endif
#ifdef __ARM_FEATURE_CRC32
#define USE_NEON_CRC32
#endif
#endif

#ifdef  S_IFLNK
#define SAVE_LINKS
#endif

#if defined(__linux) || defined(__FreeBSD__)
#include <sys/time.h>
#define USE_LUTIMES
#endif

#define ENABLE_ACCESS

#define DefConfigName  L".rarrc"
#define DefLogName     L".rarlog"


#define SPATHDIVIDER L"/"
#define CPATHDIVIDER L'/'
#define MASKALL      L"*"

#define READBINARY   "r"
#define READTEXT     "r"
#define UPDATEBINARY "r+"
#define CREATEBINARY "w+"
#define WRITEBINARY  "w"
#define APPENDTEXT   "a"

#define _stdfunction 
#define _forceinline inline

#ifdef _APPLE
  #if defined(__BIG_ENDIAN__) && !defined(BIG_ENDIAN)
    #define BIG_ENDIAN
    #undef LITTLE_ENDIAN
  #endif
  #if defined(__i386__) && !defined(LITTLE_ENDIAN)
    #define LITTLE_ENDIAN
    #undef BIG_ENDIAN
  #endif
#endif

#if defined(__sparc) || defined(sparc) || defined(__hpux)
  #ifndef BIG_ENDIAN
     #define BIG_ENDIAN
  #endif
#endif

#ifdef __VMS
# define LITTLE_ENDIAN
#endif

// Unlike Apple x64, utimensat shall be available in all Apple M1 systems.
#if _POSIX_C_SOURCE >= 200809L || defined(__APPLE__) && defined(__arm64__)
  #define UNIX_TIME_NS // Nanosecond time precision in Unix.
#endif

#endif // _UNIX

  typedef const wchar* MSGID;

#ifndef SSE_ALIGNMENT // No SSE use and no special data alignment is required.
  #define SSE_ALIGNMENT 1
#endif

// Solaris defines _LITTLE_ENDIAN or _BIG_ENDIAN.
#if defined(_LITTLE_ENDIAN) && !defined(LITTLE_ENDIAN)
  #define LITTLE_ENDIAN
#endif
#if defined(_BIG_ENDIAN) && !defined(BIG_ENDIAN)
  #define BIG_ENDIAN
#endif

#if !defined(LITTLE_ENDIAN) && !defined(BIG_ENDIAN)
  #if defined(__i386) || defined(i386) || defined(__i386__) || defined(__x86_64)
    #define LITTLE_ENDIAN
  #elif defined(BYTE_ORDER) && BYTE_ORDER == LITTLE_ENDIAN || defined(__LITTLE_ENDIAN__)
    #define LITTLE_ENDIAN
  #elif defined(BYTE_ORDER) && BYTE_ORDER == BIG_ENDIAN || defined(__BIG_ENDIAN__)
    #define BIG_ENDIAN
  #else
    #error "Neither LITTLE_ENDIAN nor BIG_ENDIAN are defined. Define one of them."
  #endif
#endif

#if defined(LITTLE_ENDIAN) && defined(BIG_ENDIAN)
  #if defined(BYTE_ORDER) && BYTE_ORDER == BIG_ENDIAN
    #undef LITTLE_ENDIAN
  #elif defined(BYTE_ORDER) && BYTE_ORDER == LITTLE_ENDIAN
    #undef BIG_ENDIAN
  #else
    #error "Both LITTLE_ENDIAN and BIG_ENDIAN are defined. Undef one of them."
  #endif
#endif

// Disable this optimization in Chromium. Although the underlying architecture
// may allow unaligned access, C and C++ themselves do not allow this. Rather,
// unaligned loads should be written with either memcpy, or by endian-agnostic
// reassembling of values with shifts and ORs. Modern compilers recognize these
// patterns and generate the unaligned load anyway.
#if !defined(CHROMIUM_UNRAR)
#if !defined(BIG_ENDIAN) && defined(_WIN_ALL) || defined(__i386__) || defined(__x86_64__) || defined(__aarch64__)
// Allow unaligned integer access, increases speed in some operations.
#define ALLOW_MISALIGNED
#endif
#endif

#endif // _RAR_OS_
