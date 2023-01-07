// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "build_tools/embed_data/testembed1.h"
#include "build_tools/embed_data/testembed2.h"
#include "iree/testing/gtest.h"

namespace {

TEST(Generator, TestContents) {
  auto* toc1 = ::foobar::testembed1_create();
  ASSERT_EQ("file1.txt", std::string(toc1->name));
  ASSERT_EQ(R"(Are you '"Still"' here?)"
            "\n",
            std::string(toc1->data));
  ASSERT_EQ(24, toc1->size);
  ASSERT_EQ(0, *(toc1->data + toc1->size));

  ++toc1;
  ASSERT_EQ("file2.txt", std::string(toc1->name));
  ASSERT_EQ(R"(¯\_(ツ)_/¯)"
            "\n",
            std::string(toc1->data));
  ASSERT_EQ(14, toc1->size);
  ASSERT_EQ(0, *(toc1->data + toc1->size));

  ++toc1;
  ASSERT_EQ(nullptr, toc1->name);
  ASSERT_EQ(nullptr, toc1->data);

  auto* toc2 = ::foobar::testembed2_create();
  ASSERT_EQ("file3.txt", std::string(toc2->name));
  ASSERT_EQ(R"(ᕕ( ᐛ )ᕗ)"
            "\n",
            std::string(toc2->data));
  ASSERT_EQ(14, toc2->size);
  ASSERT_EQ(0, *(toc2->data + toc2->size));
}

}  // namespace
