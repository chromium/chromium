// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Takes in a list of files and outputs a list of CRC32s in the same order.
// If a file does not exist, outputs a blank line for it.
// It historically used md5, but CRC32 is faster and exists in zlib already.

#include <cstddef>
#ifdef UNSAFE_BUFFERS_BUILD
#pragma allow_unsafe_buffers
#endif

#include <iostream>
#include <random>

#include "crc32_hasher.h"
#include "tar_reader.h"
#include "zst_decompressor.h"

void PrintUsageInfo(std::string program_name) {
  std::cerr << "Usage: " << program_name << " [hash | extract]" << std::endl;
}

void PrintUsageInfoHash(std::string program_name) {
  std::cerr << "Usage: " << program_name << " hash base64-gzipped-'"
            << kFilePathDelimiter << "'-separated-files" << std::endl;
  std::cerr << "E.g.: " << program_name
            << " hash $(echo -n path1:path2 | gzip | base64)" << std::endl;
}

void PrintUsageInfoExtract(std::string program_name) {
  std::cerr << "Usage: " << program_name
            << " extract [archive-path | -] [-e extraction-dir]" << std::endl;
  std::cerr << "E.g.: tar --create --to-stdout file1 file2 | zstd --stdout - | "
            << program_name
            << " extract - -e /absolute/path/to/extraction/directory"
            << std::endl;
}

int main(int argc, const char* argv[]) {
  if (argc < 2) {
    PrintUsageInfo(std::string(argv[0]));
    return 1;
  }

  std::string command = argv[1];
  if (command == "hash") {
    // The hash command is given a list of kFilePathDelimiter-separated file
    // paths which are gzipped and base64-encoded, and it outputs a crc32 hash
    // for each file in the list, in the same order as the input list.
    if (argc != 3) {
      PrintUsageInfoHash(std::string(argv[0]));
      return 1;
    }

    Crc32Hasher hasher;
    std::vector<std::string> files =
        hasher.MakeFileListFromCompressedList(std::string_view(argv[2]));

    for (const auto& file : files) {
      std::optional<uint32_t> hash = hasher.HashFile(file);
      if (!hash.has_value()) {
        std::cout << "\n";  // Blank line for missing file.
      } else {
        std::cout << std::hex << hash.value() << "\n";
      }
    }
    return 0;

  } else if (command == "extract") {
    // The extract command is given a .tar.zst file, and it decompresses the
    // file using zstd and extracts the files from the tarball. It does so in
    // a streaming way (i.e. it reads a portion of the .tar.zst file and
    // extracts it, and then read the next portion of the .tar.zst file and
    // extracts it).
    if (argc < 3) {
      PrintUsageInfoExtract(std::string(argv[0]));
      return 1;
    }

    // If the user passes - as the input archive, then we read from standard
    // input. Otherwise, we read from a file.
    std::string archive_path = argv[2];
    std::ifstream input_file_stream;
    std::istream& input_stream =
        archive_path == "-" ? std::cin : input_file_stream;
    if (archive_path != "-") {
      input_file_stream.open(archive_path);
      if (input_file_stream.fail()) {
        std::cerr << "Failed to open the archive at " << archive_path
                  << std::endl;
        return 1;
      }
    }

    // The -e flag specifies the root extraction directory, which is where the
    // extracted files are placed. If this flag is passed, the input archive
    // must contain relative paths. If this flag is not passed, then the input
    // archive must contain absolute paths.
    std::string extraction_dir = "";
    for (int i = 3; i < argc; ++i) {
      std::string flag = argv[i];
      if (flag == "-e") {
        extraction_dir = argv[i + 1];
        ++i;
      } else {
        std::cerr << "Unrecognized flag: " << flag << std::endl;
        PrintUsageInfo(std::string(argv[0]));
        return 1;
      }
    }

    // We extract the input archive in a streaming way: first ask the zst
    // decompressor to read a portion of the input and decompress it, and then
    // ask the tar reader to extract the decompressed tarball, and then ask
    // the zst decompresssor to read the next portion of the input and repeat.
    ZstDecompressor decompressor(input_stream);
    TarReader reader(extraction_dir);
    ZstDecompressor::DecompressedContent decompressed_content;
    while (true) {
      if (decompressor.DecompressStreaming(&decompressed_content)) {
        std::cerr << "Tar reader has not reached the end of the input tar file "
                     "but there is already no data left. This likely means the "
                     "input data is truncated."
                  << std::endl;
        return 1;
      }
      if (reader.UntarStreaming(decompressed_content.buffer,
                                decompressed_content.size)) {
        break;
      }
    }
    return 0;

  } else {
    PrintUsageInfo(std::string(argv[0]));
    return 1;
  }
}
