/*******************************************************************************
* Copyright 2016-2019 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

/// @file
/// C API types definitions

#ifndef DNNL_TYPES_H
#define DNNL_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/// @cond DO_NOT_DOCUMENT_THIS
#include <stddef.h>
#include <stdint.h>
/// @endcond

/// @addtogroup dnnl_api
/// @{

/// @addtogroup dnnl_api_utils
/// @{

/// Status values returned by the library functions.
typedef enum {
    /// The operation was successful
    dnnl_success = 0,
    /// The operation failed due to an out-of-memory condition
    dnnl_out_of_memory = 1,
    /// The operation failed because of incorrect function arguments
    dnnl_invalid_arguments = 2,
    /// The operation failed because requested functionality is not implemented
    dnnl_unimplemented = 3,
    /// Primitive iterator passed over last primitive descriptor
    dnnl_iterator_ends = 4,
    /// Primitive or engine failed on execution
    dnnl_runtime_error = 5,
    /// Queried element is not required for given primitive
    dnnl_not_required = 6,
} dnnl_status_t;

/// @} dnnl_api_utils

/// @addtogroup dnnl_api_memory
/// @{

/// Data type specification
typedef enum {
    /// Undefined data type, used for empty memory descriptors.
    dnnl_data_type_undef = 0,
    /// 16-bit/half-precision floating point.
    dnnl_f16 = 1,
    /// non-standard 16-bit (bfloat16 w/ 7 bit mantissa) floating point.
    dnnl_bf16 = 2,
    /// 32-bit/single-precision floating point.
    dnnl_f32 = 3,
    /// 32-bit signed integer.
    dnnl_s32 = 4,
    /// 8-bit signed integer.
    dnnl_s8 = 5,
    /// 8-bit unsigned integer.
    dnnl_u8 = 6,
} dnnl_data_type_t;

/// Memory format kind
typedef enum {
    /// Undefined memory format kind, used for empty memory descriptors.
    dnnl_format_kind_undef = 0,
    /// Unspecified format kind.
    /// The primitive selects a format automatically.
    dnnl_format_kind_any,
    /// A tensor in a generic format described by the stride and blocking
    /// values in each dimension. See @ref dnnl_blocking_desc_t for more
    /// information.
    dnnl_blocked,
    /// Weights format used in 8bit Winograd convolution
    dnnl_format_kind_wino,
    /// Packed weights format used in RNN
    dnnl_format_kind_rnn_packed,
} dnnl_format_kind_t;

/// Memory format tag specification.
///
/// DNNL formats describe physical data layout. The physical layout
/// is described as a sequence of the dimensions as they are laid out in the
/// memory (from the outer-most to the inner-most). Note that this order
/// doesn't affect the logical order of the dimensions that is kept in the
/// `dims` field of the dnnl_memory_desc_t structure. The logical order of the
/// dimensions is specified by the primitive that uses the tensor.
///
/// For example, CNN 5D tensor always has its logical dimensions in the order
/// `(batch, channels, depth, height, width)`, while the physical layout might be
/// `NCDHW` (corresponds to #dnnl_ncdhw format tag) or
/// `NDHWC` (corresponds to #dnnl_ndhwc format tag).
///
/// ~~~cpp
/// int batch = 2, channels = 16, depth = 13, height = 13, width = 13;
///
/// int ndims = 5; // 5D tensor
/// dnnl_dims_t dims = {batch, channels, depth, height, width};
/// dnnl_memory_desc_t data_in_ncdhw;
/// dnnl_memory_desc_init_by_tag(
///      &data_in_ncdhw, 5, dims, dnnl_f32, dnnl_ncdhw);
///
/// // note that in both cases dims passed are the same
/// dnnl_memory_desc_t data_in_ndhwc;
/// dnnl_memory_desc_init_by_tag(
///      &data_in_ndhwc, 5, dims, dnnl_f32, dnnl_ndhwc);
/// ~~~
///
/// Memory format tags can be further divided into two categories:
///  - Domain-agnostic names, i.e. names the do not depend on the tensor usage
///    in the specific primitive. These names use letters from `a` to `l` to
///    denote logical dimension from 1 to 12, and form the order in which the
///    dimensions are laid in memory. For instance, #dnnl_ab is used to denote
///    2D tensor where the second logical dimension (aka `b`) is the innermost,
///    i.e. has stride = 1, and the first logical dimension (`a`) laid out in
///    memory with stride equal to the size of second dimension. On the other
///    hand, #dnnl_ba is just transposed version of the same tensor: the
///    first dimension (`a`) becomes the innermost one.
///  - Domain-specific names, i.e. names that make sense only in the context of
///    a certain domain, such as CNN. This names are just aliases to the
///    corresponding domain-agnostic tags and used mostly for the convenience.
///    For example, #dnnl_nc is used to denote 2D CNN activations tensor
///    memory format, where channels are the innermost dimension and batch is an
///    outermost one. Moreover, #dnnl_nc is just an alias to #dnnl_ab,
///    since for DNNL CNN primitives the logical dimensions of
///    activations tensors come in order: batch, channels, spatial.
///    In other words, batch corresponds to the first logical dimension (`a`),
///    channels correspond to the second one (`b`).
///
/// The following domain-specific notation applies to memory format tags:
///  - @c 'n' denotes the mini-batch dimension
///  - @c 'c' denotes a channels dimension
///  - When there are multiple channel dimensions (for example, in convolution
///    weights tensor), @c 'i' and @c 'o' denote dimensions of input and output
///    channels
///  - @c 'd', @c 'h', and @c 'w' denote spatial depth, height, and width
///    respectively
///
/// Upper-case letters indicate that the data is laid out in blocks for a
/// particular dimension. In such cases, the format name contains both upper-
/// and lower-case letters for that dimension with a lower-case letter preceded
/// by the block size. For example: #dnnl_nChw8c describes a format where the
/// outermost dimension is mini-batch, followed by the channel block number,
/// followed by the spatial height and width, and finally followed by 8-element
/// channel blocks.
///
/// @sa @ref dev_guide_understanding_memory_formats
typedef enum {
    /// Undefined memory format tag
    dnnl_format_tag_undef = 0,
    /// Undefined memory format tag.
    /// The primitive selects a format automatically.
    dnnl_format_tag_any,

    // Semantic agnostic section
    // The physical order of dimensions is defined by the permutation of the
    // characters, assuming that ab..z defines the natural order.

    // Plain formats

    dnnl_a, ///< plain 1D tensor
    dnnl_ab, ///< plain 2D tensor
    dnnl_abc, ///< plain 3D tensor
    dnnl_abcd, ///< plain 4D tensor
    dnnl_abcde, ///< plain 5D tensor
    dnnl_abcdef, ///< plain 6D tensor

    // Permuted plain formats

    dnnl_abdec, ///< permuted 5D tensor
    dnnl_acb, ///< permuted 3D tensor
    dnnl_acbde, ///< permuted 5D tensor
    dnnl_acbdef, ///< permuted 6D tensor
    dnnl_acdb, ///< permuted 4D tensor
    dnnl_acdeb, ///< permuted 5D tensor
    dnnl_ba, ///< permuted 2D tensor
    dnnl_bac, ///< permuted 3D tensor
    dnnl_bacd, ///< permuted 4D tensor
    dnnl_bca, ///< permuted 3D tensor
    dnnl_bcda, ///< permuted 4D tensor
    dnnl_bcdea, ///< permuted 5D tensor
    dnnl_cba, ///< permuted 3D tensor
    dnnl_cdba, ///< permuted 4D tensor
    dnnl_cdeba, ///< permuted 5D tensor
    dnnl_decab, ///< permuted 5D tensor
    dnnl_defcab, ///< permuted 6D tensor

    // Opaque blocked formats

    dnnl_Abc16a,
    dnnl_ABc16a16b,
    dnnl_ABc4a4b,
    /// 3D tensor blocked by 2nd dimension with block size 16
    dnnl_aBc16b,
    dnnl_ABc16b16a,
    dnnl_Abc4a,
    /// 3D tensor blocked by 2nd dimension with block size 4
    dnnl_aBc4b,
    dnnl_ABc4b16a4b,
    dnnl_ABc2b8a4b,
    dnnl_ABc4b4a,
    dnnl_ABc8a16b2a,
    dnnl_ABc8a8b,
    /// 3D tensor blocked by 2nd dimension with block size 8
    dnnl_aBc8b,
    dnnl_ABc8b16a2b,
    dnnl_BAc8a16b2a,
    dnnl_ABc8b8a,
    dnnl_Abcd16a,
    dnnl_Abcd8a,
    dnnl_ABcd16a16b,
    dnnl_ABcd32a32b,
    /// 4D tensor blocked by 2nd dimension with block size 16
    dnnl_aBcd16b,
    dnnl_ABcd16b16a,
    dnnl_aBCd16b16c,
    dnnl_aBCd16c16b,
    dnnl_Abcd4a,
    /// 4D tensor blocked by 2nd dimension with block size 4
    dnnl_aBcd4b,
    dnnl_ABcd4b16a4b,
    dnnl_ABcd4b4a,
    dnnl_ABcd4a4b,
    dnnl_aBCd4c16b4c,
    dnnl_aBCd2c8b4c,
    dnnl_aBCd4c4b,
    dnnl_aBCd4b4c,
    dnnl_ABcd8a16b2a,
    dnnl_ABcd2b8a4b,
    dnnl_ABcd8a8b,
    /// 4D tensor blocked by 2nd dimension with block size 8
    dnnl_aBcd8b,
    dnnl_ABcd8b16a2b,
    dnnl_aBCd8b16c2b,
    dnnl_BAcd8a16b2a,
    /// 4D tensor blocked by 1st and 2nd dimension with block size 8
    dnnl_ABcd8b8a,
    dnnl_aBCd8b8c,
    dnnl_aBCd8c16b2c,
    dnnl_ABcde8a16b2a,
    dnnl_aCBd8b16c2b,
    dnnl_aBCd8c8b,
    dnnl_Abcde16a,
    dnnl_ABcde16a16b,
    dnnl_BAcde8a16b2a,
    /// 5D tensor blocked by 1st dimension with block size 16
    dnnl_ABcde4b16a4b,
    /// 5D tensor blocked by 1st dimension with block size 8
    dnnl_ABcde2b8a4b,
    /// 5D tensor blocked by 2nd dimension with block size 16
    dnnl_aBcde16b,
    dnnl_ABcde16b16a,
    dnnl_aBCde16b16c,
    dnnl_aBCde16c16b,
    dnnl_aBCde2c8b4c,
    dnnl_Abcde4a,
    /// 5D tensor blocked by 2nd dimension with block size 4
    dnnl_aBcde4b,
    dnnl_ABcde4b4a,
    dnnl_ABcde4a4b,
    dnnl_aBCde4b4c,
    dnnl_aBCde4c16b4c,
    dnnl_aBCde4c4b,
    dnnl_Abcde8a,
    dnnl_ABcde8a8b,
    dnnl_BAcde16b16a,
    /// 5D tensor blocked by 2nd dimension with block size 8
    dnnl_aBcde8b,
    dnnl_ABcde8b16a2b,
    dnnl_aBCde8b16c2b,
    dnnl_aCBde8b16c2b,
    dnnl_ABcde8b8a,
    dnnl_aBCde8b8c,
    dnnl_ABcd4a8b8a4b,
    dnnl_ABcd2a8b8a2b,
    dnnl_aBCde4b8c8b4c,
    dnnl_aBCde2b8c8b2c,
    dnnl_aBCde8c16b2c,
    dnnl_aBCde8c8b,
    /// 6D tensor blocked by 2nd dimension with block size 16
    dnnl_aBcdef16b,
    dnnl_aBCdef16b16c,
    dnnl_aBCdef16c16b,
    dnnl_aBCdef4c16b4c,
    /// 6D tensor blocked by 2nd dimension with block size 8
    dnnl_aBCdef2c8b4c,
    /// 6D tensor blocked by 2nd dimension with block size 4
    dnnl_aBcdef4b,
    dnnl_aBCdef4c4b,
    dnnl_aBCdef4b4c,
    dnnl_aBCdef8b8c,
    dnnl_aBCdef8c16b2c,
    dnnl_aBCdef8b16c2b,
    dnnl_aCBdef8b16c2b,
    dnnl_aBCdef8c8b,
    dnnl_aBdc16b,
    dnnl_aBdC16b2c,
    dnnl_aBdc4b,
    dnnl_aBdc8b,
    dnnl_aBdec16b,
    dnnl_aBdeC16b2c,
    dnnl_aBdec32b,
    dnnl_aBdec4b,
    dnnl_aBdec8b,
    dnnl_aBdefc16b,
    dnnl_aBdefC16b2c,
    dnnl_aCBdef16c16b,
    dnnl_aBdefc4b,
    dnnl_aBdefc8b,
    dnnl_Abcdef16a,
    dnnl_Acb16a,
    dnnl_AcB16a2b,
    dnnl_Acb4a,
    dnnl_Acb8a,
    dnnl_aCBd16b16c,
    dnnl_aCBd16c16b,
    dnnl_aCBde16b16c,
    dnnl_aCBde16c16b,
    dnnl_Acdb16a,
    dnnl_AcdB16a2b,
    dnnl_Acdb32a,
    dnnl_Acdb4a,
    dnnl_Acdb8a,
    dnnl_Acdeb16a,
    dnnl_AcdeB16a2b,
    dnnl_Acdeb4a,
    dnnl_Acdeb8a,
    dnnl_BAc16a16b,
    dnnl_BAc16b16a,
    dnnl_BAcd16a16b,
    dnnl_BAcd16b16a,

    /// Just a sentinel, not real memory format tag. Must be changed after new
    /// format tag is added.
    dnnl_format_tag_last,

    // Aliases

    /// 1D tensor, an alias to #dnnl_a
    dnnl_x = dnnl_a,
    /// 2D CNN activations tensor, an alias to #dnnl_ab
    dnnl_nc = dnnl_ab,
    /// 2D CNN activations tensor, an alias to #dnnl_ba
    dnnl_cn = dnnl_ba,
    /// 2D RNN statistics tensor, an alias to #dnnl_ab
    dnnl_tn = dnnl_ab,
    /// 2D RNN statistics tensor, an alias to #dnnl_ba
    dnnl_nt = dnnl_ba,
    /// 3D CNN activations tensor, an alias to #dnnl_abc
    dnnl_ncw = dnnl_abc,
    /// 3D CNN activations tensor, an alias to #dnnl_acb
    dnnl_nwc = dnnl_acb,
    /// 4D CNN activations tensor, an alias to #dnnl_abcd
    dnnl_nchw = dnnl_abcd,
    /// 4D CNN activations tensor, an alias to #dnnl_acdb
    dnnl_nhwc = dnnl_acdb,
    /// 4D CNN activations tensor, an alias to #dnnl_bcda
    dnnl_chwn = dnnl_bcda,
    /// 5D CNN activations tensor, an alias to #dnnl_abcde
    dnnl_ncdhw = dnnl_abcde,
    /// 5D CNN activations tensor, an alias to #dnnl_acdeb
    dnnl_ndhwc = dnnl_acdeb,

    /// 2D CNN weights tensor, an alias to #dnnl_ab
    dnnl_oi = dnnl_ab,
    /// 2D CNN weights tensor, an alias to #dnnl_ba
    dnnl_io = dnnl_ba,
    /// 3D CNN weights tensor, an alias to #dnnl_abc
    dnnl_oiw = dnnl_abc,
    /// 3D CNN weights tensor, an alias to #dnnl_acb
    dnnl_owi = dnnl_acb,
    /// 3D CNN weights tensor, an alias to #dnnl_cba
    dnnl_wio = dnnl_cba,
    /// 3D CNN weights tensor, an alias to #dnnl_bca
    dnnl_iwo = dnnl_bca,
    /// 4D CNN weights tensor, an alias to #dnnl_abcd
    dnnl_oihw = dnnl_abcd,
    /// 4D CNN weights tensor, an alias to #dnnl_cdba
    dnnl_hwio = dnnl_cdba,
    /// 4D CNN weights tensor, an alias to #dnnl_acdb
    dnnl_ohwi = dnnl_acdb,
    /// 4D CNN weights tensor, an alias to #dnnl_bcda
    dnnl_ihwo = dnnl_bcda,
    /// 4D CNN weights tensor, an alias to #dnnl_bacd
    dnnl_iohw = dnnl_bacd,
    /// 5D CNN weights tensor, an alias to #dnnl_abcde
    dnnl_oidhw = dnnl_abcde,
    /// 5D CNN weights tensor, an alias to #dnnl_cdeba
    dnnl_dhwio = dnnl_cdeba,
    /// 5D CNN weights tensor, an alias to #dnnl_acdeb
    dnnl_odhwi = dnnl_acdeb,
    /// 5D CNN weights tensor, an alias to #dnnl_bcdea
    dnnl_idhwo = dnnl_bcdea,

    /// 4D CNN weights tensor (incl. groups), an alias to #dnnl_abcd
    dnnl_goiw = dnnl_abcd,
    /// 5D CNN weights tensor (incl. groups), an alias to #dnnl_abcde
    dnnl_goihw = dnnl_abcde,
    /// 5D CNN weights tensor (incl. groups), an alias to #dnnl_decab
    dnnl_hwigo = dnnl_decab,
    /// 5D CNN weights tensor (incl. groups), an alias to #dnnl_acbde
    dnnl_giohw = dnnl_acbde,
    /// 6D CNN weights tensor (incl. groups), an alias to #dnnl_abcdef
    dnnl_goidhw = dnnl_abcdef,
    /// 6D CNN weights tensor (incl. groups), an alias to #dnnl_acbdef
    dnnl_giodhw = dnnl_acbdef,
    /// 6D CNN weights tensor (incl. groups), an alias to #dnnl_defcab
    dnnl_dhwigo = dnnl_defcab,

    /// 3D RNN data tensor in the format (seq_length, batch, input channels).
    dnnl_tnc = dnnl_abc,
    /// 3D RNN data tensor in the format (batch, seq_length, input channels).
    dnnl_ntc = dnnl_bac,
    /// 4D RNN states tensor in the format (num_layers, num_directions,
    /// batch, state channels).
    dnnl_ldnc = dnnl_abcd,
    /// 5D RNN weights tensor in the format (num_layers, num_directions,
    ///  input_channels, num_gates, output_channels).
    ///
    ///  - For LSTM cells, the gates order is input, forget, candidate
    ///    and output gate.
    ///  - For GRU cells, the gates order is update, reset and output gate.
    dnnl_ldigo = dnnl_abcde,
    /// 5D RNN weights tensor in the format (num_layers, num_directions,
    /// num_gates, output_channels, input_channels).
    ///
    ///  - For LSTM cells, the gates order is input, forget, candidate
    ///    and output gate.
    ///  - For GRU cells, the gates order is update, reset and output gate.
    dnnl_ldgoi = dnnl_abdec,
    /// 4D RNN bias tensor in the format (num_layers, num_directions,
    /// num_gates, output_channels).
    ///
    ///  - For LSTM cells, the gates order is input, forget, candidate
    ///    and output gate.
    ///  - For GRU cells, the gates order is update, reset and output gate.
    dnnl_ldgo = dnnl_abcd,

    // Opaque data types, are not to be used explicitly

    // data

    /// 5D CNN activations tensor blocked by channels with block size 16,
    /// an alias to #dnnl_aBcde16b
    dnnl_nCdhw16c = dnnl_aBcde16b,
    /// 5D CNN activations tensor blocked by channels with block size 4,
    /// an alias to #dnnl_aBcde4b
    dnnl_nCdhw4c = dnnl_aBcde4b,
    /// 5D CNN activations tensor blocked by channels with block size 8,
    /// an alias to #dnnl_aBcde8b
    dnnl_nCdhw8c = dnnl_aBcde8b,
    /// 4D CNN activations tensor blocked by channels with block size 16,
    /// an alias to #dnnl_aBcd16b
    dnnl_nChw16c = dnnl_aBcd16b,
    /// 4D CNN activations tensor blocked by channels with block size 4,
    /// an alias to #dnnl_aBcd4b
    dnnl_nChw4c = dnnl_aBcd4b,
    /// 4D CNN activations tensor blocked by channels with block size 8,
    /// an alias to #dnnl_aBcd8b
    dnnl_nChw8c = dnnl_aBcd8b,
    /// 3D CNN activations tensor blocked by channels with block size 16,
    /// an alias to #dnnl_aBc16b
    dnnl_nCw16c = dnnl_aBc16b,
    /// 3D CNN activations tensor blocked by channels with block size 4,
    /// an alias to #dnnl_aBc4b
    dnnl_nCw4c = dnnl_aBc4b,
    /// 3D CNN activations tensor blocked by channels with block size 8,
    /// an alias to #dnnl_aBc8b
    dnnl_nCw8c = dnnl_aBc8b,
    dnnl_NCw16n16c = dnnl_ABc16a16b,
    dnnl_NCdhw16n16c = dnnl_ABcde16a16b,
    dnnl_NChw16n16c = dnnl_ABcd16a16b,
    dnnl_NChw32n32c = dnnl_ABcd32a32b,

    // weights, 3D
    dnnl_IOw16o16i = dnnl_BAc16a16b,
    dnnl_IOw16i16o = dnnl_BAc16b16a,
    dnnl_OIw16i16o = dnnl_ABc16b16a,
    dnnl_OIw16o16i = dnnl_ABc16a16b,
    dnnl_Oiw16o = dnnl_Abc16a,
    dnnl_OIw4i16o4i = dnnl_ABc4b16a4b,
    dnnl_OIw2i8o4i = dnnl_ABc2b8a4b,
    dnnl_OIw4i4o = dnnl_ABc4b4a,
    dnnl_OIw4o4i = dnnl_ABc4a4b,
    dnnl_Oiw4o = dnnl_Abc4a,
    dnnl_OIw8i16o2i = dnnl_ABc8b16a2b,
    dnnl_OIw8i8o = dnnl_ABc8b8a,
    dnnl_OIw8o16i2o = dnnl_ABc8a16b2a,
    dnnl_IOw8o16i2o = dnnl_BAc8a16b2a,
    dnnl_OIw8o8i = dnnl_ABc8a8b,
    dnnl_Owi16o = dnnl_Acb16a,
    dnnl_OwI16o2i = dnnl_AcB16a2b,
    dnnl_Owi4o = dnnl_Acb4a,
    dnnl_Owi8o = dnnl_Acb8a,

    // weights, 4D
    dnnl_IOhw16i16o = dnnl_BAcd16b16a,
    dnnl_IOhw16o16i = dnnl_BAcd16a16b,
    dnnl_Ohwi16o = dnnl_Acdb16a,
    dnnl_OhwI16o2i = dnnl_AcdB16a2b,
    dnnl_Ohwi32o = dnnl_Acdb32a,
    dnnl_Ohwi4o = dnnl_Acdb4a,
    dnnl_Ohwi8o = dnnl_Acdb8a,
    dnnl_OIhw16i16o = dnnl_ABcd16b16a,
    dnnl_OIhw16o16i = dnnl_ABcd16a16b,
    dnnl_Oihw16o = dnnl_Abcd16a,
    dnnl_OIhw4i16o4i = dnnl_ABcd4b16a4b,
    dnnl_OIhw4i4o = dnnl_ABcd4b4a,
    dnnl_OIhw4o4i = dnnl_ABcd4a4b,
    dnnl_Oihw4o = dnnl_Abcd4a,
    dnnl_OIhw8i16o2i = dnnl_ABcd8b16a2b,
    dnnl_OIhw8i8o = dnnl_ABcd8b8a,
    dnnl_OIhw8o16i2o = dnnl_ABcd8a16b2a,
    dnnl_OIhw2i8o4i = dnnl_ABcd2b8a4b,
    dnnl_IOhw8o16i2o = dnnl_BAcd8a16b2a,
    dnnl_OIhw8o8i = dnnl_ABcd8a8b,

    // weights, 5D
    dnnl_Odhwi16o = dnnl_Acdeb16a,
    dnnl_OdhwI16o2i = dnnl_AcdeB16a2b,
    dnnl_Odhwi4o = dnnl_Acdeb4a,
    dnnl_Odhwi8o = dnnl_Acdeb8a,
    dnnl_OIdhw16i16o = dnnl_ABcde16b16a,
    dnnl_OIdhw16o16i = dnnl_ABcde16a16b,
    dnnl_Oidhw16o = dnnl_Abcde16a,
    dnnl_OIdhw4i4o = dnnl_ABcde4b4a,
    dnnl_OIdhw4o4i = dnnl_ABcde4a4b,
    dnnl_Oidhw4o = dnnl_Abcde4a,
    dnnl_OIdhw8i16o2i = dnnl_ABcde8b16a2b,
    dnnl_OIdhw8i8o = dnnl_ABcde8b8a,
    dnnl_OIdhw8o16i2o = dnnl_ABcde8a16b2a,
    dnnl_IOdhw8o16i2o = dnnl_BAcde8a16b2a,
    dnnl_OIdhw4i16o4i = dnnl_ABcde4b16a4b,
    dnnl_OIdhw2i8o4i = dnnl_ABcde2b8a4b,
    dnnl_OIdhw8o8i = dnnl_ABcde8a8b,
    dnnl_IOdhw16i16o = dnnl_BAcde16b16a,

    // weights w/ groups, 3D
    dnnl_Goiw16g = dnnl_Abcd16a,
    dnnl_Goiw8g = dnnl_Abcd8a,
    dnnl_gIOw16o16i = dnnl_aCBd16b16c,
    dnnl_gIOw16i16o = dnnl_aCBd16c16b,
    dnnl_gOIw16i16o = dnnl_aBCd16c16b,
    dnnl_gOIw16o16i = dnnl_aBCd16b16c,
    dnnl_gOiw16o = dnnl_aBcd16b,
    dnnl_gOIw4i16o4i = dnnl_aBCd4c16b4c,
    dnnl_gOIw2i8o4i = dnnl_aBCd2c8b4c,
    dnnl_gOIw4i4o = dnnl_aBCd4c4b,
    dnnl_gOIw4o4i = dnnl_aBCd4b4c,
    dnnl_gOiw4o = dnnl_aBcd4b,
    dnnl_gOIw8i16o2i = dnnl_aBCd8c16b2c,
    dnnl_gOIw8i8o = dnnl_aBCd8c8b,
    dnnl_gOIw8o16i2o = dnnl_aBCd8b16c2b,
    dnnl_gIOw8o16i2o = dnnl_aCBd8b16c2b,
    dnnl_gOIw8o8i = dnnl_aBCd8b8c,
    dnnl_gOwi16o = dnnl_aBdc16b,
    dnnl_gOwI16o2i = dnnl_aBdC16b2c,
    dnnl_gOwi4o = dnnl_aBdc4b,
    dnnl_gOwi8o = dnnl_aBdc8b,

    // weights w/ groups, 4D
    dnnl_gIOhw16i16o = dnnl_aCBde16c16b,
    dnnl_gIOhw16o16i = dnnl_aCBde16b16c,
    dnnl_gOhwi16o = dnnl_aBdec16b,
    dnnl_gOhwI16o2i = dnnl_aBdeC16b2c,
    dnnl_gOhwi32o = dnnl_aBdec32b,
    dnnl_gOhwi4o = dnnl_aBdec4b,
    dnnl_gOhwi8o = dnnl_aBdec8b,
    dnnl_Goihw16g = dnnl_Abcde16a,
    dnnl_gOIhw16i16o = dnnl_aBCde16c16b,
    dnnl_gOIhw16o16i = dnnl_aBCde16b16c,
    dnnl_gOihw16o = dnnl_aBcde16b,
    dnnl_gOIhw2i8o4i = dnnl_aBCde2c8b4c,
    dnnl_gOIhw4i16o4i = dnnl_aBCde4c16b4c,
    dnnl_gOIhw4i4o = dnnl_aBCde4c4b,
    dnnl_gOIhw4o4i = dnnl_aBCde4b4c,
    dnnl_gOihw4o = dnnl_aBcde4b,
    dnnl_Goihw8g = dnnl_Abcde8a,
    dnnl_gOIhw8i16o2i = dnnl_aBCde8c16b2c,
    dnnl_gOIhw8i8o = dnnl_aBCde8c8b,
    dnnl_gOIhw8o16i2o = dnnl_aBCde8b16c2b,
    dnnl_gIOhw8o16i2o = dnnl_aCBde8b16c2b,
    dnnl_gOIhw8o8i = dnnl_aBCde8b8c,

    dnnl_OIhw4o8i8o4i = dnnl_ABcd4a8b8a4b,
    dnnl_OIhw2o8i8o2i = dnnl_ABcd2a8b8a2b,
    dnnl_gOIhw4o8i8o4i = dnnl_aBCde4b8c8b4c,
    dnnl_gOIhw2o8i8o2i = dnnl_aBCde2b8c8b2c,

    // weights w/ groups, 6D
    dnnl_gIOdhw16i16o = dnnl_aCBdef16c16b,
    dnnl_gOdhwi16o = dnnl_aBdefc16b,
    dnnl_gOdhwI16o2i = dnnl_aBdefC16b2c,
    dnnl_gOdhwi4o = dnnl_aBdefc4b,
    dnnl_gOdhwi8o = dnnl_aBdefc8b,
    dnnl_gOIdhw16i16o = dnnl_aBCdef16c16b,
    dnnl_gOIdhw4i16o4i = dnnl_aBCdef4c16b4c,
    dnnl_gOIdhw2i8o4i = dnnl_aBCdef2c8b4c,
    dnnl_gOIdhw16o16i = dnnl_aBCdef16b16c,
    dnnl_gOidhw16o = dnnl_aBcdef16b,
    dnnl_gOIdhw4i4o = dnnl_aBCdef4c4b,
    dnnl_gOIdhw4o4i = dnnl_aBCdef4b4c,
    dnnl_gOidhw4o = dnnl_aBcdef4b,
    dnnl_gOIdhw8i16o2i = dnnl_aBCdef8c16b2c,
    dnnl_gOIdhw8i8o = dnnl_aBCdef8c8b,
    dnnl_gOIdhw8o16i2o = dnnl_aBCdef8b16c2b,
    dnnl_gIOdhw8o16i2o = dnnl_aCBdef8b16c2b,
    dnnl_gOIdhw8o8i = dnnl_aBCdef8b8c,
    dnnl_Goidhw16g = dnnl_Abcdef16a,
} dnnl_format_tag_t;

/// @} dnnl_api_memory

/// @addtogroup dnnl_api_primitives
/// @{
/// @addtogroup dnnl_api_primitives_common
/// @{

/// Kinds of propagation.
typedef enum {
    // TODO: suggest renames
    /// Undefined propagation type.
    dnnl_prop_kind_undef = 0,
    /// Forward data propagation (training mode). In this mode primitives
    /// perform computations necessary for subsequent backward propagation.
    dnnl_forward_training = 64,
    /// Forward data propagation (inference mode). In this mode primitives
    /// perform only computations that are necessary for inference and omit
    /// computations that are necessary only for backward propagation.
    dnnl_forward_inference = 96,
    /// Forward data propagation (alias for @c dnnl_forward_inference).
    dnnl_forward_scoring = dnnl_forward_inference,
    /// Forward data propagation (alias for @c dnnl_forward_training).
    dnnl_forward = dnnl_forward_training,
    /// Backward propagation (with respect to all parameters).
    dnnl_backward = 128,
    /// Backward data propagation.
    dnnl_backward_data = 160,
    /// Backward weights propagation.
    dnnl_backward_weights = 192,
    /// Backward bias propagation.
    dnnl_backward_bias = 193,
} dnnl_prop_kind_t;

/// Kinds of primitives. Used to implement a way to extend the library with new
/// primitives without changing the ABI.
typedef enum {
    /// Undefined primitive
    dnnl_undefined_primitive,
    /// A reorder primitive.
    dnnl_reorder,
    /// A shuffle primitive.
    dnnl_shuffle,
    /// A (out-of-place) concat primitive.
    dnnl_concat,
    /// A sum primitive.
    dnnl_sum,
    /// A convolution primitive.
    dnnl_convolution,
    /// A deconvolution primitive.
    dnnl_deconvolution,
    /// An element-wise primitive.
    dnnl_eltwise,
    /// A softmax primitive.
    dnnl_softmax,
    /// A pooling primitive.
    dnnl_pooling,
    /// An LRN primitive.
    dnnl_lrn,
    /// A batch normalization primitive.
    dnnl_batch_normalization,
    /// A layer normalization primitive.
    dnnl_layer_normalization,
    /// An inner product primitive.
    dnnl_inner_product,
    /// A rnn primitive.
    dnnl_rnn,
    /// A matrix multiplication primitive (internal).
    dnnl_gemm,
    /// A binary primitive.
    dnnl_binary,
    /// A logsoftmax primitive.
    dnnl_logsoftmax,
    /// A matrix multiplication primitive.
    dnnl_matmul,
    /// A resampling primitive.
    dnnl_resampling,
} dnnl_primitive_kind_t;

/// Kinds of algorithms.
typedef enum {
    dnnl_alg_kind_undef,
    /// Direct convolution
    dnnl_convolution_direct = 0x1,
    /// Winograd convolution
    dnnl_convolution_winograd = 0x2,
    /// Convolution algorithm(either direct or Winograd) is chosen just in time
    dnnl_convolution_auto = 0x3,
    /// Direct deconvolution
    dnnl_deconvolution_direct = 0xa,
    /// Winograd deconvolution
    dnnl_deconvolution_winograd = 0xb,
    /// Eltwise: ReLU
    dnnl_eltwise_relu = 0x1f,
    /// Eltwise: hyperbolic tangent non-linearity (tanh)
    dnnl_eltwise_tanh = 0x2f,
    /// Eltwise: parametric exponential linear unit (elu)
    dnnl_eltwise_elu = 0x3f,
    /// Eltwise: square
    dnnl_eltwise_square = 0x4f,
    /// Eltwise: abs
    dnnl_eltwise_abs = 0x5f,
    /// Eltwise: square root
    dnnl_eltwise_sqrt = 0x6f,
    /// Eltwise: linear
    dnnl_eltwise_linear = 0x7f,
    /// Eltwise: bounded_relu
    dnnl_eltwise_bounded_relu = 0x8f,
    /// Eltwise: soft_relu
    dnnl_eltwise_soft_relu = 0x9f,
    /// Eltwise: logistic
    dnnl_eltwise_logistic = 0xaf,
    /// Eltwise: exponent
    dnnl_eltwise_exp = 0xbf,
    /// Eltwise: gelu
    ///
    /// @note Tanh approximation formula is used to approximate
    /// cumulative distribution function of a Gaussian
    dnnl_eltwise_gelu = 0xcf,
    /// Eltwise: swish
    dnnl_eltwise_swish = 0xdf,
    /// Eltwise: natural logarithm
    dnnl_eltwise_log = 0xef,
    /// Eltwise: clip
    dnnl_eltwise_clip = 0xff,
    /// Max pooling
    dnnl_pooling_max = 0x1ff,
    /// Average pooling include padding
    dnnl_pooling_avg_include_padding = 0x2ff,
    /// Average pooling exclude padding
    dnnl_pooling_avg_exclude_padding = 0x3ff,
    /// Average pooling (alias for #dnnl_pooling_avg_exclude_padding)
    dnnl_pooling_avg = dnnl_pooling_avg_exclude_padding,
    /// Local response normalization (LRN) across multiple channels
    dnnl_lrn_across_channels = 0xaff,
    /// LRN within a single channel
    dnnl_lrn_within_channel = 0xbff,
    /// RNN cell
    dnnl_vanilla_rnn = 0x1fff,
    /// LSTM cell
    dnnl_vanilla_lstm = 0x2fff,
    /// GRU cell
    dnnl_vanilla_gru = 0x3fff,
    /// GRU cell with linear before reset
    ///
    /// Modification of original GRU cell. Differs from #dnnl_vanilla_gru
    /// in how the new memory gate is calculated:
    /// \f[ c_t = tanh(W_c*x_t + b_{c_x} + r_t*(U_c*h_{t-1}+b_{c_h})) \f]
    /// Primitive expects 4 biases on input:
    /// \f$[b_{u}, b_{r}, b_{c_x}, b_{c_h}]\f$
    dnnl_lbr_gru = 0x4fff,
    /// Binary add
    dnnl_binary_add = 0x1fff0,
    /// Binary mul
    dnnl_binary_mul = 0x1fff1,
    /// Nearest Neighbor Resampling Method
    dnnl_resampling_nearest = 0x2fff0,
    /// Linear Resampling Method
    dnnl_resampling_linear = 0x2fff1,
} dnnl_alg_kind_t;

/// Flags for batch normalization primitive.
typedef enum {
    /// Use global statistics
    ///
    /// If specified
    ///  - on forward propagation use mean and variance provided by user (input)
    ///  - on backward propagation reduces the amount of computations, since
    ///    mean and variance are considered as constants
    ///
    ///  If not specified:
    ///   - on forward propagation mean and variance are computed and stored in
    ///     output
    ///   - on backward propagation compute full derivative wrt to data
    dnnl_use_global_stats = 0x1U,

    /// Use scale and shift parameters
    ///
    /// If specified:
    ///  - on forward propagation use scale and shift (aka scale and bias) for
    ///    the batch normalization results
    ///  - on backward propagation (for prop_kind == #dnnl_backward) compute
    ///    diff wrt to scale and shift (hence one extra output used)
    ///
    /// If no specified:
    ///  - on backward propagation prop_kind == #dnnl_backward_data has the
    ///    same behavior as prop_kind == #dnnl_backward
    dnnl_use_scaleshift = 0x2U,

    /// Fuse with ReLU
    ///
    /// The flag implies negative slope being 0. On training this is the only
    /// configuration supported. For inference, to use non-zero negative slope
    /// consider using @ref dev_guide_attributes_post_ops.
    ///
    /// If specified:
    ///  - on inference this option behaves the same as if the primitive were
    ///    fused with ReLU using post ops API with zero negative slope.
    ///  - on training primitive requires workspace (required to be able to
    ///    perform backward pass)
    dnnl_fuse_norm_relu = 0x4U,
} dnnl_normalization_flags_t;

/// @} dnnl_api_primitives_common
/// @} dnnl_api_primitives

/// @addtogroup dnnl_api_memory
/// @{

/// Maximum number of dimensions a tensor can have. Only restricts the amount
/// of space used for the tensor description. Individual computational
/// primitives may support only tensors of certain dimensions.
#define DNNL_MAX_NDIMS 12

/// A wildcard value for dimensions that are unknown at a primitive creation
/// time.
#define DNNL_RUNTIME_DIM_VAL INT64_MIN

/// A `size_t` counterpart of the DNNL_RUNTIME_DIM_VAL.
/// For instance, this value is returned by dnnl_memory_desc_get_size() if
/// either of the dimensions or strides equal to #DNNL_RUNTIME_DIM_VAL.
#define DNNL_RUNTIME_SIZE_VAL ((size_t)DNNL_RUNTIME_DIM_VAL)

/// @cond DO_NOT_DOCUMENT_THIS
/// Hex representation for a **special** quiet NAN (!= NAN from math.h)
static const union {
    unsigned u;
    float f;
} DNNL_RUNTIME_F32_VAL_REP = {0x7fc000d0};
/// @endcond

/// A wildcard value for floating point values that are unknown at a primitive
/// creation time.
#define DNNL_RUNTIME_F32_VAL (DNNL_RUNTIME_F32_VAL_REP.f)

/// @cond DO_NOT_DOCUMENT_THIS
static const int DNNL_RUNTIME_S32_VAL_REP = INT32_MIN;
/// @endcond

/// A wildcard value for int32_t values that are unknown at a primitive creation
/// time.
#define DNNL_RUNTIME_S32_VAL DNNL_RUNTIME_S32_VAL_REP

/// A type to describe tensor dimension.
typedef int64_t dnnl_dim_t;

/// A type to describe tensor dimensions.
typedef dnnl_dim_t dnnl_dims_t[DNNL_MAX_NDIMS];

/// Generic description of blocked data layout for most memory formats.
///
/// @sa @ref dev_guide_understanding_memory_formats
typedef struct {
    /// The strides between the outermost blocks.
    /// In case of plain (non-blocked) formats the strides between dimensions.
    dnnl_dims_t strides;
    // Innermost section
    // ASSUMPTION: the innermost blocks are always dense
    /// The number of innermost blocks, e.g. 3 in case of `OIhw_4i16o4i_`
    int inner_nblks;
    /// The size of the blocks, e.g. `{4, 16, 4}` in case of `OIhw_4i16o4i`
    dnnl_dims_t inner_blks;
    /// The logical indices of the blocks, e.g. `{1, 0, 1}` in case of
    /// `4i16o4i`, because `i` is the 1st dim and `o` is the 0st dim
    dnnl_dims_t inner_idxs;
} dnnl_blocking_desc_t;

/// Winograd-specific formats
typedef enum {
    /// Undefined memory format, used for empty memory descriptors.
    dnnl_wino_undef = 0,
    // Tensors of weights for 2x3 winograd convolutions.
    dnnl_wino_wei_aaOIoi, ///< Internal weights format for 2x3 Winograd
    dnnl_wino_wei_aaOio, ///< Internal weights format for 2x3 Winograd
    dnnl_wino_wei_aaOBiOo, ///< Internal weights format for 2x3 Winograd
    // Tensor of weights for 4x3 convolution.
    dnnl_wino_wei_OBaaIBOIio ///< Internal weights format for 4x3 Winograd
} dnnl_wino_memory_format_t;

/// Description of tensor of weights for winograd 2x3 convolution.
typedef struct {
    dnnl_wino_memory_format_t wino_format;
    int r;
    int alpha;
    int ic;
    int oc;
    int ic_block;
    int oc_block;
    int ic2_block;
    int oc2_block;
    float adj_scale;
    size_t size;
} dnnl_wino_desc_t;

typedef enum {
    dnnl_packed_format_undef = 0,
    dnnl_ldigo_p,
    dnnl_ldgoi_p
} dnnl_rnn_packed_memory_format_t;

/// Maximum number of parts of RNN weights tensor that require separate
/// computation.
#define DNNL_RNN_MAX_N_PARTS 4

/// Description of tensor of packed weights for rnn.
typedef struct {
    dnnl_rnn_packed_memory_format_t format;
    int n_parts;
    int n;
    int ldb;
    int parts[DNNL_RNN_MAX_N_PARTS];
    size_t part_pack_size[DNNL_RNN_MAX_N_PARTS];
    unsigned pack_part[DNNL_RNN_MAX_N_PARTS];
    size_t offset_compensation;
    size_t size;
    char reserved[200];
} dnnl_rnn_packed_desc_t;

/// Flags for memory special features
typedef enum {
    dnnl_memory_extra_flag_none = 0x0U,
    /// Indicates the weights have an additional buffer, that depends on the
    /// @p compensation_mask.
    ///
    /// For instance, in 4D case with the compensation mask equals (1 << 0)
    /// the additional buffer would consist of OC values:
    /// O[oc : 0,OC] =
    ///  -128 * SUM(ic : 0,IC; kh : 0,KH; kw : 0,KW){ weights(oc, ic, kh, kw) }
    dnnl_memory_extra_flag_compensation_conv_s8s8 = 0x1U,
    dnnl_memory_extra_flag_scale_adjust = 0x2U,
    dnnl_memory_extra_flag_gpu_rnn_u8s8_compensation = 0x4U,
} dnnl_memory_extra_flags_t;

/// Description of extra information stored in memory
typedef struct {
    /// The flags contain arbitrary extra information, such as compensation.
    /// @sa dnnl_memory_extra_flags_t
    uint64_t flags;
    /// Compensation mask
    int compensation_mask;
    /// Scale applied to the data
    float scale_adjust;
    /// For future backwards compatibility
    char reserved[64];
} dnnl_memory_extra_desc_t;

/// Memory descriptor. The description is based on a number of dimensions,
/// dimensions themselves, plus information about elements type and memory
/// format. Additionally, contains format-specific descriptions of the data
/// layout.
typedef struct {
    /// Number of dimensions
    int ndims;
    /// Dimensions in the following order:
    /// - CNN data tensors: mini-batch, channel, spatial
    ///   (<code>{N, C, [[D,] H,] W}</code>)
    /// - CNN weight tensors: group (optional), output channel, input channel,
    ///   spatial (<code>{[G,] O, I, [[D,] H,] W}</code>)
    /// - RNN data tensors: time, mini-batch, channels (<code>{T, N, C}</code>)
    ///   or layers, directions, states, mini-batch, channels (<code>{L, D, S, N, C}</code>)
    /// - RNN weight tensor: layers, directions, input channel, gates, output channels
    ///   (<code>{L, D, I, G, O}</code>).
    ///
    /// @note
    ///    The order of dimensions does not depend on the memory format, so
    ///    whether the data is laid out in #dnnl_nchw or #dnnl_nhwc
    ///    the dims for 4D CN data tensor would be <code>{N, C, H, W}</code>.
    dnnl_dims_t dims;

    /// Data type of the tensor elements.
    dnnl_data_type_t data_type;

    /// Size of the data including padding in each dimension.
    dnnl_dims_t padded_dims;

    /// Per-dimension offset from the padding to actual data, the top-level
    /// tensor with offsets applied must lie within the padding area.
    dnnl_dims_t padded_offsets;

    /// Offset from memory origin to the current block, non-zero only in
    /// a description of a memory sub-block.
    dnnl_dim_t offset0;

    /// Memory format kind.
    dnnl_format_kind_t format_kind;
    union {
        /// Description of the data layout for memory formats that use
        /// blocking.
        dnnl_blocking_desc_t blocking;
        /// Tensor of weights for integer 8bit winograd convolution.
        dnnl_wino_desc_t wino_desc;
        /// Tensor of packed weights for RNN.
        dnnl_rnn_packed_desc_t rnn_packed_desc;
        // ... other descriptions possible
    } format_desc;

    dnnl_memory_extra_desc_t extra;
} dnnl_memory_desc_t;

/// @struct dnnl_memory
/// An opaque structure to describe a memory.
struct dnnl_memory;

/// A memory handle.
typedef struct dnnl_memory *dnnl_memory_t;

/// A constant memory handle.
typedef const struct dnnl_memory *const_dnnl_memory_t;

#define DNNL_MEMORY_NONE (NULL)
#define DNNL_MEMORY_ALLOCATE ((void *)(size_t)-1)

/// @} dnnl_api_memory

/// @addtogroup dnnl_api_primitives
/// @{
/// @addtogroup dnnl_api_primitives_common
/// @{

/// A pointer to any of the operation descriptors.
typedef void *dnnl_op_desc_t;
/// A pointer to any of the operation descriptors (constant variant).
typedef const void *const_dnnl_op_desc_t;

/// @} dnnl_api_primitives_common
/// @} dnnl_api_primitives

/// @addtogroup dnnl_api_primitives
/// @{

/// @addtogroup dnnl_api_convolution
/// @{

/// A descriptor of a convolution operation.
typedef struct {
    /// The kind of primitive. Used for self-identifying the primitive
    /// descriptor. Must be #dnnl_convolution.
    dnnl_primitive_kind_t primitive_kind;
    /// The kind of propagation. Possible values: #dnnl_forward_training,
    /// #dnnl_forward_inference, #dnnl_backward_data,
    /// #dnnl_backward_weights, and #dnnl_backward_bias.
    dnnl_prop_kind_t prop_kind;
    /// The kind of the convolution algorithm. Possible values:
    /// #dnnl_convolution_direct.
    dnnl_alg_kind_t alg_kind;
    /// Source memory descriptor.
    dnnl_memory_desc_t src_desc;
    /// Source gradient memory descriptor.
    dnnl_memory_desc_t diff_src_desc;
    /// Weights memory descriptor.
    dnnl_memory_desc_t weights_desc;
    /// Weights gradient memory descriptor.
    dnnl_memory_desc_t diff_weights_desc;
    /// Bias memory descriptor.
    dnnl_memory_desc_t bias_desc;
    /// Bias gradient memory descriptor.
    dnnl_memory_desc_t diff_bias_desc;
    /// Destination memory descriptor.
    dnnl_memory_desc_t dst_desc;
    /// Destination gradient memory descriptor.
    dnnl_memory_desc_t diff_dst_desc;
    /// Convolution strides in each spatial dimension.
    dnnl_dims_t strides;
    /// Convolution dilates in each spatial dimension.
    dnnl_dims_t dilates;
    /// Padding in each spatial dimension. padding[0] is a padding in the
    /// beginning (@p padding_l), padding[1] is a padding in the end (@p
    /// padding_r).
    dnnl_dims_t padding[2];
    /// The accumulator data type. Initialized automatically.
    dnnl_data_type_t accum_data_type;
} dnnl_convolution_desc_t;

/// @} dnnl_api_convolution

/// @addtogroup dnnl_api_deconvolution
/// @{

/// A descriptor of a deconvolution operation.
typedef dnnl_convolution_desc_t dnnl_deconvolution_desc_t;

/// @} dnnl_api_deconvolution

/// @addtogroup dnnl_api_shuffle
/// @{

/// A descriptor of a shuffle operation.
typedef struct {
    /// The kind of primitive. Used for self-identifying the primitive
    /// descriptor. Must be #dnnl_shuffle.
    dnnl_primitive_kind_t primitive_kind;
    /// The kind of propagation. Possible values: #dnnl_forward_training,
    /// #dnnl_forward_inference, and #dnnl_backward_data.
    dnnl_prop_kind_t prop_kind;
    /// Source and destination memory descriptor,
    /// and source and destination gradient memory descriptor.
    dnnl_memory_desc_t data_desc;
    /// Axis for shuffling.
    int axis;
    /// Number of groups.
    dnnl_dim_t group_size;
} dnnl_shuffle_desc_t;

/// @} dnnl_api_shuffle

/// @addtogroup dnnl_api_eltwise
/// @{

/// A descriptor of a element-wise operation.
typedef struct {
    /// The kind of primitive. Used for self-identifying the primitive
    /// descriptor. Must be #dnnl_eltwise.
    dnnl_primitive_kind_t primitive_kind;
    /// The kind of propagation. Possible values: #dnnl_forward_training,
    /// #dnnl_forward_inference, #dnnl_backward, and #dnnl_backward_data.
    dnnl_prop_kind_t prop_kind;
    /// The kind of eltwise algorithm. Possible values: #dnnl_eltwise_relu,
    /// #dnnl_eltwise_tanh, #dnnl_eltwise_elu, #dnnl_eltwise_square,
    /// #dnnl_eltwise_abs, #dnnl_eltwise_sqrt, #dnnl_eltwise_linear,
    /// #dnnl_eltwise_bounded_relu, #dnnl_eltwise_soft_relu,
    /// #dnnl_eltwise_logistic, #dnnl_eltwise_exp, #dnnl_eltwise_gelu,
    /// #dnnl_eltwise_swish, #dnnl_eltwise_log, #dnnl_eltwise_clip.
    dnnl_alg_kind_t alg_kind;
    /// Source and destination memory descriptor.
    dnnl_memory_desc_t data_desc;
    /// Source and destination gradient memory descriptor.
    dnnl_memory_desc_t diff_data_desc;
    /// Algorithm specific parameter.
    /// Accordance table:
    ///  - #dnnl_eltwise_relu: @p alpha -- negative slope, @p beta ignored
    ///  - #dnnl_eltwise_tanh: @p alpha and @p beta ignored
    ///  - #dnnl_eltwise_elu: @p alpha -- negative slope, @p beta ignored
    ///  - #dnnl_eltwise_square: @p alpha and @p beta ignored
    ///  - #dnnl_eltwise_abs: @p alpha and @p beta ignored
    ///  - #dnnl_eltwise_sqrt: @p alpha and @p beta ignored
    ///  - #dnnl_eltwise_linear: @p alpha -- scale, @p beta -- shift
    ///  - #dnnl_eltwise_bounded_relu: @p alpha -- upper bound, @p beta ignored
    ///  - #dnnl_eltwise_soft_relu: @p alpha and @p beta ignored
    ///  - #dnnl_eltwise_logistic: @p alpha and @p beta ignored
    ///  - #dnnl_eltwise_exp: @p alpha and @p beta ignored
    ///  - #dnnl_eltwise_gelu: @p alpha and @p beta ignored
    ///  - #dnnl_eltwise_swish: @p alpha -- sigmoid arg scaling, @p beta ignored
    ///  - #dnnl_eltwise_log: @p alpha and @p beta ignored
    ///  - #dnnl_eltwise_clip: @p alpha -- lower bound, @p beta -- upper bound
    float alpha, beta;
} dnnl_eltwise_desc_t;

/// @} dnnl_api_eltwise

/// @addtogroup dnnl_api_softmax
/// @{

/// A descriptor of a Softmax operation.
typedef struct {
    /// The kind of primitive. Used for self-identifying the primitive
    /// descriptor. Must be #dnnl_softmax.
    dnnl_primitive_kind_t primitive_kind;
    /// The kind of propagation. Possible values: #dnnl_forward_training and
    /// #dnnl_forward_inference.
    dnnl_prop_kind_t prop_kind;
    /// Source and destination memory descriptor.
    dnnl_memory_desc_t data_desc;
    /// Source and Destination of gradient memory descriptor.
    dnnl_memory_desc_t diff_desc;
    /// The axis along which to perform the softmax.
    int softmax_axis;
} dnnl_softmax_desc_t;

/// @} dnnl_api_softmax

/// @addtogroup dnnl_api_logsoftmax
/// @{

/// A descriptor of a LogSoftmax operation. An alias of Softmax structure, but
/// primitive_kind must be #dnnl_logsoftmax.
typedef dnnl_softmax_desc_t dnnl_logsoftmax_desc_t;

/// @} dnnl_api_logsoftmax

/// @addtogroup dnnl_api_pooling
/// @{

/// A descriptor of a pooling operation.
typedef struct {
    /// The kind of primitive. Used for self-identifying the primitive
    /// descriptor. Must be #dnnl_pooling.
    dnnl_primitive_kind_t primitive_kind;
    /// The kind of propagation. Possible values: #dnnl_forward_training,
    /// #dnnl_forward_inference, #dnnl_backward, and #dnnl_backward_data.
    dnnl_prop_kind_t prop_kind;
    /// The kind of pooling algorithm.
    /// Possible values: #dnnl_pooling_max,
    /// #dnnl_pooling_avg_include_padding, and
    /// #dnnl_pooling_avg_exclude_padding.
    dnnl_alg_kind_t alg_kind;
    /// Source memory descriptor.
    dnnl_memory_desc_t src_desc;
    /// Source gradient memory descriptor.
    dnnl_memory_desc_t diff_src_desc;
    /// Destination memory descriptor.
    dnnl_memory_desc_t dst_desc;
    /// Destination gradient memory descriptor.
    dnnl_memory_desc_t diff_dst_desc;
    /// Pooling kernel strides for spatial dimensions.
    dnnl_dims_t strides;
    /// Pooling kernel spatial dimensions.
    dnnl_dims_t kernel;
    /// Padding in each spatial dimension. padding[0] is a padding in the
    /// beginning (@p padding_l), padding[1] is a padding in the end (@p
    /// padding_r).
    dnnl_dims_t padding[2];
    /// The accumulator data type. Initialized automatically.
    dnnl_data_type_t accum_data_type;
} dnnl_pooling_desc_t;

/// @} dnnl_api_pooling

/// @addtogroup dnnl_api_lrn
/// @{

/// A descriptor of a Local Response Normalization (LRN) operation.
typedef struct {
    /// The kind of primitive. Used for self-identifying the primitive
    /// descriptor. Must be #dnnl_lrn.
    dnnl_primitive_kind_t primitive_kind;
    /// The kind of propagation. Possible values: #dnnl_forward_training,
    /// #dnnl_forward_inference, #dnnl_backward, and #dnnl_backward_data.
    dnnl_prop_kind_t prop_kind;
    /// LRN algorithm. Possible values: #dnnl_lrn_within_channel and
    /// #dnnl_lrn_across_channels.
    dnnl_alg_kind_t alg_kind;
    /// Source and destination memory descriptor.
    dnnl_memory_desc_t data_desc;
    /// Source and destination gradient memory descriptor.
    dnnl_memory_desc_t diff_data_desc;
    /// The number of channels to sum over (for cross-channel LRN) or the side
    /// length of the square region to sum over (for within-channel LRN).
    dnnl_dim_t local_size;
    /// LRN alpha parameter.
    float lrn_alpha;
    /// LRN beta parameter.
    float lrn_beta;
    /// LRN k parameter.
    float lrn_k;
} dnnl_lrn_desc_t;

/// @} dnnl_api_lrn

/// @addtogroup dnnl_api_batch_normalization
/// @{

/// A descriptor of a Batch Normalization operation.
typedef struct {
    /// The kind of primitive. Used for self-identifying the primitive
    /// descriptor. Must be #dnnl_batch_normalization.
    dnnl_primitive_kind_t primitive_kind;
    /// The kind of propagation. Possible values: #dnnl_forward_training,
    /// #dnnl_forward_inference, #dnnl_backward, and #dnnl_backward_data.
    dnnl_prop_kind_t prop_kind;
    /// Source and destination memory descriptor.
    dnnl_memory_desc_t data_desc;
    /// Source and destination gradient memory descriptor.
    dnnl_memory_desc_t diff_data_desc;
    /// Scale and shift data and gradient memory descriptors.
    ///
    /// Scaleshift memory descriptor uses 2D #dnnl_nc format[2,Channels]. 1-st
    /// dimension contains gamma parameter, 2-nd dimension contains beta
    /// parameter.
    dnnl_memory_desc_t data_scaleshift_desc;
    dnnl_memory_desc_t diff_data_scaleshift_desc;
    /// Statistics memory descriptor.
    ///
    /// Statistics (mean or variance) descriptor use 1D #dnnl_x format[Channels].
    dnnl_memory_desc_t stat_desc;
    /// Batch normalization epsilon parameter.
    float batch_norm_epsilon;
    unsigned flags;
} dnnl_batch_normalization_desc_t;

/// @} dnnl_api_batch_normalization

/// @addtogroup dnnl_api_layer_normalization
/// @{

/// A descriptor of a Layer Normalization operation.
typedef struct {
    /// The kind of primitive. Used for self-identifying the primitive
    /// descriptor. Must be #dnnl_layer_normalization.
    dnnl_primitive_kind_t primitive_kind;
    /// The kind of propagation. Possible values: #dnnl_forward_training,
    /// #dnnl_forward_inference, #dnnl_backward, and #dnnl_backward_data.
    dnnl_prop_kind_t prop_kind;
    /// Source and destination memory descriptor.
    dnnl_memory_desc_t data_desc;
    /// Source and destination gradient memory descriptor.
    dnnl_memory_desc_t diff_data_desc;
    /// Scale and shift data and gradient memory descriptors.
    ///
    /// Scaleshift memory descriptor uses 2D #dnnl_ab
    /// format[2, normalized_dim] where 1-st dimension contains gamma parameter,
    /// 2-nd dimension contains beta parameter. Normalized_dim is equal to the
    /// last logical dimension of the data tensor across which normalization is
    /// performed.
    dnnl_memory_desc_t data_scaleshift_desc;
    dnnl_memory_desc_t diff_data_scaleshift_desc;
    /// Mean and variance data memory descriptors.
    ///
    /// Statistics (mean and variance) memory descriptor is the k-dimensional tensor
    /// where k is equal to data_tensor_ndims - 1 and may have any plain
    /// (stride[last_dim] == 1) user-provided format.
    dnnl_memory_desc_t stat_desc;
    /// Layer normalization epsilon parameter.
    float layer_norm_epsilon;
    unsigned flags;
} dnnl_layer_normalization_desc_t;

/// @} dnnl_api_layer_normalization

/// @addtogroup dnnl_api_inner_product
/// @{

/// A descriptor of an inner product operation.
typedef struct {
    /// The kind of primitive. Used for self-identifying the primitive
    /// descriptor. Must be #dnnl_inner_product.
    dnnl_primitive_kind_t primitive_kind;
    /// The kind of propagation. Possible values: #dnnl_forward_training,
    /// #dnnl_forward_inference, #dnnl_backward_data,
    /// #dnnl_backward_weights, and #dnnl_backward_bias.
    dnnl_prop_kind_t prop_kind;
    /// Source memory descriptor.
    dnnl_memory_desc_t src_desc;
    /// Source gradient memory descriptor.
    dnnl_memory_desc_t diff_src_desc;
    /// Weights memory descriptor.
    dnnl_memory_desc_t weights_desc;
    /// Weights gradient memory descriptor.
    dnnl_memory_desc_t diff_weights_desc;
    /// Bias memory descriptor.
    dnnl_memory_desc_t bias_desc;
    /// Bias gradient memory descriptor.
    dnnl_memory_desc_t diff_bias_desc;
    /// Destination memory descriptor.
    dnnl_memory_desc_t dst_desc;
    /// Destination gradient memory descriptor.
    dnnl_memory_desc_t diff_dst_desc;
    /// The accumulator data type. Initialized automatically.
    dnnl_data_type_t accum_data_type;
} dnnl_inner_product_desc_t;

/// @} dnnl_api_inner_product

/// @addtogroup dnnl_api_rnn
/// @{

/// Flags for RNN cell.
typedef enum {
    /// Undefined RNN flags
    dnnl_rnn_flags_undef = 0x0
} dnnl_rnn_flags_t;

/// A direction of RNN primitive execution.
typedef enum {
    /// Unidirectional execution of RNN primitive from left to right.
    dnnl_unidirectional_left2right,
    /// Unidirectional execution of RNN primitive from right to left.
    dnnl_unidirectional_right2left,
    /// Bidirectional execution of RNN primitive with concatenation of the
    /// results.
    dnnl_bidirectional_concat,
    /// Bidirectional execution of RNN primitive with summation of the
    /// results.
    dnnl_bidirectional_sum,
    /// Alias for #dnnl_unidirectional_left2right.
    dnnl_unidirectional = dnnl_unidirectional_left2right,
} dnnl_rnn_direction_t;

/// A descriptor for an RNN operation.
typedef struct {
    /// The kind of primitive. Used for self-identifying the primitive
    /// descriptor. Must be #dnnl_rnn.
    dnnl_primitive_kind_t primitive_kind;
    /// The kind of propagation. Possible values: #dnnl_forward_training,
    /// #dnnl_forward_inference, and #dnnl_backward.
    dnnl_prop_kind_t prop_kind;
    /// RNN cell kind. Must be one of #dnnl_vanilla_rnn,
    /// #dnnl_vanilla_lstm, #dnnl_vanilla_gru, or #dnnl_lbr_gru.
    dnnl_alg_kind_t cell_kind;
    /// The direction of RNN primitive execution.
    dnnl_rnn_direction_t direction;
    /// Source layer memory descriptor.
    dnnl_memory_desc_t src_layer_desc;
    /// Source iteration memory descriptor for hidden state.
    dnnl_memory_desc_t src_iter_desc;
    /// Source iteration memory descriptor for cell state.
    dnnl_memory_desc_t src_iter_c_desc;
    /// Weights layer memory descriptor.
    dnnl_memory_desc_t weights_layer_desc;
    /// Weights iteration memory descriptor.
    dnnl_memory_desc_t weights_iter_desc;
    /// Bias memory descriptor.
    dnnl_memory_desc_t bias_desc;
    /// Destination layer memory descriptor.
    dnnl_memory_desc_t dst_layer_desc;
    /// Destination iter memory descriptor for hidden state.
    dnnl_memory_desc_t dst_iter_desc;
    /// Destination iter memory descriptor for cell state.
    dnnl_memory_desc_t dst_iter_c_desc;
    /// Placeholders
    dnnl_memory_desc_t placeholder_desc;
    dnnl_memory_desc_t placeholder2_desc;

    /// Source gradient layer memory descriptor.
    dnnl_memory_desc_t diff_src_layer_desc;
    /// Source gradient iter memory descriptor for hidden state.
    dnnl_memory_desc_t diff_src_iter_desc;
    /// Source gradient iter memory descriptor for cell state.
    dnnl_memory_desc_t diff_src_iter_c_desc;
    /// Weights gradient layer memory descriptor.
    dnnl_memory_desc_t diff_weights_layer_desc;
    /// Weights gradient iter memory descriptor.
    dnnl_memory_desc_t diff_weights_iter_desc;
    /// Bias gradient memory descriptor.
    dnnl_memory_desc_t diff_bias_desc;
    /// Destination gradient layer memory descriptor.
    dnnl_memory_desc_t diff_dst_layer_desc;
    /// Destination gradient iteration memory descriptor for hidden state.
    dnnl_memory_desc_t diff_dst_iter_desc;
    /// Destination gradient iteration memory descriptor for cell state.
    dnnl_memory_desc_t diff_dst_iter_c_desc;
    /// Placeholders
    dnnl_memory_desc_t diff_placeholder_desc;
    dnnl_memory_desc_t diff_placeholder2_desc;

    /// RNN cell flags
    unsigned int flags;
    /// Activation function used for vanilla_rnn cell kind.
    /// Must be either #dnnl_eltwise_relu or #dnnl_eltwise_tanh.
    dnnl_alg_kind_t activation_kind;
    float alpha;
    float beta;

} dnnl_rnn_desc_t;

/// @} dnnl_api_rnn

/// @addtogroup dnnl_api_binary
/// @{

/// A descriptor of a binary operation.
typedef struct {
    /// The kind of primitive. Used for self-identifying the primitive
    /// descriptor. Must be #dnnl_binary.
    dnnl_primitive_kind_t primitive_kind;
    /// The kind of the binary algorithm. Possible values:
    /// #dnnl_binary_add and #dnnl_binary_mul.
    dnnl_alg_kind_t alg_kind;
    /// Source memory descriptors.
    dnnl_memory_desc_t src_desc[2];
    /// Destination memory descriptor.
    dnnl_memory_desc_t dst_desc;
} dnnl_binary_desc_t;

/// @} dnnl_api_binary

/// @addtogroup dnnl_api_matmul
/// @{

/// A descriptor of a matrix multiplication operation.
///
/// 2D case:
///     dst[m, n] = src[m, k] * weights[k, n] + bias[m, n]
///
/// 3D case:
///     dst[mb, m, n] = src[mb, m, k] * weights[mb, k, n] + bias[mb, m, n]
typedef struct {
    /// The kind of primitive. Used for self-identifying the primitive
    /// descriptor. Must be #dnnl_matmul.
    dnnl_primitive_kind_t primitive_kind;
    /// Source memory descriptor.
    dnnl_memory_desc_t src_desc;
    /// Weights memory descriptor.
    dnnl_memory_desc_t weights_desc;
    /// Bias memory descriptor.
    dnnl_memory_desc_t bias_desc;
    /// Destination memory descriptor.
    dnnl_memory_desc_t dst_desc;
    /// The accumulator data type. Initialized automatically.
    dnnl_data_type_t accum_data_type;
} dnnl_matmul_desc_t;

/// @} dnnl_api_matmul

/// @addtogroup dnnl_api_resampling
/// @{

/// A descriptor of resampling operation.
typedef struct {
    /// The kind of primitive. Used for self-identifying the primitive
    /// descriptor. Must be #dnnl_resampling.
    dnnl_primitive_kind_t primitive_kind;
    /// The kind of propagation. Possible values: #dnnl_forward_training,
    /// #dnnl_forward_inference, #dnnl_backward_data,
    dnnl_prop_kind_t prop_kind;
    /// The kind of the resampling algorithm. Possible values:
    /// #dnnl_resampling_nearest, #dnnl_resampling_linear.
    dnnl_alg_kind_t alg_kind;
    /// Source memory descriptor.
    dnnl_memory_desc_t src_desc;
    /// Source gradient memory descriptor.
    dnnl_memory_desc_t diff_src_desc;
    /// Destination memory descriptor.
    dnnl_memory_desc_t dst_desc;
    /// Destination gradient memory descriptor.
    dnnl_memory_desc_t diff_dst_desc;
    /// Resampling factor in each spatial dimension.
    float factors[DNNL_MAX_NDIMS];
} dnnl_resampling_desc_t;

/// @} dnnl_api_resampling

/// @} dnnl_api_primitives

/// @addtogroup dnnl_api_engine
/// @{

/// @brief Kinds of engines.
typedef enum {
    /// An unspecified engine.
    dnnl_any_engine,
    /// CPU engine.
    dnnl_cpu,
    /// GPU engine.
    dnnl_gpu,
} dnnl_engine_kind_t;

/// @struct dnnl_engine
/// @brief An opaque structure to describe an engine.
struct dnnl_engine;
/// @brief An engine handle.
typedef struct dnnl_engine *dnnl_engine_t;
#if 0
// FIXME: looks like this never happens
/// @brief A constant engine handle.
typedef const struct dnnl_engine *const_dnnl_engine_t;
#endif

/// @} dnnl_api_engine

/// @addtogroup dnnl_api_primitives
/// @{
/// @addtogroup dnnl_api_primitives_common
/// @{

/// @struct dnnl_primitive_desc_iterator
/// @brief An opaque structure to describe a primitive descriptor iterator.
struct dnnl_primitive_desc_iterator;

/// @brief A primitive descriptor iterator handle.
typedef struct dnnl_primitive_desc_iterator *dnnl_primitive_desc_iterator_t;

/// @brief A constant primitive descriptor iterator handle.
typedef const struct dnnl_primitive_desc_iterator
        *const_dnnl_primitive_desc_iterator_t;

/// @struct dnnl_primitive_desc
/// @brief An opaque structure to describe a primitive descriptor.
struct dnnl_primitive_desc;

/// @brief A primitive descriptor handle.
typedef struct dnnl_primitive_desc *dnnl_primitive_desc_t;

/// @brief A constant primitive descriptor handle.
typedef const struct dnnl_primitive_desc *const_dnnl_primitive_desc_t;

/// @} dnnl_api_primitives_common

/// @addtogroup dnnl_api_attributes
/// @{

/// Scratchpad mode
typedef enum {
    /// The library manages scratchpad (default)
    /// The allocation policy is controlled by the DNNL_ENABLE_CONCURRENT_EXEC
    /// build option (@ref dev_guide_build_options).
    ///
    /// When DNNL_ENABLE_CONCURRENT_EXEC=OFF (default), the library
    /// scratchpad is common to all primitives to reduce the memory
    /// footprint.  This configuration comes with limited
    /// thread-safety properties, namely different primitives can be
    /// created and executed in parallel but cannot migrate between
    /// threads (in other words, each primitive should be executed in
    /// the same thread it was created in).
    ///
    /// When DNNL_ENABLE_CONCURRENT_EXEC=ON, the library scratchpad is private
    /// to each primitive. The memory footprint is larger than when using
    /// DNNL_ENABLE_CONCURRENT_EXEC=OFF but different primitives can be created
    /// and run concurrently (the same primitive cannot be run concurrently from
    /// two different threads though).
    dnnl_scratchpad_mode_library,
    /// A user shall query and provide the scratchpad memory to primitives
    /// This mode is thread-safe as long as the scratchpad buffers
    /// are not used concurrently by two primitive executions.
    dnnl_scratchpad_mode_user,
} dnnl_scratchpad_mode_t;

/// @struct dnnl_primitive_attr
/// @brief An opaque structure for primitive descriptor attributes.
///
/// Attributes may contain:
///  - output scales (to scale the result prior to storing it to the memory)
struct dnnl_primitive_attr;

/// @brief A primitive descriptor attributes handle that controls primitive
/// behavior.
typedef struct dnnl_primitive_attr *dnnl_primitive_attr_t;

/// @brief A constant primitive descriptor attributes handle.
typedef const struct dnnl_primitive_attr *const_dnnl_primitive_attr_t;

/// @struct dnnl_post_ops
/// @brief An opaque structure for a chain of post operations.
///
/// dnnl_post_ops can be used to perform some (trivial) operations like
/// accumulation or eltwise after certain primitives like convolution.
///
/// Post operations might be combined together, making a chain of post
/// operations. For instance one can configure convolution followed by
/// accumulation followed by eltwise. This might be especially beneficial
/// for residual learning blocks.
///
/// @warning
///      Of course not all combinations are supported, so the user should handle
///      errors accordingly.
///
/// Supported post operations:
///  - accumulation (base primitive: convolution)
///  - eltwise (base primitive: convolution)
struct dnnl_post_ops;

/// @brief A post operation chain handle.
typedef struct dnnl_post_ops *dnnl_post_ops_t;

/// @brief A constant post operation chain handle.
typedef const struct dnnl_post_ops *const_dnnl_post_ops_t;

/// @} dnnl_api_attributes

/// @addtogroup dnnl_api_primitives_common
/// @{

/// @struct dnnl_primitive
/// An opaque structure to describe a primitive.
struct dnnl_primitive;
/// A primitive handle.
typedef struct dnnl_primitive *dnnl_primitive_t;
/// A constant primitive handle.
typedef const struct dnnl_primitive *const_dnnl_primitive_t;

/// Source argument #0.
#define DNNL_ARG_SRC_0 1
/// A special mnemonic for source argument for primitives that have a
/// single source. An alias for #DNNL_ARG_SRC_0.
#define DNNL_ARG_SRC DNNL_ARG_SRC_0
/// A special mnemonic for RNN input vector. An alias for
/// #DNNL_ARG_SRC_0.
#define DNNL_ARG_SRC_LAYER DNNL_ARG_SRC_0
/// A special mnemonic for reorder source argument. An alias for
/// #DNNL_ARG_SRC_0.
#define DNNL_ARG_FROM DNNL_ARG_SRC_0

/// Source argument #1.
#define DNNL_ARG_SRC_1 2
/// A special mnemonic for RNN input recurrent hidden state vector. An alias
/// for #DNNL_ARG_SRC_1.
#define DNNL_ARG_SRC_ITER DNNL_ARG_SRC_1

/// Source argument #2.
#define DNNL_ARG_SRC_2 3
/// A special mnemonic for RNN input recurrent cell state vector. An alias for
/// #DNNL_ARG_SRC_2.
#define DNNL_ARG_SRC_ITER_C DNNL_ARG_SRC_2

/// Destination argument #0.
#define DNNL_ARG_DST_0 17
/// A special mnemonic for destination argument for primitives that have a
/// single destination. An alias for #DNNL_ARG_DST_0.
#define DNNL_ARG_DST DNNL_ARG_DST_0
/// A special mnemonic for reorder destination argument. An alias for
/// #DNNL_ARG_DST_0.
#define DNNL_ARG_TO DNNL_ARG_DST_0
/// A special mnemonic for RNN output vector. An alias for #DNNL_ARG_DST_0.
#define DNNL_ARG_DST_LAYER DNNL_ARG_DST_0

/// Destination argument #1.
#define DNNL_ARG_DST_1 18
/// A special mnemonic for RNN input recurrent hidden state vector. An
/// alias for #DNNL_ARG_DST_1.
#define DNNL_ARG_DST_ITER DNNL_ARG_DST_1

/// Destination argument #2.
#define DNNL_ARG_DST_2 19
/// A special mnemonic for LSTM output recurrent cell state vector. An
/// alias for #DNNL_ARG_DST_2.
#define DNNL_ARG_DST_ITER_C DNNL_ARG_DST_2

/// Weights argument #0.
#define DNNL_ARG_WEIGHTS_0 33
/// A special mnemonic for primitives that have a single weights
/// argument. Alias for #DNNL_ARG_WEIGHTS_0.
#define DNNL_ARG_WEIGHTS DNNL_ARG_WEIGHTS_0
/// A special mnemonic for scale and shift argument of normalization
/// primitives. Alias for #DNNL_ARG_WEIGHTS_0.
#define DNNL_ARG_SCALE_SHIFT DNNL_ARG_WEIGHTS_0
/// A special mnemonic for RNN weights applied to the layer input. An
/// alias for #DNNL_ARG_WEIGHTS_0.
#define DNNL_ARG_WEIGHTS_LAYER DNNL_ARG_WEIGHTS_0

/// Weights argument #1.
#define DNNL_ARG_WEIGHTS_1 34
/// A special mnemonic for RNN weights applied to the recurrent input.
/// An alias for #DNNL_ARG_WEIGHTS_1.
#define DNNL_ARG_WEIGHTS_ITER DNNL_ARG_WEIGHTS_1

/// Bias tensor argument.
#define DNNL_ARG_BIAS 41

/// Mean values tensor argument.
#define DNNL_ARG_MEAN 49
/// Variance values tensor argument.
#define DNNL_ARG_VARIANCE 50

/// Workspace tensor argument. Workspace is used to pass information
/// from forward propagation to backward propagation computations.
#define DNNL_ARG_WORKSPACE 64
/// Scratchpad (temporary storage) tensor argument.
#define DNNL_ARG_SCRATCHPAD 80

/// Gradient (diff) of the source argument #0.
#define DNNL_ARG_DIFF_SRC_0 129
/// A special mnemonic for primitives that have a single diff source argument.
/// An alias for #DNNL_ARG_DIFF_SRC_0.
#define DNNL_ARG_DIFF_SRC DNNL_ARG_DIFF_SRC_0
/// A special mnemonic for gradient (diff) of RNN input vector. An alias for
/// #DNNL_ARG_DIFF_SRC_0.
#define DNNL_ARG_DIFF_SRC_LAYER DNNL_ARG_DIFF_SRC_0

/// Gradient (diff) of the source argument #1.
#define DNNL_ARG_DIFF_SRC_1 130
/// A special mnemonic for gradient (diff) of RNN input recurrent hidden state
/// vector. An alias for #DNNL_ARG_DIFF_SRC_1.
#define DNNL_ARG_DIFF_SRC_ITER DNNL_ARG_DIFF_SRC_1

/// Gradient (diff) of the source argument #2.
#define DNNL_ARG_DIFF_SRC_2 131
/// A special mnemonic for gradient (diff) of RNN input recurrent cell state
/// vector. An alias for #DNNL_ARG_DIFF_SRC_1.
#define DNNL_ARG_DIFF_SRC_ITER_C DNNL_ARG_DIFF_SRC_2

/// Gradient (diff) of the destination argument #0.
#define DNNL_ARG_DIFF_DST_0 145
/// A special mnemonic for primitives that have a single diff destination
/// argument. An alias for #DNNL_ARG_DIFF_DST_0.
#define DNNL_ARG_DIFF_DST DNNL_ARG_DIFF_DST_0
/// A special mnemonic for gradient (diff) of RNN output vector. An alias for
/// #DNNL_ARG_DIFF_DST_0.
#define DNNL_ARG_DIFF_DST_LAYER DNNL_ARG_DIFF_DST_0

/// Gradient (diff) of the destination argument #1.
#define DNNL_ARG_DIFF_DST_1 146
/// A special mnemonic for gradient (diff) of RNN input recurrent hidden state
/// vector. An alias for #DNNL_ARG_DIFF_DST_1.
#define DNNL_ARG_DIFF_DST_ITER DNNL_ARG_DIFF_DST_1

/// Gradient (diff) of the destination argument #2.
#define DNNL_ARG_DIFF_DST_2 147
/// A special mnemonic for gradient (diff) of RNN input recurrent cell state
/// vector. An alias for #DNNL_ARG_DIFF_DST_2.
#define DNNL_ARG_DIFF_DST_ITER_C DNNL_ARG_DIFF_DST_2

/// Gradient (diff) of the weights argument #0.
#define DNNL_ARG_DIFF_WEIGHTS_0 161
/// A special mnemonic for primitives that have a single diff weights
/// argument. Alias for #DNNL_ARG_DIFF_WEIGHTS_0.
#define DNNL_ARG_DIFF_WEIGHTS DNNL_ARG_DIFF_WEIGHTS_0
/// A special mnemonic for diff of scale and shift argument of normalization
/// primitives. Alias for #DNNL_ARG_DIFF_WEIGHTS_0.
#define DNNL_ARG_DIFF_SCALE_SHIFT DNNL_ARG_DIFF_WEIGHTS_0
/// A special mnemonic for diff of RNN weights applied to the layer input. An
/// alias for #DNNL_ARG_DIFF_WEIGHTS_0.
#define DNNL_ARG_DIFF_WEIGHTS_LAYER DNNL_ARG_DIFF_WEIGHTS_0

/// Gradient (diff) of the weights argument #1.
#define DNNL_ARG_DIFF_WEIGHTS_1 162
/// A special mnemonic for diff of RNN weights applied to the recurrent input.
/// An alias for #DNNL_ARG_DIFF_WEIGHTS_1.
#define DNNL_ARG_DIFF_WEIGHTS_ITER DNNL_ARG_DIFF_WEIGHTS_1

/// Gradient (diff) of the bias tensor argument.
#define DNNL_ARG_DIFF_BIAS 169

/// Output scaling factors provided at execution time.
#define DNNL_ARG_ATTR_OUTPUT_SCALES 513

/// Starting index for source arguments for primitives that take a variable
/// number of source arguments.
#define DNNL_ARG_MULTIPLE_SRC 1024
/// Starting index for destination arguments for primitives that produce a
/// variable number of destination arguments.
#define DNNL_ARG_MULTIPLE_DST 2048

/// Zero points provided at execution time.
#define DNNL_ARG_ATTR_ZERO_POINTS 4096

/// A structure that contains an index and a memory object, and is used to pass
/// arguments to dnnl_primitive_execute().
typedef struct {
    int arg; ///< An argument index, e.g. DNNL_ARG_SRC
    dnnl_memory_t memory; ///< Input/output memory
} dnnl_exec_arg_t;

/// @} dnnl_api_primitives_common

/// @addtogroup dnnl_api_primitives_common
/// @{

/// Primitive descriptor query specification
///
/// For generic function dnnl_primitive_desc_query(), the type of result must
/// agree with the queried argument. The correspondence table:
///
/// Query kind                      | Type of query result
/// --------------------------------|-----------------------------
/// #dnnl_query_engine              | #dnnl_engine_t *
/// #dnnl_query_scratchpad_engine   | #dnnl_engine_t *
/// #dnnl_query_primitive_kind      | #dnnl_primitive_kind_t *
/// dnnl_query_*_s32                | int *
/// dnnl_query_*_s64                | #dnnl_dim_t * (same as int64_t *)
/// dnnl_query_*_f64                | double *
/// dnnl_query_*_str                | const char **
/// #dnnl_query_op_d                | #const_dnnl_op_desc_t *
/// dnnl_query_*_md                 | const #dnnl_memory_desc_t **
/// dnnl_query_*_\<op\>_d           | const dnnl_\<op\>_desc_t **
/// dnnl_query_*_pd                 | #const_dnnl_primitive_desc_t *
///
/// @note
///     Rule of thumb: all opaque types and structures are returned by
///     reference. All numbers are returned by value.
///
/// @warning
///     All returned references point to constant objects and are valid only
///     during the lifetime of the queried primitive descriptor. Returned objects
///     must not be destroyed by the user. If you need to keep the object longer
///     than the lifetime of the queried primitive descriptor, use
///     dnnl_primitive_desc_clone() to make a copy.
typedef enum {
    dnnl_query_undef = 0, ///< no query

    dnnl_query_engine, ///< execution engine
    dnnl_query_primitive_kind, ///< primitive kind

    dnnl_query_num_of_inputs_s32, ///< number of inputs expected
    dnnl_query_num_of_outputs_s32, ///< number of outputs expected

    dnnl_query_time_estimate_f64, ///< runtime estimation (seconds)
    dnnl_query_memory_consumption_s64, ///< memory consumption -- extra
    ///  (scratch) memory, additional to
    ///  all inputs and outputs memory
    ///  (bytes)

    dnnl_query_scratchpad_engine, ///< scratchpad engine -- engine to be used
    ///  for creating scratchpad memory

    dnnl_query_impl_info_str, ///< implementation name

    dnnl_query_reorder_src_engine, ///< source engine
    dnnl_query_reorder_dst_engine, ///< destination engine

    dnnl_query_prop_kind, ///< propagation kind

    // memory and op descriptor section
    dnnl_query_some_d = 64, ///< stub
    dnnl_query_op_d, ///< op descriptor
    dnnl_query_convolution_d, ///< convolution descriptor
    dnnl_query_deconvolution_d, ///< deconvolution descriptor
    dnnl_query_shuffle_d, ///< shuffle descriptor
    dnnl_query_eltwise_d, ///< eltwise descriptor
    dnnl_query_softmax_d, ///< softmax descriptor
    dnnl_query_pooling_d, ///< pooling descriptor
    dnnl_query_lrn_d, ///< lrn descriptor
    dnnl_query_batch_normalization_d, ///< batch normalization descriptor
    dnnl_query_layer_normalization_d, ///< layer normalization descriptor
    dnnl_query_inner_product_d, ///< inner product descriptor
    dnnl_query_rnn_d, ///< rnn descriptor
    dnnl_query_gemm_d, ///< GEMM descriptor (internal)
    dnnl_query_binary_d, ///< binary descriptor
    dnnl_query_logsoftmax_d, ///< logsoftmax descriptor
    dnnl_query_matmul_d, ///< matrix multiplication (matmul) descriptor
    dnnl_query_resampling_d, ///< resampling descriptor

    // memory descriptor section
    dnnl_query_some_md = 128, ///< stub
    dnnl_query_src_md, ///< source memory desc
    dnnl_query_diff_src_md, ///< source gradient memory desc
    dnnl_query_weights_md, ///< weights memory descriptor desc
    dnnl_query_diff_weights_md, ///< weights grad. memory desc
    dnnl_query_dst_md, ///< destination memory desc
    dnnl_query_diff_dst_md, ///< destination grad. memory desc
    dnnl_query_workspace_md, ///< workspace memory desc
    dnnl_query_scratchpad_md, ///< scratchpad memory desc
    dnnl_query_exec_arg_md = 255, ///< memory desc of an execute argument
} dnnl_query_t;

/// @} dnnl_api_primitives_common

/// @} dnnl_api_primitives

/// @addtogroup dnnl_api_stream
/// @{

/// @brief Stream flags.
typedef enum {
    /// Default order execution. Either in-order or out-of-order depending on
    /// the runtime.
    dnnl_stream_default_order = 0x1U,
    /// In-order execution.
    dnnl_stream_in_order = 0x2U,
    /// Out-of-order execution.
    dnnl_stream_out_of_order = 0x4U,
    /// Default stream configuration.
    dnnl_stream_default_flags = dnnl_stream_default_order,
} dnnl_stream_flags_t;

/// @struct dnnl_stream
/// An opaque structure to describe an execution stream.
struct dnnl_stream;
/// An execution stream handle.
typedef struct dnnl_stream *dnnl_stream_t;
/// A constant execution stream handle.
typedef const struct dnnl_stream *const_dnnl_stream_t;

/// @} dnnl_api_stream

/// @addtogroup dnnl_api_service
/// @{

/// No runtime (disabled)
#define DNNL_RUNTIME_NONE 0u

/// Sequential runtime (CPU only)
#define DNNL_RUNTIME_SEQ 1u

/// OpenMP runtime (CPU only)
#define DNNL_RUNTIME_OMP 2u

/// TBB runtime (CPU only)
#define DNNL_RUNTIME_TBB 4u

/// OpenCL runtime
#define DNNL_RUNTIME_OCL 256u

/// Structure containing version information as per [Semantic
/// Versioning](https://semver.org)
typedef struct {
    int major; ///< Major version
    int minor; ///< Minor version
    int patch; ///< Patch version
    const char *hash; ///< Git hash of the sources (may be absent)
    unsigned cpu_runtime; ///< CPU runtime
    unsigned gpu_runtime; ///< GPU runtime
} dnnl_version_t;

/// Disable profiling completely
#define DNNL_JIT_PROFILE_NONE 0u

/// Enable VTune integration
#define DNNL_JIT_PROFILE_VTUNE 1u

/// Enable Linux perf integration via perfmap files
#define DNNL_JIT_PROFILE_LINUX_PERFMAP 2u

/// Enable Linux perf integration via jitdump files
#define DNNL_JIT_PROFILE_LINUX_JITDUMP 4u

/// Instruct Linux perf integration via jitdump files to use TSC. @ref
/// DNNL_JIT_PROFILE_LINUX_JITDUMP must be set too for this to take effect.
#define DNNL_JIT_PROFILE_LINUX_JITDUMP_USE_TSC 8u

/// Enable Linux perf integration (both jitdump and perfmap)
#define DNNL_JIT_PROFILE_LINUX_PERF \
    (DNNL_JIT_PROFILE_LINUX_JITDUMP | DNNL_JIT_PROFILE_LINUX_PERFMAP)

/// CPU instruction set flags
typedef enum {
    /// Any ISA (no restrictions)
    dnnl_cpu_isa_all = 0x0,

    /// Intel(R) SSE4.1.
    dnnl_cpu_isa_sse41 = 0x1,

    /// Intel(R) Advanced Vector Extensions.
    dnnl_cpu_isa_avx = 0x3,

    /// Intel(R) Advanced Vector Extensions 2.
    dnnl_cpu_isa_avx2 = 0x7,

    /// Intel(R) Advanced Vector Extensions 512 subset for Intel(R) Xeon
    /// Phi(TM) Processors x200 Series.
    dnnl_cpu_isa_avx512_mic = 0xf,

    /// Intel(R) Advanced Vector Extensions 512 subset for Intel(R) Xeon
    /// Phi(TM) Processors 7235, 7285, 7295 Series.
    dnnl_cpu_isa_avx512_mic_4ops = 0x1f,

    /// Intel(R) Advanced Vector Extensions 512 for Intel(R) Xeon(R) Processor
    /// Scalable Family and Intel(R) Core(TM) processor family.
    dnnl_cpu_isa_avx512_core = 0x27,

    /// Intel(R) Advanced Vector Extensions 512 with Intel(R) DL Boost Support
    /// for Intel(R) Xeon(R) Processor Scalable Family and Intel(R) Core(TM)
    /// processor family.
    dnnl_cpu_isa_avx512_core_vnni = 0x67,

    /// Intel(R) Advanced Vector Extensions 512 with Intel(R) DL Boost and
    /// Bfloat16 Support for Intel(R) Xeon(R) Processor Scalable Family and
    /// Intel(R) Core(TM) processor family.
    dnnl_cpu_isa_avx512_core_bf16 = 0xe7,
} dnnl_cpu_isa_t;

/// @} dnnl_api_service

/// @} dnnl_api

#ifdef __cplusplus
}
#endif

#endif
