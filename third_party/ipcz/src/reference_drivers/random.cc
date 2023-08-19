// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/random.h"

#include <cstddef>
#include <cstdint>

#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/base/macros.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#elif BUILDFLAG(IS_FUCHSIA)
#include <zircon/syscalls.h>
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include <asm/unistd.h>
#include <sys/syscall.h>
#include <unistd.h>
#elif BUILDFLAG(IS_MAC)
#include <sys/random.h>
#include <unistd.h>
#elif BUILDFLAG(IS_NACL)
#include <nacl/nacl_random.h>
#endif

#if BUILDFLAG(IS_POSIX)
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#if BUILDFLAG(IS_WIN)
// Prototype for ProcessPrng.
// See: https://learn.microsoft.com/en-us/windows/win32/seccng/processprng
extern "C" {
BOOL WINAPI ProcessPrng(PBYTE pbData, SIZE_T cbData);
}
#endif

namespace ipcz::reference_drivers {

namespace {

#if BUILDFLAG(IS_WIN)
decltype(&ProcessPrng) GetProcessPrng() {
  HMODULE hmod = LoadLibraryW(L"bcryptprimitives.dll");
  ABSL_ASSERT(hmod);
  decltype(&ProcessPrng) process_prng_fn =
      reinterpret_cast<decltype(&ProcessPrng)>(
          GetProcAddress(hmod, "ProcessPrng"));
  ABSL_ASSERT(process_prng_fn);
  return process_prng_fn;
}
#endif

#if defined(OS_POSIX) && !BUILDFLAG(IS_MAC)
void RandomBytesFromDevUrandom(absl::Span<uint8_t> destination) {
  static int urandom_fd = [] {
    for (;;) {
      int result = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
      if (result >= 0) {
        return result;
      }
      ABSL_ASSERT(errno == EINTR);
    }
  }();

  while (!destination.empty()) {
    ssize_t result = read(urandom_fd, destination.data(), destination.size());
    if (result < 0) {
      ABSL_ASSERT(errno == EINTR);
      continue;
    }
    destination.remove_prefix(result);
  }
}
#endif

}  // namespace

void RandomBytes(absl::Span<uint8_t> destination) {
#if BUILDFLAG(IS_WIN)
  static decltype(&ProcessPrng) process_prng_fn = GetProcessPrng();
  process_prng_fn(destination.data(), destination.size());
#elif BUILDFLAG(IS_FUCHSIA)
  zx_cprng_draw(destination.data(), destination.size());
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  while (!destination.empty()) {
    ssize_t result =
        syscall(__NR_getrandom, destination.data(), destination.size(), 0);
    if (result == -1 && errno == EINTR) {
      continue;
    } else if (result > 0) {
      destination.remove_prefix(result);
    } else {
      RandomBytesFromDevUrandom(destination);
      return;
    }
  }
#elif BUILDFLAG(IS_MAC)
  const bool ok = getentropy(destination.data(), destination.size()) == 0;
  ABSL_ASSERT(ok);
#elif BUILDFLAG(IS_IOS)
  RandomBytesFromDevUrandom(destination);
#elif BUILDFLAG(IS_NACL)
  while (!destination.empty()) {
    size_t nread;
    nacl_secure_random(destination.data(), destination.size(), &nread);
    destination.remove_prefix(nread);
  }
#else
#error "Unsupported platform"
#endif
}

}  // namespace ipcz::reference_drivers
