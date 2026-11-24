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

#pragma once

#include <stdint.h>
#include <sys/types.h>

#include <memory>
#include <string>
#include <vector>

#include <unwindstack/Arch.h>
#include <unwindstack/DexFiles.h>
#include <unwindstack/Error.h>
#include <unwindstack/JitDebug.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>
#include <unwindstack/Regs.h>
#include <unwindstack/SharedString.h>

namespace unwindstack {

// Forward declarations.
class Elf;
class ThreadEntry;

struct FrameData {
  size_t num;

  uint64_t rel_pc;
  uint64_t pc;
  uint64_t sp;

  SharedString function_name;
  uint64_t function_offset = 0;

  std::shared_ptr<MapInfo> map_info;
};

class Unwinder {
 public:
  Unwinder(size_t max_frames, Maps* maps, Regs* regs, std::shared_ptr<Memory> process_memory)
      : max_frames_(max_frames),
        maps_(maps),
        regs_(regs),
        process_memory_(process_memory),
        arch_(regs->Arch()) {}
  Unwinder(size_t max_frames, Maps* maps, std::shared_ptr<Memory> process_memory)
      : max_frames_(max_frames), maps_(maps), process_memory_(process_memory) {}

  virtual ~Unwinder() = default;

  virtual void Unwind(const std::vector<std::string>* initial_map_names_to_skip = nullptr,
                      const std::vector<std::string>* map_suffixes_to_ignore = nullptr);

  size_t NumFrames() const { return frames_.size(); }

  // Returns frames after unwinding.
  // Intentionally mutable (which can be used to swap in reserved memory before unwinding).
  std::vector<FrameData>& frames() { return frames_; }

  std::vector<FrameData> ConsumeFrames() {
    std::vector<FrameData> frames = std::move(frames_);
    frames_.clear();
    return frames;
  }

  std::string FormatFrame(size_t frame_num) const;
  std::string FormatFrame(const FrameData& frame) const;

  static std::string FormatFrame(ArchEnum arch, const FrameData& frame,
                                 bool display_build_id = true);

  void SetArch(ArchEnum arch) { arch_ = arch; }

  void SetJitDebug(JitDebug* jit_debug);

  void SetRegs(Regs* regs) {
    regs_ = regs;
    arch_ = regs_ != nullptr ? regs->Arch() : ARCH_UNKNOWN;
  }
  Maps* GetMaps() { return maps_; }
  std::shared_ptr<Memory>& GetProcessMemory() { return process_memory_; }

  // Disabling the resolving of names results in the function name being
  // set to an empty string and the function offset being set to zero.
  void SetResolveNames(bool resolve) { resolve_names_ = resolve; }

  void SetDisplayBuildID(bool display_build_id) { display_build_id_ = display_build_id; }

  void SetDexFiles(DexFiles* dex_files);

  const ErrorData& LastError() { return last_error_; }
  ErrorCode LastErrorCode() { return last_error_.code; }
  const char* LastErrorCodeString() { return GetErrorCodeString(last_error_.code); }
  uint64_t LastErrorAddress() { return last_error_.address; }
  uint64_t warnings() { return warnings_; }

  // Builds a frame for symbolization using the maps from this unwinder. The
  // constructed frame contains just enough information to be used to symbolize
  // frames collected by frame-pointer unwinding that's done outside of
  // libunwindstack. This is used by tombstoned to symbolize frame pointer-based
  // stack traces that are collected by tools such as GWP-ASan and MTE.
  static FrameData BuildFrameFromPcOnly(uint64_t pc, ArchEnum arch, Maps* maps, JitDebug* jit_debug,
                                        std::shared_ptr<Memory> process_memory, bool resolve_names);
  FrameData BuildFrameFromPcOnly(uint64_t pc);

 protected:
  Unwinder(size_t max_frames, Maps* maps = nullptr) : max_frames_(max_frames), maps_(maps) {}
  Unwinder(size_t max_frames, ArchEnum arch, Maps* maps = nullptr)
      : max_frames_(max_frames), maps_(maps), arch_(arch) {}
  Unwinder(size_t max_frames, ArchEnum arch, Maps* maps, std::shared_ptr<Memory>& process_memory)
      : max_frames_(max_frames), maps_(maps), process_memory_(process_memory), arch_(arch) {}

  void ClearErrors() {
    warnings_ = WARNING_NONE;
    last_error_.code = ERROR_NONE;
    last_error_.address = 0;
  }

  void FillInDexFrame();
  FrameData* FillInFrame(std::shared_ptr<MapInfo>& map_info, Elf* elf, uint64_t rel_pc,
                         uint64_t pc_adjustment);

  size_t max_frames_;
  Maps* maps_ = nullptr;
  Regs* regs_;
  std::vector<FrameData> frames_;
  std::shared_ptr<Memory> process_memory_;
  JitDebug* jit_debug_ = nullptr;
  DexFiles* dex_files_ = nullptr;
  bool resolve_names_ = true;
  bool display_build_id_ = false;
  ErrorData last_error_;
  uint64_t warnings_;
  ArchEnum arch_ = ARCH_UNKNOWN;
};

class UnwinderFromPid : public Unwinder {
 public:
  UnwinderFromPid(size_t max_frames, pid_t pid, Maps* maps = nullptr)
      : Unwinder(max_frames, maps), pid_(pid) {}
  UnwinderFromPid(size_t max_frames, pid_t pid, std::shared_ptr<Memory>& process_memory)
      : Unwinder(max_frames, nullptr, process_memory), pid_(pid) {}
  UnwinderFromPid(size_t max_frames, pid_t pid, ArchEnum arch, Maps* maps = nullptr)
      : Unwinder(max_frames, arch, maps), pid_(pid) {}
  UnwinderFromPid(size_t max_frames, pid_t pid, ArchEnum arch, Maps* maps,
                  std::shared_ptr<Memory>& process_memory)
      : Unwinder(max_frames, arch, maps, process_memory), pid_(pid) {}
  virtual ~UnwinderFromPid() = default;

  bool Init();

  void Unwind(const std::vector<std::string>* initial_map_names_to_skip = nullptr,
              const std::vector<std::string>* map_suffixes_to_ignore = nullptr) override;

 protected:
  pid_t pid_;
  std::unique_ptr<Maps> maps_ptr_;
  std::unique_ptr<JitDebug> jit_debug_ptr_;
  std::unique_ptr<DexFiles> dex_files_ptr_;
  bool initted_ = false;
};

class ThreadUnwinder : public UnwinderFromPid {
 public:
  ThreadUnwinder(size_t max_frames, Maps* maps = nullptr);
  ThreadUnwinder(size_t max_frames, Maps* maps, std::shared_ptr<Memory>& process_memory);
  ThreadUnwinder(size_t max_frames, const ThreadUnwinder* unwinder);
  virtual ~ThreadUnwinder() = default;

  void SetObjects(ThreadUnwinder* unwinder);

  void Unwind(const std::vector<std::string>*, const std::vector<std::string>*) override {}

  void UnwindWithSignal(int signal, pid_t tid, std::unique_ptr<Regs>* initial_regs = nullptr,
                        const std::vector<std::string>* initial_map_names_to_skip = nullptr,
                        const std::vector<std::string>* map_suffixes_to_ignore = nullptr);

 protected:
  ThreadEntry* SendSignalToThread(int signal, pid_t tid);
};

}  // namespace unwindstack
