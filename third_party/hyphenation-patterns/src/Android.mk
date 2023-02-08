# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#disable build in PDK
ifneq ($(TARGET_BUILD_PDK),true)

LOCAL_PATH := $(call my-dir)

pattern_locales := \
    af/af \
    am/mul-ethi \
    as/as \
    be/be \
    bn/bn \
    bg/bg \
    cs/cs \
    cu/cu \
    cy/cy \
    da/da \
    de/de-1901 \
    de/de-1996 \
    de/de-ch-1901 \
    el/el \
    en-GB/en-gb \
    en-US/en-us \
    es/es \
    et/et \
    eu/eu \
    Ethi/und-ethi \
    fr/fr \
    ga/ga \
    gl/gl \
    gu/gu \
    hi/hi \
    hr/hr \
    hu/hu \
    hy/hy \
    it/it \
    ka/ka \
    kn/kn \
    la/la \
    lt/lt \
    lv/lv \
    ml/ml \
    mn/mn-cyrl \
    mr/mr \
    nb/nb \
    nl/nl \
    nn/nn \
    or/or \
    pa/pa \
    pt/pt \
    ru/ru \
    sk/sk \
    sl/sl \
    sq/sq \
    sv/sv \
    ta/ta \
    te/te \
    tk/tk \
    uk/uk

# TODO: we have data for sa/sa, but it requires special case handling for case
# folding and normalization, so don't build it until that's fixed.
#
# TODO: we have data for Liturgical Latin, but there is no standard BCP 47 tag
# for it, so we don't build it.

BUILD_HYB := $(LOCAL_PATH)/build-hyb.mk

#############################################################################
# $(1): The subdirectory where the source files live.
$ $(2): The file name fragment.
#       It is used to find source files, and also generate the resulting binary.
#############################################################################
define build-one-pattern-module
$(eval include $(CLEAR_VARS))\
$(eval LOCAL_MODULE := $(addprefix hyph-, $(2)))\
$(eval LOCAL_SRC_FILES := $(addprefix $(1)/hyph-, $(addprefix $(2), .pat.txt .chr.txt .hyp.txt)))\
$(eval LOCAL_LICENSE_KINDS := SPDX-license-identifier-MIT SPDX-license-identifier-BSD-3-Clause SPDX-license-identifier-FSFAP SPDX-license-identifier-Unicode-DFS SPDX-license-identifier-LGPL-2.1 SPDX-license-identifier-MPL)\
$(eval LOCAL_LICENSE_CONDITIONS := notice reciprocal restricted)\
$(eval LOCAL_NOTICE_FILE := $(wildcard $(addprefix $(LOCAL_PATH)/$(1), /NOTICE /LICENSE)))\
$(eval include $(BUILD_HYB))\
$(eval include $(CLEAR_VARS))\
$(eval LOCAL_MODULE := $(addprefix $(addprefix hyph-, $(2)), .lic.txt))\
$(eval LOCAL_SRC_FILES := $(addprefix $(1)/hyph-, $(addprefix $(2), .lic.txt)))\
$(eval LOCAL_MODULE_CLASS := ETC)\
$(eval LOCAL_MODULE_TAGS := optional)\
$(eval LOCAL_MODULE_PATH := $(TARGET_OUT)/usr/hyphen-data)\
$(eval include $(BUILD_PREBUILT))
endef

$(foreach l, $(pattern_locales), $(call build-one-pattern-module,$(dir $(l)),$(notdir $l)))
build-one-pattern-module :=
pattern_locales :=

endif #TARGET_BUILD_PDK
