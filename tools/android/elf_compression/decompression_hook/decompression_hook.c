// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains userfaultfd watcher constructor that decompress
// parts of the library's code, compressed by compress_section.py script.
#include <asm-generic/ioctls.h>
#include <asm/unistd_64.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/userfaultfd.h>
#include <poll.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/syscall.h>
#include <sys/ttydefaults.h>
#include <sys/types.h>
#include <unistd.h>

// Symbol with virtual address of the start of ELF header of the library. Set by
// linker.
extern char __ehdr_start;

// This function can be used to prevent a value or expression from being
// optimized away by the compiler.
void DoNotOptimize(void* value) {
  __asm__ volatile("" : : "r,m"(value) : "memory");
}

// The following 4 arrays are here to be patched into by compress_section.py
// script. They currently contain magic bytes that will be used to locate them
// in the library file. DoNotOptimize method is applied to them at the
// beginning of the decompression hook to ensure that the arrays are not
// optimized away.
//
// TODO(crbug.com/41478372): Check if dl_iterate_phdr can replace the
// magic bytes approach.
unsigned char g_dummy_cut_range_begin[8] = {0x2e, 0x2a, 0xee, 0xf6,
                                            0x45, 0x03, 0xd2, 0x50};
unsigned char g_dummy_cut_range_end[8] = {0x52, 0x40, 0xeb, 0x9d,
                                          0xdb, 0x11, 0xed, 0x1a};
unsigned char g_dummy_compressed_range_begin[8] = {0x5e, 0x49, 0x4a, 0x4c,
                                                   0xae, 0x28, 0xc8, 0xbb};
unsigned char g_dummy_compressed_range_end[8] = {0xdd, 0x60, 0xed, 0xcf,
                                                 0xc3, 0x29, 0xa6, 0xd6};

static void DecompressPage(void* cut_start,
                           void* compressed_start,
                           void* page_start,
                           size_t page_size,
                           void* buffer) {
  // TODO(crbug.com/41478372): Update the method to work with arbitrary
  // block sizes.

  // This method is a stub to plug the decompression login into.
  uint64_t delta = page_start - cut_start;
  void* compressed_page_start = compressed_start + delta;
  memcpy(buffer, compressed_page_start, page_size);
}

typedef struct PollArray {
  struct pollfd* pollfd_array;
  size_t size;
  size_t capacity;
} PollArray;

PollArray* CreatePollArray() {
  PollArray* array = calloc(1, sizeof(PollArray));
  return array;
}

void DestroyPollArray(PollArray* arr) {
  free(arr->pollfd_array);
  free(arr);
}

void IncreasePollArrayCapacity(PollArray* arr) {
  if (arr->capacity == 0) {
    arr->capacity = 1;
  } else {
    arr->capacity *= 2;
  }
  arr->pollfd_array =
      realloc(arr->pollfd_array, arr->capacity * sizeof(struct pollfd));
}

void PollArrayPush(PollArray* arr, struct pollfd el) {
  if (arr->size == arr->capacity) {
    IncreasePollArrayCapacity(arr);
  }
  arr->pollfd_array[arr->size] = el;
  arr->size++;
}

void PollArrayPop(PollArray* arr) {
  arr->size--;
}

typedef struct ThreadArguments {
  int uffd;
  void* cut_start;
  void* compressed_start;
  size_t cut_length;
  size_t page_size;
} ThreadArguments;

static void HandlePageFault(int uffd,
                            void* pagefault_address,
                            void* cut_start,
                            void* compressed_start,
                            size_t page_size,
                            void* buffer) {
  DecompressPage(cut_start, compressed_start, pagefault_address, page_size,
                 buffer);
  struct uffdio_copy copy;
  copy.dst = (uint64_t)pagefault_address;
  copy.src = (uint64_t)buffer;
  copy.len = page_size;
  copy.mode = 0;
  if (ioctl(uffd, UFFDIO_COPY, &copy)) {
    switch (errno) {
      case EINVAL:
      case EAGAIN:
        perror("UFFDIO_COPY failed");
    }
  }
}

static void* WatcherThreadFunc(void* thread_args) {
  ThreadArguments* args = thread_args;

  void* buffer = calloc(args->page_size, sizeof(char));
  struct uffd_msg message;

  PollArray* poll_array = CreatePollArray();
  struct pollfd poll_fd = {.fd = args->uffd, .events = POLLIN};
  PollArrayPush(poll_array, poll_fd);

  // TODO(crbug.com/41478372): Use epoll instead
  while (poll_array->size &&
         poll(poll_array->pollfd_array, poll_array->size, -1) >= 0) {
    for (int i = 0; i < poll_array->size; i++) {
      struct pollfd* current_fd = &poll_array->pollfd_array[i];
      if (current_fd->revents & POLLIN) {
        read(current_fd->fd, &message, sizeof(message));
        if (message.event == UFFD_EVENT_FORK) {
          struct pollfd new_fd = {.fd = message.arg.fork.ufd, .events = POLLIN};
          PollArrayPush(poll_array, new_fd);
        }
        if (message.event == UFFD_EVENT_PAGEFAULT) {
          HandlePageFault(current_fd->fd, (void*)message.arg.pagefault.address,
                          args->cut_start, args->compressed_start,
                          args->page_size, buffer);
        }
      } else if (current_fd->revents & POLLHUP) {
        close(current_fd->fd);
        poll_array->pollfd_array[i] =
            poll_array->pollfd_array[poll_array->size - 1];
        PollArrayPop(poll_array);
      }
    }
  }
  // Everything died, the thread is free.
  free(buffer);
  free(thread_args);
  DestroyPollArray(poll_array);
  return NULL;
}

static int StartWatcherThread(void* cut_addr,
                              void* compressed_l,
                              size_t cut_length,
                              size_t page_size,
                              int uffd) {
  pthread_attr_t attr;
  if (pthread_attr_init(&attr)) {
    return -1;
  }
  if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) {
    pthread_attr_destroy(&attr);
    return -1;
  }
  ThreadArguments* args = malloc(sizeof(ThreadArguments));
  args->uffd = uffd;
  args->cut_start = cut_addr;
  args->compressed_start = compressed_l;
  args->page_size = page_size;

  pthread_t thread_id;
  if (pthread_create(&thread_id, &attr, WatcherThreadFunc, (void*)args)) {
    pthread_attr_destroy(&attr);
    free(args);
    return -1;
  }
  pthread_attr_destroy(&attr);
  return 0;
}

static int SetupUserfaultFd(void* cut_start, size_t cut_length) {
  int uffd = syscall(__NR_userfaultfd, O_NONBLOCK | O_CLOEXEC);
  if (uffd == -1) {
    perror("Userfaultfd syscall failed");
    return -1;
  }
  // Enabling userfaultfd.
  struct uffdio_api api = {.api = UFFD_API,
                           .features = UFFD_FEATURE_EVENT_FORK};
  if (ioctl(uffd, UFFDIO_API, &api)) {
    perror("ioctl UFFDIO_API failed");
    close(uffd);
    return -1;
  }
  // Setting userfaultfd watch over cut region.
  struct uffdio_register uffd_register;
  uffd_register.range.start = (uint64_t)cut_start;
  uffd_register.range.len = cut_length;
  uffd_register.mode = UFFDIO_REGISTER_MODE_MISSING;
  if (ioctl(uffd, UFFDIO_REGISTER, &uffd_register)) {
    perror("ioctl UFFDIO_REGISTER failed");
    close(uffd);
    return -1;
  }
  return uffd;
}

// Unregisters the userfaultfd watch on cut range. Used to revert to
// DecompressWholeRange in case of error during the creation of the thread.
static void UnregisterUserfaultFd(void* cut_start,
                                  size_t cut_length,
                                  int uffd) {
  struct uffdio_register uffd_unregister;
  uffd_unregister.range.start = (uint64_t)cut_start;
  uffd_unregister.range.len = cut_length;
  uffd_unregister.mode = UFFDIO_REGISTER_MODE_MISSING;
  // No error handling here since we are already resorting to the fallback
  // option.
  ioctl(uffd, UFFDIO_UNREGISTER, &uffd_unregister);
}

// Backup slow solution for the hook. Fully decompresses and populates
// the cut range. This method is used if the userfaultfd setup failed to ensure
// that the library will still function despite the failure.
static void DecompressWholeRange(void* cut_start,
                                 void* compressed_start,
                                 size_t cut_length,
                                 size_t page_size) {
  if (mprotect(cut_start, cut_length, PROT_READ | PROT_EXEC | PROT_WRITE)) {
    perror("Failed to enable PROT_WRITE on cut range");
    exit(1);
  }

  void* buffer = calloc(page_size, sizeof(char));
  for (uint64_t offset = 0; offset < cut_length; offset += page_size) {
    DecompressPage(cut_start, compressed_start, cut_start + offset, page_size,
                   buffer);
    memcpy(cut_start + offset, buffer, page_size);
  }
  free(buffer);

  if (mprotect(cut_start, cut_length, PROT_READ | PROT_EXEC)) {
    perror("Failed to disable PROT_WRITE on cut range");
    exit(1);
  }
}

static unsigned char* MapCutRange(void* cut_start, size_t cut_length) {
  void* addr = mmap(cut_start, cut_length, PROT_READ | PROT_EXEC,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  if (addr == MAP_FAILED) {
    perror("Constructor cut range mapping failed");
    // If we fail at this point of time there is no way for us to recover since
    // without valid mapping we can't change the cut region.
    exit(1);
  }
  return addr;
}

void* ConvertDummyArrayToAddress(void* dummy_array) {
  uintptr_t* value_ptr = (uintptr_t*)dummy_array;
  uintptr_t value = *value_ptr;
  value += (uintptr_t)&__ehdr_start;
  return (void*)value;
}

void __attribute__((constructor(0))) InitLibraryDecompressor() {
  // The constructor only works on 64 bit systems and as such expecting the
  // pointer size to be 8 bytes.
  // The constructor priority is set to 0(the highest) to ensure that it starts
  // as a first constructor.
  _Static_assert(sizeof(uint64_t) == sizeof(uintptr_t),
                 "Pointer size is not 8 bytes");

  DoNotOptimize(g_dummy_cut_range_begin);
  DoNotOptimize(g_dummy_cut_range_end);
  DoNotOptimize(g_dummy_compressed_range_begin);
  DoNotOptimize(g_dummy_compressed_range_end);

  void* cut_l = ConvertDummyArrayToAddress(g_dummy_cut_range_begin);
  void* cut_r = ConvertDummyArrayToAddress(g_dummy_cut_range_end);
  void* compressed_l =
      ConvertDummyArrayToAddress(g_dummy_compressed_range_begin);
  void* compressed_r = ConvertDummyArrayToAddress(g_dummy_compressed_range_end);

  uint64_t cut_range_length = (uintptr_t)cut_r - (uintptr_t)cut_l;
  void* cut_addr = MapCutRange(cut_l, cut_range_length);

  size_t page_size = sysconf(_SC_PAGESIZE);
  int uffd = SetupUserfaultFd(cut_addr, cut_range_length);
  if (uffd == -1) {
    DecompressWholeRange(cut_addr, compressed_l, cut_range_length, page_size);
    return;
  }
  if (StartWatcherThread(cut_addr, compressed_l, cut_range_length, page_size,
                         uffd)) {
    UnregisterUserfaultFd(cut_addr, cut_range_length, uffd);
    close(uffd);
    DecompressWholeRange(cut_addr, compressed_l, cut_range_length, page_size);
    return;
  }
}
