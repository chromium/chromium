// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stack_map_parser.h"

namespace stackmap {

FrameRoots StackmapV3Parser::ParseFrame() {
  std::vector<DWARF> reg_roots;
  std::vector<RBPOffset> stack_roots;

  auto* loc =
      ptr_offset<const StkMapLocation*>(cur_frame_, sizeof(StkMapRecordHeader));

  // The first few locations are reserved constants and not of interest to us.
  // We skip over them but assert that they are indeed constants (else
  // something has gone very wrong!).
  for (int i = 0; i < kSkipLocs; i++) {
    assert(loc->kind == kConstant);
    loc++;
  }

  // Deopt locations are not of interest to us either, but the first one
  // describes how many will follow, so we need it to jump over the rest in
  // order to get to the recorded gc root locations.
  int num_deopts = loc->offset;
  loc += num_deopts + 1;

  int gc_locs = (cur_frame_->num_locations - (num_deopts + 1) - kSkipLocs);

  // Locations come in pairs of a base pointer followed by a derived pointer.
  // At the moment we assume derived pointers are the same as base pointers so
  // we skip over them.
  for (uint16_t i = 0; i < gc_locs; i += 2) {
    switch (loc->kind) {
      case kRegister:
        reg_roots.push_back(loc->reg_num);
        break;
      case kIndirect:
        stack_roots.push_back(loc->offset);
        break;
      default:
        // Ignore
        break;
    }
    loc += 2;
  }

  // The liveouts part of the stack map record is not of interest to us.
  // However, it is dynamically sized, so we need to work out many records
  // exist so that we can effectively jump over them.
  int incr = sizeof(StkMapHeader) +
             (cur_frame_->num_locations * sizeof(StkMapLocation));
  auto* liveouts = align_8(ptr_offset<const LiveOutsHeader*>(cur_frame_, incr));
  incr = sizeof(LiveOutsHeader) + (liveouts->num_liveouts * sizeof(LiveOut));

  // LLVM V3 stackmap format requires padding here if we need to align to an 8
  // byte boundary.
  cur_frame_ = align_8(ptr_offset<const StkMapRecordHeader*>(liveouts, incr));
  return FrameRoots(reg_roots, stack_roots);
}

SafepointTable StackmapV3Parser::Parse() {
  auto* header = reinterpret_cast<const StkMapHeader*>(cursor_);

  assert(header->version == kStackmapVersion &&
         "Stackmap Parser is incorrect version");

  // Work out the the offset needed to get to the first stack map frame record
  // entry (i.e. call site). This needs to jump over the dynamically sized
  // function and constant table.
  uint32_t size_consts = header->num_constants * kSizeConstantEntry;
  uint32_t size_fns = header->num_functions * sizeof(StkSizeRecord);
  uint32_t rec_offset = sizeof(StkMapHeader) + size_consts + size_fns;
  cur_frame_ = ptr_offset<const StkMapRecordHeader*>(cursor_, rec_offset);

  // For each function in the stack map, we iterate over the stack map record
  // list looking for its respective callsite, adding its entry to the table.
  auto* fn = ptr_offset<const StkSizeRecord*>(cursor_, sizeof(StkMapHeader));
  std::map<ReturnAddress, FrameRoots> roots;
  for (uint32_t i = 0; i < header->num_functions; i++) {
    for (uint32_t j = 0; j < fn->record_count; j++) {
      ReturnAddress key = fn->address + cur_frame_->return_addr;
      auto frame_roots = ParseFrame();
      if (!frame_roots.empty())
        roots.insert({key, frame_roots});
    }
    fn++;
  }

  return SafepointTable(roots);
}
}  // namespace stackmap

SafepointTable GenSafepointTable() {
  auto parser = stackmap::StackmapV3Parser();
  return parser.Parse();
}
