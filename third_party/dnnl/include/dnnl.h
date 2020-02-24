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
/// C API

#ifndef DNNL_H
#define DNNL_H

#include "dnnl_config.h"
#include "dnnl_types.h"
#include "dnnl_version.h"

/// @cond DO_NOT_DOCUMENT_THIS
#if DNNL_GPU_RUNTIME == DNNL_RUNTIME_OCL
#include <CL/cl.h>
#endif
/// @endcond

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup dnnl_api
/// @{

/// @addtogroup dnnl_api_primitives
/// @{

/// @addtogroup dnnl_api_primitives_common
/// @{

/// Creates a primitive descriptor iterator.
///
/// @param iterator Output primitive descriptor iterator.
/// @param op_desc Operation descriptor.
/// @param attr Primitive attributes (can be NULL).
/// @param engine Engine to use.
/// @param hint_forward_primitive_desc For backward propagation: primitive
///     descriptor for a respective forward propagation primitive. Pass NULL
///     for forward propagation.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_desc_iterator_create(
        dnnl_primitive_desc_iterator_t *iterator, const_dnnl_op_desc_t op_desc,
        const_dnnl_primitive_attr_t attr, dnnl_engine_t engine,
        const_dnnl_primitive_desc_t hint_forward_primitive_desc);

/// Advances the primitive descriptor iterator to point to the next available
/// implementation.
///
/// @param iterator A primitive descriptor iterator to advance.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
/// @returns #dnnl_iterator_ends if no more implementations available.
dnnl_status_t DNNL_API dnnl_primitive_desc_iterator_next(
        dnnl_primitive_desc_iterator_t iterator);

/// Fetches the current primitive descriptor from a primitive descriptor
/// iterator.
///
/// @note
///     The user is responsible for deleting the resulting primitive
///     descriptor using dnnl_primitive_desc_destroy().
///
/// @param iterator A primitive descriptor iterator.
/// @returns A primitive descriptor.
dnnl_primitive_desc_t DNNL_API dnnl_primitive_desc_iterator_fetch(
        const_dnnl_primitive_desc_iterator_t iterator);

/// Destroys a primitive descriptor iterator.
///
/// @param iterator Primitive descriptor iterator to destroy.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_desc_iterator_destroy(
        dnnl_primitive_desc_iterator_t iterator);

/// Creates a primitive descriptor. This function is equivalent to a sequence
/// of #dnnl_primitive_desc_iterator_create() and
/// #dnnl_primitive_desc_iterator_fetch(). In other words, the library will
/// pick the first suitable implementation.
///
/// @param primitive_desc Output primitive descriptor.
/// @param op_desc Operation descriptor.
/// @param attr Primitive attributes (can be NULL).
/// @param engine Engine to use.
/// @param hint_forward_primitive_desc For backward propagation: primitive
///     descriptor for a respective forward propagation primitive. Pass NULL
///     for forward propagation.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_desc_create(
        dnnl_primitive_desc_t *primitive_desc, const_dnnl_op_desc_t op_desc,
        const_dnnl_primitive_attr_t attr, dnnl_engine_t engine,
        const_dnnl_primitive_desc_t hint_forward_primitive_desc);

/// Clones a primitive descriptor. The resulting primitive descriptor must be
/// destroyed separately.
///
/// @param primitive_desc Output primitive descriptor.
/// @param existing_primitive_desc Primitive descriptor to clone.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_desc_clone(
        dnnl_primitive_desc_t *primitive_desc,
        const_dnnl_primitive_desc_t existing_primitive_desc);

/// Returns a constant reference to the attributes of a primitive descriptor.
///
/// @warning
///     It is an error to destroy the resulting @p attr.
///
/// @warning
///     The lifetime of an @p attr is the same as that of a @p
///     primitive_desc, so it is an error to use the @p attr once the @p
///     primitive_desc has been destroyed.
///
/// @param primitive_desc Primitive descriptor.
/// @param attr Ouput primitive attributes.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_desc_get_attr(
        const_dnnl_primitive_desc_t primitive_desc,
        const_dnnl_primitive_attr_t *attr);

/// Destroys a primitive descriptor.
///
/// @param primitive_desc Primitive descriptor to destroy.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_desc_destroy(
        dnnl_primitive_desc_t primitive_desc);

/// Queries a primitive descriptor for various pieces of information.
///
/// The most common use case is to query a primitive descriptor, created with
/// source, weights, and destination memory descriptors with format tags set
/// to #dnnl_format_tag_any, for the corresponding memory descriptors (in this
/// case the @p what is set to #dnnl_query_src_md, #dnnl_query_weights_md, and
/// #dnnl_query_dst_md respectively) so that it is possible to create memory
/// objects and reorder primitives if necessary.
///
/// Another typical use case is to query a primitive descriptor for workspace
/// memory descriptor (with @p what set to #dnnl_query_workspace_md). If this
/// query returns #dnnl_not_required status, then workspace memory is not
/// required.
///
/// @note
///     When querying for a memory descriptor for a scratchpad, a workspace,
///     or an optional parameter, the query will return a pointer to a zero
///     memory descriptor if the parameter is not needed.
///
/// A few other use cases:
///  - query a primitive descriptor for the underlying operation descriptor
///    (#dnnl_query_convolution_d, #dnnl_query_eltwise_d, #dnnl_query_rnn_d,
///    etc.)
///  - query a primitive descriptor for the implementation information string
///    (#dnnl_query_impl_info_str)
///  - query a primitive descriptor for the number of inputs and outputs
///    (#dnnl_query_num_of_inputs_s32 and #dnnl_query_num_of_outputs_s32
///    respectively)
///
/// @sa dnnl_query_t for more options
///
/// @param primitive_desc Primitive descriptor.
/// @param what Parameter to query.
/// @param index Index of the parameter to query for.
/// @param result Output result. The type depends on the query. For example,
///     it must be a @c dnnl_memory_desc_t* if querying for a memory
///     descriptor.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_desc_query(
        const_dnnl_primitive_desc_t primitive_desc, dnnl_query_t what,
        int index, void *result);

/// Queries primitive descriptor for a memory descriptor.
///
/// @note
///     This function is a convenience version of
///     #dnnl_primitive_desc_query().
///
/// @param primitive_desc Primitive descriptor.
/// @param what Kind of memory descriptor parameter to query for.
/// @param index Index of the parameter to query.
/// @returns A pointer to the requested memory descriptor.
/// @returns A pointer to a zero memory descriptor if the parameter is not
///          needed.
/// @returns NULL in case of any error.
///
const dnnl_memory_desc_t DNNL_API *dnnl_primitive_desc_query_md(
        const_dnnl_primitive_desc_t primitive_desc, dnnl_query_t what,
        int index);

/// Queries primitive descriptor for a signed 32bit int.
///
/// @note
///     This function is a convenience version of
///     #dnnl_primitive_desc_query().
///
/// @param primitive_desc Primitive descriptor.
/// @param what Kind of the value to query for.
/// @param index Index of the parameter to query.
/// @returns The requested value.
/// @returns 0 in case of any error (in particular if the queried entity is
///     not of type int32_t). Note that 0 may also be the actual returned
///     value.
int DNNL_API dnnl_primitive_desc_query_s32(
        const_dnnl_primitive_desc_t primitive_desc, dnnl_query_t what,
        int index);

/// Creates a primitive.
///
/// @param primitive Output primitive.
/// @param primitive_desc Primitive descriptor used to create the primitive.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_create(dnnl_primitive_t *primitive,
        const_dnnl_primitive_desc_t primitive_desc);

/// Executes a primitive.
///
/// @param primitive Primitive to execute.
/// @param stream Stream to use.
/// @param nargs Number of arguments.
/// @param args Array of arguments. Each argument is an
///     <index, #dnnl_memory_t> pair. The index is one of the `DNNL_ARG_*`
///     values such as `DNNL_ARG_SRC`. Unless runtime shapes are used (see
///     #DNNL_RUNTIME_DIM_VAL), the memory object must have the same memory
///     descriptor as that returned by
///     #dnnl_primitive_desc_query_md(#dnnl_query_exec_arg_md, index).
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_execute(const_dnnl_primitive_t primitive,
        dnnl_stream_t stream, int nargs, const dnnl_exec_arg_t *args);

/// Retrieves a constant reference to the primitive descriptor of a given
/// primitive.
///
/// @warning
///     It is an error to destroy the returned object. It is owned by the
///     primitive. The @c const qualifier of the returned object prevents
///     such attempts.
///
/// @param primitive Primitive to query for the primitive descriptor.
/// @param primitive_desc Output primitive descriptor.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_get_primitive_desc(
        const_dnnl_primitive_t primitive,
        const_dnnl_primitive_desc_t *primitive_desc);

/// Destroys a primitive.
///
/// @param primitive The primitive to destroy.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_destroy(dnnl_primitive_t primitive);

/// @} dnnl_api_primitives_common

/// @addtogroup dnnl_api_attributes
/// @{

/// Creates an empty (default) primitive attributes with all the parameters
/// set to their default values.
///
/// Empty attributes are implied whenever the respective argument is NULL.
///
/// @param attr Output primitive attributes.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_attr_create(dnnl_primitive_attr_t *attr);

/// Clones primitive attributes.
///
/// @param attr Output primitive attributes.
/// @param existing_attr Primitive attributes to clone.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_attr_clone(
        dnnl_primitive_attr_t *attr, const_dnnl_primitive_attr_t existing_attr);

/// Destroys primitive attributes.
///
/// @param attr Primitive attributes to destroy.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_attr_destroy(dnnl_primitive_attr_t attr);

/// Returns the primitive attributes scratchpad mode.
///
/// @param attr Primitive attributes.
/// @param mode Output scratchpad mode.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_attr_get_scratchpad_mode(
        const_dnnl_primitive_attr_t attr, dnnl_scratchpad_mode_t *mode);

/// Sets primitive attributes scratchpad mode.
///
/// @param attr Primitive attributes.
/// @param mode Scratchpad mode. The possible values are:
///     #dnnl_scratchpad_mode_library (default) and
///     #dnnl_scratchpad_mode_user.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_attr_set_scratchpad_mode(
        dnnl_primitive_attr_t attr, dnnl_scratchpad_mode_t mode);

/// Returns primitive attributes output scaling factors correspondence mask
/// and values.
///
/// @warning
///     The @p scales array is an internal part of the primitive attributes
///     @p attr, so it is an error to modify or destroy the @p scales array.
///
/// @warning
///     The lifetime of @p scales array is the same as that of the primitive
///     attributes @p attr to which it belongs, so it is an error to use
///     @p scales after @p attr is destroyed.
///
/// @param attr Primitive attributes.
/// @param count Output length of the array of scaling factors @p scales.
/// @param mask Output scaling factors correspondence mask that defines the
///     correspondence between the output tensor dimensions and the @p scales
///     vector. The set i-th bit indicates that a dedicated output scaling
///     factor is used for each index along that dimension. The mask value of
///     0 implies a common output scaling factor for the whole output tensor.
/// @param scales Output pointer to a constant array of scaling factors.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_attr_get_output_scales(
        const_dnnl_primitive_attr_t attr, dnnl_dim_t *count, int *mask,
        const float **scales);

/// Sets output scaling factors correspondence mask and values.
///
/// @note
///     The order of dimensions does not depend on how elements are laid
///     out in memory. For example:
///     - for a 2D CNN activations tensor the order is always (n, c)
///     - for a 4D CNN activations tensor the order is always (n, c, h, w)
///     - for a 5D CNN weights tensor the order is always
///        (g, oc, ic, kh, kw)
///
/// Example usage:
/// @code
///     int mb = 32, oc = 32, oh = 14, ow = 14; // convolution output params
///     float scales[oc] = { ... }; // unique output scales per output channel
///     int oc_dim = 1; // mb_dim = 0, channel_dim = 1, height_dim = 2, ...
///
///     dnnl_convolution_desc_t conv_d; // create a convolution descriptor
///
///     dnnl_primitive_attr_t attr;
///     dnnl_primitive_attr_create(&attr); // create primitive attributes
///     dnnl_primitive_attr_set_output_scales(attr, oc, 1 << oc_dim, scales);
///
///     dnnl_primitive_desc_t conv_pd;
///     dnnl_primitive_desc_create(&conv_pd, &conv_d, attr, engine, NULL);
/// @endcode
///
/// @param attr Primitive attributes.
/// @param count Length of the array of scaling factors @p scales.
/// @param mask Scaling factors correspondence mask that defines the
///     correspondence between the output tensor dimensions and the @p scales
///     array. The set i-th bit indicates that a dedicated output scaling
///     factor is used for each index along that dimension. The mask value of
///     0 implies a common output scaling factor for the whole output tensor.
/// @param scales Array of output scaling factors. If the output scaling
///     factors are known at the time of this call, this array must contain @p
///     count values and the following equality must hold:
///     \f[count = \prod\limits_{d \in mask} output.dims[d].\f]
///     Violations can only be detected when the attributes are used to create
///     a primitive descriptor.
///     If the output scaling factors are not known at the time of the call,
///     this array must contain a single #DNNL_RUNTIME_F32_VAL value and the
///     output scaling factors must be passed at execution time as an argument
///     with index #DNNL_ARG_ATTR_OUTPUT_SCALES.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_attr_set_output_scales(
        dnnl_primitive_attr_t attr, dnnl_dim_t count, int mask,
        const float *scales);

/// Returns primitive attributes scaling factors correspondence mask and values
/// for a given memory argument.
///
/// @warning
///     The output @p scales array is an internal part of the primitive
///     attributes @p attr, so it is an error to modify or destroy the @p
///     scales array.
///
/// @warning
///     The lifetime of the @p scales array is the same as that of the primitive
///     attributes @p attr to which it belongs, so it is an error to use @p
///     scales after @p attr is destroyed.
///
///
/// @param attr Primitive attributes.
/// @param arg Parameter argument index as passed to the
///     dnnl_primitive_execute() call.
/// @param count Output length of the array of scaling factors @p scales.
/// @param mask Output scaling factors correspondence mask that defines the
///     correspondence between the output tensor dimensions and the @p
///     scales array. The set i-th bit indicates that a dedicated output scaling
///     factor is used for each index along that dimension. The mask value of 0
///     implies a common scaling factor for the whole output tensor.
/// @param scales Output pointer to a constant array of float scaling factors.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_attr_get_scales(
        dnnl_primitive_attr_t attr, int arg, dnnl_dim_t *count, int *mask,
        const float **scales);

/// Sets primitive attributes scaling factors for primitive operations for a
/// given memory argument.
///
/// @sa dnnl_primitive_attr_set_output_scales
///
///
/// @param attr Primitive attributes.
/// @param arg Parameter argument index as passed to the
///     dnnl_primitive_execute() call.
/// @param count Length of the array of scaling factors @p scales.
/// @param mask Scaling factors correspondence mask that defines the
///     correspondence between the tensor dimensions and the @p scales array.
///     The set i-th bit indicates that a dedicated scaling factor is used for
///     each index along that dimension. Set the mask to 0 to use a common
///     scaling factor for the whole output tensor.
/// @param scales Constant array of float scaling factors. This array must
///     contain @p count scales and the following equality must hold:
///     \f[count = \prod\limits_{d \in mask} output.dims[d].\f]
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_attr_set_scales(
        dnnl_primitive_attr_t attr, int arg, dnnl_dim_t count, int mask,
        const float *scales);

/// Returns @p count, correspondence zero point @p mask, and a pointer to a
/// constant int32_t array of @p zero_points for given @p attr and memory
/// argument (index), previously set by dnnl_primitive_attr_set_zero_points.
///
/// @warning
///     The output @p zero_points array is an internal part of the primitive
///     attributes @p attr, so it is an error to modify or destroy the @p
///     zero_points array.
///
/// @warning
///     The lifetime of @p zero_points array is the same as that of the
///     primitive attributes @p attr to which it belongs, so it is an error
///     to use @p zero_points after @p attr is destroyed.
///
///
/// @param attr Primitive attributes.
/// @param arg Parameter argument index as passed to the
///     dnnl_primitive_execute() call.
/// @param count Output length of the array of zero points @p zero_points.
/// @param mask Output zero points correspondence mask that defines the
///     correspondence between the output tensor dimensions and the @p
///     zero_points array. The set i-th bit indicates that a dedicated output
///     zero point is used for each index along that dimension. The mask
///     value of 0 implies a common zero point for the whole output tensor.
/// @param zero_points Output pointer to a constant array of int32_t zero
///     points.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_attr_get_zero_points(
        const_dnnl_primitive_attr_t attr, int arg, dnnl_dim_t *count, int *mask,
        const int32_t **zero_points);

/// Sets primitive attributes zero points for primitive operations for a given
/// memory argument.
///
/// @sa dnnl_primitive_attr_set_output_scales
///
///
/// @param attr Primitive attributes.
/// @param arg Parameter argument index as passed to the
///     dnnl_primitive_execute() call.
/// @param count Length of the array of zero points @p zero_points.
/// @param mask Zero point correspondence mask that defines the
///     correspondence between the tensor dimensions and the @p
///     zero_points array. The set i-th bit indicates that a dedicated
///     zero point is used for each index along that dimension. Set the
///     mask to 0 to use a common zero point for the whole output tensor.
/// @param zero_points Constant array of int32_t zero points. If the zero
///     points are known at the time of this call, this array must contain @p
///     count zero points and the following equality must hold:
///     \f[count = \prod\limits_{d \in mask} output.dims[d].\f]
///     If the zero points are not known at the time of the call, this array
///     must contain a single #DNNL_RUNTIME_S32_VAL and the zero points must
///     be passed at execution time as an argument with index
///     #DNNL_ARG_ATTR_ZERO_POINTS.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_attr_set_zero_points(
        dnnl_primitive_attr_t attr, int arg, dnnl_dim_t count, int mask,
        const int32_t *zero_points);

/// Returns primitive attributes post-ops.
///
/// @warning
///     The output @p post_ops points to the internal @p attr field, so it is
///     an error to modify or destroy them. The lifetime of @p post_ops is
///     the same as that of the @p attr it belongs to, so it is an error to
///     use @p post_ops after @p attr has been destroyed.
///
/// @param attr Primitive attributes.
/// @param post_ops Output post-ops.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_attr_get_post_ops(
        const_dnnl_primitive_attr_t attr, const_dnnl_post_ops_t *post_ops);

/// Sets primitive attributes post-ops.
///
/// @note
///     There is no way to check whether the post-ops would be supported by
///     the target primitive. Any error will be reported by the
///     dnnl_primitive_desc_create() function call.
///
/// @param attr Primitive attributes.
/// @param post_ops Post-ops to set.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_attr_set_post_ops(
        dnnl_primitive_attr_t attr, const_dnnl_post_ops_t post_ops);

/// Creates empty post-ops sequence.
///
/// @param post_ops Output post-ops.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_post_ops_create(dnnl_post_ops_t *post_ops);

/// Destroys post-ops.
///
/// @param post_ops Post-ops to destroy.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_post_ops_destroy(dnnl_post_ops_t post_ops);

/// Returns the length of post-ops.
///
/// @param post_ops Post-ops.
/// @returns The number of post-ops entries.
int DNNL_API dnnl_post_ops_len(const_dnnl_post_ops_t post_ops);

/// Returns the kind of a post-op entry.
///
/// @param post_ops Post-ops.
/// @param index Post-op entry index.
/// @returns The kind of the post-op with the specified index.
/// @returns #dnnl_undefined_primitive if there is no post-op at the specified
///     index.
dnnl_primitive_kind_t DNNL_API dnnl_post_ops_get_kind(
        const_dnnl_post_ops_t post_ops, int index);

/// Appends an accumulation (sum) to post-ops. Prior to accumulating the
/// result, the previous value is multiplied by a scale.
///
/// The kind of this post-op is #dnnl_sum.
///
/// This feature may improve performance for cases like residual learning
/// blocks, where the result of convolution is accumulated to the previously
/// computed activations. The parameter @p scale may be used for the
/// integer-based computations when the result and previous activations have
/// different logical scaling factors.
///
/// In the simplest case when the accumulation is the only post-op, the
/// computations would be:
///
///     dst[:] <- scale * dst[:] + op(...) // instead of dst[:] <- op(...)
///
/// @note
///     This post-op executes in-place and does not change the
///     destination layout.
///
/// @param post_ops Post-ops.
/// @param scale Accumulation scaling factor.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_post_ops_append_sum(
        dnnl_post_ops_t post_ops, float scale);

/// Returns the parameters of an accumulation (sum) post-op.
///
/// @param post_ops Post-ops.
/// @param index Index of the sum post-op.
/// @param scale Output accumulation scaling factor.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
/// @returns #dnnl_invalid_arguments if @p index does not refer to a sum
///     post-op.
dnnl_status_t DNNL_API dnnl_post_ops_get_params_sum(
        const_dnnl_post_ops_t post_ops, int index, float *scale);

/// Appends an elementwise post-op.
///
/// The kind of this post operation is #dnnl_eltwise.
///
/// In the simplest case when the elementwise is the only post operation, the
/// computations would be:
///
///     dst[:] <- scale * eltwise_op (op(...)) // instead of dst[:] <- op(...)
///
/// where eltwise_op is configured with the given parameters.
///
/// @param post_ops Post-ops.
/// @param scale Scaling factor.
/// @param alg_kind Elementwise algorithm for the post-op.
/// @param alpha Alpha parameter for the elementwise algorithm.
/// @param beta Beta parameter for the elementwise algorithm.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_post_ops_append_eltwise(dnnl_post_ops_t post_ops,
        float scale, dnnl_alg_kind_t alg_kind, float alpha, float beta);

/// Returns the parameters of an elementwise post-up.
///
/// @param post_ops Post-ops.
/// @param index Index of the elementwise post-op.
/// @param scale Output scaling factor.
/// @param alg_kind Output elementwise algorithm kind.
/// @param alpha Output alpha parameter for the elementwise algorithm.
/// @param beta Output beta parameter for the elementwise algorithm.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
/// @returns #dnnl_invalid_arguments if @p index does not refer to an
///     elementwise post-op.
dnnl_status_t DNNL_API dnnl_post_ops_get_params_eltwise(
        const_dnnl_post_ops_t post_ops, int index, float *scale,
        dnnl_alg_kind_t *alg_kind, float *alpha, float *beta);

/// @} dnnl_api_attributes

/// @} dnnl_api_primitives

/// @addtogroup dnnl_api_memory
/// @{

/// Initializes a memory descriptor using dimensions and strides.
///
/// @note
///     As always, the logical order of dimensions corresponds to the `abc...`
///     format tag, and the physical meaning of the dimensions depends on both
///     the primitive that consumes the memory and the context of that
///     consumption.
///
/// @param memory_desc Output memory descriptor.
/// @param ndims Number of dimensions
/// @param dims Array of dimensions.
/// @param data_type Elements data type.
/// @param strides Strides in each dimension.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_memory_desc_init_by_strides(
        dnnl_memory_desc_t *memory_desc, int ndims, const dnnl_dims_t dims,
        dnnl_data_type_t data_type, const dnnl_dims_t strides);

/// Initializes a memory descriptor using dimensions and memory format tag.
///
/// @note
///     As always, the logical order of dimensions corresponds to the `abc...`
///     format tag, and the physical meaning of the dimensions depends on both
///     the primitive that consumes the memory and the context of that
///     consumption.
///
/// @param memory_desc Output memory descriptor.
/// @param ndims Number of dimensions
/// @param dims Array of dimensions.
/// @param data_type Elements data type.
/// @param tag Memory format tag. Can be #dnnl_format_tag_any which would
///     allow a primitive to chose the final memory format. In this case the
///     format_kind field of the memory descriptor would be set to
///     #dnnl_format_kind_any.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_memory_desc_init_by_tag(
        dnnl_memory_desc_t *memory_desc, int ndims, const dnnl_dims_t dims,
        dnnl_data_type_t data_type, dnnl_format_tag_t tag);

/// Initializes a memory descriptor for a region inside an area
/// described by an existing memory descriptor.
///
/// @warning
///     Some combinations of physical memory layout and/or offsets or dims may
///     result in a failure to create a submemory.
//
/// @param memory_desc Output memory descriptor.
/// @param parent_memory_desc An existing memory descriptor.
/// @param dims Sizes of the region.
/// @param offsets Offsets to the region from the encompassing
///     memory object in each dimension
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_memory_desc_init_submemory(
        dnnl_memory_desc_t *memory_desc,
        const dnnl_memory_desc_t *parent_memory_desc, const dnnl_dims_t dims,
        const dnnl_dims_t offsets);

/// Initializes a memory descriptor by reshaping an existing one. The new
/// memory descriptor inherits the data type. This operation is valid only for
/// memory descriptors that have format_kind set to #dnnl_blocked.
///
/// @note
///     Currently reshape is limited to appending 1-sized dimensions to the
///     end of the dimensions array.
///
/// @param out_memory_desc Output memory descriptor.
/// @param in_memory_desc An existing memory descriptor. Must have format_kind
///     set to #dnnl_blocked.
/// @param ndims Number of dimensions for the output memory descriptor.
/// @param dims Dimensions for the output memory descriptor.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_memory_desc_reshape(
        dnnl_memory_desc_t *out_memory_desc,
        const dnnl_memory_desc_t *in_memory_desc, int ndims,
        const dnnl_dims_t dims);

/// Compares two memory descriptors.
///
/// Use this function to identify whether a reorder is required between the
/// two memories
///
/// @param lhs Left-hand side of the comparison.
/// @param rhs Right-hand side of the comparison.
/// @returns 1 if the descriptors are the same.
/// @returns 0 if the descriptors are different.
int DNNL_API dnnl_memory_desc_equal(
        const dnnl_memory_desc_t *lhs, const dnnl_memory_desc_t *rhs);

/// Returns the size of a memory descriptor.
///
/// @param memory_desc Memory descriptor.
/// @returns The number of bytes required for memory described by a memory
///     descriptor.
size_t DNNL_API dnnl_memory_desc_get_size(
        const dnnl_memory_desc_t *memory_desc);

/// Creates a memory object.
///
/// @param memory Output memory object.
/// @param memory_desc Memory descriptor.
/// @param engine Engine to use.
/// @param handle Memory handle:
///     - A pointer to the user- allocated buffer. In this case the library
///       doesn't own the buffer.
///     - DNNL_MEMORY_ALLOCATE special value instructs the library to allocate
///       the buffer for the memory object. In this case the library owns
///       the buffer.
///     - DNNL_MEMORY_NONE to create dnnl_memory without an underlying buffer.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_memory_create(dnnl_memory_t *memory,
        const dnnl_memory_desc_t *memory_desc, dnnl_engine_t engine,
        void *handle);

/// Returns the memory descriptor for a memory object.
///
/// @param memory Memory object.
/// @param memory_desc Output memory descriptor (a copy).
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_memory_get_memory_desc(
        const_dnnl_memory_t memory, const dnnl_memory_desc_t **memory_desc);

/// Returns the engine of a memory object.
///
/// @param memory Memory object.
/// @param engine Output engine on which the memory is located.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_memory_get_engine(
        const_dnnl_memory_t memory, dnnl_engine_t *engine);

/// Maps a memory object and returns a pointer to host-side buffer with a
/// copy of its contents.
///
/// Mapping allows explicit direct access to memory contents for the engines
/// that do not support it implicitly.
///
/// Mapping is an exclusive operation - a memory object cannot be used in
/// other operations until this memory object is unmapped.
///
/// @note
///     Any primitives working with @p memory should be completed before
///     mapping the memory. Use dnnl_stream_wait to synchronize the
///     corresponding execution stream.
///
/// @note
///     dnnl_memory_map_data() and dnnl_memory_unmap_data() function are
///     mainly provided for debug and testing purposes, and its performance
///     may be suboptimal.
///
/// @param memory Memory object.
/// @param mapped_ptr Output pointer to the mapped buffer.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_memory_map_data(
        const_dnnl_memory_t memory, void **mapped_ptr);

/// Unmaps a memory objects and writes any changes to the previously mapped
/// buffer back.
///
/// @note
///     dnnl_memory_map_data() and dnnl_memory_unmap_data() function are
///     mainly provided for debug and testing purposes, and its performance
///     may be suboptimal.
///
/// @param memory Memory object.
/// @param mapped_ptr Pointer to the mapped buffer that must have been
///     obtained using dnnl_memory_map_data() function.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_memory_unmap_data(
        const_dnnl_memory_t memory, void *mapped_ptr);

/// Returns memory object's data handle.
///
/// @param memory Memory object.
/// @param handle Output data handle. For the CPU engine, the data handle is a
///     pointer to the actual data. For OpenCL it is a cl_mem.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_memory_get_data_handle(
        const_dnnl_memory_t memory, void **handle);

/// Sets memory object's data handle.
///
/// @param memory Memory object.
/// @param handle Data handle. For the CPU engine, the data handle is a
///     pointer to the actual data. For OpenCL it is a cl_mem.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_memory_set_data_handle(
        dnnl_memory_t memory, void *handle);

#if DNNL_GPU_RUNTIME == DNNL_RUNTIME_OCL
/// Returns an OpenCL memory object associated with a memory object.
///
/// @param memory Memory object.
/// @param mem_object Output OpenCL memory object.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_memory_get_ocl_mem_object(
        const_dnnl_memory_t memory, cl_mem *mem_object);

/// Sets OpenCL memory object associated with a memory object.
///
/// @param memory Memory object.
/// @param mem_object OpenCL memory object.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_memory_set_ocl_mem_object(
        dnnl_memory_t memory, cl_mem mem_object);
#endif

/// Destroys a memory object.
///
/// @param memory Memory object to destroy.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_memory_destroy(dnnl_memory_t memory);

/// @} dnnl_api_memory

/// @addtogroup dnnl_api_primitives
/// @{

/// @addtogroup dnnl_api_reorder
/// @{

/// Creates a primitive descriptor for a reorder primitive.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///
/// Outputs:
///  - dst (#dnnl_query_dst_md, 0)
///
/// @param reorder_primitive_desc Output primitive descriptor.
/// @param src_desc Source memory descriptor.
/// @param src_engine Engine on which the source memory object will be
///     located.
/// @param dst_desc Destination memory descriptor.
/// @param dst_engine Engine on which the destination memory object
///     will be located.
/// @param attr Primitive attributes to use (can be NULL).
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_reorder_primitive_desc_create(
        dnnl_primitive_desc_t *reorder_primitive_desc,
        const dnnl_memory_desc_t *src_desc, dnnl_engine_t src_engine,
        const dnnl_memory_desc_t *dst_desc, dnnl_engine_t dst_engine,
        const_dnnl_primitive_attr_t attr);

/// @} dnnl_api_reorder

/// @addtogroup dnnl_api_concat
/// @{

/// Creates a primitive descriptor for an out-of-place concatenation
/// primitive.
///
/// Inputs:
///  - src\[0\] (#dnnl_query_src_md, 0)
///  - src\[1\] (#dnnl_query_src_md, 1)
///  - ...
///  - src\[@p n - 1\] (#dnnl_query_src_md, @p n - 1)
///
/// Outputs:
///  - dst (#dnnl_query_dst_md, 0)
///
/// @param concat_primitive_desc Output primitive descriptor.
/// @param dst_desc Destination memory descriptor.
/// @param n Number of source parameters.
/// @param concat_dimension Source tensors will be concatenated over
///     dimension with this index. Note that order of dimensions does
///     not depend on memory format.
/// @param src_descs Array of source memory descriptors with @p n elements.
/// @param attr Primitive attributes to use (can be NULL).
/// @param engine Engine to use.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_concat_primitive_desc_create(
        dnnl_primitive_desc_t *concat_primitive_desc,
        const dnnl_memory_desc_t *dst_desc, int n, int concat_dimension,
        const dnnl_memory_desc_t *src_descs, const_dnnl_primitive_attr_t attr,
        dnnl_engine_t engine);

/// @} dnnl_api_concat

/// @addtogroup dnnl_api_sum
/// @{

/// Creates a primitive descriptor for an (out-of-place) sum primitive.
///
/// Inputs:
///  - src\[0\] (#dnnl_query_src_md, 0)
///  - src\[1\] (#dnnl_query_src_md, 1)
///  - ...
///  - src\[@p n - 1\] (#dnnl_query_src_md, @p n - 1)
///
/// Outputs:
///  - dst (#dnnl_query_dst_md, 0)
///
/// @param sum_primitive_desc Output primitive descriptor.
/// @param dst_desc Destination memory descriptor.
/// @param n Number of source parameters.
/// @param scales Vector of scales to multiply data in each source
///     memory by.
/// @param src_descs Array of source memory descriptors having @p n elements.
/// @param attr Primitive attributes to use (can be NULL).
/// @param engine Engine to use.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_sum_primitive_desc_create(
        dnnl_primitive_desc_t *sum_primitive_desc,
        const dnnl_memory_desc_t *dst_desc, int n, const float *scales,
        const dnnl_memory_desc_t *src_descs, const_dnnl_primitive_attr_t attr,
        dnnl_engine_t engine);

/// @} dnnl_api_sum

/// @addtogroup dnnl_api_binary
/// @{

/// Initializes a descriptor for a binary primitive.
///
/// @note
///     Memory descriptor @p dst_desc is allowed to be initialized with
///     #dnnl_format_tag_any or with format_kind set to #dnnl_format_kind_any.
///
/// @note
///     Both memory descriptors must have the same number of dimensions.
///     Element broadcasting is supported for memory descriptor @p src1_desc
///     and are applied to @ src1_desc dimensions that have size equal to 1.
///
/// Inputs:
///  - src0 (#dnnl_query_src_md, 0)
///  - src1 (#dnnl_query_src_md, 1)
///
/// Outputs:
///  - dst (#dnnl_query_dst_md, 0)
///
/// @param binary_desc Output descriptor for a binary primitive.
/// @param alg_kind Algorithm kind. Valid values are #dnnl_binary_add and
///     #dnnl_binary_mul.
/// @param src0_desc Source 0 memory descriptor.
/// @param src1_desc Source 1 memory descriptor.
/// @param dst_desc Destination memory descriptor.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_binary_desc_init(dnnl_binary_desc_t *binary_desc,
        dnnl_alg_kind_t alg_kind, const dnnl_memory_desc_t *src0_desc,
        const dnnl_memory_desc_t *src1_desc,
        const dnnl_memory_desc_t *dst_desc);

/// @} dnnl_api_binary

/// @addtogroup dnnl_api_convolution
/// @{

/// Initializes a descriptor for a convolution forward propagation primitive.
///
/// @note
///     Memory descriptors are allowed to be initialized with
///     #dnnl_format_tag_any or with format_kind set to #dnnl_format_kind_any.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///  - weights (#dnnl_query_weights_md, 0)
///  - bias (#dnnl_query_weights_md, 1), if created with bias
///
/// Outputs:
///  - dst (#dnnl_query_dst_md, 0)
///
/// @param conv_desc Output descriptor for a convolution primitive.
/// @param prop_kind Propagation kind. Possible values are
///     #dnnl_forward_training and #dnnl_forward_inference.
/// @param alg_kind Convolution algorithm. Possible values are
///     #dnnl_convolution_direct, #dnnl_convolution_winograd,
///     #dnnl_convolution_auto.
/// @param src_desc Source memory descriptor.
/// @param weights_desc Weights memory descriptor.
/// @param bias_desc Bias memory descriptor. Passing NULL, a zero memory
///     descriptor, or a memory descriptor with format_kind set to
///     #dnnl_format_kind_undef disables the bias term.
/// @param dst_desc Destination memory descriptor.
/// @param strides Array of strides for spatial dimension.
/// @param padding_l Array of padding values for low indices for each spatial
///     dimension (front, top, left).
/// @param padding_r Array of padding values for high indices for each spatial
///     dimension (back, bottom, right). Can be NULL in which case padding is
///     considered to be symmetrical.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_convolution_forward_desc_init(
        dnnl_convolution_desc_t *conv_desc, dnnl_prop_kind_t prop_kind,
        dnnl_alg_kind_t alg_kind, const dnnl_memory_desc_t *src_desc,
        const dnnl_memory_desc_t *weights_desc,
        const dnnl_memory_desc_t *bias_desc, const dnnl_memory_desc_t *dst_desc,
        const dnnl_dims_t strides, const dnnl_dims_t padding_l,
        const dnnl_dims_t padding_r);

/// Initializes a descriptor for a dilated convolution forward propagation
/// primitive.
///
/// @note
///     Memory descriptors are allowed to be initialized with
///     #dnnl_format_tag_any or with format_kind set to #dnnl_format_kind_any.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///  - weights (#dnnl_query_weights_md, 0)
///  - bias (#dnnl_query_weights_md, 1), if created with bias
///
/// Outputs:
///  - dst (#dnnl_query_dst_md, 0)
///
/// @param conv_desc Output descriptor for a convolution primitive.
/// @param prop_kind Propagation kind. Possible values are
///     #dnnl_forward_training and #dnnl_forward_inference.
/// @param alg_kind Convolution algorithm. Possible values are
///     #dnnl_convolution_direct, #dnnl_convolution_winograd,
///     #dnnl_convolution_auto.
/// @param src_desc Source memory descriptor.
/// @param weights_desc Weights memory descriptor.
/// @param bias_desc Bias memory descriptor. Passing NULL, a zero memory
///     descriptor, or a memory descriptor with format_kind set to
///     #dnnl_format_kind_undef disables the bias term.
/// @param dst_desc Destination memory descriptor.
/// @param strides Array of strides for spatial dimension.
/// @param dilates Array of dilations for spatial dimension. A zero value
///     means no dilation in the corresponding dimension.
/// @param padding_l Array of padding values for low indices for each spatial
///     dimension (front, top, left).
/// @param padding_r Array of padding values for high indices for each spatial
///     dimension (back, bottom, right). Can be NULL in which case padding is
///     considered to be symmetrical.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_dilated_convolution_forward_desc_init(
        dnnl_convolution_desc_t *conv_desc, dnnl_prop_kind_t prop_kind,
        dnnl_alg_kind_t alg_kind, const dnnl_memory_desc_t *src_desc,
        const dnnl_memory_desc_t *weights_desc,
        const dnnl_memory_desc_t *bias_desc, const dnnl_memory_desc_t *dst_desc,
        const dnnl_dims_t strides, const dnnl_dims_t dilates,
        const dnnl_dims_t padding_l, const dnnl_dims_t padding_r);

/// Initializes a descriptor for a convolution backward propagation primitive.
///
/// @note
///     Memory descriptors are allowed to be initialized with
///     #dnnl_format_tag_any or with format_kind set to #dnnl_format_kind_any.
///
/// Inputs:
///  - diff_dst (#dnnl_query_diff_dst_md, 0)
///  - weights (#dnnl_query_weights_md, 0)
///
/// Outputs:
///  - diff_src (#dnnl_query_diff_src_md, 0)
///
/// @param conv_desc Output descriptor for a convolution primitive.
/// @param alg_kind Convolution algorithm. Possible values are
///     #dnnl_convolution_direct, #dnnl_convolution_winograd,
///     #dnnl_convolution_auto.
/// @param diff_src_desc Diff source memory descriptor.
/// @param weights_desc Weights memory descriptor.
/// @param diff_dst_desc Diff destination memory descriptor.
/// @param strides Array of strides for spatial dimension.
/// @param padding_l Array of padding values for low indices for each spatial
///     dimension (front, top, left).
/// @param padding_r Array of padding values for high indices for each spatial
///     dimension (back, bottom, right). Can be NULL in which case padding is
///     considered to be symmetrical.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_convolution_backward_data_desc_init(
        dnnl_convolution_desc_t *conv_desc, dnnl_alg_kind_t alg_kind,
        const dnnl_memory_desc_t *diff_src_desc,
        const dnnl_memory_desc_t *weights_desc,
        const dnnl_memory_desc_t *diff_dst_desc, const dnnl_dims_t strides,
        const dnnl_dims_t padding_l, const dnnl_dims_t padding_r);

/// Initializes a descriptor for a dilated convolution backward propagation
/// primitive.
///
/// @note
///     Memory descriptors are allowed to be initialized with
///     #dnnl_format_tag_any or with format_kind set to #dnnl_format_kind_any.
///
/// Inputs:
///  - diff_dst (#dnnl_query_diff_dst_md, 0)
///  - weights (#dnnl_query_weights_md, 0)
///
/// Outputs:
///  - diff_src (#dnnl_query_diff_src_md, 0)
///
/// @param conv_desc Output descriptor for a convolution primitive.
/// @param alg_kind Convolution algorithm. Possible values are
///     #dnnl_convolution_direct, #dnnl_convolution_winograd,
///     #dnnl_convolution_auto.
/// @param diff_src_desc Diff source memory descriptor.
/// @param weights_desc Weights memory descriptor.
/// @param diff_dst_desc Diff destination memory descriptor.
/// @param strides Array of strides for spatial dimension.
/// @param dilates Array of dilations for spatial dimension. A zero value
///     means no dilation in the corresponding dimension.
/// @param padding_l Array of padding values for low indices for each spatial
///     dimension (front, top, left).
/// @param padding_r Array of padding values for high indices for each spatial
///     dimension (back, bottom, right). Can be NULL in which case padding is
///     considered to be symmetrical.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_dilated_convolution_backward_data_desc_init(
        dnnl_convolution_desc_t *conv_desc, dnnl_alg_kind_t alg_kind,
        const dnnl_memory_desc_t *diff_src_desc,
        const dnnl_memory_desc_t *weights_desc,
        const dnnl_memory_desc_t *diff_dst_desc, const dnnl_dims_t strides,
        const dnnl_dims_t dilates, const dnnl_dims_t padding_l,
        const dnnl_dims_t padding_r);

/// Initializes a descriptor for a convolution weights gradient primitive.
///
/// @note
///     Memory descriptors are allowed to be initialized with
///     #dnnl_format_tag_any or with format_kind set to #dnnl_format_kind_any.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///  - diff_dst (#dnnl_query_diff_dst_md, 0)
///
/// Outputs:
///  - diff_weights (#dnnl_query_diff_weights_md, 0)
///  - diff_bias (#dnnl_query_diff_weights_md, 1), if created with bias
///
/// @param conv_desc Output descriptor for a convolution primitive.
/// @param alg_kind Convolution algorithm. Possible values are
///     #dnnl_convolution_direct, #dnnl_convolution_winograd,
///     #dnnl_convolution_auto.
/// @param src_desc Source memory descriptor.
/// @param diff_weights_desc Diff weights memory descriptor.
/// @param diff_bias_desc Diff bias memory descriptor. Passing NULL, a zero
///     memory descriptor, or a memory descriptor with format_kind set to
///     #dnnl_format_kind_undef disables the bias term.
/// @param diff_dst_desc Diff destination memory descriptor.
/// @param strides Array of strides for spatial dimension.
/// @param padding_l Array of padding values for low indices for each spatial
///     dimension (front, top, left).
/// @param padding_r Array of padding values for high indices for each spatial
///     dimension (back, bottom, right). Can be NULL in which case padding is
///     considered to be symmetrical.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_convolution_backward_weights_desc_init(
        dnnl_convolution_desc_t *conv_desc, dnnl_alg_kind_t alg_kind,
        const dnnl_memory_desc_t *src_desc,
        const dnnl_memory_desc_t *diff_weights_desc,
        const dnnl_memory_desc_t *diff_bias_desc,
        const dnnl_memory_desc_t *diff_dst_desc, const dnnl_dims_t strides,
        const dnnl_dims_t padding_l, const dnnl_dims_t padding_r);

/// Initializes a descriptor for a dilated convolution weights gradient
/// primitive.
///
/// @note
///     Memory descriptors are allowed to be initialized with
///     #dnnl_format_tag_any or with format_kind set to #dnnl_format_kind_any.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///  - diff_dst (#dnnl_query_diff_dst_md, 0)
///
/// Outputs:
///  - diff_weights (#dnnl_query_diff_weights_md, 0)
///  - diff_bias (#dnnl_query_diff_weights_md, 1), if created with bias
///
/// @param conv_desc Output descriptor for a convolution primitive.
/// @param alg_kind Convolution algorithm. Possible values are
///     #dnnl_convolution_direct, #dnnl_convolution_winograd,
///     #dnnl_convolution_auto.
/// @param src_desc Source memory descriptor.
/// @param diff_weights_desc Diff weights memory descriptor.
/// @param diff_bias_desc Diff bias memory descriptor. Passing NULL, a zero
///     memory descriptor, or a memory descriptor with format_kind set to
///     #dnnl_format_kind_undef disables the bias term.
/// @param diff_dst_desc Diff destination memory descriptor.
/// @param strides Array of strides for spatial dimension.
/// @param dilates Array of dilations for spatial dimension. A zero value
///     means no dilation in the corresponding dimension.
/// @param padding_l Array of padding values for low indices for each spatial
///     dimension (front, top, left).
/// @param padding_r Array of padding values for high indices for each spatial
///     dimension (back, bottom, right). Can be NULL in which case padding is
///     considered to be symmetrical.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_dilated_convolution_backward_weights_desc_init(
        dnnl_convolution_desc_t *conv_desc, dnnl_alg_kind_t alg_kind,
        const dnnl_memory_desc_t *src_desc,
        const dnnl_memory_desc_t *diff_weights_desc,
        const dnnl_memory_desc_t *diff_bias_desc,
        const dnnl_memory_desc_t *diff_dst_desc, const dnnl_dims_t strides,
        const dnnl_dims_t dilates, const dnnl_dims_t padding_l,
        const dnnl_dims_t padding_r);

/// @} dnnl_api_convolution

/// @addtogroup dnnl_api_deconvolution
/// @{

/// Initializes a descriptor for a deconvolution forward propagation primitive.
///
/// @note
///     Memory descriptors are allowed to be initialized with
///     #dnnl_format_tag_any or with format_kind set to #dnnl_format_kind_any.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///  - weights (#dnnl_query_weights_md, 0)
///  - bias (#dnnl_query_weights_md, 1), if created with bias
///
/// Outputs:
///  - dst (#dnnl_query_dst_md, 0)
///
/// @param deconv_desc Output descriptor for a deconvolution primitive.
/// @param prop_kind Propagation kind. Possible values are
///     #dnnl_forward_training and #dnnl_forward_inference.
/// @param alg_kind Deconvolution algorithm. Possible values are
///     #dnnl_deconvolution_direct, #dnnl_deconvolution_winograd.
/// @param src_desc Source memory descriptor.
/// @param weights_desc Weights memory descriptor.
/// @param bias_desc Bias memory descriptor. Passing NULL, a zero memory
///     descriptor, or a memory descriptor with format_kind set to
///     #dnnl_format_kind_undef disables the bias term.
/// @param dst_desc Destination memory descriptor.
/// @param strides Array of strides for spatial dimension.
/// @param padding_l Array of padding values for low indices for each spatial
///     dimension (front, top, left).
/// @param padding_r Array of padding values for high indices for each spatial
///     dimension (back, bottom, right). Can be NULL in which case padding is
///     considered to be symmetrical.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_deconvolution_forward_desc_init(
        dnnl_deconvolution_desc_t *deconv_desc, dnnl_prop_kind_t prop_kind,
        dnnl_alg_kind_t alg_kind, const dnnl_memory_desc_t *src_desc,
        const dnnl_memory_desc_t *weights_desc,
        const dnnl_memory_desc_t *bias_desc, const dnnl_memory_desc_t *dst_desc,
        const dnnl_dims_t strides, const dnnl_dims_t padding_l,
        const dnnl_dims_t padding_r);

/// Initializes a descriptor for a dilated deconvolution forward propagation
/// primitive.
///
/// @note
///     Memory descriptors are allowed to be initialized with
///     #dnnl_format_tag_any or with format_kind set to #dnnl_format_kind_any.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///  - weights (#dnnl_query_weights_md, 0)
///  - bias (#dnnl_query_weights_md, 1), if created with bias
///
/// Outputs:
///  - dst (#dnnl_query_dst_md, 0)
///
/// @param deconv_desc Output descriptor for a deconvolution primitive.
/// @param prop_kind Propagation kind. Possible values are
///     #dnnl_forward_training and #dnnl_forward_inference.
/// @param alg_kind Deconvolution algorithm. Possible values are
///     #dnnl_deconvolution_direct, #dnnl_deconvolution_winograd.
/// @param src_desc Source memory descriptor.
/// @param weights_desc Weights memory descriptor.
/// @param bias_desc Bias memory descriptor. Passing NULL, a zero memory
///     descriptor, or a memory descriptor with format_kind set to
///     #dnnl_format_kind_undef disables the bias term.
/// @param dst_desc Destination memory descriptor.
/// @param strides Array of strides for spatial dimension.
/// @param dilates Array of dilations for spatial dimension. A zero value
///     means no dilation in the corresponding dimension.
/// @param padding_l Array of padding values for low indices for each spatial
///     dimension (front, top, left).
/// @param padding_r Array of padding values for high indices for each spatial
///     dimension (back, bottom, right). Can be NULL in which case padding is
///     considered to be symmetrical.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_dilated_deconvolution_forward_desc_init(
        dnnl_deconvolution_desc_t *deconv_desc, dnnl_prop_kind_t prop_kind,
        dnnl_alg_kind_t alg_kind, const dnnl_memory_desc_t *src_desc,
        const dnnl_memory_desc_t *weights_desc,
        const dnnl_memory_desc_t *bias_desc, const dnnl_memory_desc_t *dst_desc,
        const dnnl_dims_t strides, const dnnl_dims_t dilates,
        const dnnl_dims_t padding_l, const dnnl_dims_t padding_r);

/// Initializes a descriptor for a deconvolution backward propagation primitive.
///
/// @note
///     Memory descriptors are allowed to be initialized with
///     #dnnl_format_tag_any or with format_kind set to #dnnl_format_kind_any.
///
/// Inputs:
///  - diff_dst (#dnnl_query_diff_dst_md, 0)
///  - weights (#dnnl_query_weights_md, 0)
///
/// Outputs:
///  - diff_src (#dnnl_query_diff_src_md, 0)
///
/// @param deconv_desc Output descriptor for a deconvolution primitive.
/// @param alg_kind Deconvolution algorithm. Possible values are
///     #dnnl_deconvolution_direct, #dnnl_deconvolution_winograd.
/// @param diff_src_desc Diff source memory descriptor.
/// @param weights_desc Weights memory descriptor.
/// @param diff_dst_desc Diff destination memory descriptor.
/// @param strides Array of strides for spatial dimension.
/// @param padding_l Array of padding values for low indices for each spatial
///     dimension (front, top, left).
/// @param padding_r Array of padding values for high indices for each spatial
///     dimension (back, bottom, right). Can be NULL in which case padding is
///     considered to be symmetrical.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_deconvolution_backward_data_desc_init(
        dnnl_deconvolution_desc_t *deconv_desc, dnnl_alg_kind_t alg_kind,
        const dnnl_memory_desc_t *diff_src_desc,
        const dnnl_memory_desc_t *weights_desc,
        const dnnl_memory_desc_t *diff_dst_desc, const dnnl_dims_t strides,
        const dnnl_dims_t padding_l, const dnnl_dims_t padding_r);

/// Initializes a descriptor for a dilated deconvolution backward propagation
/// primitive.
///
/// @note
///     Memory descriptors are allowed to be initialized with
///     #dnnl_format_tag_any or with format_kind set to #dnnl_format_kind_any.
///
/// Inputs:
///  - diff_dst (#dnnl_query_diff_dst_md, 0)
///  - weights (#dnnl_query_weights_md, 0)
///
/// Outputs:
///  - diff_src (#dnnl_query_diff_src_md, 0)
///
/// @param deconv_desc Output descriptor for a deconvolution primitive.
/// @param alg_kind Deconvolution algorithm. Possible values are
///     #dnnl_deconvolution_direct, #dnnl_deconvolution_winograd.
/// @param diff_src_desc Diff source memory descriptor.
/// @param weights_desc Weights memory descriptor.
/// @param diff_dst_desc Diff destination memory descriptor.
/// @param strides Array of strides for spatial dimension.
/// @param dilates Array of dilations for spatial dimension. A zero value
///     means no dilation in the corresponding dimension.
/// @param padding_l Array of padding values for low indices for each spatial
///     dimension (front, top, left).
/// @param padding_r Array of padding values for high indices for each spatial
///     dimension (back, bottom, right). Can be NULL in which case padding is
///     considered to be symmetrical.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_dilated_deconvolution_backward_data_desc_init(
        dnnl_deconvolution_desc_t *deconv_desc, dnnl_alg_kind_t alg_kind,
        const dnnl_memory_desc_t *diff_src_desc,
        const dnnl_memory_desc_t *weights_desc,
        const dnnl_memory_desc_t *diff_dst_desc, const dnnl_dims_t strides,
        const dnnl_dims_t dilates, const dnnl_dims_t padding_l,
        const dnnl_dims_t padding_r);

/// Initializes a descriptor for a deconvolution weights gradient primitive.
///
/// @note
///     Memory descriptors are allowed to be initialized with
///     #dnnl_format_tag_any or with format_kind set to #dnnl_format_kind_any.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///  - diff_dst (#dnnl_query_diff_dst_md, 0)
///
/// Outputs:
///  - diff_weights (#dnnl_query_diff_weights_md, 0)
///  - diff_bias (#dnnl_query_diff_weights_md, 1), if created with bias
///
/// @param deconv_desc Output descriptor for a deconvolution primitive.
/// @param alg_kind Deconvolution algorithm. Possible values are
///     #dnnl_deconvolution_direct, #dnnl_deconvolution_winograd.
/// @param src_desc Source memory descriptor.
/// @param diff_weights_desc Diff weights memory descriptor.
/// @param diff_bias_desc Diff bias memory descriptor. Passing NULL, a zero
///     memory descriptor, or a memory descriptor with format_kind set to
///     #dnnl_format_kind_undef disables the bias term.
/// @param diff_dst_desc Diff destination memory descriptor.
/// @param strides Array of strides for spatial dimension.
/// @param padding_l Array of padding values for low indices for each spatial
///     dimension (front, top, left).
/// @param padding_r Array of padding values for high indices for each spatial
///     dimension (back, bottom, right). Can be NULL in which case padding is
///     considered to be symmetrical.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_deconvolution_backward_weights_desc_init(
        dnnl_deconvolution_desc_t *deconv_desc, dnnl_alg_kind_t alg_kind,
        const dnnl_memory_desc_t *src_desc,
        const dnnl_memory_desc_t *diff_weights_desc,
        const dnnl_memory_desc_t *diff_bias_desc,
        const dnnl_memory_desc_t *diff_dst_desc, const dnnl_dims_t strides,
        const dnnl_dims_t padding_l, const dnnl_dims_t padding_r);

/// Initializes a descriptor for a dilated deconvolution weights gradient
/// primitive.
///
/// @note
///     Memory descriptors are allowed to be initialized with
///     #dnnl_format_tag_any or with format_kind set to #dnnl_format_kind_any.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///  - diff_dst (#dnnl_query_diff_dst_md, 0)
///
/// Outputs:
///  - diff_weights (#dnnl_query_diff_weights_md, 0)
///  - diff_bias (#dnnl_query_diff_weights_md, 1), if created with bias
///
/// @param deconv_desc Output descriptor for a deconvolution primitive.
/// @param alg_kind Deconvolution algorithm. Possible values are
///     #dnnl_deconvolution_direct, #dnnl_deconvolution_winograd.
/// @param src_desc Source memory descriptor.
/// @param diff_weights_desc Diff weights memory descriptor.
/// @param diff_bias_desc Diff bias memory descriptor. Passing NULL, a zero
///     memory descriptor, or a memory descriptor with format_kind set to
///     #dnnl_format_kind_undef disables the bias term.
/// @param diff_dst_desc Diff destination memory descriptor.
/// @param strides Array of strides for spatial dimension.
/// @param dilates Array of dilations for spatial dimension. A zero value
///     means no dilation in the corresponding dimension.
/// @param padding_l Array of padding values for low indices for each spatial
///     dimension (front, top, left).
/// @param padding_r Array of padding values for high indices for each spatial
///     dimension (back, bottom, right). Can be NULL in which case padding is
///     considered to be symmetrical.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_dilated_deconvolution_backward_weights_desc_init(
        dnnl_deconvolution_desc_t *deconv_desc, dnnl_alg_kind_t alg_kind,
        const dnnl_memory_desc_t *src_desc,
        const dnnl_memory_desc_t *diff_weights_desc,
        const dnnl_memory_desc_t *diff_bias_desc,
        const dnnl_memory_desc_t *diff_dst_desc, const dnnl_dims_t strides,
        const dnnl_dims_t dilates, const dnnl_dims_t padding_l,
        const dnnl_dims_t padding_r);

/// @} dnnl_api_deconvolution

/// @addtogroup dnnl_api_shuffle
/// @{

/// Initializes a descriptor for shuffle forward propagation primitive.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///
/// Outputs:
///  - dst (#dnnl_query_dst_md, 0)
///
/// @param shuffle_desc Output descriptor for a shuffle primitive.
/// @param prop_kind Propagation kind. Possible values are
///     #dnnl_forward_training and #dnnl_forward_inference.
/// @param data_desc Source and destination memory descriptor.
/// @param axis The axis along which the data is shuffled.
/// @param group_size Shuffle group size.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_shuffle_forward_desc_init(
        dnnl_shuffle_desc_t *shuffle_desc, dnnl_prop_kind_t prop_kind,
        const dnnl_memory_desc_t *data_desc, int axis, dnnl_dim_t group_size);

/// Initializes a descriptor for shuffle backward propagation primitive.
///
/// Inputs:
///  - diff_dst (#dnnl_query_diff_dst_md, 0)
///
/// Outputs:
///  - diff_src (#dnnl_query_diff_src_md, 0)
///
/// @param shuffle_desc Output descriptor for a shuffle primitive.
/// @param diff_data_desc Diff source and diff destination memory descriptor.
/// @param axis The axis along which the data is shuffled.
/// @param group_size Shuffle group size.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_shuffle_backward_desc_init(
        dnnl_shuffle_desc_t *shuffle_desc,
        const dnnl_memory_desc_t *diff_data_desc, int axis,
        dnnl_dim_t group_size);

/// @} dnnl_api_shuffle

/// @addtogroup dnnl_api_eltwise
/// @{

/// Initializes a descriptor for eltwise forward propagation primitive.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///
/// Outputs:
///  - dst (#dnnl_query_dst_md, 0)
///
/// @param eltwise_desc Output descriptor for an eltwise primitive.
/// @param prop_kind Propagation kind. Possible values are
///     #dnnl_forward_training and #dnnl_forward_inference.
/// @param alg_kind Elementwise algorithm kind.
/// @param data_desc Source and destination memory descriptor.
/// @param alpha The alpha parameter for the elementwise operation. Specific
///     meaning depends on the algorithm.
/// @param beta The beta parameter for the elementwise operation. Specific
///     meaning depends on the algorithm.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_eltwise_forward_desc_init(
        dnnl_eltwise_desc_t *eltwise_desc, dnnl_prop_kind_t prop_kind,
        dnnl_alg_kind_t alg_kind, const dnnl_memory_desc_t *data_desc,
        float alpha, float beta);

/// Initializes a descriptor for eltwise backward propagation primitive.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///  - diff_dst (#dnnl_query_diff_dst_md, 0)
///
/// Outputs:
///  - diff_src (#dnnl_query_diff_src_md, 0)
///
/// @param eltwise_desc Output descriptor for an eltwise primitive.
/// @param alg_kind Elementwise algorithm kind.
/// @param diff_data_desc Diff source and diff destination memory descriptors.
/// @param data_desc Source and destination memory descriptor.
/// @param alpha The alpha parameter for the elementwise operation. Specific
///     meaning depends on the algorithm.
/// @param beta The beta parameter for the elementwise operation. Specific
///     meaning depends on the algorithm.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_eltwise_backward_desc_init(
        dnnl_eltwise_desc_t *eltwise_desc, dnnl_alg_kind_t alg_kind,
        const dnnl_memory_desc_t *diff_data_desc,
        const dnnl_memory_desc_t *data_desc, float alpha, float beta);

/// @} dnnl_api_eltwise

/// @addtogroup dnnl_api_softmax
/// @{

/// Initializes a descriptor for softmax forward propagation primitive.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///
/// Outputs:
///  - dst (#dnnl_query_dst_md, 0)
///
/// @param softmax_desc Output descriptor for a softmax primitive.
/// @param prop_kind Propagation kind. Possible values are
///     #dnnl_forward_training and #dnnl_forward_inference.
/// @param data_desc Source and destination memory descriptor.
/// @param softmax_axis Axis over which softmax is computed.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_softmax_forward_desc_init(
        dnnl_softmax_desc_t *softmax_desc, dnnl_prop_kind_t prop_kind,
        const dnnl_memory_desc_t *data_desc, int softmax_axis);

/// Initializes a descriptor for softmax backward propagation primitive.
///
/// Inputs:
///  - dst (#dnnl_query_dst_md, 0)
///  - diff_dst (#dnnl_query_diff_dst_md, 0)
///
/// Outputs:
///  - diff_src (#dnnl_query_diff_src_md, 0)
///
/// @param softmax_desc Output descriptor for a softmax primitive.
/// @param diff_data_desc Diff source and diff destination memory descriptors.
/// @param data_desc Destination memory descriptor.
/// @param softmax_axis Axis over which softmax is computed.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_softmax_backward_desc_init(
        dnnl_softmax_desc_t *softmax_desc,
        const dnnl_memory_desc_t *diff_data_desc,
        const dnnl_memory_desc_t *data_desc, int softmax_axis);

/// @} dnnl_api_softmax

/// @addtogroup dnnl_api_logsoftmax
/// @{

/// Initializes a descriptor for logsoftmax forward propagation primitive.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///
/// Outputs:
///  - dst (#dnnl_query_dst_md, 0)
///
/// @param logsoftmax_desc Output descriptor for a logsoftmax primitive.
/// @param prop_kind Propagation kind. Possible values are
///     #dnnl_forward_training and #dnnl_forward_inference.
/// @param data_desc Source and destination memory descriptor.
/// @param logsoftmax_axis Axis over which logsoftmax is computed.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_logsoftmax_forward_desc_init(
        dnnl_logsoftmax_desc_t *logsoftmax_desc, dnnl_prop_kind_t prop_kind,
        const dnnl_memory_desc_t *data_desc, int logsoftmax_axis);

/// Initializes a descriptor for logsoftmax backward propagation primitive.
///
/// Inputs:
///  - dst (#dnnl_query_dst_md, 0)
///  - diff_dst (#dnnl_query_diff_dst_md, 0)
///
/// Outputs:
///  - diff_src (#dnnl_query_diff_src_md, 0)
///
/// @param logsoftmax_desc Output descriptor for a logsoftmax primitive.
/// @param diff_data_desc Diff source and diff destination memory descriptors.
/// @param data_desc Destination memory descriptor.
/// @param logsoftmax_axis Axis over which softmax is computed.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_logsoftmax_backward_desc_init(
        dnnl_logsoftmax_desc_t *logsoftmax_desc,
        const dnnl_memory_desc_t *diff_data_desc,
        const dnnl_memory_desc_t *data_desc, int logsoftmax_axis);

/// @} dnnl_api_logsoftmax

/// @addtogroup dnnl_api_pooling
/// @{

/// Initializes a descriptor for pooling forward propagation primitive.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///
/// Outputs:
///  - dst (#dnnl_query_dst_md, 0)
///  - workspace (#dnnl_query_workspace_md, 0),
///     if @p alg_kind = #dnnl_pooling_max and
///     @p prop_kind = #dnnl_forward_training; must be queried for using @ref
///     dnnl_primitive_desc_query_md() after a corresponding primitive
///     descriptor is created
///
/// @param pool_desc Output descriptor for a pooling primitive.
/// @param prop_kind Propagation kind. Possible values are
///     #dnnl_forward_training and #dnnl_forward_inference.
/// @param alg_kind Pooling algorithm kind: either #dnnl_pooling_max,
///     #dnnl_pooling_avg_include_padding, or #dnnl_pooling_avg (same as
///     #dnnl_pooling_avg_exclude_padding).
/// @param src_desc Source memory descriptor.
/// @param dst_desc Destination memory descriptor.
/// @param strides Array of strides for spatial dimension.
/// @param kernel Array of kernel spatial dimensions.
/// @param padding_l Array of padding values for low indices for each spatial
///     dimension (front, top, left).
/// @param padding_r Array of padding values for high indices for each spatial
///     dimension (back, bottom, right). Can be NULL in which case padding is
///     considered to be symmetrical.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_pooling_forward_desc_init(
        dnnl_pooling_desc_t *pool_desc, dnnl_prop_kind_t prop_kind,
        dnnl_alg_kind_t alg_kind, const dnnl_memory_desc_t *src_desc,
        const dnnl_memory_desc_t *dst_desc, const dnnl_dims_t strides,
        const dnnl_dims_t kernel, const dnnl_dims_t padding_l,
        const dnnl_dims_t padding_r);

/// Initializes a descriptor for pooling backward propagation primitive.
///
/// Inputs:
///  - diff_dst (#dnnl_query_diff_dst_md, 0)
///  - workspace (#dnnl_query_workspace_md, 0),
///     if @p alg_kind = #dnnl_pooling_max; must be queried for using @ref
///     dnnl_primitive_desc_query_md() after a corresponding primitive
///     descriptor is created
///
/// Outputs:
///  - diff_src (#dnnl_query_diff_src_md, 0)
///
/// @param pool_desc Output descriptor for a pooling primitive.
/// @param alg_kind Pooling algorithm kind: either #dnnl_pooling_max,
///     #dnnl_pooling_avg_include_padding, or #dnnl_pooling_avg (same as
///     #dnnl_pooling_avg_exclude_padding).
/// @param diff_src_desc Diff source memory descriptor.
/// @param diff_dst_desc Diff destination memory descriptor.
/// @param strides Array of strides for spatial dimension.
/// @param kernel Array of kernel spatial dimensions.
/// @param padding_l Array of padding values for low indices for each spatial
///     dimension (front, top, left).
/// @param padding_r Array of padding values for high indices for each spatial
///     dimension (back, bottom, right). Can be NULL in which case padding is
///     considered to be symmetrical.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_pooling_backward_desc_init(
        dnnl_pooling_desc_t *pool_desc, dnnl_alg_kind_t alg_kind,
        const dnnl_memory_desc_t *diff_src_desc,
        const dnnl_memory_desc_t *diff_dst_desc, const dnnl_dims_t strides,
        const dnnl_dims_t kernel, const dnnl_dims_t padding_l,
        const dnnl_dims_t padding_r);

/// @} dnnl_api_pooling

/// @addtogroup dnnl_api_lrn
/// @{

/// Initializes a descriptor for LRN forward propagation primitive.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///
/// Outputs:
///  - dst (#dnnl_query_dst_md, 0)
///  - workspace (#dnnl_query_workspace_md, 0),
///     if the underlying implementation requires it; must be queried for
///     using @ref dnnl_primitive_desc_query_md() after a corresponding
///     primitive descriptor is created
///
/// @param lrn_desc Output descriptor for a LRN primitive.
/// @param prop_kind Propagation kind. Possible values are
///     #dnnl_forward_training and #dnnl_forward_inference.
/// @param alg_kind LRN algorithm kind: either #dnnl_lrn_across_channels or
///     #dnnl_lrn_within_channel.
/// @param data_desc Source and destination memory descriptor.
/// @param local_size Regularization local size.
/// @param alpha The alpha regularization parameter.
/// @param beta The beta regularization parameter.
/// @param k The k regularization parameter.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_lrn_forward_desc_init(dnnl_lrn_desc_t *lrn_desc,
        dnnl_prop_kind_t prop_kind, dnnl_alg_kind_t alg_kind,
        const dnnl_memory_desc_t *data_desc, dnnl_dim_t local_size, float alpha,
        float beta, float k);

/// Initializes a descriptor for LRN backward propagation primitive.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///  - diff_dst (#dnnl_query_diff_dst_md, 0)
///  - workspace (#dnnl_query_workspace_md, 0),
///     if the underlying implementation requires it; must be queried for
///     using @ref dnnl_primitive_desc_query_md() after a corresponding
///     primitive descriptor is created
///
/// Outputs:
///  - diff_src (#dnnl_query_diff_src_md, 0)
///
/// @param lrn_desc Output descriptor for a LRN primitive.
/// @param alg_kind LRN algorithm kind: either #dnnl_lrn_across_channels or
///     #dnnl_lrn_within_channel.
/// @param diff_data_desc Diff source and diff destination memory descriptor.
/// @param data_desc Source memory descriptor.
/// @param local_size Regularization local size.
/// @param alpha The alpha regularization parameter.
/// @param beta The beta regularization parameter.
/// @param k The k regularization parameter.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_lrn_backward_desc_init(dnnl_lrn_desc_t *lrn_desc,
        dnnl_alg_kind_t alg_kind, const dnnl_memory_desc_t *diff_data_desc,
        const dnnl_memory_desc_t *data_desc, dnnl_dim_t local_size, float alpha,
        float beta, float k);

/// @} dnnl_api_lrn

/// @addtogroup dnnl_api_batch_normalization
/// @{

/// Initializes a descriptor for a batch normalization forward propagation
/// primitive.
///
/// @note
///     In-place operation is supported: the dst can refer to the same memory
///     as the src.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///  - mean (#dnnl_query_src_md, 1),
///     if #dnnl_use_global_stats bit-flags is set in @p flags
///  - variance (#dnnl_query_src_md, 2),
///     if #dnnl_use_global_stats bit-flags is set in @p flags
///  - scale_and_shift (#dnnl_query_weights_md, 0),
///     if #dnnl_use_scaleshift bit-flags is set in @p flags
///
/// Outputs:
///  - dst (#dnnl_query_dst_md, 0)
///  - mean (#dnnl_query_dst_md, 1),
///     if #dnnl_use_global_stats bit-flags is not set in @p flags
///     and @p prop_kind = #dnnl_forward_training
///  - variance (#dnnl_query_dst_md, 2),
///     if #dnnl_use_global_stats bit-flags is not set in @p flags
///     and @p prop_kind = #dnnl_forward_training
///  - workspace (#dnnl_query_workspace_md, 0),
///     if #dnnl_fuse_norm_relu bit-flags is set in @p flags
///     and @p prop_kind = #dnnl_forward_training; must be queried for
///     using @ref dnnl_primitive_desc_query_md() after a corresponding
///     primitive descriptor is created
///
/// @param bnrm_desc Output descriptor for batch normalization primitive.
/// @param prop_kind Propagation kind. Possible values are
///     #dnnl_forward_training and #dnnl_forward_inference.
/// @param data_desc Source and destination memory descriptor.
/// @param epsilon Batch normalization epsilon parameter.
/// @param flags Batch normalization flags (@ref dnnl_normalization_flags_t).
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_batch_normalization_forward_desc_init(
        dnnl_batch_normalization_desc_t *bnrm_desc, dnnl_prop_kind_t prop_kind,
        const dnnl_memory_desc_t *data_desc, float epsilon, unsigned flags);

/// Initializes a descriptor for a batch normalization backward propagation
/// primitive.
///
/// @note
///     In-place operation is supported: the diff_dst can refer to the same
///     memory as the diff_src.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///  - mean (#dnnl_query_src_md, 1)
///  - variance (#dnnl_query_src_md, 2)
///  - diff_dst (#dnnl_query_diff_dst_md, 0)
///  - scale_and_shift (#dnnl_query_weights_md, 0),
///     if #dnnl_use_scaleshift bit-flags is set in @p flags
///  - workspace (#dnnl_query_workspace_md, 0),
///     if #dnnl_fuse_norm_relu bit-flags is set in @p flags
///
/// Outputs:
///  - diff_src (#dnnl_query_diff_src_md, 0)
///  - diff_scale_and_shift (#dnnl_query_diff_weights_md, 0),
///     if #dnnl_use_scaleshift bit-flags is set in @p flags
///     and @p prop_kind = #dnnl_backward
///
/// @param bnrm_desc Output descriptor for batch normalization primitive.
/// @param prop_kind Propagation kind. Possible values are
///     #dnnl_backward_data and #dnnl_backward (diffs for all parameters are
///     computed in this case).
/// @param diff_data_desc Diff source and diff destination memory descriptor.
/// @param data_desc Source memory descriptor.
/// @param epsilon Batch normalization epsilon parameter.
/// @param flags Batch normalization flags (@ref dnnl_normalization_flags_t).
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_batch_normalization_backward_desc_init(
        dnnl_batch_normalization_desc_t *bnrm_desc, dnnl_prop_kind_t prop_kind,
        const dnnl_memory_desc_t *diff_data_desc,
        const dnnl_memory_desc_t *data_desc, float epsilon, unsigned flags);

/// @} dnnl_api_batch_normalization

/// @addtogroup dnnl_api_layer_normalization
/// @{

/// Initializes a descriptor for layer normalization forward propagation
/// primitive.
///
/// @note
///     In-place operation is supported: the dst can refer to the same memory
///     as the src.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///  - mean (#dnnl_query_src_md, 1),
///     if #dnnl_use_global_stats bit-flags is set in @p flags
///  - variance (#dnnl_query_src_md, 2),
///     if #dnnl_use_global_stats bit-flags is set in @p flags
///  - scale_and_shift (#dnnl_query_weights_md, 0),
///     if #dnnl_use_scaleshift bit-flags is set in @p flags
///
/// Outputs:
///  - dst (#dnnl_query_dst_md, 0)
///  - mean (#dnnl_query_dst_md, 1),
///     if #dnnl_use_global_stats bit-flags is not set in @p flags
///     and @p prop_kind = #dnnl_forward_training
///  - variance (#dnnl_query_dst_md, 2),
///     if #dnnl_use_global_stats bit-flags is not set in @p flags
///     and @p prop_kind = #dnnl_forward_training
///
/// @param lnrm_desc Output descriptor for layer normalization primitive.
/// @param prop_kind Propagation kind. Possible values are
///     #dnnl_forward_training and #dnnl_forward_inference.
/// @param data_desc Source and destination memory descriptor.
/// @param stat_desc Memory descriptor for mean and variance. If this
///     parameter is NULL, a zero memory descriptor, or a memory descriptor
///     with format_kind set to #dnnl_format_kind_undef, then the memory
///     descriptor for stats is derived from @p data_desc by removing the last
///     dimension.
/// @param epsilon Layer normalization epsilon parameter.
/// @param flags Layer normalization flags (@ref dnnl_normalization_flags_t).
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_layer_normalization_forward_desc_init(
        dnnl_layer_normalization_desc_t *lnrm_desc, dnnl_prop_kind_t prop_kind,
        const dnnl_memory_desc_t *data_desc,
        const dnnl_memory_desc_t *stat_desc, float epsilon, unsigned flags);

/// Initializes a descriptor for a layer normalization backward propagation
/// primitive.
///
/// @note
///     In-place operation is supported: the diff_dst can refer to the same
///     memory as the diff_src.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///  - mean (#dnnl_query_src_md, 1)
///  - variance (#dnnl_query_src_md, 2)
///  - diff_dst (#dnnl_query_diff_dst_md, 0)
///  - scale_and_shift (#dnnl_query_weights_md, 0),
///     if #dnnl_use_scaleshift bit-flags is set in @p flags
///
/// Outputs:
///  - diff_src (#dnnl_query_diff_src_md, 0)
///  - diff_scale_and_shift (#dnnl_query_diff_weights_md, 0),
///     if #dnnl_use_scaleshift bit-flags is set in @p flags
///     and @p prop_kind = #dnnl_backward
///
/// @param lnrm_desc Output descriptor for layer normalization primitive.
/// @param prop_kind Propagation kind. Possible values are
///     #dnnl_backward_data and #dnnl_backward (diffs for all parameters are
///     computed in this case).
/// @param diff_data_desc Diff source and diff destination memory descriptor.
/// @param data_desc Source memory descriptor.
/// @param stat_desc Memory descriptor for mean and variance. If this
///     parameter is NULL, a zero memory descriptor, or a memory descriptor
///     with format_kind set to #dnnl_format_kind_undef, then the memory
///     descriptor for stats is derived from @p data_desc by removing the last
///     dimension.
/// @param epsilon Layer normalization epsilon parameter.
/// @param flags Layer normalization flags (@ref dnnl_normalization_flags_t).
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_layer_normalization_backward_desc_init(
        dnnl_layer_normalization_desc_t *lnrm_desc, dnnl_prop_kind_t prop_kind,
        const dnnl_memory_desc_t *diff_data_desc,
        const dnnl_memory_desc_t *data_desc,
        const dnnl_memory_desc_t *stat_desc, float epsilon, unsigned flags);

/// @} dnnl_api_layer_normalization

/// @addtogroup dnnl_api_inner_product
/// @{

/// Initializes descriptor for inner product forward propagation.
///
/// @note
///     Memory descriptors are allowed to be initialized with
///     #dnnl_format_tag_any or with format_kind set to #dnnl_format_kind_any.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///  - weights (#dnnl_query_weights_md, 0)
///  - bias (#dnnl_query_weights_md, 1), if created with bias
///
/// Outputs:
///  - dst (#dnnl_query_dst_md, 0)
///
/// @param ip_desc Output descriptor for inner product primitive.
/// @param prop_kind Propagation kind. Possible values are
///     #dnnl_forward_training and #dnnl_forward_inference.
/// @param src_desc Source memory descriptor.
/// @param weights_desc Weights memory descriptor.
/// @param bias_desc Bias memory descriptor. Passing NULL, a zero memory
///     descriptor, or a memory descriptor with format_kind set to
///     #dnnl_format_kind_undef disables the bias term.
/// @param dst_desc Destination memory descriptor.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_inner_product_forward_desc_init(
        dnnl_inner_product_desc_t *ip_desc, dnnl_prop_kind_t prop_kind,
        const dnnl_memory_desc_t *src_desc,
        const dnnl_memory_desc_t *weights_desc,
        const dnnl_memory_desc_t *bias_desc,
        const dnnl_memory_desc_t *dst_desc);

/// Initializes descriptor for inner product backward propagation.
///
/// @note
///     Memory descriptors are allowed to be initialized with
///     #dnnl_format_tag_any or with format_kind set to #dnnl_format_kind_any.
///
/// Inputs:
///  - diff_dst (#dnnl_query_diff_dst_md, 0)
///  - weights (#dnnl_query_weights_md, 0)
///
/// Outputs:
///  - diff_src (#dnnl_query_diff_src_md, 0)
///
/// @param ip_desc Output descriptor for inner product primitive.
/// @param diff_src_desc Diff source memory descriptor.
/// @param weights_desc Weights memory descriptor.
/// @param diff_dst_desc Diff destination memory descriptor.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_inner_product_backward_data_desc_init(
        dnnl_inner_product_desc_t *ip_desc,
        const dnnl_memory_desc_t *diff_src_desc,
        const dnnl_memory_desc_t *weights_desc,
        const dnnl_memory_desc_t *diff_dst_desc);

/// Initializes descriptor for inner product weights gradient primitive.
///
/// @note
///     Memory descriptors are allowed to be initialized with
///     #dnnl_format_tag_any or with format_kind set to #dnnl_format_kind_any.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///  - diff_dst (#dnnl_query_diff_dst_md, 0)
///
/// Outputs:
///  - diff_weights (#dnnl_query_diff_weights_md, 0)
///  - diff_bias (#dnnl_query_diff_weights_md, 1), if created with bias
///
/// @param ip_desc Output descriptor for inner product primitive.
/// @param src_desc Source memory descriptor.
/// @param diff_weights_desc Diff weights memory descriptor.
/// @param diff_bias_desc Diff bias memory descriptor. Passing NULL, a zero
///     memory descriptor, or a memory descriptor with format_kind set to
///     #dnnl_format_kind_undef disables the bias term.
/// @param diff_dst_desc Diff destination memory descriptor.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_inner_product_backward_weights_desc_init(
        dnnl_inner_product_desc_t *ip_desc, const dnnl_memory_desc_t *src_desc,
        const dnnl_memory_desc_t *diff_weights_desc,
        const dnnl_memory_desc_t *diff_bias_desc,
        const dnnl_memory_desc_t *diff_dst_desc);

/// @} dnnl_api_inner_product

/// @addtogroup dnnl_api_attributes
/// @{

/// Set quantization scale and shift parameters for RNN data tensors.
///
/// For performance reasons, the low-precision configuration of the RNN
/// primitives expects input activations to have the unsigned 8-bit integer
/// data type. The scale and shift parameters are used to quantize
/// floating-point data to unsigned integer and must be passed to the RNN
/// primitive using attributes.
///
/// The quantization formula is `scale * (data + shift)`.
///
/// @note
///     Quantization scale and shift are common for src_layer, src_iter,
///     dst_iter, and dst_layer.
///
/// Example usage:
/// @code
///     // RNN parameters
///     int l = 2, t = 2, mb = 32, sic = 32, slc = 32, dic = 32, dlc = 32;
///     // Activations quantization parameters
///     float scale = ..., shift = ..;
///
///     dnnl_primitive_attr_t rnn_attr;
///     // Create default attributes
///     dnnl_primitive_attr_create(&rnn_attr);
///
///     // Set scale and shift for int8 quantization of activation
///     dnnl_primitive_attr_set_rnn_data_qparams(rnn_attr, scale, shift);
///
///     // Create and configure rnn op_desc
///     dnnl_rnn_desc_t rnn_d;
///     dnnl_primitive_desc_t rnn_pd;
///     dnnl_primitive_desc_create(&rnn_pd, &rnn_d, attr, engine, NULL);
/// @endcode
///
/// @param attr Primitive attributes.
/// @param scale The value to scale the data by.
/// @param shift The value to shift the data by.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_attr_set_rnn_data_qparams(
        dnnl_primitive_attr_t attr, const float scale, const float shift);

/// Sets quantization scaling factors for RNN weights tensors. The
/// low-precision configuration of the RNN primitives expects input weights to
/// use the signed 8-bit integer data type. The scaling factors are used to
/// quantize floating-point data to signed integer and must be passed to RNN
/// primitives using attributes.
///
/// @note
///     The dimension order is always native and does not depend on the actual
///     layout used. For example, five-dimensional weights always have (l, d,
///     i, g, o) logical dimension ordering.
///
/// @note
///     Quantization scales are common for weights_layer and weights_iteration
///
/// @param attr Primitive attributes.
/// @param count Number of elements in the @p scales array.
/// @param mask Scaling factors correspondence mask that defines the
///     correspondence between the output tensor dimensions and the @p
///     scales vector. The set i-th bit indicates that a dedicated scaling
///     factor should be used for each index along that dimension. Set the
///     mask to 0 to use a common scaling factor for the whole output
///     tensor.
/// @param scales Array of output scaling factors that must contain @p count
///     values and the following equality must hold:
///     \f[count = \prod\limits_{d \in mask} weights.dims[d].\f]
///     Violations can only be detected when the attributes are used to create
///     a primitive descriptor.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_primitive_attr_set_rnn_weights_qparams(
        dnnl_primitive_attr_t attr, dnnl_dim_t count, int mask,
        const float *scales);

/// @} dnnl_api_attributes

/// @addtogroup dnnl_api_rnn
/// @{

/// Initializes a descriptor for vanilla RNN forward propagation primitive.
///
/// The @p src_iter_desc, @p bias_desc, and @p dst_iter_desc may either be @c
/// NULL or point to a zero memory descriptor. This would then indicate that
/// the RNN forward propagation primitive should not use them and should
/// default to zero values instead.
///
/// @note
///     All memory descriptors are allowed to be initialized with
///     #dnnl_format_tag_any or with format_kind set to #dnnl_format_kind_any.
///
/// Inputs:
///  - src_layer (#dnnl_query_src_md, 0)
///  - src_iter (#dnnl_query_src_md, 1), if used
///  - weights_layer (#dnnl_query_weights_md, 0)
///  - weights_iter (#dnnl_query_weights_md, 1)
///  - bias (#dnnl_query_weights_md, 2), if used
///
/// Outputs:
///  - dst_layer (#dnnl_query_dst_md, 0)
///  - dst_iter (#dnnl_query_dst_md, 1), if used
///  - workspace (#dnnl_query_workspace_md, 0),
///     if @p prop_kind equals #dnnl_forward_training; must be queried for
///     using @ref dnnl_primitive_desc_query_md() after a corresponding
///     primitive descriptor is created
///
/// @param rnn_desc Output descriptor for vanilla RNN primitive.
/// @param prop_kind Propagation kind. Possible values are
///     #dnnl_forward_training and #dnnl_forward_inference.
/// @param activation Activation kind. Possible values are #dnnl_eltwise_relu,
///     #dnnl_eltwise_tanh or #dnnl_eltwise_logistic.
/// @param direction RNN direction. See @ref dnnl_rnn_direction_t for more
///     info.
/// @param src_layer_desc Memory descriptor for the input vector.
/// @param src_iter_desc Memory descriptor for the input recurrent hidden
///     state vector.
/// @param weights_layer_desc Memory descriptor for the weights applied to the
///     layer input.
/// @param weights_iter_desc Memory descriptor for the weights applied to the
///     recurrent input.
/// @param bias_desc Bias memory descriptor.
/// @param dst_layer_desc Memory descriptor for the output vector.
/// @param dst_iter_desc Memory descriptor for the output recurrent hidden
///     state vector.
/// @param flags Unused.
/// @param alpha Negative slope if activation is #dnnl_eltwise_relu.
/// @param beta Unused.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_vanilla_rnn_forward_desc_init(
        dnnl_rnn_desc_t *rnn_desc, dnnl_prop_kind_t prop_kind,
        const dnnl_alg_kind_t activation, const dnnl_rnn_direction_t direction,
        const dnnl_memory_desc_t *src_layer_desc,
        const dnnl_memory_desc_t *src_iter_desc,
        const dnnl_memory_desc_t *weights_layer_desc,
        const dnnl_memory_desc_t *weights_iter_desc,
        const dnnl_memory_desc_t *bias_desc,
        const dnnl_memory_desc_t *dst_layer_desc,
        const dnnl_memory_desc_t *dst_iter_desc, unsigned flags, float alpha,
        float beta);

/// Initializes a descriptor for vanilla RNN backward propagation primitive.
///
/// The @p src_iter_desc together with @p diff_src_iter_desc, @p bias_desc
/// together with @p diff_bias_desc, and @p dst_iter_desc together with @p
/// diff_src_iter_desc, may either be @c NULL or point to a zero memory
/// descriptor. This would then indicate that the RNN backward propagation
/// primitive should not use the respective data and should use zero values
/// instead.
///
/// @note
///     All memory descriptors are allowed to be initialized with
///     #dnnl_format_tag_any or with format_kind set to #dnnl_format_kind_any.
///
/// Inputs:
///  - src_layer (#dnnl_query_src_md, 0)
///  - src_iter (#dnnl_query_src_md, 1), if used
///  - weights_layer (#dnnl_query_weights_md, 0)
///  - weights_iter (#dnnl_query_weights_md, 1)
///  - bias (#dnnl_query_weights_md, 2), if used
///  - dst_layer (#dnnl_query_dst_md, 0)
///  - dst_iter (#dnnl_query_dst_md, 1), if used
///  - diff_dst_layer (#dnnl_query_diff_dst_md, 0)
///  - diff_dst_iter (#dnnl_query_diff_dst_md, 1), if used
///  - workspace (#dnnl_query_workspace_md, 0)
///
/// Outputs:
///  - diff_src_layer (#dnnl_query_diff_src_md, 0)
///  - diff_src_iter (#dnnl_query_diff_src_md, 1), if used
///  - diff_weights_layer (#dnnl_query_diff_weights_md, 0)
///  - diff_weights_iter (#dnnl_query_diff_weights_md, 1)
///  - diff_bias (#dnnl_query_diff_weights_md, 2), if used
///
/// @param rnn_desc Output descriptor for vanilla RNN primitive.
/// @param prop_kind Propagation kind. Must be #dnnl_backward.
/// @param activation Activation kind. Possible values are #dnnl_eltwise_relu,
///     #dnnl_eltwise_tanh or #dnnl_eltwise_logistic.
/// @param direction RNN direction. See @ref dnnl_rnn_direction_t for more
///     info.
/// @param src_layer_desc Memory descriptor for the input vector.
/// @param src_iter_desc Memory descriptor for the input recurrent hidden
///     state vector.
/// @param weights_layer_desc Memory descriptor for the weights applied to the
///     layer input.
/// @param weights_iter_desc Memory descriptor for the weights applied to the
///     recurrent input.
/// @param bias_desc Bias memory descriptor.
/// @param dst_layer_desc Memory descriptor for the output vector.
/// @param dst_iter_desc Memory descriptor for the output recurrent hidden
///     state vector.
/// @param diff_src_layer_desc Memory descriptor for the diff of input vector.
/// @param diff_src_iter_desc Memory descriptor for the diff of input recurrent
///     hidden state vector.
/// @param diff_weights_layer_desc Memory descriptor for the diff of weights
///     applied to the layer input.
/// @param diff_weights_iter_desc Memory descriptor for the diff of weights
///     applied to the recurrent input.
/// @param diff_bias_desc Diff bias memory descriptor.
/// @param diff_dst_layer_desc Memory descriptor for the diff of output
///     vector.
/// @param diff_dst_iter_desc Memory descriptor for the diff of output
///     recurrent hidden state vector.
/// @param flags Unused.
/// @param alpha Negative slope if activation is #dnnl_eltwise_relu.
/// @param beta Unused.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_vanilla_rnn_backward_desc_init(
        dnnl_rnn_desc_t *rnn_desc, dnnl_prop_kind_t prop_kind,
        const dnnl_alg_kind_t activation, const dnnl_rnn_direction_t direction,
        const dnnl_memory_desc_t *src_layer_desc,
        const dnnl_memory_desc_t *src_iter_desc,
        const dnnl_memory_desc_t *weights_layer_desc,
        const dnnl_memory_desc_t *weights_iter_desc,
        const dnnl_memory_desc_t *bias_desc,
        const dnnl_memory_desc_t *dst_layer_desc,
        const dnnl_memory_desc_t *dst_iter_desc,
        const dnnl_memory_desc_t *diff_src_layer_desc,
        const dnnl_memory_desc_t *diff_src_iter_desc,
        const dnnl_memory_desc_t *diff_weights_layer_desc,
        const dnnl_memory_desc_t *diff_weights_iter_desc,
        const dnnl_memory_desc_t *diff_bias_desc,
        const dnnl_memory_desc_t *diff_dst_layer_desc,
        const dnnl_memory_desc_t *diff_dst_iter_desc, unsigned flags,
        float alpha, float beta);

/// Initializes a descriptor for LSTM forward propagation primitive.
///
/// The @p src_iter_desc, @p src_iter_c_desc, @p bias_desc, @p dst_iter_desc,
/// and @p dst_iter_c_desc may either be @c NULL or point to a zero memory
/// descriptor. This would then indicate that the LSTM forward propagation
/// primitive should not use them and should default to zero values instead.
///
/// @note
///     All memory descriptors are allowed to be initialized with
///     #dnnl_format_tag_any or with format_kind set to #dnnl_format_kind_any.
///
/// Inputs:
///  - src_layer (#dnnl_query_src_md, 0)
///  - src_iter (#dnnl_query_src_md, 1), if used
///  - src_iter_c (#dnnl_query_src_md, 2), if used
///  - weights_layer (#dnnl_query_weights_md, 0)
///  - weights_iter (#dnnl_query_weights_md, 1)
///  - bias (#dnnl_query_weights_md, 2), if used
///
/// Outputs:
///  - dst_layer (#dnnl_query_dst_md, 0)
///  - dst_iter (#dnnl_query_dst_md, 1), if used
///  - dst_iter_c (#dnnl_query_dst_md, 2), if used
///  - workspace (#dnnl_query_workspace_md, 0),
///     if @p prop_kind equals #dnnl_forward_training; must be queried for
///     using @ref dnnl_primitive_desc_query_md() after a corresponding
///     primitive descriptor is created
///
/// @param rnn_desc Output descriptor for LSTM primitive.
/// @param prop_kind Propagation kind. Possible values are
///     #dnnl_forward_training and #dnnl_forward_inference.
/// @param direction RNN direction. See @ref dnnl_rnn_direction_t for more
///     info.
/// @param src_layer_desc Memory descriptor for the input vector.
/// @param src_iter_desc Memory descriptor for the input recurrent hidden
///     state vector.
/// @param src_iter_c_desc Memory descriptor for the input recurrent cell
///     state vector.
/// @param weights_layer_desc Memory descriptor for the weights applied to the
///     layer input.
/// @param weights_iter_desc Memory descriptor for the weights applied to the
///     recurrent input.
/// @param bias_desc Bias memory descriptor.
/// @param dst_layer_desc Memory descriptor for the output vector.
/// @param dst_iter_desc Memory descriptor for the output recurrent hidden
///     state vector.
/// @param dst_iter_c_desc Memory descriptor for the output recurrent cell
///     state vector.
/// @param flags Unused.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_lstm_forward_desc_init(dnnl_rnn_desc_t *rnn_desc,
        dnnl_prop_kind_t prop_kind, dnnl_rnn_direction_t direction,
        const dnnl_memory_desc_t *src_layer_desc,
        const dnnl_memory_desc_t *src_iter_desc,
        const dnnl_memory_desc_t *src_iter_c_desc,
        const dnnl_memory_desc_t *weights_layer_desc,
        const dnnl_memory_desc_t *weights_iter_desc,
        const dnnl_memory_desc_t *bias_desc,
        const dnnl_memory_desc_t *dst_layer_desc,
        const dnnl_memory_desc_t *dst_iter_desc,
        const dnnl_memory_desc_t *dst_iter_c_desc, unsigned flags);

/// Initializes a descriptor for LSTM backward propagation primitive.
///
/// The @p src_iter_desc together with @p diff_iter_desc, @p src_iter_c_desc
/// together with @p src_iter_c_desc, @p bias_desc together with @p
/// diff_bias_desc, @p dst_iter_desc together with @p diff_dst_iter_desc, and
/// @p dst_iter_c_desc together with @p diff_dst_iter_c_desc, may either be @c
/// NULL or point to a zero memory descriptor. This would then indicate that
/// the LSTM backward propagation primitive should not use them and should
/// default to zero values instead.
///
/// @note
///     All memory descriptors are allowed to be initialized with
///     #dnnl_format_tag_any or with format_kind set to #dnnl_format_kind_any.
///
/// Inputs:
///  - src_layer (#dnnl_query_src_md, 0)
///  - src_iter (#dnnl_query_src_md, 1), if used
///  - src_iter_c (#dnnl_query_src_md, 2), if used
///  - weights_layer (#dnnl_query_weights_md, 0)
///  - weights_iter (#dnnl_query_weights_md, 1)
///  - bias (#dnnl_query_weights_md, 2), if used
///  - dst_layer (#dnnl_query_dst_md, 0)
///  - dst_iter (#dnnl_query_dst_md, 1), if used
///  - dst_iter_c (#dnnl_query_dst_md, 2), if used
///  - diff_dst_layer (#dnnl_query_diff_dst_md, 0)
///  - diff_dst_iter (#dnnl_query_diff_dst_md, 1), if used
///  - diff_dst_iter_c (#dnnl_query_diff_dst_md, 2), if used
///  - workspace (#dnnl_query_workspace_md, 0)
///
/// Outputs:
///  - diff_src_layer (#dnnl_query_diff_src_md, 0)
///  - diff_src_iter (#dnnl_query_diff_src_md, 1), if used
///  - diff_src_iter_c (#dnnl_query_diff_src_md, 2), if used
///  - diff_weights_layer (#dnnl_query_diff_weights_md, 0)
///  - diff_weights_iter (#dnnl_query_diff_weights_md, 1)
///  - diff_bias (#dnnl_query_diff_weights_md, 2), if used
///
/// @param rnn_desc Output descriptor for LSTM primitive.
/// @param prop_kind Propagation kind. Must be #dnnl_backward.
/// @param direction RNN direction. See @ref dnnl_rnn_direction_t for more
///     info.
/// @param src_layer_desc Memory descriptor for the input vector.
/// @param src_iter_desc Memory descriptor for the input recurrent hidden
///     state vector.
/// @param src_iter_c_desc Memory descriptor for the input recurrent cell
///     state vector.
/// @param weights_layer_desc Memory descriptor for the weights applied to the
///     layer input.
/// @param weights_iter_desc Memory descriptor for the weights applied to the
///     recurrent input.
/// @param bias_desc Bias memory descriptor.
/// @param dst_layer_desc Memory descriptor for the output vector.
/// @param dst_iter_desc Memory descriptor for the output recurrent hidden
///     state vector.
/// @param dst_iter_c_desc Memory descriptor for the output recurrent cell
///     state vector.
/// @param diff_src_layer_desc Memory descriptor for the diff of input vector.
/// @param diff_src_iter_desc Memory descriptor for the diff of input recurrent
///     hidden state vector.
/// @param diff_src_iter_c_desc Memory descriptor for the diff of input
/// recurrent cell state vector.
/// @param diff_weights_layer_desc Memory descriptor for the diff of weights
///     applied to the layer input.
/// @param diff_weights_iter_desc Memory descriptor for the diff of weights
///     applied to the recurrent input.
/// @param diff_bias_desc Diff bias memory descriptor.
/// @param diff_dst_layer_desc Memory descriptor for the diff of output
///     vector.
/// @param diff_dst_iter_desc Memory descriptor for the diff of output
///     recurrent hidden state vector.
/// @param diff_dst_iter_c_desc Memory descriptor for the diff of output
///     recurrent cell state vector.
/// @param flags Unused.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_lstm_backward_desc_init(dnnl_rnn_desc_t *rnn_desc,
        dnnl_prop_kind_t prop_kind, dnnl_rnn_direction_t direction,
        const dnnl_memory_desc_t *src_layer_desc,
        const dnnl_memory_desc_t *src_iter_desc,
        const dnnl_memory_desc_t *src_iter_c_desc,
        const dnnl_memory_desc_t *weights_layer_desc,
        const dnnl_memory_desc_t *weights_iter_desc,
        const dnnl_memory_desc_t *bias_desc,
        const dnnl_memory_desc_t *dst_layer_desc,
        const dnnl_memory_desc_t *dst_iter_desc,
        const dnnl_memory_desc_t *dst_iter_c_desc,
        const dnnl_memory_desc_t *diff_src_layer_desc,
        const dnnl_memory_desc_t *diff_src_iter_desc,
        const dnnl_memory_desc_t *diff_src_iter_c_desc,
        const dnnl_memory_desc_t *diff_weights_layer_desc,
        const dnnl_memory_desc_t *diff_weights_iter_desc,
        const dnnl_memory_desc_t *diff_bias_desc,
        const dnnl_memory_desc_t *diff_dst_layer_desc,
        const dnnl_memory_desc_t *diff_dst_iter_desc,
        const dnnl_memory_desc_t *diff_dst_iter_c_desc, unsigned flags);

/// Initializes a descriptor for GRU forward propagation primitive.
///
/// The @p src_iter_desc, @p bias_desc, and @p dst_iter, may either be @c NULL
/// or point to a zero memory descriptor. This would then indicate that the
/// GRU forward propagation primitive should not use them and should default
/// to zero values instead.
///
/// @note
///     All memory descriptors are allowed to be initialized with
///     #dnnl_format_tag_any or with format_kind set to #dnnl_format_kind_any.
///
/// Inputs:
///  - src_layer (#dnnl_query_src_md, 0)
///  - src_iter (#dnnl_query_src_md, 1), if used
///  - weights_layer (#dnnl_query_weights_md, 0)
///  - weights_iter (#dnnl_query_weights_md, 1)
///  - bias (#dnnl_query_weights_md, 2), if used
///
/// Outputs:
///  - dst_layer (#dnnl_query_dst_md, 0)
///  - dst_iter (#dnnl_query_dst_md, 1), if used
///  - workspace (#dnnl_query_workspace_md, 0),
///     if @p prop_kind equals #dnnl_forward_training; must be queried for
///     using @ref dnnl_primitive_desc_query_md() after a corresponding
///     primitive descriptor is created
///
/// @param rnn_desc Output descriptor for GRU primitive.
/// @param prop_kind Propagation kind. Possible values are
///     #dnnl_forward_training and #dnnl_forward_inference.
/// @param direction RNN direction. See @ref dnnl_rnn_direction_t for more
///     info.
/// @param src_layer_desc Memory descriptor for the input vector.
/// @param src_iter_desc Memory descriptor for the input recurrent hidden
///     state vector.
/// @param weights_layer_desc Memory descriptor for the weights applied to the
///     layer input.
/// @param weights_iter_desc Memory descriptor for the weights applied to the
///     recurrent input.
/// @param bias_desc Bias memory descriptor.
/// @param dst_layer_desc Memory descriptor for the output vector.
/// @param dst_iter_desc Memory descriptor for the output recurrent hidden
///     state vector.
/// @param flags Unused.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_gru_forward_desc_init(dnnl_rnn_desc_t *rnn_desc,
        dnnl_prop_kind_t prop_kind, dnnl_rnn_direction_t direction,
        const dnnl_memory_desc_t *src_layer_desc,
        const dnnl_memory_desc_t *src_iter_desc,
        const dnnl_memory_desc_t *weights_layer_desc,
        const dnnl_memory_desc_t *weights_iter_desc,
        const dnnl_memory_desc_t *bias_desc,
        const dnnl_memory_desc_t *dst_layer_desc,
        const dnnl_memory_desc_t *dst_iter_desc, unsigned flags);

/// Initializes a descriptor for GRU backward propagation primitive.
///
/// The @p src_iter_desc together with @p diff_src_iter_desc, @p bias_desc
/// together with @p diff_bias_desc, and @p dst_iter together with @p
/// diff_dst_iter, may either be @c NULL or point to a zero memory descriptor.
/// This would then indicate that the GRU backward propagation primitive
/// should not use them and should default to zero values instead.
///
/// @note
///     All memory descriptors are allowed to be initialized with
///     #dnnl_format_tag_any or with format_kind set to #dnnl_format_kind_any.
///
/// Inputs:
///  - src_layer (#dnnl_query_src_md, 0)
///  - src_iter (#dnnl_query_src_md, 1), if used
///  - weights_layer (#dnnl_query_weights_md, 0)
///  - weights_iter (#dnnl_query_weights_md, 1)
///  - bias (#dnnl_query_weights_md, 2), if used
///  - dst_layer (#dnnl_query_dst_md, 0)
///  - dst_iter (#dnnl_query_dst_md, 1), if used
///  - diff_dst_layer (#dnnl_query_diff_dst_md, 0)
///  - diff_dst_iter (#dnnl_query_diff_dst_md, 1), if used
///  - workspace (#dnnl_query_workspace_md, 0)
///
/// Outputs:
///  - diff_src_layer (#dnnl_query_diff_src_md, 0)
///  - diff_src_iter (#dnnl_query_diff_src_md, 1), if used
///  - diff_weights_layer (#dnnl_query_diff_weights_md, 0)
///  - diff_weights_iter (#dnnl_query_diff_weights_md, 1)
///  - diff_bias (#dnnl_query_diff_weights_md, 2), if used
///
/// @param rnn_desc Output descriptor for GRU primitive.
/// @param prop_kind Propagation kind. Must be #dnnl_backward.
/// @param direction RNN direction. See @ref dnnl_rnn_direction_t for more
///     info.
/// @param src_layer_desc Memory descriptor for the input vector.
/// @param src_iter_desc Memory descriptor for the input recurrent hidden
///     state vector.
/// @param weights_layer_desc Memory descriptor for the weights applied to the
///     layer input.
/// @param weights_iter_desc Memory descriptor for the weights applied to the
///     recurrent input.
/// @param bias_desc Bias memory descriptor.
/// @param dst_layer_desc Memory descriptor for the output vector.
/// @param dst_iter_desc Memory descriptor for the output recurrent hidden
///     state vector.
/// @param diff_src_layer_desc Memory descriptor for the diff of input vector.
/// @param diff_src_iter_desc Memory descriptor for the diff of input recurrent
///     hidden state vector.
/// @param diff_weights_layer_desc Memory descriptor for the diff of weights
///     applied to the layer input.
/// @param diff_weights_iter_desc Memory descriptor for the diff of weights
///     applied to the recurrent input.
/// @param diff_bias_desc Diff bias memory descriptor.
/// @param diff_dst_layer_desc Memory descriptor for the diff of output
///     vector.
/// @param diff_dst_iter_desc Memory descriptor for the diff of output
///     recurrent hidden state vector.
/// @param flags Unused.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_gru_backward_desc_init(dnnl_rnn_desc_t *rnn_desc,
        dnnl_prop_kind_t prop_kind, dnnl_rnn_direction_t direction,
        const dnnl_memory_desc_t *src_layer_desc,
        const dnnl_memory_desc_t *src_iter_desc,
        const dnnl_memory_desc_t *weights_layer_desc,
        const dnnl_memory_desc_t *weights_iter_desc,
        const dnnl_memory_desc_t *bias_desc,
        const dnnl_memory_desc_t *dst_layer_desc,
        const dnnl_memory_desc_t *dst_iter_desc,
        const dnnl_memory_desc_t *diff_src_layer_desc,
        const dnnl_memory_desc_t *diff_src_iter_desc,
        const dnnl_memory_desc_t *diff_weights_layer_desc,
        const dnnl_memory_desc_t *diff_weights_iter_desc,
        const dnnl_memory_desc_t *diff_bias_desc,
        const dnnl_memory_desc_t *diff_dst_layer_desc,
        const dnnl_memory_desc_t *diff_dst_iter_desc, unsigned flags);

/// Initializes a descriptor for LBR GRU forward propagation primitive.
///
/// The @p src_iter_desc, @p bias_desc, and @p dst_iter, may either be @c NULL
/// or point to a zero memory descriptor. This would then indicate that the
/// LBR GRU forward propagation primitive should not use them and should
/// default to zero values instead.
///
/// Inputs:
///  - src_layer (#dnnl_query_src_md, 0)
///  - src_iter (#dnnl_query_src_md, 1), if used
///  - weights_layer (#dnnl_query_weights_md, 0)
///  - weights_iter (#dnnl_query_weights_md, 1)
///  - bias (#dnnl_query_weights_md, 2), if used
///
/// Outputs:
///  - dst_layer (#dnnl_query_dst_md, 0)
///  - dst_iter (#dnnl_query_dst_md, 1), if used
///  - workspace (#dnnl_query_workspace_md, 0),
///     if @p prop_kind equals #dnnl_forward_training; must be queried for
///     using @ref dnnl_primitive_desc_query_md() after a corresponding
///     primitive descriptor is created
///
/// @param rnn_desc Output descriptor for LBR GRU primitive.
/// @param prop_kind Propagation kind. Possible values are
///     #dnnl_forward_training and #dnnl_forward_inference.
/// @param direction RNN direction. See @ref dnnl_rnn_direction_t for more
///     info.
/// @param src_layer_desc Memory descriptor for the input vector.
/// @param src_iter_desc Memory descriptor for the input recurrent hidden
///     state vector.
/// @param weights_layer_desc Memory descriptor for the weights applied to the
///     layer input.
/// @param weights_iter_desc Memory descriptor for the weights applied to the
///     recurrent input.
/// @param bias_desc Bias memory descriptor.
/// @param dst_layer_desc Memory descriptor for the output vector.
/// @param dst_iter_desc Memory descriptor for the output recurrent hidden
///     state vector.
/// @param flags Unused.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_lbr_gru_forward_desc_init(dnnl_rnn_desc_t *rnn_desc,
        dnnl_prop_kind_t prop_kind, dnnl_rnn_direction_t direction,
        const dnnl_memory_desc_t *src_layer_desc,
        const dnnl_memory_desc_t *src_iter_desc,
        const dnnl_memory_desc_t *weights_layer_desc,
        const dnnl_memory_desc_t *weights_iter_desc,
        const dnnl_memory_desc_t *bias_desc,
        const dnnl_memory_desc_t *dst_layer_desc,
        const dnnl_memory_desc_t *dst_iter_desc, unsigned flags);

/// Initializes a descriptor for LBR GRU backward propagation primitive.
///
/// The @p src_iter_desc together with @p diff_src_iter_desc, @p bias_desc
/// together with @p diff_bias_desc, and @p dst_iter together with @p
/// diff_dst_iter, may either be @c NULL or point to a zero memory descriptor.
/// This would then indicate that the LBR GRU backward propagation primitive
/// should not use them and should default to zero values instead.
///
/// @note
///     All memory descriptors are allowed to be initialized with
///     #dnnl_format_tag_any or with format_kind set to #dnnl_format_kind_any.
///
/// Inputs:
///  - src_layer (#dnnl_query_src_md, 0)
///  - src_iter (#dnnl_query_src_md, 1), if used
///  - weights_layer (#dnnl_query_weights_md, 0)
///  - weights_iter (#dnnl_query_weights_md, 1)
///  - bias (#dnnl_query_weights_md, 2), if used
///  - dst_layer (#dnnl_query_dst_md, 0)
///  - dst_iter (#dnnl_query_dst_md, 1), if used
///  - diff_dst_layer (#dnnl_query_diff_dst_md, 0)
///  - diff_dst_iter (#dnnl_query_diff_dst_md, 1), if used
///  - workspace (#dnnl_query_workspace_md, 0)
///
/// Outputs:
///  - diff_src_layer (#dnnl_query_diff_src_md, 0)
///  - diff_src_iter (#dnnl_query_diff_src_md, 1), if used
///  - diff_weights_layer (#dnnl_query_diff_weights_md, 0)
///  - diff_weights_iter (#dnnl_query_diff_weights_md, 1)
///  - diff_bias (#dnnl_query_diff_weights_md, 2), if used
///
/// @param rnn_desc Output descriptor for LBR GRU primitive.
/// @param prop_kind Propagation kind. Must be #dnnl_backward.
/// @param direction RNN direction. See @ref dnnl_rnn_direction_t for more
///     info.
/// @param src_layer_desc Memory descriptor for the input vector.
/// @param src_iter_desc Memory descriptor for the input recurrent hidden
///     state vector.
/// @param weights_layer_desc Memory descriptor for the weights applied to the
///     layer input.
/// @param weights_iter_desc Memory descriptor for the weights applied to the
///     recurrent input.
/// @param bias_desc Bias memory descriptor.
/// @param dst_layer_desc Memory descriptor for the output vector.
/// @param dst_iter_desc Memory descriptor for the output recurrent hidden
///     state vector.
/// @param diff_src_layer_desc Memory descriptor for the diff of input vector.
/// @param diff_src_iter_desc Memory descriptor for the diff of input recurrent
///     hidden state vector.
/// @param diff_weights_layer_desc Memory descriptor for the diff of weights
///     applied to the layer input.
/// @param diff_weights_iter_desc Memory descriptor for the diff of weights
///     applied to the recurrent input.
/// @param diff_bias_desc Diff bias memory descriptor.
/// @param diff_dst_layer_desc Memory descriptor for the diff of output
///     vector.
/// @param diff_dst_iter_desc Memory descriptor for the diff of output
///     recurrent hidden state vector.
/// @param flags Unused.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_lbr_gru_backward_desc_init(
        dnnl_rnn_desc_t *rnn_desc, dnnl_prop_kind_t prop_kind,
        dnnl_rnn_direction_t direction,
        const dnnl_memory_desc_t *src_layer_desc,
        const dnnl_memory_desc_t *src_iter_desc,
        const dnnl_memory_desc_t *weights_layer_desc,
        const dnnl_memory_desc_t *weights_iter_desc,
        const dnnl_memory_desc_t *bias_desc,
        const dnnl_memory_desc_t *dst_layer_desc,
        const dnnl_memory_desc_t *dst_iter_desc,
        const dnnl_memory_desc_t *diff_src_layer_desc,
        const dnnl_memory_desc_t *diff_src_iter_desc,
        const dnnl_memory_desc_t *diff_weights_layer_desc,
        const dnnl_memory_desc_t *diff_weights_iter_desc,
        const dnnl_memory_desc_t *diff_bias_desc,
        const dnnl_memory_desc_t *diff_dst_layer_desc,
        const dnnl_memory_desc_t *diff_dst_iter_desc, unsigned flags);

/// @} dnnl_api_rnn

/// @addtogroup dnnl_api_matmul
/// @{

/// Initializes a matrix multiplication descriptor.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///  - weights (#dnnl_query_weights_md, 0)
///  - bias (#dnnl_query_weights_md, 1)
///
/// Outputs:
///  - dst (#dnnl_query_dst_md, 0)
///
/// @param matmul_desc Output descriptor for matmul primitive.
/// @param src_desc Source memory descriptor (matrix A)
/// @param weights_desc Weights memory descriptor (matrix B)
/// @param bias_desc Bias memory descriptor. Passing NULL, a zero memory
///     descriptor, or a memory descriptor with format_kind set to
///     #dnnl_format_kind_undef disables the bias term.
/// @param dst_desc Destination memory descriptor (matrix C).
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_matmul_desc_init(dnnl_matmul_desc_t *matmul_desc,
        const dnnl_memory_desc_t *src_desc,
        const dnnl_memory_desc_t *weights_desc,
        const dnnl_memory_desc_t *bias_desc,
        const dnnl_memory_desc_t *dst_desc);

/// @} dnnl_api_matmul

/// @addtogroup dnnl_api_resampling Resampling
/// @{

/// Initializes a descriptor for a resampling forward propagation primitive.
///
/// @note
///     Destination memory descriptor is allowed to be initialized with
///     #dnnl_format_tag_any or with format_kind set to #dnnl_format_kind_any.
///
/// Inputs:
///  - src (#dnnl_query_src_md, 0)
///
/// Outputs:
///  - dst (#dnnl_query_dst_md, 0)
///
///
/// @param resampling_desc Output descriptor for a resamplinging primitive.
/// @param prop_kind Propagation kind. Possible values are
///     #dnnl_forward_training and #dnnl_forward_inference.
/// @param alg_kind resampling algorithm kind: either #dnnl_resampling_nearest,
///     or #dnnl_resampling_linear.
/// @param factors Array of scaling factors for spatial dimension.
/// @param src_desc Source memory descriptor.
/// @param dst_desc Destination memory descriptor.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_resampling_forward_desc_init(
        dnnl_resampling_desc_t *resampling_desc, dnnl_prop_kind_t prop_kind,
        dnnl_alg_kind_t alg_kind, const float *factors,
        const dnnl_memory_desc_t *src_desc, const dnnl_memory_desc_t *dst_desc);

/// Initializes a descriptor for resampling backward propagation primitive.
///
/// Inputs:
///  - diff_dst (#dnnl_query_diff_dst_md, 0)
///
/// Outputs:
///  - diff_src (#dnnl_query_diff_src_md, 0)
///
/// @param resampling_desc Output descriptor for a resampling primitive.
/// @param alg_kind resamplinging algorithm kind: either
///     #dnnl_resampling_nearest, or #dnnl_resampling_linear.
/// @param diff_src_desc Diff source memory descriptor.
/// @param diff_dst_desc Diff destination memory descriptor.
/// @param factors Array of scaling factors for spatial dimension.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
///
dnnl_status_t DNNL_API dnnl_resampling_backward_desc_init(
        dnnl_resampling_desc_t *resampling_desc, dnnl_alg_kind_t alg_kind,
        const float *factors, const dnnl_memory_desc_t *diff_src_desc,
        const dnnl_memory_desc_t *diff_dst_desc);

/// @} dnnl_api_resampling

/// @} dnnl_api_primitives

/// @addtogroup dnnl_api_engine
/// @{

/// Returns the number of engines of a particular kind.
///
/// @param kind Kind of engines to count.
/// @returns Count of the engines.
size_t DNNL_API dnnl_engine_get_count(dnnl_engine_kind_t kind);

/// Creates an engine.
///
/// @param engine Output engine.
/// @param kind Engine kind.
/// @param index Engine index that should be between 0 and the count of
///     engines of the requested kind.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_engine_create(
        dnnl_engine_t *engine, dnnl_engine_kind_t kind, size_t index);

#if DNNL_GPU_RUNTIME == DNNL_RUNTIME_OCL
/// Creates an engine associated with an OpenCL device and an OpenCL context.
///
/// @param engine Output engine.
/// @param kind Engine kind.
/// @param device Underlying OpenCL device to use for the engine.
/// @param context Underlying OpenCL context to use for the engine.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_engine_create_ocl(dnnl_engine_t *engine,
        dnnl_engine_kind_t kind, cl_device_id device, cl_context context);
#endif

/// Returns the kind of an engine.
///
/// @param engine Engine to query.
/// @param kind Output engine kind.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_engine_get_kind(
        dnnl_engine_t engine, dnnl_engine_kind_t *kind);

#if DNNL_GPU_RUNTIME == DNNL_RUNTIME_OCL
/// Returns the OpenCL context associated with an engine.
///
/// @param engine Engine to query.
/// @param context Output underlying OpenCL context of the engine.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_engine_get_ocl_context(
        dnnl_engine_t engine, cl_context *context);

/// Returns the OpenCL device associated with an engine.
///
/// @param engine Engine to query.
/// @param device Output underlying OpenCL device of the engine.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_engine_get_ocl_device(
        dnnl_engine_t engine, cl_device_id *device);
#endif

/// Destroys an engine.
///
/// @param engine Engine to destroy.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_engine_destroy(dnnl_engine_t engine);

/// @} dnnl_api_engine

/// @addtogroup dnnl_api_stream
/// @{

/// Creates an execution @p stream for @p engine and with @p flags.
dnnl_status_t DNNL_API dnnl_stream_create(
        dnnl_stream_t *stream, dnnl_engine_t engine, unsigned flags);

#if DNNL_GPU_RUNTIME == DNNL_RUNTIME_OCL
/// Creates an execution stream for a given engine associated with
/// an OpenCL command queue.
///
/// @param stream Output execution stream.
/// @param engine Engine to create the execution stream on.
/// @param queue OpenCL command queue to use.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_stream_create_ocl(
        dnnl_stream_t *stream, dnnl_engine_t engine, cl_command_queue queue);

/// Returns the OpenCL command queue associated with an execution stream.
///
/// @param stream Execution stream to query.
/// @param queue Output OpenCL command queue.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_stream_get_ocl_command_queue(
        dnnl_stream_t stream, cl_command_queue *queue);
#endif

/// Waits for all primitives in the execution stream to finish computations.
///
/// @param stream Execution stream.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_stream_wait(dnnl_stream_t stream);

/// Destroys an execution stream.
///
/// @param stream Execution stream to destroy.
/// @returns #dnnl_success on success and a status describing the error
///     otherwise.
dnnl_status_t DNNL_API dnnl_stream_destroy(dnnl_stream_t stream);

/// @} dnnl_api_stream

/// @addtogroup dnnl_api_service
/// @{

/// Configures verbose output to stdout.
///
/// @note
///     Enabling verbose output affects performance.
///     This setting overrides the DNNL_VERBOSE environment variable.
///
/// @param level Verbosity level:
///  - 0: no verbose output (default),
///  - 1: primitive information at execution,
///  - 2: primitive information at creation and execution.
/// @returns #dnnl_invalid_arguments/#dnnl::status::invalid_arguments if the
///     @p level value is invalid, and #dnnl_success/#dnnl::status::success on
///     success.
dnnl_status_t DNNL_API dnnl_set_verbose(int level);

/// Configures dumping of JIT-generated code.
///
/// @note
///     This setting overrides the DNNL_JIT_DUMP environment variable.
///
/// @param enable Flag value. Set to 0 to disable and set to 1 to enable.
/// @returns #dnnl_invalid_arguments/#dnnl::status::invalid_arguments if the
///     @p flag value is invalid, and #dnnl_success/#dnnl::status::success on
///     success.
dnnl_status_t DNNL_API dnnl_set_jit_dump(int enable);

/// Returns library version information.
/// @returns Pointer to a constant structure containing
///  - major: major version number,
///  - minor: minor version number,
///  - patch: patch release number,
///  - hash: git commit hash.
const dnnl_version_t DNNL_API *dnnl_version();

/// Sets library profiling flags. The flags define which profilers are
/// supported.
///
/// @note
///     This setting overrides DNNL_JIT_PROFILE environment variable.
///
/// @sa @ref dev_guide_profilers
///
/// @param flags Profiling flags that can contain the following bits:
///     - @ref DNNL_JIT_PROFILE_VTUNE -- integration with VTune (on by default)
///     - @ref DNNL_JIT_PROFILE_LINUX_JITDUMP -- produce Linux-specific
///         jit-pid.dump output (off by default). The location of the output
///         is controlled via JITDUMPDIR environment variable or via
///         dnnl_set_jit_profiling_jitdumpdir() function.
///     - @ref DNNL_JIT_PROFILE_LINUX_PERFMAP -- produce Linux-specific
///         perf-pid.map output (off by default). The output is always placed
///         into /tmp.
///
///     Passing @ref DNNL_JIT_PROFILE_NONE disables profiling completely.
///
/// @returns #dnnl_invalid_arguments/#dnnl::status::invalid_arguments if the
///     @p flags value is invalid, and #dnnl_success/#dnnl::status::success on
///     success.
dnnl_status_t DNNL_API dnnl_set_jit_profiling_flags(unsigned flags);

/// Sets JIT dump output path. Only applicable to Linux and is only
/// used when profiling flags have DNNL_JIT_PROFILE_LINUX_PERF bit set.
///
/// After the first JIT kernel is generated, the jitdump output will be placed
/// into temporary directory created using the mkdtemp template
/// 'dir/.debug/jit/dnnl.XXXXXX'.
///
/// @sa @ref dev_guide_profilers
///
/// @note
///     This setting overrides JITDUMPDIR environment variable.  If
///     JITDUMPDIR is not set, and this function is never called, the path
///     defaults to HOME. Passing NULL reverts the value to default.
///
/// @note
///     The directory is accessed only when the first JIT kernel is being
///     created. JIT profiling will be disabled in case of any errors
///     accessing or creating this directory.
///
/// @param dir JIT dump output path.
/// @returns #dnnl_success/#dnnl::status::success if the
///     output directory was set correctly and an error status otherwise.
/// @returns #dnnl_unimplemented/#dnnl::status::unimplemented on Windows.
dnnl_status_t DNNL_API dnnl_set_jit_profiling_jitdumpdir(const char *dir);

/// Sets the maximal ISA DNNL can dispatch to on the CPU. See
/// #dnnl_cpu_isa_t and #dnnl::cpu_isa for the list of the values accepted by
/// the C and C++ API functions respectively.
///
/// This function has effect only before the first JIT kernel is generated and
/// will return an error afterwards.
///
/// This function overrides the DNNL_MAX_CPU_ISA environment variable. The
/// environment variable can be set to the desired maximal ISA name in upper
/// case and with dnnl_cpu_isa prefix removed. For example:
/// `DNNL_MAX_CPU_ISA=AVX2`.
///
/// @note
///     The ISAs are only partially ordered:
///         - SSE41 < AVX < AVX2,
///         - AVX2 < AVX512_MIC < AVX512_MIC_4OPS,
///         - AVX2 < AVX512_CORE < AVX512_CORE_VNNI < AVX512_CORE_BF16.
///
/// @sa @ref dev_guide_cpu_dispatcher_control for more details
///
/// @param isa Maximal ISA DNNL should dispatch to. Pass
///     #dnnl_cpu_isa_all/#dnnl::cpu_isa::all to remove ISA restrictions.
/// @returns #dnnl_success/#dnnl::status::success on success and a
///     #dnnl_invalid_arguments/#dnnl::status::invalid_arguments if the @p isa
///     parameter is invalid or the ISA cannot be changed at this time.
/// @returns #dnnl_unimplemented/#dnnl::status::unimplemented if the feature
///     was disabled at build time (see @ref dev_guide_build_options for more
///     details).
dnnl_status_t DNNL_API dnnl_set_max_cpu_isa(dnnl_cpu_isa_t isa);

/// @} dnnl_api_service

/// @addtogroup dnnl_api_blas
/// @{

/// Performs single-precision matrix-matrix multiply.
///
/// The operation is defined as:
///
/// `C := alpha * op( A ) * op( B ) + beta * C`
///
/// where
///  - `op( X ) = X` or `op( X ) = X**T`,
///  - `alpha` and `beta` are scalars, and
///  - `A`, `B`, and `C` are matrices:
///     - `op( A )` is an `MxK` matrix,
///     - `op( B )` is an `KxN` matrix,
///     - `C` is an `MxN` matrix.
///
/// The matrices are assumed to be stored in row-major order (the elements in
/// each of the matrix rows are contiguous in memory).
///
/// @note
///     This API does not support XERBLA. Instead, unlike the standard BLAS
///     functions, this one returns a dnnl_status_t value to allow error
///     handling.
///
/// @param transa Transposition flag for matrix A: 'N' or 'n' means A is not
///     transposed, and 'T' or 't' means that A is transposed.
/// @param transb Transposition flag for matrix B: 'N' or 'n' means B is not
///     transposed, and 'T' or 't' means that B is transposed.
/// @param M The M dimension.
/// @param N The N dimension.
/// @param K The K dimension.
/// @param alpha The alpha parameter that is used to scale the product of
///     matrices A and B.
/// @param A A pointer to the A matrix data.
/// @param lda The leading dimension for the matrix A.
/// @param B A pointer to the B matrix data.
/// @param ldb The leading dimension for the matrix B.
/// @param beta The beta parameter that is used to scale the matrix C.
/// @param C A pointer to the C matrix data.
/// @param ldc The leading dimension for the matrix C.
/// @returns #dnnl_success/#dnnl::status::success on success and a status
///     describing the error otherwise.
dnnl_status_t DNNL_API dnnl_sgemm(char transa, char transb, dnnl_dim_t M,
        dnnl_dim_t N, dnnl_dim_t K, float alpha, const float *A, dnnl_dim_t lda,
        const float *B, dnnl_dim_t ldb, float beta, float *C, dnnl_dim_t ldc);

/// Performs integer matrix-matrix multiply on 8-bit unsigned matrix A, 8-bit
/// signed matrix B, and 32-bit signed resulting matrix C.
///
/// The operation is defined as:
///
/// `C := alpha * (op(A) - A_offset) * (op(B) - B_offset) + beta * C + C_offset`
///
/// where
///  - `op( X ) = X` or `op( X ) = X**T`,
///  - `alpha` and `beta` are scalars, and
///  - `A`, `B`, and `C` are matrices:
///     - `op( A )` is an `MxK` matrix,
///     - `op( B )` is an `KxN` matrix,
///     - `C` is an `MxN` matrix.
///  - `A_offset` is an `MxK` matrix with every element equal the `ao` value,
///  - `B_offset` is an `KxN` matrix with every element equal the `bo` value,
///  - `C_offset` is an `MxN` matrix which is defined by the `co` array of size `len`:
///    - if `offsetc = F`: the `len` must be at least `1`,
///    - if `offsetc = C`: the `len` must be at least `max(1, m)`,
///    - if `offsetc = R`: the `len` must be at least `max(1, n)`,
///
/// The matrices are assumed to be stored in row-major order (the elements in
/// each of the matrix rows are contiguous in memory).
///
/// @note
///     This API does not support XERBLA. Instead, unlike the standard BLAS
///     functions, this one returns a dnnl_status_t value to allow error
///     handling.
///
/// @warning
///     On some architectures saturation may happen during intermediate
///     computations, which would lead to unexpected results. For more
///     details, refer to @ref dev_guide_int8_computations.
///
/// @param transa Transposition flag for matrix A: 'N' or 'n' means A is not
///     transposed, and 'T' or 't' means that A is transposed.
/// @param transb Transposition flag for matrix B: 'N' or 'n' means B is not
///     transposed, and 'T' or 't' means that B is transposed.
/// @param offsetc Flag specifying how offsets should be applied to matrix C:
///     - 'F' means that the same offset will be applied to each element of
///         the matrix C,
///     - 'C' means that individual offset will be applied to each element
///         within each column,
///     - 'R' means that individual offset will be applied to each element
///         within each row.
/// @param M The M dimension.
/// @param N The N dimension.
/// @param K The K dimension.
/// @param alpha The alpha parameter that is used to scale the product of
///     matrices A and B.
/// @param A A pointer to the A matrix data.
/// @param lda The leading dimension for the matrix A.
/// @param ao The offset value for the matrix A.
/// @param B A pointer to the B matrix data.
/// @param ldb The leading dimension for the matrix B.
/// @param bo The offset value for the matrix B.
/// @param beta The beta parameter that is used to scale the matrix C.
/// @param C A pointer to the C matrix data.
/// @param ldc The leading dimension for the matrix C.
/// @param co An array of offset values for the matrix C. The number of
///     elements in the array depends on the value of @p offsetc.
/// @returns #dnnl_success/#dnnl::status::success on success and a status
///     describing the error otherwise.
dnnl_status_t DNNL_API dnnl_gemm_u8s8s32(char transa, char transb, char offsetc,
        dnnl_dim_t M, dnnl_dim_t N, dnnl_dim_t K, float alpha, const uint8_t *A,
        dnnl_dim_t lda, uint8_t ao, const int8_t *B, dnnl_dim_t ldb, int8_t bo,
        float beta, int32_t *C, dnnl_dim_t ldc, const int32_t *co);

/// Performs integer matrix-matrix multiply on 8-bit signed matrix A, 8-bit
/// signed matrix B, and 32-bit signed resulting matrix C.
///
/// The operation is defined as:
///
/// `C := alpha * (op(A) - A_offset) * (op(B) - B_offset) + beta * C + C_offset`
///
/// where
///  - `op( X ) = X` or `op( X ) = X**T`,
///  - `alpha` and `beta` are scalars, and
///  - `A`, `B`, and `C` are matrices:
///     - `op( A )` is an `MxK` matrix,
///     - `op( B )` is an `KxN` matrix,
///     - `C` is an `MxN` matrix.
///  - `A_offset` is an `MxK` matrix with every element equal the `ao` value,
///  - `B_offset` is an `KxN` matrix with every element equal the `bo` value,
///  - `C_offset` is an `MxN` matrix which is defined by the `co` array of size `len`:
///    - if `offsetc = F`: the `len` must be at least `1`,
///    - if `offsetc = C`: the `len` must be at least `max(1, m)`,
///    - if `offsetc = R`: the `len` must be at least `max(1, n)`,
///
/// The matrices are assumed to be stored in row-major order (the elements in
/// each of the matrix rows are contiguous in memory).
///
/// @note
///     This API does not support XERBLA. Instead, unlike the standard BLAS
///     functions, this one returns a dnnl_status_t value to allow error
///     handling.
///
/// @warning
///     On some architectures saturation may happen during intermediate
///     computations, which would lead to unexpected results. For more
///     details, refer to @ref dev_guide_int8_computations.
///
/// @param transa Transposition flag for matrix A: 'N' or 'n' means A is not
///     transposed, and 'T' or 't' means that A is transposed.
/// @param transb Transposition flag for matrix B: 'N' or 'n' means B is not
///     transposed, and 'T' or 't' means that B is transposed.
/// @param offsetc Flag specifying how offsets should be applied to matrix C:
///     - 'F' means that the same offset will be applied to each element of
///         the matrix C,
///     - 'C' means that individual offset will be applied to each element
///         within each column,
///     - 'R' means that individual offset will be applied to each element
///         within each row.
/// @param M The M dimension.
/// @param N The N dimension.
/// @param K The K dimension.
/// @param alpha The alpha parameter that is used to scale the product of
///     matrices A and B.
/// @param A A pointer to the A matrix data.
/// @param lda The leading dimension for the matrix A.
/// @param ao The offset value for the matrix A.
/// @param B A pointer to the B matrix data.
/// @param ldb The leading dimension for the matrix B.
/// @param bo The offset value for the matrix B.
/// @param beta The beta parameter that is used to scale the matrix C.
/// @param C A pointer to the C matrix data.
/// @param ldc The leading dimension for the matrix C.
/// @param co An array of offset values for the matrix C. The number of
///     elements in the array depends on the value of @p offsetc.
/// @returns #dnnl_success/#dnnl::status::success on success and a status
///     describing the error otherwise.
dnnl_status_t DNNL_API dnnl_gemm_s8s8s32(char transa, char transb, char offsetc,
        dnnl_dim_t M, dnnl_dim_t N, dnnl_dim_t K, float alpha, const int8_t *A,
        dnnl_dim_t lda, int8_t ao, const int8_t *B, dnnl_dim_t ldb, int8_t bo,
        float beta, int32_t *C, dnnl_dim_t ldc, const int32_t *co);
/// @} dnnl_api_blas

/// @} dnnl_api

#ifdef __cplusplus
}
#endif

#endif
