// Copyright 2015 The Crashpad Authors. All rights reserved.
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

#include "snapshot/minidump/process_snapshot_minidump.h"

#include <windows.h>
#include <dbghelp.h>
#include <string.h>

#include <memory>

#include "gtest/gtest.h"
#include "minidump/minidump_context.h"
#include "snapshot/minidump/minidump_annotation_reader.h"
#include "snapshot/module_snapshot.h"
#include "util/file/string_file.h"

namespace crashpad {
namespace test {
namespace {

TEST(ProcessSnapshotMinidump, EmptyFile) {
  StringFile string_file;
  ProcessSnapshotMinidump process_snapshot;

  EXPECT_FALSE(process_snapshot.Initialize(&string_file));
}

TEST(ProcessSnapshotMinidump, InvalidSignatureAndVersion) {
  StringFile string_file;

  MINIDUMP_HEADER header = {};

  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  ProcessSnapshotMinidump process_snapshot;
  EXPECT_FALSE(process_snapshot.Initialize(&string_file));
}

TEST(ProcessSnapshotMinidump, Empty) {
  StringFile string_file;

  MINIDUMP_HEADER header = {};
  header.Signature = MINIDUMP_SIGNATURE;
  header.Version = MINIDUMP_VERSION;

  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  ProcessSnapshotMinidump process_snapshot;
  EXPECT_TRUE(process_snapshot.Initialize(&string_file));

  UUID client_id;
  process_snapshot.ClientID(&client_id);
  EXPECT_EQ(client_id, UUID());

  EXPECT_TRUE(process_snapshot.AnnotationsSimpleMap().empty());
}

// Writes |string| to |writer| as a MinidumpUTF8String, and returns the file
// offset of the beginning of the string.
RVA WriteString(FileWriterInterface* writer, const std::string& string) {
  RVA rva = static_cast<RVA>(writer->SeekGet());

  uint32_t string_size = static_cast<uint32_t>(string.size());
  EXPECT_TRUE(writer->Write(&string_size, sizeof(string_size)));

  // Include the trailing NUL character.
  EXPECT_TRUE(writer->Write(string.c_str(), string.size() + 1));

  return rva;
}

// Writes |dictionary| to |writer| as a MinidumpSimpleStringDictionary, and
// populates |location| with a location descriptor identifying what was written.
void WriteMinidumpSimpleStringDictionary(
    MINIDUMP_LOCATION_DESCRIPTOR* location,
    FileWriterInterface* writer,
    const std::map<std::string, std::string>& dictionary) {
  std::vector<MinidumpSimpleStringDictionaryEntry> entries;
  for (const auto& it : dictionary) {
    MinidumpSimpleStringDictionaryEntry entry;
    entry.key = WriteString(writer, it.first);
    entry.value = WriteString(writer, it.second);
    entries.push_back(entry);
  }

  location->Rva = static_cast<RVA>(writer->SeekGet());

  const uint32_t simple_string_dictionary_entries =
      static_cast<uint32_t>(entries.size());
  EXPECT_TRUE(writer->Write(&simple_string_dictionary_entries,
                            sizeof(simple_string_dictionary_entries)));
  for (const MinidumpSimpleStringDictionaryEntry& entry : entries) {
    EXPECT_TRUE(writer->Write(&entry, sizeof(entry)));
  }

  location->DataSize = static_cast<uint32_t>(
      sizeof(simple_string_dictionary_entries) +
      entries.size() * sizeof(MinidumpSimpleStringDictionaryEntry));
}

// Writes |strings| to |writer| as a MinidumpRVAList referencing
// MinidumpUTF8String objects, and populates |location| with a location
// descriptor identifying what was written.
void WriteMinidumpStringList(MINIDUMP_LOCATION_DESCRIPTOR* location,
                             FileWriterInterface* writer,
                             const std::vector<std::string>& strings) {
  std::vector<RVA> rvas;
  for (const std::string& string : strings) {
    rvas.push_back(WriteString(writer, string));
  }

  location->Rva = static_cast<RVA>(writer->SeekGet());

  const uint32_t string_list_entries = static_cast<uint32_t>(rvas.size());
  EXPECT_TRUE(writer->Write(&string_list_entries, sizeof(string_list_entries)));
  for (RVA rva : rvas) {
    EXPECT_TRUE(writer->Write(&rva, sizeof(rva)));
  }

  location->DataSize = static_cast<uint32_t>(sizeof(string_list_entries) +
                                             rvas.size() * sizeof(RVA));
}

// Writes |data| to |writer| as a MinidumpByteArray, and returns the file offset
// from the beginning of the string.
RVA WriteByteArray(FileWriterInterface* writer,
                   const std::vector<uint8_t> data) {
  auto rva = static_cast<RVA>(writer->SeekGet());

  auto length = static_cast<uint32_t>(data.size());
  EXPECT_TRUE(writer->Write(&length, sizeof(length)));
  EXPECT_TRUE(writer->Write(data.data(), length));

  return rva;
}

// Writes |annotations| to |writer| as a MinidumpAnnotationList, and populates
// |location| with a location descriptor identifying what was written.
void WriteMinidumpAnnotationList(
    MINIDUMP_LOCATION_DESCRIPTOR* location,
    FileWriterInterface* writer,
    const std::vector<AnnotationSnapshot>& annotations) {
  std::vector<MinidumpAnnotation> minidump_annotations;
  for (const auto& it : annotations) {
    MinidumpAnnotation annotation;
    annotation.name = WriteString(writer, it.name);
    annotation.type = it.type;
    annotation.reserved = 0;
    annotation.value = WriteByteArray(writer, it.value);
    minidump_annotations.push_back(annotation);
  }

  location->Rva = static_cast<RVA>(writer->SeekGet());

  auto count = static_cast<uint32_t>(minidump_annotations.size());
  EXPECT_TRUE(writer->Write(&count, sizeof(count)));

  for (const auto& it : minidump_annotations) {
    EXPECT_TRUE(writer->Write(&it, sizeof(MinidumpAnnotation)));
  }

  location->DataSize =
      sizeof(MinidumpAnnotationList) + count * sizeof(MinidumpAnnotation);
}

TEST(ProcessSnapshotMinidump, ClientID) {
  StringFile string_file;

  MINIDUMP_HEADER header = {};
  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  UUID client_id;
  ASSERT_TRUE(
      client_id.InitializeFromString("0001f4a9-d00d-5155-0a55-c0ffeec0ffee"));

  MinidumpCrashpadInfo crashpad_info = {};
  crashpad_info.version = MinidumpCrashpadInfo::kVersion;
  crashpad_info.client_id = client_id;

  MINIDUMP_DIRECTORY crashpad_info_directory = {};
  crashpad_info_directory.StreamType = kMinidumpStreamTypeCrashpadInfo;
  crashpad_info_directory.Location.Rva =
      static_cast<RVA>(string_file.SeekGet());
  EXPECT_TRUE(string_file.Write(&crashpad_info, sizeof(crashpad_info)));
  crashpad_info_directory.Location.DataSize = sizeof(crashpad_info);

  header.StreamDirectoryRva = static_cast<RVA>(string_file.SeekGet());
  EXPECT_TRUE(string_file.Write(&crashpad_info_directory,
                                sizeof(crashpad_info_directory)));

  header.Signature = MINIDUMP_SIGNATURE;
  header.Version = MINIDUMP_VERSION;
  header.NumberOfStreams = 1;
  EXPECT_TRUE(string_file.SeekSet(0));
  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  ProcessSnapshotMinidump process_snapshot;
  EXPECT_TRUE(process_snapshot.Initialize(&string_file));

  UUID actual_client_id;
  process_snapshot.ClientID(&actual_client_id);
  EXPECT_EQ(actual_client_id, client_id);

  EXPECT_TRUE(process_snapshot.AnnotationsSimpleMap().empty());
}

TEST(ProcessSnapshotMinidump, AnnotationsSimpleMap) {
  StringFile string_file;

  MINIDUMP_HEADER header = {};
  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  MinidumpCrashpadInfo crashpad_info = {};
  crashpad_info.version = MinidumpCrashpadInfo::kVersion;

  std::map<std::string, std::string> dictionary;
  dictionary["the first key"] = "THE FIRST VALUE EVER!";
  dictionary["2key"] = "a lowly second value";
  WriteMinidumpSimpleStringDictionary(
      &crashpad_info.simple_annotations, &string_file, dictionary);

  MINIDUMP_DIRECTORY crashpad_info_directory = {};
  crashpad_info_directory.StreamType = kMinidumpStreamTypeCrashpadInfo;
  crashpad_info_directory.Location.Rva =
      static_cast<RVA>(string_file.SeekGet());
  EXPECT_TRUE(string_file.Write(&crashpad_info, sizeof(crashpad_info)));
  crashpad_info_directory.Location.DataSize = sizeof(crashpad_info);

  header.StreamDirectoryRva = static_cast<RVA>(string_file.SeekGet());
  EXPECT_TRUE(string_file.Write(&crashpad_info_directory,
                                sizeof(crashpad_info_directory)));

  header.Signature = MINIDUMP_SIGNATURE;
  header.Version = MINIDUMP_VERSION;
  header.NumberOfStreams = 1;
  EXPECT_TRUE(string_file.SeekSet(0));
  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  ProcessSnapshotMinidump process_snapshot;
  EXPECT_TRUE(process_snapshot.Initialize(&string_file));

  UUID client_id;
  process_snapshot.ClientID(&client_id);
  EXPECT_EQ(client_id, UUID());

  const auto annotations_simple_map = process_snapshot.AnnotationsSimpleMap();
  EXPECT_EQ(annotations_simple_map, dictionary);
}

TEST(ProcessSnapshotMinidump, AnnotationObjects) {
  StringFile string_file;

  MINIDUMP_HEADER header{};
  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  std::vector<AnnotationSnapshot> annotations;
  annotations.emplace_back(
      AnnotationSnapshot("name 1", 0xBBBB, {'t', 'e', '\0', 's', 't', '\0'}));
  annotations.emplace_back(
      AnnotationSnapshot("name 2", 0xABBA, {0xF0, 0x9F, 0x92, 0x83}));

  MINIDUMP_LOCATION_DESCRIPTOR location;
  WriteMinidumpAnnotationList(&location, &string_file, annotations);

  std::vector<AnnotationSnapshot> read_annotations;
  EXPECT_TRUE(internal::ReadMinidumpAnnotationList(
      &string_file, location, &read_annotations));

  EXPECT_EQ(read_annotations, annotations);
}

TEST(ProcessSnapshotMinidump, Modules) {
  StringFile string_file;

  MINIDUMP_HEADER header = {};
  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  MINIDUMP_MODULE minidump_module = {};
  uint32_t minidump_module_count = 4;

  MINIDUMP_DIRECTORY minidump_module_list_directory = {};
  minidump_module_list_directory.StreamType = kMinidumpStreamTypeModuleList;
  minidump_module_list_directory.Location.DataSize =
      sizeof(MINIDUMP_MODULE_LIST) +
      minidump_module_count * sizeof(MINIDUMP_MODULE);
  minidump_module_list_directory.Location.Rva =
      static_cast<RVA>(string_file.SeekGet());

  EXPECT_TRUE(
      string_file.Write(&minidump_module_count, sizeof(minidump_module_count)));
  for (uint32_t minidump_module_index = 0;
       minidump_module_index < minidump_module_count;
       ++minidump_module_index) {
    EXPECT_TRUE(string_file.Write(&minidump_module, sizeof(minidump_module)));
  }

  MinidumpModuleCrashpadInfo crashpad_module_0 = {};
  crashpad_module_0.version = MinidumpModuleCrashpadInfo::kVersion;
  std::map<std::string, std::string> dictionary_0;
  dictionary_0["ptype"] = "browser";
  dictionary_0["pid"] = "12345";
  WriteMinidumpSimpleStringDictionary(
      &crashpad_module_0.simple_annotations, &string_file, dictionary_0);

  MinidumpModuleCrashpadInfoLink crashpad_module_0_link = {};
  crashpad_module_0_link.minidump_module_list_index = 0;
  crashpad_module_0_link.location.DataSize = sizeof(crashpad_module_0);
  crashpad_module_0_link.location.Rva = static_cast<RVA>(string_file.SeekGet());
  EXPECT_TRUE(string_file.Write(&crashpad_module_0, sizeof(crashpad_module_0)));

  MinidumpModuleCrashpadInfo crashpad_module_2 = {};
  crashpad_module_2.version = MinidumpModuleCrashpadInfo::kVersion;
  std::map<std::string, std::string> dictionary_2;
  dictionary_2["fakemodule"] = "yes";
  WriteMinidumpSimpleStringDictionary(
      &crashpad_module_2.simple_annotations, &string_file, dictionary_2);

  std::vector<std::string> list_annotations_2;
  list_annotations_2.push_back("first string");
  list_annotations_2.push_back("last string");
  WriteMinidumpStringList(
      &crashpad_module_2.list_annotations, &string_file, list_annotations_2);

  MinidumpModuleCrashpadInfoLink crashpad_module_2_link = {};
  crashpad_module_2_link.minidump_module_list_index = 2;
  crashpad_module_2_link.location.DataSize = sizeof(crashpad_module_2);
  crashpad_module_2_link.location.Rva = static_cast<RVA>(string_file.SeekGet());
  EXPECT_TRUE(string_file.Write(&crashpad_module_2, sizeof(crashpad_module_2)));

  MinidumpModuleCrashpadInfo crashpad_module_4 = {};
  crashpad_module_4.version = MinidumpModuleCrashpadInfo::kVersion;
  std::vector<AnnotationSnapshot> annotations_4{
      {"first one", 0xBADE, {'a', 'b', 'c'}},
      {"2", 0xEDD1, {0x11, 0x22, 0x33}},
      {"threeeeee", 0xDADA, {'f'}},
  };
  WriteMinidumpAnnotationList(
      &crashpad_module_4.annotation_objects, &string_file, annotations_4);

  MinidumpModuleCrashpadInfoLink crashpad_module_4_link = {};
  crashpad_module_4_link.minidump_module_list_index = 3;
  crashpad_module_4_link.location.DataSize = sizeof(crashpad_module_4);
  crashpad_module_4_link.location.Rva = static_cast<RVA>(string_file.SeekGet());
  EXPECT_TRUE(string_file.Write(&crashpad_module_4, sizeof(crashpad_module_4)));

  MinidumpCrashpadInfo crashpad_info = {};
  crashpad_info.version = MinidumpCrashpadInfo::kVersion;

  uint32_t crashpad_module_count = 3;

  crashpad_info.module_list.DataSize =
      sizeof(MinidumpModuleCrashpadInfoList) +
      crashpad_module_count * sizeof(MinidumpModuleCrashpadInfoLink);
  crashpad_info.module_list.Rva = static_cast<RVA>(string_file.SeekGet());

  EXPECT_TRUE(
      string_file.Write(&crashpad_module_count, sizeof(crashpad_module_count)));
  EXPECT_TRUE(string_file.Write(&crashpad_module_0_link,
                                sizeof(crashpad_module_0_link)));
  EXPECT_TRUE(string_file.Write(&crashpad_module_2_link,
                                sizeof(crashpad_module_2_link)));
  EXPECT_TRUE(string_file.Write(&crashpad_module_4_link,
                                sizeof(crashpad_module_4_link)));

  MINIDUMP_DIRECTORY crashpad_info_directory = {};
  crashpad_info_directory.StreamType = kMinidumpStreamTypeCrashpadInfo;
  crashpad_info_directory.Location.DataSize = sizeof(crashpad_info);
  crashpad_info_directory.Location.Rva =
      static_cast<RVA>(string_file.SeekGet());
  EXPECT_TRUE(string_file.Write(&crashpad_info, sizeof(crashpad_info)));

  header.StreamDirectoryRva = static_cast<RVA>(string_file.SeekGet());
  EXPECT_TRUE(string_file.Write(&minidump_module_list_directory,
                                sizeof(minidump_module_list_directory)));
  EXPECT_TRUE(string_file.Write(&crashpad_info_directory,
                                sizeof(crashpad_info_directory)));

  header.Signature = MINIDUMP_SIGNATURE;
  header.Version = MINIDUMP_VERSION;
  header.NumberOfStreams = 2;
  EXPECT_TRUE(string_file.SeekSet(0));
  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  ProcessSnapshotMinidump process_snapshot;
  EXPECT_TRUE(process_snapshot.Initialize(&string_file));

  std::vector<const ModuleSnapshot*> modules = process_snapshot.Modules();
  ASSERT_EQ(modules.size(), minidump_module_count);

  auto annotations_simple_map = modules[0]->AnnotationsSimpleMap();
  EXPECT_EQ(annotations_simple_map, dictionary_0);

  auto annotations_vector = modules[0]->AnnotationsVector();
  EXPECT_TRUE(annotations_vector.empty());

  annotations_simple_map = modules[1]->AnnotationsSimpleMap();
  EXPECT_TRUE(annotations_simple_map.empty());

  annotations_vector = modules[1]->AnnotationsVector();
  EXPECT_TRUE(annotations_vector.empty());

  annotations_simple_map = modules[2]->AnnotationsSimpleMap();
  EXPECT_EQ(annotations_simple_map, dictionary_2);

  annotations_vector = modules[2]->AnnotationsVector();
  EXPECT_EQ(annotations_vector, list_annotations_2);

  auto annotation_objects = modules[3]->AnnotationObjects();
  EXPECT_EQ(annotation_objects, annotations_4);
}

TEST(ProcessSnapshotMinidump, ProcessID) {
  StringFile string_file;

  MINIDUMP_HEADER header = {};
  ASSERT_TRUE(string_file.Write(&header, sizeof(header)));

  static const pid_t kTestProcessId = 42;
  MINIDUMP_MISC_INFO misc_info = {};
  misc_info.SizeOfInfo = sizeof(misc_info);
  misc_info.Flags1 = MINIDUMP_MISC1_PROCESS_ID;
  misc_info.ProcessId = kTestProcessId;

  MINIDUMP_DIRECTORY misc_directory = {};
  misc_directory.StreamType = kMinidumpStreamTypeMiscInfo;
  misc_directory.Location.DataSize = sizeof(misc_info);
  misc_directory.Location.Rva = static_cast<RVA>(string_file.SeekGet());
  ASSERT_TRUE(string_file.Write(&misc_info, sizeof(misc_info)));

  header.StreamDirectoryRva = static_cast<RVA>(string_file.SeekGet());
  ASSERT_TRUE(string_file.Write(&misc_directory, sizeof(misc_directory)));

  header.Signature = MINIDUMP_SIGNATURE;
  header.Version = MINIDUMP_VERSION;
  header.NumberOfStreams = 1;
  ASSERT_TRUE(string_file.SeekSet(0));
  ASSERT_TRUE(string_file.Write(&header, sizeof(header)));

  ProcessSnapshotMinidump process_snapshot;
  ASSERT_TRUE(process_snapshot.Initialize(&string_file));
  EXPECT_EQ(process_snapshot.ProcessID(), kTestProcessId);
}

TEST(ProcessSnapshotMinidump, Threads) {
  StringFile string_file;

  MINIDUMP_HEADER header = {};
  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  MINIDUMP_THREAD minidump_thread = {};
  uint32_t minidump_thread_count = 4;

  minidump_thread.ThreadId = 42;
  minidump_thread.Teb = 24;

  MINIDUMP_DIRECTORY minidump_thread_list_directory = {};
  minidump_thread_list_directory.StreamType = kMinidumpStreamTypeThreadList;
  minidump_thread_list_directory.Location.DataSize =
      sizeof(MINIDUMP_THREAD_LIST) +
      minidump_thread_count * sizeof(MINIDUMP_THREAD);
  minidump_thread_list_directory.Location.Rva =
      static_cast<RVA>(string_file.SeekGet());

  // Fields in MINIDUMP_THREAD_LIST.
  EXPECT_TRUE(
      string_file.Write(&minidump_thread_count, sizeof(minidump_thread_count)));
  for (uint32_t minidump_thread_index = 0;
       minidump_thread_index < minidump_thread_count;
       ++minidump_thread_index) {
    EXPECT_TRUE(string_file.Write(&minidump_thread, sizeof(minidump_thread)));
    minidump_thread.ThreadId++;
  }

  header.StreamDirectoryRva = static_cast<RVA>(string_file.SeekGet());
  EXPECT_TRUE(string_file.Write(&minidump_thread_list_directory,
                                sizeof(minidump_thread_list_directory)));

  header.Signature = MINIDUMP_SIGNATURE;
  header.Version = MINIDUMP_VERSION;
  header.NumberOfStreams = 1;
  EXPECT_TRUE(string_file.SeekSet(0));
  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  ProcessSnapshotMinidump process_snapshot;
  EXPECT_TRUE(process_snapshot.Initialize(&string_file));

  std::vector<const ThreadSnapshot*> threads = process_snapshot.Threads();
  ASSERT_EQ(threads.size(), minidump_thread_count);

  uint32_t thread_id = 42;
  for (const auto& thread : threads) {
    EXPECT_EQ(thread->ThreadID(), thread_id);
    EXPECT_EQ(thread->ThreadSpecificDataAddress(), 24UL);
    thread_id++;
  }
}

TEST(ProcessSnapshotMinidump, System) {
  const char* cpu_info = "GenuineIntel";
  const uint32_t* cpu_info_bytes = reinterpret_cast<const uint32_t*>(cpu_info);
  StringFile string_file;

  MINIDUMP_HEADER header = {};
  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  MINIDUMP_SYSTEM_INFO minidump_system_info = {};

  minidump_system_info.ProcessorArchitecture = kMinidumpCPUArchitectureX86;
  minidump_system_info.ProcessorLevel = 3;
  minidump_system_info.ProcessorRevision = 3;
  minidump_system_info.NumberOfProcessors = 8;
  minidump_system_info.ProductType = kMinidumpOSTypeServer;
  minidump_system_info.PlatformId = kMinidumpOSFuchsia;
  minidump_system_info.MajorVersion = 3;
  minidump_system_info.MinorVersion = 4;
  minidump_system_info.BuildNumber = 56;
  minidump_system_info.CSDVersionRva = WriteString(&string_file, "Snazzle");
  minidump_system_info.Cpu.X86CpuInfo.VendorId[0] = cpu_info_bytes[0];
  minidump_system_info.Cpu.X86CpuInfo.VendorId[1] = cpu_info_bytes[1];
  minidump_system_info.Cpu.X86CpuInfo.VendorId[2] = cpu_info_bytes[2];

  MINIDUMP_DIRECTORY minidump_system_info_directory = {};
  minidump_system_info_directory.StreamType = kMinidumpStreamTypeSystemInfo;
  minidump_system_info_directory.Location.DataSize =
      sizeof(MINIDUMP_SYSTEM_INFO);
  minidump_system_info_directory.Location.Rva =
      static_cast<RVA>(string_file.SeekGet());

  ASSERT_TRUE(string_file.Write(&minidump_system_info,
                                sizeof(minidump_system_info)));

  header.StreamDirectoryRva = static_cast<RVA>(string_file.SeekGet());
  ASSERT_TRUE(string_file.Write(&minidump_system_info_directory,
                                sizeof(minidump_system_info_directory)));

  header.Signature = MINIDUMP_SIGNATURE;
  header.Version = MINIDUMP_VERSION;
  header.NumberOfStreams = 1;
  EXPECT_TRUE(string_file.SeekSet(0));
  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  ProcessSnapshotMinidump process_snapshot;
  EXPECT_TRUE(process_snapshot.Initialize(&string_file));

  const SystemSnapshot* s = process_snapshot.System();

  EXPECT_EQ(s->GetCPUArchitecture(), kCPUArchitectureX86);
  EXPECT_EQ(s->CPURevision(), 3UL);
  EXPECT_EQ(s->CPUVendor(), "GenuineIntel");
  EXPECT_EQ(s->GetOperatingSystem(),
            SystemSnapshot::OperatingSystem::kOperatingSystemFuchsia);

  int major, minor, bugfix;
  std::string build;
  s->OSVersion(&major, &minor, &bugfix, &build);

  EXPECT_EQ(major, 3);
  EXPECT_EQ(minor, 4);
  EXPECT_EQ(bugfix, 56);
  EXPECT_EQ(build, "Snazzle");
}

TEST(ProcessSnapshotMinidump, ThreadContextARM64) {
  StringFile string_file;

  MINIDUMP_HEADER header = {};
  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  MINIDUMP_SYSTEM_INFO minidump_system_info = {};

  minidump_system_info.ProcessorArchitecture = kMinidumpCPUArchitectureARM64;
  minidump_system_info.ProductType = kMinidumpOSTypeServer;
  minidump_system_info.PlatformId = kMinidumpOSFuchsia;
  minidump_system_info.CSDVersionRva = WriteString(&string_file, "");

  MINIDUMP_DIRECTORY minidump_system_info_directory = {};
  minidump_system_info_directory.StreamType = kMinidumpStreamTypeSystemInfo;
  minidump_system_info_directory.Location.DataSize =
      sizeof(MINIDUMP_SYSTEM_INFO);
  minidump_system_info_directory.Location.Rva =
      static_cast<RVA>(string_file.SeekGet());

  ASSERT_TRUE(string_file.Write(&minidump_system_info,
                                sizeof(minidump_system_info)));

  MINIDUMP_THREAD minidump_thread = {};
  uint32_t minidump_thread_count = 1;

  minidump_thread.ThreadId = 42;
  minidump_thread.Teb = 24;

  MinidumpContextARM64 minidump_context;

  minidump_context.context_flags = kMinidumpContextARM64Full;

  minidump_context.cpsr = 0;

  for (int i = 0; i < 29; i++) {
    minidump_context.regs[i] = i + 1;
  }

  minidump_context.fp = 30;
  minidump_context.lr = 31;
  minidump_context.sp = 32;
  minidump_context.pc = 33;

  for (int i = 0; i < 32; i++) {
    minidump_context.fpsimd[i].lo = i * 2 + 34;
    minidump_context.fpsimd[i].hi = i * 2 + 35;
  }

  minidump_context.fpcr = 98;
  minidump_context.fpsr = 99;

  for (int i = 0; i < 8; i++) {
    minidump_context.bcr[i] = i * 2 + 100;
    minidump_context.bvr[i] = i * 2 + 101;
  }

  for (int i = 0; i < 2; i++) {
    minidump_context.wcr[i] = i * 2 + 115;
    minidump_context.wvr[i] = i * 2 + 116;
  }

  minidump_thread.ThreadContext.DataSize = sizeof(minidump_context);
  minidump_thread.ThreadContext.Rva = static_cast<RVA>(string_file.SeekGet());

  EXPECT_TRUE(string_file.Write(&minidump_context, sizeof(minidump_context)));

  MINIDUMP_DIRECTORY minidump_thread_list_directory = {};
  minidump_thread_list_directory.StreamType = kMinidumpStreamTypeThreadList;
  minidump_thread_list_directory.Location.DataSize =
      sizeof(MINIDUMP_THREAD_LIST) +
      minidump_thread_count * sizeof(MINIDUMP_THREAD);
  minidump_thread_list_directory.Location.Rva =
      static_cast<RVA>(string_file.SeekGet());

  // Fields in MINIDUMP_THREAD_LIST.
  EXPECT_TRUE(
      string_file.Write(&minidump_thread_count, sizeof(minidump_thread_count)));
  EXPECT_TRUE(string_file.Write(&minidump_thread, sizeof(minidump_thread)));

  header.StreamDirectoryRva = static_cast<RVA>(string_file.SeekGet());
  ASSERT_TRUE(string_file.Write(&minidump_system_info_directory,
                                sizeof(minidump_system_info_directory)));
  ASSERT_TRUE(string_file.Write(&minidump_thread_list_directory,
                                sizeof(minidump_thread_list_directory)));

  header.Signature = MINIDUMP_SIGNATURE;
  header.Version = MINIDUMP_VERSION;
  header.NumberOfStreams = 2;
  EXPECT_TRUE(string_file.SeekSet(0));
  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  ProcessSnapshotMinidump process_snapshot;
  EXPECT_TRUE(process_snapshot.Initialize(&string_file));

  std::vector<const ThreadSnapshot*> threads = process_snapshot.Threads();
  ASSERT_EQ(threads.size(), minidump_thread_count);

  const CPUContext* ctx_generic = threads[0]->Context();

  ASSERT_EQ(ctx_generic->architecture, CPUArchitecture::kCPUArchitectureARM64);

  const CPUContextARM64* ctx = ctx_generic->arm64;

  EXPECT_EQ(ctx->pstate, 0UL);

  for (unsigned int i = 0; i < 31; i++) {
    EXPECT_EQ(ctx->regs[i], i + 1);
  }

  EXPECT_EQ(ctx->sp, 32UL);
  EXPECT_EQ(ctx->pc, 33UL);
  EXPECT_EQ(ctx->fpcr, 98UL);
  EXPECT_EQ(ctx->fpsr, 99UL);

  for (unsigned int i = 0; i < 32; i++) {
    EXPECT_EQ(ctx->fpsimd[i].lo, i * 2 + 34);
    EXPECT_EQ(ctx->fpsimd[i].hi, i * 2 + 35);
  }
}

TEST(ProcessSnapshotMinidump, ThreadContextX86_64) {
  StringFile string_file;

  MINIDUMP_HEADER header = {};
  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  MINIDUMP_SYSTEM_INFO minidump_system_info = {};

  minidump_system_info.ProcessorArchitecture = kMinidumpCPUArchitectureAMD64;
  minidump_system_info.ProductType = kMinidumpOSTypeServer;
  minidump_system_info.PlatformId = kMinidumpOSFuchsia;
  minidump_system_info.CSDVersionRva = WriteString(&string_file, "");

  MINIDUMP_DIRECTORY minidump_system_info_directory = {};
  minidump_system_info_directory.StreamType = kMinidumpStreamTypeSystemInfo;
  minidump_system_info_directory.Location.DataSize =
      sizeof(MINIDUMP_SYSTEM_INFO);
  minidump_system_info_directory.Location.Rva =
      static_cast<RVA>(string_file.SeekGet());

  ASSERT_TRUE(string_file.Write(&minidump_system_info,
                                sizeof(minidump_system_info)));

  MINIDUMP_THREAD minidump_thread = {};
  uint32_t minidump_thread_count = 1;

  minidump_thread.ThreadId = 42;
  minidump_thread.Teb = 24;

  MinidumpContextAMD64 minidump_context;

  minidump_context.context_flags = kMinidumpContextAMD64Full;

  minidump_context.mx_csr = 0;
  minidump_context.cs = 1;
  minidump_context.ds = 2;
  minidump_context.es = 3;
  minidump_context.fs = 4;
  minidump_context.gs = 5;
  minidump_context.ss = 6;
  minidump_context.eflags = 7;
  minidump_context.dr0 = 8;
  minidump_context.dr1 = 9;
  minidump_context.dr2 = 10;
  minidump_context.dr3 = 11;
  minidump_context.dr6 = 12;
  minidump_context.dr7 = 13;
  minidump_context.rax = 14;
  minidump_context.rcx = 15;
  minidump_context.rdx = 16;
  minidump_context.rbx = 17;
  minidump_context.rsp = 18;
  minidump_context.rbp = 19;
  minidump_context.rsi = 20;
  minidump_context.rdi = 21;
  minidump_context.r8 = 22;
  minidump_context.r9 = 23;
  minidump_context.r10 = 24;
  minidump_context.r11 = 25;
  minidump_context.r12 = 26;
  minidump_context.r13 = 27;
  minidump_context.r14 = 28;
  minidump_context.r15 = 29;
  minidump_context.rip = 30;
  minidump_context.vector_control = 31;
  minidump_context.debug_control = 32;
  minidump_context.last_branch_to_rip = 33;
  minidump_context.last_branch_from_rip = 34;
  minidump_context.last_exception_to_rip = 35;
  minidump_context.last_exception_from_rip = 36;
  minidump_context.fxsave.fcw = 37;
  minidump_context.fxsave.fsw = 38;
  minidump_context.fxsave.ftw = 39;
  minidump_context.fxsave.reserved_1 = 40;
  minidump_context.fxsave.fop = 41;
  minidump_context.fxsave.fpu_ip_64 = 42;
  minidump_context.fxsave.fpu_dp_64 = 43;

  for (size_t i = 0; i < arraysize(minidump_context.vector_register); i++) {
    minidump_context.vector_register[i].lo = i * 2 + 44;
    minidump_context.vector_register[i].hi = i * 2 + 45;
  }

  for (uint8_t i = 0; i < arraysize(minidump_context.fxsave.reserved_4); i++) {
    minidump_context.fxsave.reserved_4[i] = i * 2 + 115;
    minidump_context.fxsave.available[i] = i * 2 + 116;
  }

  for (size_t i = 0; i < arraysize(minidump_context.fxsave.st_mm); i++) {
    for (uint8_t j = 0;
         j < arraysize(minidump_context.fxsave.st_mm[0].mm_value);
         j++) {
      minidump_context.fxsave.st_mm[i].mm_value[j] = j + 1;
      minidump_context.fxsave.st_mm[i].mm_reserved[j] = j + 1;
    }
  }

  for (size_t i = 0; i < arraysize(minidump_context.fxsave.xmm); i++) {
    for (uint8_t j = 0; j < arraysize(minidump_context.fxsave.xmm[0]); j++) {
      minidump_context.fxsave.xmm[i][j] = j + 1;
    }
  }

  minidump_thread.ThreadContext.DataSize = sizeof(minidump_context);
  minidump_thread.ThreadContext.Rva = static_cast<RVA>(string_file.SeekGet());

  EXPECT_TRUE(string_file.Write(&minidump_context, sizeof(minidump_context)));

  MINIDUMP_DIRECTORY minidump_thread_list_directory = {};
  minidump_thread_list_directory.StreamType = kMinidumpStreamTypeThreadList;
  minidump_thread_list_directory.Location.DataSize =
      sizeof(MINIDUMP_THREAD_LIST) +
      minidump_thread_count * sizeof(MINIDUMP_THREAD);
  minidump_thread_list_directory.Location.Rva =
      static_cast<RVA>(string_file.SeekGet());

  // Fields in MINIDUMP_THREAD_LIST.
  EXPECT_TRUE(
      string_file.Write(&minidump_thread_count, sizeof(minidump_thread_count)));
  EXPECT_TRUE(string_file.Write(&minidump_thread, sizeof(minidump_thread)));

  header.StreamDirectoryRva = static_cast<RVA>(string_file.SeekGet());
  ASSERT_TRUE(string_file.Write(&minidump_system_info_directory,
                                sizeof(minidump_system_info_directory)));
  ASSERT_TRUE(string_file.Write(&minidump_thread_list_directory,
                                sizeof(minidump_thread_list_directory)));

  header.Signature = MINIDUMP_SIGNATURE;
  header.Version = MINIDUMP_VERSION;
  header.NumberOfStreams = 2;
  EXPECT_TRUE(string_file.SeekSet(0));
  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  ProcessSnapshotMinidump process_snapshot;
  EXPECT_TRUE(process_snapshot.Initialize(&string_file));

  std::vector<const ThreadSnapshot*> threads = process_snapshot.Threads();
  ASSERT_EQ(threads.size(), minidump_thread_count);

  const CPUContext* ctx_generic = threads[0]->Context();

  ASSERT_EQ(ctx_generic->architecture, CPUArchitecture::kCPUArchitectureX86_64);

  const CPUContextX86_64* ctx = ctx_generic->x86_64;
  EXPECT_EQ(ctx->cs, 1);
  EXPECT_EQ(ctx->fs, 4);
  EXPECT_EQ(ctx->gs, 5);
  EXPECT_EQ(ctx->rflags, 7UL);
  EXPECT_EQ(ctx->dr0, 8UL);
  EXPECT_EQ(ctx->dr1, 9U);
  EXPECT_EQ(ctx->dr2, 10U);
  EXPECT_EQ(ctx->dr3, 11U);
  EXPECT_EQ(ctx->dr4, 12U);
  EXPECT_EQ(ctx->dr5, 13U);
  EXPECT_EQ(ctx->dr6, 12U);
  EXPECT_EQ(ctx->dr7, 13U);
  EXPECT_EQ(ctx->rax, 14U);
  EXPECT_EQ(ctx->rcx, 15U);
  EXPECT_EQ(ctx->rdx, 16U);
  EXPECT_EQ(ctx->rbx, 17U);
  EXPECT_EQ(ctx->rsp, 18U);
  EXPECT_EQ(ctx->rbp, 19U);
  EXPECT_EQ(ctx->rsi, 20U);
  EXPECT_EQ(ctx->rdi, 21U);
  EXPECT_EQ(ctx->r8, 22U);
  EXPECT_EQ(ctx->r9, 23U);
  EXPECT_EQ(ctx->r10, 24U);
  EXPECT_EQ(ctx->r11, 25U);
  EXPECT_EQ(ctx->r12, 26U);
  EXPECT_EQ(ctx->r13, 27U);
  EXPECT_EQ(ctx->r14, 28U);
  EXPECT_EQ(ctx->r15, 29U);
  EXPECT_EQ(ctx->rip, 30U);
  EXPECT_EQ(ctx->fxsave.fcw, 37U);
  EXPECT_EQ(ctx->fxsave.fsw, 38U);
  EXPECT_EQ(ctx->fxsave.ftw, 39U);
  EXPECT_EQ(ctx->fxsave.reserved_1, 40U);
  EXPECT_EQ(ctx->fxsave.fop, 41U);
  EXPECT_EQ(ctx->fxsave.fpu_ip_64, 42U);
  EXPECT_EQ(ctx->fxsave.fpu_dp_64, 43U);

  for (uint8_t i = 0; i < arraysize(ctx->fxsave.reserved_4); i++) {
    EXPECT_EQ(ctx->fxsave.reserved_4[i], i * 2 + 115);
    EXPECT_EQ(ctx->fxsave.available[i], i * 2 + 116);
  }

  for (size_t i = 0; i < arraysize(ctx->fxsave.st_mm); i++) {
    for (uint8_t j = 0;
         j < arraysize(ctx->fxsave.st_mm[0].mm_value);
         j++) {
      EXPECT_EQ(ctx->fxsave.st_mm[i].mm_value[j], j + 1);
      EXPECT_EQ(ctx->fxsave.st_mm[i].mm_reserved[j], j + 1);
    }
  }

  for (size_t i = 0; i < arraysize(ctx->fxsave.xmm); i++) {
    for (uint8_t j = 0; j < arraysize(ctx->fxsave.xmm[0]); j++) {
      EXPECT_EQ(ctx->fxsave.xmm[i][j], j + 1);
    }
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
