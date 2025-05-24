// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
#pragma allow_unsafe_buffers
#endif

#include <sys/stat.h>

#include <cstddef>
#include <iostream>
#include <random>
#include <sstream>

#include "archive_reader.h"
#include "archive_writer.h"
#include "base/strings/string_split.h"
#include "crc32_hasher.h"
#include "zst_compressor.h"
#include "zst_decompressor.h"

void PrintUsageInfo(std::string program_name) {
  std::cerr << "Usage: " << program_name << " [hash | archive | extract | pipe]"
            << std::endl;
}

void PrintUsageInfoHash(std::string program_name) {
  std::cerr << "Usage: " << program_name << " hash '" << kFilePathDelimiter
            << "'-separated-file-paths" << std::endl;
  std::cerr << "E.g.: " << program_name << " hash path1" << kFilePathDelimiter
            << "path2" << std::endl;
}

void PrintUsageInfoCompress(std::string program_name) {
  std::cerr << "Usage: " << program_name
            << " compress destination-file-path uncompressed-content"
            << std::endl;
  std::cerr << "E.g.: " << program_name
            << " compress /path/to/compressed/content abcdefg" << std::endl;
}

void PrintUsageInfoArchive(std::string program_name) {
  std::cerr << "Usage: " << program_name
            << " archive [archive-path | -] archive-members-file-path"
            << std::endl;
  std::cerr << "E.g.: " << program_name
            << " archive /path/to/archive /path/to/archive/members/file"
            << std::endl;
}

void PrintUsageInfoExtract(std::string program_name) {
  std::cerr << "Usage: " << program_name << " extract [archive-path | -]"
            << std::endl;
  std::cerr << "E.g.: " << program_name
            << " archive - /path/to/archive/members/file | " << program_name
            << " extract -" << std::endl;
}

void PrintUsageInfoPipe(std::string program_name) {
  std::cerr << "Usage: " << program_name << " pipe named-pipe-path"
            << std::endl;
  std::cerr << "E.g.: " << program_name << " pipe /path/to/named/pipe"
            << std::endl;
}

// The hash command is given a list of kFilePathDelimiter-separated file paths
// which are gzipped and base64-encoded, and it outputs a crc32 hash for each
// file in the list, in the same order as the input list. If a file does not
// exist, outputs a blank line for it.
int DoHash(const std::vector<std::string>& argv) {
  if (argv.size() != 3) {
    PrintUsageInfoHash(argv[0]);
    return 1;
  }

  Crc32Hasher hasher;
  std::vector<std::string> files = hasher.ParseFileList(argv[2]);

  for (const auto& file : files) {
    std::optional<uint32_t> hash = hasher.HashFile(file);
    if (!hash.has_value()) {
      std::cout << "\n";  // Blank line for missing file.
    } else {
      std::cout << std::hex << hash.value() << "\n";
    }
  }
  return 0;
}

// The compress command is given a string that needs to be compressed.
// It compresses the string via zst and saves the result to the specified file.
int DoCompress(const std::vector<std::string>& argv) {
  if (argv.size() != 4) {
    PrintUsageInfoCompress(argv[0]);
    return 1;
  }

  std::string destination_file_path = argv[2];
  std::ofstream output_file_stream;
  output_file_stream.open(destination_file_path,
                          std::ios::binary | std::ios::trunc);
  if (output_file_stream.fail()) {
    std::cerr << "Failed to open the destination file at "
              << destination_file_path << std::endl;
    return 1;
  }

  ZstCompressor compressor(output_file_stream, 3);
  std::string uncompressed_string = argv[3];
  ZstCompressor::UncompressedContent uncompressed_content;
  uncompressed_content.buffer = uncompressed_string.data();
  uncompressed_content.size = uncompressed_string.size();
  compressor.CompressStreaming(uncompressed_content, true);
  return 0;
}

// The archive command creates a zst-compressed archive file. It is given a text
// file that contains the paths to the files that should be included in archive.
// It then creates an archive from these files and compresses the archive via
// zstd. The archive is in a custom file format and can be extracted using the
// extract command below.
int DoArchive(const std::vector<std::string>& argv) {
  if (argv.size() != 4) {
    PrintUsageInfoArchive(argv[0]);
    return 1;
  }

  // If the user passes - as the output archive, then we write to standard
  // output. Otherwise, we write to a file.
  std::string archive_path = argv[2];
  std::ofstream output_file_stream;
  std::ostream& output_stream =
      archive_path == "-" ? std::cout : output_file_stream;
  if (archive_path != "-") {
    output_file_stream.open(archive_path, std::ios::binary | std::ios::trunc);
    if (output_file_stream.fail()) {
      std::cerr << "Failed to open the archive at " << archive_path
                << std::endl;
      return 1;
    }
  }

  // The archive members file contains two lines for each member: the first
  // line is the file path of the member in the host machine, the second line
  // is the file path of the member in the archive.
  std::string archive_members_file_path = argv[3];
  std::ifstream archive_members_file(archive_members_file_path);
  std::vector<ArchiveWriter::ArchiveMember> archive_members;
  while (true) {
    ArchiveWriter::ArchiveMember member;
    if (!std::getline(archive_members_file, member.file_path_in_host)) {
      break;
    }
    if (!std::getline(archive_members_file, member.file_path_in_archive)) {
      std::cerr << "The archive members file contains an odd number of lines!"
                << std::endl;
      return 1;
    }
    archive_members.push_back(member);
  }

  // Now we start creating the archive: first ask the archive writer to create
  // a portion of the uncompressed archive, and then ask the zst compressor to
  // compress it and write to the output stream, and then ask the archive
  // writer to create the next portion of the uncompressed archive and repeat.
  ArchiveWriter writer(std::move(archive_members));
  ZstCompressor compressor(output_stream, 3);
  size_t archive_buffer_size = compressor.GetRecommendedInputBufferSize();
  std::unique_ptr<char[]> archive_buffer =
      std::make_unique<char[]>(archive_buffer_size);
  ZstCompressor::UncompressedContent uncompressed_content;
  while (true) {
    size_t num_bytes_written = writer.CreateArchiveStreaming(
        archive_buffer.get(), archive_buffer_size);
    uncompressed_content.buffer = archive_buffer.get();
    uncompressed_content.size = num_bytes_written;
    bool last_chunk = (num_bytes_written < archive_buffer_size);
    compressor.CompressStreaming(uncompressed_content, last_chunk);
    if (last_chunk) {
      break;
    }
  }
  return 0;
}

// The extract command is given a zst-compressed archive file, and it
// decompresses the file using zstd and extracts the files from the archive.
// It does so in a streaming way (i.e. it reads a portion of the input file and
// extracts it, and then read the next portion of input file and extracts it).
// The input file can be created by the archive command above.
int DoExtract(const std::vector<std::string>& argv) {
  if (argv.size() != 3) {
    PrintUsageInfoExtract(argv[0]);
    return 1;
  }

  // If the user passes - as the input archive, then we read from standard
  // input. Otherwise, we read from a file.
  std::string archive_path = argv[2];
  std::ifstream input_file_stream;
  std::istream& input_stream =
      archive_path == "-" ? std::cin : input_file_stream;
  if (archive_path != "-") {
    input_file_stream.open(archive_path, std::ios::binary);
    if (input_file_stream.fail()) {
      std::cerr << "Failed to open the archive at " << archive_path
                << std::endl;
      return 1;
    }
  }

  // We extract the input archive in a streaming way: first ask the zst
  // decompressor to read a portion of the input and decompress it, and then
  // ask the archive reader to extract the decompressed archive, and then ask
  // the zst decompresssor to read the next portion of the input and repeat.
  ZstDecompressor decompressor(input_stream);
  ArchiveReader reader;
  ZstDecompressor::DecompressedContent decompressed_content;
  while (true) {
    if (decompressor.DecompressStreaming(&decompressed_content)) {
      std::cerr << "Archive reader has not reached the end of the input file "
                   "but there is already no data left. This likely means the "
                   "input data is truncated."
                << std::endl;
      return 1;
    }
    if (reader.ExtractArchiveStreaming(decompressed_content.buffer,
                                       decompressed_content.size)) {
      break;
    }
  }
  return 0;
}

// The pipe command is given a path, and it creates a named pipe at that path
// via a mkfifo() system call.
int DoPipe(const std::vector<std::string>& argv) {
  if (argv.size() != 3) {
    PrintUsageInfoPipe(argv[0]);
    return 1;
  }

  std::string named_pipe_path = argv[2];
  int result = mkfifo(named_pipe_path.c_str(), 0777);
  if (result != 0) {
    std::cerr << "Failed to call mkfifo(): " << strerror(errno) << std::endl;
    return 1;
  }
  return 0;
}

// Given the path to a response file, process the response file and return the
// command line arguments that it contains as a vector of strings.
std::vector<std::string> HandleResponseFile(
    const std::string& response_file_path) {
  std::string response_file_content;
  if (response_file_path.length() >= 4 &&
      response_file_path.substr(response_file_path.length() - 4) == ".zst") {
    // If the path to the response file ends in .zst, then decompress the
    // content of the response file via zst.
    std::ifstream input_file_stream;
    input_file_stream.open(response_file_path, std::ios::binary);
    if (input_file_stream.fail()) {
      std::cerr << "Failed to open the input response file at "
                << response_file_path << std::endl;
      exit(1);
    }
    ZstDecompressor decompressor(input_file_stream);
    ZstDecompressor::DecompressedContent decompressed_content;
    std::stringstream decompressed_string_stream;
    while (true) {
      if (decompressor.DecompressStreaming(&decompressed_content)) {
        break;
      }
      decompressed_string_stream.write(decompressed_content.buffer,
                                       decompressed_content.size);
    }
    response_file_content = decompressed_string_stream.str();
  } else {
    // If the path to the response file does not end in .zst, then read the
    // entire content of the response file.
    std::ifstream input_file_stream;
    input_file_stream.open(response_file_path);
    if (input_file_stream.fail()) {
      std::cerr << "Failed to open the input response file at "
                << response_file_path << std::endl;
      exit(1);
    }
    std::stringstream string_stream;
    string_stream << input_file_stream.rdbuf();
    response_file_content = string_stream.str();
  }
  // Each line in the response file is treated as a separate command line
  // argument.
  return SplitString(response_file_content, "\n", base::KEEP_WHITESPACE,
                     base::SPLIT_WANT_NONEMPTY);
}

int main(int argc, const char* argv[]) {
  // Pre-process the command line arguments and expand all the response files
  // (a response file is identified by the @ symbol).
  std::vector<std::string> processed_argv;
  for (int i = 0; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg.length() >= 1 && arg[0] == '@') {
      std::string response_file_path = arg.substr(1);
      std::vector<std::string> response_file_args =
          HandleResponseFile(response_file_path);
      processed_argv.insert(processed_argv.end(), response_file_args.begin(),
                            response_file_args.end());
    } else {
      processed_argv.push_back(arg);
    }
  }

  if (processed_argv.size() < 2) {
    PrintUsageInfo(processed_argv[0]);
    return 1;
  }

  std::string command = processed_argv[1];
  if (command == "hash") {
    return DoHash(processed_argv);
  } else if (command == "compress") {
    return DoCompress(processed_argv);
  } else if (command == "archive") {
    return DoArchive(processed_argv);
  } else if (command == "extract") {
    return DoExtract(processed_argv);
  } else if (command == "pipe") {
    return DoPipe(processed_argv);
  } else {
    PrintUsageInfo(processed_argv[0]);
    return 1;
  }
}
