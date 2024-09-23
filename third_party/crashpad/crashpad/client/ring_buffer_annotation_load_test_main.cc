// Copyright 2023 The Crashpad Authors
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

#include <getopt.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <random>
#include <ratio>
#include <string>
#include <string_view>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "client/annotation.h"
#include "client/length_delimited_ring_buffer.h"
#include "client/ring_buffer_annotation.h"
#include "tools/tool_support.h"
#include "util/stdlib/string_number_conversion.h"
#include "util/synchronization/scoped_spin_guard.h"
#include "util/thread/thread.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#else
#include <signal.h>
#endif  // BUILDFLAG(IS_WIN)

namespace crashpad {

namespace test {

namespace {

constexpr Annotation::Type kRingBufferLoadTestType =
    Annotation::UserDefinedType(0x0042);
std::atomic<bool> g_should_exit = false;

struct RingBufferAnnotationSnapshotParams final {
  enum class Mode {
    kUseScopedSpinGuard = 1,
    kDoNotUseSpinGuard = 2,
  };
  Mode mode = Mode::kUseScopedSpinGuard;
  using Duration = std::chrono::duration<uint64_t, std::nano>;
  Duration producer_thread_min_run_duration = std::chrono::milliseconds(1);
  Duration producer_thread_max_run_duration = std::chrono::milliseconds(10);
  Duration producer_thread_sleep_duration = std::chrono::nanoseconds(10);
  Duration consumer_thread_min_run_duration = std::chrono::milliseconds(5);
  Duration consumer_thread_max_run_duration = std::chrono::milliseconds(100);
  Duration quiesce_timeout = std::chrono::microseconds(500);
  uint64_t num_loops = std::numeric_limits<uint64_t>::max();
  std::optional<Duration> main_thread_run_duration = std::nullopt;
};

template <uint32_t RingBufferCapacity>
class RingBufferAnnotationSnapshot final {
  using RingBufferAnnotationType = RingBufferAnnotation<RingBufferCapacity>;

  struct State final {
    State()
        : ring_buffer_annotation(kRingBufferLoadTestType,
                                 "ring-buffer-load-test"),
          ring_buffer_ready(false),
          producer_thread_running(false),
          producer_thread_finished(false),
          consumer_thread_finished(false),
          should_exit(false) {}

    State(const State&) = delete;
    State& operator=(const State&) = delete;

    RingBufferAnnotationType ring_buffer_annotation;
    bool ring_buffer_ready;
    bool producer_thread_running;
    bool producer_thread_finished;
    bool consumer_thread_finished;
    bool should_exit;
  };

  class Thread final : public crashpad::Thread {
   public:
    Thread(std::function<void()> thread_main)
        : thread_main_(std::move(thread_main)) {}

   private:
    void ThreadMain() override { thread_main_(); }

    const std::function<void()> thread_main_;
  };

 public:
  RingBufferAnnotationSnapshot(const RingBufferAnnotationSnapshotParams& params)
      : params_(params),
        main_loop_thread_([this]() { MainLoopThreadMain(); }),
        producer_thread_([this]() { ProducerThreadMain(); }),
        consumer_thread_([this]() { ConsumerThreadMain(); }),
        mutex_(),
        state_changed_condition_(),
        state_() {}

  RingBufferAnnotationSnapshot(const RingBufferAnnotationSnapshot&) = delete;
  RingBufferAnnotationSnapshot& operator=(const RingBufferAnnotationSnapshot&) =
      delete;

  void Start() {
    main_loop_thread_.Start();
    producer_thread_.Start();
    consumer_thread_.Start();
  }

  void Stop() {
    consumer_thread_.Join();
    producer_thread_.Join();
    main_loop_thread_.Join();
  }

 private:
  void MainLoopThreadMain() {
    std::chrono::steady_clock::time_point main_thread_end_time;
    if (params_.main_thread_run_duration) {
      main_thread_end_time =
          std::chrono::steady_clock::now() + *params_.main_thread_run_duration;
    } else {
      main_thread_end_time = std::chrono::steady_clock::time_point::max();
    }
    for (uint64_t i = 0;
         i < params_.num_loops &&
         std::chrono::steady_clock::now() < main_thread_end_time;
         i++) {
      {
        std::unique_lock<std::mutex> start_lock(mutex_);
        state_.ring_buffer_annotation.ResetForTesting();
        state_.ring_buffer_ready = true;
        state_changed_condition_.notify_all();
      }

      {
        std::unique_lock<std::mutex> lock(mutex_);
        state_changed_condition_.wait(lock, [this] {
          return state_.producer_thread_finished &&
                 state_.consumer_thread_finished;
        });
        state_.ring_buffer_ready = false;
        if (g_should_exit) {
          printf("Exiting on Control-C.\n");
          break;
        }
        printf(".");
        fflush(stdout);
        state_changed_condition_.notify_all();
      }
    }
    state_.should_exit = true;
    state_changed_condition_.notify_all();
  }

  void ProducerThreadMain() {
    while (true) {
      {
        std::unique_lock<std::mutex> lock(mutex_);
        state_changed_condition_.wait(lock, [this] {
          return state_.should_exit || state_.ring_buffer_ready;
        });
        if (state_.should_exit) {
          return;
        }
        state_.producer_thread_running = true;
        state_.producer_thread_finished = false;
        state_changed_condition_.notify_all();
      }

      auto min_run_duration_micros =
          std::chrono::duration_cast<std::chrono::microseconds>(
              params_.producer_thread_min_run_duration);
      auto max_run_duration_micros =
          std::chrono::duration_cast<std::chrono::microseconds>(
              params_.producer_thread_max_run_duration);
      std::uniform_int_distribution<std::chrono::microseconds::rep>
          run_duration_distribution(min_run_duration_micros.count(),
                                    max_run_duration_micros.count());
      static thread_local std::mt19937 random_number_generator;
      auto run_duration = std::chrono::microseconds(
          run_duration_distribution(random_number_generator));
      auto end_time = std::chrono::steady_clock::now() + run_duration;
      uint64_t next_value = 0;
      while (std::chrono::steady_clock::now() < end_time) {
        if (!Produce(next_value++)) {
          // The consumer thread interrupted this.
          break;
        }
      }
      {
        std::unique_lock<std::mutex> lock(mutex_);
        state_changed_condition_.wait(
            lock, [this] { return state_.consumer_thread_finished; });
        state_.producer_thread_running = false;
        state_.producer_thread_finished = true;
        state_changed_condition_.notify_all();
      }
    }
  }

  bool Produce(uint64_t value) {
    std::string hex_value = base::StringPrintf("0x%08" PRIx64, value);
    if (!state_.ring_buffer_annotation.Push(
            hex_value.data(), static_cast<uint32_t>(hex_value.size()))) {
      fprintf(stderr,
              "Ignoring failed call to Push(0x%" PRIx64
              ") (ScopedSpinGuard was held by snapshot thread)\n",
              value);
      return false;
    }
    return true;
  }

  void ConsumerThreadMain() {
    while (true) {
      {
        std::unique_lock<std::mutex> lock(mutex_);
        state_changed_condition_.wait(lock, [this] {
          return state_.should_exit ||
                 (state_.ring_buffer_ready && state_.producer_thread_running);
        });
        if (state_.should_exit) {
          return;
        }
        state_.consumer_thread_finished = false;
        state_changed_condition_.notify_all();
      }
      auto min_run_duration_micros =
          std::chrono::duration_cast<std::chrono::microseconds>(
              params_.consumer_thread_min_run_duration);
      auto max_run_duration_micros =
          std::chrono::duration_cast<std::chrono::microseconds>(
              params_.consumer_thread_max_run_duration);
      std::uniform_int_distribution<std::chrono::microseconds::rep>
          run_duration_distribution(min_run_duration_micros.count(),
                                    max_run_duration_micros.count());
      static thread_local std::mt19937 random_number_generator;
      auto run_duration = std::chrono::microseconds(
          run_duration_distribution(random_number_generator));
      auto end_time = std::chrono::steady_clock::now() + run_duration;
      while (std::chrono::steady_clock::now() < end_time) {
        constexpr uint64_t kSleepTimeNs = 10000;  // 10 us
        SleepNanoseconds(kSleepTimeNs);
      }
      Snapshot();
      {
        std::unique_lock<std::mutex> lock(mutex_);
        state_.consumer_thread_finished = true;
        state_.ring_buffer_ready = false;
        state_changed_condition_.notify_all();
      }
    }
  }

  void Snapshot() {
    int64_t timeout_ns = static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            params_.quiesce_timeout)
            .count());
    uint8_t serialized_ring_buffer[sizeof(state_.ring_buffer_annotation)];
    Annotation::ValueSizeType ring_buffer_size;
    {
      std::optional<ScopedSpinGuard> scoped_spin_guard;
      if (params_.mode ==
          RingBufferAnnotationSnapshotParams::Mode::kUseScopedSpinGuard) {
        scoped_spin_guard =
            state_.ring_buffer_annotation.TryCreateScopedSpinGuard(timeout_ns);
      }
      if (params_.mode ==
              RingBufferAnnotationSnapshotParams::Mode::kUseScopedSpinGuard &&
          !scoped_spin_guard) {
        fprintf(stderr,
                "Could not quiesce writes within %" PRIi64 " ns\n",
                timeout_ns);
        abort();
      }
      ring_buffer_size = state_.ring_buffer_annotation.size();
      memcpy(&serialized_ring_buffer[0],
             state_.ring_buffer_annotation.value(),
             ring_buffer_size);
    }
    RingBufferData ring_buffer;
    if (!ring_buffer.DeserializeFromBuffer(serialized_ring_buffer,
                                           ring_buffer_size)) {
      fprintf(stderr, "Could not deserialize ring buffer\n");
      abort();
    }
    LengthDelimitedRingBufferReader ring_buffer_reader(ring_buffer);
    int value = std::numeric_limits<int>::max();
    std::vector<uint8_t> bytes;
    while (ring_buffer_reader.Pop(bytes)) {
      int next_value;
      std::string_view str = base::as_string_view(bytes);
      if (!base::HexStringToInt(str, &next_value)) {
        fprintf(stderr,
                "Couldn't parse value: [%.*s]\n",
                base::checked_cast<int>(bytes.size()),
                bytes.data());
        abort();
      }
      if (value == std::numeric_limits<int>::max()) {
        // First value in buffer.
      } else if (value + 1 != next_value) {
        fprintf(stderr,
                "Expected value 0x%08x, got 0x%08x\n",
                value + 1,
                next_value);
        abort();
      }
      value = next_value;
      bytes.clear();
    }
  }

  const RingBufferAnnotationSnapshotParams params_;
  Thread main_loop_thread_;
  Thread producer_thread_;
  Thread consumer_thread_;
  std::mutex mutex_;

  // Fired whenever `state_` changes.
  std::condition_variable state_changed_condition_;

  // Protected by `mutex_`.
  State state_;
};

void Usage(const base::FilePath& me) {
  // clang-format off
  fprintf(stderr,
"Usage: %" PRFilePath " [OPTION]...\n"
"Runs a load test for concurrent I/O to RingBufferAnnotation.\n"
"\n"
"By default, enables the annotation spin guard and runs indefinitely\n"
"until interrupted (e.g., with Control-C or SIGINT).\n"
"\n"
"  -d,--disable-spin-guard  Disables the annotation spin guard\n"
"                           (the test is expected to crash in this case)\n"
"  -n,--num-loops=N         Runs the test for N iterations, not indefinitely\n"
"  -s,--duration-secs=SECS  Runs the test for SECS seconds, not indefinitely\n",
          me.value().c_str());
  // clang-format on
  ToolSupport::UsageTail(me);
}

int TestMain(int argc, char** argv) {
  const base::FilePath argv0(
      ToolSupport::CommandLineArgumentToFilePathStringType(argv[0]));
  const base::FilePath me(argv0.BaseName());

#if BUILDFLAG(IS_WIN)
  auto handler_routine = [](DWORD type) -> BOOL {
    if (type == CTRL_C_EVENT) {
      g_should_exit = true;
      return TRUE;
    }
    return FALSE;
  };
  if (!SetConsoleCtrlHandler(handler_routine, /*Add=*/TRUE)) {
    fprintf(stderr, "Couldn't set Control-C handler\n");
    return EXIT_FAILURE;
  }
#else
  signal(SIGINT, [](int signal) { g_should_exit = true; });
#endif  // BUILDFLAG(IS_WIN)
  RingBufferAnnotationSnapshotParams params;
  enum OptionFlags {
    // "Short" (single-character) options.
    kOptionDisableSpinGuard = 'd',
    kOptionNumLoops = 'n',
    kOptionDurationSecs = 's',

    // Standard options.
    kOptionHelp = -2,
    kOptionVersion = -3,
  };
  static constexpr option long_options[] = {
      {"disable-spin-guard", no_argument, nullptr, kOptionDisableSpinGuard},
      {"num-loops", required_argument, nullptr, kOptionNumLoops},
      {"duration-secs", required_argument, nullptr, kOptionDurationSecs},
      {"help", no_argument, nullptr, kOptionHelp},
      {"version", no_argument, nullptr, kOptionVersion},
      {nullptr, 0, nullptr, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "dn:s:", long_options, nullptr)) !=
         -1) {
    switch (opt) {
      case kOptionDisableSpinGuard:
        printf("Disabling spin guard logic (this test will fail!)\n");
        params.mode =
            RingBufferAnnotationSnapshotParams::Mode::kDoNotUseSpinGuard;
        break;
      case kOptionNumLoops: {
        std::string num_loops(optarg);
        uint64_t num_loops_value;
        if (!StringToNumber(num_loops, &num_loops_value)) {
          ToolSupport::UsageHint(me, "--num-loops requires integer value");
          return EXIT_FAILURE;
        }
        params.num_loops = num_loops_value;
        break;
      }
      case kOptionDurationSecs: {
        std::string duration_secs(optarg);
        uint64_t duration_secs_value;
        if (!StringToNumber(duration_secs, &duration_secs_value)) {
          ToolSupport::UsageHint(me, "--duration-secs requires integer value");
          return EXIT_FAILURE;
        }
        params.main_thread_run_duration =
            std::chrono::seconds(duration_secs_value);
        break;
      }
      case kOptionHelp:
        Usage(me);
        return EXIT_SUCCESS;
      case kOptionVersion:
        ToolSupport::Version(me);
        return EXIT_SUCCESS;
      default:
        ToolSupport::UsageHint(me, nullptr);
        return EXIT_FAILURE;
    }
  }

  RingBufferAnnotationSnapshot<8192> test_producer_snapshot(params);
  printf("Starting test (Control-C to exit)...\n");
  test_producer_snapshot.Start();
  test_producer_snapshot.Stop();
  printf("Test finished.\n");
  return EXIT_SUCCESS;
}

}  // namespace
}  // namespace test
}  // namespace crashpad

#if BUILDFLAG(IS_POSIX)

int main(int argc, char** argv) {
  return crashpad::test::TestMain(argc, argv);
}

#elif BUILDFLAG(IS_WIN)

int wmain(int argc, wchar_t* argv[]) {
  return crashpad::ToolSupport::Wmain(argc, argv, crashpad::test::TestMain);
}

#endif
