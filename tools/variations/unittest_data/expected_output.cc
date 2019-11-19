// Copyright 2015 The Chromium Authors. All rights reserved.
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
const Study::FormFactor array_kFieldTrialConfig_form_factors_8[] = {
};
const Study::Platform array_kFieldTrialConfig_platforms_8[] = {
      Study::PLATFORM_WINDOWS,
};
const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_5[] = {
    {
      "TestGroup",
      array_kFieldTrialConfig_platforms_8,
      1,
      array_kFieldTrialConfig_form_factors_8,
      0,
      Study::OPTIONAL_BOOL_MISSING,
      NULL,
      0,
      NULL,
      0,
      NULL,
      0,
      NULL,
      array_kFieldTrialConfig_override_ui_string,
      1,
    },
};
const Study::FormFactor array_kFieldTrialConfig_form_factors_7[] = {
      Study::TABLET,
};
const Study::Platform array_kFieldTrialConfig_platforms_7[] = {
      Study::PLATFORM_WINDOWS,
};
const Study::FormFactor array_kFieldTrialConfig_form_factors_6[] = {
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
      1,
      array_kFieldTrialConfig_form_factors_6,
      2,
      Study::OPTIONAL_BOOL_MISSING,
      NULL,
      0,
      NULL,
      0,
      NULL,
      0,
      NULL,
      NULL,
      0,
    },
    {
      "TestGroup2",
      array_kFieldTrialConfig_platforms_7,
      1,
      array_kFieldTrialConfig_form_factors_7,
      1,
      Study::OPTIONAL_BOOL_MISSING,
      NULL,
      0,
      NULL,
      0,
      NULL,
      0,
      NULL,
      NULL,
      0,
    },
};
const Study::FormFactor array_kFieldTrialConfig_form_factors_5[] = {
};
const Study::Platform array_kFieldTrialConfig_platforms_5[] = {
      Study::PLATFORM_WINDOWS,
};
const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_3[] = {
    {
      "ForcedGroup",
      array_kFieldTrialConfig_platforms_5,
      1,
      array_kFieldTrialConfig_form_factors_5,
      0,
      Study::OPTIONAL_BOOL_MISSING,
      NULL,
      0,
      NULL,
      0,
      NULL,
      0,
      "my-forcing-flag",
      NULL,
      0,
    },
};
const Study::FormFactor array_kFieldTrialConfig_form_factors_4[] = {
};
const Study::Platform array_kFieldTrialConfig_platforms_4[] = {
      Study::PLATFORM_WINDOWS,
};
const Study::FormFactor array_kFieldTrialConfig_form_factors_3[] = {
};
const Study::Platform array_kFieldTrialConfig_platforms_3[] = {
      Study::PLATFORM_WINDOWS,
};
const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_2[] = {
    {
      "TestGroup1",
      array_kFieldTrialConfig_platforms_3,
      1,
      array_kFieldTrialConfig_form_factors_3,
      0,
      Study::OPTIONAL_BOOL_TRUE,
      NULL,
      0,
      NULL,
      0,
      NULL,
      0,
      NULL,
      NULL,
      0,
    },
    {
      "TestGroup2",
      array_kFieldTrialConfig_platforms_4,
      1,
      array_kFieldTrialConfig_form_factors_4,
      0,
      Study::OPTIONAL_BOOL_FALSE,
      NULL,
      0,
      NULL,
      0,
      NULL,
      0,
      NULL,
      NULL,
      0,
    },
};
const char* const array_kFieldTrialConfig_enable_features_1[] = {
      "X",
};
const Study::FormFactor array_kFieldTrialConfig_form_factors_2[] = {
};
const Study::Platform array_kFieldTrialConfig_platforms_2[] = {
      Study::PLATFORM_WINDOWS,
};
const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_1[] = {
    {
      "TestGroup3",
      array_kFieldTrialConfig_platforms_2,
      1,
      array_kFieldTrialConfig_form_factors_2,
      0,
      Study::OPTIONAL_BOOL_MISSING,
      NULL,
      0,
      array_kFieldTrialConfig_enable_features_1,
      1,
      NULL,
      0,
      NULL,
      NULL,
      0,
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
const Study::FormFactor array_kFieldTrialConfig_form_factors_1[] = {
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
const Study::FormFactor array_kFieldTrialConfig_form_factors_0[] = {
};
const Study::Platform array_kFieldTrialConfig_platforms_0[] = {
      Study::PLATFORM_WINDOWS,
};
const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_0[] = {
    {
      "TestGroup2",
      array_kFieldTrialConfig_platforms_0,
      1,
      array_kFieldTrialConfig_form_factors_0,
      0,
      Study::OPTIONAL_BOOL_MISSING,
      array_kFieldTrialConfig_params,
      2,
      array_kFieldTrialConfig_enable_features,
      2,
      array_kFieldTrialConfig_disable_features,
      1,
      NULL,
      NULL,
      0,
    },
    {
      "TestGroup2-2",
      array_kFieldTrialConfig_platforms_1,
      1,
      array_kFieldTrialConfig_form_factors_1,
      0,
      Study::OPTIONAL_BOOL_MISSING,
      array_kFieldTrialConfig_params_0,
      2,
      array_kFieldTrialConfig_enable_features_0,
      2,
      array_kFieldTrialConfig_disable_features_0,
      1,
      NULL,
      NULL,
      0,
    },
};
const Study::FormFactor array_kFieldTrialConfig_form_factors[] = {
};
const Study::Platform array_kFieldTrialConfig_platforms[] = {
      Study::PLATFORM_WINDOWS,
};
const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
    {
      "TestGroup1",
      array_kFieldTrialConfig_platforms,
      1,
      array_kFieldTrialConfig_form_factors,
      0,
      Study::OPTIONAL_BOOL_MISSING,
      NULL,
      0,
      NULL,
      0,
      NULL,
      0,
      NULL,
      NULL,
      0,
    },
};
const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
  {
    "TestTrial1",
    array_kFieldTrialConfig_experiments,
    1,
  },
  {
    "TestTrial2",
    array_kFieldTrialConfig_experiments_0,
    2,
  },
  {
    "TestTrial3",
    array_kFieldTrialConfig_experiments_1,
    1,
  },
  {
    "TrialWithDeviceType",
    array_kFieldTrialConfig_experiments_2,
    2,
  },
  {
    "TrialWithForcingFlag",
    array_kFieldTrialConfig_experiments_3,
    1,
  },
  {
    "TrialWithFormFactors",
    array_kFieldTrialConfig_experiments_4,
    2,
  },
  {
    "TrialWithOverrideUIString",
    array_kFieldTrialConfig_experiments_5,
    1,
  },
};
const FieldTrialTestingConfig kFieldTrialConfig = {
  array_kFieldTrialConfig_studies,
  7,
};
