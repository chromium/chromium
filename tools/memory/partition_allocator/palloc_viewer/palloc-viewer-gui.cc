// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This uses SDL+OpenGL as documented at
// <https://github.com/ocornut/imgui/blob/master/examples/example_sdl_opengl2/main.cpp>.

#include <assert.h>
#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

#define restrict __restrict__

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <vector>

#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui.h>
#include "ProggyTiny.ttf.h"
#include "imgui_impl_opengl2.h"
#include "imgui_impl_sdl.h"
#include "implot.h"

#include "common.h"

/* partitionalloc structs, must be kept roughly in sync */
struct PartitionBucket {
  unsigned long active_slot_spans_head;
  unsigned long empty_slot_spans_head;
  unsigned long decommitted_slot_spans_head;
  uint32_t slot_size;
  uint32_t num_system_pages_per_slot_span : 8;
  uint32_t num_full_slot_spans : 24;
};
struct PartitionSuperPageExtentEntry {
  unsigned long root;
  unsigned long extent_base;
  unsigned long extent_end;
  unsigned long next;
};
struct __attribute__((packed)) SlotSpanMetadata {
  unsigned long freelist_head;
  unsigned long next_slot_span;
  unsigned long bucket;
  uint32_t marked_full : 1;
  uint32_t num_allocated_slots : 13;
  uint32_t num_unprovisioned_slots : 13;
  uint32_t can_store_raw_size : 1;
  uint32_t freelist_is_sorted : 1;
  uint32_t unused1 : (32 - 1 - 2 * 13 - 1 - 1);
  uint16_t in_empty_cache : 1;
  uint16_t empty_cache_index : 7;
  uint16_t unused2 : (16 - 1 - 7);
};
struct PartitionPage {
  union {
    struct SlotSpanMetadata span;
    size_t raw_size; /* SubsequentPageMetadata */
    struct PartitionSuperPageExtentEntry head;
    struct {
      char pad[32 - sizeof(uint16_t)];
      uint16_t slot_span_metadata_offset;
    };
  };
};
static_assert(sizeof(struct PartitionPage) == 32);
struct ThreadCacheBucket {
  unsigned long freelist_head;
  unsigned char count;
  unsigned char limit;
  unsigned short slot_size;
};

#define SUPERPAGE_SIZE 0x200000UL
#define SUPERPAGE_MASK 0x1fffffUL
#define SUPERPAGE_PAGES 512
#define PAGES_PER_SPAN 4UL
#define SPANS_PER_SUPERPAGE (SUPERPAGE_PAGES / PAGES_PER_SPAN)
#define NUM_TCACHE_BUCKETS 41

#define PAGEMAP_SOFT_DIRTY 0x0080000000000000ULL
#define PAGEMAP_SWAP 0x4000000000000000ULL
#define PAGEMAP_PRESENT 0x8000000000000000ULL
#define PAGEMAP_EXCLUSIVE 0x0100000000000000ULL

#define VMA_R 1
#define VMA_W 2
#define VMA_X 4
#define VMA_SHARED 8
struct vma {
  unsigned long start, end;
  unsigned char perms;
  unsigned long inode;
  char* path; /* borrow from task_state::maps_buf */
  bool pa_superpage;
};

struct pa_bucket {
  struct PartitionBucket data;
  unsigned long addr;
  unsigned long span_pa_pages;
  unsigned long root;
  unsigned long objects_per_span;
  unsigned long tcache_count;
  std::vector<struct SlotSpanMetadata*> bucket_spans;
  char size_str[21];
};

#define SLOT_STATE_USED 0
#define SLOT_STATE_FREE 1
#define SLOT_STATE_UNPROVISIONED 2
#define SLOT_STATE_TCACHE 3
struct span_info {
  struct pa_bucket* bucket;
  std::vector<unsigned char> slot_states;
  bool decommitted;
};

struct partition;
struct superpage {
  unsigned long addr;
  struct superpage* extent_head;
  bool direct_mapped;
  struct partition* partition;
  uint64_t pagemap[SUPERPAGE_PAGES];
  struct PartitionPage meta_page[SPANS_PER_SUPERPAGE];
  struct span_info span_info[SPANS_PER_SUPERPAGE];
  bool ospage_has_allocations[SUPERPAGE_PAGES];
  bool ospage_has_tcache[SUPERPAGE_PAGES];
  bool ospage_has_unallocated[SUPERPAGE_PAGES];
};

struct partition {
  unsigned long addr;
  unsigned long superpage_count;
  std::unordered_map<unsigned long, std::unique_ptr<struct pa_bucket>>
      all_buckets;
};

struct thread_state {
  pid_t tid;

  /* from procfs */
  char comm[32];
  unsigned long minflt;
  unsigned long majflt;
  unsigned long utime;
  unsigned long stime;
  unsigned long starttime;
  unsigned long cpu;
  unsigned long delayacct;
  unsigned long voluntary_ctxt_switches;
  unsigned long nonvoluntary_ctxt_switches;

  /* from ptrace (cached to minimize interference) */
  unsigned long fsbase;

  /* from memory peek */
  unsigned long stackblock;
  unsigned long stackblock_size;
  unsigned long stack_phys_used;
  unsigned long stack_phys_dirty;
  unsigned char should_purge;
  struct ThreadCacheBucket tcache_buckets[NUM_TCACHE_BUCKETS];

  /* from previous */
  unsigned long flt_const_cycles;
  unsigned long cpu_const_cycles;
  unsigned long switches_const_cycles;
};

struct task_state {
  unsigned long collect_cycle;
  bool reader_active;
  char* maps_buf; /* owned; borrows in vma::path */
  struct vma* vmas;
  struct vma* stack_vma;
  size_t vma_count;
  struct superpage* superpages;
  size_t superpage_count;
  bool probed_payloads;
  std::unordered_map<unsigned long, std::unique_ptr<struct partition>>
      partitions;
  std::unordered_map<pid_t, std::unique_ptr<struct thread_state>> threads;
  struct thread_state* main_thread;

  /* overall PA stats */
  unsigned long stats_history_len;
#define STATS_HISTORY_MAX 300U
  double physical_allocated_KiB[STATS_HISTORY_MAX];
  double physical_tcache_KiB[STATS_HISTORY_MAX];
  double physical_free_KiB[STATS_HISTORY_MAX];
  ImU64 full_pages[STATS_HISTORY_MAX];
  ImU64 partial_pages[STATS_HISTORY_MAX];
  ImU64 tcache_and_free_pages[STATS_HISTORY_MAX];
  ImU64 free_pages[STATS_HISTORY_MAX];
};

struct task {
  /* const */
  pid_t pid;
  int task_fd;
  int pidfd;
  unsigned long pthread_block_offset;
  unsigned long pthread_stackblock_offset;
  unsigned long pthread_stackblock_size_offset;
  unsigned long thread_cache_registry_addr;
  unsigned long thread_cache_should_purge_offset;
  unsigned int tls_key;

  /* owned by collector */
  int maps_fd;
  int mem_fd;
  int pagemap_fd;
  size_t old_maps_len; /* hint from last read */
  unsigned long collect_cycle;

  /* locked */
  struct task_state* cur_state;

  /* shared */
  volatile bool enable_collection;
  volatile bool probe_payloads;
};

static int peek_buf(const struct task* t,
                    void* dst,
                    unsigned long src,
                    size_t len) {
  if (pread(t->mem_fd, dst, len, src) == (ssize_t)len)
    return 0;
  memset(dst, '\0', len);
  return -1;
}

static int open_task(struct task* restrict out, pid_t pid) {
  out->pid = pid;
  out->collect_cycle = 0;
  out->enable_collection = true;
  out->probe_payloads = false;
  out->cur_state = NULL;

  out->pidfd = syscall(__NR_pidfd_open, pid, 0);
  if (out->pidfd == -1)
    perror("pidfd_open");

  char path[40];
  sprintf(path, "/proc/%d", pid);
  out->task_fd = open(path, O_PATH);
  if (out->task_fd == -1)
    return -1;

  out->maps_fd = openat(out->task_fd, "maps", O_RDONLY);
  if (out->maps_fd == -1)
    goto err_maps;
  out->old_maps_len = 0x1000;

  out->mem_fd = openat(out->task_fd, "mem", O_RDONLY);
  if (out->mem_fd == -1)
    goto err_mem;

  out->pagemap_fd = openat(out->task_fd, "pagemap", O_RDONLY);
  if (out->pagemap_fd == -1)
    goto err_pagemap;

  {
    Dwfl* dwfl = addrlookup_init(pid);

    Dwfl_Module* libpthread_module = addrlookup_find_lib(dwfl, "/libpthread-");
    unsigned long pthread_bias;
    void* pthread_cu = lookup_cu(dwfl, libpthread_module,
                                 "pthread_getspecific.c", &pthread_bias);
    out->pthread_block_offset = addrlookup_get_struct_offset(
        pthread_cu, NULL, 0, "pthread", "specific_1stblock");
    out->pthread_stackblock_offset = addrlookup_get_struct_offset(
        pthread_cu, NULL, 0, "pthread", "stackblock");
    out->pthread_stackblock_size_offset = addrlookup_get_struct_offset(
        pthread_cu, NULL, 0, "pthread", "stackblock_size");

    unsigned long thread_cache_bias;
    void* thread_cache_cu =
        lookup_cu(dwfl, NULL,
                  "../../base/allocator/partition_allocator/src/"
                  "partition_alloc/thread_cache.cc",
                  &thread_cache_bias);
    const char* nspath[] = {"base", "internal", NULL};
    out->thread_cache_registry_addr = addrlookup_get_variable_address(
        thread_cache_cu, thread_cache_bias, nspath, 3, "g_instance");
    unsigned long tls_key_addr = addrlookup_get_variable_address(
        thread_cache_cu, thread_cache_bias, nspath, 2, "g_thread_cache_key");
    out->thread_cache_should_purge_offset = addrlookup_get_struct_offset(
        thread_cache_cu, nspath, 2, "ThreadCache", "should_purge_");
    if (peek_buf(out, &out->tls_key, tls_key_addr, sizeof(out->tls_key)))
      err(1, "unable to read g_thread_cache_key");
    addrlookup_finish(dwfl);

    printf(
        "g_instance=0x%lx; offsetof(struct pthread, specific_1stblock)=0x%lx; "
        "g_thread_cache_key=0x%x\n",
        out->thread_cache_registry_addr, out->pthread_block_offset,
        out->tls_key);
  }

  return 0;

err_pagemap:
  close(out->mem_fd);
err_mem:
  close(out->maps_fd);
err_maps:
  close(out->task_fd);
  return -1;
}

static int pagemap_read(const struct task* t,
                        uint64_t* out,
                        unsigned long addr,
                        size_t num_pages) {
  off_t off = addr / PAGE_SIZE * sizeof(*out);
  size_t len = sizeof(*out) * num_pages;
  return (pread(t->pagemap_fd, out, len, off) == (ssize_t)len) ? 0 : -1;
}

#if 0
// checks whether page is present or swapped.
// returns false on error.
// returns true for zeropage.
static bool page_present(const struct task *t, unsigned long addr) {
  uint64_t entry;
  if (pagemap_read(t, &entry, addr, 1))
    return false; // error
  return entry & 0xc000000000000000ULL;
}
#endif

// static const ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
static const ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.00f, 1.00f);
static const ImVec4 highlight_color = ImVec4(0.5f, 0.0f, 0.0f, 1.0f);

static const ImVec4 swap_color = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
// static const ImVec4 not_present_color = ImVec4(0.00f, 0.50f, 0.00f, 1.00f);
static const ImVec4 not_present_color = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
static const ImVec4 exclusive_color = ImVec4(0.0f, 0.3f, 1.0f, 1.00f);
static const ImVec4 shared_color = ImVec4(1.00f, 1.00f, 0.00f, 1.00f);
static const ImVec4 dirty_color = ImVec4(0.0f, 0.7f, 0.7f, 1.00f);

static const ImVec4 span_color_active = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
static const ImVec4 span_color_decommitted = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

void append_legend(const char* name,
                   const char* label,
                   const ImVec4& color,
                   const char* help_text) {
  ImGui::SameLine(0, 20);
  ImGui::ColorButton(name, color, ImGuiColorEditFlags_NoTooltip,
                     ImVec2(12, 12));
  ImGui::SameLine();
  ImGui::TextUnformatted(label);
  if (help_text && ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50.0f);
    ImGui::TextUnformatted(help_text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

/* read a complete file, independent of read offset. caller initializes out_len
 * with vague size hint. */
static int read_whole_file(int fd,
                           char** restrict outp,
                           size_t* restrict out_len) {
  size_t len = *out_len;
  if (len < 64)
    len = 64;
  if (len < SIZE_MAX / 4)
    len = len + (len >> 3);  // some extra space
  char* buf = (char*)malloc(len);
  if (!buf)
    return -1;

  size_t offset = 0;
  ssize_t res;
  while ((res = pread(fd, buf + offset, len - offset, offset)) > 0) {
    offset += res;

    if (offset == len) {
      len = len + (len >> 2);
      char* buf_new = (char*)realloc(buf, len);
      if (buf_new == NULL) {
        free(buf);
        return -1;
      }
      buf = buf_new;
    }
  }

  if (res == -1) {
    free(buf);
    return -1;
  }

  *outp = buf;
  *out_len = offset;
  return 0;
}

static int collect_mmap(struct task* t, struct task_state* restrict state) {
  size_t len = t->old_maps_len;
  char* buf;
  if (read_whole_file(t->maps_fd, &buf, &len))
    return -1;
  char* end = buf + len;

  // count VMAs
  char* line_start = buf;
  char* eol;
  state->vma_count = 0;
  while ((eol = (char*)memchr(line_start, '\n', end - line_start)) != NULL) {
    line_start = eol + 1;
    state->vma_count++;
  }
  state->vmas = new struct vma[state->vma_count];
  state->stack_vma = NULL;

  {
    line_start = buf;
    size_t vma_idx = 0;
    while ((eol = (char*)memchr(line_start, '\n', end - line_start)) != NULL) {
      struct vma* vma = &state->vmas[vma_idx++];
      *eol = '\0';
      char* itemp = line_start;
      line_start = eol + 1;  // this may be ==end

      char* endp;
      vma->start = strtoul(itemp, &endp, 16);
      if (*endp != '-')
        goto err_parse;

      itemp = endp + 1;
      vma->end = strtoul(itemp, &endp, 16);
      if (*endp != ' ')
        goto err_parse;

      itemp = endp + 1;
      if (eol - itemp < 5)
        goto err_parse;
      vma->perms = ((itemp[0] == 'r') ? VMA_R : 0) |
                   ((itemp[1] == 'w') ? VMA_W : 0) |
                   ((itemp[2] == 'x') ? VMA_X : 0) |
                   ((itemp[3] == 's') ? VMA_SHARED : 0);
      if (itemp[4] != ' ')
        goto err_parse;

      // offset
      itemp = itemp + 5;
      endp = strchr(itemp, ' ');
      if (!endp)
        goto err_parse;

      // dev
      itemp = endp + 1;
      endp = strchr(itemp, ' ');
      if (!endp)
        goto err_parse;

      // inode
      itemp = endp + 1;
      vma->inode = strtoul(itemp, &endp, 10);
      if (*endp != ' ' && *endp)
        goto err_parse;

      while (*endp == ' ')
        endp++;
      vma->path = (*endp) ? endp : NULL;
      if (vma->path && strcmp(vma->path, "[stack]") == 0) {
        state->stack_vma = vma;
      }
    }
  }

  state->maps_buf = buf;
  t->old_maps_len = len;
  return 0;

err_parse:
  delete[] state->vmas;
  free(buf);
  return -1;
}

static int find_pa_regions(struct task* t, struct task_state* restrict state) {
  size_t pa_superpage_count = 0;
  for (size_t vma_idx = 1; vma_idx < state->vma_count - 1; vma_idx++) {
    /* look for a metadata page at page offset 1 inside a 2MiB-aligned region...
     */
    struct vma* vma = &state->vmas[vma_idx];
    vma->pa_superpage = false;
    if ((vma->start & SUPERPAGE_MASK) != PAGE_SIZE)
      continue;
    if (vma->end != vma->start + PAGE_SIZE)
      continue;
    if (vma->perms != (VMA_R | VMA_W) || vma->inode)
      continue;
    unsigned long super_base = vma->start & ~SUPERPAGE_MASK;
    unsigned long super_end = super_base + SUPERPAGE_SIZE;

    /* ... surrounded by guard pages (1 before, 2 after) ... */
    struct vma* prev = &state->vmas[vma_idx - 1];
    if (prev->end != vma->start || prev->perms != 0)
      continue;
    struct vma* next = &state->vmas[vma_idx + 1];
    if (next->start != vma->end || next->end - next->start < 0x2000 ||
        next->perms != 0 || next->inode)
      continue;

    /* ... and with the whole superpage 2MiB region mapped. */
    for (size_t idx2 = vma_idx + 1; true; idx2++) {
      if (idx2 == state->vma_count)
        goto next_vma;
      if (state->vmas[idx2].start != state->vmas[idx2 - 1].end)
        goto next_vma;
      if (state->vmas[idx2].perms & (VMA_X | VMA_SHARED))
        goto next_vma;
      if (state->vmas[idx2].inode)
        goto next_vma;
      if (state->vmas[idx2].end >= super_end)
        break;
    }

    vma->pa_superpage = true;
    pa_superpage_count++;
  next_vma:;
  }

  state->superpages = new struct superpage[pa_superpage_count];

  size_t sp_idx = 0;
  struct superpage* extent_head = NULL;
  unsigned long meta_page_end = 0;
  for (size_t vma_idx = 1; vma_idx < state->vma_count - 1; vma_idx++) {
    struct vma* vma = &state->vmas[vma_idx];
    if (!vma->pa_superpage)
      continue;

    struct superpage* sp = &state->superpages[sp_idx];
    sp->addr = vma->start & ~SUPERPAGE_MASK;
    if (pagemap_read(t, sp->pagemap, sp->addr, SUPERPAGE_PAGES)) {
      // Something went very wrong... set the whole range to "not present"
      // and try to continue anyway.
      memset(sp->pagemap, '\0', sizeof(sp->pagemap));
    }
    if (peek_buf(t, &sp->meta_page, sp->addr + PAGE_SIZE,
                 sizeof(sp->meta_page))) {
      continue;
    }

    // check that the root pointer points to readable memory
    unsigned long dummy;
    if (peek_buf(t, &dummy, sp->meta_page[0].head.root, sizeof(unsigned long)))
      continue;

    if (sp->meta_page[1].span.bucket - sp->addr < 2 * PAGE_SIZE)
      sp->direct_mapped = true;

    if (sp->meta_page[0].head.extent_base == sp->addr) {
      // point of no return
      meta_page_end = sp->meta_page[0].head.extent_end;
      extent_head = sp;
    } else if (sp->addr < meta_page_end &&
               sp->meta_page[0].head.extent_base == 0) {
    } else {
      continue;
    }
    sp->extent_head = extent_head;

    unsigned long partition_addr = sp->meta_page[0].head.root;
    if (!state->partitions.contains(partition_addr)) {
      std::unique_ptr<struct partition> new_part =
          std::make_unique<struct partition>();
      new_part->addr = partition_addr;
      new_part->superpage_count = 0;
      state->partitions.insert({partition_addr, std::move(new_part)});
    }
    auto part_iter = state->partitions.find(partition_addr);
    assert(part_iter != state->partitions.end());
    struct partition* partition = part_iter->second.get();
    sp->partition = partition;
    partition->superpage_count++;

    for (unsigned long span = 0; span < SPANS_PER_SUPERPAGE; span++)
      sp->span_info[span].bucket = nullptr;
    for (unsigned long span = 1; span < SPANS_PER_SUPERPAGE;) {
      unsigned long bucket_addr = sp->meta_page[span].span.bucket;
      // printf("span %lu: bucket_addr=0x%lx slot_span_metadata_offset=%u\n",
      // span, bucket_addr, sp->meta_page[span].slot_span_metadata_offset);
      if (bucket_addr == 0) {
        span++;
        continue;
      }
      if (!partition->all_buckets.contains(bucket_addr)) {
        std::unique_ptr<struct pa_bucket> new_bucket =
            std::make_unique<struct pa_bucket>();
        new_bucket->addr = bucket_addr;
        new_bucket->root = sp->meta_page[0].head.root;
        if (peek_buf(t, &new_bucket->data, bucket_addr,
                     sizeof(new_bucket->data))) {
          fprintf(stderr, "failed to fetch bucket 0x%lx\n", bucket_addr);
          memset(&new_bucket->data, '\0', sizeof(new_bucket->data));
        }
        new_bucket->span_pa_pages =
            (new_bucket->data.num_system_pages_per_slot_span +
             (PAGES_PER_SPAN - 1)) /
            PAGES_PER_SPAN;
        if (new_bucket->data.slot_size >= 16) {
          new_bucket->objects_per_span =
              new_bucket->data.num_system_pages_per_slot_span * PAGE_SIZE /
              new_bucket->data.slot_size;
        } else {
          // avoid DIV/0
          new_bucket->objects_per_span = 0;
        }
        new_bucket->tcache_count = 0;
        char size_str[11];
        snprintf(size_str, sizeof(size_str), "%x", new_bucket->data.slot_size);

        int charidx;
        int outcharidx;
        for (charidx = 0, outcharidx = 0; size_str[charidx]; charidx++) {
          if (charidx % 2 == 0)
            new_bucket->size_str[outcharidx++] = '\n';
          new_bucket->size_str[outcharidx++] = size_str[charidx];
        }
        new_bucket->size_str[outcharidx] = '\0';

        partition->all_buckets.insert({bucket_addr, std::move(new_bucket)});
      }
      auto bucket_iter = partition->all_buckets.find(bucket_addr);
      assert(bucket_iter != partition->all_buckets.end());
      struct pa_bucket* bucket = bucket_iter->second.get();
      sp->span_info[span].bucket = bucket;
      bucket->bucket_spans.push_back(&sp->meta_page[span].span);

      // printf("span %lu for bucket 0x%x bytes, 0x%x pages: pa_pages=%lu\n",
      //    span, bucket->data.slot_size,
      //    bucket->data.num_system_pages_per_slot_span, bucket->span_pa_pages);

      unsigned long span_pa_pages;
      if (bucket->span_pa_pages > 0 &&
          bucket->span_pa_pages <= SPANS_PER_SUPERPAGE) {
        // for (unsigned long j=1; j<bucket->span_pa_pages; j++) {
        //  printf("  slot_span_metadata_offset=%u\n",
        //  sp->meta_page[span+j].slot_span_metadata_offset);
        //}
        span_pa_pages = bucket->span_pa_pages;
      } else {
        span_pa_pages = 1;  // probably broken, but avoid endless loop at least
      }

      std::vector<unsigned char>& slot_states = sp->span_info[span].slot_states;
      slot_states.resize(bucket->objects_per_span, SLOT_STATE_USED);
      unsigned long unprovisioned =
          sp->meta_page[span].span.num_unprovisioned_slots;
      if (unprovisioned > bucket->objects_per_span) {
        fprintf(stderr,
                "bogus unprovisioned @ SP=0x%lx span=%lu: bucket=0x%lx/0x%lx "
                "slot_size=0x%x, system_pages_per_slot_span=0x%x, "
                "objects_per_span=0x%lx, unprovisioned=0x%lx\n",
                sp->addr, span, bucket_addr, bucket->addr,
                bucket->data.slot_size,
                bucket->data.num_system_pages_per_slot_span,
                bucket->objects_per_span, unprovisioned);
        unprovisioned = 0;
      }
      for (unsigned long idx = bucket->objects_per_span - unprovisioned;
           idx < bucket->objects_per_span; idx++)
        slot_states.at(idx) = SLOT_STATE_UNPROVISIONED;

      if (state->probed_payloads) {
        unsigned long span_start = sp->addr + span * PAGES_PER_SPAN * PAGE_SIZE;
        unsigned long freelist_ptr = sp->meta_page[span].span.freelist_head;
        while (freelist_ptr) {
          // validate
          if (freelist_ptr < span_start) {
            fprintf(stderr, "bogus freelist pointer: 0x%lx not in span 0x%lx\n",
                    freelist_ptr, span_start);
            break;
          }
          unsigned long span_offset = freelist_ptr - span_start;
          if (span_offset % bucket->data.slot_size != 0) {
            fprintf(
                stderr,
                "bogus freelist pointer: offset 0x%lx not aligned to 0x%x\n",
                span_offset, bucket->data.slot_size);
            break;
          }
          unsigned long slot_idx = span_offset / bucket->data.slot_size;
          if (slot_idx >= bucket->objects_per_span - unprovisioned) {
            fprintf(stderr,
                    "bogus freelist pointer: slot 0x%lx >= 0x%lx - 0x%lx\n",
                    slot_idx, bucket->objects_per_span, unprovisioned);
            break;
          }

          // mark
          slot_states.at(slot_idx) = SLOT_STATE_FREE;

          // fetch next
          unsigned long encoded_freeptr[2];
          if (peek_buf(t, encoded_freeptr, freelist_ptr,
                       sizeof(encoded_freeptr))) {
            fprintf(stderr, "freelist walk failed read\n");
            break;
          }
          if (encoded_freeptr[0] != ~encoded_freeptr[1]) {
            fprintf(stderr, "encoded freeptr is inconsistent\n");
            break;
          }
          freelist_ptr = __builtin_bswap64(encoded_freeptr[0]);
        }
      }
      sp->span_info[span].decommitted =
          (sp->meta_page[span].span.freelist_head == 0 &&
           sp->meta_page[span].span.num_allocated_slots == 0);

      /* step to next span, must be at end of loop */
      span += span_pa_pages;
    }

    sp_idx++;
  }
  state->superpage_count = sp_idx;
  // exit(1);
  return 0;
}

static int read_thread_state(int pid, struct user_regs_struct* out) {
  int r;

  // attach and asynchronously request stop
  r = ptrace(PTRACE_ATTACH, pid, 0, 0);
  if (r == -1)
    return -1;

  // wait for SIGSTOP, but reinject everything else
  while (1) {
    int status;
    if (waitpid(pid, &status, 0) != pid)
      err(1, "waitpid on ptrace child");
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
      // we raced, it's dead
      errno = ESRCH;
      return -1;
    }
    assert(WIFSTOPPED(status));
    int sig = WSTOPSIG(status);
    if (sig == SIGSTOP) {
      // wheee, it's stopped now!
      break;
    } else {
      // bleh. reinject and loop back.
      if (ptrace(PTRACE_CONT, pid, NULL, sig))
        err(1, "reinject signal");
    }
  }

  // grab register state
  if (ptrace(PTRACE_GETREGS, pid, NULL, out))
    err(1, "PTRACE_GETREGS");

  if (ptrace(PTRACE_DETACH, pid, 0, 0))
    err(1, "PTRACE_DETACH");

  return 0;
}

/*
static int cmp_superpage(const void *a_, const void *b_) {
  struct superpage **a = (struct superpage **)a_;
  struct superpage **b = (struct superpage **)b_;
  if ((*a)->addr < (*b)->addr)
    return -1;
  if ((*a)->addr > (*b)->addr)
    return 1;
  return 0;
}
*/
static struct superpage* find_superpage(struct task_state* state,
                                        unsigned long sp_addr) {
  if (state->superpage_count == 0)
    return NULL;
  sp_addr = sp_addr & ~SUPERPAGE_MASK;

  size_t low = 0;
  size_t high = state->superpage_count - 1;
  while (1) {
    size_t middle = (low + high) / 2;
    if (state->superpages[middle].addr == sp_addr) {
      return &state->superpages[middle];
    } else if (low == high) {
      return NULL;
    } else if (state->superpages[middle].addr > sp_addr) {
      if (low == middle)
        return NULL;
      high = middle - 1;
    } else {
      low = middle + 1;
    }
  }
}

static void collect_threads(struct task* t,
                            struct task_state* state,
                            struct task_state* old_state) {
  state->main_thread = NULL;
  int tasks_fd = openat(t->task_fd, "task", O_RDONLY);
  if (tasks_fd == -1)
    return;
  DIR* tasks_dir = fdopendir(tasks_fd);
  if (!tasks_dir) {
    close(tasks_fd);
    return;
  }
  while (1) {
  next_item:;
    struct dirent* dent = readdir(tasks_dir);
    if (dent == NULL)
      break;

    pid_t tid = atoi(dent->d_name);
    if (tid == 0)
      continue;

    char stat_path[50];
    snprintf(stat_path, sizeof(stat_path), "%d/stat", tid);
    int stat_fd = openat(dirfd(tasks_dir), stat_path, O_RDONLY);
    if (stat_fd == -1)
      continue;
    char stat_buf[0x1000];
    ssize_t stat_len = read(stat_fd, stat_buf, sizeof(stat_buf) - 1);
    close(stat_fd);
    if (stat_len <= 0)
      continue;
    stat_buf[stat_len] = '\0';

    std::unique_ptr<struct thread_state> thread =
        std::make_unique<struct thread_state>();
    thread->tid = tid;

    // field 2: comm
    char* comm_start_paren = strchr(stat_buf, '(');
    if (comm_start_paren == NULL)
      continue;
    char* comm_end_paren = strchr(comm_start_paren, ')');
    if (comm_end_paren == NULL)
      continue;
    char* p = comm_end_paren;
    size_t comm_len =
        std::min((size_t)(comm_end_paren - (comm_start_paren + 1)),
                 sizeof(thread->comm) - 1);
    memcpy(thread->comm, comm_start_paren + 1, comm_len);
    thread->comm[comm_len] = '\0';

    // https://man7.org/linux/man-pages/man5/proc.5.html
    // field 10: minflt
    // field 12: majflt
    // field 14: utime (in ticks)
    // field 15: stime (in ticks)
    // field 22: starttime (in ticks)
    // field 39: cpu
    // field 42: block io delay (including swapin) (in ticks)
    for (int i = 2; i < 10; i++) {
      p = strchr(p, ' ');
      if (!p)
        goto next_item;
      p++;
    }
    thread->minflt = strtoul(p, NULL, 10);
    for (int i = 10; i < 12; i++) {
      p = strchr(p, ' ');
      if (!p)
        goto next_item;
      p++;
    }
    thread->majflt = strtoul(p, NULL, 10);
    for (int i = 12; i < 14; i++) {
      p = strchr(p, ' ');
      if (!p)
        goto next_item;
      p++;
    }
    thread->utime = strtoul(p, NULL, 10);
    for (int i = 14; i < 15; i++) {
      p = strchr(p, ' ');
      if (!p)
        goto next_item;
      p++;
    }
    thread->stime = strtoul(p, NULL, 10);
    for (int i = 15; i < 22; i++) {
      p = strchr(p, ' ');
      if (!p)
        goto next_item;
      p++;
    }
    thread->starttime = strtoul(p, NULL, 10);
    for (int i = 22; i < 39; i++) {
      p = strchr(p, ' ');
      if (!p)
        goto next_item;
      p++;
    }
    thread->cpu = strtoul(p, NULL, 10);
    for (int i = 39; i < 42; i++) {
      p = strchr(p, ' ');
      if (!p)
        goto next_item;
      p++;
    }
    thread->delayacct = strtoul(p, NULL, 10);

    char status_path[50];
    snprintf(status_path, sizeof(status_path), "%d/status", tid);
    int status_fd = openat(dirfd(tasks_dir), status_path, O_RDONLY);
    if (status_fd == -1)
      continue;
    char status_buf[0x1000];
    ssize_t status_len = read(status_fd, status_buf, sizeof(status_buf) - 1);
    close(status_fd);
    if (status_len <= 0)
      continue;
    status_buf[status_len] = '\0';
    p = strstr(status_buf, "\nvoluntary_ctxt_switches:\t");
    if (!p)
      continue;
    p += strlen("\nvoluntary_ctxt_switches:\t");
    thread->voluntary_ctxt_switches = strtoul(p, NULL, 10);
    p = strstr(status_buf, "\nnonvoluntary_ctxt_switches:\t");
    if (!p)
      continue;
    p += strlen("\nnonvoluntary_ctxt_switches:\t");
    thread->nonvoluntary_ctxt_switches = strtoul(p, NULL, 10);

    struct thread_state* old_thread = NULL;
    if (old_state) {
      auto thread_iter = old_state->threads.find(tid);
      if (thread_iter != state->threads.end()) {
        old_thread = thread_iter->second.get();
        if (old_thread->starttime != thread->starttime)
          old_thread = NULL;
      }
    }

    if (old_thread) {
      thread->fsbase = old_thread->fsbase;
    } else {
      struct user_regs_struct regs;
      if (read_thread_state(tid, &regs)) {
        // only print errors on first iteration
        if (!old_state)
          perror(
              "unable to read thread state, maybe needs root privs because of "
              "Yama or maybe GDB/strace is already attached");
        continue;
      }
      thread->fsbase = regs.fs_base;
    }

    thread->flt_const_cycles =
        (old_thread && old_thread->majflt == thread->majflt &&
         old_thread->minflt == thread->minflt)
            ? old_thread->flt_const_cycles + 1
            : 0;
    thread->cpu_const_cycles = (old_thread && old_thread->cpu == thread->cpu)
                                   ? old_thread->cpu_const_cycles + 1
                                   : 0;
    thread->switches_const_cycles = (old_thread &&
                                     old_thread->voluntary_ctxt_switches ==
                                         thread->voluntary_ctxt_switches &&
                                     old_thread->nonvoluntary_ctxt_switches ==
                                         thread->nonvoluntary_ctxt_switches)
                                        ? old_thread->switches_const_cycles + 1
                                        : 0;

    unsigned long tcache_addr;
    if (peek_buf(
            t, &tcache_addr,
            thread->fsbase + t->pthread_block_offset + 0x10 * t->tls_key + 0x8,
            sizeof(tcache_addr)) == 0) {
      peek_buf(t, thread->tcache_buckets, tcache_addr,
               sizeof(thread->tcache_buckets));
      peek_buf(t, &thread->should_purge,
               tcache_addr + t->thread_cache_should_purge_offset,
               sizeof(thread->should_purge));
    }

    if (state->probed_payloads) {
      for (unsigned long bucket_idx = 0; bucket_idx < NUM_TCACHE_BUCKETS;
           bucket_idx++) {
        unsigned long freelist_ptr =
            thread->tcache_buckets[bucket_idx].freelist_head;
        while (freelist_ptr) {
          struct superpage* sp = find_superpage(state, freelist_ptr);
          if (sp == NULL) {
            fprintf(stderr, "unable to find superpage for freelist ptr 0x%lx\n",
                    freelist_ptr);
            break;
          }
          unsigned long offset_in_superpage = freelist_ptr & SUPERPAGE_MASK;
          unsigned long raw_span_idx =
              offset_in_superpage / (PAGES_PER_SPAN * PAGE_SIZE);
          unsigned long metadata_offset =
              sp->meta_page[raw_span_idx].slot_span_metadata_offset;
          if (metadata_offset > raw_span_idx) {
            fprintf(stderr, "slot_span_metadata_offset impossibly big\n");
            break;
          }
          unsigned long span = raw_span_idx - metadata_offset;
          struct pa_bucket* bucket = sp->span_info[span].bucket;
          if (bucket == NULL) {
            fprintf(stderr, "tcache walk unable to find bucket\n");
            break;
          }

          unsigned long span_start =
              sp->addr + span * PAGES_PER_SPAN * PAGE_SIZE;
          unsigned long span_offset = freelist_ptr - span_start;
          if (span_offset % bucket->data.slot_size != 0) {
            fprintf(stderr,
                    "tcache: bogus freelist pointer: offset 0x%lx not aligned "
                    "to 0x%x\n",
                    span_offset, bucket->data.slot_size);
            break;
          }
          unsigned long slot_idx = span_offset / bucket->data.slot_size;
          if (slot_idx >= bucket->objects_per_span) {
            fprintf(stderr,
                    "tcache: bogus freelist pointer: slot 0x%lx >= 0x%lx\n",
                    slot_idx, bucket->objects_per_span);
            break;
          }

          // mark
          std::vector<unsigned char>& slot_states =
              sp->span_info[span].slot_states;
          slot_states.at(slot_idx) = SLOT_STATE_TCACHE;
          bucket->tcache_count++;

          // fetch next
          unsigned long encoded_freeptr[2];
          if (peek_buf(t, encoded_freeptr, freelist_ptr,
                       sizeof(encoded_freeptr))) {
            fprintf(stderr, "tcache: freelist walk failed read\n");
            break;
          }
          if (encoded_freeptr[0] != ~encoded_freeptr[1]) {
            fprintf(stderr, "tcache: encoded freeptr is inconsistent\n");
            break;
          }
          freelist_ptr = __builtin_bswap64(encoded_freeptr[0]);
        }
      }
    }

    thread->stack_phys_used = 0;
    thread->stack_phys_dirty = 0;

    if (tid != t->pid) {
      peek_buf(t, &thread->stackblock,
               thread->fsbase + t->pthread_stackblock_offset,
               sizeof(thread->stackblock));
      peek_buf(t, &thread->stackblock_size,
               thread->fsbase + t->pthread_stackblock_size_offset,
               sizeof(thread->stackblock_size));
    } else if (state->stack_vma) {
      thread->stackblock = state->stack_vma->start;
      thread->stackblock_size = state->stack_vma->end - state->stack_vma->start;
    } else {
      thread->stackblock = 0;
      thread->stackblock_size = 0;
    }
    // sanity check
    if (thread->stackblock_size < 1024UL * 1024 * 1024 &&
        thread->stackblock < thread->stackblock + thread->stackblock_size &&
        thread->stackblock % PAGE_SIZE == 0 &&
        thread->stackblock_size % PAGE_SIZE == 0 && thread->stackblock != 0) {
      unsigned long num_pages = thread->stackblock_size / PAGE_SIZE;
      for (unsigned long page = 0; page < num_pages; page += 16) {
        unsigned long remaining_pages = std::min(num_pages - page, 16UL);
        uint64_t pagemap[16];
        if (pagemap_read(t, pagemap, thread->stackblock + page * PAGE_SIZE,
                         remaining_pages)) {
          thread->stack_phys_used = 0;
          break;
        }
        for (unsigned long i = 0; i < remaining_pages; i++) {
          if (pagemap[i] & (PAGEMAP_PRESENT | PAGEMAP_SWAP)) {
            if (pagemap[i] & PAGEMAP_SOFT_DIRTY)
              thread->stack_phys_dirty += PAGE_SIZE;
            thread->stack_phys_used += PAGE_SIZE;
          }
        }
      }
    }

    if (tid == t->pid)
      state->main_thread = thread.get();
    state->threads.insert({tid, std::move(thread)});
  }
  closedir(tasks_dir);
}

static void compute_usage_stats(struct task* t,
                                struct task_state* state,
                                struct task_state* old_state) {
  if (old_state) {
    unsigned long offset =
        (old_state->stats_history_len == STATS_HISTORY_MAX) ? 1 : 0;
    state->stats_history_len = old_state->stats_history_len - offset;
#define COPY_STATS_ARRAY(NAME)                  \
  memcpy(state->NAME, old_state->NAME + offset, \
         sizeof(state->NAME[0]) * state->stats_history_len)
    COPY_STATS_ARRAY(physical_allocated_KiB);
    COPY_STATS_ARRAY(physical_tcache_KiB);
    COPY_STATS_ARRAY(physical_free_KiB);
    COPY_STATS_ARRAY(full_pages);
    COPY_STATS_ARRAY(partial_pages);
    COPY_STATS_ARRAY(tcache_and_free_pages);
    COPY_STATS_ARRAY(free_pages);
  } else {
    state->stats_history_len = 0;
  }

  if (!state->probed_payloads) {
    state->stats_history_len = 0;
    return;
  }
  state->physical_allocated_KiB[state->stats_history_len] = 0;
  state->physical_tcache_KiB[state->stats_history_len] = 0;
  state->physical_free_KiB[state->stats_history_len] = 0;
  state->full_pages[state->stats_history_len] = 0;
  state->partial_pages[state->stats_history_len] = 0;
  state->tcache_and_free_pages[state->stats_history_len] = 0;
  state->free_pages[state->stats_history_len] = 0;

  for (unsigned long superpage_idx = 0; superpage_idx < state->superpage_count;
       superpage_idx++) {
    struct superpage* sp = &state->superpages[superpage_idx];
    for (unsigned long i = 0; i < SUPERPAGE_PAGES; i++) {
      sp->ospage_has_allocations[i] = false;
      sp->ospage_has_tcache[i] = false;
      sp->ospage_has_unallocated[i] = false;
    }
    for (unsigned long span = 1; span < SPANS_PER_SUPERPAGE;) {
      struct span_info* sinfo = &sp->span_info[span];
      if (!sinfo->bucket) {
        span++;
        continue;
      }
      if (span + sinfo->bucket->span_pa_pages >= SPANS_PER_SUPERPAGE) {
        fprintf(stderr, "span doesn't fit\n");
        break;
      }
      for (unsigned int slot = 0; slot < sinfo->bucket->objects_per_span;
           slot++) {
        unsigned long slot_offset = slot * sinfo->bucket->data.slot_size;
        unsigned long slot_offset_end =
            slot_offset + sinfo->bucket->data.slot_size;
        unsigned char slot_state = sinfo->slot_states.at(slot);
        for (unsigned long offset = slot_offset; true;) {
          unsigned long page_idx = span * PAGES_PER_SPAN + offset / PAGE_SIZE;
          bool present =
              (sp->pagemap[page_idx] & (PAGEMAP_PRESENT | PAGEMAP_EXCLUSIVE)) ==
                  (PAGEMAP_PRESENT | PAGEMAP_EXCLUSIVE) ||
              (sp->pagemap[page_idx] & PAGEMAP_SWAP) != 0;
          unsigned long next_page = (offset & PAGE_MASK) + PAGE_SIZE;
          unsigned long fragment_end = std::min(next_page, slot_offset_end);
          if (present) {
            if (slot_state == SLOT_STATE_USED) {
              state->physical_allocated_KiB[state->stats_history_len] +=
                  (fragment_end - offset) / 1024.0f;
              sp->ospage_has_allocations[page_idx] = true;
            }
            if (slot_state == SLOT_STATE_TCACHE) {
              state->physical_tcache_KiB[state->stats_history_len] +=
                  (fragment_end - offset) / 1024.0f;
              sp->ospage_has_tcache[page_idx] = true;
            }
            if (slot_state == SLOT_STATE_FREE)
              state->physical_free_KiB[state->stats_history_len] +=
                  (fragment_end - offset) / 1024.0f;
            if (slot_state != SLOT_STATE_USED)
              sp->ospage_has_unallocated[page_idx] = true;
          }
          if (slot_offset_end <= next_page)
            break;
          offset = next_page;
        }
      }

      /* go to next */
      span += sinfo->bucket->span_pa_pages;
    }

    for (unsigned long page = 0; page < SUPERPAGE_PAGES; page++) {
      if ((sp->pagemap[page] & (PAGEMAP_PRESENT | PAGEMAP_EXCLUSIVE)) !=
              (PAGEMAP_PRESENT | PAGEMAP_EXCLUSIVE) &&
          (sp->pagemap[page] & PAGEMAP_SWAP) == 0)
        continue;
      if (!sp->ospage_has_unallocated[page]) {
        state->full_pages[state->stats_history_len]++;
      } else if (sp->ospage_has_allocations[page]) {
        state->partial_pages[state->stats_history_len]++;
      } else if (sp->ospage_has_tcache[page]) {
        state->tcache_and_free_pages[state->stats_history_len]++;
      } else {
        state->free_pages[state->stats_history_len]++;
      }
    }
  }
  state->stats_history_len++;
}

static struct task* global_task;
static pthread_mutex_t update_lock = PTHREAD_MUTEX_INITIALIZER;
static uint32_t sdl_force_repaint_event;

static int try_collect(struct task* t) {
  int ret = 1;
  struct task_state* old_state =
      t->cur_state;  // we are the updater, we can read the pointer without
                     // locking

  t->collect_cycle++;
  struct task_state* state = new struct task_state;
  if (!state)
    return 1;
  state->collect_cycle = t->collect_cycle;
  state->reader_active = false;
  state->probed_payloads = t->probe_payloads;
  if (collect_mmap(t, state))
    goto err_mmap;
  if (find_pa_regions(t, state))
    goto err_find_pa;
  collect_threads(t, state, old_state);
  compute_usage_stats(t, state, old_state);

  // we've collected a new state, swap out the old one
  {
    if (pthread_mutex_lock(&update_lock))
      errx(1, "pthread_mutex_lock");
    t->cur_state = state;
    state = (!old_state || old_state->reader_active) ? NULL : old_state;
    if (pthread_mutex_unlock(&update_lock))
      errx(1, "pthread_mutex_unlock");
  }

  ret = 0;
  if (state == NULL)
    return ret;

  delete[] state->superpages;
err_find_pa:
  delete[] state->vmas;
  free(state->maps_buf);
err_mmap:
  delete state;
  return ret;
}

static void* collector_thread_fn(void* data_) {
  prctl(PR_SET_NAME, "collector");
  while (true) {
    usleep(200000);
    struct task* t = global_task;
    if (!t->enable_collection)
      continue;
    if (try_collect(t)) {
      printf("collector_thread: try_collect() failed\n");
      continue;
    }
    SDL_Event ev = {.type = sdl_force_repaint_event};
    SDL_PushEvent(&ev);
  }
}

static bool show_soft_dirty = false;
static bool enable_logscale = false;
static bool show_logical_page_state = false;
static ImFont* small_font;

void render_superpages_legend(struct task_state* state) {
  if (show_logical_page_state) {
    ImGui::TextUnformatted("Legend (logical state, OS pages):");
  } else {
    ImGui::TextUnformatted("Legend (physical state, OS pages):");
  }
  if (state->probed_payloads) {
    append_legend("legend:uncommitted", "uncommitted[?]", not_present_color,
                  "does not exclusively use RAM or swap space.\n"
                  "no page present / zeropage / CoW.\n"
                  "to distinguish further, restart target and monitor,\n"
                  "and don't enable swap-disturbing probes.");
    if (show_logical_page_state) {
      append_legend("legend:full", "fully used", exclusive_color, NULL);
      append_legend(
          "legend:partial", "partially used [?]", shared_color,
          "includes pages with thread cache but no actually allocated memory");
      append_legend("legend:free", "completely free", swap_color,
                    "unused except for freelist pointers");
    } else {
      append_legend(
          "legend:committed", "committed[?]", exclusive_color,
          "uses RAM (not shared with any other process) or swap space.\n");
    }
  } else {
    append_legend("legend:not-present", "not-present", not_present_color, NULL);
    append_legend("legend:exclusive", "exclusive[?]", exclusive_color,
                  "normal anonymous memory.\npresent in RAM.\nnot shared with "
                  "any other process.");
    append_legend("legend:shared", "copy-on-write[?]", shared_color,
                  "copy-on-write memory.\n"
                  "normally created via one of:\n"
                  " - fork()\n"
                  " - read fault on not-present memory (zeropage)\n"
                  " - accidentally by probing memory that used to be "
                  "not-present (zeropage)");
    append_legend("legend:swap", "swap[?]", swap_color,
                  "swapped out by the kernel.\n"
                  "WARNING:\n"
                  "inspecting heap metadata swaps metadata memory back in!");
  }
  if (show_soft_dirty) {
    append_legend("legend:dirty", "dirty[?]", dirty_color,
                  "modified after soft-dirty state was last reset");
  }

  ImGui::TextUnformatted("Legend (span state, painted as border):");
  append_legend("legend:pa-normal", "active", span_color_active, NULL);
  append_legend("legend:pa-decom", "decommitted", span_color_decommitted, NULL);
}
void render_superpages(struct task* task, struct task_state* state) {
  unsigned long page_tables_phys = state->superpage_count * PAGE_SIZE / 1024;
  unsigned long pagemap_phys = 0;
  unsigned long swap_size = 0;
  for (size_t super_idx = 0; super_idx < state->superpage_count; super_idx++) {
    struct superpage* sp = &state->superpages[super_idx];
    for (size_t i = 0; i < SUPERPAGE_SIZE / PAGE_SIZE; i++) {
      uint64_t entry = sp->pagemap[i];
      if (entry & PAGEMAP_SWAP)
        swap_size += PAGE_SIZE / 1024;
      if (entry & (PAGEMAP_EXCLUSIVE | PAGEMAP_SWAP))
        pagemap_phys += PAGE_SIZE / 1024;
    }
  }
  ImGui::Text(
      "%lu superpages; %lu KiB virtual; %lu KiB private allocated (including "
      "%lu KiB metadata and %lu KiB swap; NOT COUNTING kernel overhead like "
      "struct page and %lu KiB L1 page tables) [?]",
      state->superpage_count, SUPERPAGE_SIZE * state->superpage_count / 1024,
      pagemap_phys, state->superpage_count * PAGE_SIZE / 1024, swap_size,
      page_tables_phys);
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("NOTE: swap is always accounted as private");

  static bool wide_display = false;
  ImGui::Checkbox("wide display (for 4K screens)", &wide_display);
#define OSPAGE_WIDTH_NARROW 2.0f
#define OSPAGE_WIDTH_WIDE 4.0f
#define OSPAGE_HEIGHT 7.0f
#define OSPAGE_SPACING 1.0f
#define PAPAGE_SPACING_NARROW 2.0f
#define PAPAGE_SPACING_WIDE 5.0f
#define BUCKET_BORDER_WIDTH 1.0f
#define PAPAGE_HEIGHT \
  (BUCKET_BORDER_WIDTH + OSPAGE_HEIGHT + BUCKET_BORDER_WIDTH)
  float OSPAGE_WIDTH = wide_display ? OSPAGE_WIDTH_WIDE : OSPAGE_WIDTH_NARROW;
  float PAPAGE_SPACING =
      wide_display ? PAPAGE_SPACING_WIDE : PAPAGE_SPACING_NARROW;

  if (task->probe_payloads) {
    ImGui::Checkbox("show logical page states", &show_logical_page_state);
  } else {
    show_logical_page_state = false;
  }

  render_superpages_legend(state);

  for (size_t super_idx = 0; super_idx < state->superpage_count; super_idx++) {
    struct superpage* sp = &state->superpages[super_idx];

    ImGui::PushFont(small_font);
    ImGui::Text("%014lx", sp->addr);
    ImGui::PopFont();
    ImGui::SameLine(0.0f, 8.0f);

    for (unsigned int span = 0; span < SPANS_PER_SUPERPAGE;) {
      struct pa_bucket* bucket = sp->span_info[span].bucket;
      struct SlotSpanMetadata* span_meta = &sp->meta_page[span].span;
      unsigned long pa_pages = bucket ? bucket->span_pa_pages : 1;
      if (pa_pages > SPANS_PER_SUPERPAGE - span)
        pa_pages = SPANS_PER_SUPERPAGE - span;  // hack in case of error
      unsigned long os_pages = pa_pages * PAGES_PER_SPAN;

      ImGui::SameLine(0.0f, PAPAGE_SPACING);
      ImDrawList* draw_list = ImGui::GetWindowDrawList();
      const ImVec2 pos = ImGui::GetCursorScreenPos();

      float span_width = os_pages * OSPAGE_WIDTH +
                         (os_pages - os_pages / 4) * OSPAGE_SPACING +
                         (os_pages / 4 - 1) * PAPAGE_SPACING;
      if (bucket) {
        ImU32 span_color =
            ImColor(sp->span_info[span].decommitted ? span_color_decommitted
                                                    : span_color_active);
        draw_list->AddRect(ImVec2(pos.x - BUCKET_BORDER_WIDTH, pos.y),
                           ImVec2(pos.x + span_width + BUCKET_BORDER_WIDTH,
                                  pos.y + BUCKET_BORDER_WIDTH + OSPAGE_HEIGHT +
                                      BUCKET_BORDER_WIDTH),
                           ImColor(span_color), 0.0f, ImDrawFlags_None);
      }

      for (unsigned int i = 0; i < os_pages; i++) {
        float os_page_x = pos.x + i * OSPAGE_WIDTH +
                          (i - i / 4) * OSPAGE_SPACING +
                          (i / 4) * PAPAGE_SPACING;
        float os_page_y = pos.y + BUCKET_BORDER_WIDTH;

        int os_page_idx = i + span * PAGES_PER_SPAN;
        assert(os_page_idx >= 0 && os_page_idx < 512);
        uint64_t entry = sp->pagemap[os_page_idx];
        const ImVec4* color;

        if (show_logical_page_state) {
          if ((entry & (PAGEMAP_PRESENT | PAGEMAP_EXCLUSIVE)) !=
                  (PAGEMAP_PRESENT | PAGEMAP_EXCLUSIVE) &&
              (entry & PAGEMAP_SWAP) == 0) {
            color = &not_present_color;
          } else if (!sp->ospage_has_unallocated[os_page_idx]) {
            color = &exclusive_color;
          } else if (sp->ospage_has_allocations[os_page_idx] ||
                     sp->ospage_has_tcache[os_page_idx]) {
            color = &shared_color;
          } else {
            color = &swap_color;
          }
        } else {
          if (show_soft_dirty && (entry & PAGEMAP_SOFT_DIRTY)) {
            color = &dirty_color;
          } else if (entry & PAGEMAP_SWAP) {
            color = state->probed_payloads ? &exclusive_color : &swap_color;
          } else if ((entry & PAGEMAP_PRESENT) == 0) {
            color = &not_present_color;
          } else if (entry & PAGEMAP_EXCLUSIVE) {
            color = &exclusive_color;
          } else {
            color = state->probed_payloads ? &not_present_color : &shared_color;
          }
        }

        draw_list->AddRectFilled(
            ImVec2(os_page_x, os_page_y),
            ImVec2(os_page_x + OSPAGE_WIDTH, os_page_y + OSPAGE_HEIGHT),
            ImColor(*color), 0.0f, ImDrawFlags_None);
      }

      ImGui::Dummy(ImVec2(span_width, BUCKET_BORDER_WIDTH + OSPAGE_HEIGHT +
                                          BUCKET_BORDER_WIDTH));
      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        if (span == 0)
          ImGui::Text("METADATA/GUARD PAGE");
        if (span == SPANS_PER_SUPERPAGE - 1)
          ImGui::Text("GUARD PAGE");
        if (bucket) {
          ImGui::Text("bucket 0x%lx\n", bucket->addr);
          ImGui::Text("slot size: %u\n", bucket->data.slot_size);
          unsigned long waste =
              bucket->data.num_system_pages_per_slot_span * PAGE_SIZE -
              bucket->objects_per_span * bucket->data.slot_size;
          ImGui::Text("system pages per slot span: %u\n",
                      bucket->data.num_system_pages_per_slot_span);
          ImGui::Text("objects per span: %lu\n", bucket->objects_per_span);
          ImGui::Text("padding wasted per span (outside slot): %lu\n", waste);
          ImGui::Text("allocated slots: %d (%.0f%%)\n",
                      span_meta->num_allocated_slots,
                      100.0f * span_meta->num_allocated_slots /
                          (float)bucket->objects_per_span);
          ImGui::Text("unprovisioned slots: %d (%.0f%%)\n",
                      span_meta->num_unprovisioned_slots,
                      100.0f * span_meta->num_unprovisioned_slots /
                          (float)bucket->objects_per_span);
          if (pa_pages > 1 && bucket->objects_per_span == 1) {
            unsigned long raw_size = sp->meta_page[span + 1].raw_size;
            ImGui::Text("raw size: %lu (%.0f%%)\n", raw_size,
                        100.0f * raw_size / (float)bucket->data.slot_size);
          }
          if (sp->span_info[span].decommitted) {
            ImGui::Text("*** DECOMMITTED ***");
          } else if (state->probed_payloads) {
            std::string slot_states_formatted;
            unsigned long line_len = 0;
            for (unsigned char slot_state : sp->span_info[span].slot_states) {
              if (slot_state == SLOT_STATE_USED) {
                slot_states_formatted.push_back('_');
              } else if (slot_state == SLOT_STATE_FREE) {
                slot_states_formatted.push_back('#');
              } else if (slot_state == SLOT_STATE_UNPROVISIONED) {
                slot_states_formatted.push_back('U');
              } else if (slot_state == SLOT_STATE_TCACHE) {
                slot_states_formatted.push_back('T');
              } else {
                slot_states_formatted.push_back('?');
              }
              line_len++;
              if (line_len % 64 == 0)
                slot_states_formatted.push_back('\n');
            }
            ImGui::Text(
                "\nLegend: [_] used   [#] free   [U] unprovisioned   [T] "
                "thread cache");
            ImGui::Text("%s", slot_states_formatted.c_str());
          } else {
            ImGui::Text("enable swap-disturbing heap probes for details");
          }
        }
        ImGui::EndTooltip();
      }

      span += pa_pages;
    }
  }
}

static bool bucket_cmp_size(struct pa_bucket* a, struct pa_bucket* b) {
  return a->data.slot_size < b->data.slot_size;
}

void render_buckets(struct task* task, struct task_state* state) {
  if (!ImGui::BeginTabBar("partition", ImGuiTabBarFlags_None))
    return;
  for (auto& [part_addr, partition_ref] : state->partitions) {
    struct partition* partition = partition_ref.get();
    char tab_name[128];
    snprintf(tab_name, IM_ARRAYSIZE(tab_name), "root 0x%lx (%lu superpages)",
             partition->addr, partition->superpage_count);
    if (!ImGui::BeginTabItem(tab_name))
      continue;

    std::vector<struct pa_bucket*> buckets;
    for (auto& [addr, bucket_ref] : partition->all_buckets) {
      buckets.push_back(bucket_ref.get());
    }
    std::sort(buckets.begin(), buckets.end(), bucket_cmp_size);

    std::vector<char*> bucket_labels;
    std::vector<float> bucket_vmem_allocated;
    std::vector<float> bucket_vmem_tcache;
    for (struct pa_bucket* bucket : buckets) {
      bucket_labels.push_back(bucket->size_str);
      unsigned int bucket_allocated = 0;
      for (struct SlotSpanMetadata* span : bucket->bucket_spans) {
        if (span->num_allocated_slots <=
            bucket->objects_per_span)  // sanity check
          bucket_allocated += span->num_allocated_slots;
      }
      bucket_vmem_allocated.push_back(bucket_allocated *
                                      bucket->data.slot_size / 1024.0);
      bucket_vmem_tcache.push_back(bucket->tcache_count *
                                   bucket->data.slot_size / 1024.0);
    }

    // allocated objects per bucket
    ImPlot::SetNextPlotTicksX(0, buckets.size() - 1, buckets.size(),
                              bucket_labels.data());
    if (ImPlot::BeginPlot(
            "allocated virtual memory by bucket (_NOT_ physical memory)",
            "bucket", "virtual memory (KiB)", ImVec2(-1, 200),
            ImPlotFlags_NoChild, 0,
            enable_logscale ? ImPlotAxisFlags_LogScale : 0)) {
      ImPlot::SetLegendLocation(ImPlotLocation_North,
                                ImPlotOrientation_Horizontal);
      if (state->probed_payloads) {
        ImPlot::PlotBars("allocated", bucket_vmem_allocated.data(),
                         bucket_vmem_allocated.size(), 0.3, -0.15);
        ImPlot::PlotBars("per-thread cache", bucket_vmem_tcache.data(),
                         bucket_vmem_tcache.size(), 0.3, +0.15);
      } else {
        ImPlot::PlotBars("allocated", bucket_vmem_allocated.data(),
                         bucket_vmem_allocated.size());
      }
      ImPlot::EndPlot();
    }

#if 0
    // physical memory per bucket
    ImPlot::SetNextPlotTicksX(0, buckets.size()-1, buckets.size(), bucket_labels.data());
    if (ImPlot::BeginPlot("_physical_ memory by bucket", "bucket", "physical memory (KiB)",
                          ImVec2(-1, 200), ImPlotFlags_NoChild, 0, enable_logscale ? ImPlotAxisFlags_LogScale : 0)) {
      ImPlot::SetLegendLocation(ImPlotLocation_North, ImPlotOrientation_Horizontal);
      ImPlot::PlotBars("allocated", bucket_pmem_allocated.data(), bucket_pmem_allocated.size()/*, 0.3, -0.15*/);
      ImPlot::EndPlot();
    }
#endif

    ImGui::Text("per-bucket histograms of allocated slots per span:");
    ImGui::BeginChild("buckets", ImVec2(0, 0), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    bool can_stack = false;
    for (struct pa_bucket* bucket : buckets) {
      if (can_stack)
        ImGui::SameLine();
      ImGui::PushID(bucket->data.slot_size);
      ImGui::BeginGroup();
      // ImGui::Text("size %u: %d spans\n", bucket->data.slot_size,
      // (int)bucket->bucket_spans.size()); if (bucket->bucket_spans.size() ==
      // 1)
      //  ImGui::Text("xxx %d\n", bucket->bucket_spans[0]->num_allocated_slots);

      // ImGui::Text("allocated slots:\n");
      // ImU32 allocated_histogram_values[bucket->objects_per_span+1];
      // memset(allocated_histogram_values, '\0',
      // sizeof(allocated_histogram_values));
      std::vector<ImU32> allocated_slots_by_bucket;
      for (struct SlotSpanMetadata* span : bucket->bucket_spans) {
        if (span->num_allocated_slots <= bucket->objects_per_span)
          // allocated_histogram_values[allocated_slots]++;
          allocated_slots_by_bucket.push_back(span->num_allocated_slots);
      }
      // ImGui::PlotHistogram("##allocated", allocated_histogram_values,
      // IM_ARRAYSIZE(allocated_histogram_values),
      //                     0, NULL, FLT_MAX, FLT_MAX, ImVec2(0, 80));
      ImPlot::SetNextPlotLimits(0, bucket->objects_per_span, 0,
                                bucket->bucket_spans.size(), ImGuiCond_Always);
      ImPlot::PushColormap(ImPlotColormap_Dark);
      if (ImPlot::BeginPlot("##allocated objects per bucket", NULL, NULL,
                            ImVec2(256, 128), ImPlotFlags_NoChild, 0,
                            enable_logscale ? ImPlotAxisFlags_LogScale : 0)) {
        // ImPlot::PlotStems("number of buckets", allocated_histogram_values,
        // bucket->objects_per_span+1);
        char legend_str[30];
        snprintf(legend_str, sizeof(legend_str), "0x%x",
                 bucket->data.slot_size);
        int bins = std::min(bucket->objects_per_span + 1, 64UL);
        ImPlot::PlotHistogram(legend_str, allocated_slots_by_bucket.data(),
                              allocated_slots_by_bucket.size(), bins, false,
                              false, ImPlotRange(0, bucket->objects_per_span));
        ImPlot::EndPlot();
      }
      ImPlot::PopColormap();

      ImGui::EndGroup();
      ImGui::PopID();

      float last_x2 = ImGui::GetItemRectMax().x;
      float next_x2 = last_x2 + ImGui::GetStyle().ItemSpacing.x +
                      ImGui::GetItemRectSize().x;
      can_stack = (next_x2 < ImGui::GetWindowPos().x +
                                 ImGui::GetWindowContentRegionMax().x);
    }
    ImGui::EndChild();

    ImGui::EndTabItem();
  }
  ImGui::EndTabBar();
}

void render_threads(struct task* task, struct task_state* state) {
  const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;

  unsigned long total_stack_phys_used = 0;
  unsigned long total_stack_phys_dirty = 0;
  unsigned long total_cache_bytes = 0;
  for (auto& [tid, thread_ref] : state->threads) {
    struct thread_state* thread = thread_ref.get();
    total_stack_phys_used += thread->stack_phys_used;
    total_stack_phys_dirty += thread->stack_phys_dirty;
    for (int i = 0; i < NUM_TCACHE_BUCKETS; i++) {
      total_cache_bytes +=
          thread->tcache_buckets[i].count * thread->tcache_buckets[i].slot_size;
    }
  }
  char stack_dirty_total_text[50];
  if (show_soft_dirty) {
    snprintf(stack_dirty_total_text, sizeof(stack_dirty_total_text), "%lu KiB",
             total_stack_phys_dirty / 1024);
  } else {
    strcpy(stack_dirty_total_text, "N/A [requires enabling soft dirty]");
  }
  ImGui::Text("Total stack memory: total %lu KiB, dirty %s",
              total_stack_phys_used / 1024, stack_dirty_total_text);
  ImGui::Text(
      "Total thread cache memory (WITHOUT accounting for any kind of "
      "overhead): %.1f KiB",
      total_cache_bytes / 1024.0f);

  if (!ImGui::BeginTable("threads", 8 + NUM_TCACHE_BUCKETS,
                         ImGuiTableFlags_Borders |
                             ImGuiTableFlags_SizingFixedFit |
                             ImGuiTableFlags_NoHostExtendX))
    return;

  ImGui::TableSetupColumn("TID", ImGuiTableColumnFlags_WidthFixed,
                          TEXT_BASE_WIDTH * 7);
  ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthFixed,
                          TEXT_BASE_WIDTH * 15);
  ImGui::TableSetupColumn("CPU", ImGuiTableColumnFlags_WidthFixed,
                          TEXT_BASE_WIDTH * 3);
  ImGui::TableSetupColumn("voluntary/\nforced\nswitches",
                          ImGuiTableColumnFlags_WidthFixed,
                          TEXT_BASE_WIDTH * 10);
  ImGui::TableSetupColumn("minor/major\nfaults",
                          ImGuiTableColumnFlags_WidthFixed,
                          TEXT_BASE_WIDTH * 11);
  ImGui::TableSetupColumn("stack\ntotal/\ndirty",
                          ImGuiTableColumnFlags_WidthFixed,
                          TEXT_BASE_WIDTH * 8);
  ImGui::TableSetupColumn("purge\npending", ImGuiTableColumnFlags_WidthFixed,
                          TEXT_BASE_WIDTH * 7);
  ImGui::TableSetupColumn("cache RAM\nWITHOUT\nOVERHEAD",
                          ImGuiTableColumnFlags_WidthFixed,
                          TEXT_BASE_WIDTH * 9);
  for (int i = 0; i < NUM_TCACHE_BUCKETS; i++) {
    if (state->main_thread) {
      unsigned short slot_size =
          state->main_thread->tcache_buckets[i].slot_size;
      char bucket_name[10];
      snprintf(bucket_name, sizeof(bucket_name), "%hx", slot_size);
      ImGui::TableSetupColumn(bucket_name, ImGuiTableColumnFlags_WidthFixed,
                              TEXT_BASE_WIDTH * (slot_size ? 4 : 1));
    } else {
      ImGui::TableSetupColumn("????", ImGuiTableColumnFlags_WidthFixed,
                              TEXT_BASE_WIDTH * 4);
    }
  }
  ImGui::TableHeadersRow();
  for (auto& [tid, thread_ref] : state->threads) {
    struct thread_state* thread = thread_ref.get();
    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    ImGui::Text("%d", thread->tid);

    ImGui::TableNextColumn();
    ImGui::Text("%s", thread->comm);

    ImGui::TableNextColumn();
    if (thread->cpu_const_cycles < 3)
      ImGui::TableSetBgColor(
          ImGuiTableBgTarget_CellBg,
          ImGui::GetColorU32(ImVec4(0.8f - thread->cpu_const_cycles * 0.3f,
                                    0.0f, 0.0f, 1.0f)));
    ImGui::Text("%lu", thread->cpu);

    ImGui::TableNextColumn();
    if (thread->switches_const_cycles < 3)
      ImGui::TableSetBgColor(
          ImGuiTableBgTarget_CellBg,
          ImGui::GetColorU32(ImVec4(0.8f - thread->switches_const_cycles * 0.3f,
                                    0.0f, 0.0f, 1.0f)));
    ImGui::Text("%lu\n%lu", thread->voluntary_ctxt_switches,
                thread->nonvoluntary_ctxt_switches);

    ImGui::TableNextColumn();
    if (thread->flt_const_cycles < 3)
      ImGui::TableSetBgColor(
          ImGuiTableBgTarget_CellBg,
          ImGui::GetColorU32(ImVec4(0.8f - thread->flt_const_cycles * 0.3f,
                                    0.0f, 0.0f, 1.0f)));
    ImGui::Text("%lu\n%lu", thread->minflt, thread->majflt);

    ImGui::TableNextColumn();
    if (thread->stack_phys_used == 0) {
      ImGui::Text("???\n???");
    } else {
      ImGui::Text("%6lu K", thread->stack_phys_used / 1024);
      if (show_soft_dirty) {
        ImGui::Text("%6lu K", thread->stack_phys_dirty / 1024);
      } else {
        ImGui::Text("N/A");
      }
    }

    ImGui::TableNextColumn();
    if (thread->should_purge) {
      ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg,
                             ImGui::GetColorU32(highlight_color));
      ImGui::Text("X");
    } else {
      ImGui::Text(" ");
    }

    ImGui::TableNextColumn();
    unsigned long cache_bytes = 0;
    for (int i = 0; i < NUM_TCACHE_BUCKETS; i++) {
      cache_bytes +=
          thread->tcache_buckets[i].count * thread->tcache_buckets[i].slot_size;
    }
    ImGui::Text("%6.1lf K", cache_bytes / 1024.0f);

    for (int i = 0; i < NUM_TCACHE_BUCKETS; i++) {
      ImGui::TableNextColumn();
      if (thread->tcache_buckets[i].count == 0) {
        ImGui::TableSetBgColor(
            ImGuiTableBgTarget_CellBg,
            ImGui::GetColorU32(ImVec4(0.0f, 0.5f, 0.0f, 1.0f)));
      }
      if (thread->tcache_buckets[i].count != 0 ||
          thread->tcache_buckets[i].limit != 0) {
        ImGui::Text("%hhu/\n%hhu", thread->tcache_buckets[i].count,
                    thread->tcache_buckets[i].limit);
      }
    }
  }
  ImGui::EndTable();
}

void render_overview(struct task* task, struct task_state* state) {
  if (state->probed_payloads) {
    unsigned long all_span_pages =
        state->full_pages[state->stats_history_len - 1] +
        state->partial_pages[state->stats_history_len - 1] +
        state->tcache_and_free_pages[state->stats_history_len - 1] +
        state->free_pages[state->stats_history_len - 1];
    double all_span_pages_KiB = all_span_pages * PAGE_SIZE / 1024.0f;
    ImGui::Text(
        "total physical memory in spans (*excluding* metadata pages and such): "
        "%lu KiB",
        all_span_pages * (PAGE_SIZE / 1024));
    ImGui::Text("physical memory per slot state:");
    ImGui::Text(
        "  %.1f KiB (%.2f%%) slot-allocated",
        state->physical_allocated_KiB[state->stats_history_len - 1],
        100.0f * state->physical_allocated_KiB[state->stats_history_len - 1] /
            all_span_pages_KiB);
    ImGui::Text("  %.1f KiB (%.2f%%) thread-cache-slots",
                state->physical_tcache_KiB[state->stats_history_len - 1],
                100.0f *
                    state->physical_tcache_KiB[state->stats_history_len - 1] /
                    all_span_pages_KiB);
    ImGui::Text("  %.1f KiB (%.2f%%) free",
                state->physical_free_KiB[state->stats_history_len - 1],
                100.0f *
                    state->physical_free_KiB[state->stats_history_len - 1] /
                    all_span_pages_KiB);
    ImGui::Text("OS physical page stats:");
    ImGui::Text("            full: %lu (%.2f%%)\n",
                (unsigned long)state->full_pages[state->stats_history_len - 1],
                100.0f * state->full_pages[state->stats_history_len - 1] /
                    all_span_pages);
    ImGui::Text(
        "  partially used: %lu (%.2f%%)\n",
        (unsigned long)state->partial_pages[state->stats_history_len - 1],
        100.0f * state->partial_pages[state->stats_history_len - 1] /
            all_span_pages);
    ImGui::Text("     tcache+free: %lu (%.2f%%)\n",
                (unsigned long)
                    state->tcache_and_free_pages[state->stats_history_len - 1],
                100.0f *
                    state->tcache_and_free_pages[state->stats_history_len - 1] /
                    all_span_pages);
    ImGui::Text("            free: %lu (%.2f%%)\n",
                (unsigned long)state->free_pages[state->stats_history_len - 1],
                100.0f * state->free_pages[state->stats_history_len - 1] /
                    all_span_pages);

    double max_slot_mem = 0;
    ImU64 max_span_pages = 0;
    for (unsigned long i = 0; i < state->stats_history_len; i++) {
      max_slot_mem = std::max({max_slot_mem, state->physical_allocated_KiB[i],
                               state->physical_tcache_KiB[i],
                               state->physical_free_KiB[i]});
      max_span_pages = std::max(
          {max_span_pages, state->full_pages[i], state->partial_pages[i],
           state->tcache_and_free_pages[i], state->free_pages[i]});
    }

    ImPlot::SetNextPlotLimitsX(0, STATS_HISTORY_MAX, ImGuiCond_Always);
    ImPlot::SetNextPlotLimitsY(0, max_slot_mem, ImGuiCond_Always);
    if (ImPlot::BeginPlot("physical memory by slot state", "time",
                          "KiB (physical)", ImVec2(-1, 200),
                          ImPlotFlags_NoChild)) {
      ImPlot::PlotLine("allocated", state->physical_allocated_KiB,
                       state->stats_history_len);
      ImPlot::PlotLine("thread cache", state->physical_tcache_KiB,
                       state->stats_history_len);
      ImPlot::PlotLine("free", state->physical_free_KiB,
                       state->stats_history_len);
      ImPlot::EndPlot();
    }

    ImPlot::SetNextPlotLimitsX(0, STATS_HISTORY_MAX, ImGuiCond_Always);
    ImPlot::SetNextPlotLimitsY(0, max_span_pages, ImGuiCond_Always);
    if (ImPlot::BeginPlot("physical OS pages (in spans) by state of contained "
                          "spans (including partial overlap)",
                          "time", "pages (physical)", ImVec2(-1, 200),
                          ImPlotFlags_NoChild)) {
      ImPlot::PlotLine("fully allocated", state->full_pages,
                       state->stats_history_len);
      ImPlot::PlotLine("partially allocated", state->partial_pages,
                       state->stats_history_len);
      ImPlot::PlotLine("free except for tcache", state->tcache_and_free_pages,
                       state->stats_history_len);
      ImPlot::PlotLine("completely free", state->free_pages,
                       state->stats_history_len);
      ImPlot::EndPlot();
    }

  } else {
    ImGui::Text(
        "<enable swap-disturbing heap probes for slot-state-related stats>");
  }
}

int main(int argc, char** argv) {
  if (argc != 2)
    errx(1, "usage: %s <pid>", argv[0]);
  struct task task;
  if (open_task(&task, atoi(argv[1])))
    err(1, "unable to open task");
  global_task = &task;  // for now we only have one task

  if (try_collect(&task))
    err(1, "initial info collection failed");

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    errx(1, "SDL_Init: %s", SDL_GetError());
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_WindowFlags window_flags =
      (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                        SDL_WINDOW_ALLOW_HIGHDPI /*| SDL_WINDOW_MAXIMIZED*/
      );
  SDL_Window* window =
      SDL_CreateWindow("PartitionAlloc inspector", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1920, 1080, window_flags);
  SDL_GLContext gl_context = SDL_GL_CreateContext(window);
  SDL_GL_MakeCurrent(window, gl_context);
  SDL_GL_SetSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  ImGui::StyleColorsDark();
  ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
  ImGui_ImplOpenGL2_Init();
  io.Fonts->AddFontDefault();
  small_font = io.Fonts->AddFontFromMemoryCompressedTTF(
      ProggyTiny_compressed_data, ProggyTiny_compressed_size, 10.0f);

  sdl_force_repaint_event = SDL_RegisterEvents(1);
  pthread_t collector_thread;
  if (pthread_create(&collector_thread, NULL, collector_thread_fn, NULL))
    errx(1, "pthread_create");

  while (1) {
    SDL_Event event;
    bool need_repaint = false;
    while (true) {
      if (need_repaint) {
        if (!SDL_PollEvent(&event))
          break;
      } else {
        if (!SDL_WaitEvent(&event))
          break;
      }
      if (ImGui_ImplSDL2_ProcessEvent(&event))
        need_repaint = true;
      if (event.type == SDL_QUIT)
        goto exit;
      if (event.type == SDL_WINDOWEVENT &&
          event.window.event == SDL_WINDOWEVENT_CLOSE &&
          event.window.windowID == SDL_GetWindowID(window))
        goto exit;
      if (event.type == sdl_force_repaint_event)
        need_repaint = true;
      if (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN ||
          event.type == SDL_MOUSEBUTTONUP)
        need_repaint = true;
    }

    if (pthread_mutex_lock(&update_lock))
      errx(1, "pthread_mutex_lock");
    struct task_state* state = task.cur_state;
    state->reader_active = true;
    if (pthread_mutex_unlock(&update_lock))
      errx(1, "pthread_mutex_unlock");

    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();

    // START actual rendering

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("PA heap state", NULL,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoSavedSettings);

    bool freeze = !task.enable_collection;
    ImGui::Checkbox("freeze", &freeze);
    task.enable_collection = !freeze;
    ImGui::SameLine(0.0f, 20.0f);
    bool probe_payloads = task.probe_payloads;
    ImGui::Checkbox("swap-disturbing heap probes [?]", &probe_payloads);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "Allow probing all PA heap memory, not just metadata.\n"
          "This can cause zeropage PTEs to be created\n"
          "and disturbs swapping.\n"
          "Therefore, slightly less information can be shown\n"
          "about OS page state, and performance of the target\n"
          "may be impacted further.\n"
          "This is destructive; once it has been enabled once,\n"
          "the process's memory will permanently look weird.");
    }
    task.probe_payloads = probe_payloads;
    ImGui::SameLine(0.0f, 20.0f);
    ImGui::Checkbox("show soft-dirty", &show_soft_dirty);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "Show which pages have been modified since the\n"
          "soft-dirty bits were reset.");
    }
    if (show_soft_dirty) {
      ImGui::SameLine();
      if (ImGui::Button("reset soft-dirty")) {
        int soft_dirty_fd = openat(task.task_fd, "clear_refs", O_WRONLY);
        if (soft_dirty_fd >= 0) {
          if (write(soft_dirty_fd, "4", 1) != 1)
            perror("write clear_refs");
          close(soft_dirty_fd);
        } else {
          perror("open clear_refs");
        }
      }
    }
    ImGui::SameLine(0.0f, 20.0f);
    if (probe_payloads) {
      ImGui::Text("<forced pageout unavailable [?]>");
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "forced pageout is unavailable because\n"
            "swap-disturbing heap probes are enabled.");
      }
    } else {
      if (ImGui::Button("force pageout [?]")) {
        for (size_t super_idx = 0; super_idx < state->superpage_count;
             super_idx++) {
          struct superpage* sp = &state->superpages[super_idx];
          struct iovec iov = {.iov_base = (void*)sp->addr,
                              .iov_len = SUPERPAGE_SIZE};
          int madv_ret = syscall(__NR_process_madvise, task.pidfd, &iov, 1,
                                 MADV_PAGEOUT, 0);
          printf("process_madvise says %d (%m)\n", madv_ret);
        }
      }
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("requires root privileges!\nrequires recent kernel");
    }
#if BROKEN
    ImGui::SameLine(0.0f, 20.0f);
    ImGui::Checkbox("log scale", &enable_logscale);
#endif

    if (ImGui::BeginTabBar("maintabbar", ImGuiTabBarFlags_None)) {
      if (ImGui::BeginTabItem("overview")) {
        render_overview(&task, state);
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("superpages")) {
        render_superpages(&task, state);
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("buckets")) {
        render_buckets(&task, state);
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("threads")) {
        render_threads(&task, state);
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }

    ImGui::End();

    // END actual rendering

    if (pthread_mutex_lock(&update_lock))
      errx(1, "pthread_mutex_lock");
    if (task.cur_state == state) {
      state->reader_active = false;
      state = NULL;
    }
    if (pthread_mutex_unlock(&update_lock))
      errx(1, "pthread_mutex_unlock");
    if (state) {
      // state has been updated in the meantime, we have to clean up
      delete[] state->superpages;
      delete[] state->vmas;
      free(state->maps_buf);
      delete state;
    }

    // ImPlot::ShowDemoWindow();

    ImGui::Render();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w,
                 clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);
  }

exit:
  ImGui_ImplOpenGL2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImPlot::DestroyContext();
  ImGui::DestroyContext();
  SDL_GL_DeleteContext(gl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  exit(0);  // don't return!
}
