// Copyright 2020 The Crashpad Authors. All rights reserved.
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

#include "snapshot/linux/test_modules.h"

#include <elf.h>

#include <limits>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/test_paths.h"
#include "util/file/filesystem.h"
#include "util/file/file_writer.h"

namespace crashpad {
namespace test {

bool WriteTestModule(const base::FilePath& module_path,
                     const std::string& soname) {
#if defined(ARCH_CPU_64_BITS)
  using Ehdr = Elf64_Ehdr;
  using Phdr = Elf64_Phdr;
  using Shdr = Elf64_Shdr;
  using Dyn = Elf64_Dyn;
  using Sym = Elf64_Sym;
  unsigned char elf_class = ELFCLASS64;
#else
  using Ehdr = Elf32_Ehdr;
  using Phdr = Elf32_Phdr;
  using Shdr = Elf32_Shdr;
  using Dyn = Elf32_Dyn;
  using Sym = Elf32_Sym;
  unsigned char elf_class = ELFCLASS32;
#endif

  struct {
    Ehdr ehdr;
    struct {
      Phdr load1;
      Phdr load2;
      Phdr dynamic;
    } phdr_table;
    struct {
      Dyn hash;
      Dyn strtab;
      Dyn symtab;
      Dyn strsz;
      Dyn syment;
      Dyn soname;
      Dyn null;
    } dynamic_array;
    struct {
      Elf32_Word nbucket;
      Elf32_Word nchain;
      Elf32_Word bucket;
      Elf32_Word chain;
    } hash_table;
    char string_table[32];
    struct {
    } section_header_string_table;
    struct {
      Sym und_symbol;
    } symbol_table;
    struct {
      Shdr null;
      Shdr dynamic;
      Shdr string_table;
      Shdr section_header_string_table;
    } shdr_table;
  } module = {};

  module.ehdr.e_ident[EI_MAG0] = ELFMAG0;
  module.ehdr.e_ident[EI_MAG1] = ELFMAG1;
  module.ehdr.e_ident[EI_MAG2] = ELFMAG2;
  module.ehdr.e_ident[EI_MAG3] = ELFMAG3;

  module.ehdr.e_ident[EI_CLASS] = elf_class;

#if defined(ARCH_CPU_LITTLE_ENDIAN)
  module.ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
#else
  module.ehdr.e_ident[EI_DATA] = ELFDATA2MSB;
#endif  // ARCH_CPU_LITTLE_ENDIAN

  module.ehdr.e_ident[EI_VERSION] = EV_CURRENT;

  module.ehdr.e_type = ET_DYN;

#if defined(ARCH_CPU_X86)
  module.ehdr.e_machine = EM_386;
#elif defined(ARCH_CPU_X86_64)
  module.ehdr.e_machine = EM_X86_64;
#elif defined(ARCH_CPU_ARMEL)
  module.ehdr.e_machine = EM_ARM;
#elif defined(ARCH_CPU_ARM64)
  module.ehdr.e_machine = EM_AARCH64;
#elif defined(ARCH_CPU_MIPSEL) || defined(ARCH_CPU_MIPS64EL)
  module.ehdr.e_machine = EM_MIPS;
#endif

  module.ehdr.e_version = EV_CURRENT;
  module.ehdr.e_ehsize = sizeof(module.ehdr);

  module.ehdr.e_phoff = offsetof(decltype(module), phdr_table);
  module.ehdr.e_phnum = sizeof(module.phdr_table) / sizeof(Phdr);
  module.ehdr.e_phentsize = sizeof(Phdr);

  module.ehdr.e_shoff = offsetof(decltype(module), shdr_table);
  module.ehdr.e_shentsize = sizeof(Shdr);
  module.ehdr.e_shnum = sizeof(module.shdr_table) / sizeof(Shdr);
  module.ehdr.e_shstrndx =
      offsetof(decltype(module.shdr_table), section_header_string_table) /
      sizeof(Shdr);

  const size_t page_size = getpagesize();
  auto align = [page_size](uintptr_t addr) {
    return (addr + page_size - 1) & ~(page_size - 1);
  };
  constexpr size_t segment_size = offsetof(decltype(module), shdr_table);

  // This test module covers cases where:
  // 1. Multiple segments are mapped from file offset 0.
  // 2. Load bias is negative.

  const uintptr_t load2_vaddr = align(std::numeric_limits<uintptr_t>::max() -
                                      align(segment_size) - page_size);
  const uintptr_t load1_vaddr = load2_vaddr - align(segment_size);

  module.phdr_table.load1.p_type = PT_LOAD;
  module.phdr_table.load1.p_offset = 0;
  module.phdr_table.load1.p_vaddr = load1_vaddr;
  module.phdr_table.load1.p_filesz = segment_size;
  module.phdr_table.load1.p_memsz = segment_size;
  module.phdr_table.load1.p_flags = PF_R;
  module.phdr_table.load1.p_align = page_size;

  module.phdr_table.load2.p_type = PT_LOAD;
  module.phdr_table.load2.p_offset = 0;
  module.phdr_table.load2.p_vaddr = load2_vaddr;
  module.phdr_table.load2.p_filesz = segment_size;
  module.phdr_table.load2.p_memsz = segment_size;
  module.phdr_table.load2.p_flags = PF_R | PF_W;
  module.phdr_table.load2.p_align = page_size;

  module.phdr_table.dynamic.p_type = PT_DYNAMIC;
  module.phdr_table.dynamic.p_offset =
      offsetof(decltype(module), dynamic_array);
  module.phdr_table.dynamic.p_vaddr =
      load2_vaddr + module.phdr_table.dynamic.p_offset;
  module.phdr_table.dynamic.p_filesz = sizeof(module.dynamic_array);
  module.phdr_table.dynamic.p_memsz = sizeof(module.dynamic_array);
  module.phdr_table.dynamic.p_flags = PF_R | PF_W;
  module.phdr_table.dynamic.p_align = 8;

  module.dynamic_array.hash.d_tag = DT_HASH;
  module.dynamic_array.hash.d_un.d_ptr =
      load1_vaddr + offsetof(decltype(module), hash_table);
  module.dynamic_array.strtab.d_tag = DT_STRTAB;
  module.dynamic_array.strtab.d_un.d_ptr =
      load1_vaddr + offsetof(decltype(module), string_table);
  module.dynamic_array.symtab.d_tag = DT_SYMTAB;
  module.dynamic_array.symtab.d_un.d_ptr =
      load1_vaddr + offsetof(decltype(module), symbol_table);
  module.dynamic_array.strsz.d_tag = DT_STRSZ;
  module.dynamic_array.strsz.d_un.d_val = sizeof(module.string_table);
  module.dynamic_array.syment.d_tag = DT_SYMENT;
  module.dynamic_array.syment.d_un.d_val = sizeof(Sym);
  constexpr size_t kSonameOffset = 1;
  module.dynamic_array.soname.d_tag = DT_SONAME;
  module.dynamic_array.soname.d_un.d_val = kSonameOffset;

  module.dynamic_array.null.d_tag = DT_NULL;

  module.hash_table.nbucket = 1;
  module.hash_table.nchain = 1;
  module.hash_table.bucket = 0;
  module.hash_table.chain = 0;

  if (sizeof(module.string_table) < soname.size() + 2) {
    ADD_FAILURE() << "string table too small";
    return false;
  }
  module.string_table[0] = '\0';
  memcpy(&module.string_table[kSonameOffset], soname.c_str(), soname.size());

  module.shdr_table.null.sh_type = SHT_NULL;

  module.shdr_table.dynamic.sh_name = 0;
  module.shdr_table.dynamic.sh_type = SHT_DYNAMIC;
  module.shdr_table.dynamic.sh_flags = SHF_WRITE | SHF_ALLOC;
  module.shdr_table.dynamic.sh_addr = module.phdr_table.dynamic.p_vaddr;
  module.shdr_table.dynamic.sh_offset = module.phdr_table.dynamic.p_offset;
  module.shdr_table.dynamic.sh_size = module.phdr_table.dynamic.p_filesz;
  module.shdr_table.dynamic.sh_link =
      offsetof(decltype(module.shdr_table), string_table) / sizeof(Shdr);

  module.shdr_table.string_table.sh_name = 0;
  module.shdr_table.string_table.sh_type = SHT_STRTAB;
  module.shdr_table.string_table.sh_offset =
      offsetof(decltype(module), string_table);
  module.shdr_table.string_table.sh_size = sizeof(module.string_table);

  module.shdr_table.section_header_string_table.sh_name = 0;
  module.shdr_table.section_header_string_table.sh_type = SHT_STRTAB;
  module.shdr_table.section_header_string_table.sh_offset =
      offsetof(decltype(module), section_header_string_table);
  module.shdr_table.section_header_string_table.sh_size =
      sizeof(module.section_header_string_table);

  FileWriter writer;
  if (!writer.Open(module_path,
                   FileWriteMode::kCreateOrFail,
                   FilePermissions::kWorldReadable)) {
    ADD_FAILURE();
    return false;
  }

  if (!writer.Write(&module, sizeof(module))) {
    ADD_FAILURE();
    LoggingRemoveFile(module_path);
    return false;
  }

  return true;
}

ScopedModuleHandle LoadTestModule(const std::string& module_name,
                                  const std::string& module_soname) {
  base::FilePath module_path(
      TestPaths::Executable().DirName().Append(module_name));

  if (!WriteTestModule(module_path, module_soname)) {
    return ScopedModuleHandle(nullptr);
  }
  EXPECT_TRUE(IsRegularFile(module_path));

  ScopedModuleHandle handle(
      dlopen(module_path.value().c_str(), RTLD_LAZY | RTLD_LOCAL));
  EXPECT_TRUE(handle.valid())
      << "dlopen: " << module_path.value() << " " << dlerror();

  EXPECT_TRUE(LoggingRemoveFile(module_path));

  return handle;
}

}  // namespace test
}  // namespace crashpad
