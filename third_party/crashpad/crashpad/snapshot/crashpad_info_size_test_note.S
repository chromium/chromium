// Copyright 2018 The Crashpad Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This note section is used on ELF platforms to give ElfImageReader a method
// of finding the instance of CrashpadInfo g_crashpad_info without requiring
// that symbol to be in the dynamic symbol table.

#include "util/misc/elf_note_types.h"
#include "util/misc/arm64_pac_bti.S"

// namespace crashpad {
// CrashpadInfo g_test_crashpad_info;
// }  // namespace crashpad
#define TEST_CRASHPAD_INFO_SYMBOL _ZN8crashpad20g_test_crashpad_infoE

#define NOTE_ALIGN 4

  // This section must be "a"llocated so that it appears in the final binary at
  // runtime. The reference to TEST_CRASHPAD_INFO_SYMBOL uses an offset relative
  // to this note to avoid making this note writable, which triggers a bug in
  // GNU ld, or adding text relocations which require the target system to allow
  // making text segments writable. https://crbug.com/crashpad/260.
  .section .note.crashpad.info,"a",%note
  .balign NOTE_ALIGN
  .type info_size_test_note, %object
info_size_test_note:
  .long name_end - name  // namesz
  .long desc_end - desc  // descsz
  .long CRASHPAD_ELF_NOTE_TYPE_CRASHPAD_INFO  // type
name:
  .asciz CRASHPAD_ELF_NOTE_NAME
name_end:
  .balign NOTE_ALIGN
desc:
#if defined(__LP64__)
  .quad TEST_CRASHPAD_INFO_SYMBOL - desc
#else
  .long TEST_CRASHPAD_INFO_SYMBOL - desc
#endif  // __LP64__
desc_end:
  .size info_size_test_note, .-info_size_test_note
