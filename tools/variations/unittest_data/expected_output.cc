// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// GENERATED FROM THE SCHEMA DEFINITION AND DESCRIPTION IN
//   field_trial_testing_config_schema.json
//   test_config.json
// DO NOT EDIT.

#include "test_output.h"


const OverrideUIString array_kFieldTrialConfig_override_ui_string[] = {
      {
        4045341670,
        "test",
      },
};
const Study::Platform array_kFieldTrialConfig_platforms_8[] = {
      Study::PLATFORM_WINDOWS,
};
const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_5[] = {
    {
      "TestGroup",
      array_kFieldTrialConfig_platforms_8,
      {},
      std::nullopt,
      nullptr,
      {},
      {},
      {},
      nullptr,
      array_kFieldTrialConfig_override_ui_string,
      {},
      {},
    },
};
const Study::FormFactor array_kFieldTrialConfig_form_factors_0[] = {
      Study::TABLET,
};
const Study::Platform array_kFieldTrialConfig_platforms_7[] = {
      Study::PLATFORM_WINDOWS,
};
const Study::FormFactor array_kFieldTrialConfig_form_factors[] = {
      Study::DESKTOP,
      Study::PHONE,
};
const Study::Platform array_kFieldTrialConfig_platforms_6[] = {
      Study::PLATFORM_WINDOWS,
};
const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_4[] = {
    {
      "TestGroup1",
      array_kFieldTrialConfig_platforms_6,
      array_kFieldTrialConfig_form_factors,
      std::nullopt,
      nullptr,
      {},
      {},
      {},
      nullptr,
      {},
      {},
      {},
    },
    {
      "TestGroup2",
      array_kFieldTrialConfig_platforms_7,
      array_kFieldTrialConfig_form_factors_0,
      std::nullopt,
      nullptr,
      {},
      {},
      {},
      nullptr,
      {},
      {},
      {},
    },
};
const Study::Platform array_kFieldTrialConfig_platforms_5[] = {
      Study::PLATFORM_WINDOWS,
};
const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_3[] = {
    {
      "ForcedGroup",
      array_kFieldTrialConfig_platforms_5,
      {},
      std::nullopt,
      nullptr,
      {},
      {},
      {},
      "my-forcing-flag",
      {},
      {},
      {},
    },
};
const Study::Platform array_kFieldTrialConfig_platforms_4[] = {
      Study::PLATFORM_WINDOWS,
};
const Study::Platform array_kFieldTrialConfig_platforms_3[] = {
      Study::PLATFORM_WINDOWS,
};
const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_2[] = {
    {
      "TestGroup1",
      array_kFieldTrialConfig_platforms_3,
      {},
      true,
      nullptr,
      {},
      {},
      {},
      nullptr,
      {},
      {},
      {},
    },
    {
      "TestGroup2",
      array_kFieldTrialConfig_platforms_4,
      {},
      false,
      nullptr,
      {},
      {},
      {},
      nullptr,
      {},
      {},
      {},
    },
};
const char* const array_kFieldTrialConfig_enable_features_1[] = {
      "X",
};
const Study::Platform array_kFieldTrialConfig_platforms_2[] = {
      Study::PLATFORM_WINDOWS,
};
const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_1[] = {
    {
      "TestGroup3",
      array_kFieldTrialConfig_platforms_2,
      {},
      std::nullopt,
      nullptr,
      {},
      array_kFieldTrialConfig_enable_features_1,
      {},
      nullptr,
      {},
      {},
      {},
    },
};
const char* const array_kFieldTrialConfig_disable_features_0[] = {
      "F",
};
const char* const array_kFieldTrialConfig_enable_features_0[] = {
      "D",
      "E",
};
const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params_0[] = {
      {
        "x",
        "3",
      },
      {
        "y",
        "4",
      },
};
const Study::Platform array_kFieldTrialConfig_platforms_1[] = {
      Study::PLATFORM_WINDOWS,
};
const char* const array_kFieldTrialConfig_disable_features[] = {
      "C",
};
const char* const array_kFieldTrialConfig_enable_features[] = {
      "A",
      "B",
};
const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params[] = {
      {
        "x",
        "1",
      },
      {
        "y",
        "2",
      },
};
const Study::Platform array_kFieldTrialConfig_platforms_0[] = {
      Study::PLATFORM_WINDOWS,
};
const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_0[] = {
    {
      "TestGroup2",
      array_kFieldTrialConfig_platforms_0,
      {},
      std::nullopt,
      nullptr,
      array_kFieldTrialConfig_params,
      array_kFieldTrialConfig_enable_features,
      array_kFieldTrialConfig_disable_features,
      nullptr,
      {},
      {},
      {},
    },
    {
      "TestGroup2-2",
      array_kFieldTrialConfig_platforms_1,
      {},
      std::nullopt,
      nullptr,
      array_kFieldTrialConfig_params_0,
      array_kFieldTrialConfig_enable_features_0,
      array_kFieldTrialConfig_disable_features_0,
      nullptr,
      {},
      {},
      {},
    },
};
const Study::Platform array_kFieldTrialConfig_platforms[] = {
      Study::PLATFORM_WINDOWS,
};
const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
    {
      "TestGroup1",
      array_kFieldTrialConfig_platforms,
      {},
      std::nullopt,
      nullptr,
      {},
      {},
      {},
      nullptr,
      {},
      {},
      {},
    },
};
const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
  {
    "TestTrial1",
    array_kFieldTrialConfig_experiments,
  },
  {
    "TestTrial2",
    array_kFieldTrialConfig_experiments_0,
  },
  {
    "TestTrial3",
    array_kFieldTrialConfig_experiments_1,
  },
  {
    "TrialWithDeviceType",
    array_kFieldTrialConfig_experiments_2,
  },
  {
    "TrialWithForcingFlag",
    array_kFieldTrialConfig_experiments_3,
  },
  {
    "TrialWithFormFactors",
    array_kFieldTrialConfig_experiments_4,
  },
  {
    "TrialWithOverrideUIString",
    array_kFieldTrialConfig_experiments_5,
  },
};
const FieldTrialTestingConfig kFieldTrialConfig = {
  array_kFieldTrialConfig_studies,
};
