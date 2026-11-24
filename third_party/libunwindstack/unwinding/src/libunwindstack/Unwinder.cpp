/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _GNU_SOURCE 1
#include <elf.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <memory>
#include <string>

#include <android-base/file.h>
#include <android-base/stringprintf.h>

#include <unwindstack/Demangle.h>
#include <unwindstack/DexFiles.h>
#include <unwindstack/Elf.h>
#include <unwindstack/JitDebug.h>
#include <unwindstack/MapInfo.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>
#include <unwindstack/Unwinder.h>

#include "Check.h"

namespace unwindstack {

// Inject extra 'virtual' frame that represents the dex pc data.
// The dex pc is a magic register defined in the Mterp interpreter,
// and thus it will be restored/observed in the frame after it.
// Adding the dex frame first here will create something like:
//   #7 pc 0015fa20 core.vdex   java.util.Arrays.binarySearch+8
//   #8 pc 006b1ba1 libartd.so  ExecuteMterpImpl+14625
//   #9 pc 0039a1ef libartd.so  art::interpreter::Execute+719
void Unwinder::FillInDexFrame() {
  size_t frame_num = frames_.size();
  frames_.resize(frame_num + 1);
  FrameData* frame = &frames_.at(frame_num);
  frame->num = frame_num;

  uint64_t dex_pc = regs_->dex_pc();
  frame->pc = dex_pc;
  frame->sp = regs_->sp();

  frame->map_info = maps_->Find(dex_pc);
  if (frame->map_info == nullptr) {
    frame->rel_pc = dex_pc;
    warnings_ |= WARNING_DEX_PC_NOT_IN_MAP;
    return;
  }

  auto& map_info = frame->map_info;
  frame->rel_pc = dex_pc - map_info->start();
  if (!map_info->LoadBiasInitialized()) {
    // Only do this once per MapInfo object used for a dex pc frame. If
    // multiple threads happen to do this at the same time, this action
    // is idempotent and will set the same values.
    map_info->set_elf_start_offset(map_info->offset());
    map_info->set_load_bias(0);
  }

  if (!resolve_names_) {
    return;
  }

#if defined(DEXFILE_SUPPORT)
  if (dex_files_ == nullptr) {
    return;
  }

  dex_files_->GetFunctionName(maps_, dex_pc, &frame->function_name, &frame->function_offset);
#endif
}

FrameData* Unwinder::FillInFrame(std::shared_ptr<MapInfo>& map_info, Elf* /*elf*/, uint64_t rel_pc,
                                 uint64_t pc_adjustment) {
  size_t frame_num = frames_.size();
  frames_.resize(frame_num + 1);
  FrameData* frame = &frames_.at(frame_num);
  frame->num = frame_num;
  frame->sp = regs_->sp();
  frame->rel_pc = rel_pc - pc_adjustment;
  frame->pc = regs_->pc() - pc_adjustment;

  if (map_info == nullptr) {
    // Nothing else to update.
    return nullptr;
  }

  frame->map_info = map_info;

  return frame;
}

static bool ShouldStop(const std::vector<std::string>* map_suffixes_to_ignore,
                       const std::string& map_name) {
  if (map_suffixes_to_ignore == nullptr) {
    return false;
  }
  auto pos = map_name.find_last_of('.');
  if (pos == std::string::npos) {
    return false;
  }

  return std::find(map_suffixes_to_ignore->begin(), map_suffixes_to_ignore->end(),
                   map_name.substr(pos + 1)) != map_suffixes_to_ignore->end();
}

void Unwinder::Unwind(const std::vector<std::string>* initial_map_names_to_skip,
                      const std::vector<std::string>* map_suffixes_to_ignore) {
  CHECK(arch_ != ARCH_UNKNOWN);
  ClearErrors();

  frames_.clear();

  // Clear any cached data from previous unwinds.
  process_memory_->Clear();

  if (maps_->Find(regs_->pc()) == nullptr) {
    regs_->fallback_pc();
  }

  bool return_address_attempt = false;
  bool adjust_pc = false;
  for (; frames_.size() < max_frames_;) {
    uint64_t cur_pc = regs_->pc();
    uint64_t cur_sp = regs_->sp();

    std::shared_ptr<MapInfo> map_info = maps_->Find(regs_->pc());
    uint64_t pc_adjustment = 0;
    uint64_t step_pc;
    uint64_t rel_pc;
    Elf* elf;
    bool ignore_frame = false;
    if (map_info == nullptr) {
      step_pc = regs_->pc();
      rel_pc = step_pc;
      // If we get invalid map via return_address_attempt, don't hide error for the previous frame.
      if (!return_address_attempt || last_error_.code == ERROR_NONE) {
        last_error_.code = ERROR_INVALID_MAP;
        last_error_.address = step_pc;
      }
      elf = nullptr;
    } else {
      ignore_frame =
          initial_map_names_to_skip != nullptr &&
          std::find(initial_map_names_to_skip->begin(), initial_map_names_to_skip->end(),
                    android::base::Basename(map_info->name())) != initial_map_names_to_skip->end();
      if (!ignore_frame && ShouldStop(map_suffixes_to_ignore, map_info->name())) {
        break;
      }
      elf = map_info->GetElf(process_memory_, arch_);
      step_pc = regs_->pc();
      rel_pc = elf->GetRelPc(step_pc, map_info.get());
      // Everyone except elf data in gdb jit debug maps uses the relative pc.
      if (!(map_info->flags() & MAPS_FLAGS_JIT_SYMFILE_MAP)) {
        step_pc = rel_pc;
      }
      if (adjust_pc) {
        pc_adjustment = GetPcAdjustment(rel_pc, elf, arch_);
      } else {
        pc_adjustment = 0;
      }
      step_pc -= pc_adjustment;

      // If the pc is in an invalid elf file, try and get an Elf object
      // using the jit debug information.
      if (!elf->valid() && jit_debug_ != nullptr && (map_info->flags() & PROT_EXEC)) {
        uint64_t adjusted_jit_pc = regs_->pc() - pc_adjustment;
        Elf* jit_elf = jit_debug_->Find(maps_, adjusted_jit_pc);
        if (jit_elf != nullptr) {
          // The jit debug information requires a non relative adjusted pc.
          step_pc = adjusted_jit_pc;
          elf = jit_elf;
        }
      }
    }

    FrameData* frame = nullptr;
    if (!ignore_frame) {
      if (regs_->dex_pc() != 0) {
        // Add a frame to represent the dex file.
        FillInDexFrame();
        // Clear the dex pc so that we don't repeat this frame later.
        regs_->set_dex_pc(0);

        // Make sure there is enough room for the real frame.
        if (frames_.size() == max_frames_) {
          last_error_.code = ERROR_MAX_FRAMES_EXCEEDED;
          break;
        }
      }

      frame = FillInFrame(map_info, elf, rel_pc, pc_adjustment);

      // Once a frame is added, stop skipping frames.
      initial_map_names_to_skip = nullptr;
    }
    adjust_pc = true;

    bool stepped = false;
    bool in_device_map = false;
    bool finished = false;
    if (map_info != nullptr) {
      if (map_info->flags() & MAPS_FLAGS_DEVICE_MAP) {
        // Do not stop here, fall through in case we are
        // in the speculative unwind path and need to remove
        // some of the speculative frames.
        in_device_map = true;
      } else {
        auto sp_info = maps_->Find(regs_->sp());
        if (sp_info != nullptr && sp_info->flags() & MAPS_FLAGS_DEVICE_MAP) {
          // Do not stop here, fall through in case we are
          // in the speculative unwind path and need to remove
          // some of the speculative frames.
          in_device_map = true;
        } else {
          bool is_signal_frame = false;
          if (elf->StepIfSignalHandler(rel_pc, regs_, process_memory_.get())) {
            stepped = true;
            is_signal_frame = true;
          } else if (elf->Step(step_pc, regs_, process_memory_.get(), &finished,
                               &is_signal_frame)) {
            stepped = true;
          }
          if (is_signal_frame && frame != nullptr) {
            // Need to adjust the relative pc because the signal handler
            // pc should not be adjusted.
            frame->rel_pc = rel_pc;
            frame->pc += pc_adjustment;
            step_pc = rel_pc;
          }
          elf->GetLastError(&last_error_);
        }
      }
    }

    if (frame != nullptr) {
      if (!resolve_names_ ||
          !elf->GetFunctionName(step_pc, &frame->function_name, &frame->function_offset)) {
        frame->function_name = "";
        frame->function_offset = 0;
      }
    }

    if (finished) {
      break;
    }

    if (!stepped) {
      if (return_address_attempt) {
        // Only remove the speculative frame if there are more than two frames
        // or the pc in the first frame is in a valid map.
        // This allows for a case where the code jumps into the middle of
        // nowhere, but there is no other unwind information after that.
        if (frames_.size() > 2 || (frames_.size() > 0 && maps_->Find(frames_[0].pc) != nullptr)) {
          // Remove the speculative frame.
          frames_.pop_back();
        }
        break;
      } else if (in_device_map) {
        // Do not attempt any other unwinding, pc or sp is in a device
        // map.
        break;
      } else {
        // Steping didn't work, try this secondary method.
        if (!regs_->SetPcFromReturnAddress(process_memory_.get())) {
          break;
        }
        return_address_attempt = true;
      }
    } else {
      return_address_attempt = false;
      if (max_frames_ == frames_.size()) {
        last_error_.code = ERROR_MAX_FRAMES_EXCEEDED;
      }
    }

    // If the pc and sp didn't change, then consider everything stopped.
    if (cur_pc == regs_->pc() && cur_sp == regs_->sp()) {
      last_error_.code = ERROR_REPEATED_FRAME;
      break;
    }
  }
}

std::string Unwinder::FormatFrame(const FrameData& frame) const {
  return FormatFrame(arch_, frame, display_build_id_);
}

std::string Unwinder::FormatFrame(ArchEnum arch, const FrameData& frame, bool display_build_id) {
  std::string data;
  if (ArchIs32Bit(arch)) {
    data += android::base::StringPrintf("  #%02zu pc %08" PRIx64, frame.num, frame.rel_pc);
  } else {
    data += android::base::StringPrintf("  #%02zu pc %016" PRIx64, frame.num, frame.rel_pc);
  }

  auto map_info = frame.map_info;
  if (map_info == nullptr) {
    // No valid map associated with this frame.
    data += "  <unknown>";
  } else if (!map_info->name().empty()) {
    data += "  ";
    data += map_info->GetFullName();
  } else {
    data += android::base::StringPrintf("  <anonymous:%" PRIx64 ">", map_info->start());
  }

  if (map_info != nullptr && map_info->elf_start_offset() != 0) {
    data += android::base::StringPrintf(" (offset 0x%" PRIx64 ")", map_info->elf_start_offset());
  }

  if (!frame.function_name.empty()) {
    data += " (" + DemangleNameIfNeeded(frame.function_name);
    if (frame.function_offset != 0) {
      data += android::base::StringPrintf("+%" PRId64, frame.function_offset);
    }
    data += ')';
  }

  if (map_info != nullptr && display_build_id) {
    std::string build_id = map_info->GetPrintableBuildID();
    if (!build_id.empty()) {
      data += " (BuildId: " + build_id + ')';
    }
  }
  return data;
}

std::string Unwinder::FormatFrame(size_t frame_num) const {
  if (frame_num >= frames_.size()) {
    return "";
  }
  return FormatFrame(arch_, frames_[frame_num], display_build_id_);
}

void Unwinder::SetJitDebug(JitDebug* jit_debug) {
  jit_debug_ = jit_debug;
}

void Unwinder::SetDexFiles(DexFiles* dex_files) {
  dex_files_ = dex_files;
}

bool UnwinderFromPid::Init() {
  CHECK(arch_ != ARCH_UNKNOWN);
  if (initted_) {
    return true;
  }
  initted_ = true;

  if (maps_ == nullptr) {
    if (pid_ == getpid()) {
      maps_ptr_.reset(new LocalMaps());
    } else {
      maps_ptr_.reset(new RemoteMaps(pid_));
    }
    if (!maps_ptr_->Parse()) {
      ClearErrors();
      last_error_.code = ERROR_INVALID_MAP;
      return false;
    }
    maps_ = maps_ptr_.get();
  }

  if (process_memory_ == nullptr) {
    if (pid_ == getpid()) {
      // Local unwind, so use thread cache to allow multiple threads
      // to cache data even when multiple threads access the same object.
      process_memory_ = Memory::CreateProcessMemoryThreadCached(pid_);
    } else {
      // Remote unwind should be safe to cache since the unwind will
      // be occurring on a stopped process.
      process_memory_ = Memory::CreateProcessMemoryCached(pid_);
    }
  }

  // jit_debug_ and dex_files_ may have already been set, for example in
  // AndroidLocalUnwinder::InternalUnwind.
  if (jit_debug_ == nullptr) {
    jit_debug_ptr_ = CreateJitDebug(arch_, process_memory_);
    SetJitDebug(jit_debug_ptr_.get());
  }
#if defined(DEXFILE_SUPPORT)
  if (dex_files_ == nullptr) {
    dex_files_ptr_ = CreateDexFiles(arch_, process_memory_);
    SetDexFiles(dex_files_ptr_.get());
  }
#endif

  return true;
}

void UnwinderFromPid::Unwind(const std::vector<std::string>* initial_map_names_to_skip,
                             const std::vector<std::string>* map_suffixes_to_ignore) {
  if (!Init()) {
    return;
  }
  Unwinder::Unwind(initial_map_names_to_skip, map_suffixes_to_ignore);
}

FrameData Unwinder::BuildFrameFromPcOnly(uint64_t pc, ArchEnum arch, Maps* maps,
                                         JitDebug* jit_debug,
                                         std::shared_ptr<Memory> process_memory,
                                         bool resolve_names) {
  FrameData frame;

  std::shared_ptr<MapInfo> map_info = maps->Find(pc);
  if (map_info == nullptr || arch == ARCH_UNKNOWN) {
    frame.pc = pc;
    frame.rel_pc = pc;
    return frame;
  }

  Elf* elf = map_info->GetElf(process_memory, arch);

  uint64_t relative_pc = elf->GetRelPc(pc, map_info.get());

  uint64_t pc_adjustment = GetPcAdjustment(relative_pc, elf, arch);
  relative_pc -= pc_adjustment;
  // The debug PC may be different if the PC comes from the JIT.
  uint64_t debug_pc = relative_pc;

  // If we don't have a valid ELF file, check the JIT.
  if (!elf->valid() && jit_debug != nullptr) {
    uint64_t jit_pc = pc - pc_adjustment;
    Elf* jit_elf = jit_debug->Find(maps, jit_pc);
    if (jit_elf != nullptr) {
      debug_pc = jit_pc;
      elf = jit_elf;
    }
  }

  // Copy all the things we need into the frame for symbolization.
  frame.rel_pc = relative_pc;
  frame.pc = pc - pc_adjustment;
  frame.map_info = map_info;

  if (!resolve_names ||
      !elf->GetFunctionName(debug_pc, &frame.function_name, &frame.function_offset)) {
    frame.function_name = "";
    frame.function_offset = 0;
  }
  return frame;
}

FrameData Unwinder::BuildFrameFromPcOnly(uint64_t pc) {
  return BuildFrameFromPcOnly(pc, arch_, maps_, jit_debug_, process_memory_, resolve_names_);
}

}  // namespace unwindstack
