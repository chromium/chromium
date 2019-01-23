// Copyright (c) 2016-2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "../C/pyramid_roi_align.h"
#include "primitive.hpp"

using namespace std;

namespace cldnn {

    struct pyramid_roi_align : public primitive_base<pyramid_roi_align, CLDNN_PRIMITIVE_DESC(pyramid_roi_align)>
    {
        CLDNN_DECLARE_PRIMITIVE(pyramid_roi_align)

        pyramid_roi_align(
            const primitive_id& id,
            const primitive_id& input,
            const padding& output_padding = padding()
        )
         : primitive_base(id, { input }, output_padding)
        {}

        pyramid_roi_align(
            const primitive_id &id_c,
            const primitive_id &base_str,
            const primitive_id &meta_str,
            const primitive_id &P2_str,
            const primitive_id &P3_str,
            const primitive_id &P4_str,
            const primitive_id &P5_str,
            const primitive_id &pool_size_str,
            const padding& output_padding = padding()
        )
            : primitive_base(std::string(id_c), { 
                    base_str, meta_str, P2_str, P3_str,
                    P4_str, P5_str, pool_size_str},
                    output_padding)
        {}

        /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{broadcast}
        pyramid_roi_align(const dto* dto)
            : primitive_base(dto)

        {}

    protected:
        void update_dto(dto &) const override
        {}

    };
}