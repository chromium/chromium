// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1009553): Remove this wrapper after finding a way to plumb a
// workable temporary path into googletest on Android.

// This wrapper lets us compile gtest-port.cc without its stream redirection
// code. We replace this code with a variant that works on all Chrome platforms.
// This is a temporary workaround until we get good code upstream.
//
// Stream redirection requires the ability to create files in a temporary
// directory. Traditionally, this directory has been /sdcard on Android.
// Commit bf0fe874a27bd6c9a4a35b98e662d2d02f8879a2 changed the Android
// directory to /data/local/tmp, which is not writable in Chrome's testing
// setup. We work around this problem by using the old code for now.
//
// It is tempting to consider disabling the stream redirection code altogether,
// by setting GTEST_HAS_STREAM_REDIRECTION to 0 in googletest's BUILD.gn.
// This breaks gtest-death-test.cc, which assumes the existence of
// testing::internal::{GetCapturedStderr,CaptureStderr} without any macro
// checking.

#define GTEST_HAS_STREAM_REDIRECTION 0
#include "third_party/googletest/src/googletest/src/gtest-port.cc"

namespace testing {
namespace internal {

// Verbatim copy from gtest-port.cc, since it only provides these constants when
// GTEST_HAS_STREAM_REDIRECTION is true.
#if defined(_MSC_VER) || defined(__BORLANDC__)
// MSVC and C++Builder do not provide a definition of STDERR_FILENO.
const int kStdOutFileno = 1;
const int kStdErrFileno = 2;
#else
const int kStdOutFileno = STDOUT_FILENO;
const int kStdErrFileno = STDERR_FILENO;
#endif  // defined(_MSC_VER) || defined(__BORLANDC__)

// Object that captures an output stream (stdout/stderr).
class CapturedStream {
 public:
  // The ctor redirects the stream to a temporary file.
  explicit CapturedStream(int fd) : fd_(fd), uncaptured_fd_(dup(fd)) {
    std::string temp_dir = ::testing::TempDir();

    // testing::TempDir() should return a directory with a path separator.
    // However, this rule was documented fairly recently, so we normalize across
    // implementations with and without a trailing path separator.
    if (temp_dir.back() != GTEST_PATH_SEP_[0])
      temp_dir.push_back(GTEST_PATH_SEP_[0]);

#if GTEST_OS_WINDOWS
    char temp_file_path[MAX_PATH + 1] = {'\0'};  // NOLINT
    const UINT success = ::GetTempFileNameA(temp_dir.c_str(), "gtest_redir",
                                            0,  // Generate unique file name.
                                            temp_file_path);
    GTEST_CHECK_(success != 0)
        << "Unable to create a temporary file in " << temp_dir;
    const int captured_fd = creat(temp_file_path, _S_IREAD | _S_IWRITE);
    GTEST_CHECK_(captured_fd != -1)
        << "Unable to open temporary file " << temp_file_path;
    filename_ = temp_file_path;
#else
    std::string name_template = temp_dir + "gtest_captured_stream.XXXXXX";

    // mkstemp() modifies the string bytes in place, and does not go beyond the
    // string's length. This results in well-defined behavior in C++17.
    //
    // The const_cast is needed below C++17. The constraints on std::string
    // implementations in C++11 and above make assumption behind the const_cast
    // fairly safe.
    const int captured_fd = ::mkstemp(const_cast<char*>(name_template.data()));
    GTEST_CHECK_(captured_fd != -1)
        << "Failed to create tmp file " << name_template
        << " for test; does the test have write access to the directory?";
    filename_ = std::move(name_template);
#endif  // GTEST_OS_WINDOWS
    fflush(nullptr);
    dup2(captured_fd, fd_);
    close(captured_fd);
  }

  CapturedStream(const CapturedStream&) = delete;
  CapturedStream& operator=(const CapturedStream&) = delete;

  ~CapturedStream() { remove(filename_.c_str()); }

  std::string GetCapturedString() {
    if (uncaptured_fd_ != -1) {
      // Restores the original stream.
      fflush(nullptr);
      dup2(uncaptured_fd_, fd_);
      close(uncaptured_fd_);
      uncaptured_fd_ = -1;
    }

    FILE* const file = posix::FOpen(filename_.c_str(), "r");
    if (file == nullptr) {
      GTEST_LOG_(FATAL) << "Failed to open tmp file " << filename_
                        << " for capturing stream.";
    }
    const std::string content = ReadEntireFile(file);
    posix::FClose(file);
    return content;
  }

 private:
  const int fd_;  // A stream to capture.
  int uncaptured_fd_;
  // Name of the temporary file holding the stderr output.
  ::std::string filename_;
};

static CapturedStream* g_captured_stderr = nullptr;
static CapturedStream* g_captured_stdout = nullptr;

// Starts capturing an output stream (stdout/stderr).
static void CaptureStream(int fd,
                          const char* stream_name,
                          CapturedStream** stream) {
  if (*stream != nullptr) {
    GTEST_LOG_(FATAL) << "Only one " << stream_name
                      << " capturer can exist at a time.";
  }
  *stream = new CapturedStream(fd);
}

// Stops capturing the output stream and returns the captured string.
static std::string GetCapturedStream(CapturedStream** captured_stream) {
  const std::string content = (*captured_stream)->GetCapturedString();

  delete *captured_stream;
  *captured_stream = nullptr;

  return content;
}

// Starts capturing stdout.
void CaptureStdout() {
  CaptureStream(kStdOutFileno, "stdout", &g_captured_stdout);
}

// Starts capturing stderr.
void CaptureStderr() {
  CaptureStream(kStdErrFileno, "stderr", &g_captured_stderr);
}

// Stops capturing stdout and returns the captured string.
std::string GetCapturedStdout() {
  return GetCapturedStream(&g_captured_stdout);
}

// Stops capturing stderr and returns the captured string.
std::string GetCapturedStderr() {
  return GetCapturedStream(&g_captured_stderr);
}

}  // namespace internal
}  // namespace testing
