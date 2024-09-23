// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/display/win/audio_edid_scan.h"

#include <objbase.h>

#include <oleauto.h>
#include <string.h>

#include "base/logging.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "base/win/wmi.h"
#include "ui/display/util/edid_parser.h"

namespace display {
namespace win {

namespace {

// Use 'wmi_services' to execute the WmiGetMonitorRawEEdidV1Block method of
// 'get_edid_block' for the 'path' and 'id'.  The result will be EDID data
// which will be appended to the 'edid_blob'.  A single device (or monitor)
// will have one 'path' with multiple 'id' values.  To get all 'id' values
// for a given 'path', start with 'id' of zero and stop when 'id' cannot be
// read.
bool AppendBlock(Microsoft::WRL::ComPtr<IWbemServices>& wmi_services,
                 Microsoft::WRL::ComPtr<IWbemClassObject>& get_edid_block,
                 base::win::ScopedVariant& path,
                 int id,
                 std::vector<uint8_t>& edid_blob) {
  base::win::ScopedVariant block_id(id);

  if (FAILED(get_edid_block->Put(L"BlockId", 0, block_id.AsInput(), 0)))
    return false;

  Microsoft::WRL::ComPtr<IWbemClassObject> out_params;
  HRESULT hr = wmi_services->ExecMethod(
      V_BSTR(path.ptr()),
      base::win::ScopedBstr(L"WmiGetMonitorRawEEdidV1Block").Get(), 0, nullptr,
      get_edid_block.Get(), &out_params, nullptr);
  if (FAILED(hr))
    return false;

  base::win::ScopedVariant block_type;
  if (FAILED(
          out_params->Get(L"BlockType", 0, block_type.Receive(), nullptr, 0))) {
    return false;
  }

  base::win::ScopedVariant block_content;
  if (FAILED(out_params->Get(L"BlockContent", 0, block_content.Receive(),
                             nullptr, 0))) {
    return false;
  }

  // Block type: 1=EDID base block, 2=EDID block map, 255=Other
  constexpr int allowed_types[] = {1, 2, 255};
  const int found_type = V_I4(block_type.ptr());
  if (std::none_of(std::begin(allowed_types), std::end(allowed_types),
                   [found_type](int i) { return i == found_type; })) {
    return false;
  }

  if (block_content.type() != (VT_ARRAY | VT_UI1))
    return false;

  SAFEARRAY* array = V_ARRAY(block_content.ptr());
  if (SafeArrayGetDim(array) != 1)
    return false;

  long lower_bound = 0;
  long upper_bound = 0;
  SafeArrayGetLBound(array, 1, &lower_bound);
  SafeArrayGetUBound(array, 1, &upper_bound);
  if (lower_bound || upper_bound <= lower_bound)
    return false;

  uint8_t* block = nullptr;
  SafeArrayAccessData(array, reinterpret_cast<void**>(&block));
  edid_blob.insert(edid_blob.end(), block, block + upper_bound + 1);
  SafeArrayUnaccessData(array);

  return true;
}

}  // namespace

uint32_t ScanEdidBitstreams() {
  // The WMI service allows the querying of monitor-type devices which report
  // Extended Display Identification Data (EDID).  The WMI service can be
  // queried for a list of COM objects which represent the "paths" which
  // are associated with individual EDID devices.  Querying each of those
  // paths using the WmiGetMonitorRawEEdidV1Block method returns the EDID
  // blocks for those devices.  We query the extended blocks which contain
  // the Short Audio Descriptor (SAD), and parse them to obtain a bitmask
  // indicating which audio content is supported.  The mask consists of
  // AudioParameters::Format flags.  If multiple EDID devices are present,
  // the intersection of flags is reported.
  Microsoft::WRL::ComPtr<IWbemServices> wmi_services =
      base::win::CreateWmiConnection(true, L"ROOT\\WMI");
  if (!wmi_services)
    return 0;

  Microsoft::WRL::ComPtr<IWbemClassObject> get_edid_block;
  if (!base::win::CreateWmiClassMethodObject(
          wmi_services.Get(), L"WmiMonitorDescriptorMethods",
          L"WmiGetMonitorRawEEdidV1Block", &get_edid_block)) {
    return 0;
  }

  Microsoft::WRL::ComPtr<IEnumWbemClassObject> wmi_enumerator;
  HRESULT hr = wmi_services->CreateInstanceEnum(
      base::win::ScopedBstr(L"WmiMonitorDescriptorMethods").Get(),
      WBEM_FLAG_FORWARD_ONLY, nullptr, &wmi_enumerator);
  if (FAILED(hr))
    return 0;

  bool first = true;
  uint32_t bitstream_mask = 0;

  while (true) {
    Microsoft::WRL::ComPtr<IWbemClassObject> class_object;
    ULONG items_returned = 0;
    hr = wmi_enumerator->Next(WBEM_INFINITE, 1, &class_object, &items_returned);
    if (FAILED(hr) || hr == WBEM_S_FALSE || items_returned == 0)
      break;

    // Path refers to the display for which EDID will be read.
    base::win::ScopedVariant path;
    class_object->Get(L"__PATH", 0, path.Receive(), nullptr, nullptr);

    // Reading a single EDID block at a time, assemble all blocks for the path.
    int block_id = 0;
    std::vector<uint8_t> edid_blob;
    while (AppendBlock(wmi_services, get_edid_block, path, block_id, edid_blob))
      block_id++;

    if (first) {
      first = false;
      bitstream_mask =
          display::EdidParser(std::move(edid_blob)).audio_formats();
    } else {
      bitstream_mask &=
          display::EdidParser(std::move(edid_blob)).audio_formats();
    }
  }

  return bitstream_mask;
}

}  // namespace win
}  // namespace display
