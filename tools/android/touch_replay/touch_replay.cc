// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <limits.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>

#include <iostream>
#include <string>
#include <type_traits>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/cstring_view.h"
#include "base/strings/string_number_conversions.h"

// Records and replays touch events on Android device.
//
// An attempt is made to reproduce the delays between events. The primary goal
// of this tool is for the replays to be consistent with each other as much as
// possible to allow for repeating touch gestures. Maintaining consistency with
// the original sequence of touches is only secondary.
//
// The structure of touch events depends heavily on device type and OS version.
// Replaying on a device different from the one used for recording is probably
// not going to work.
//
// Inspired by two Android tools:
// 1. system/core/toolbox/getevent.c
// 2. external/toybox/toys/android/sendevent.c

namespace {

constexpr char kInputDeviceDirPath[] = "/dev/input";
const int kClockMonotonicId = CLOCK_MONOTONIC;

struct TouchInputEventRecord {
  long sec;
  long usec;
  int type;
  int code;
  int value;
#if LONG_MAX > INT_MAX
  // Explicitly zero the padding so `std::has_unique_object_representations_v`
  // will hold.
  int unused_padding = 0;
#endif

  uint64_t InMilliseconds() const {
    return base::checked_cast<uint64_t>(sec) * 1000 +
           base::checked_cast<uint64_t>(usec / 1000);
  }
};
static_assert(std::has_unique_object_representations_v<TouchInputEventRecord>);

class InputDevice {
 public:
  InputDevice(const base::FilePath& full_path, bool read_only);
  ~InputDevice() = default;

  bool is_valid() { return fd_.is_valid(); }
  const base::FilePath& path() { return path_; }
  int fd() { return fd_.get(); }

  bool HasTouch();

  void SendEvents(std::vector<TouchInputEventRecord>& events,
                  int index_first,
                  int index_last,
                  int offset_x,
                  int offset_y);

 private:
  base::ScopedFD fd_;
  base::FilePath path_;
};

InputDevice::InputDevice(const base::FilePath& full_path, bool read_only) {
  int open_flag = read_only ? O_RDONLY : O_RDWR;
  int fd = open(full_path.MaybeAsASCII().c_str(), open_flag | O_CLOEXEC);
  if (fd < 0) {
    PLOG(ERROR) << "open " << full_path.MaybeAsASCII();
    return;
  }
  path_ = full_path;

  // Close the fd on errors.
  base::ScopedFD scoped_fd(fd);

  if (read_only && ioctl(fd, EVIOCSCLOCKID, &kClockMonotonicId) != 0) {
    PLOG(ERROR) << "Could not enable monotonic clock reporting";
    return;
  }

  fd_ = std::move(scoped_fd);
}

bool InputDevice::HasTouch() {
  if (!fd_.is_valid()) {
    return false;
  }

  // Should return true iff the ABS_MT_POSITION_X is found in the output of:
  //   `adb shell getevent -lp`
  // Avoiding to run ioctls on non-ABS_MT_POSITION_X bits. Hopefully there are
  // no side effects from this omission later when the device is used.
  int res;
  ssize_t bits_size = 0;
  uint8_t* bits = nullptr;
  int fd = fd_.get();
  while (true) {
    res = ioctl(fd, EVIOCGBIT(EV_ABS, bits_size), bits);  // Get event bits.
    if (res < bits_size) {
      break;
    }
    bits_size = res + 16;
    bits = static_cast<uint8_t*>(realloc(bits, bits_size * 2));
    if (!bits) {
      LOG(ERROR) << "bits = realloc";
      return false;
    }
  }
  int j = ABS_MT_POSITION_X / 8;
  int k = ABS_MT_POSITION_X % 8;
  struct input_absinfo abs;
  if (j < bits_size && (bits[j] & (1 << k)) &&
      ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &abs) == 0) {
    return true;
  }
  return false;
}

void InputDevice::SendEvents(std::vector<TouchInputEventRecord>& events,
                             int index_first,
                             int index_last,
                             int offset_x,
                             int offset_y) {
  for (int i = index_first; i < index_last; i++) {
    TouchInputEventRecord& rec = events[i];
    int offset = 0;
    if (rec.code == ABS_MT_POSITION_X) {
      offset = offset_x;
    } else if (rec.code == ABS_MT_POSITION_Y) {
      offset = offset_y;
    }

    input_event event{.type = base::checked_cast<uint16_t>(rec.type),
                      .code = base::checked_cast<uint16_t>(rec.code),
                      .value = rec.value + offset};
    char* data = reinterpret_cast<char*>(&event);
    constexpr int kSize = static_cast<int>(sizeof(event));
    int bytes_written = 0;
    long rv;
    do {
      rv = HANDLE_EINTR(write(fd_.get(), data + bytes_written,
                              static_cast<size_t>(kSize - bytes_written)));
      if (rv <= 0) {
        PLOG(ERROR) << "Could not write: " << path_.MaybeAsASCII();
        abort();
      }
      bytes_written += rv;
    } while (bytes_written < kSize);
  }
}

void PrintUsage(char* prog) {
  std::cout << "Usage: " << prog
            << " record|replay FILE [--offset-x X] [--offset-y Y]" << std::endl
            << std::endl
            << "Record input events to FILE or replay them from FILE."
            << std::endl
            << std::endl
            << "Options:" << std::endl
            << "--offset-x X\t"
            << "the offset along the x-axis to be applied to replay events"
            << std::endl
            << "--offset-y Y\t"
            << "the offset along the y-axis to be applied to replay events"
            << std::endl;
}

enum class Command { kNone, kRecord, kReplay };

Command ParseCommand(char* arg) {
  if (!std::string("record").compare(arg)) {
    return Command::kRecord;
  } else if (!std::string("replay").compare(arg)) {
    return Command::kReplay;
  }
  return Command::kNone;
}

bool ParseOffset(const char* offset_type,
                 const char* offset_value,
                 std::optional<int>& offset_x,
                 std::optional<int>& offset_y) {
  int offset = 0;
  base::StringToInt(std::string(offset_value), &offset);
  if (!std::string("--offset-x").compare(offset_type) &&
      !offset_x.has_value()) {
    offset_x.emplace(offset);
  } else if (!std::string("--offset-y").compare(offset_type) &&
             !offset_y.has_value()) {
    offset_y.emplace(offset);
  } else {
    return false;
  }

  return true;
}

void PrintProgressMarkers(int i) {
  if (i != 0 && i % 10 == 0) {
    std::cout << " ";
    if (i % 100 == 0) {
      std::cout << std::endl;
    }
    if (i % 1000 == 0) {
      std::cout << std::endl;
    }
  }
  std::cout << ".";
}

constexpr char kLogFileHeader[] = "TouchEventsV0";
constexpr int kLogFileHeaderSize = static_cast<int>(sizeof(kLogFileHeader));

bool IsValidFileHeader(base::File& dump_file) {
  char magic_buf[kLogFileHeaderSize];
  if (!dump_file.ReadAtCurrentPosAndCheck(
          base::as_writable_byte_span(magic_buf))) {
    PLOG(ERROR) << "read magic";
    return false;
  }
  if (std::memcmp(magic_buf, kLogFileHeader, kLogFileHeaderSize) != 0) {
    return false;
  }
  return true;
}

bool ReadNullTerminatedString(base::File& f,
                              int offset,
                              std::string* contents) {
  if (!contents) {
    return false;
  }

  char cur;
  int64_t name_bytes_read = 0;
  do {
    if (!f.ReadAndCheck(offset, base::byte_span_from_ref(cur))) {
      PLOG(ERROR) << "read device name";
      return false;
    }
    contents->resize(++name_bytes_read, cur);
    offset++;
  } while (cur != '\0');
  return true;
}

void SleepMillis(uint64_t ms) {
  timespec sleep_time{
      .tv_sec = static_cast<time_t>(ms / 1000),
      .tv_nsec = static_cast<time_t>((ms % 1000) * 1000 * 1000)};
  timespec remaining{};
  while (nanosleep(&sleep_time, &remaining) == -1 && errno == EINTR) {
    sleep_time = remaining;
  }
}

void SleepUntil(uint64_t record_time, int64_t timebase_offset_ms) {
  int64_t current_time =
      base::TimeTicks::Now().ToUptimeMillis() - timebase_offset_ms;
  int64_t sleep_millis = record_time - current_time;
  if (sleep_millis > 0) {
    SleepMillis(sleep_millis);
  }
}

bool RecordForever(const base::FilePath& file_path) {
  // Open the dump file for writing.
  base::File dump_file(file_path,
                       base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!dump_file.IsValid()) {
    PLOG(ERROR) << "Could not open for writing: " << file_path.MaybeAsASCII();
    return false;
  }

  // Write the magic.
  if (!dump_file.WriteAtCurrentPosAndCheck(
          base::byte_span_with_nul_from_cstring(kLogFileHeader))) {
    LOG(ERROR) << "Could not write magic";
  }

  // Find the device with the ability to send ABS_MT events.
  std::unique_ptr<InputDevice> device;
  base::FileEnumerator e(base::FilePath(kInputDeviceDirPath),
                         /* recursive= */ false, base::FileEnumerator::FILES);
  for (base::FilePath device_path = e.Next(); !device_path.empty();
       device_path = e.Next()) {
    std::unique_ptr<InputDevice> d =
        std::make_unique<InputDevice>(device_path, /*read_only=*/true);
    if (!d->is_valid()) {
      continue;
    }
    if (d->HasTouch()) {
      device = std::move(d);
      break;
    }
  }
  if (!device) {
    LOG(ERROR) << "Device with touch events not found";
    return false;
  }

  // Write the device file name to the dump (inc. terminating NUL).
  if (!dump_file.WriteAtCurrentPosAndCheck(
          base::byte_span_with_nul_from_cstring_view(
              base::cstring_view(device->path().MaybeAsASCII())))) {
    LOG(ERROR) << "Could not write device name";
    return false;
  }

  // Capture events from the device.
  pollfd pollfd{.fd = device->fd(), .events = POLLIN};
  for (int i = 0;; i++) {
    poll(&pollfd, 1, -1);
    if (pollfd.revents & POLLIN) {
      input_event event;
      constexpr size_t kEventSize = sizeof(event);
      int result = read(pollfd.fd, &event, kEventSize);
      if (result < static_cast<int>(kEventSize)) {
        LOG(ERROR) << "Short read on event";
        return false;
      }
      TouchInputEventRecord record{.sec = event.time.tv_sec,
                                   .usec = event.time.tv_usec,
                                   .type = event.type,
                                   .code = event.code,
                                   .value = event.value};
      if (!dump_file.WriteAtCurrentPosAndCheck(
              base::byte_span_from_ref(record))) {
        LOG(ERROR) << "Failed to write record";
        return false;
      }
      PrintProgressMarkers(i);
    }
  }
}

void ValidateRecords(const std::vector<TouchInputEventRecord>& records) {
  // Ensure records are monotonically non-decreasing.
  uint64_t last_ms = 0;
  for (const TouchInputEventRecord& record : records) {
    uint64_t current_ms = record.InMilliseconds();
    if (current_ms < last_ms) {
      LOG(ERROR) << "Log timestamps are required to be "
                 << "monotonically non-decreasing";
      abort();
    }
    last_ms = current_ms;
  }
}

bool Replay(const base::FilePath& file_path, int offset_x, int offset_y) {
  // Open the dump file for reading.
  base::File dump_file(file_path,
                       base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!dump_file.IsValid()) {
    PLOG(ERROR) << "Could not open for reading: " << file_path.MaybeAsASCII();
    return false;
  }

  // Verify the magic.
  if (!IsValidFileHeader(dump_file)) {
    LOG(ERROR) << "wrong magic number in the file";
    return false;
  }

  // Read the device name.
  std::string device_name;
  if (!ReadNullTerminatedString(dump_file, kLogFileHeaderSize, &device_name)) {
    return false;
  }

  // Read the events.
  const int64_t offset = kLogFileHeaderSize + device_name.size();
  const int64_t file_size = dump_file.GetLength();
  const int64_t data_size = file_size - offset;
  const int64_t num_records = data_size / sizeof(TouchInputEventRecord);
  if (num_records == 0) {
    LOG(ERROR) << "No events to send";
    return false;
  }
  std::vector<TouchInputEventRecord> records(num_records);
  if (!dump_file.ReadAndCheck(offset, base::as_writable_byte_span(records))) {
    PLOG(ERROR) << "Could not read records";
    return false;
  }
  ValidateRecords(records);

  // Open the device.
  base::FilePath device_path(device_name);
  std::unique_ptr<InputDevice> device =
      std::make_unique<InputDevice>(device_path, /*read_only=*/false);
  if (!device->is_valid()) {
    // The low level errors should have been printed.
    return false;
  }

  // Send the events to the device in chunks.
  //
  // Chunk size was chosen to be 1 millisecond because in local experiments
  // emulating touch events often got delayed by an order of 1ms on modern
  // Android devices. Therefore, emulating with precision higher than 1ms
  // would be nontrivial.
  int chunk_last = 0;
  // Offset between our two timebases.
  int64_t offset_ms =
      base::TimeTicks::Now().ToUptimeMillis() - records[0].InMilliseconds();
  while (chunk_last < num_records) {
    SleepUntil(records[chunk_last].InMilliseconds(), offset_ms);
    uint64_t chunk_end_ms = base::TimeTicks::Now().ToUptimeMillis() - offset_ms;

    int chunk_first = chunk_last;
    // Arrange the chunk. Clamping to millisecond intervals should be OK
    // because of the overall 1ms precision.
    while (chunk_last < num_records &&
           records[chunk_last].InMilliseconds() <= chunk_end_ms) {
      chunk_last++;
    }

    // Send all events from the chunk to the device without delays in between.
    device->SendEvents(records, chunk_first, chunk_last, offset_x, offset_y);

    // Print progress after sending the chunk of events to reduce
    // delays between the events within each chunk.
    for (int j = chunk_first; j < chunk_last; j++) {
      PrintProgressMarkers(j);
    }
  }
  std::cout << std::endl;
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (!(argc == 3 || argc == 5 || argc == 7)) {
    PrintUsage(argv[0]);
    return 1;
  }
  Command command = ParseCommand(argv[1]);
  if (command == Command::kNone) {
    PrintUsage(argv[0]);
    return 1;
  }

  // Set stdout to unbuffered to visualize events as they are captured or sent.
  std::cout << std::unitbuf;

  base::FilePath file_path(argv[2]);
  if (command == Command::kRecord) {
    if (!RecordForever(file_path)) {
      return 1;
    }
  } else if (command == Command::kReplay) {
    std::optional<int> offset_x;
    std::optional<int> offset_y;
    for (int i = 3; i < argc; i += 2) {
      // Expect the next arguments to be of the form: --offset-[x|y] OFFSET
      if (!ParseOffset(argv[i], argv[i + 1], offset_x, offset_y)) {
        PrintUsage(argv[0]);
        return 1;
      }
    }

    if (!Replay(file_path, offset_x.value_or(0), offset_y.value_or(0))) {
      return 1;
    }
  }
  return 0;
}
