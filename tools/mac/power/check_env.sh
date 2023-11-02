#!/bin/zsh

# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -eu

function SystemProfilerProperty()
{
  local result=$2
  local local_result=$(system_profiler $3| grep -i $1 |\
    cut -d ":" -f 2 | awk '{$1=$1};1')
  eval $result="'$local_result'"
}

function GetPowerProperty()
{
  SystemProfilerProperty $1 $2 "SPPowerDataType"
}

function GetDisplayProperty()
{
  SystemProfilerProperty $1 $2 "SPDisplaysDataType"
}

function CompareValue()
{
  if [ "$1" != "$2" ]; then
    echo $3
    exit 127
  fi
}

CheckPowerValue()
{
  # Query value, remove newlines.
  GetPowerProperty $1 VALUE
  VALUE=$(echo $VALUE|tr -d '\n')

  CompareValue $VALUE $2 $3
}

CheckDisplayValue()
{
  # Query value, remove newlines.
  GetDisplayProperty $1 VALUE
  VALUE=$(echo $VALUE|tr -d '\n')

  CompareValue $VALUE $2 $3
}

function CheckProgramNotRunning(){
  if pgrep -x "$1" > /dev/null; then
    echo "$2"
    exit 127
  fi
}

function CheckEnv()
{
  # Validate power setup.
  CheckPowerValue "charging" "NoNo" "Laptop cannot be charging during test."
  CheckPowerValue "connected" "No" "Charger cannot be connected during test."

  # Validate display setup.
  CheckDisplayValue "Automatically adjust brightness" "No"\
    "Disable automatic brightness adjustments and unplug external monitors"

  # Verify that no terminals are running.
  # They introduce too much overhead. (As measured with powermetrics)
  CheckProgramNotRunning "Terminal" "Do not have a terminal opened. Use SSH.";
  CheckProgramNotRunning "iTerm2" "Do not have a terminal opened. Use SSH.";

}

CheckEnv
