// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#include "power_sampler.h"

// These correspond to the funcID values returned by GetMsrFunc
const int MSR_FUNC_FREQ = 0;
const int MSR_FUNC_POWER = 1;
const int MSR_FUNC_TEMP = 2;
const int MSR_FUNC_MAX_POWER = 3; /* ????? */

void PowerSampler::SampleCPUPowerState() {
  if (!IntelEnergyLibInitialize || !GetNumMsrs || !GetMsrName || !GetMsrFunc ||
      !GetPowerData || !ReadSample) {
    return;
  }

  int num_MSRs = 0;
  GetNumMsrs(&num_MSRs);
  ReadSample();

  for (int i = 0; i < num_MSRs; ++i) {
    int func_id;
    wchar_t MSR_name[1024];
    GetMsrFunc(i, &func_id);

    int nData;
    double data[3] = {};
    GetPowerData(0, i, data, &nData);

    if (func_id == MSR_FUNC_POWER) {
      // data[0] is Power (W)
      // data[1] is Energy (J)
      // data[2] is Energy (mWh)
      // Round to nearest .0001 to avoid distracting excess precision.
      GetMsrName(i, MSR_name);
      power_[MSR_name] = round(data[0] * 10000) / 10000;
    }
  }
}

PowerSampler::PowerSampler() {
// If Intel Power Gadget is installed then use it to get CPU power data.
#if _M_X64
  PCWSTR dllName = L"\\EnergyLib64.dll";
#else
  PCWSTR dllName = L"\\EnergyLib32.dll";
#endif
#pragma warning(disable : 4996)
  PCWSTR powerGadgetDir = _wgetenv(L"IPG_Dir");
  if (powerGadgetDir)
    energy_lib_ = LoadLibrary((std::wstring(powerGadgetDir) + dllName).c_str());
  if (energy_lib_) {
    IntelEnergyLibInitialize = (IntelEnergyLibInitialize_t)GetProcAddress(
        energy_lib_, "IntelEnergyLibInitialize");
    GetNumMsrs = (GetNumMsrs_t)GetProcAddress(energy_lib_, "GetNumMsrs");
    GetMsrName = (GetMsrName_t)GetProcAddress(energy_lib_, "GetMsrName");
    GetMsrFunc = (GetMsrFunc_t)GetProcAddress(energy_lib_, "GetMsrFunc");
    GetPowerData = (GetPowerData_t)GetProcAddress(energy_lib_, "GetPowerData");
    ReadSample = (ReadSample_t)GetProcAddress(energy_lib_, "ReadSample");

    if (IntelEnergyLibInitialize && ReadSample) {
      IntelEnergyLibInitialize();
      ReadSample();
    }
  }
}

PowerSampler::~PowerSampler() {
  if (energy_lib_)
    FreeLibrary(energy_lib_);
}