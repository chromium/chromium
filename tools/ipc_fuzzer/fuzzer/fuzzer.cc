// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/ipc_fuzzer/fuzzer/fuzzer.h"

#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/types/id_type.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/common/gpu_param_traits_macros.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message.h"
#include "media/gpu/ipc/common/media_param_traits.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/mojom/print.mojom-shared.h"
#include "services/device/public/mojom/screen_orientation_lock_types.mojom-shared.h"
#include "third_party/blink/public/common/page_state/page_state.h"
#include "third_party/blink/public/mojom/widget/device_emulation_params.mojom-shared.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "tools/ipc_fuzzer/fuzzer/rand_util.h"
#include "tools/ipc_fuzzer/message_lib/message_cracker.h"
#include "tools/ipc_fuzzer/message_lib/message_file.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/range/range.h"
#include "ui/gl/gpu_preference.h"
#include "ui/latency/latency_info.h"

#if BUILDFLAG(IS_POSIX)
#include <unistd.h>
#endif

// First include of all message files to provide basic types.
#include "tools/ipc_fuzzer/message_lib/all_messages.h"
#include "tools/ipc_fuzzer/message_lib/all_message_null_macros.h"

namespace IPC {
class Message;
}  // namespace IPC

namespace {
// For breaking deep recursion.
int g_depth = 0;
}  // namespace

namespace ipc_fuzzer {

FuzzerFunctionVector g_function_vector;

bool Fuzzer::ShouldGenerate() {
  return false;
}

// Partially-specialized class that knows how to handle a given type.
template <class P>
struct FuzzTraits {
  static bool Fuzz(P* p, Fuzzer *fuzzer) {
    // This is the catch-all for types we don't have enough information
    // to generate.
    std::cerr << "Can't handle " << PRETTY_FUNCTION << "\n";
    return false;
  }
};

// Template function to invoke partially-specialized class method.
template <class P>
static bool FuzzParam(P* p, Fuzzer* fuzzer) {
  return FuzzTraits<P>::Fuzz(p, fuzzer);
}

template <class P>
static bool FuzzParamArray(P* p, size_t length, Fuzzer* fuzzer) {
  for (size_t i = 0; i < length; i++, p++) {
    if (!FuzzTraits<P>::Fuzz(p, fuzzer))
      return false;
  }
  return true;
}

// Specializations to generate primitive types.
template <>
struct FuzzTraits<bool> {
  static bool Fuzz(bool* p, Fuzzer* fuzzer) {
    fuzzer->FuzzBool(p);
    return true;
  }
};

template <>
struct FuzzTraits<int> {
  static bool Fuzz(int* p, Fuzzer* fuzzer) {
    fuzzer->FuzzInt(p);
    return true;
  }
};

template <>
struct FuzzTraits<unsigned int> {
  static bool Fuzz(unsigned int* p, Fuzzer* fuzzer) {
    fuzzer->FuzzInt(reinterpret_cast<int*>(p));
    return true;
  }
};

template <>
struct FuzzTraits<long> {
  static bool Fuzz(long* p, Fuzzer* fuzzer) {
    fuzzer->FuzzLong(p);
    return true;
  }
};

template <>
struct FuzzTraits<unsigned long> {
  static bool Fuzz(unsigned long* p, Fuzzer* fuzzer) {
    fuzzer->FuzzLong(reinterpret_cast<long*>(p));
    return true;
  }
};

template <>
struct FuzzTraits<long long> {
  static bool Fuzz(long long* p, Fuzzer* fuzzer) {
    fuzzer->FuzzInt64(reinterpret_cast<int64_t*>(p));
    return true;
  }
};

template <>
struct FuzzTraits<unsigned long long> {
  static bool Fuzz(unsigned long long* p, Fuzzer* fuzzer) {
    fuzzer->FuzzInt64(reinterpret_cast<int64_t*>(p));
    return true;
  }
};

template <>
struct FuzzTraits<short> {
  static bool Fuzz(short* p, Fuzzer* fuzzer) {
    fuzzer->FuzzUInt16(reinterpret_cast<uint16_t*>(p));
    return true;
  }
};

template <>
struct FuzzTraits<unsigned short> {
  static bool Fuzz(unsigned short* p, Fuzzer* fuzzer) {
    fuzzer->FuzzUInt16(reinterpret_cast<uint16_t*>(p));
    return true;
  }
};

template <>
struct FuzzTraits<char> {
  static bool Fuzz(char* p, Fuzzer* fuzzer) {
    fuzzer->FuzzUChar(reinterpret_cast<unsigned char*>(p));
    return true;
  }
};

template <>
struct FuzzTraits<signed char> {
  static bool Fuzz(signed char* p, Fuzzer* fuzzer) {
    fuzzer->FuzzUChar(reinterpret_cast<unsigned char*>(p));
    return true;
  }
};

template <>
struct FuzzTraits<unsigned char> {
  static bool Fuzz(unsigned char* p, Fuzzer* fuzzer) {
    fuzzer->FuzzUChar(p);
    return true;
  }
};

template <>
struct FuzzTraits<wchar_t> {
  static bool Fuzz(wchar_t* p, Fuzzer* fuzzer) {
    fuzzer->FuzzWChar(p);
    return true;
  }
};

template <>
struct FuzzTraits<float> {
  static bool Fuzz(float* p, Fuzzer* fuzzer) {
    fuzzer->FuzzFloat(p);
    return true;
  }
};

template <>
struct FuzzTraits<double> {
  static bool Fuzz(double* p, Fuzzer* fuzzer) {
    fuzzer->FuzzDouble(p);
    return true;
  }
};

template <>
struct FuzzTraits<std::string> {
  static bool Fuzz(std::string* p, Fuzzer* fuzzer) {
    fuzzer->FuzzString(p);
    return true;
  }
};

template <>
struct FuzzTraits<std::u16string> {
  static bool Fuzz(std::u16string* p, Fuzzer* fuzzer) {
    fuzzer->FuzzString16(p);
    return true;
  }
};

// Specializations for tuples.
template <>
struct FuzzTraits<std::tuple<>> {
  static bool Fuzz(std::tuple<>* p, Fuzzer* fuzzer) { return true; }
};

template <class A>
struct FuzzTraits<std::tuple<A>> {
  static bool Fuzz(std::tuple<A>* p, Fuzzer* fuzzer) {
    return FuzzParam(&std::get<0>(*p), fuzzer);
  }
};

template <class A, class B>
struct FuzzTraits<std::tuple<A, B>> {
  static bool Fuzz(std::tuple<A, B>* p, Fuzzer* fuzzer) {
    return FuzzParam(&std::get<0>(*p), fuzzer) &&
           FuzzParam(&std::get<1>(*p), fuzzer);
  }
};

template <class A, class B, class C>
struct FuzzTraits<std::tuple<A, B, C>> {
  static bool Fuzz(std::tuple<A, B, C>* p, Fuzzer* fuzzer) {
    return FuzzParam(&std::get<0>(*p), fuzzer) &&
           FuzzParam(&std::get<1>(*p), fuzzer) &&
           FuzzParam(&std::get<2>(*p), fuzzer);
  }
};

template <class A, class B, class C, class D>
struct FuzzTraits<std::tuple<A, B, C, D>> {
  static bool Fuzz(std::tuple<A, B, C, D>* p, Fuzzer* fuzzer) {
    return FuzzParam(&std::get<0>(*p), fuzzer) &&
           FuzzParam(&std::get<1>(*p), fuzzer) &&
           FuzzParam(&std::get<2>(*p), fuzzer) &&
           FuzzParam(&std::get<3>(*p), fuzzer);
  }
};

template <class A, class B, class C, class D, class E>
struct FuzzTraits<std::tuple<A, B, C, D, E>> {
  static bool Fuzz(std::tuple<A, B, C, D, E>* p, Fuzzer* fuzzer) {
    return FuzzParam(&std::get<0>(*p), fuzzer) &&
           FuzzParam(&std::get<1>(*p), fuzzer) &&
           FuzzParam(&std::get<2>(*p), fuzzer) &&
           FuzzParam(&std::get<3>(*p), fuzzer) &&
           FuzzParam(&std::get<4>(*p), fuzzer);
  }
};

// Specializations for containers.
template <class A>
struct FuzzTraits<std::vector<A> > {
  static bool Fuzz(std::vector<A>* p, Fuzzer* fuzzer) {
    ++g_depth;
    size_t count = p->size();
    if (fuzzer->ShouldGenerate()) {
      count = g_depth > 3 ? 0 : RandElementCount();
      p->resize(count);
    }
    for (size_t i = 0; i < count; ++i) {
      if (!FuzzParam(&p->at(i), fuzzer)) {
        --g_depth;
        return false;
      }
    }
    --g_depth;
    return true;
  }
};

template <class A>
struct FuzzTraits<std::set<A> > {
  static bool Fuzz(std::set<A>* p, Fuzzer* fuzzer) {
    if (!fuzzer->ShouldGenerate()) {
      std::set<A> result;
      typename std::set<A>::iterator it;
      for (it = p->begin(); it != p->end(); ++it) {
        A item = *it;
        if (!FuzzParam(&item, fuzzer))
          return false;
        result.insert(item);
      }
      *p = result;
      return true;
    }

    static int g_depth = 0;
    size_t count = ++g_depth > 3 ? 0 : RandElementCount();
    A a;
    for (size_t i = 0; i < count; ++i) {
      if (!FuzzParam(&a, fuzzer)) {
        --g_depth;
        return false;
      }
      p->insert(a);
    }
    --g_depth;
    return true;
  }
};

template <class A, class B>
struct FuzzTraits<std::map<A, B> > {
  static bool Fuzz(std::map<A, B>* p, Fuzzer* fuzzer) {
    if (!fuzzer->ShouldGenerate()) {
      typename std::map<A, B>::iterator it;
      for (it = p->begin(); it != p->end(); ++it) {
        if (!FuzzParam(&it->second, fuzzer))
          return false;
      }
      return true;
    }

    static int g_depth = 0;
    size_t count = ++g_depth > 3 ? 0 : RandElementCount();
    std::pair<A, B> place_holder;
    for (size_t i = 0; i < count; ++i) {
      if (!FuzzParam(&place_holder, fuzzer)) {
        --g_depth;
        return false;
      }
      p->insert(place_holder);
    }
    --g_depth;
    return true;
  }
};

template <class A, class B, class C, class D>
struct FuzzTraits<std::map<A, B, C, D>> {
  static bool Fuzz(std::map<A, B, C, D>* p, Fuzzer* fuzzer) {
    if (!fuzzer->ShouldGenerate()) {
      typename std::map<A, B, C, D>::iterator it;
      for (it = p->begin(); it != p->end(); ++it) {
        if (!FuzzParam(&it->second, fuzzer))
          return false;
      }
      return true;
    }

    static int g_depth = 0;
    size_t count = ++g_depth > 3 ? 0 : RandElementCount();
    std::pair<A, B> place_holder;
    for (size_t i = 0; i < count; ++i) {
      if (!FuzzParam(&place_holder, fuzzer)) {
        --g_depth;
        return false;
      }
      p->insert(place_holder);
    }
    --g_depth;
    return true;
  }
};

template <class A, class B>
struct FuzzTraits<std::pair<A, B> > {
  static bool Fuzz(std::pair<A, B>* p, Fuzzer* fuzzer) {
    return
        FuzzParam(&p->first, fuzzer) &&
        FuzzParam(&p->second, fuzzer);
  }
};

// Specializations for hand-coded types.

template <>
struct FuzzTraits<base::FilePath> {
  static bool Fuzz(base::FilePath* p, Fuzzer* fuzzer) {
    if (!fuzzer->ShouldGenerate()) {
      base::FilePath::StringType path = p->value();
      if(!FuzzParam(&path, fuzzer))
        return false;
      *p = base::FilePath(path);
      return true;
    }

    const char path_chars[] = "ACz0/.~:";
    size_t count = RandInRange(60);
    base::FilePath::StringType random_path;
    for (size_t i = 0; i < count; ++i)
      random_path += path_chars[RandInRange(sizeof(path_chars) - 1)];
    *p = base::FilePath(random_path);
    return true;
  }
};

template <>
struct FuzzTraits<base::File::Error> {
  static bool Fuzz(base::File::Error* p, Fuzzer* fuzzer) {
    int value = static_cast<int>(*p);
    if (!FuzzParam(&value, fuzzer))
      return false;
    *p = static_cast<base::File::Error>(value);
    return true;
  }
};

template <>
struct FuzzTraits<base::File::Info> {
  static bool Fuzz(base::File::Info* p, Fuzzer* fuzzer) {
    double last_modified = p->last_modified.InSecondsFSinceUnixEpoch();
    double last_accessed = p->last_accessed.InSecondsFSinceUnixEpoch();
    double creation_time = p->creation_time.InSecondsFSinceUnixEpoch();
    if (!FuzzParam(&p->size, fuzzer))
      return false;
    if (!FuzzParam(&p->is_directory, fuzzer))
      return false;
    if (!FuzzParam(&last_modified, fuzzer))
      return false;
    if (!FuzzParam(&last_accessed, fuzzer))
      return false;
    if (!FuzzParam(&creation_time, fuzzer))
      return false;
    p->last_modified = base::Time::FromSecondsSinceUnixEpoch(last_modified);
    p->last_accessed = base::Time::FromSecondsSinceUnixEpoch(last_accessed);
    p->creation_time = base::Time::FromSecondsSinceUnixEpoch(creation_time);
    return true;
  }
};

template <>
struct FuzzTraits<base::Time> {
  static bool Fuzz(base::Time* p, Fuzzer* fuzzer) {
    int64_t internal_value = p->ToInternalValue();
    if (!FuzzParam(&internal_value, fuzzer))
      return false;
    *p = base::Time::FromInternalValue(internal_value);
    return true;
  }
};

template <>
struct FuzzTraits<base::TimeDelta> {
  static bool Fuzz(base::TimeDelta* p, Fuzzer* fuzzer) {
    int64_t internal_value = p->ToInternalValue();
    if (!FuzzParam(&internal_value, fuzzer))
      return false;
    *p = base::TimeDelta::FromInternalValue(internal_value);
    return true;
  }
};

template <>
struct FuzzTraits<base::TimeTicks> {
  static bool Fuzz(base::TimeTicks* p, Fuzzer* fuzzer) {
    int64_t internal_value = p->ToInternalValue();
    if (!FuzzParam(&internal_value, fuzzer))
      return false;
    *p = base::TimeTicks::FromInternalValue(internal_value);
    return true;
  }
};

template <>
struct FuzzTraits<base::Value> {
  static bool Fuzz(base::Value* p, Fuzzer* fuzzer) {
    DCHECK(p->type() == base::Value::Type::LIST ||
           p->type() == base::Value::Type::DICT);

    // TODO(mbarbella): Support mutation.
    if (!fuzzer->ShouldGenerate())
      return true;

    if (g_depth > 2)
      return true;

    g_depth++;

    const size_t kMaxSize = 8;
    size_t random_size = RandInRange(kMaxSize);
    for (size_t i = 0; i < random_size; i++) {
      base::Value random_value;
      const size_t kNumValueTypes = 8;
      switch (static_cast<base::Value::Type>(RandInRange(kNumValueTypes))) {
        case base::Value::Type::BOOLEAN: {
          bool tmp;
          fuzzer->FuzzBool(&tmp);
          random_value = base::Value(tmp);
          break;
        }
        case base::Value::Type::INTEGER: {
          int tmp;
          fuzzer->FuzzInt(&tmp);
          random_value = base::Value(tmp);
          break;
        }
        case base::Value::Type::DOUBLE: {
          double tmp;
          fuzzer->FuzzDouble(&tmp);
          random_value = base::Value(tmp);
          break;
        }
        case base::Value::Type::BINARY: {
          char tmp[200];
          size_t bin_length = RandInRange(sizeof(tmp));
          fuzzer->FuzzData(tmp, bin_length);
          random_value =
              base::Value(base::as_bytes(base::make_span(tmp, bin_length)));
          break;
        }
        case base::Value::Type::STRING: {
          random_value = base::Value(base::Value::Type::STRING);
          fuzzer->FuzzString(&random_value.GetString());
          break;
        }
        case base::Value::Type::DICT: {
          random_value = base::Value(base::Value::Type::DICT);
          FuzzParam(&random_value, fuzzer);
          break;
        }
        case base::Value::Type::LIST: {
          random_value = base::Value(base::Value::Type::LIST);
          FuzzParam(&random_value, fuzzer);
          break;
        }
        case base::Value::Type::NONE:
          // |random_value| already has type NONE, nothing to do.
          break;
      }

      // Add |random_value| to the container.
      if (p->type() == base::Value::Type::LIST) {
        p->GetList().Append(std::move(random_value));
      } else {
        // |p| is a dictionary, a fuzzed key is also required.
        std::string key;
        fuzzer->FuzzString(&key);
        p->GetDict().Set(key, std::move(random_value));
      }
    }

    --g_depth;
    return true;
  }
};

template <>
struct FuzzTraits<base::UnguessableToken> {
  static bool Fuzz(base::UnguessableToken* p, Fuzzer* fuzzer) {
    auto low = p->GetLowForSerialization();
    auto high = p->GetHighForSerialization();
    if (!FuzzParam(&low, fuzzer))
      return false;
    if (!FuzzParam(&high, fuzzer))
      return false;
    while (high == 0 && low == 0) {
      FuzzParam(&low, fuzzer);
      FuzzParam(&high, fuzzer);
    }
    *p = base::UnguessableToken::Deserialize(high, low).value();
    return true;
  }
};

template <>
struct FuzzTraits<base::UnsafeSharedMemoryRegion> {
  static bool Fuzz(base::UnsafeSharedMemoryRegion* p, Fuzzer* fuzzer) {
    size_t size = RandInRange(16 * 1024 * 1024 * sizeof(char));
    *p = base::UnsafeSharedMemoryRegion::Create(size);
    return true;
  }
};

template <>
struct FuzzTraits<blink::mojom::EmulatedScreenType> {
  static bool Fuzz(blink::mojom::EmulatedScreenType* p, Fuzzer* fuzzer) {
    int screen_type = RandInRange(
        static_cast<int>(blink::mojom::EmulatedScreenType::kMaxValue) + 1);
    *p = static_cast<blink::mojom::EmulatedScreenType>(screen_type);
    return true;
  }
};

template <>
struct FuzzTraits<viz::FrameSinkId> {
  static bool Fuzz(viz::FrameSinkId* p, Fuzzer* fuzzer) {
    uint32_t client_id;
    uint32_t sink_id;
    if (!FuzzParam(&client_id, fuzzer))
      return false;
    if (!FuzzParam(&sink_id, fuzzer))
      return false;
    *p = viz::FrameSinkId(client_id, sink_id);
    return true;
  }
};

template <>
struct FuzzTraits<viz::LocalSurfaceId> {
  static bool Fuzz(viz::LocalSurfaceId* p, Fuzzer* fuzzer) {
    uint32_t parent_sequence_number = p->parent_sequence_number();
    uint32_t child_sequence_number = p->child_sequence_number();
    base::UnguessableToken embed_token = p->embed_token();
    if (!FuzzParam(&parent_sequence_number, fuzzer))
      return false;
    if (!FuzzParam(&child_sequence_number, fuzzer))
      return false;
    if (!FuzzParam(&embed_token, fuzzer))
      return false;
    *p = viz::LocalSurfaceId(parent_sequence_number, child_sequence_number,
                             embed_token);
    return true;
  }
};

template <>
struct FuzzTraits<blink::PageState> {
  static bool Fuzz(blink::PageState* p, Fuzzer* fuzzer) {
    std::string data = p->ToEncodedData();
    if (!FuzzParam(&data, fuzzer))
      return false;
    *p = blink::PageState::CreateFromEncodedData(data);
    return true;
  }
};

template <>
struct FuzzTraits<device::mojom::ScreenOrientationLockType> {
  static bool Fuzz(device::mojom::ScreenOrientationLockType* p,
                   Fuzzer* fuzzer) {
    int value = RandInRange(
        static_cast<int>(device::mojom::ScreenOrientationLockType::kMaxValue) +
        1);
    *p = static_cast<device::mojom::ScreenOrientationLockType>(value);
    return true;
  }
};

template <>
struct FuzzTraits<ContentSettingsPattern> {
  static bool Fuzz(ContentSettingsPattern* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): This can crash if a pattern is generated from a random
    // string. We could carefully generate a pattern or fix pattern generation.
    return true;
  }
};

template <>
struct FuzzTraits<gfx::BufferFormat> {
  static bool Fuzz(gfx::BufferFormat* p, Fuzzer* fuzzer) {
    int format = RandInRange(static_cast<int>(gfx::BufferFormat::LAST) + 1);
    *p = static_cast<gfx::BufferFormat>(format);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::ColorSpace> {
  static bool Fuzz(gfx::ColorSpace* p, Fuzzer* fuzzer) {
    gfx::ColorSpace::PrimaryID primaries;
    gfx::ColorSpace::TransferID transfer;
    gfx::ColorSpace::MatrixID matrix;
    gfx::ColorSpace::RangeID range;
    if (!FuzzParam(&primaries, fuzzer))
      return false;
    if (!FuzzParam(&transfer, fuzzer))
      return false;
    if (!FuzzParam(&matrix, fuzzer))
      return false;
    if (!FuzzParam(&range, fuzzer))
      return false;
    *p = gfx::ColorSpace(primaries, transfer, matrix, range);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::ColorSpace::MatrixID> {
  static bool Fuzz(gfx::ColorSpace::MatrixID* p, Fuzzer* fuzzer) {
    uint8_t matrix =
        RandInRange(static_cast<int>(gfx::ColorSpace::MatrixID::kMaxValue) + 1);
    *p = static_cast<gfx::ColorSpace::MatrixID>(matrix);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::ColorSpace::PrimaryID> {
  static bool Fuzz(gfx::ColorSpace::PrimaryID* p, Fuzzer* fuzzer) {
    int primaries = RandInRange(
        static_cast<int>(gfx::ColorSpace::PrimaryID::kMaxValue) + 1);
    *p = static_cast<gfx::ColorSpace::PrimaryID>(primaries);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::ColorSpace::RangeID> {
  static bool Fuzz(gfx::ColorSpace::RangeID* p, Fuzzer* fuzzer) {
    uint8_t range =
        RandInRange(static_cast<int>(gfx::ColorSpace::RangeID::kMaxValue) + 1);
    *p = static_cast<gfx::ColorSpace::RangeID>(range);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::ColorSpace::TransferID> {
  static bool Fuzz(gfx::ColorSpace::TransferID* p, Fuzzer* fuzzer) {
    uint8_t transfer = RandInRange(
        static_cast<int>(gfx::ColorSpace::TransferID::kMaxValue) + 1);
    *p = static_cast<gfx::ColorSpace::TransferID>(transfer);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::GpuFenceHandle> {
  static bool Fuzz(gfx::GpuFenceHandle* p, Fuzzer* fuzzer) {
    return true;
  }
};

template <>
struct FuzzTraits<gfx::GpuMemoryBufferHandle> {
  static bool Fuzz(gfx::GpuMemoryBufferHandle* p, Fuzzer* fuzzer) {
    int type;
    if (!FuzzParam(&type, fuzzer))
      return false;
    if (!FuzzParam(&p->offset, fuzzer))
      return false;
    if (!FuzzParam(&p->stride, fuzzer))
      return false;
    if (!FuzzParam(&p->region, fuzzer))
      return false;
    p->type = static_cast<gfx::GpuMemoryBufferType>(type);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::Point> {
  static bool Fuzz(gfx::Point* p, Fuzzer* fuzzer) {
    int x = p->x();
    int y = p->y();
    if (!FuzzParam(&x, fuzzer))
      return false;
    if (!FuzzParam(&y, fuzzer))
      return false;
    p->SetPoint(x, y);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::PointF> {
  static bool Fuzz(gfx::PointF* p, Fuzzer* fuzzer) {
    float x = p->x();
    float y = p->y();
    if (!FuzzParam(&x, fuzzer))
      return false;
    if (!FuzzParam(&y, fuzzer))
      return false;
    p->SetPoint(x, y);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::Rect> {
  static bool Fuzz(gfx::Rect* p, Fuzzer* fuzzer) {
    gfx::Point origin = p->origin();
    gfx::Size size = p->size();
    if (!FuzzParam(&origin, fuzzer))
      return false;
    if (!FuzzParam(&size, fuzzer))
      return false;
    p->set_origin(origin);
    p->set_size(size);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::RectF> {
  static bool Fuzz(gfx::RectF* p, Fuzzer* fuzzer) {
    gfx::PointF origin = p->origin();
    gfx::SizeF size = p->size();
    if (!FuzzParam(&origin, fuzzer))
      return false;
    if (!FuzzParam(&size, fuzzer))
      return false;
    p->set_origin(origin);
    p->set_size(size);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::Range> {
  static bool Fuzz(gfx::Range* p, Fuzzer* fuzzer) {
    size_t start = p->start();
    size_t end = p->end();
    if (!FuzzParam(&start, fuzzer))
      return false;
    if (!FuzzParam(&end, fuzzer))
      return false;
    *p = gfx::Range(start, end);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::Size> {
  static bool Fuzz(gfx::Size* p, Fuzzer* fuzzer) {
    int width = p->width();
    int height = p->height();
    if (!FuzzParam(&width, fuzzer))
      return false;
    if (!FuzzParam(&height, fuzzer))
      return false;
    p->SetSize(width, height);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::SizeF> {
  static bool Fuzz(gfx::SizeF* p, Fuzzer* fuzzer) {
    float w;
    float h;
    if (!FuzzParam(&w, fuzzer))
      return false;
    if (!FuzzParam(&h, fuzzer))
      return false;
    p->SetSize(w, h);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::SwapResponse> {
  static bool Fuzz(gfx::SwapResponse* p, Fuzzer* fuzzer) {
    if (!FuzzParam(&p->swap_id, fuzzer))
      return false;
    if (!FuzzParam(&p->result, fuzzer))
      return false;
    if (!FuzzParam(&p->timings, fuzzer))
      return false;
    return true;
  }
};

template <>
struct FuzzTraits<gfx::SwapResult> {
  static bool Fuzz(gfx::SwapResult* p, Fuzzer* fuzzer) {
    int result =
        RandInRange(static_cast<int>(gfx::SwapResult::SWAP_RESULT_LAST) + 1);
    *p = static_cast<gfx::SwapResult>(result);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::SwapTimings> {
  static bool Fuzz(gfx::SwapTimings* p, Fuzzer* fuzzer) {
    if (!FuzzParam(&p->swap_start, fuzzer))
      return false;
    if (!FuzzParam(&p->swap_end, fuzzer))
      return false;
    return true;
  }
};

template <>
struct FuzzTraits<gfx::Transform> {
  static bool Fuzz(gfx::Transform* p, Fuzzer* fuzzer) {
    float matrix[16];
    p->GetColMajorF(matrix);
    if (!FuzzParamArray(&matrix[0], std::size(matrix), fuzzer))
      return false;
    *p = gfx::Transform::ColMajorF(matrix);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::Vector2d> {
  static bool Fuzz(gfx::Vector2d* p, Fuzzer* fuzzer) {
    int x = p->x();
    int y = p->y();
    if (!FuzzParam(&x, fuzzer))
      return false;
    if (!FuzzParam(&y, fuzzer))
      return false;
    *p = gfx::Vector2d(x, y);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::Vector2dF> {
  static bool Fuzz(gfx::Vector2dF* p, Fuzzer* fuzzer) {
    float x = p->x();
    float y = p->y();
    if (!FuzzParam(&x, fuzzer))
      return false;
    if (!FuzzParam(&y, fuzzer))
      return false;
    *p = gfx::Vector2dF(x, y);
    return true;
  }
};

template <typename TypeMarker, typename WrappedType, WrappedType kInvalidValue>
struct FuzzTraits<base::IdType<TypeMarker, WrappedType, kInvalidValue>> {
  using param_type = base::IdType<TypeMarker, WrappedType, kInvalidValue>;
  static bool Fuzz(param_type* id, Fuzzer* fuzzer) {
    WrappedType raw_value = id->GetUnsafeValue();
    if (!FuzzParam(&raw_value, fuzzer))
      return false;
    *id = param_type::FromUnsafeValue(raw_value);
    return true;
  }
};

template <>
struct FuzzTraits<gl::GpuPreference> {
  static bool Fuzz(gl::GpuPreference* p, Fuzzer* fuzzer) {
    int preference =
        RandInRange(static_cast<int>(gl::GpuPreference::kMaxValue) + 1);
    *p = static_cast<gl::GpuPreference>(preference);
    return true;
  }
};

template <>
struct FuzzTraits<gpu::CommandBuffer::State> {
  static bool Fuzz(gpu::CommandBuffer::State* p, Fuzzer* fuzzer) {
    if (!FuzzParam(&p->get_offset, fuzzer))
      return false;
    if (!FuzzParam(&p->token, fuzzer))
      return false;
    if (!FuzzParam(&p->release_count, fuzzer))
      return false;
    if (!FuzzParam(&p->error, fuzzer))
      return false;
    if (!FuzzParam(&p->context_lost_reason, fuzzer))
      return false;
    if (!FuzzParam(&p->generation, fuzzer))
      return false;
    if (!FuzzParam(&p->set_get_buffer_count, fuzzer))
      return false;
    return true;
  }
};

template <>
struct FuzzTraits<gpu::CommandBufferNamespace> {
  static bool Fuzz(gpu::CommandBufferNamespace* p, Fuzzer* fuzzer) {
    int name_space =
        RandInRange(gpu::CommandBufferNamespace::NUM_COMMAND_BUFFER_NAMESPACES);
    *p = static_cast<gpu::CommandBufferNamespace>(name_space);
    return true;
  }
};

template <>
struct FuzzTraits<gpu::ContextCreationAttribs> {
  static bool Fuzz(gpu::ContextCreationAttribs* p, Fuzzer* fuzzer) {
    if (!FuzzParam(&p->gpu_preference, fuzzer))
      return false;
    if (!FuzzParam(&p->context_type, fuzzer))
      return false;
    return true;
  }
};

template <>
struct FuzzTraits<gpu::ContextType> {
  static bool Fuzz(gpu::ContextType* p, Fuzzer* fuzzer) {
    int type = RandInRange(gpu::ContextType::CONTEXT_TYPE_LAST + 1);
    *p = static_cast<gpu::ContextType>(type);
    return true;
  }
};

template <>
struct FuzzTraits<gpu::error::ContextLostReason> {
  static bool Fuzz(gpu::error::ContextLostReason* p, Fuzzer* fuzzer) {
    int reason =
        RandInRange(gpu::error::ContextLostReason::kContextLostReasonLast + 1);
    *p = static_cast<gpu::error::ContextLostReason>(reason);
    return true;
  }
};

template <>
struct FuzzTraits<gpu::error::Error> {
  static bool Fuzz(gpu::error::Error* p, Fuzzer* fuzzer) {
    int error = RandInRange(gpu::error::Error::kErrorLast + 1);
    *p = static_cast<gpu::error::Error>(error);
    return true;
  }
};

template <>
struct FuzzTraits<gpu::Mailbox> {
  static bool Fuzz(gpu::Mailbox* p, Fuzzer* fuzzer) {
    fuzzer->FuzzBytes(p->name, sizeof(p->name));
    return true;
  }
};

template <>
struct FuzzTraits<gpu::SwapBuffersCompleteParams> {
  static bool Fuzz(gpu::SwapBuffersCompleteParams* p, Fuzzer* fuzzer) {
    if (!FuzzParam(&p->swap_response, fuzzer))
      return false;
    return true;
  }
};

template <>
struct FuzzTraits<gpu::SyncToken> {
  static bool Fuzz(gpu::SyncToken* p, Fuzzer* fuzzer) {
    bool verified_flush = false;
    gpu::CommandBufferNamespace namespace_id =
        gpu::CommandBufferNamespace::INVALID;
    gpu::CommandBufferId command_buffer_id;
    uint64_t release_count = 0;

    if (!FuzzParam(&verified_flush, fuzzer))
      return false;
    if (!FuzzParam(&namespace_id, fuzzer))
      return false;
    if (!FuzzParam(&command_buffer_id, fuzzer))
      return false;
    if (!FuzzParam(&release_count, fuzzer))
      return false;

    p->Clear();
    p->Set(namespace_id, command_buffer_id, release_count);
    if (verified_flush)
      p->SetVerifyFlush();
    return true;
  }
};

template <>
struct FuzzTraits<gpu::MailboxHolder> {
  static bool Fuzz(gpu::MailboxHolder* p, Fuzzer* fuzzer) {
    if (!FuzzParam(&p->mailbox, fuzzer))
      return false;
    if (!FuzzParam(&p->sync_token, fuzzer))
      return false;
    if (!FuzzParam(&p->texture_target, fuzzer))
      return false;
    return true;
  }
};

template <>
struct FuzzTraits<GURL> {
  static bool Fuzz(GURL* p, Fuzzer* fuzzer) {
    if (!fuzzer->ShouldGenerate()) {
      std::string spec = p->possibly_invalid_spec();
      if (!FuzzParam(&spec, fuzzer))
        return false;
      if (spec != p->possibly_invalid_spec())
        *p = GURL(spec);
      return true;
    }

    const char url_chars[] = "Ahtp0:/.?+\\%&#";
    size_t count = RandInRange(100);
    std::string random_url;
    for (size_t i = 0; i < count; ++i)
      random_url += url_chars[RandInRange(sizeof(url_chars) - 1)];
    int selector = RandInRange(10);
    if (selector == 0)
      random_url = std::string("http://") + random_url;
    else if (selector == 1)
      random_url = std::string("file://") + random_url;
    else if (selector == 2)
      random_url = std::string("javascript:") + random_url;
    else if (selector == 2)
      random_url = std::string("data:") + random_url;
    *p = GURL(random_url);
    return true;
  }
};

#if BUILDFLAG(IS_WIN)
template <>
struct FuzzTraits<HWND> {
  static bool Fuzz(HWND* p, Fuzzer* fuzzer) {
    // TODO(aarya): This should actually do something.
    return true;
  }
};
#endif

template <>
struct FuzzTraits<std::unique_ptr<IPC::Message>> {
  static bool Fuzz(std::unique_ptr<IPC::Message>* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): Support mutation.
    if (!fuzzer->ShouldGenerate())
      return true;

    if (g_function_vector.empty())
      return false;
    size_t index = RandInRange(g_function_vector.size());
    std::unique_ptr<IPC::Message> ipc_message =
        (*g_function_vector[index])(nullptr, fuzzer);
    if (!ipc_message)
      return false;
    *p = std::move(ipc_message);
    return true;
  }
};

template <>
struct FuzzTraits<IPC::PlatformFileForTransit> {
  static bool Fuzz(IPC::PlatformFileForTransit* p, Fuzzer* fuzzer) {
    // TODO(inferno): I don't think we can generate real ones due to check on
    // construct.
    return true;
  }
};

template <>
struct FuzzTraits<IPC::ChannelHandle> {
  static bool Fuzz(IPC::ChannelHandle* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): Support mutation.
    if (!fuzzer->ShouldGenerate())
      return true;

    return FuzzParam(&p->mojo_handle, fuzzer);
  }
};

#if BUILDFLAG(IS_WIN)
template <>
struct FuzzTraits<LOGFONT> {
  static bool Fuzz(LOGFONT* p, Fuzzer* fuzzer) {
    // TODO(aarya): This should actually do something.
    return true;
  }
};
#endif

template <>
struct FuzzTraits<media::AudioParameters> {
  static bool Fuzz(media::AudioParameters* p, Fuzzer* fuzzer) {
    int channel_layout = p->channel_layout();
    int format = p->format();
    int sample_rate = p->sample_rate();
    int frames_per_buffer = p->frames_per_buffer();
    int channels = p->channels();
    int effects = p->effects();
    // TODO(mbarbella): Support ChannelLayout mutation and invalid values.
    if (fuzzer->ShouldGenerate()) {
      channel_layout =
          RandInRange(media::ChannelLayout::CHANNEL_LAYOUT_MAX + 1);
    }
    if (!FuzzParam(&format, fuzzer))
      return false;
    if (!FuzzParam(&sample_rate, fuzzer))
      return false;
    if (!FuzzParam(&frames_per_buffer, fuzzer))
      return false;
    if (!FuzzParam(&channels, fuzzer))
      return false;
    if (!FuzzParam(&effects, fuzzer))
      return false;
    media::AudioParameters params(
        static_cast<media::AudioParameters::Format>(format),
        {static_cast<media::ChannelLayout>(channel_layout), channels},
        sample_rate, frames_per_buffer);
    params.set_effects(effects);
    *p = params;
    return true;
  }
};

template <>
struct FuzzTraits<media::OverlayInfo> {
  static bool Fuzz(media::OverlayInfo* p, Fuzzer* fuzzer) {
    if (!FuzzParam(&p->is_fullscreen, fuzzer))
      return false;
    if (!FuzzParam(&p->is_persistent_video, fuzzer))
      return false;
    if (!FuzzParam(&p->routing_token, fuzzer))
      return false;
    return true;
  }
};

template <>
struct FuzzTraits<media::VideoPixelFormat> {
  static bool Fuzz(media::VideoPixelFormat* p, Fuzzer* fuzzer) {
    int format = RandInRange(media::VideoPixelFormat::PIXEL_FORMAT_MAX + 1);
    *p = static_cast<media::VideoPixelFormat>(format);
    return true;
  }
};

template <>
struct FuzzTraits<net::EffectiveConnectionType> {
  static bool Fuzz(net::EffectiveConnectionType* p, Fuzzer* fuzzer) {
    int type = RandInRange(
        net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_LAST + 1);
    *p = static_cast<net::EffectiveConnectionType>(type);
    return true;
  }
};

template <>
struct FuzzTraits<net::LoadTimingInfo> {
  static bool Fuzz(net::LoadTimingInfo* p, Fuzzer* fuzzer) {
    return FuzzParam(&p->socket_log_id, fuzzer) &&
           FuzzParam(&p->socket_reused, fuzzer) &&
           FuzzParam(&p->request_start_time, fuzzer) &&
           FuzzParam(&p->request_start, fuzzer) &&
           FuzzParam(&p->proxy_resolve_start, fuzzer) &&
           FuzzParam(&p->proxy_resolve_end, fuzzer) &&
           FuzzParam(&p->connect_timing.domain_lookup_start, fuzzer) &&
           FuzzParam(&p->connect_timing.domain_lookup_end, fuzzer) &&
           FuzzParam(&p->connect_timing.connect_start, fuzzer) &&
           FuzzParam(&p->connect_timing.connect_end, fuzzer) &&
           FuzzParam(&p->connect_timing.ssl_start, fuzzer) &&
           FuzzParam(&p->connect_timing.ssl_end, fuzzer) &&
           FuzzParam(&p->send_start, fuzzer) &&
           FuzzParam(&p->send_end, fuzzer) &&
           FuzzParam(&p->receive_headers_end, fuzzer);
  }
};

template <>
struct FuzzTraits<net::HostPortPair> {
  static bool Fuzz(net::HostPortPair* p, Fuzzer* fuzzer) {
    std::string host = p->host();
    uint16_t port = p->port();
    if (!FuzzParam(&host, fuzzer))
      return false;
    if (!FuzzParam(&port, fuzzer))
      return false;
    p->set_host(host);
    p->set_port(port);
    return true;
  }
};

template <>
struct FuzzTraits<net::IPAddress> {
  static bool Fuzz(net::IPAddress* p, Fuzzer* fuzzer) {
    std::vector<uint8_t> bytes = p->CopyBytesToVector();
    if (!FuzzParam(&bytes, fuzzer))
      return false;
    net::IPAddress ip_address(bytes);
    *p = ip_address;
    return true;
  }
};

template <>
struct FuzzTraits<net::IPEndPoint> {
  static bool Fuzz(net::IPEndPoint* p, Fuzzer* fuzzer) {
    net::IPAddress ip_address = p->address();
    int port = p->port();
    if (!FuzzParam(&ip_address, fuzzer))
      return false;
    if (!FuzzParam(&port, fuzzer))
      return false;
    net::IPEndPoint ip_endpoint(ip_address, port);
    *p = ip_endpoint;
    return true;
  }
};

#if BUILDFLAG(ENABLE_PPAPI)
// PP_ traits.
template <>
struct FuzzTraits<PP_Bool> {
  static bool Fuzz(PP_Bool* p, Fuzzer* fuzzer) {
    bool tmp = PP_ToBool(*p);
    if (!FuzzParam(&tmp, fuzzer))
      return false;
    *p = PP_FromBool(tmp);
    return true;
  }
};

template <>
struct FuzzTraits<PP_NetAddress_Private> {
  static bool Fuzz(PP_NetAddress_Private* p, Fuzzer* fuzzer) {
    p->size = RandInRange(sizeof(p->data) + 1);
    fuzzer->FuzzBytes(&p->data, p->size);
    return true;
  }
};

template <>
struct FuzzTraits<ppapi::PPB_X509Certificate_Fields> {
  static bool Fuzz(ppapi::PPB_X509Certificate_Fields* p,
                       Fuzzer* fuzzer) {
    // TODO(mbarbella): This should actually do something.
    return true;
  }
};

template <>
struct FuzzTraits<ppapi::proxy::ResourceMessageCallParams> {
  static bool Fuzz(
      ppapi::proxy::ResourceMessageCallParams* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): Support mutation.
    if (!fuzzer->ShouldGenerate())
      return true;

    PP_Resource resource;
    int32_t sequence;
    bool has_callback;
    if (!FuzzParam(&resource, fuzzer))
      return false;
    if (!FuzzParam(&sequence, fuzzer))
      return false;
    if (!FuzzParam(&has_callback, fuzzer))
      return false;
    *p = ppapi::proxy::ResourceMessageCallParams(resource, sequence);
    if (has_callback)
      p->set_has_callback();
    return true;
  }
};

template <>
struct FuzzTraits<ppapi::proxy::ResourceMessageReplyParams> {
  static bool Fuzz(
      ppapi::proxy::ResourceMessageReplyParams* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): Support mutation.
    if (!fuzzer->ShouldGenerate())
      return true;

    PP_Resource resource;
    int32_t sequence;
    int32_t result;
    if (!FuzzParam(&resource, fuzzer))
      return false;
    if (!FuzzParam(&sequence, fuzzer))
      return false;
    if (!FuzzParam(&result, fuzzer))
      return false;
    *p = ppapi::proxy::ResourceMessageReplyParams(resource, sequence);
    p->set_result(result);
    return true;
  }
};

template <>
struct FuzzTraits<ppapi::proxy::SerializedHandle> {
  static bool Fuzz(ppapi::proxy::SerializedHandle* p,
                       Fuzzer* fuzzer) {
    // TODO(mbarbella): This should actually do something.
    return true;
  }
};

template <>
struct FuzzTraits<ppapi::proxy::SerializedFontDescription> {
  static bool Fuzz(ppapi::proxy::SerializedFontDescription* p,
                       Fuzzer* fuzzer) {
    // TODO(mbarbella): This should actually do something.
    return true;
  }
};

template <>
struct FuzzTraits<ppapi::proxy::SerializedVar> {
  static bool Fuzz(ppapi::proxy::SerializedVar* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): This should actually do something.
    return true;
  }
};

template <>
struct FuzzTraits<ppapi::HostResource> {
  static bool Fuzz(ppapi::HostResource* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): Support mutation.
    if (!fuzzer->ShouldGenerate())
      return true;

    PP_Instance instance;
    PP_Resource resource;
    if (!FuzzParam(&instance, fuzzer))
      return false;
    if (!FuzzParam(&resource, fuzzer))
      return false;
    p->SetHostResource(instance, resource);
    return true;
  }
};

template <>
struct FuzzTraits<ppapi::PepperFilePath> {
  static bool Fuzz(ppapi::PepperFilePath *p, Fuzzer* fuzzer) {
    // TODO(mbarbella): Support mutation.
    if (!fuzzer->ShouldGenerate())
      return true;

    unsigned domain = RandInRange(ppapi::PepperFilePath::DOMAIN_MAX_VALID+1);
    base::FilePath path;
    if (!FuzzParam(&path, fuzzer))
      return false;
    *p = ppapi::PepperFilePath(
        static_cast<ppapi::PepperFilePath::Domain>(domain), path);
    return true;
  }
};

template <>
struct FuzzTraits<ppapi::PpapiPermissions> {
  static bool Fuzz(ppapi::PpapiPermissions* p, Fuzzer* fuzzer) {
    uint32_t bits = p->GetBits();
    if (!FuzzParam(&bits, fuzzer))
      return false;
    *p = ppapi::PpapiPermissions(bits);
    return true;
  }
};

template <>
struct FuzzTraits<ppapi::SocketOptionData> {
  static bool Fuzz(ppapi::SocketOptionData* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): This can be improved.
    int32_t tmp;
    p->GetInt32(&tmp);
    if (!FuzzParam(&tmp, fuzzer))
      return false;
    p->SetInt32(tmp);
    return true;
  }
};
#endif  // BUILDFLAG(ENABLE_PPAPI)

template <>
struct FuzzTraits<printing::mojom::MarginType> {
  static bool Fuzz(printing::mojom::MarginType* p, Fuzzer* fuzzer) {
    int type = RandInRange(
        static_cast<int>(printing::mojom::MarginType::kMaxValue) + 1);
    *p = static_cast<printing::mojom::MarginType>(type);
    return true;
  }
};

template <>
struct FuzzTraits<SkBitmap> {
  static bool Fuzz(SkBitmap* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): This should actually do something.
    return true;
  }
};

template <>
struct FuzzTraits<ui::LatencyInfo> {
  static bool Fuzz(ui::LatencyInfo* p, Fuzzer* fuzzer) {
    // TODO(inferno): Add param traits for |latency_components|.
    int64_t trace_id = p->trace_id();
    bool terminated = p->terminated();
    if (!FuzzParam(&trace_id, fuzzer))
      return false;
    if (!FuzzParam(&terminated, fuzzer))
      return false;

    ui::LatencyInfo latency(trace_id, terminated);
    *p = latency;

    return true;
  }
};

template <>
struct FuzzTraits<url::Origin> {
  static bool Fuzz(url::Origin* p, Fuzzer* fuzzer) {
    bool opaque = p->opaque();
    if (!FuzzParam(&opaque, fuzzer))
      return false;
    std::string scheme = p->GetTupleOrPrecursorTupleIfOpaque().scheme();
    std::string host = p->GetTupleOrPrecursorTupleIfOpaque().host();
    uint16_t port = p->GetTupleOrPrecursorTupleIfOpaque().port();
    if (!FuzzParam(&scheme, fuzzer))
      return false;
    if (!FuzzParam(&host, fuzzer))
      return false;
    if (!FuzzParam(&port, fuzzer))
      return false;

    std::optional<url::Origin> origin;
    if (!opaque) {
      origin = url::Origin::UnsafelyCreateTupleOriginWithoutNormalization(
          scheme, host, port);
    } else {
      std::optional<base::UnguessableToken> token;
      if (auto* nonce = p->GetNonceForSerialization()) {
        token = *nonce;
      } else {
        auto high = RandU64();
        auto low = RandU64();
        while (high == 0 && low == 0) {
          high = RandU64();
          low = RandU64();
        }
        token = base::UnguessableToken::Deserialize(high, low).value();
      }
      if (!FuzzParam(&(*token), fuzzer))
        return false;
      origin = url::Origin::UnsafelyCreateOpaqueOriginWithoutNormalization(
          scheme, host, port, url::Origin::Nonce(*token));
    }

    if (!origin) {
      // This means that we produced non-canonical values that were rejected by
      // UnsafelyCreate. Which is nice, except, those are arguably interesting
      // values to be sending over the wire sometimes, to make sure they're
      // rejected at the receiving end.
      //
      // We could potentially call CreateFromNormalizedTuple here to force their
      // creation, except that could lead to invariant violations within the
      // url::Origin we construct -- and potentially crash the fuzzer. What to
      // do?
      return false;
    }

    *p = std::move(origin).value();
    return true;
  }
};

// Redefine macros to generate generating from traits declarations.
// STRUCT declarations cause corresponding STRUCT_TRAITS declarations to occur.
#undef IPC_STRUCT_BEGIN
#undef IPC_STRUCT_BEGIN_WITH_PARENT
#undef IPC_STRUCT_MEMBER
#undef IPC_STRUCT_END
#define IPC_STRUCT_BEGIN_WITH_PARENT(struct_name, parent) \
  IPC_STRUCT_BEGIN(struct_name)
#define IPC_STRUCT_BEGIN(struct_name) IPC_STRUCT_TRAITS_BEGIN(struct_name)
#define IPC_STRUCT_MEMBER(type, name, ...) IPC_STRUCT_TRAITS_MEMBER(name)
#define IPC_STRUCT_END() IPC_STRUCT_TRAITS_END()

// Set up so next include will generate generate trait classes.
#undef IPC_STRUCT_TRAITS_BEGIN
#undef IPC_STRUCT_TRAITS_MEMBER
#undef IPC_STRUCT_TRAITS_PARENT
#undef IPC_STRUCT_TRAITS_END
#define IPC_STRUCT_TRAITS_BEGIN(struct_name) \
  template <> \
  struct FuzzTraits<struct_name> { \
    static bool Fuzz(struct_name *p, Fuzzer* fuzzer) {

#define IPC_STRUCT_TRAITS_MEMBER(name) \
      if (!FuzzParam(&p->name, fuzzer)) \
        return false;

#define IPC_STRUCT_TRAITS_PARENT(type) \
      if (!FuzzParam(static_cast<type*>(p), fuzzer)) \
        return false;

#define IPC_STRUCT_TRAITS_END() \
      return true; \
    } \
  };

// If |condition| isn't met, the messsge will fail to serialize. Try
// increasingly smaller ranges until we find one that happens to meet
// the condition, or fail trying.
// TODO(mbarbella): Attempt to validate even in the mutation case.
#undef IPC_ENUM_TRAITS_VALIDATE
#define IPC_ENUM_TRAITS_VALIDATE(enum_name, condition)             \
  template <>                                                      \
  struct FuzzTraits<enum_name> {                                   \
    static bool Fuzz(enum_name* p, Fuzzer* fuzzer) {               \
      if (!fuzzer->ShouldGenerate()) {                             \
        return FuzzParam(reinterpret_cast<int*>(p), fuzzer);       \
      }                                                            \
      for (int shift = 30; shift; --shift) {                       \
        for (int tries = 0; tries < 2; ++tries) {                  \
          int value = RandInRange(1 << shift);                     \
          if (condition) {                                         \
            *reinterpret_cast<int*>(p) = value;                    \
            return true;                                           \
          }                                                        \
        }                                                          \
      }                                                            \
      std::cerr << "failed to satisfy " << #condition << "\n";     \
      return false;                                                \
    }                                                              \
  };

// Bring them into existence.
#include "tools/ipc_fuzzer/message_lib/all_messages.h"
#include "tools/ipc_fuzzer/message_lib/all_message_null_macros.h"

#define MAX_FAKE_ROUTING_ID 15

// MessageFactory abstracts away constructing control/routed messages by
// providing an additional random routing ID argument when necessary.
template <typename Message, IPC::MessageKind>
class MessageFactory;

template <typename Message>
class MessageFactory<Message, IPC::MessageKind::CONTROL> {
 public:
  template <typename... Args>
  static std::unique_ptr<Message> New(const Args&... args) {
    return std::make_unique<Message>(args...);
  }
};

template <typename Message>
class MessageFactory<Message, IPC::MessageKind::ROUTED> {
 public:
  template <typename... Args>
  static std::unique_ptr<Message> New(const Args&... args) {
    return std::make_unique<Message>(RandInRange(MAX_FAKE_ROUTING_ID), args...);
  }
};

template <typename Message>
class FuzzerHelper;

template <typename Meta, typename... Ins>
class FuzzerHelper<IPC::MessageT<Meta, std::tuple<Ins...>, void>> {
 public:
  using Message = IPC::MessageT<Meta, std::tuple<Ins...>, void>;

  static std::unique_ptr<IPC::Message> Fuzz(IPC::Message* msg, Fuzzer* fuzzer) {
    return FuzzImpl(msg, fuzzer, std::index_sequence_for<Ins...>());
  }

 private:
  template <size_t... Ns>
  static std::unique_ptr<IPC::Message> FuzzImpl(IPC::Message* msg,
                                                Fuzzer* fuzzer,
                                                std::index_sequence<Ns...>) {
    typename Message::Param p;
    if (msg) {
      Message::Read(static_cast<Message*>(msg), &p);
    }
    if (FuzzParam(&p, fuzzer)) {
      return MessageFactory<Message, Meta::kKind>::New(std::get<Ns>(p)...);
    }
    std::cerr << "Don't know how to handle " << Meta::kName << "\n";
    return nullptr;
  }
};

template <typename Meta, typename... Ins, typename... Outs>
class FuzzerHelper<
    IPC::MessageT<Meta, std::tuple<Ins...>, std::tuple<Outs...>>> {
 public:
  using Message = IPC::MessageT<Meta, std::tuple<Ins...>, std::tuple<Outs...>>;

  static std::unique_ptr<IPC::Message> Fuzz(IPC::Message* msg, Fuzzer* fuzzer) {
    return FuzzImpl(msg, fuzzer, std::index_sequence_for<Ins...>());
  }

 private:
  template <size_t... Ns>
  static std::unique_ptr<IPC::Message> FuzzImpl(IPC::Message* msg,
                                                Fuzzer* fuzzer,
                                                std::index_sequence<Ns...>) {
    typename Message::SendParam p;
    Message* real_msg = static_cast<Message*>(msg);
    std::unique_ptr<Message> new_msg;
    if (real_msg) {
      Message::ReadSendParam(real_msg, &p);
    }
    if (FuzzParam(&p, fuzzer)) {
      new_msg = MessageFactory<Message, Meta::kKind>::New(
          std::get<Ns>(p)..., static_cast<Outs*>(nullptr)...);
    }
    if (real_msg && new_msg) {
      MessageCracker::CopyMessageID(new_msg.get(), real_msg);
    } else if (!new_msg) {
      std::cerr << "Don't know how to handle " << Meta::kName << "\n";
    }
    return new_msg;
  }
};

#include "tools/ipc_fuzzer/message_lib/all_message_null_macros.h"

void PopulateFuzzerFunctionVector(
    FuzzerFunctionVector* function_vector) {
#undef IPC_MESSAGE_DECL
#define IPC_MESSAGE_DECL(name, ...) \
  function_vector->push_back(FuzzerHelper<name>::Fuzz);
#include "tools/ipc_fuzzer/message_lib/all_messages.h"
}

// Redefine macros to register fuzzing functions into map.
#include "tools/ipc_fuzzer/message_lib/all_message_null_macros.h"
#undef IPC_MESSAGE_DECL
#define IPC_MESSAGE_DECL(name, ...) \
  (*map)[static_cast<uint32_t>(name::ID)] = FuzzerHelper<name>::Fuzz;

void PopulateFuzzerFunctionMap(FuzzerFunctionMap* map) {
#include "tools/ipc_fuzzer/message_lib/all_messages.h"
}

}  // namespace ipc_fuzzer
