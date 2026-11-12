/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <deque>
#include <map>

namespace unwindstack {

// Forward declarations.
class Memory;
class RegsArm;

enum ArmStatus : size_t {
  ARM_STATUS_NONE = 0,
  ARM_STATUS_NO_UNWIND,
  ARM_STATUS_FINISH,
  ARM_STATUS_RESERVED,
  ARM_STATUS_SPARE,
  ARM_STATUS_TRUNCATED,
  ARM_STATUS_READ_FAILED,
  ARM_STATUS_MALFORMED,
  ARM_STATUS_INVALID_ALIGNMENT,
  ARM_STATUS_INVALID_PERSONALITY,
};

enum ArmOp : uint8_t {
  ARM_OP_FINISH = 0xb0,
};

enum ArmLogType : uint8_t {
  ARM_LOG_NONE,
  ARM_LOG_FULL,
  ARM_LOG_BY_REG,
};

class ArmExidx {
 public:
  ArmExidx(RegsArm* regs, Memory* elf_memory, Memory* process_memory)
      : regs_(regs), elf_memory_(elf_memory), process_memory_(process_memory) {}
  virtual ~ArmExidx() {}

  void LogRawData();

  void LogByReg();

  bool ExtractEntryData(uint32_t entry_offset);

  bool Eval();

  bool Decode();

  std::deque<uint8_t>* data() { return &data_; }

  ArmStatus status() { return status_; }
  uint64_t status_address() { return status_address_; }

  RegsArm* regs() { return regs_; }

  uint32_t cfa() { return cfa_; }
  void set_cfa(uint32_t cfa) { cfa_ = cfa; }

  bool pc_set() { return pc_set_; }
  void set_pc_set(bool pc_set) { pc_set_ = pc_set; }

  void set_log(ArmLogType log_type) { log_type_ = log_type; }
  void set_log_skip_execution(bool skip_execution) { log_skip_execution_ = skip_execution; }
  void set_log_indent(uint8_t indent) { log_indent_ = indent; }

 private:
  bool GetByte(uint8_t* byte);
  void AdjustRegisters(int32_t offset);

  bool DecodePrefix_10_00(uint8_t byte);
  bool DecodePrefix_10_01(uint8_t byte);
  bool DecodePrefix_10_10(uint8_t byte);
  bool DecodePrefix_10_11_0000();
  bool DecodePrefix_10_11_0001();
  bool DecodePrefix_10_11_0010();
  bool DecodePrefix_10_11_0011();
  bool DecodePrefix_10_11_01nn();
  bool DecodePrefix_10_11_1nnn(uint8_t byte);
  bool DecodePrefix_10(uint8_t byte);

  bool DecodePrefix_11_000(uint8_t byte);
  bool DecodePrefix_11_001(uint8_t byte);
  bool DecodePrefix_11_010(uint8_t byte);
  bool DecodePrefix_11(uint8_t byte);

  RegsArm* regs_ = nullptr;
  uint32_t cfa_ = 0;
  std::deque<uint8_t> data_;
  ArmStatus status_ = ARM_STATUS_NONE;
  uint64_t status_address_ = 0;

  Memory* elf_memory_;
  Memory* process_memory_;

  ArmLogType log_type_ = ARM_LOG_NONE;
  uint8_t log_indent_ = 0;
  bool log_skip_execution_ = false;
  bool pc_set_ = false;
  int32_t log_cfa_offset_ = 0;
  std::map<uint8_t, int32_t> log_regs_;
};

}  // namespace unwindstack
