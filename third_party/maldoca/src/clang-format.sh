# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Runs clang-format and show diff.

clang-format --version

EXIT_CODE=0

for FILE in maldoca/base/* \
            maldoca/base/testing/* \
            maldoca/base/utf8/* \
            maldoca/pdf_parser/* \
            maldoca/pdf_parser/proto/* \
            maldoca/service/* \
            maldoca/service/common/* \
            maldoca/service/common/proto/* \
            maldoca/vba/* \
            maldoca/vba/antivirus/*; do
  case $FILE in
    *.h|*.cc|*.proto)
      if [ "$1" = "diff" ]
      then
            # Run clang-format, then compare the output.
            clang-format --verbose $FILE | diff --color $FILE -
      else
            # Run clang-format to format code.
            clang-format --verbose -i $FILE
      fi

      # If diff found any difference, its exit code would be non-zero.
      # In such case, we set our exit code to 1.
      [ $? -eq 0 ] || EXIT_CODE=1

      ;;
  esac
done

exit $EXIT_CODE
