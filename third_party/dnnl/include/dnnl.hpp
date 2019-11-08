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
/// C++ API

#ifndef DNNL_HPP
#define DNNL_HPP

#include "dnnl_config.h"

/// @cond DO_NOT_DOCUMENT_THIS
#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <vector>
#include <unordered_map>

#include "dnnl.h"

#if DNNL_GPU_RUNTIME == DNNL_RUNTIME_OCL
#include <CL/cl.h>
#endif
/// @endcond

namespace dnnl {

/// @addtogroup cpp_api C++ API
/// @{

/// @addtogroup cpp_api_utils Utils
/// @{

/// DNNL exception class.
///
/// This class captures the status returned by the failed C API function, error
/// message, and, optionally, handle of the primitive that caused the error.
struct error : public std::exception {
    dnnl_status_t status;
    const char *message;

    /// Constructs an error instance.
    ///
    /// @param astatus The error status returned by the C API.
    /// @param amessage The error message.
    error(dnnl_status_t astatus, const char *amessage)
        : status(astatus), message(amessage) {}

    /// Returns the explanatory string.
    const char *what() const noexcept override { return message; }

    /// A convenience function for wrapping calls to the C API. Checks the
    /// return status and throws an #error in case of failure.
    ///
    /// @param status The error status returned by the C API.
    /// @param message The error message.
    static void wrap_c_api(dnnl_status_t status, const char *message) {
        if (status != dnnl_success) throw error(status, message);
    }
};

/// A class that provides the destructor for an DNNL C handle
template <typename T>
class handle_traits {};

/// A class for wrapping an DNNL handle. It is used as the base
/// class for primitive (#dnnl_primitive_t), engine (#dnnl_engine_t), and
/// stream (#dnnl_stream_t) handles. An object of the #dnnl::handle class
/// can be passed by value. This class enables wrapping:
///  - Newly constructed handles.
///    @n In this case, the constructed handle uses reference counting provided
///    by @p std::shared_ptr with a proper deleter function specified through
///    the @p handle_traits class.
///  - Pre-existing handles returned by the DNNL C API (for
///    example, through dnnl_primitive_get_primitive_desc()).
///    @n In this case, an DNNL C API handle is wrapped without a
///    deleter because it is assumed that the handle wrapper for the original
///    object deletes the handle (this model is similar to @p std::weak_ptr).
template <typename T, typename traits = handle_traits<T>>
class handle {
private:
    static dnnl_status_t dummy_destructor(T) { return dnnl_success; }

    std::shared_ptr<typename std::remove_pointer<T>::type> _data {0};

protected:
    bool operator==(const T other) const { return other == _data.get(); }
    bool operator!=(const T other) const { return !(*this == other); }

public:
    /// Empty constructor.
    ///
    /// Allows declaring an object before actual initialization
    /// (mostly for convenience).
    ///
    /// @warning
    ///     Uninitialized object cannot be used in any library calls.
    ///     Any attempt to use its methods or passing it to the other library
    ///     function will lead to a thrown exception.
    handle() = default;
    handle(const handle<T, traits> &) = default;
    handle(handle<T, traits> &&) = default;
    handle<T, traits> &operator=(handle<T, traits> &&) = default;
    handle<T, traits> &operator=(const handle<T, traits> &) = default;

    /// Constructs a C handle wrapper from a C handle.
    /// @param t The C handle to wrap.
    /// @param weak A flag to specify whether to construct a weak wrapper.
    explicit handle(T t, bool weak = false) { reset(t, weak); }

    /// Resets the value of a C handle.
    /// @param t The new value of the C handle.
    /// @param weak A flag to specify whether the wrapper should be weak.
    void reset(T t, bool weak = false) {
        _data.reset(t, weak ? &dummy_destructor : traits::destructor);
    }

    /// Returns the value of the underlying C handle.
    T get(bool allow_emtpy = false) const {
        T result = _data.get();

        if (allow_emtpy == false && result == nullptr)
            throw dnnl::error(dnnl_invalid_arguments,
                    "attempt to use uninitialized object");

        return result;
    }

    explicit operator T() const { return get(true); }

    explicit operator bool() const { return get(true) != nullptr; }

    bool operator==(const handle &other) const {
        return other._data.get() == _data.get();
    }
    bool operator!=(const handle &other) const { return !(*this == other); }
};

/// @cond DO_NOT_DOCUMENT_THIS
template <>
struct handle_traits<dnnl_memory_t> {
    static constexpr auto destructor = &dnnl_memory_destroy;
};

template <>
struct handle_traits<dnnl_primitive_desc_t> {
    static constexpr auto destructor = &dnnl_primitive_desc_destroy;
};

template <>
struct handle_traits<dnnl_primitive_t> {
    static constexpr auto destructor = &dnnl_primitive_destroy;
};

template <>
struct handle_traits<dnnl_primitive_desc_iterator_t> {
    static constexpr auto destructor = &dnnl_primitive_desc_iterator_destroy;
};
/// @endcond

struct stream;
struct error;
struct memory;
struct primitive_desc;

/// Base class for all computational primitives.
class primitive : public handle<dnnl_primitive_t> {
    friend struct error;
    friend struct stream;
    using handle::handle;

public:
    /// Kinds of primitives. Used to implement a way to extend the library with
    /// new primitives without changing the ABI.
    enum class kind {
        /// Undefined primitive
        undef = dnnl_undefined_primitive,
        /// A reorder primitive.
        reorder = dnnl_reorder,
        /// A shuffle primitive.
        shuffle = dnnl_shuffle,
        /// A (out-of-place) concat primitive.
        concat = dnnl_concat,
        /// A sum primitive.
        sum = dnnl_sum,
        /// A convolution primitive.
        convolution = dnnl_convolution,
        /// A deconvolution primitive.
        deconvolution = dnnl_deconvolution,
        /// An element-wise primitive.
        eltwise = dnnl_eltwise,
        /// A softmax primitive.
        softmax = dnnl_softmax,
        /// A pooling primitive.
        pooling = dnnl_pooling,
        /// An LRN primitive.
        lrn = dnnl_lrn,
        /// A batch normalization primitive.
        batch_normalization = dnnl_batch_normalization,
        /// A layer normalization primitive.
        layer_normalization = dnnl_layer_normalization,
        /// An inner product primitive.
        inner_product = dnnl_inner_product,
        /// A rnn primitive.
        rnn = dnnl_rnn,
        /// A binary primitive.
        binary = dnnl_binary,
    };

    primitive(const_dnnl_primitive_desc_t c_pd);
    primitive(const primitive_desc &pd);

    /// Returns the descriptor of the underlying C API primitive.
    inline const_dnnl_primitive_desc_t get_primitive_desc() const;
    // TODO: use the C++ API wrapper structure.

    void execute(
            stream &astream, const std::unordered_map<int, memory> &args) const;
};

inline dnnl_primitive_kind_t convert_to_c(primitive::kind akind) {
    return static_cast<dnnl_primitive_kind_t>(akind);
}

const_dnnl_primitive_desc_t primitive::get_primitive_desc() const {
    const_dnnl_primitive_desc_t pd;
    error::wrap_c_api(dnnl_primitive_get_primitive_desc(get(), &pd),
            "could not get primitive descriptor by primitive");
    return pd;
}
/// @}

/// @addtogroup cpp_api_enums Common data types and enumerations
/// A proxy to @ref c_api_types in @ref c_api.
///
/// @{

/// Scratchpad mode
enum class scratchpad_mode {
    /// The library manages scratchpad (default)
    library = dnnl_scratchpad_mode_library,
    /// A user shall query and provide the scratchpad memory to primitives
    user = dnnl_scratchpad_mode_user,
};

inline dnnl_scratchpad_mode_t convert_to_c(scratchpad_mode mode) {
    return static_cast<dnnl_scratchpad_mode_t>(mode);
}

/// Propagation kind
enum class prop_kind {
    /// Undefined propagation kind
    undef = dnnl_prop_kind_undef,
    /// Forward data propagation (training mode). In this mode primitives
    /// perform computations necessary for subsequent backward propagation.
    forward_training = dnnl_forward_training,
    /// Forward data propagation (inference mode). In this mode primitives
    /// perform only computations that are necessary for inference and omit
    /// computations that are necessary only for backward propagation.
    forward_inference = dnnl_forward_inference,
    /// Forward data propagation,
    /// alias for #dnnl::prop_kind::forward_inference
    forward_scoring = dnnl_forward_scoring,
    /// Forward data propagation,
    /// alias for #dnnl::prop_kind::forward_training
    forward = dnnl_forward,
    /// Backward propagation (with respect to all parameters).
    backward = dnnl_backward,
    /// Backward data propagation.
    backward_data = dnnl_backward_data,
    /// Backward weights propagation.
    backward_weights = dnnl_backward_weights,
    /// Backward bias propagation.
    backward_bias = dnnl_backward_bias
};

inline dnnl_prop_kind_t convert_to_c(prop_kind kind) {
    return static_cast<dnnl_prop_kind_t>(kind);
}

/// Kinds of algorithms.
enum class algorithm {
    undef = dnnl_alg_kind_undef,
    /// Convolution algorithm(either direct or Winograd) is chosen just in time
    convolution_auto = dnnl_convolution_auto,
    /// Direct convolution
    convolution_direct = dnnl_convolution_direct,
    /// Winograd convolution
    convolution_winograd = dnnl_convolution_winograd,
    /// Direct deconvolution
    deconvolution_direct = dnnl_deconvolution_direct,
    /// Winograd deconvolution
    deconvolution_winograd = dnnl_deconvolution_winograd,
    /// Eltwise: ReLU
    eltwise_relu = dnnl_eltwise_relu,
    /// Eltwise: hyperbolic tangent non-linearity (tanh)
    eltwise_tanh = dnnl_eltwise_tanh,
    /// Eltwise: parametric exponential linear unit (elu)
    eltwise_elu = dnnl_eltwise_elu,
    /// Eltwise: square
    eltwise_square = dnnl_eltwise_square,
    /// Eltwise: abs
    eltwise_abs = dnnl_eltwise_abs,
    /// Eltwise: square root
    eltwise_sqrt = dnnl_eltwise_sqrt,
    /// Eltwise: x*sigmoid(a*x)
    eltwise_swish = dnnl_eltwise_swish,
    /// Eltwise: linear
    eltwise_linear = dnnl_eltwise_linear,
    /// Eltwise: bounded_relu
    eltwise_bounded_relu = dnnl_eltwise_bounded_relu,
    /// Eltwise: soft_relu
    eltwise_soft_relu = dnnl_eltwise_soft_relu,
    /// Eltwise: logistic
    eltwise_logistic = dnnl_eltwise_logistic,
    /// Eltwise: exponent
    eltwise_exp = dnnl_eltwise_exp,
    /// Eltwise: gelu
    eltwise_gelu = dnnl_eltwise_gelu,
    /// Local response normalization (LRN) across multiple channels
    lrn_across_channels = dnnl_lrn_across_channels,
    /// LRN within a single channel
    lrn_within_channel = dnnl_lrn_within_channel,
    /// Max pooling
    pooling_max = dnnl_pooling_max,
    /// Average pooling exclude padding,
    /// alias for #dnnl::algorithm::pooling_avg_include_padding
    pooling_avg = dnnl_pooling_avg,
    /// Average pooling include padding
    pooling_avg_include_padding = dnnl_pooling_avg_include_padding,
    /// Average pooling exclude padding
    pooling_avg_exclude_padding = dnnl_pooling_avg_exclude_padding,
    /// RNN cell
    vanilla_rnn = dnnl_vanilla_rnn,
    /// LSTM cell
    vanilla_lstm = dnnl_vanilla_lstm,
    /// GRU cell
    vanilla_gru = dnnl_vanilla_gru,
    /// GRU cell with linear before reset
    ///
    /// Modification of original GRU cell. Differs from #dnnl_vanilla_gru
    /// in how the new memory gate is calculated:
    /// \f[ c_t = tanh(W_c*x_t + b_{c_x} + r_t*(U_c*h_{t-1}+b_{c_h})) \f]
    /// Primitive expects 4 biases on input:
    /// \f$[b_{u}, b_{r}, b_{c_x}, b_{c_h}]\f$
    lbr_gru = dnnl_lbr_gru,
    /// Binary add
    binary_add = dnnl_binary_add,
    /// Binary mul
    binary_mul = dnnl_binary_mul,
};

inline dnnl_alg_kind_t convert_to_c(algorithm aalgorithm) {
    return static_cast<dnnl_alg_kind_t>(aalgorithm);
}

/// Flags for batch normalization primitive.
enum class normalization_flags : unsigned {
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
    use_global_stats = dnnl_use_global_stats,

    /// Use scale and shift parameters
    ///
    /// If specified:
    ///  - on forward propagation use scale and shift (aka scale and bias) for
    ///    the batch normalization results
    ///  - on backward propagation
    ///    (for prop_kind == #dnnl::prop_kind::backward) compute
    ///    diff wrt to scale and shift (hence one extra output used)
    ///
    /// If not specified:
    ///  - on backward propagation
    ///    prop_kind == #dnnl::prop_kind::backward_data has the
    ///    same behavior as prop_kind == #dnnl::prop_kind::backward
    use_scale_shift = dnnl_use_scaleshift,

    /// Fuse with ReLU
    ///
    /// If specified:
    ///  - on inference this option behaves the same as if the primitive were
    ///    fused with ReLU via post ops API
    ///  - on training primitive requires workspace (required to be able to
    ///    perform backward pass)
    fuse_norm_relu = dnnl_fuse_norm_relu
};

inline dnnl_normalization_flags_t convert_to_c(normalization_flags aflag) {
    return static_cast<dnnl_normalization_flags_t>(aflag);
}

enum class rnn_flags : unsigned { undef = dnnl_rnn_flags_undef };

inline dnnl_rnn_flags_t convert_to_c(rnn_flags aflag) {
    return static_cast<dnnl_rnn_flags_t>(aflag);
}

#define DNNL_DEFINE_BITMASK_OPS(enum_name) \
    inline enum_name operator|(enum_name lhs, enum_name rhs) { \
        return static_cast<enum_name>( \
                static_cast<unsigned>(lhs) | static_cast<unsigned>(rhs)); \
    } \
\
    inline enum_name operator&(enum_name lhs, enum_name rhs) { \
        return static_cast<enum_name>( \
                static_cast<unsigned>(lhs) & static_cast<unsigned>(rhs)); \
    } \
\
    inline enum_name operator^(enum_name lhs, enum_name rhs) { \
        return static_cast<enum_name>( \
                static_cast<unsigned>(lhs) ^ static_cast<unsigned>(rhs)); \
    } \
\
    inline enum_name &operator|=(enum_name &lhs, enum_name rhs) { \
        lhs = static_cast<enum_name>( \
                static_cast<unsigned>(lhs) | static_cast<unsigned>(rhs)); \
        return lhs; \
    } \
\
    inline enum_name &operator&=(enum_name &lhs, enum_name rhs) { \
        lhs = static_cast<enum_name>( \
                static_cast<unsigned>(lhs) & static_cast<unsigned>(rhs)); \
        return lhs; \
    } \
\
    inline enum_name &operator^=(enum_name &lhs, enum_name rhs) { \
        lhs = static_cast<enum_name>( \
                static_cast<unsigned>(lhs) ^ static_cast<unsigned>(rhs)); \
        return lhs; \
    } \
\
    inline enum_name operator~(enum_name rhs) { \
        return static_cast<enum_name>(~static_cast<unsigned>(rhs)); \
    }

DNNL_DEFINE_BITMASK_OPS(normalization_flags)
DNNL_DEFINE_BITMASK_OPS(rnn_flags)

#undef DNNL_DEFINE_BITMASK_OPS

enum class rnn_direction {
    unidirectional_left2right = dnnl_unidirectional_left2right,
    unidirectional_right2left = dnnl_unidirectional_right2left,
    unidirectional = dnnl_unidirectional,
    bidirectional_concat = dnnl_bidirectional_concat,
    bidirectional_sum = dnnl_bidirectional_sum,
};

inline dnnl_rnn_direction_t convert_to_c(rnn_direction adir) {
    return static_cast<dnnl_rnn_direction_t>(adir);
}

/// Primitive descriptor query specification
///
/// In general should be used from C++ API since required queries are directly
/// implemented as class members (for instance, a query for source memory
/// descriptor).
///
/// For more information see @ref dnnl_query_t.
enum class query {
    /// no query
    undef = dnnl_query_undef,

    /// execution engine
    engine = dnnl_query_engine,
    /// primitive kind
    primitive_kind = dnnl_query_primitive_kind,

    /// number of inputs expected
    num_of_inputs_s32 = dnnl_query_num_of_inputs_s32,
    /// number of outputs expected
    num_of_outputs_s32 = dnnl_query_num_of_outputs_s32,

    /// runtime estimation (seconds), unimplemented
    time_estimate_f64 = dnnl_query_time_estimate_f64,
    /// memory consumption (bytes)
    ///
    /// extra (scratch) memory, additional to all inputs and outputs memory
    ///
    /// @sa @ref dev_guide_attributes_scratchpad
    memory_consumption_s64 = dnnl_query_memory_consumption_s64,

    /// scratchpad engine
    ///
    /// engine to be used for creating scratchpad memory
    scratchpad_engine = dnnl_query_scratchpad_engine,

    /// reorder source engine
    reorder_src_engine = dnnl_query_reorder_src_engine,
    /// reorder destination engine
    reorder_dst_engine = dnnl_query_reorder_dst_engine,

    /// implementation name
    impl_info_str = dnnl_query_impl_info_str,

    /// op descriptor
    op_d = dnnl_query_op_d,
    /// convolution descriptor
    convolution_d = dnnl_query_convolution_d,
    /// deconvolution descriptor
    deconvolution_d = dnnl_query_deconvolution_d,
    /// shuffle descriptor
    shuffle_d = dnnl_query_shuffle_d,
    /// eltwise descriptor
    eltwise_d = dnnl_query_eltwise_d,
    /// softmax descriptor
    softmax_d = dnnl_query_softmax_d,
    /// pooling descriptor
    pooling_d = dnnl_query_pooling_d,
    /// lrn descriptor
    lrn_d = dnnl_query_lrn_d,
    /// batch normalization descriptor
    batch_normalization_d = dnnl_query_batch_normalization_d,
    /// layer normalization descriptor
    layer_normalization_d = dnnl_query_layer_normalization_d,
    /// inner product descriptor
    inner_product_d = dnnl_query_inner_product_d,
    /// rnn descriptor
    rnn_d = dnnl_query_rnn_d,
    /// binary descriptor
    binary_d = dnnl_query_binary_d,

    /// source memory desc
    src_md = dnnl_query_src_md,
    /// source gradient memory desc
    diff_src_md = dnnl_query_diff_src_md,
    /// weights memory descriptor desc
    weights_md = dnnl_query_weights_md,
    /// weights grad. memory desc
    diff_weights_md = dnnl_query_diff_weights_md,
    /// destination memory desc
    dst_md = dnnl_query_dst_md,
    /// destination grad. memory desc
    diff_dst_md = dnnl_query_diff_dst_md,
    /// workspace memory desc
    workspace_md = dnnl_query_workspace_md,
    /// scratchpad memory desc
    scratchpad_md = dnnl_query_scratchpad_md,
};

inline dnnl_query_t convert_to_c(query aquery) {
    return static_cast<dnnl_query_t>(aquery);
}

/// @}

/// @addtogroup cpp_api_attr Attributes
/// An extension for controlling primitive behavior.
///
/// @sa @ref c_api_attributes in @ref c_api
/// @{

/// @cond DO_NOT_DOCUMENT_THIS
template <>
struct handle_traits<dnnl_post_ops_t> {
    static constexpr auto destructor = &dnnl_post_ops_destroy;
};
/// @endcond

/// Post operations
///
/// @sa @ref dev_guide_attributes_post_ops
struct post_ops : public handle<dnnl_post_ops_t> {
    using handle<dnnl_post_ops_t>::handle;

    /// Creates an empty sequence of post operations.
    post_ops() {
        dnnl_post_ops_t result;
        error::wrap_c_api(dnnl_post_ops_create(&result),
                "could not create post operation sequence");
        reset(result);
    }

    /// Returns the length of post operations
    int len() const { return dnnl_post_ops_len(get()); }

    /// Returns the kind of post operation with index @p index.
    primitive::kind kind(int index) const {
        error::wrap_c_api(index < len() ? dnnl_success : dnnl_invalid_arguments,
                "post_ops index is out of range");
        return static_cast<primitive::kind>(
                dnnl_post_ops_get_kind(get(), index));
    }

    /// Appends accumulation (sum) post operation. Prior to accumulating the
    /// result, the previous value would be multiplied by @p scale.
    ///
    /// The kind of this post operation is #dnnl_sum.
    ///
    /// This feature might improve performance for cases like residual learning
    /// blocks, where the result of convolution is accumulated to the previously
    /// computed activations. The parameter @p scale might be extreme for the
    /// integer-based computations when the result and previous activations have
    /// different logical scaling factors.
    ///
    /// In the simplest case when the accumulation is the only post operation,
    /// the computations would be:
    /// dst[] <- scale * dst[] + op(...) // instead of dst[] <- op(...)
    ///
    /// @note
    ///     This post operation (as well as all the others) disregards the
    ///     original layout of the destination; that is, the layout of the
    ///     original destination is expected to be the same as the layout of the
    ///     stored destination.
    void append_sum(float scale = 1.) {
        error::wrap_c_api(
                dnnl_post_ops_append_sum(get(), scale), "could not append sum");
    }

    /// Gets the parameters of the accumulation (sum) post operation with index
    /// @p index.
    void get_params_sum(int index, float &scale) const {
        error::wrap_c_api(dnnl_post_ops_get_params_sum(get(), index, &scale),
                "could not get sum params");
    }

    /// Appends eltwise post operation.
    ///
    /// The kind of this post operation is #dnnl_eltwise.
    ///
    /// In the simplest case when the eltwise is the only post operation, the
    /// computations would be:
    /// dst[] <- scale * eltwise_op ( op(...) ) // instead of dst[] <- op(...)
    /// where eltwise_op is configured with the given parameters.
    void append_eltwise(float scale, algorithm alg, float alpha, float beta) {
        error::wrap_c_api(dnnl_post_ops_append_eltwise(
                                  get(), scale, convert_to_c(alg), alpha, beta),
                "could not append eltwise");
    }

    /// Gets the eltwise parameters of the post operation with index @p index.
    void get_params_eltwise(int index, float &scale, algorithm &alg,
            float &alpha, float &beta) const {
        dnnl_alg_kind_t c_alg;
        error::wrap_c_api(dnnl_post_ops_get_params_eltwise(
                                  get(), index, &scale, &c_alg, &alpha, &beta),
                "could not get eltwise params");
        alg = static_cast<algorithm>(c_alg);
    }
};

/// @cond DO_NOT_DOCUMENT_THIS
template <>
struct handle_traits<dnnl_primitive_attr_t> {
    static constexpr auto destructor = &dnnl_primitive_attr_destroy;
};
/// @endcond

/// Primitive attributes
///
/// @sa @ref dev_guide_attributes
struct primitive_attr : public handle<dnnl_primitive_attr_t> {
    using handle<dnnl_primitive_attr_t>::handle;

    /// Creates default primitive attributes.
    primitive_attr() {
        dnnl_primitive_attr_t result;
        error::wrap_c_api(dnnl_primitive_attr_create(&result),
                "could not create a primitive attr");
        reset(result);
    }

    /// Creates primitive attributes from a C dnnl_primitive_attr_t handle.
    /// The resulting handle is never weak and the C handle will be destroyed
    /// during the destruction of the C++ object.
    primitive_attr(dnnl_primitive_attr_t attr)
        : handle<dnnl_primitive_attr_t>(attr) {}

    /// Returns the scratchpad mode.
    scratchpad_mode get_scratchpad_mode() const {
        dnnl_scratchpad_mode_t result;
        error::wrap_c_api(
                dnnl_primitive_attr_get_scratchpad_mode(get(), &result),
                "could not get scratchpad mode");
        return scratchpad_mode(result);
    }

    /// Sets scratchpad mode.
    void set_scratchpad_mode(scratchpad_mode mode) {
        error::wrap_c_api(dnnl_primitive_attr_set_scratchpad_mode(
                                  get(), dnnl::convert_to_c(mode)),
                "could not set scratchpad mode");
    }

    /// Gets correspondence scale @p mask and a constant floating point vector
    /// of output @p scales previously set by set_output_scales.
    void get_output_scales(int &mask, std::vector<float> &scales) const {
        dnnl_dim_t count;
        int c_mask;
        const float *c_scales;
        error::wrap_c_api(dnnl_primitive_attr_get_output_scales(
                                  get(), &count, &c_mask, &c_scales),
                "could not get int output scales");
        scales.resize(count);

        mask = c_mask;
        for (dnnl_dim_t c = 0; c < count; ++c)
            scales[c] = c_scales[c];
    }

    /// Sets output scales for primitive operations. The correspondence scale
    /// @p mask is stored for future use.
    ///
    /// The @p mask argument defines the correspondence between the output
    /// tensor dimensions and the @p scales vector. Set the i-th bit of @p mask
    /// to 1 to use a dedicated scaling factor for each slice of the output
    /// tensor over the i-th dimension. Set @p mask to 0 to use a common
    /// scaling factor for the whole output tensor.
    ///
    /// @note
    ///      The dimension order is always native and does not depend on the
    ///      actual layout used. Examples:
    ///       - 2D dimensional data the order of dimensions is always: (n, c)
    ///       - 4D dimensional data the order is always: (n, c, h, w)
    ///       - 5D dimensional weights the order is always: (g, oc, ic, kh, kw)
    void set_output_scales(int mask, const std::vector<float> &scales) {
        error::wrap_c_api(dnnl_primitive_attr_set_output_scales(get(),
                                  (dnnl_dim_t)scales.size(), mask, &scales[0]),
                "could not set int output scales");
    }

    /// Returns @p post_ops previously set by set_post_ops.
    const post_ops get_post_ops() const {
        post_ops result;
        const_dnnl_post_ops_t c_result;
        error::wrap_c_api(dnnl_primitive_attr_get_post_ops(get(), &c_result),
                "could not get post operation sequence");
        result.reset(const_cast<dnnl_post_ops_t>(c_result), true);
        return result;
    }

    /// Sets @p post_ops for future use.
    void set_post_ops(post_ops ops) {
        error::wrap_c_api(dnnl_primitive_attr_set_post_ops(get(), ops.get()),
                "could not set post operation sequence");
    }

    /// Sets quantization @p scale and @p shift for RNN data tensors.  For
    /// performance reasons, the low-precision configuration of the RNN
    /// primitive expects input activations to have the unsigned int8 data type.
    /// Scale and shift used to quantize floating-point data to unsigned integer
    /// must be passed to the RNN primitive using attributes.
    /// @note
    ///     Quantization scale and shift are common for src_layer, src_iter,
    ///     dst_iter, and dst_layer.
    void set_rnn_data_qparams(float scale, float shift) {
        error::wrap_c_api(
                dnnl_primitive_attr_set_rnn_data_qparams(get(), scale, shift),
                "could not set rnn data int scale/shift");
    }

    /// Sets quantization scales @p weights_scales for RNN weights tensors.  The
    /// low-precision configuration of the RNN primitive expects input weights
    /// to have the signed int8 data type. Scales used to quantize
    /// floating-point data to signed integer must be passed to the RNN
    /// primitive using attributes.  The @p mask argument defines correspondence
    /// between output tensor dimensions and the @p weights_scales array. Set
    /// the i-th bit of @p mask to 1 to use a dedicated scaling factor for each
    /// slice of the output tensor over the i-th dimension. Set @p mask to 0 to
    /// use a common scaling factor for the whole output tensor.
    /// @note
    ///      The dimension order is always native and does not depend on the
    ///      actual layout used. For example, five-dimensional weights always
    ///      have (l, d, i, g, o) logical dimension ordering.
    /// @note
    ///     Quantization scales are common for weights_layer and
    ///     weights_iteration
    /// @note
    ///     There is no way to check whether @p count corresponds to @p mask
    ///     until an actual primitive descriptor is created, so it is the user's
    ///     responsibility to set proper values. The following formula must
    ///     hold:
    ///
    ///      \f[count = \prod\limits_{d \in mask} output.dims[d]\f]
    void set_rnn_weights_qparams(int mask, const std::vector<float> &scales) {
        error::wrap_c_api(dnnl_primitive_attr_set_rnn_weights_qparams(
                                  get(), (int)scales.size(), mask, &scales[0]),
                "could not set rnn weights int scales");
    }
};

/// @}

/// @addtogroup cpp_api_engine Engine
/// Engine operations.
///
/// @sa @ref c_api_engine in @ref c_api
/// @{

/// @cond DO_NOT_DOCUMENT_THIS
template <>
struct handle_traits<dnnl_engine_t> {
    static constexpr auto destructor = &dnnl_engine_destroy;
};
/// @endcond

/// An execution engine.
struct engine : public handle<dnnl_engine_t> {
    friend class primitive;
    friend struct reorder;

    /// Kinds of engines.
    enum class kind {
        /// An unspecified engine
        any = dnnl_any_engine,
        /// CPU engine
        cpu = dnnl_cpu,
        /// GPU engine
        gpu = dnnl_gpu,
    };

    engine() = default;

    /// Returns the number of engines of a certain kind.
    ///
    /// @param akind The kind of engines to count.
    static size_t get_count(kind akind) {
        return dnnl_engine_get_count(convert_to_c(akind));
    }

    /// Constructs an engine.
    ///
    /// @param akind The kind of engine to construct.
    /// @param index The index of the engine. Must be less than the value
    ///              returned by #get_count() for this particular kind
    ///              of engine.
    engine(kind akind, size_t index) {
        dnnl_engine_t aengine;
        error::wrap_c_api(
                dnnl_engine_create(&aengine, convert_to_c(akind), index),
                "could not create an engine");
        reset(aengine);
    }

#if DNNL_GPU_RUNTIME == DNNL_RUNTIME_OCL
    /// Constructs an engine of particular @p akind associated with the given
    /// OpenCL @p device and @p context objects.
    engine(kind akind, cl_device_id device, cl_context context) {
        dnnl_engine_t aengine;
        error::wrap_c_api(dnnl_engine_create_ocl(&aengine, convert_to_c(akind),
                                  device, context),
                "could not create an engine");
        reset(aengine);
    }
#endif

    /// Constructs an engine from other engine @p aengine.
    explicit engine(const dnnl_engine_t &aengine) : handle(aengine, true) {}

    /// Constructs an engine from the primitive descriptor @p pd
    /// by querying its engine.
    engine(const handle<dnnl_primitive_desc_t> &pd) {
        dnnl_engine_t engine_q;
        error::wrap_c_api(
                dnnl_primitive_desc_query(pd.get(),
                        dnnl::convert_to_c(dnnl::query::engine), 0, &engine_q),
                "could not get engine from primitive_desc");
        reset(engine_q, true);
    }

    /// Returns the kind of the engine.
    kind get_kind() const {
        dnnl_engine_kind_t akind;
        error::wrap_c_api(dnnl_engine_get_kind(get(), &akind),
                "could not get the engine kind");
        return static_cast<engine::kind>(akind);
    }

#if DNNL_GPU_RUNTIME == DNNL_RUNTIME_OCL
    /// Returns the OpenCL context associated with the engine.
    cl_context get_ocl_context() const {
        cl_context context = nullptr;
        error::wrap_c_api(dnnl_engine_get_ocl_context(get(), &context),
                "could not get a context handle");
        return context;
    }

    /// Returns the OpenCL device associated with the engine.
    cl_device_id get_ocl_device() const {
        cl_device_id device = nullptr;
        error::wrap_c_api(dnnl_engine_get_ocl_device(get(), &device),
                "could not get a device handle");
        return device;
    }
#endif

    template <class primitive_desc>
    static engine query(const primitive_desc &pd) {
        return query(pd, dnnl::query::engine);
    }

private:
    static dnnl_engine_kind_t convert_to_c(kind akind) {
        return static_cast<dnnl_engine_kind_t>(akind);
    }

    template <class primitive_desc>
    static engine query(const primitive_desc &pd, dnnl::query what) {
        dnnl_engine_t engine_q;
        error::wrap_c_api(dnnl_primitive_desc_query(pd.get(),
                                  dnnl::convert_to_c(what), 0, &engine_q),
                "could not get engine from primitive_desc");

        return engine(engine_q);
    }
};

/// @}

/// @addtogroup cpp_api_stream Stream
/// Execution stream operations
///
/// @sa @ref c_api_stream in @ref c_api
/// @{

/// @cond DO_NOT_DOCUMENT_THIS
template <>
struct handle_traits<dnnl_stream_t> {
    static constexpr auto destructor = &dnnl_stream_destroy;
};
/// @endcond

/// An execution stream.
struct stream : public handle<dnnl_stream_t> {
    using handle::handle;

    /// @brief Stream flags.
    enum class flags : unsigned {
        /// Default order execution. Either in-order or out-of-order depending
        /// on the engine runtime
        default_order = dnnl_stream_default_order,
        /// In-order execution.
        in_order = dnnl_stream_default_order,
        /// Out-of-order execution.
        out_of_order = dnnl_stream_out_of_order,
        /// Default stream configuration.
        default_flags = dnnl_stream_default_flags,
    };

    stream() = default;

    /// Constructs a stream.
    stream(const engine &aengine, flags aflags = flags::default_flags) {
        dnnl_stream_t astream;
        error::wrap_c_api(dnnl_stream_create(&astream, aengine.get(),
                                  static_cast<dnnl_stream_flags_t>(aflags)),
                "could not create a stream");
        reset(astream);
    }

#if DNNL_GPU_RUNTIME == DNNL_RUNTIME_OCL
    /// Constructs a stream associated with the engine @p eng and with the
    /// OpenCL command queue @p queue.
    stream(const engine &eng, cl_command_queue queue) {
        dnnl_stream_t astream;
        error::wrap_c_api(dnnl_stream_create_ocl(&astream, eng.get(), queue),
                "could not create a stream");
        reset(astream);
    }

    /// Returns the OpenCL command queue associated with the stream.
    cl_command_queue get_ocl_command_queue() const {
        cl_command_queue queue = nullptr;
        error::wrap_c_api(dnnl_stream_get_ocl_command_queue(get(), &queue),
                "could not get OpenCL command queue");
        return queue;
    }
#endif

    /// Waits for all primitives in the stream to finish.
    stream &wait() {
        error::wrap_c_api(dnnl_stream_wait(get()), "could not wait a stream");
        return *this;
    }
};

inline stream::flags operator|(stream::flags lhs, stream::flags rhs) {
    return static_cast<stream::flags>(
            static_cast<unsigned>(lhs) | static_cast<unsigned>(rhs));
}

inline stream::flags operator&(stream::flags lhs, stream::flags rhs) {
    return static_cast<stream::flags>(
            static_cast<unsigned>(lhs) & static_cast<unsigned>(rhs));
}

inline stream::flags operator^(stream::flags lhs, stream::flags rhs) {
    return static_cast<stream::flags>(
            static_cast<unsigned>(lhs) ^ static_cast<unsigned>(rhs));
}

inline stream::flags operator~(stream::flags rhs) {
    return static_cast<stream::flags>(~static_cast<unsigned>(rhs));
}

/// @}

/// @addtogroup cpp_api_memory_related Memory and memory related operations
/// @{

/// @addtogroup cpp_api_memory Memory
/// A primitive to describe and store data.
///
/// For more information, refer to @ref c_api_memory in @ref c_api.
/// @{

/// Memory that describes the data.
struct memory : public handle<dnnl_memory_t> {
    typedef dnnl_dim_t dim;
    typedef std::vector<dim> dims;

    template <typename T>
    static void validate_dims(const std::vector<T> &v) {
        if (v.size() > DNNL_MAX_NDIMS)
            throw error(dnnl_invalid_arguments, "invalid dimensions");
    }

    /// Data type specification
    enum class data_type {
        /// Undefined data type, used for empty memory descriptors.
        undef = dnnl_data_type_undef,
        /// 16-bit/half-precision floating point.
        f16 = dnnl_f16,
        /// non-standard 16-bit (bfloat16 w/ 7 bit mantissa) floating point.
        bf16 = dnnl_bf16,
        /// 32-bit/single-precision floating point.
        f32 = dnnl_f32,
        /// 32-bit signed integer.
        s32 = dnnl_s32,
        /// 8-bit signed integer.
        s8 = dnnl_s8,
        /// 8-bit unsigned integer.
        u8 = dnnl_u8,
    };

    /// Memory format kind
    enum class format_kind {
        /// Undefined memory format kind, used for empty memory descriptors.
        undef = dnnl_format_kind_undef,
        /// Unspecified format kind.
        /// The primitive selects a format automatically.
        any = dnnl_format_kind_any,
        /// A tensor in a generic format described by the stride and blocking
        /// values in each dimension. See @ref dnnl_blocking_desc_t for more
        /// information.
        blocked = dnnl_blocked,
        /// Weights format used in 8bit Winograd convolution
        wino = dnnl_format_kind_wino,
        /// Packed weights format used in RNN
        packed = dnnl_format_kind_rnn_packed,
    };

    /// Memory format tag specification. See @ref dnnl_format_tag_t for a
    /// detailed description.
    enum class format_tag {
        /// Undefined memory format tag
        undef = dnnl_format_tag_undef,
        /// Placeholder memory format tag. The primitive selects a format
        /// automatically.
        any = dnnl_format_tag_any,

        // Semantic agnostic section
        // The physical order of dimensions is defined by the permutation of the
        // characters, assuming that ab..z defines the natural order.

        // Plain formats

        a = dnnl_a, ///< plain 1D tensor
        ab = dnnl_ab, ///< plain 2D tensor
        abc = dnnl_abc, ///< plain 3D tensor
        abcd = dnnl_abcd, ///< plain 4D tensor
        abcde = dnnl_abcde, ///< plain 5D tensor
        abcdef = dnnl_abcdef, ///< plain 6D tensor

        // Permuted plain formats

        abdec = dnnl_abdec, ///< permuted 5D tensor
        acb = dnnl_acb, ///< permuted 3D tensor
        acbde = dnnl_acbde, ///< permuted 5D tensor
        acdb = dnnl_acdb, ///< permuted 4D tensor
        acdeb = dnnl_acdeb, ///< permuted 5D tensor
        ba = dnnl_ba, ///< permuted 2D tensor
        bac = dnnl_bac, ///< permuted 3D tensor
        bacd = dnnl_bacd, ///< permuted 4D tensor
        bcda = dnnl_bcda, ///< permuted 4D tensor
        cba = dnnl_cba, ///< permuted 3D tensor
        cdba = dnnl_cdba, ///< permuted 4D tensor
        cdeba = dnnl_cdeba, ///< permuted 5D tensor
        decab = dnnl_decab, ///< permuted 5D tensor

        // Opaque blocked formats

        Abc16a = dnnl_Abc16a,
        ABc16a16b = dnnl_ABc16a16b,
        aBc16b = dnnl_aBc16b,
        ABc16b16a = dnnl_ABc16b16a,
        Abc4a = dnnl_Abc4a,
        aBc4b = dnnl_aBc4b,
        ABc4b16a4b = dnnl_ABc4b16a4b,
        ABc4b4a = dnnl_ABc4b4a,
        ABc8a16b2a = dnnl_ABc8a16b2a,
        ABc8a8b = dnnl_ABc8a8b,
        aBc8b = dnnl_aBc8b,
        ABc8b16a2b = dnnl_ABc8b16a2b,
        ABc8b8a = dnnl_ABc8b8a,
        Abcd16a = dnnl_Abcd16a,
        ABcd16a16b = dnnl_ABcd16a16b,
        aBcd16b = dnnl_aBcd16b,
        ABcd16b16a = dnnl_ABcd16b16a,
        aBCd16b16c = dnnl_aBCd16b16c,
        aBCd16c16b = dnnl_aBCd16c16b,
        Abcd4a = dnnl_Abcd4a,
        aBcd4b = dnnl_aBcd4b,
        ABcd4b16a4b = dnnl_ABcd4b16a4b,
        ABcd4b4a = dnnl_ABcd4b4a,
        aBCd4c16b4c = dnnl_aBCd4c16b4c,
        aBCd4c4b = dnnl_aBCd4c4b,
        ABcd8a16b2a = dnnl_ABcd8a16b2a,
        ABcd8a8b = dnnl_ABcd8a8b,
        /// 4D tensor blocked by 2nd dimension with block size 8
        aBcd8b = dnnl_aBcd8b,
        ABcd8b16a2b = dnnl_ABcd8b16a2b,
        aBCd8b16c2b = dnnl_aBCd8b16c2b,
        /// 4D tensor blocked by 1st and 2nd dimension with block size 8
        ABcd8b8a = dnnl_ABcd8b8a,
        aBCd8b8c = dnnl_aBCd8b8c,
        aBCd8c16b2c = dnnl_aBCd8c16b2c,
        aBCd8c8b = dnnl_aBCd8c8b,
        Abcde16a = dnnl_Abcde16a,
        ABcde16a16b = dnnl_ABcde16a16b,
        aBcde16b = dnnl_aBcde16b,
        ABcde16b16a = dnnl_ABcde16b16a,
        aBCde16b16c = dnnl_aBCde16b16c,
        aBCde16c16b = dnnl_aBCde16c16b,
        aBCde2c8b4c = dnnl_aBCde2c8b4c,
        Abcde4a = dnnl_Abcde4a,
        aBcde4b = dnnl_aBcde4b,
        ABcde4b4a = dnnl_ABcde4b4a,
        aBCde4b4c = dnnl_aBCde4b4c,
        aBCde4c16b4c = dnnl_aBCde4c16b4c,
        aBCde4c4b = dnnl_aBCde4c4b,
        Abcde8a = dnnl_Abcde8a,
        ABcde8a8b = dnnl_ABcde8a8b,
        aBcde8b = dnnl_aBcde8b,
        ABcde8b16a2b = dnnl_ABcde8b16a2b,
        aBCde8b16c2b = dnnl_aBCde8b16c2b,
        ABcde8b8a = dnnl_ABcde8b8a,
        aBCde8b8c = dnnl_aBCde8b8c,
        ABcd4a8b8a4b = dnnl_ABcd4a8b8a4b,
        ABcd2a8b8a2b = dnnl_ABcd2a8b8a2b,
        aBCde4b8c8b4c = dnnl_aBCde4b8c8b4c,
        aBCde2b8c8b2c = dnnl_aBCde2b8c8b2c,
        aBCde8c16b2c = dnnl_aBCde8c16b2c,
        aBCde8c8b = dnnl_aBCde8c8b,
        aBcdef16b = dnnl_aBcdef16b,
        aBCdef16b16c = dnnl_aBCdef16b16c,
        aBCdef16c16b = dnnl_aBCdef16c16b,
        aBcdef4b = dnnl_aBcdef4b,
        aBCdef4c4b = dnnl_aBCdef4c4b,
        aBCdef8b8c = dnnl_aBCdef8b8c,
        aBCdef8c16b2c = dnnl_aBCdef8c16b2c,
        aBCdef8c8b = dnnl_aBCdef8c8b,
        aBdc16b = dnnl_aBdc16b,
        aBdc4b = dnnl_aBdc4b,
        aBdc8b = dnnl_aBdc8b,
        aBdec16b = dnnl_aBdec16b,
        aBdec4b = dnnl_aBdec4b,
        aBdec8b = dnnl_aBdec8b,
        aBdefc16b = dnnl_aBdefc16b,
        aCBdef16c16b = dnnl_aCBdef16c16b,
        aBdefc4b = dnnl_aBdefc4b,
        aBdefc8b = dnnl_aBdefc8b,
        Acb16a = dnnl_Acb16a,
        Acb4a = dnnl_Acb4a,
        Acb8a = dnnl_Acb8a,
        aCBd16b16c = dnnl_aCBd16b16c,
        aCBd16c16b = dnnl_aCBd16c16b,
        aCBde16b16c = dnnl_aCBde16b16c,
        aCBde16c16b = dnnl_aCBde16c16b,
        Acdb16a = dnnl_Acdb16a,
        Acdb4a = dnnl_Acdb4a,
        Acdb8a = dnnl_Acdb8a,
        Acdeb16a = dnnl_Acdeb16a,
        Acdeb4a = dnnl_Acdeb4a,
        Acdeb8a = dnnl_Acdeb8a,
        BAc16a16b = dnnl_BAc16a16b,
        BAc16b16a = dnnl_BAc16b16a,
        BAcd16a16b = dnnl_BAcd16a16b,
        BAcd16b16a = dnnl_BAcd16b16a,
        ABcd32a32b = dnnl_ABcd32a32b,
        BAcde16b16 = dnnl_BAcde16b16a,
        aBdec32b = dnnl_aBdec32b,
        Abcdef16a = dnnl_Abcdef16a,
        Acdb32a = dnnl_Acdb32a,
        format_tag_last = dnnl_format_tag_last,

        x = dnnl_x,
        /// 2D CNN activations tensor,
        /// an alias to #dnnl::memory::format_tag::ab
        nc = dnnl_nc,
        cn = dnnl_cn,
        tn = dnnl_tn,
        nt = dnnl_nt,
        ncw = dnnl_ncw,
        nwc = dnnl_nwc,
        /// 4D CNN activations tensor,
        /// an alias to #dnnl::memory::format_tag::abcd
        nchw = dnnl_nchw,
        /// 4D CNN activations tensor,
        /// an alias to #dnnl::memory::format_tag::acdb
        nhwc = dnnl_nhwc,
        /// 4D CNN activations tensor,
        /// an alias to #dnnl::memory::format_tag::bcda
        chwn = dnnl_chwn,
        ncdhw = dnnl_ncdhw,
        ndhwc = dnnl_ndhwc,
        oi = dnnl_oi,
        io = dnnl_io,
        oiw = dnnl_oiw,
        wio = dnnl_wio,
        oihw = dnnl_oihw,
        hwio = dnnl_hwio,
        ihwo = dnnl_ihwo,
        iohw = dnnl_iohw,
        oidhw = dnnl_oidhw,
        dhwio = dnnl_dhwio,
        goiw = dnnl_goiw,
        goihw = dnnl_goihw,
        hwigo = dnnl_hwigo,
        giohw = dnnl_giohw,
        goidhw = dnnl_goidhw,
        tnc = dnnl_tnc,
        ntc = dnnl_ntc,
        ldnc = dnnl_ldnc,
        ldigo = dnnl_ldigo,
        ldgoi = dnnl_ldgoi,
        ldgo = dnnl_ldgo,
        nCdhw16c = dnnl_nCdhw16c,
        nCdhw4c = dnnl_nCdhw4c,
        nCdhw8c = dnnl_nCdhw8c,
        nChw16c = dnnl_nChw16c,
        nChw4c = dnnl_nChw4c,
        nChw8c = dnnl_nChw8c,
        nCw16c = dnnl_nCw16c,
        nCw4c = dnnl_nCw4c,
        nCw8c = dnnl_nCw8c,
        NCw16n16c = dnnl_NCw16n16c,
        NChw16n16c = dnnl_NChw16n16c,
        NCdhw16n16c = dnnl_NCdhw16n16c,
        NChw32n32c = dnnl_NChw32n32c,
        IOhw16i16o = dnnl_IOhw16i16o,
        Ohwi32o = dnnl_Ohwi32o,
        IOdhw16i16o = dnnl_IOdhw16i16o,
        gIOhw16i16o = dnnl_gIOhw16i16o,
        gOhwi32o = dnnl_gOhwi32o,
        Goidhw16g = dnnl_Goidhw16g,
        IOw16o16i = dnnl_IOw16o16i,
        OIw16i16o = dnnl_OIw16i16o,
        IOw16i16o = dnnl_IOw16i16o,
        gIOw16i16o = dnnl_gIOw16i16o,
        OIw16o16i = dnnl_OIw16o16i,
        Oiw16o = dnnl_Oiw16o,
        OIw4i16o4i = dnnl_OIw4i16o4i,
        OIw4i4o = dnnl_OIw4i4o,
        Oiw4o = dnnl_Oiw4o,
        OIw8i16o2i = dnnl_OIw8i16o2i,
        OIw8i8o = dnnl_OIw8i8o,
        OIw8o16i2o = dnnl_OIw8o16i2o,
        OIw8o8i = dnnl_OIw8o8i,
        Owi16o = dnnl_Owi16o,
        Owi4o = dnnl_Owi4o,
        Owi8o = dnnl_Owi8o,
        IOhw16o16i = dnnl_IOhw16o16i,
        Ohwi16o = dnnl_Ohwi16o,
        Ohwi4o = dnnl_Ohwi4o,
        Ohwi8o = dnnl_Ohwi8o,
        OIhw16i16o = dnnl_OIhw16i16o,
        OIhw16o16i = dnnl_OIhw16o16i,
        Oihw16o = dnnl_Oihw16o,
        OIhw4i16o4i = dnnl_OIhw4i16o4i,
        OIhw4i4o = dnnl_OIhw4i4o,
        Oihw4o = dnnl_Oihw4o,
        OIhw8i16o2i = dnnl_OIhw8i16o2i,
        OIhw8i8o = dnnl_OIhw8i8o,
        OIhw8o16i2o = dnnl_OIhw8o16i2o,
        OIhw8o8i = dnnl_OIhw8o8i,
        Odhwi16o = dnnl_Odhwi16o,
        Odhwi4o = dnnl_Odhwi4o,
        Odhwi8o = dnnl_Odhwi8o,
        OIdhw16i16o = dnnl_OIdhw16i16o,
        OIdhw16o16i = dnnl_OIdhw16o16i,
        Oidhw16o = dnnl_Oidhw16o,
        OIdhw4i4o = dnnl_OIdhw4i4o,
        Oidhw4o = dnnl_Oidhw4o,
        OIdhw8i16o2i = dnnl_OIdhw8i16o2i,
        OIdhw8i8o = dnnl_OIdhw8i8o,
        OIdhw8o8i = dnnl_OIdhw8o8i,
        gIOw16o16i = dnnl_gIOw16o16i,
        gOIw16i16o = dnnl_gOIw16i16o,
        gOIw16o16i = dnnl_gOIw16o16i,
        gOiw16o = dnnl_gOiw16o,
        gOIw4i16o4i = dnnl_gOIw4i16o4i,
        gOIw4i4o = dnnl_gOIw4i4o,
        gOiw4o = dnnl_gOiw4o,
        gOIw8i16o2i = dnnl_gOIw8i16o2i,
        gOIw8i8o = dnnl_gOIw8i8o,
        gOIw8o16i2o = dnnl_gOIw8o16i2o,
        gOIw8o8i = dnnl_gOIw8o8i,
        gOwi16o = dnnl_gOwi16o,
        gOwi4o = dnnl_gOwi4o,
        gOwi8o = dnnl_gOwi8o,
        gIOhw16o16i = dnnl_gIOhw16o16i,
        gOhwi16o = dnnl_gOhwi16o,
        gOhwi4o = dnnl_gOhwi4o,
        gOhwi8o = dnnl_gOhwi8o,
        Goihw16g = dnnl_Goihw16g,
        gOIhw16i16o = dnnl_gOIhw16i16o,
        gOIhw16o16i = dnnl_gOIhw16o16i,
        gOihw16o = dnnl_gOihw16o,
        gOIhw2i8o4i = dnnl_gOIhw2i8o4i,
        gOIhw4i16o4i = dnnl_gOIhw4i16o4i,
        gOIhw4i4o = dnnl_gOIhw4i4o,
        gOIhw4o4i = dnnl_gOIhw4o4i,
        gOihw4o = dnnl_gOihw4o,
        Goihw8g = dnnl_Goihw8g,
        gOIhw8i16o2i = dnnl_gOIhw8i16o2i,
        gOIhw8i8o = dnnl_gOIhw8i8o,
        gOIhw8o16i2o = dnnl_gOIhw8o16i2o,
        OIhw4o8i8o4i = dnnl_OIhw4o8i8o4i,
        OIhw2o8i8o2i = dnnl_OIhw2o8i8o2i,
        gOIhw4o8i8o4i = dnnl_gOIhw4o8i8o4i,
        gOIhw2o8i8o2i = dnnl_gOIhw2o8i8o2i,
        gOIhw8o8i = dnnl_gOIhw8o8i,
        gIOdhw16i16o = dnnl_gIOdhw16i16o,
        gOdhwi16o = dnnl_gOdhwi16o,
        gOdhwi4o = dnnl_gOdhwi4o,
        gOdhwi8o = dnnl_gOdhwi8o,
        gOIdhw16i16o = dnnl_gOIdhw16i16o,
        gOIdhw16o16i = dnnl_gOIdhw16o16i,
        gOidhw16o = dnnl_gOidhw16o,
        gOIdhw4i4o = dnnl_gOIdhw4i4o,
        gOidhw4o = dnnl_gOidhw4o,
        gOIdhw8i16o2i = dnnl_gOIdhw8i16o2i,
        gOIdhw8i8o = dnnl_gOIdhw8i8o,
        gOIdhw8o8i = dnnl_gOIdhw8o8i,
    };

    /// A memory descriptor.
    struct desc {
        friend struct memory;
        /// The underlying C API data structure.
        dnnl_memory_desc_t data;

        /// Constructs a zero memory descriptor
        desc() : data() {}

        /// Constructs a memory descriptor.
        ///
        /// @param adims Data dimensions
        /// @param adata_type Data precision/type.
        /// @param aformat_tag Data layout format tag.
        desc(const dims &adims, data_type adata_type, format_tag aformat_tag) {
            validate_dims(adims);
            error::wrap_c_api(
                    dnnl_memory_desc_init_by_tag(&data, (int)adims.size(),
                            adims.size() == 0 ? nullptr : &adims[0],
                            convert_to_c(adata_type),
                            convert_to_c(aformat_tag)),
                    "could not initialize a memory descriptor by tag");
        }

        /// Constructs a memory descriptor by strides.
        ///
        /// @param adims Data dimensions
        /// @param adata_type Data precision/type.
        /// @param astrides The strides for dimensions.
        desc(const dims &adims, data_type adata_type, const dims &astrides) {
            validate_dims(adims);
            error::wrap_c_api(
                    dnnl_memory_desc_init_by_strides(&data, (int)adims.size(),
                            adims.size() == 0 ? nullptr : &adims[0],
                            convert_to_c(adata_type),
                            astrides.size() == 0 ? nullptr : &astrides[0]),
                    "could not initialize a memory descriptor by strides");
        }

        /// Constructs a memory descriptor from a C API data structure.
        ///
        /// @param adata A C API #dnnl_memory_desc_t structure.
        desc(const dnnl_memory_desc_t &adata) : data(adata) {}

        /// Constructs a sub-memory descriptor.
        //
        /// @param adims Sizes of a sub-memory
        /// @param offsets Offsets of a sub-memory
        desc submemory_desc(const dims &adims, const dims &offsets) {
            dnnl_memory_desc_t sub_md;
            error::wrap_c_api(dnnl_memory_desc_init_submemory(
                                      &sub_md, &data, &adims[0], &offsets[0]),
                    "could not initialize a sub-memory");
            return desc(sub_md);
        }

        /// Constructs a memory descriptor by reshaping existing one.
        desc reshape(const dims &adims) {
            dnnl_memory_desc_t out_md;
            error::wrap_c_api(dnnl_memory_desc_reshape(&out_md, &data,
                                      (int)adims.size(), &adims[0]),
                    "could not reshape a memory descriptor");
            return desc(out_md);
        }

        /// Returns the number of bytes required to allocate the memory
        /// described including the padding area.
        size_t get_size() const { return dnnl_memory_desc_get_size(&data); }

        /// Returns true if the memory descriptor describes an empty memory
        bool is_zero() const { return data.ndims == 0; }

        bool operator==(const desc &other) const {
            return dnnl_memory_desc_equal(&data, &other.data) != 0;
        }

        bool operator!=(const desc &other) const { return !operator==(other); }
    };

    memory() = default;

    /// Constructs a memory.
    ///
    /// @param md Memory descriptor.
    /// @param aengine Engine.
    /// @param ahandle handle.
    memory(const desc &md, const engine &aengine, void *ahandle) {
        dnnl_memory_t result;
        error::wrap_c_api(
                dnnl_memory_create(&result, &md.data, aengine.get(), ahandle),
                "could not create a memory");
        reset(result);
    }

    /// Constructs a memory.
    ///
    /// @param md Memory descriptor.
    /// @param aengine Engine.
    memory(const desc &md, const engine &aengine)
        : memory(md, aengine, DNNL_MEMORY_ALLOCATE) {}

    /// Returns the descriptor of the memory.
    desc get_desc() const {
        const dnnl_memory_desc_t *cdesc;
        error::wrap_c_api(dnnl_memory_get_memory_desc(get(), &cdesc),
                "could not get memory descriptor from a memory");
        return desc(*cdesc);
    }

    /// Returns the engine of the memory.
    engine get_engine() const {
        dnnl_engine_t engine_q;
        error::wrap_c_api(dnnl_memory_get_engine(get(), &engine_q),
                "could not get engine from a memory");
        return engine(engine_q);
    }

    /// Returns a handle of the data contained in the memory.
    ///
    /// On the CPU engine, this is a pointer to the allocated memory.
    void *get_data_handle() const {
        void *handle;
        error::wrap_c_api(dnnl_memory_get_data_handle(get(), &handle),
                "could not get native handle");
        return handle;
    }

    void set_data_handle(void *handle) const {
        error::wrap_c_api(dnnl_memory_set_data_handle(get(), handle),
                "could not set native handle");
    }

    /// Maps the data of the memory.
    ///
    /// Mapping allows to read/write directly from/to the memory contents for
    /// engines that do not support direct memory access.
    ///
    /// Mapping is an exclusive operation - a memory object cannot be used in
    /// other operations until this memory object is unmapped.
    /// @tparam T Type of the pointer to be mapped.
    ///
    /// @note Any primitives working with the memory should be completed before
    ///       mapping. Use stream::wait() to synchronize the corresponding
    ///       execution stream.
    ///
    /// @note Map/unmap API is provided mainly for debug/testing purposes and
    ///       its performance may be suboptimal.
    template <typename T = void>
    T *map_data() const {
        void *mapped_ptr;
        error::wrap_c_api(dnnl_memory_map_data(get(), &mapped_ptr),
                "could not map the data");
        return static_cast<T *>(mapped_ptr);
    }

    /// Unmaps the previously mapped data for the memory.
    ///
    /// Any changes of the mapped data are synchronized back to the memory
    /// after the call is complete. The mapped pointer must be
    /// obtained through a map_data() call.
    ///
    /// @note Map/unmap API is provided mainly for debug/testing purposes and
    ///       its performance may be suboptimal.
    void unmap_data(void *mapped_ptr) const {
        error::wrap_c_api(dnnl_memory_unmap_data(get(), mapped_ptr),
                "could not unmap the data");
    }

#if DNNL_GPU_RUNTIME == DNNL_RUNTIME_OCL
    /// Returns the OpenCL memory object associated with the memory.
    cl_mem get_ocl_mem_object() const {
        cl_mem mem_object;
        error::wrap_c_api(dnnl_memory_get_ocl_mem_object(get(), &mem_object),
                "could not get OpenCL memory object");
        return mem_object;
    }

    /// Sets the OpenCL memory object @p mem_object associated with the memory.
    void set_ocl_mem_object(cl_mem mem_object) {
        error::wrap_c_api(dnnl_memory_set_ocl_mem_object(get(), mem_object),
                "could not set OpenCL memory object");
    }
#endif

    // Must go away or be private:
    static dnnl_data_type_t convert_to_c(data_type adata_type) {
        return static_cast<dnnl_data_type_t>(adata_type);
    }
    static dnnl_format_tag_t convert_to_c(format_tag aformat) {
        return static_cast<dnnl_format_tag_t>(aformat);
    }
};

inline bool operator==(dnnl_data_type_t a, memory::data_type b) {
    return a == memory::convert_to_c(b);
}
inline bool operator!=(dnnl_data_type_t a, memory::data_type b) {
    return !(a == b);
}
inline bool operator==(memory::data_type a, dnnl_data_type_t b) {
    return b == a;
}
inline bool operator!=(memory::data_type a, dnnl_data_type_t b) {
    return !(a == b);
}

inline bool operator==(dnnl_format_tag_t a, memory::format_tag b) {
    return a == memory::convert_to_c(b);
}
inline bool operator!=(dnnl_format_tag_t a, memory::format_tag b) {
    return !(a == b);
}
inline bool operator==(memory::format_tag a, dnnl_format_tag_t b) {
    return b == a;
}
inline bool operator!=(memory::format_tag a, dnnl_format_tag_t b) {
    return !(a == b);
}

/// @}

/// @addtogroup cpp_api_primitives Primitives
/// @{

/// @addtogroup cpp_api_primitive_descriptors Primitive descriptors
/// @{

/// The base class for all primitive descriptors.
struct primitive_desc_base : public handle<dnnl_primitive_desc_t> {
    using handle<dnnl_primitive_desc_t>::handle;

    primitive_desc_base() = default;

    /// Returns the engine of the primitive descriptor.
    engine get_engine() const { return engine::query(*this); }

    /// Returns implementation name.
    const char *impl_info_str() const {
        const char *res;
        error::wrap_c_api(dnnl_primitive_desc_query(
                                  get(), dnnl_query_impl_info_str, 0, &res),
                "could not query implementation info string");
        return res;
    }

    /// Queries the memory::dim value (same as int64_t).
    memory::dim query_s64(query q) const {
        memory::dim res;
        dnnl_status_t status = dnnl_primitive_desc_query(
                get(), dnnl::convert_to_c(q), 0, &res);
        return status == dnnl_success ? res : 0;
    }

    /// Queries and returns requested memory descriptor.
    memory::desc query_md(query what, int idx = 0) const {
        std::vector<query> valid_q {query::src_md, query::diff_src_md,
                query::weights_md, query::diff_weights_md, query::dst_md,
                query::diff_dst_md, query::workspace_md, query::scratchpad_md};
        if (!std::any_of(valid_q.cbegin(), valid_q.cend(),
                    [=](query q) { return what == q; }))
            throw error(dnnl_invalid_arguments, "invalid memory query");

        const dnnl_memory_desc_t *cdesc = dnnl_primitive_desc_query_md(
                get(), dnnl::convert_to_c(what), idx);
        return memory::desc(*cdesc);
    }

    /// Queries scratchpad memory descriptor.
    ///
    /// @sa @ref dev_guide_attributes_scratchpad
    /// Returns a zero_md if no scratchpad is required.
    memory::desc scratchpad_desc() const {
        return query_md(query::scratchpad_md, 0);
    }

    /// Returns the engine that owns the scratchpad memory.
    engine scratchpad_engine() const {
        dnnl_engine_t engine_q;
        error::wrap_c_api(dnnl_primitive_desc_query(get(),
                                  dnnl::convert_to_c(query::scratchpad_engine),
                                  0, &engine_q),
                "could not get scratchpad engine from a primitive_desc");

        return engine(engine_q);
    }

    /// Returns the attributes.
    primitive_attr get_primitive_attr() const {
        const_dnnl_primitive_attr_t const_cattr;
        error::wrap_c_api(dnnl_primitive_desc_get_attr(get(), &const_cattr),
                "could not get attributes");
        dnnl_primitive_attr_t cattr;
        error::wrap_c_api(dnnl_primitive_attr_clone(&cattr, const_cattr),
                "could not clone attributes");

        return primitive_attr(cattr);
    }

protected:
    void reset_with_clone(const_dnnl_primitive_desc_t pd) {
        dnnl_primitive_desc_t new_pd;
        error::wrap_c_api(dnnl_primitive_desc_clone(&new_pd, pd),
                "could not clone primitive descriptor");
        reset(new_pd);
    }

    primitive_desc_base(
            dnnl_primitive_desc_t pd, dnnl::primitive::kind prim_kind)
        : primitive_desc_base(pd, prim_kind, dnnl::prop_kind::undef) {}

    primitive_desc_base(dnnl_primitive_desc_t pd,
            dnnl::primitive::kind prim_kind, dnnl::prop_kind prop_kind)
        : primitive_desc_base(pd, prim_kind, prop_kind, prop_kind) {}

    /// Constructs a primitive_desc from a C counterpart. Performs certain
    /// checks to make sure that the C counterpart refers to a primitive
    /// descriptor of a particular primitive kind and propagation kind.
    ///
    /// Note: primitive_desc constructed this way does not support
    /// next_impl().
    primitive_desc_base(dnnl_primitive_desc_t pd,
            dnnl::primitive::kind prim_kind, dnnl::prop_kind prop_kind1,
            dnnl::prop_kind prop_kind2) {
        // It is OK to pass an empty primitive descriptor
        if (pd == nullptr) return;

        dnnl_status_t rc;

        dnnl_primitive_kind_t c_prim_kind = convert_to_c(prim_kind);
        dnnl_prop_kind_t c_prop_kind1 = convert_to_c(prop_kind1);
        dnnl_prop_kind_t c_prop_kind2 = convert_to_c(prop_kind2);

        // Check that primitive kind matches
        dnnl_primitive_kind_t pd_kind;
        rc = dnnl_primitive_desc_query(
                pd, dnnl_query_primitive_kind, 0, (void *)&pd_kind);
        error::wrap_c_api(rc,
                "could not get primitive kind from the primitive descriptor");
        if (pd_kind != c_prim_kind)
            throw error(dnnl_invalid_arguments,
                    "primitive descriptor operation kind mismatch");

        // Check that propagation kind matches
        dnnl_prop_kind_t pd_prop_kind;
        rc = dnnl_primitive_desc_query(
                pd, dnnl_query_prop_kind, 0, (void *)&pd_prop_kind);

        // Something went wrong
        if (rc != dnnl_success && rc != dnnl_unimplemented)
            throw error(dnnl_invalid_arguments,
                    "could not get propagation kind "
                    "from the primitive descriptor");

        // Everything is fine
        if ((rc == dnnl_unimplemented && c_prop_kind1 == dnnl_prop_kind_undef)
                || (rc == dnnl_success
                        && (pd_prop_kind == c_prop_kind1
                                || pd_prop_kind == c_prop_kind2))) {
            reset_with_clone(pd);
            return;
        }

        // We could get the propagation kind but there is a mismatch
        throw error(dnnl_invalid_arguments,
                "primitive descriptor propagation kind mismatch");
    }
};

/// @}
/// @}

/// @addtogroup cpp_api_reorder Reorder
/// A primitive to copy data between memory formats.
///
/// @sa @ref dev_guide_reorder in developer guide
/// @sa @ref c_api_reorder in @ref c_api
/// @{

/// Initializes a reorder primitive using the description of the source
/// (@p src_engine and @p src_md) and destination (@p dst_engine and @p dst_md)
/// memory, and an @p attr attribute.
struct reorder : public primitive {
    struct primitive_desc : public primitive_desc_base {
        using primitive_desc_base::primitive_desc_base;

        primitive_desc() = default;

        primitive_desc(const engine &src_engine, const memory::desc &src_md,
                const engine &dst_engine, const memory::desc &dst_md,
                const primitive_attr &aattr = primitive_attr()) {
            dnnl_primitive_desc_t result;
            error::wrap_c_api(
                    dnnl_reorder_primitive_desc_create(&result, &src_md.data,
                            src_engine.get(), &dst_md.data, dst_engine.get(),
                            aattr.get()),
                    "could not create a reorder primitive descriptor");
            reset(result);
        }

        primitive_desc(const memory &src, const memory &dst,
                const primitive_attr &aattr = primitive_attr()) {
            dnnl_primitive_desc_t result;
            auto src_md = src.get_desc();
            auto dst_md = dst.get_desc();
            error::wrap_c_api(
                    dnnl_reorder_primitive_desc_create(&result, &src_md.data,
                            src.get_engine().get(), &dst_md.data,
                            dst.get_engine().get(), aattr.get()),
                    "could not create a reorder primitive descriptor");
            reset(result);
        }

        /// Initializes a primitive descriptor for reorder from a C primitive
        /// descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : primitive_desc_base(pd, dnnl::primitive::kind::reorder) {}

        engine get_src_engine() const {
            return engine::query(*this, dnnl::query::reorder_src_engine);
        }

        engine get_dst_engine() const {
            return engine::query(*this, dnnl::query::reorder_dst_engine);
        }
    };

    reorder() = default;

    reorder(const primitive_desc &pd) : primitive(pd.get()) {}

    reorder(const memory &src, const memory &dst)
        : primitive(primitive_desc(src, dst).get()) {}

    using primitive::execute;

    void execute(stream astream, memory &src, memory &dst) {
        primitive::execute(astream, {{DNNL_ARG_FROM, src}, {DNNL_ARG_TO, dst}});
    }
};

/// @}

/// @addtogroup cpp_api_concat Concat
/// A primitive to concatenate data by arbitrary dimension.
///
/// @sa @ref dev_guide_concat in developer guide
/// @sa @ref c_api_concat in @ref c_api
/// @{

/// @cond DO_NOT_DOCUMENT_THIS
inline std::vector<dnnl_memory_desc_t> convert_to_c(
        const std::vector<memory::desc> &mems) {
    std::vector<dnnl_memory_desc_t> c_api_mems;
    c_api_mems.reserve(mems.size());
    for (const auto &s : mems)
        c_api_mems.push_back(s.data);
    return c_api_mems;
}
/// @endcond

/// Implements primitive descriptor and primitive for concat.
///
/// Creates an out-of-place primitive descriptor for concatenation of @p n
/// inputs by @p concat_dimension with resulting @p output_desc memory
/// descriptor. @p output_desc can be NULL or specified with the
/// #dnnl::memory::format_tag::any format kind--in this case, the appropriate memory
/// format would be chosen automatically.
struct concat : public primitive {
    struct primitive_desc : public primitive_desc_base {
        using primitive_desc_base::primitive_desc_base;

        primitive_desc(const memory::desc &dst, int concat_dimension,
                const std::vector<memory::desc> &srcs, const engine &aengine,
                const primitive_attr &aattr = primitive_attr()) {
            auto c_api_srcs = convert_to_c(srcs);

            dnnl_primitive_desc_t result;
            error::wrap_c_api(
                    dnnl_concat_primitive_desc_create(&result, &dst.data,
                            (int)c_api_srcs.size(), concat_dimension,
                            &c_api_srcs[0], aattr.get(), aengine.get()),
                    "could not create a concat primitive descriptor");
            reset(result);
        }

        primitive_desc(int concat_dimension,
                const std::vector<memory::desc> &srcs, const engine &aengine,
                const primitive_attr &aattr = primitive_attr()) {
            auto c_api_srcs = convert_to_c(srcs);

            dnnl_primitive_desc_t result;
            error::wrap_c_api(
                    dnnl_concat_primitive_desc_create(&result, nullptr,
                            (int)c_api_srcs.size(), concat_dimension,
                            &c_api_srcs[0], aattr.get(), aengine.get()),
                    "could not create a concat primitive descriptor");
            reset(result);
        }

        /// Initializes a primitive descriptor for concat from a C primitive
        /// descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : primitive_desc_base(pd, dnnl::primitive::kind::concat) {}

        /// Queries destination memory descriptor.
        memory::desc dst_desc() const { return query_md(query::dst_md, 0); }
    };

    concat() = default;

    concat(const primitive_desc &pd) : primitive(pd.get()) {}
};

/// @}

/// @addtogroup cpp_api_sum Sum
/// A primitive to sum data.
///
/// @sa @ref dev_guide_sum in developer guide
/// @sa @ref c_api_sum in @ref c_api
/// @{

/// Creates an out-of-place sum primitive descriptor for sum of @p n inputs
/// multiplied by the scale with resulting @p output_desc memory descriptor.
/// @p output_desc can be NULL or specified with the
/// #dnnl::memory::format_tag::any format kind--in this case, the
/// appropriate memory format would be chosen automatically.
struct sum : public primitive {
    struct primitive_desc : public primitive_desc_base {
        using primitive_desc_base::primitive_desc_base;

        primitive_desc() = default;

        primitive_desc(const memory::desc &dst,
                const std::vector<float> &scales,
                const std::vector<memory::desc> &srcs, const engine &aengine,
                const primitive_attr &aattr = primitive_attr()) {
            error::wrap_c_api(scales.size() == srcs.size()
                            ? dnnl_success
                            : dnnl_invalid_arguments,
                    "number of scales not equal to number of srcs");

            auto c_api_srcs = convert_to_c(srcs);

            dnnl_primitive_desc_t result;
            error::wrap_c_api(
                    dnnl_sum_primitive_desc_create(&result, &dst.data,
                            (int)c_api_srcs.size(), &scales[0], &c_api_srcs[0],
                            aattr.get(), aengine.get()),
                    "could not create a sum primitive descriptor");
            reset(result);
        }

        primitive_desc(const std::vector<float> &scales,
                const std::vector<memory::desc> &srcs, const engine &aengine,
                const primitive_attr &aattr = primitive_attr()) {
            error::wrap_c_api(scales.size() == srcs.size()
                            ? dnnl_success
                            : dnnl_invalid_arguments,
                    "number of scales not equal to number of srcs");

            auto c_api_srcs = convert_to_c(srcs);
            dnnl_primitive_desc_t result;
            error::wrap_c_api(
                    dnnl_sum_primitive_desc_create(&result, nullptr,
                            (int)c_api_srcs.size(), &scales[0], &c_api_srcs[0],
                            aattr.get(), aengine.get()),
                    "could not create a sum primitive descriptor");
            reset(result);
        }

        /// Initializes a primitive descriptor for sum from a C primitive
        /// descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : primitive_desc_base(pd, dnnl::primitive::kind::sum) {}

        /// Queries destination memory descriptor.
        memory::desc dst_desc() const { return query_md(query::dst_md, 0); }
    };

    sum() = default;

    sum(const primitive_desc &pd) : primitive(pd.get()) {}
};

/// @}

/// @}

/// @addtogroup cpp_api_primitives Primitives
/// @{

/// @addtogroup cpp_api_primitive_descriptors Primitive descriptors
/// @{

/// A base class for descriptors of all primitives that have an operation
/// descriptor and that support iteration over multiple implementations.
struct primitive_desc : public primitive_desc_base {
    using primitive_desc_base::primitive_desc_base;

    primitive_desc() = default;

    /// Creates a primitive descriptor from given @p op_desc, @p attr, @p
    /// engine, and optionally a hint primitive descriptor from forward
    /// propagation. If allow_empty is true, the constructor does not throw if
    /// a primitive_desc cannot be created. But calling next_impl() in this
    /// case *will* throw.
    primitive_desc(const_dnnl_op_desc_t desc, const primitive_attr *attr,
            const engine &e, const_dnnl_primitive_desc_t hint_fwd_pd,
            bool allow_empty = false)
        : allow_empty(allow_empty) {
        dnnl_primitive_desc_iterator_t iterator = nullptr;
        dnnl_status_t status = dnnl_primitive_desc_iterator_create(&iterator,
                desc, attr ? attr->get() : nullptr, e.get(), hint_fwd_pd);
        if (!allow_empty)
            error::wrap_c_api(
                    status, "could not create a primitive descriptor iterator");
        pd_iterator.reset(iterator);
        fetch_impl();
    }

    /// Advances the next implementation for the given op descriptor.
    ///
    /// Returns:
    /// - @c true on success
    /// - @c false if the last implementation reached, and
    ///   the primitive descriptor itself is kept unchanged
    bool next_impl() {
        dnnl_status_t status
                = dnnl_primitive_desc_iterator_next(pd_iterator.get());
        if (status == dnnl_iterator_ends) return false;
        error::wrap_c_api(status, "primitive descriptor iterator next failed");

        fetch_impl();
        return true;
    }

private:
    bool allow_empty = false;
    handle<dnnl_primitive_desc_iterator_t> pd_iterator;
    void fetch_impl() {
        dnnl_primitive_desc_t pd = dnnl_primitive_desc_iterator_fetch(
                pd_iterator.get(allow_empty));
        error::wrap_c_api(pd != nullptr || allow_empty ? dnnl_success
                                                       : dnnl_runtime_error,
                "could not fetch a primitive descriptor from the iterator");
        reset(pd);
    }
};

/// @}

/// @addtogroup cpp_api_convolution Convolution
/// Computes a forward propagation, backward propagation, or weight update
/// for convolution operation with bias on a batch of multi-dimensional tensors.
///
/// @sa @ref dev_guide_convolution in developer guide
/// @sa @ref c_api_convolution in @ref c_api
/// @{

/// Convolution forward propagation.
///
/// Implements descriptor, primitive descriptor, and primitive
/// for the convolution forward propagation.
struct convolution_forward : public primitive {

    /// Descriptor for convolution forward propagation.
    struct desc {
        dnnl_convolution_desc_t data;

        /// Initializes a descriptor for convolution forward propagation without
        /// bias using @p aprop_kind (possible values are
        /// #dnnl::forward_training and #dnnl::forward_inference),
        /// @p aalgorithm, memory descriptors, @p strides, @p padding_l, and
        /// @p padding_r.
        ///
        /// @note Memory descriptors are allowed to be initialized with
        ///       #dnnl::memory::format_tag::any value of @p format_kind.
        desc(prop_kind aprop_kind, algorithm aalgorithm,
                const memory::desc &src_desc, const memory::desc &weights_desc,
                const memory::desc &bias_desc, const memory::desc &dst_desc,
                const memory::dims &strides, const memory::dims &padding_l,
                const memory::dims &padding_r) {
            memory::validate_dims(strides);
            memory::validate_dims(padding_l);
            memory::validate_dims(padding_r);
            error::wrap_c_api(
                    dnnl_convolution_forward_desc_init(&data,
                            dnnl::convert_to_c(aprop_kind),
                            convert_to_c(aalgorithm), &src_desc.data,
                            &weights_desc.data, &bias_desc.data, &dst_desc.data,
                            &strides[0], &padding_l[0], &padding_r[0]),
                    "could not create a convolution forward descriptor");
        }

        /// Initializes a descriptor for convolution forward propagation with
        /// bias using @p prop_kind (possible values are
        /// #dnnl::forward_training and #dnnl::forward_inference), @p
        /// aalgorithm, memory descriptors, @p strides, @p padding_l, and @p
        /// padding_r.
        ///
        /// @note Memory descriptors are allowed to be initialized with
        ///       #dnnl::memory::format_tag::any value of @p format_kind.
        desc(prop_kind aprop_kind, algorithm aalgorithm,
                const memory::desc &src_desc, const memory::desc &weights_desc,
                const memory::desc &dst_desc, const memory::dims &strides,
                const memory::dims &padding_l, const memory::dims &padding_r) {
            memory::validate_dims(strides);
            memory::validate_dims(padding_l);
            memory::validate_dims(padding_r);
            error::wrap_c_api(
                    dnnl_convolution_forward_desc_init(&data,
                            dnnl::convert_to_c(aprop_kind),
                            convert_to_c(aalgorithm), &src_desc.data,
                            &weights_desc.data, nullptr, &dst_desc.data,
                            &strides[0], &padding_l[0], &padding_r[0]),
                    "could not create a convolution forward descriptor");
        }

        /// Initializes a descriptor for dilated convolution forward propagation
        /// without bias using @p prop_kind (possible values are
        /// #dnnl::forward_training and #dnnl::forward_inference),
        /// @p aalgorithm, memory descriptors, @p strides, @p dilates,
        /// @p padding_l, and @p padding_r.
        ///
        /// @note Memory descriptors are allowed to be initialized with
        ///       #dnnl::memory::format_tag::any value of @p format_kind.
        desc(prop_kind aprop_kind, algorithm aalgorithm,
                const memory::desc &src_desc, const memory::desc &weights_desc,
                const memory::desc &bias_desc, const memory::desc &dst_desc,
                const memory::dims &strides, const memory::dims &dilates,
                const memory::dims &padding_l, const memory::dims &padding_r) {
            memory::validate_dims(strides);
            memory::validate_dims(dilates);
            memory::validate_dims(padding_l);
            memory::validate_dims(padding_r);
            error::wrap_c_api(dnnl_dilated_convolution_forward_desc_init(&data,
                                      dnnl::convert_to_c(aprop_kind),
                                      convert_to_c(aalgorithm), &src_desc.data,
                                      &weights_desc.data, &bias_desc.data,
                                      &dst_desc.data, &strides[0], &dilates[0],
                                      &padding_l[0], &padding_r[0]),
                    "could not create a dilated convolution forward "
                    "descriptor");
        }

        /// Initializes a descriptor for dilated convolution forward propagation
        /// with bias using @p prop_kind (possible values are
        /// #dnnl::forward_training and #dnnl::forward_inference),
        /// @p aalgorithm, memory descriptors, @p strides, @p dilates,
        /// @p padding_l, and @p padding_r.
        ///
        /// @note Memory descriptors are allowed to be initialized with
        ///       #dnnl::memory::format_tag::any value of @p format_kind.
        desc(prop_kind aprop_kind, algorithm aalgorithm,
                const memory::desc &src_desc, const memory::desc &weights_desc,
                const memory::desc &dst_desc, const memory::dims &strides,
                const memory::dims &dilates, const memory::dims &padding_l,
                const memory::dims &padding_r) {
            memory::validate_dims(strides);
            memory::validate_dims(dilates);
            memory::validate_dims(padding_l);
            memory::validate_dims(padding_r);
            error::wrap_c_api(dnnl_dilated_convolution_forward_desc_init(&data,
                                      dnnl::convert_to_c(aprop_kind),
                                      convert_to_c(aalgorithm), &src_desc.data,
                                      &weights_desc.data, nullptr,
                                      &dst_desc.data, &strides[0], &dilates[0],
                                      &padding_l[0], &padding_r[0]),
                    "could not create a dilated convolution forward "
                    "descriptor");
        }
    };

    /// Primitive descriptor for convolution forward propagation.
    struct primitive_desc : public dnnl::primitive_desc {
        primitive_desc() = default;

        /// Initializes a primitive descriptor for convolution forward
        /// propagation.
        primitive_desc(
                const desc &desc, const engine &e, bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, nullptr, e, nullptr, allow_empty) {}

        /// Initializes a primitive descriptor for convolution forward
        /// propagation with attributes defined by @p attr.
        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e, bool allow_empty = false)
            : dnnl::primitive_desc(&desc.data, &attr, e, nullptr, allow_empty) {
        }

        /// Initializes a primitive descriptor for convolution forward
        /// propagation from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : dnnl::primitive_desc(pd, dnnl::primitive::kind::convolution,
                    dnnl::prop_kind::forward_training,
                    dnnl::prop_kind::forward_inference) {}

        /// Queries source memory descriptor.
        memory::desc src_desc() const { return query_md(query::src_md, 0); }

        /// Queries weights memory descriptor.
        memory::desc weights_desc() const {
            return query_md(query::weights_md, 0);
        }

        /// Queries bias memory descriptor.
        ///
        /// Returns a zero_md if no bias was specified at op_desc
        /// creation time.
        memory::desc bias_desc() const {
            return query_md(query::weights_md, 1);
        }

        /// Queries destination memory descriptor.
        memory::desc dst_desc() const { return query_md(query::dst_md, 0); }
    };

    convolution_forward() = default;

    /// Creates a convolution forward propagation primitive from the
    /// corresponding primitive descriptor.
    convolution_forward(const primitive_desc &pd) : primitive(pd) {}
};

/// Convolution backward propagation.
///
/// Implements descriptor, primitive descriptor, and primitive for the
/// convolution backward propagation.
struct convolution_backward_data : public primitive {

    /// Descriptor for convolution backward propagation.
    struct desc {
        dnnl_convolution_desc_t data;

        /// Initializes a descriptor for convolution backward propagation
        /// using @p aalgorithm, memory descriptors, @p strides, @p
        /// padding_l, and @p padding_r.
        ///
        /// @note Memory descriptors are allowed to be initialized with
        ///       #dnnl::memory::format_tag::any value of @p format_kind.
        desc(algorithm aalgorithm, const memory::desc &diff_src_desc,
                const memory::desc &weights_desc,
                const memory::desc &diff_dst_desc, const memory::dims &strides,
                const memory::dims &padding_l, const memory::dims &padding_r) {
            memory::validate_dims(strides);
            memory::validate_dims(padding_l);
            memory::validate_dims(padding_r);
            error::wrap_c_api(
                    dnnl_convolution_backward_data_desc_init(&data,
                            convert_to_c(aalgorithm), &diff_src_desc.data,
                            &weights_desc.data, &diff_dst_desc.data,
                            &strides[0], &padding_l[0], &padding_r[0]),
                    "could not create a convolution backward data descriptor");
        }

        /// Initializes a descriptor for dilated convolution backward
        /// propagation using @p aalgorithm, memory descriptors, @p strides, @p
        /// padding_l, and @p padding_r.
        ///
        /// @note Memory descriptors are allowed to be initialized with
        ///       #dnnl::memory::format_tag::any value of @p format_kind.
        desc(algorithm aalgorithm, const memory::desc &diff_src_desc,
                const memory::desc &weights_desc,
                const memory::desc &diff_dst_desc, const memory::dims &strides,
                const memory::dims &dilates, const memory::dims &padding_l,
                const memory::dims &padding_r) {
            memory::validate_dims(strides);
            memory::validate_dims(dilates);
            memory::validate_dims(padding_l);
            memory::validate_dims(padding_r);
            error::wrap_c_api(
                    dnnl_dilated_convolution_backward_data_desc_init(&data,
                            convert_to_c(aalgorithm), &diff_src_desc.data,
                            &weights_desc.data, &diff_dst_desc.data,
                            &strides[0], &dilates[0], &padding_l[0],
                            &padding_r[0]),
                    "could not create a convolution backward data descriptor");
        }
    };

    /// Primitive descriptor for convolution backward propagation.
    struct primitive_desc : public dnnl::primitive_desc {
        primitive_desc() = default;

        /// Initializes primitive descriptor for convolution backward
        /// propagation.
        primitive_desc(const desc &desc, const engine &e,
                const convolution_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, nullptr, e, hint_fwd_pd.get(), allow_empty) {}

        /// Initializes primitive descriptor for convolution backward
        /// propagation with attributes defined by @p attr.
        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e,
                const convolution_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, &attr, e, hint_fwd_pd.get(), allow_empty) {}

        /// Initializes a primitive descriptor for convolution backward
        /// propagation from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : dnnl::primitive_desc(pd, dnnl::primitive::kind::convolution,
                    dnnl::prop_kind::backward_data) {}

        /// Queries diff source gradient memory descriptor.
        memory::desc diff_src_desc() const {
            return query_md(query::diff_src_md, 0);
        }

        /// Queries weights memory descriptor.
        memory::desc weights_desc() const {
            return query_md(query::weights_md, 0);
        }

        /// Queries diff destination memory descriptor.
        memory::desc diff_dst_desc() const {
            return query_md(query::diff_dst_md, 0);
        }
    };

    convolution_backward_data() = default;

    /// Creates a convolution backward propagation primitive from the
    /// corresponding primitive descriptor.
    convolution_backward_data(const primitive_desc &pd) : primitive(pd) {}
};

/// Convolution weight update.
///
/// Implements descriptor, primitive descriptor, and primitive for the
/// convolution weight update.
struct convolution_backward_weights : public primitive {

    /// Descriptor for convolution weight update.
    struct desc {
        dnnl_convolution_desc_t data;

        /// Initializes a descriptor for convolution weight update with bias
        /// using @p aalgorithm, memory descriptors, @p strides, @p padding_l,
        /// and @p padding_r.
        ///
        /// @note Memory descriptors are allowed to be initialized with
        ///       #dnnl::memory::format_tag::any value of @p format_kind.
        desc(algorithm aalgorithm, const memory::desc &src_desc,
                const memory::desc &diff_weights_desc,
                const memory::desc &diff_bias_desc,
                const memory::desc &diff_dst_desc, const memory::dims &strides,
                const memory::dims &padding_l, const memory::dims &padding_r) {
            memory::validate_dims(strides);
            memory::validate_dims(padding_l);
            memory::validate_dims(padding_r);
            error::wrap_c_api(
                    dnnl_convolution_backward_weights_desc_init(&data,
                            convert_to_c(aalgorithm), &src_desc.data,
                            &diff_weights_desc.data, &diff_bias_desc.data,
                            &diff_dst_desc.data, &strides[0], &padding_l[0],
                            &padding_r[0]),
                    "could not create a convolution backward weights "
                    "descriptor");
        }

        /// Initializes a descriptor for convolution weight update without
        /// bias using @p aalgorithm, memory descriptors, @p strides, @p
        /// padding_l, and @p padding_r.
        ///
        /// @note Memory descriptors are allowed to be initialized with
        ///       #dnnl::memory::format_tag::any value of @p format_kind.
        desc(algorithm aalgorithm, const memory::desc &src_desc,
                const memory::desc &diff_weights_desc,
                const memory::desc &diff_dst_desc, const memory::dims &strides,
                const memory::dims &padding_l, const memory::dims &padding_r) {
            memory::validate_dims(strides);
            memory::validate_dims(padding_l);
            memory::validate_dims(padding_r);
            error::wrap_c_api(dnnl_convolution_backward_weights_desc_init(&data,
                                      convert_to_c(aalgorithm), &src_desc.data,
                                      &diff_weights_desc.data, nullptr,
                                      &diff_dst_desc.data, &strides[0],
                                      &padding_l[0], &padding_r[0]),
                    "could not create a convolution backward weights "
                    "descriptor");
        }

        /// Initializes a descriptor for dilated convolution weight update
        /// with bias using @p aalgorithm, memory descriptors, @p strides,
        /// @p dilates @p padding_l, and @p padding_r.
        ///
        /// @note Memory descriptors are allowed to be initialized with
        ///       #dnnl::memory::format_tag::any value of @p format_kind.
        desc(algorithm aalgorithm, const memory::desc &src_desc,
                const memory::desc &diff_weights_desc,
                const memory::desc &diff_bias_desc,
                const memory::desc &diff_dst_desc, const memory::dims &strides,
                const memory::dims &dilates, const memory::dims &padding_l,
                const memory::dims &padding_r) {
            memory::validate_dims(strides);
            memory::validate_dims(dilates);
            memory::validate_dims(padding_l);
            memory::validate_dims(padding_r);
            error::wrap_c_api(
                    dnnl_dilated_convolution_backward_weights_desc_init(&data,
                            convert_to_c(aalgorithm), &src_desc.data,
                            &diff_weights_desc.data, &diff_bias_desc.data,
                            &diff_dst_desc.data, &strides[0], &dilates[0],
                            &padding_l[0], &padding_r[0]),
                    "could not create a convolution backward weights "
                    "descriptor");
        }

        /// Initializes a descriptor for dilated convolution weight update
        /// without bias using @p aalgorithm, memory descriptors, @p strides,
        /// @p dilates @p padding_l, and @p padding_r.
        ///
        /// @note Memory descriptors are allowed to be initialized with
        ///       #dnnl::memory::format_tag::any value of @p format_kind.
        desc(algorithm aalgorithm, const memory::desc &src_desc,
                const memory::desc &diff_weights_desc,
                const memory::desc &diff_dst_desc, const memory::dims &strides,
                const memory::dims &dilates, const memory::dims &padding_l,
                const memory::dims &padding_r) {
            memory::validate_dims(strides);
            memory::validate_dims(dilates);
            memory::validate_dims(padding_l);
            memory::validate_dims(padding_r);
            error::wrap_c_api(
                    dnnl_dilated_convolution_backward_weights_desc_init(&data,
                            convert_to_c(aalgorithm), &src_desc.data,
                            &diff_weights_desc.data, nullptr,
                            &diff_dst_desc.data, &strides[0], &dilates[0],
                            &padding_l[0], &padding_r[0]),
                    "could not create a convolution backward weights "
                    "descriptor");
        }
    };

    /// Primitive descriptor for convolution weight update.
    struct primitive_desc : public dnnl::primitive_desc {
        primitive_desc() = default;

        /// Initializes a primitive descriptor for convolution weight update.
        primitive_desc(const desc &desc, const engine &e,
                const convolution_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, nullptr, e, hint_fwd_pd.get(), allow_empty) {}

        /// Initializes a primitive descriptor for convolution weight update
        /// with attributes defined by @p attr.
        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e,
                const convolution_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, &attr, e, hint_fwd_pd.get(), allow_empty) {}

        /// Initializes a primitive descriptor for convolution weights
        /// update from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : dnnl::primitive_desc(pd, dnnl::primitive::kind::convolution,
                    dnnl::prop_kind::backward_weights) {}

        /// Queries source memory descriptor.
        memory::desc src_desc() const { return query_md(query::src_md, 0); }

        /// Queries diff weights memory descriptor.
        memory::desc diff_weights_desc() const {
            return query_md(query::diff_weights_md, 0);
        }

        /// Queries diff bias memory descriptor.
        memory::desc diff_bias_desc() const {
            return query_md(query::diff_weights_md, 1);
        }

        /// Queries diff destination memory descriptor.
        memory::desc diff_dst_desc() const {
            return query_md(query::diff_dst_md, 0);
        }
    };

    convolution_backward_weights() = default;

    /// Creates convolution weight update primitive from corresponding
    /// primitive descriptor.
    convolution_backward_weights(const primitive_desc &pd) : primitive(pd) {}
};

/// @}
//
/// @addtogroup cpp_api_deconvolution Deconvolution
/// A primitive to compute deconvolution using different algorithms.
///
/// @sa @ref c_api_deconvolution in @ref c_api
/// @{

/// Deconvolution forward propagation.
///
/// Implements descriptor, primitive descriptor, and primitive
/// for the deconvolution forward propagation.
struct deconvolution_forward : public primitive {

    /// Descriptor for convolution forward propagation.
    struct desc {
        dnnl_deconvolution_desc_t data;

        /// Initializes a descriptor for deconvolution forward propagation
        /// with bias using @p prop_kind (possible values are
        /// #dnnl::forward_training and #dnnl::forward_inference), @p
        /// aalgorithm, memory descriptors, @p strides, @p padding_l, and @p
        /// padding_r.
        ///
        /// @note Memory descriptors are allowed to be initialized with
        ///       #dnnl::memory::format_tag::any value of @p format_kind.
        desc(prop_kind aprop_kind, algorithm aalgorithm,
                const memory::desc &src_desc, const memory::desc &weights_desc,
                const memory::desc &bias_desc, const memory::desc &dst_desc,
                const memory::dims &strides, const memory::dims &padding_l,
                const memory::dims &padding_r) {
            memory::validate_dims(strides);
            memory::validate_dims(padding_l);
            memory::validate_dims(padding_r);
            error::wrap_c_api(
                    dnnl_deconvolution_forward_desc_init(&data,
                            dnnl::convert_to_c(aprop_kind),
                            convert_to_c(aalgorithm), &src_desc.data,
                            &weights_desc.data, &bias_desc.data, &dst_desc.data,
                            &strides[0], &padding_l[0], &padding_r[0]),
                    "could not create a deconvolution forward descriptor");
        }

        /// Initializes a descriptor for deconvolution forward propagation
        /// without bias using @p prop_kind (possible values are
        /// #dnnl::forward_training and #dnnl::forward_inference), @p
        /// aalgorithm, memory descriptors, @p strides, @p padding_l, and @p
        /// padding_r.
        ///
        /// @note Memory descriptors are allowed to be initialized with
        ///       #dnnl::memory::format_tag::any value of @p format_kind.
        desc(prop_kind aprop_kind, algorithm aalgorithm,
                const memory::desc &src_desc, const memory::desc &weights_desc,
                const memory::desc &dst_desc, const memory::dims &strides,
                const memory::dims &padding_l, const memory::dims &padding_r) {
            memory::validate_dims(strides);
            memory::validate_dims(padding_l);
            memory::validate_dims(padding_r);
            error::wrap_c_api(
                    dnnl_deconvolution_forward_desc_init(&data,
                            dnnl::convert_to_c(aprop_kind),
                            convert_to_c(aalgorithm), &src_desc.data,
                            &weights_desc.data, nullptr, &dst_desc.data,
                            &strides[0], &padding_l[0], &padding_r[0]),
                    "could not create a deconvolution forward descriptor");
        }

        /// Initializes a descriptor for dilated deconvolution forward
        /// propagation with bias using @p aprop_kind (possible values are
        /// #dnnl::forward_training and #dnnl::forward_inference), @p
        /// aalgorithm memory descriptors, @p strides, @p dilates, @p
        /// padding_l, and @p padding_r.
        ///
        /// @note Memory descriptors are allowed to be initialized with
        ///       #dnnl::memory::format_tag::any value of @p format_kind.
        desc(prop_kind aprop_kind, algorithm aalgorithm,
                const memory::desc &src_desc, const memory::desc &weights_desc,
                const memory::desc &bias_desc, const memory::desc &dst_desc,
                const memory::dims &strides, const memory::dims &dilates,
                const memory::dims &padding_l, const memory::dims &padding_r) {
            memory::validate_dims(strides);
            memory::validate_dims(dilates);
            memory::validate_dims(padding_l);
            memory::validate_dims(padding_r);
            error::wrap_c_api(dnnl_dilated_deconvolution_forward_desc_init(
                                      &data, dnnl::convert_to_c(aprop_kind),
                                      convert_to_c(aalgorithm), &src_desc.data,
                                      &weights_desc.data, &bias_desc.data,
                                      &dst_desc.data, &strides[0], &dilates[0],
                                      &padding_l[0], &padding_r[0]),
                    "could not create a dilated deconvolution forward "
                    "descriptor");
        }

        /// Initializes a descriptor for dilated deconvolution forward
        /// propagation without bias using @p aprop_kind (possible values are
        /// #dnnl::forward_training and #dnnl::forward_inference), @p
        /// aalgorithm, memory descriptors, @p strides, @p dilates, @p
        /// padding_l, and @p padding_r.
        ///
        /// @note Memory descriptors are allowed to be initialized with
        ///       #dnnl::memory::format_tag::any value of @p format_kind.
        desc(prop_kind aprop_kind, algorithm aalgorithm,
                const memory::desc &src_desc, const memory::desc &weights_desc,
                const memory::desc &dst_desc, const memory::dims &strides,
                const memory::dims &dilates, const memory::dims &padding_l,
                const memory::dims &padding_r) {
            memory::validate_dims(strides);
            memory::validate_dims(dilates);
            memory::validate_dims(padding_l);
            memory::validate_dims(padding_r);
            error::wrap_c_api(dnnl_dilated_deconvolution_forward_desc_init(
                                      &data, dnnl::convert_to_c(aprop_kind),
                                      convert_to_c(aalgorithm), &src_desc.data,
                                      &weights_desc.data, nullptr,
                                      &dst_desc.data, &strides[0], &dilates[0],
                                      &padding_l[0], &padding_r[0]),
                    "could not create a dilated deconvolution forward "
                    "descriptor");
        }
    };

    /// Primitive descriptor for deconvolution forward propagation.
    struct primitive_desc : public dnnl::primitive_desc {
        primitive_desc() = default;

        /// Initializes a primitive descriptor for deconvolution forward
        /// propagation.
        primitive_desc(
                const desc &desc, const engine &e, bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, nullptr, e, nullptr, allow_empty) {}

        /// Initializes primitive descriptor for deconvolution forward
        /// propagation with attributes defined by @p attr.
        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e, bool allow_empty = false)
            : dnnl::primitive_desc(&desc.data, &attr, e, nullptr, allow_empty) {
        }

        /// Initializes a primitive descriptor for deconvolution forward
        /// propagation from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : dnnl::primitive_desc(pd, dnnl::primitive::kind::deconvolution,
                    dnnl::prop_kind::forward_training,
                    dnnl::prop_kind::forward_inference) {}

        /// Queries source memory descriptor.
        memory::desc src_desc() const { return query_md(query::src_md, 0); }

        /// Queries weights memory descriptor.
        memory::desc weights_desc() const {
            return query_md(query::weights_md, 0);
        }

        /// Queries bias memory descriptor.
        ///
        /// Returns a zero_md if no bias was specified at op_desc
        /// creation time.
        memory::desc bias_desc() const {
            return query_md(query::weights_md, 1);
        }

        /// Queries destination memory descriptor.
        memory::desc dst_desc() const { return query_md(query::dst_md, 0); }
    };

    deconvolution_forward() = default;

    /// Creates a deconvolution forward propagation primitive from the
    /// corresponding primitive descriptor.
    deconvolution_forward(const primitive_desc &pd) : primitive(pd) {}
};

/// Deconvolution backward propagation.
///
/// Implements descriptor, primitive descriptor, and primitive for the
/// deconvolution backward propagation.
struct deconvolution_backward_data : public primitive {

    /// Descriptor for deconvolution backward propagation.
    struct desc {
        dnnl_deconvolution_desc_t data;

        /// Initializes a descriptor for deconvolution backward propagation
        /// using @p aalgorithm, memory descriptors, @p strides, @p
        /// padding_l, and @p padding_r.
        ///
        /// @note Memory descriptors are allowed to be initialized with
        ///       #dnnl::memory::format_tag::any value of @p format_kind.
        desc(algorithm aalgorithm, const memory::desc &diff_src_desc,
                const memory::desc &weights_desc,
                const memory::desc &diff_dst_desc, const memory::dims &strides,
                const memory::dims &padding_l, const memory::dims &padding_r) {
            memory::validate_dims(strides);
            memory::validate_dims(padding_l);
            memory::validate_dims(padding_r);
            error::wrap_c_api(
                    dnnl_deconvolution_backward_data_desc_init(&data,
                            convert_to_c(aalgorithm), &diff_src_desc.data,
                            &weights_desc.data, &diff_dst_desc.data,
                            &strides[0], &padding_l[0], &padding_r[0]),
                    "could not create a deconvolution backward data "
                    "descriptor");
        }

        /// Initializes descriptor for dilated deconvolution backward propagation
        /// using @p aalgorithm, memory descriptors, @p strides, @p
        /// padding_l, and @p padding_r.
        ///
        /// @note Memory descriptors are allowed to be initialized with
        ///       #dnnl::memory::format_tag::any value of @p format_kind.
        desc(algorithm aalgorithm, const memory::desc &diff_src_desc,
                const memory::desc &weights_desc,
                const memory::desc &diff_dst_desc, const memory::dims &strides,
                const memory::dims &dilates, const memory::dims &padding_l,
                const memory::dims &padding_r) {
            memory::validate_dims(strides);
            memory::validate_dims(dilates);
            memory::validate_dims(padding_l);
            memory::validate_dims(padding_r);
            error::wrap_c_api(
                    dnnl_dilated_deconvolution_backward_data_desc_init(&data,
                            convert_to_c(aalgorithm), &diff_src_desc.data,
                            &weights_desc.data, &diff_dst_desc.data,
                            &strides[0], &dilates[0], &padding_l[0],
                            &padding_r[0]),
                    "could not create a dilated deconvolution backward data "
                    "descriptor");
        }
    };

    /// Primitive descriptor for deconvolution backward propagation.
    struct primitive_desc : public dnnl::primitive_desc {
        primitive_desc() = default;

        /// Initializes a primitive descriptor for deconvolution backward
        /// propagation.
        primitive_desc(const desc &desc, const engine &e,
                const deconvolution_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, nullptr, e, hint_fwd_pd.get(), allow_empty) {}

        /// Initializes a primitive descriptor for deconvolution backward
        /// propagation with attributes defined by @p attr.
        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e,
                const deconvolution_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, &attr, e, hint_fwd_pd.get(), allow_empty) {}

        /// Initializes a primitive descriptor for deconvolution backward
        /// propagation from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : dnnl::primitive_desc(pd, dnnl::primitive::kind::deconvolution,
                    dnnl::prop_kind::backward_data) {}

        /// Queries diff source gradient memory descriptor.
        memory::desc diff_src_desc() const {
            return query_md(query::diff_src_md, 0);
        }

        /// Queries weights memory descriptor.
        memory::desc weights_desc() const {
            return query_md(query::weights_md, 0);
        }

        /// Queries diff destination memory descriptor.
        memory::desc diff_dst_desc() const {
            return query_md(query::diff_dst_md, 0);
        }
    };

    deconvolution_backward_data() = default;

    /// Creates a deconvolution backward propagation primitive from the
    /// corresponding primitive descriptor.
    deconvolution_backward_data(const primitive_desc &pd) : primitive(pd) {}
};

/// Deconvolution weight update.
///
/// Implements descriptor, primitive descriptor, and primitive
/// for the deconvolution weight update.
struct deconvolution_backward_weights : public primitive {

    /// Descriptor for deconvolution weight update.
    struct desc {
        dnnl_deconvolution_desc_t data;

        /// Initializes a descriptor for deconvolution weight update with bias
        /// using @p aalgorithm, memory descriptors, @p strides, @p padding_l,
        /// and @p padding_r.
        ///
        /// @note Memory descriptors are allowed to be initialized with
        ///       #dnnl::memory::format_tag::any value of @p format_kind.
        desc(algorithm aalgorithm, const memory::desc &src_desc,
                const memory::desc &diff_weights_desc,
                const memory::desc &diff_bias_desc,
                const memory::desc &diff_dst_desc, const memory::dims &strides,
                const memory::dims &padding_l, const memory::dims &padding_r) {
            memory::validate_dims(strides);
            memory::validate_dims(padding_l);
            memory::validate_dims(padding_r);
            error::wrap_c_api(
                    dnnl_deconvolution_backward_weights_desc_init(&data,
                            convert_to_c(aalgorithm), &src_desc.data,
                            &diff_weights_desc.data, &diff_bias_desc.data,
                            &diff_dst_desc.data, &strides[0], &padding_l[0],
                            &padding_r[0]),
                    "could not create a deconvolution backward weights "
                    "descriptor");
        }

        /// Initializes a descriptor for deconvolution weight update without
        /// bias using @p aalgorithm, memory descriptors, @p strides, @p
        /// padding_l, and @p padding_r.
        ///
        /// @note Memory descriptors are allowed to be initialized with
        ///       #dnnl::memory::format_tag::any value of @p format_kind.
        desc(algorithm aalgorithm, const memory::desc &src_desc,
                const memory::desc &diff_weights_desc,
                const memory::desc &diff_dst_desc, const memory::dims &strides,
                const memory::dims &padding_l, const memory::dims &padding_r) {
            memory::validate_dims(strides);
            memory::validate_dims(padding_l);
            memory::validate_dims(padding_r);
            error::wrap_c_api(dnnl_deconvolution_backward_weights_desc_init(
                                      &data, convert_to_c(aalgorithm),
                                      &src_desc.data, &diff_weights_desc.data,
                                      nullptr, &diff_dst_desc.data, &strides[0],
                                      &padding_l[0], &padding_r[0]),
                    "could not create a deconvolution backward weights "
                    "descriptor");
        }

        /// Initializes a descriptor for dilated deconvolution weight update
        /// with bias using @p aalgorithm, memory descriptors, @p strides, @p
        /// dilates @p padding_l, and @p padding_r.
        ///
        /// @note Memory descriptors are allowed to be initialized with
        ///       #dnnl::memory::format_tag::any value of @p format_kind.
        desc(algorithm aalgorithm, const memory::desc &src_desc,
                const memory::desc &diff_weights_desc,
                const memory::desc &diff_bias_desc,
                const memory::desc &diff_dst_desc, const memory::dims &strides,
                const memory::dims &dilates, const memory::dims &padding_l,
                const memory::dims &padding_r) {
            memory::validate_dims(strides);
            memory::validate_dims(dilates);
            memory::validate_dims(padding_l);
            memory::validate_dims(padding_r);
            error::wrap_c_api(
                    dnnl_dilated_deconvolution_backward_weights_desc_init(&data,
                            convert_to_c(aalgorithm), &src_desc.data,
                            &diff_weights_desc.data, &diff_bias_desc.data,
                            &diff_dst_desc.data, &strides[0], &dilates[0],
                            &padding_l[0], &padding_r[0]),
                    "could not create a dilated  deconvolution backward "
                    "weights descriptor");
        }

        /// Initializes a descriptor for dilated deconvolution weight update
        /// without bias using @p aalgorithm, memory descriptors, @p strides,
        /// @p dilates @p padding_l, and @p padding_r.
        ///
        /// @note Memory descriptors are allowed to be initialized with
        ///       #dnnl::memory::format_tag::any value of @p format_kind.
        desc(algorithm aalgorithm, const memory::desc &src_desc,
                const memory::desc &diff_weights_desc,
                const memory::desc &diff_dst_desc, const memory::dims &strides,
                const memory::dims &dilates, const memory::dims &padding_l,
                const memory::dims &padding_r) {
            memory::validate_dims(strides);
            memory::validate_dims(dilates);
            memory::validate_dims(padding_l);
            memory::validate_dims(padding_r);
            error::wrap_c_api(
                    dnnl_dilated_deconvolution_backward_weights_desc_init(&data,
                            convert_to_c(aalgorithm), &src_desc.data,
                            &diff_weights_desc.data, nullptr,
                            &diff_dst_desc.data, &strides[0], &dilates[0],
                            &padding_l[0], &padding_r[0]),
                    "could not create a dilated deconvolution backward weights "
                    "descriptor");
        }
    };

    /// Primitive descriptor for deconvolution weight update.
    struct primitive_desc : public dnnl::primitive_desc {
        primitive_desc() = default;

        /// Initializes a primitive descriptor for deconvolution weight update.
        primitive_desc(const desc &desc, const engine &e,
                const deconvolution_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, nullptr, e, hint_fwd_pd.get(), allow_empty) {}

        /// Initializes a primitive descriptor for deconvolution weight update
        /// with attributes defined by @p attr.
        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e,
                const deconvolution_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, &attr, e, hint_fwd_pd.get(), allow_empty) {}

        /// Initializes a primitive descriptor for deconvolution weights
        /// update from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : dnnl::primitive_desc(pd, dnnl::primitive::kind::deconvolution,
                    dnnl::prop_kind::backward_weights) {}

        /// Queries source memory descriptor.
        memory::desc src_desc() const { return query_md(query::src_md, 0); }

        /// Queries diff weights memory descriptor.
        memory::desc diff_weights_desc() const {
            return query_md(query::diff_weights_md, 0);
        }

        /// Queries diff bias memory descriptor.
        memory::desc diff_bias_desc() const {
            return query_md(query::diff_weights_md, 1);
        }

        /// Queries diff destination memory descriptor.
        memory::desc diff_dst_desc() const {
            return query_md(query::diff_dst_md, 0);
        }
    };

    deconvolution_backward_weights() = default;

    /// Creates a deconvolution weight update primitive from the corresponding
    /// primitive descriptor.
    deconvolution_backward_weights(const primitive_desc &pd) : primitive(pd) {}
};

/// @}

/// @addtogroup cpp_api_lrn LRN
/// A primitive to perform local response normalization (LRN) across or within
/// channels.
///
/// @sa @ref dev_guide_lrn in developer guide
/// @sa @ref c_api_lrn in @ref c_api
/// @{

/// Local response normalization for forward propagation. Implements
/// descriptor, primitive descriptor, and primitive.
struct lrn_forward : public primitive {

    /// Descriptor for local response normalization forward propagation.
    struct desc {
        dnnl_lrn_desc_t data;

        /// Initializes a descriptor for forward propagation using @p prop_kind
        /// (possible values are #dnnl::forward_training and
        /// #dnnl::forward_inference), @p aalgorithm, memory descriptor @p
        /// data_desc, and regularization parameters @p local_size, @p alpha, @p
        /// beta, and @p k.
        desc(prop_kind aprop_kind, algorithm aalgorithm,
                const memory::desc &src_desc, memory::dim local_size,
                float alpha, float beta, float k = 1.f) {
            error::wrap_c_api(dnnl_lrn_forward_desc_init(&data,
                                      dnnl::convert_to_c(aprop_kind),
                                      convert_to_c(aalgorithm), &src_desc.data,
                                      local_size, alpha, beta, k),
                    "could not create a lrn forward descriptor");
        }
    };

    /// Primitive descriptor for local response normalization forward
    /// propagation.
    struct primitive_desc : public dnnl::primitive_desc {
        primitive_desc() = default;

        primitive_desc(
                const desc &desc, const engine &e, bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, nullptr, e, nullptr, allow_empty) {}

        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e, bool allow_empty = false)
            : dnnl::primitive_desc(&desc.data, &attr, e, nullptr, allow_empty) {
        }

        /// Initializes a primitive descriptor for local response
        /// normalization forward propagation from a C primitive descriptor @p
        /// pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : dnnl::primitive_desc(pd, dnnl::primitive::kind::lrn,
                    dnnl::prop_kind::forward_training,
                    dnnl::prop_kind::forward_inference) {}

        /// Queries source memory descriptor.
        memory::desc src_desc() const { return query_md(query::src_md, 0); }

        /// Queries destination memory descriptor.
        memory::desc dst_desc() const { return query_md(query::dst_md, 0); }

        /// Queries workspace memory descriptor.
        ///
        /// Returns a zero_md if no worspace is required.
        memory::desc workspace_desc() const {
            return query_md(query::workspace_md, 0);
        }
    };

    lrn_forward() = default;

    lrn_forward(const primitive_desc &pd) : primitive(pd) {}
};

/// Local response normalization for backward propagation.  Implements
/// descriptor, primitive descriptor, and primitive.
struct lrn_backward : public primitive {

    /// Descriptor for local response normalization backward propagation.
    struct desc {
        dnnl_lrn_desc_t data;

        /// Initializes a descriptor for backward propagation using @p aalgorithm,
        /// memory descriptors @p data_desc and @p diff_data_desc, and
        /// regularization parameters @p local_size, @p alpha, @p beta, and
        /// @p k.
        desc(algorithm aalgorithm, const memory::desc &data_desc,
                const memory::desc &diff_data_desc, memory::dim local_size,
                float alpha, float beta, float k = 1.f) {
            error::wrap_c_api(
                    dnnl_lrn_backward_desc_init(&data, convert_to_c(aalgorithm),
                            &diff_data_desc.data, &data_desc.data, local_size,
                            alpha, beta, k),
                    "could not create a lrn backward descriptor");
        }
    };

    /// Primitive descriptor for local response normalization backward
    /// propagation.
    struct primitive_desc : public dnnl::primitive_desc {
        primitive_desc() = default;

        primitive_desc(const desc &desc, const engine &e,
                const lrn_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, nullptr, e, hint_fwd_pd.get(), allow_empty) {}

        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e, const lrn_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, &attr, e, hint_fwd_pd.get(), allow_empty) {}

        /// Initializes a primitive descriptor for local response
        /// normalization backward propagation from a C primitive descriptor
        /// @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : dnnl::primitive_desc(pd, dnnl::primitive::kind::lrn,
                    dnnl::prop_kind::backward_data) {}

        /// Queries diff source memory descriptor.
        memory::desc diff_src_desc() const {
            return query_md(query::diff_src_md, 0);
        }

        /// Queries diff destination memory descriptor.
        memory::desc diff_dst_desc() const {
            return query_md(query::diff_dst_md, 0);
        }

        /// Queries workspace memory descriptor.
        ///
        /// Returns a zero_md if no worspace is required.
        memory::desc workspace_desc() const {
            return query_md(query::workspace_md, 0);
        }
    };

    lrn_backward() = default;

    lrn_backward(const primitive_desc &pd) : primitive(pd) {}
};

/// @}

/// @addtogroup cpp_api_pooling Pooling
/// A primitive to perform max or average pooling.
///
/// @sa @ref dev_guide_pooling in developer guide
/// @sa @ref c_api_pooling in @ref c_api
/// @{

/// Pooling for forward propagation.  Implements descriptor, primitive
/// descriptor, and primitive.
struct pooling_forward : public primitive {

    /// Descriptor for pooling forward propagation.
    struct desc {
        dnnl_pooling_desc_t data;

        /// Initializes a pooling descriptor for forward propagation using @p
        /// aprop_kind (possible values are #dnnl::forward_training and
        /// #dnnl::forward_inference), @p aalgorithm, memory descriptors, and
        /// pooling parameters in the spatial domain: @p strides, @p kernel
        /// sizes, @p padding_l, and @p padding_r.
        desc(prop_kind aprop_kind, algorithm aalgorithm,
                const memory::desc &src_desc, const memory::desc &dst_desc,
                const memory::dims &strides, const memory::dims &kernel,
                const memory::dims &padding_l, const memory::dims &padding_r) {
            memory::validate_dims(strides);
            memory::validate_dims(kernel);
            memory::validate_dims(padding_l);
            memory::validate_dims(padding_r);
            error::wrap_c_api(dnnl_pooling_forward_desc_init(&data,
                                      dnnl::convert_to_c(aprop_kind),
                                      convert_to_c(aalgorithm), &src_desc.data,
                                      &dst_desc.data, &strides[0], &kernel[0],
                                      &padding_l[0], &padding_r[0]),
                    "could not init a forward pooling descriptor");
        }
    };

    /// Primitive descriptor for pooling forward propagation.
    struct primitive_desc : public dnnl::primitive_desc {
        primitive_desc() = default;

        primitive_desc(
                const desc &desc, const engine &e, bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, nullptr, e, nullptr, allow_empty) {}

        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e, bool allow_empty = false)
            : dnnl::primitive_desc(&desc.data, &attr, e, nullptr, allow_empty) {
        }

        /// Initializes a primitive descriptor for pooling forward propagation
        /// from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : dnnl::primitive_desc(pd, dnnl::primitive::kind::pooling,
                    dnnl::prop_kind::forward_training,
                    dnnl::prop_kind::forward_inference) {}

        /// Queries source memory descriptor.
        memory::desc src_desc() const { return query_md(query::src_md, 0); }

        /// Queries destination memory descriptor.
        memory::desc dst_desc() const { return query_md(query::dst_md, 0); }

        /// Queries workspace memory descriptor.
        ///
        /// Returns a zero_md if no worspace is required.
        memory::desc workspace_desc() const {
            return query_md(query::workspace_md, 0);
        }
    };

    pooling_forward() = default;

    pooling_forward(const primitive_desc &pd) : primitive(pd) {}
};

struct pooling_backward : public primitive {

    /// Descriptor for pooling backward propagation.
    struct desc {
        dnnl_pooling_desc_t data;

        /// Initializes a pooling descriptor for backward propagation using @p
        /// aalgorithm, memory descriptors, and pooling parameters in the spatial
        /// domain: @p strides, @p kernel sizes, @p padding_l, and @p padding_r.
        desc(algorithm aalgorithm, const memory::desc &diff_src_desc,
                const memory::desc &diff_dst_desc, const memory::dims &strides,
                const memory::dims &kernel, const memory::dims &padding_l,
                const memory::dims &padding_r) {
            memory::validate_dims(strides);
            memory::validate_dims(kernel);
            memory::validate_dims(padding_l);
            memory::validate_dims(padding_r);
            error::wrap_c_api(
                    dnnl_pooling_backward_desc_init(&data,
                            convert_to_c(aalgorithm), &diff_src_desc.data,
                            &diff_dst_desc.data, &strides[0], &kernel[0],
                            &padding_l[0], &padding_r[0]),
                    "could not init a backward pooling descriptor");
        }
    };

    /// Primitive descriptor for pooling backward propagation.
    struct primitive_desc : public dnnl::primitive_desc {
        primitive_desc() = default;

        primitive_desc(const desc &desc, const engine &e,
                const pooling_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, nullptr, e, hint_fwd_pd.get(), allow_empty) {}

        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e,
                const pooling_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, &attr, e, hint_fwd_pd.get(), allow_empty) {}

        /// Initializes a primitive descriptor for pooling backward
        /// propagation from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : dnnl::primitive_desc(pd, dnnl::primitive::kind::pooling,
                    dnnl::prop_kind::backward_data) {}

        /// Queries diff source memory descriptor.
        memory::desc diff_src_desc() const {
            return query_md(query::diff_src_md, 0);
        }

        /// Queries diff destination memory descriptor.
        memory::desc diff_dst_desc() const {
            return query_md(query::diff_dst_md, 0);
        }

        /// Queries workspace memory descriptor.
        ///
        /// Returns a zero_md if no worspace is required.
        memory::desc workspace_desc() const {
            return query_md(query::workspace_md, 0);
        }
    };

    pooling_backward() = default;

    pooling_backward(const primitive_desc &pd) : primitive(pd) {}
};

/// @}

/// @addtogroup cpp_api_eltwise Eltwise
/// A primitive to compute element-wise operations such as rectified linear
/// unit (ReLU).
///
/// Both forward and backward passes support in-place operation; that is, src
/// and dst point to the same memory for forward pass, and diff_dst and
/// diff_src point to the same memory for backward pass.
///
/// @warning Because the original src is required for backward pass, in-place
/// forward pass in general cannot be applied during training. However, for
/// some kinds of element-wise operations (namely ReLU with alpha parameter
/// equals 0), dst and src can be interchangeable for the backward pass, which
/// enables performance of in-place forward even for training.
///
/// @sa @ref dev_guide_eltwise in developer guide
/// @sa @ref c_api_eltwise in @ref c_api
/// @{

/// Element-wise operations for forward propagation.  Implements descriptor,
/// primitive descriptor, and primitive.
struct eltwise_forward : public primitive {

    /// Initializes an eltwise descriptor for forward propagation using @p
    /// prop_kind (possible values are #dnnl::forward_training and
    /// #dnnl::forward_inference), @p aalgorithm algorithm, memory
    /// descriptor @p data_desc, @p alpha, and @p beta parameters.
    struct desc {
        dnnl_eltwise_desc_t data;
        desc(prop_kind aprop_kind, algorithm aalgorithm,
                const memory::desc &src_desc, float alpha = 0, float beta = 0) {
            error::wrap_c_api(dnnl_eltwise_forward_desc_init(&data,
                                      dnnl::convert_to_c(aprop_kind),
                                      dnnl::convert_to_c(aalgorithm),
                                      &src_desc.data, alpha, beta),
                    "could not create a eltwise forward descriptor");
        }
    };

    /// Primitive descriptor for eltwise forward propagation.
    struct primitive_desc : public dnnl::primitive_desc {
        primitive_desc() = default;

        primitive_desc(
                const desc &desc, const engine &e, bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, nullptr, e, nullptr, allow_empty) {}

        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e, bool allow_empty = false)
            : dnnl::primitive_desc(&desc.data, &attr, e, nullptr, allow_empty) {
        }

        /// Initializes a primitive descriptor for element-wise operations for
        /// forward propagation from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : dnnl::primitive_desc(pd, dnnl::primitive::kind::eltwise,
                    dnnl::prop_kind::forward_training,
                    dnnl::prop_kind::forward_inference) {}

        /// Queries source memory descriptor.
        memory::desc src_desc() const { return query_md(query::src_md, 0); }

        /// Queries destination memory descriptor.
        memory::desc dst_desc() const { return query_md(query::dst_md, 0); }
    };

    eltwise_forward() = default;

    eltwise_forward(const primitive_desc &pd) : primitive(pd) {}
};

/// Element-wise operations for backward propagation.  Implements descriptor,
/// primitive descriptor, and primitive.
struct eltwise_backward : public primitive {

    /// Initializes an eltwise descriptor for backward propagation using @p
    /// aalgorithm algorithm memory descriptors @p diff_data_desc and @p
    /// data_desc, and the @p alpha and @p beta parameters.
    struct desc {
        dnnl_eltwise_desc_t data;

        desc(algorithm aalgorithm, const memory::desc &diff_data_desc,
                const memory::desc &data_desc, float alpha = 0,
                float beta = 0) {
            error::wrap_c_api(
                    dnnl_eltwise_backward_desc_init(&data,
                            dnnl::convert_to_c(aalgorithm),
                            &diff_data_desc.data, &data_desc.data, alpha, beta),
                    "could not create a eltwise backward descriptor");
        }
    };

    /// Primitive descriptor for eltwise backward propagation.
    struct primitive_desc : public dnnl::primitive_desc {
        primitive_desc() = default;

        primitive_desc(const desc &desc, const engine &e,
                const eltwise_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, nullptr, e, hint_fwd_pd.get(), allow_empty) {}

        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e,
                const eltwise_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, &attr, e, hint_fwd_pd.get(), allow_empty) {}

        /// Initializes a primitive descriptor for element-wise operations for
        /// backward propagation from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : dnnl::primitive_desc(pd, dnnl::primitive::kind::eltwise,
                    dnnl::prop_kind::backward_data) {}

        /// Queries source memory descriptor.
        memory::desc src_desc() const { return query_md(query::src_md, 0); }

        /// Queries diff source memory descriptor.
        memory::desc diff_src_desc() const {
            return query_md(query::diff_src_md, 0);
        }

        /// Queries diff destination memory descriptor.
        memory::desc diff_dst_desc() const {
            return query_md(query::diff_dst_md, 0);
        }
    };

    eltwise_backward() = default;

    eltwise_backward(const primitive_desc &pd) : primitive(pd) {}
};

/// @}

/// @addtogroup cpp_api_softmax Softmax
/// A primitive to perform softmax.
///
/// @sa @ref dev_guide_softmax in developer guide
/// @sa @ref c_api_softmax in @ref c_api
/// @{

/// Softmax for forward propagation.  Implements descriptor, primitive
/// descriptor, and primitive.
struct softmax_forward : public primitive {

    /// Descriptor for softmax forward propagation.
    struct desc {
        dnnl_softmax_desc_t data;

        /// Initializes a softmax descriptor for forward propagation using @p
        /// prop_kind (possible values are #dnnl::forward_training and
        /// #dnnl::forward_inference) and memory descriptor @p data_desc.
        desc(prop_kind aprop_kind, const memory::desc &data_desc,
                int softmax_axis) {
            error::wrap_c_api(dnnl_softmax_forward_desc_init(&data,
                                      dnnl::convert_to_c(aprop_kind),
                                      &data_desc.data, softmax_axis),
                    "could not create a softmax forward descriptor");
        }
    };

    /// Primitive descriptor for softmax forward propagation.
    struct primitive_desc : public dnnl::primitive_desc {
        primitive_desc() = default;

        primitive_desc(
                const desc &desc, const engine &e, bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, nullptr, e, nullptr, allow_empty) {}

        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e, bool allow_empty = false)
            : dnnl::primitive_desc(&desc.data, &attr, e, nullptr, allow_empty) {
        }

        /// Initializes a primitive descriptor for softmax forward propagation
        /// from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : dnnl::primitive_desc(pd, dnnl::primitive::kind::softmax,
                    dnnl::prop_kind::forward_training,
                    dnnl::prop_kind::forward_inference) {}

        /// Queries source memory descriptor.
        memory::desc src_desc() const { return query_md(query::src_md, 0); }

        /// Queries destination memory descriptor.
        memory::desc dst_desc() const { return query_md(query::dst_md, 0); }
    };

    softmax_forward() = default;

    softmax_forward(const primitive_desc &pd) : primitive(pd) {}
};

/// Softmax for backward propagation.  Implements descriptor, primitive
/// descriptor, and primitive.
struct softmax_backward : public primitive {

    /// Descriptor for softmax backward propagation.
    struct desc {
        dnnl_softmax_desc_t data;

        /// Initializes a softmax descriptor for backward propagation using
        /// memory descriptors @p diff_desc and @p data_desc.
        desc(const memory::desc &diff_desc, const memory::desc &data_desc,
                int softmax_axis) {
            error::wrap_c_api(
                    dnnl_softmax_backward_desc_init(&data, &diff_desc.data,
                            &data_desc.data, softmax_axis),
                    "could not init a backward softmax descriptor");
        }
    };

    /// Primitive descriptor for softmax backward propagation.
    struct primitive_desc : public dnnl::primitive_desc {
        primitive_desc() = default;

        primitive_desc(const desc &desc, const engine &e,
                const softmax_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, nullptr, e, hint_fwd_pd.get(), allow_empty) {}

        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e,
                const softmax_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, &attr, e, hint_fwd_pd.get(), allow_empty) {}

        /// Initializes a primitive descriptor for softmax backward
        /// propagation from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : dnnl::primitive_desc(pd, dnnl::primitive::kind::softmax,
                    dnnl::prop_kind::backward_data) {}

        /// Queries destination memory descriptor.
        memory::desc dst_desc() const { return query_md(query::dst_md, 0); }

        /// Queries diff source memory descriptor.
        memory::desc diff_src_desc() const {
            return query_md(query::diff_src_md, 0);
        }

        /// Queries diff destination memory descriptor.
        memory::desc diff_dst_desc() const {
            return query_md(query::diff_dst_md, 0);
        }
    };

    softmax_backward() = default;

    softmax_backward(const primitive_desc &pd) : primitive(pd) {}
};

/// @}

/// @addtogroup cpp_api_batch_normalization Batch normalization
/// A primitive to perform batch normalization.
///
/// Both forward and backward passes support in-place operation; that is, src
/// and dst point to the same memory for forward pass, and diff_dst and diff_src
/// point to the same memory for backward pass.
///
/// Batch normalization supports different flavors controlled by
/// dnnl_batch_normalization_desc_t.  For example, batch normalization can
/// compute the mean and variance on its own or take them as inputs.  It can
/// either perform scaling and shifting using gamma and beta parameters or not.
/// Optionally, it can also perform a fused ReLU, which in case of training
/// would also require a workspace.
///
/// @sa @ref dev_guide_batch_normalization in developer guide
/// @sa @ref c_api_batch_normalization in @ref c_api
/// @{

/// Batch normalization for forward propagation.  Implements descriptor,
/// primitive descriptor, and primitive.
struct batch_normalization_forward : public primitive {

    /// Descriptor for batch normalization forward propagation.
    struct desc {
        dnnl_batch_normalization_desc_t data;

        /// Initializes a batch normalization descriptor for forward propagation
        /// using @p prop_kind (possible values are #dnnl::forward_training and
        /// #dnnl::forward_inference), memory descriptor @p data_desc,
        /// normalization parameter @p epsilon, and @p flags set using bit flags
        /// of type dnnl_batch_normalization_desc_t.
        ///
        /// @note In-place operation is supported; that is, dst points to the
        ///       same memory as src.
        desc(prop_kind aprop_kind, const memory::desc &src_desc, float epsilon,
                normalization_flags flags) {
            error::wrap_c_api(
                    dnnl_batch_normalization_forward_desc_init(&data,
                            dnnl::convert_to_c(aprop_kind), &src_desc.data,
                            epsilon, convert_to_c(flags)),
                    "could not create a batch normalization forward "
                    "descriptor");
        }
    };

    /// Primitive descriptor for batch normalization forward propagation.
    struct primitive_desc : public dnnl::primitive_desc {
        primitive_desc() = default;

        primitive_desc(
                const desc &desc, const engine &e, bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, nullptr, e, nullptr, allow_empty) {}

        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e, bool allow_empty = false)
            : dnnl::primitive_desc(&desc.data, &attr, e, nullptr, allow_empty) {
        }

        /// Initializes a primitive descriptor for batch normalization forward
        /// propagation from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : dnnl::primitive_desc(pd,
                    dnnl::primitive::kind::batch_normalization,
                    dnnl::prop_kind::forward_training,
                    dnnl::prop_kind::forward_inference) {}

        /// Queries source memory descriptor.
        memory::desc src_desc() const { return query_md(query::src_md, 0); }

        /// Queries weights (scale and shift) memory descriptor.
        memory::desc weights_desc() const {
            return query_md(query::weights_md, 0);
        }

        /// Queries destination memory descriptor.
        memory::desc dst_desc() const { return query_md(query::dst_md, 0); }

        /// Queries workspace memory descriptor.
        ///
        /// Returns a zero_md if no worspace is required.
        memory::desc workspace_desc() const {
            return query_md(query::workspace_md, 0);
        }

        /// Queries mean memory descriptor.
        memory::desc mean_desc() const { return stat_desc(mean); }

        /// Queries variance memory descriptor.
        memory::desc variance_desc() const { return stat_desc(var); }

    private:
        enum {
            mean = 1,
            var = 2,
        };
        memory::desc stat_desc(int kind) const {
            dnnl_batch_normalization_desc_t *p;
            error::wrap_c_api(
                    dnnl_primitive_desc_query(get(),
                            dnnl::convert_to_c(query::batch_normalization_d), 0,
                            &p),
                    "could not get a batch-normalization descriptor");
            return query_md(p->flags & dnnl_use_global_stats ? query::src_md
                                                             : query::dst_md,
                    kind);
        }
    };

    batch_normalization_forward() = default;

    batch_normalization_forward(const primitive_desc &pd) : primitive(pd) {}
};

/// Batch normalization backward propagation.  Implements descriptor, primitive
/// descriptor, and primitive.
struct batch_normalization_backward : public primitive {

    /// Descriptor for batch normalization backward propagation.
    struct desc {
        dnnl_batch_normalization_desc_t data;

        /// Initializes a batch normalization descriptor for backward
        /// propagation with respect to data and scale-shift parameters using
        /// memory descriptors @p data_desc and @p diff_data_desc, normalization
        /// parameter @p epsilon, and @p flags set using bit flags of type
        /// dnnl_batch_normalization_desc_t.
        ///
        /// @note In-place operation is supported; that is, diff_src points to
        ///       the same memory as diff_dst.
        desc(prop_kind aprop_kind, const memory::desc &diff_data_desc,
                const memory::desc &data_desc, float epsilon,
                normalization_flags flags) {
            error::wrap_c_api(dnnl_batch_normalization_backward_desc_init(&data,
                                      dnnl::convert_to_c(aprop_kind),
                                      &diff_data_desc.data, &data_desc.data,
                                      epsilon, convert_to_c(flags)),
                    "could not create a batch normalization backward "
                    "descriptor");
        }
    };

    /// Primitive descriptor for batch normalization backward propagation.
    struct primitive_desc : public dnnl::primitive_desc {
        primitive_desc() = default;

        primitive_desc(const desc &desc, const engine &e,
                const batch_normalization_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, nullptr, e, hint_fwd_pd.get(), allow_empty) {}

        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e,
                const batch_normalization_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, &attr, e, hint_fwd_pd.get(), allow_empty) {}

        /// Initializes a primitive descriptor for batch normalization
        /// backward propagation from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : dnnl::primitive_desc(pd,
                    dnnl::primitive::kind::batch_normalization,
                    dnnl::prop_kind::backward, dnnl::prop_kind::backward_data) {
        }

        /// Queries source memory descriptor.
        memory::desc src_desc() const { return query_md(query::src_md, 0); }

        /// Queries mean memory descriptor.
        memory::desc mean_desc() const { return query_md(query::src_md, 1); }

        /// Queries variance memory descriptor.
        memory::desc variance_desc() const {
            return query_md(query::src_md, 2);
        }

        /// Queries weights (scale and shift) memory descriptor.
        memory::desc weights_desc() const {
            return query_md(query::weights_md, 0);
        }

        /// Queries destination memory descriptor.
        memory::desc dst_desc() const { return query_md(query::dst_md, 0); }

        /// Queries diff destination memory descriptor.
        memory::desc diff_dst_desc() const {
            return query_md(query::diff_dst_md, 0);
        }

        /// Queries workspace memory descriptor.
        ///
        /// Returns a zero_md if no worspace is required.
        memory::desc workspace_desc() const {
            return query_md(query::workspace_md, 0);
        }

        /// Queries diff source memory descriptor.
        memory::desc diff_src_desc() const {
            return query_md(query::diff_src_md, 0);
        }

        /// Queries diff weights (scale and shift) memory descriptor.
        memory::desc diff_weights_desc() const {
            return query_md(query::diff_weights_md, 0);
        }
    };

    batch_normalization_backward() = default;

    batch_normalization_backward(const primitive_desc &pd) : primitive(pd) {}
};

/// @}

/// @addtogroup cpp_api_layer_normalization layer normalization
/// A primitive to perform layer normalization. Normalization is performed over
/// the last logical axis of data tensor.
///
/// Both forward and backward passes support in-place operation; that is, src
/// and dst point to the same memory for forward pass, and diff_dst and diff_src
/// point to the same memory for backward pass.
///
/// layer normalization supports different flavors controlled by
/// dnnl_layer_normalization_desc_t.  For example, layer normalization can
/// compute the mean and variance on its own or take them as inputs.  It can
/// either perform scaling and shifting using gamma and beta parameters or not.
/// Optionally, it can also perform a fused ReLU, which in case of training
/// would also require a workspace.
///
/// @sa @ref dev_guide_layer_normalization in developer guide
/// @sa @ref c_api_layer_normalization in @ref c_api
/// @{

/// layer normalization for forward propagation.  Implements descriptor,
/// primitive descriptor, and primitive.
struct layer_normalization_forward : public primitive {

    /// Descriptor for layer normalization forward propagation.
    struct desc {
        dnnl_layer_normalization_desc_t data;

        /// Initializes a layer normalization descriptor for forward propagation
        /// using @p prop_kind (possible values are #dnnl::forward_training and
        /// #dnnl::forward_inference), memory descriptor @p data_desc,
        /// normalization parameter @p epsilon, and @p flags set using bit flags
        /// of type dnnl_layer_normalization_desc_t.
        ///
        /// @note In-place operation is supported; that is, dst points to the
        ///       same memory as src.
        desc(prop_kind aprop_kind, const memory::desc &src_desc,
                const memory::desc &stat_desc, float epsilon,
                normalization_flags flags) {
            error::wrap_c_api(
                    dnnl_layer_normalization_forward_desc_init(&data,
                            dnnl::convert_to_c(aprop_kind), &src_desc.data,
                            &stat_desc.data, epsilon, convert_to_c(flags)),
                    "could not create a layer normalization forward "
                    "descriptor");
        }

        desc(prop_kind aprop_kind, const memory::desc &src_desc, float epsilon,
                normalization_flags flags) {
            error::wrap_c_api(
                    dnnl_layer_normalization_forward_desc_init(&data,
                            dnnl::convert_to_c(aprop_kind), &src_desc.data,
                            nullptr, epsilon, convert_to_c(flags)),
                    "could not create a layer normalization forward "
                    "descriptor");
        }
    };

    /// Primitive descriptor for layer normalization forward propagation.
    struct primitive_desc : public dnnl::primitive_desc {
        primitive_desc() = default;

        primitive_desc(
                const desc &desc, const engine &e, bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, nullptr, e, nullptr, allow_empty) {}

        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e, bool allow_empty = false)
            : dnnl::primitive_desc(&desc.data, &attr, e, nullptr, allow_empty) {
        }

        /// Initializes a primitive descriptor for layer normalization forward
        /// propagation from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : dnnl::primitive_desc(pd,
                    dnnl::primitive::kind::layer_normalization,
                    dnnl::prop_kind::forward_training,
                    dnnl::prop_kind::forward_inference) {}

        /// Queries source memory descriptor.
        memory::desc src_desc() const { return query_md(query::src_md, 0); }

        /// Queries weights (scale and shift) memory descriptor.
        memory::desc weights_desc() const {
            return query_md(query::weights_md, 0);
        }

        /// Queries destination memory descriptor.
        memory::desc dst_desc() const { return query_md(query::dst_md, 0); }

        /// Queries mean memory descriptor.
        memory::desc mean_desc() const { return stat_desc(mean); }

        /// Queries variance memory descriptor.
        memory::desc variance_desc() const { return stat_desc(var); }

        /// Queries workspace memory descriptor.
        ///
        /// Returns a zero_md if no worspace is required.
        memory::desc workspace_desc() const {
            return query_md(query::workspace_md, 0);
        }

    private:
        enum {
            mean = 1,
            var = 2,
        };
        memory::desc stat_desc(int kind) const {
            dnnl_layer_normalization_desc_t *p;
            error::wrap_c_api(
                    dnnl_primitive_desc_query(get(),
                            dnnl::convert_to_c(query::layer_normalization_d), 0,
                            &p),
                    "could not get a layer-normalization descriptor");
            return query_md(p->flags & dnnl_use_global_stats ? query::src_md
                                                             : query::dst_md,
                    kind);
        }
    };

    layer_normalization_forward() = default;

    layer_normalization_forward(const primitive_desc &pd) : primitive(pd) {}
};

/// layer normalization backward propagation.  Implements descriptor, primitive
/// descriptor, and primitive.
struct layer_normalization_backward : public primitive {

    /// Descriptor for layer normalization backward propagation.
    struct desc {
        dnnl_layer_normalization_desc_t data;

        /// Initializes a layer normalization descriptor for backward
        /// propagation with respect to data and scale-shift parameters using
        /// memory descriptors @p data_desc and @p diff_data_desc, normalization
        /// parameter @p epsilon, and @p flags set using bit flags of type
        /// dnnl_layer_normalization_desc_t.
        ///
        /// @note In-place operation is supported; that is, diff_src points to
        ///       the same memory as diff_dst.
        desc(prop_kind aprop_kind, const memory::desc &diff_data_desc,
                const memory::desc &data_desc, const memory::desc &stat_desc,
                float epsilon, normalization_flags flags) {
            error::wrap_c_api(
                    dnnl_layer_normalization_backward_desc_init(&data,
                            dnnl::convert_to_c(aprop_kind),
                            &diff_data_desc.data, &data_desc.data,
                            &stat_desc.data, epsilon, convert_to_c(flags)),
                    "could not create a layer normalization backward "
                    "descriptor");
        }

        desc(prop_kind aprop_kind, const memory::desc &diff_data_desc,
                const memory::desc &data_desc, float epsilon,
                normalization_flags flags) {
            error::wrap_c_api(dnnl_layer_normalization_backward_desc_init(&data,
                                      dnnl::convert_to_c(aprop_kind),
                                      &diff_data_desc.data, &data_desc.data,
                                      nullptr, epsilon, convert_to_c(flags)),
                    "could not create a layer normalization backward "
                    "descriptor");
        }
    };

    /// Primitive descriptor for layer normalization backward propagation.
    struct primitive_desc : public dnnl::primitive_desc {
        primitive_desc() = default;

        primitive_desc(const desc &desc, const engine &e,
                const layer_normalization_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, nullptr, e, hint_fwd_pd.get(), allow_empty) {}

        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e,
                const layer_normalization_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, &attr, e, hint_fwd_pd.get(), allow_empty) {}

        /// Initializes a primitive descriptor for layer normalization
        /// backward propagation from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : dnnl::primitive_desc(pd,
                    dnnl::primitive::kind::layer_normalization,
                    dnnl::prop_kind::backward, dnnl::prop_kind::backward_data) {
        }

        /// Queries source memory descriptor.
        memory::desc src_desc() const { return query_md(query::src_md, 0); }

        /// Queries mean memory descriptor.
        memory::desc mean_desc() const { return query_md(query::src_md, 1); }

        /// Queries variance memory descriptor.
        memory::desc variance_desc() const {
            return query_md(query::src_md, 2);
        }

        /// Queries weights (scale and shift) memory descriptor.
        memory::desc weights_desc() const {
            return query_md(query::weights_md, 0);
        }

        /// Queries destination memory descriptor.
        memory::desc dst_desc() const { return query_md(query::dst_md, 0); }

        /// Queries diff destination memory descriptor.
        memory::desc diff_dst_desc() const {
            return query_md(query::diff_dst_md, 0);
        }

        /// Queries diff source memory descriptor.
        memory::desc diff_src_desc() const {
            return query_md(query::diff_src_md, 0);
        }

        /// Queries diff weights (scale and shift) memory descriptor.
        memory::desc diff_weights_desc() const {
            return query_md(query::diff_weights_md, 0);
        }

        /// Queries workspace memory descriptor.
        ///
        /// Returns a zero_md if no worspace is required.
        memory::desc workspace_desc() const {
            return query_md(query::workspace_md, 0);
        }
    };

    layer_normalization_backward() = default;

    layer_normalization_backward(const primitive_desc &pd) : primitive(pd) {}
};

/// @}

/// @addtogroup cpp_api_inner_product Inner Product
/// A primitive to compute an inner product.
///
/// @sa @ref dev_guide_inner_product in developer guide
/// @sa @ref c_api_inner_product in @ref c_api
/// @{

/// Inner product for forward propagation.  Implements descriptor, primitive
/// descriptor, and primitive.
struct inner_product_forward : public primitive {

    /// Initializes an inner product descriptor for forward propagation using
    /// @p prop_kind (possible values are #dnnl::prop_kind::forward_training
    /// and #dnnl::prop_kind::forward_inference) and memory descriptors. In
    /// order to create an inner product without bias, @p bias_desc should
    /// refer to a descriptor with memory format kind set to
    /// #dnnl::memory::format_tag::undef.
    ///
    /// @note Memory descriptors are allowed to be initialized with
    ///       #dnnl::memory::format_tag::any value of @p format_kind.
    struct desc {
        dnnl_inner_product_desc_t data;
        desc(prop_kind aprop_kind, const memory::desc &src_desc,
                const memory::desc &weights_desc, const memory::desc &bias_desc,
                const memory::desc &dst_desc) {
            error::wrap_c_api(dnnl_inner_product_forward_desc_init(&data,
                                      dnnl::convert_to_c(aprop_kind),
                                      &src_desc.data, &weights_desc.data,
                                      &bias_desc.data, &dst_desc.data),
                    "could not create a inner product forward descriptor");
        }

        desc(prop_kind aprop_kind, const memory::desc &src_desc,
                const memory::desc &weights_desc,
                const memory::desc &dst_desc) {
            error::wrap_c_api(
                    dnnl_inner_product_forward_desc_init(&data,
                            dnnl::convert_to_c(aprop_kind), &src_desc.data,
                            &weights_desc.data, nullptr, &dst_desc.data),
                    "could not create a inner product forward descriptor");
        }
    };

    /// Primitive descriptor for inner product forward propagation.
    struct primitive_desc : public dnnl::primitive_desc {
        primitive_desc() = default;

        primitive_desc(
                const desc &desc, const engine &e, bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, nullptr, e, nullptr, allow_empty) {}

        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e, bool allow_empty = false)
            : dnnl::primitive_desc(&desc.data, &attr, e, nullptr, allow_empty) {
        }

        /// Initializes a primitive descriptor for inner product forward
        /// propagation from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : dnnl::primitive_desc(pd, dnnl::primitive::kind::inner_product,
                    dnnl::prop_kind::forward_training,
                    dnnl::prop_kind::forward_inference) {}

        /// Queries source memory descriptor.
        memory::desc src_desc() const { return query_md(query::src_md, 0); }

        /// Queries weights memory descriptor.
        memory::desc weights_desc() const {
            return query_md(query::weights_md, 0);
        }

        /// Queries bias memory descriptor.
        ///
        /// Returns a zero_md if no bias was specified at op_desc
        /// creation time.
        memory::desc bias_desc() const {
            return query_md(query::weights_md, 1);
        }

        /// Queries destination memory descriptor.
        memory::desc dst_desc() const { return query_md(query::dst_md, 0); }
    };

    inner_product_forward() = default;

    inner_product_forward(const primitive_desc &pd) : primitive(pd) {}
};

/// Inner product for backward propagation with respect to data.  Implements
/// descriptor, primitive descriptor, and primitive.
struct inner_product_backward_data : public primitive {

    /// Initializes an inner product descriptor for backward propagation with
    /// respect to data using memory descriptors.
    ///
    /// @note Memory descriptors are allowed to be initialized with
    ///       #dnnl::memory::format_tag::any value of @p format_kind.
    struct desc {
        dnnl_inner_product_desc_t data;
        desc(const memory::desc &diff_src_desc,
                const memory::desc &weights_desc,
                const memory::desc &diff_dst_desc) {
            error::wrap_c_api(dnnl_inner_product_backward_data_desc_init(&data,
                                      &diff_src_desc.data, &weights_desc.data,
                                      &diff_dst_desc.data),
                    "could not create a inner product backward data "
                    "descriptor");
        }
    };

    /// Primitive descriptor for inner product backward propagation with
    /// respect to data.
    struct primitive_desc : public dnnl::primitive_desc {
        primitive_desc() = default;

        primitive_desc(const desc &desc, const engine &e,
                const inner_product_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, nullptr, e, hint_fwd_pd.get(), allow_empty) {}

        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e,
                const inner_product_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, &attr, e, hint_fwd_pd.get(), allow_empty) {}

        /// Initializes a primitive descriptor for inner product backward
        /// propagation from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : dnnl::primitive_desc(pd, dnnl::primitive::kind::inner_product,
                    dnnl::prop_kind::backward_data) {}

        /// Queries diff source gradient memory descriptor.
        memory::desc diff_src_desc() const {
            return query_md(query::diff_src_md, 0);
        }

        /// Queries weights memory descriptor.
        memory::desc weights_desc() const {
            return query_md(query::weights_md, 0);
        }

        /// Queries diff destination memory descriptor.
        memory::desc diff_dst_desc() const {
            return query_md(query::diff_dst_md, 0);
        }
    };

    inner_product_backward_data() = default;

    inner_product_backward_data(const primitive_desc &pd) : primitive(pd) {}
};

/// Inner product for backward propagation with respect to weights.  Implements
/// descriptor, primitive descriptor, and primitive.
struct inner_product_backward_weights : public primitive {

    /// Initializes an inner product descriptor for backward propagation with
    /// respect to weights using memory descriptors.
    ///
    /// @note Memory descriptors are allowed to be initialized with
    ///       #dnnl::memory::format_tag::any value of @p format_kind.
    struct desc {
        dnnl_inner_product_desc_t data;
        desc(const memory::desc &src_desc,
                const memory::desc &diff_weights_desc,
                const memory::desc &diff_bias_desc,
                const memory::desc &diff_dst_desc) {
            error::wrap_c_api(
                    dnnl_inner_product_backward_weights_desc_init(&data,
                            &src_desc.data, &diff_weights_desc.data,
                            &diff_bias_desc.data, &diff_dst_desc.data),
                    "could not create a inner product backward weights "
                    "descriptor");
        }
        desc(const memory::desc &src_desc,
                const memory::desc &diff_weights_desc,
                const memory::desc &diff_dst_desc) {
            error::wrap_c_api(
                    dnnl_inner_product_backward_weights_desc_init(&data,
                            &src_desc.data, &diff_weights_desc.data, nullptr,
                            &diff_dst_desc.data),
                    "could not create a inner product backward weights "
                    "descriptor");
        }
    };

    /// Primitive descriptor for inner product backward propagation with
    /// respect to weights.
    struct primitive_desc : public dnnl::primitive_desc {
        primitive_desc() = default;

        primitive_desc(const desc &desc, const engine &e,
                const inner_product_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, nullptr, e, hint_fwd_pd.get(), allow_empty) {}

        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e,
                const inner_product_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, &attr, e, hint_fwd_pd.get(), allow_empty) {}

        /// Initializes a primitive descriptor for inner product weights
        /// update from a C primitive descriptor @p cpd.
        primitive_desc(dnnl_primitive_desc_t cpd)
            : dnnl::primitive_desc(cpd, dnnl::primitive::kind::inner_product,
                    dnnl::prop_kind::backward_weights) {}

        /// Queries source memory descriptor.
        memory::desc src_desc() const { return query_md(query::src_md, 0); }

        /// Queries diff weights memory descriptor.
        memory::desc diff_weights_desc() const {
            return query_md(query::diff_weights_md, 0);
        }

        /// Queries diff bias memory descriptor.
        memory::desc diff_bias_desc() const {
            return query_md(query::diff_weights_md, 1);
        }

        /// Queries diff destination memory descriptor.
        memory::desc diff_dst_desc() const {
            return query_md(query::diff_dst_md, 0);
        }
    };

    inner_product_backward_weights() = default;

    inner_product_backward_weights(const primitive_desc &pd) : primitive(pd) {}
};

/// @}

/// @addtogroup cpp_api_rnn RNN
/// A primitive to compute common recurrent layer.
///
/// @sa @ref dev_guide_rnn in developer guide
/// @sa @ref c_api_rnn in @ref c_api
/// @{

struct rnn_primitive_desc_base : public primitive_desc {
    using primitive_desc::primitive_desc;

    rnn_primitive_desc_base() = default;

protected:
    // Constructs an RNN primitive descriptor from a C counterpart while
    // checking that it actually describes the expected primitive.
    rnn_primitive_desc_base(dnnl_primitive_desc_t pd,
            dnnl::prop_kind prop_kind1, dnnl::prop_kind prop_kind2,
            dnnl::algorithm cell_kind) {
        dnnl_rnn_desc_t *rnn_d;
        dnnl_status_t rc;
        rc = dnnl_primitive_desc_query(pd, dnnl_query_rnn_d, 0, &rnn_d);
        error::wrap_c_api(
                rc, "could not retrieve rnn_desc from a primitive descriptor");

        dnnl_prop_kind_t c_prop_kind1 = convert_to_c(prop_kind1);
        dnnl_prop_kind_t c_prop_kind2 = convert_to_c(prop_kind2);
        dnnl_alg_kind_t c_cell_kind = convert_to_c(cell_kind);

        bool ok = rnn_d->primitive_kind == dnnl_rnn
                && (rnn_d->prop_kind == c_prop_kind1
                        || rnn_d->prop_kind == c_prop_kind2)
                && rnn_d->cell_kind == c_cell_kind;

        if (!ok) throw error(dnnl_invalid_arguments, "rnn descriptor mismatch");

        reset_with_clone(pd);
    }

    // Constructs an RNN primitive descriptor from a C counterpart while
    // checking that it actually describes the expected primitive.
    rnn_primitive_desc_base(dnnl_primitive_desc_t pd, dnnl::prop_kind prop_kind,
            dnnl::algorithm cell_kind)
        : rnn_primitive_desc_base(pd, prop_kind, prop_kind, cell_kind) {}
};

/// Vanilla RNN for forward propagation.
///
/// Implements descriptor, primitive descriptor, and primitive.
struct vanilla_rnn_forward : public primitive {

    /// Descriptor for RNN forward propagation.
    struct desc {
        dnnl_rnn_desc_t data;

        /// Initializes an RNN descriptor for forward propagation using @p
        /// prop_kind, @p activation, @p direction, and memory descriptors.
        /// @note If @p prop_kind equals #dnnl::forward_training, you must
        /// query a workspace memory descriptor before creating the primitive.
        ///
        /// @p alpha, @p beta and @p flags are parameters to the RNN descriptor.
        /// If @p activation is #eltwise_relu, @p alpha represents the negative
        /// slope.
        /// @p beta and @p flags are currently ignored.
        ///
        /// @p src_iter_desc, @p bias_desc, and @p dst_iter_desc are allowed
        /// to point to a zero memory descriptor, which would indicate that
        /// the RNN primitive should not use them.
        ///
        /// @note
        ///     All memory descriptors except @p src_iter_desc can be
        ///     initialized with an #dnnl::memory::format_tag::any value of @p
        ///     format_kind.
        desc(prop_kind aprop_kind, algorithm activation,
                rnn_direction direction, const memory::desc &src_layer_desc,
                const memory::desc &src_iter_desc,
                const memory::desc &weights_layer_desc,
                const memory::desc &weights_iter_desc,
                const memory::desc &bias_desc,
                const memory::desc &dst_layer_desc,
                const memory::desc &dst_iter_desc,
                rnn_flags flags = rnn_flags::undef, float alpha = 0.0f,
                float beta = 0.0f) {
            error::wrap_c_api(
                    dnnl_vanilla_rnn_forward_desc_init(&data,
                            dnnl::convert_to_c(aprop_kind),
                            dnnl::convert_to_c(activation),
                            dnnl::convert_to_c(direction), &src_layer_desc.data,
                            &src_iter_desc.data, &weights_layer_desc.data,
                            &weights_iter_desc.data, &bias_desc.data,
                            &dst_layer_desc.data, &dst_iter_desc.data,
                            dnnl::convert_to_c(flags), alpha, beta),
                    "could not create an RNN forward descriptor");
        }
    };

    /// Primitive descriptor for RNN forward propagation.
    struct primitive_desc : public rnn_primitive_desc_base {
        primitive_desc() = default;

        primitive_desc(
                const desc &desc, const engine &e, bool allow_empty = false)
            : rnn_primitive_desc_base(
                    &desc.data, nullptr, e, nullptr, allow_empty) {}

        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e, bool allow_empty = false)
            : rnn_primitive_desc_base(
                    &desc.data, &attr, e, nullptr, allow_empty) {}

        /// Initializes a primitive descriptor for RNN forward propagation
        /// from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : rnn_primitive_desc_base(pd, dnnl::prop_kind::forward_training,
                    dnnl::prop_kind::forward_inference,
                    dnnl::algorithm::vanilla_rnn) {}

        /// Queries source layer memory descriptor.
        memory::desc src_layer_desc() const {
            return query_md(query::src_md, 0);
        }

        /// Queries source iteration memory descriptor.
        ///
        /// Returns a zero_md if no src_iter was specified at op_desc
        /// creation time.
        memory::desc src_iter_desc() const {
            return query_md(query::src_md, 1);
        }

        /// Queries weights layer memory descriptor.
        memory::desc weights_layer_desc() const {
            return query_md(query::weights_md, 0);
        }

        /// Queries weights iteration memory descriptor.
        memory::desc weights_iter_desc() const {
            return query_md(query::weights_md, 1);
        }

        /// Queries bias memory descriptor.
        ///
        /// Returns a zero_md if no bias was specified at op_desc
        /// creation time.
        memory::desc bias_desc() const {
            return query_md(query::weights_md, 2);
        }

        /// Queries destination layer memory descriptor.
        memory::desc dst_layer_desc() const {
            return query_md(query::dst_md, 0);
        }

        /// Queries destination iteration memory descriptor.
        ///
        /// Returns a zero_md if no dst_iter was specified at op_desc
        /// creation time.
        memory::desc dst_iter_desc() const {
            return query_md(query::dst_md, 1);
        }

        /// Queries workspace memory descriptor.
        ///
        /// Returns a zero_md if no worspace is required.
        memory::desc workspace_desc() const {
            return query_md(query::workspace_md, 0);
        }
    };

    vanilla_rnn_forward() = default;

    vanilla_rnn_forward(const primitive_desc &pd) : primitive(pd) {}
};

/// Vanilla RNN for backward propagation.
///
/// Implements descriptor, primitive descriptor, and primitive.
struct vanilla_rnn_backward : public primitive {

    /// RNN descriptor for backward propagation.
    struct desc {
        dnnl_rnn_desc_t data;

        /// Initializes an RNN descriptor for backward propagation using @p
        /// prop_kind, @p activation, @p direction, and memory descriptors.
        ///
        /// @p alpha, @p beta and @p flags are parameters to the RNN descriptor.
        /// If @p activation is #eltwise_relu, @p alpha represents the negative
        /// slope.
        /// @p beta and @p flags are currently ignored.
        ///
        /// @note All memory descriptors are allowed to be initialized with
        ///       #dnnl::memory::format_tag::any value of @p format_kind.
        ///
        /// @p src_iter_desc (simultaneously with @p diff_src_iter_desc), @p
        /// bias_desc (simultaneously with @p diff_bias_desc), and @p
        /// dst_iter_desc (simultaneously with @p diff_src_iter_desc) are
        /// allowed point to a zero memory descriptor, which would indicate
        /// that the RNN primitive should not use them and consider them to be
        /// zero values.
        desc(prop_kind aprop_kind, algorithm activation,
                rnn_direction direction, const memory::desc &src_layer_desc,
                const memory::desc &src_iter_desc,
                const memory::desc &weights_layer_desc,
                const memory::desc &weights_iter_desc,
                const memory::desc &bias_desc,
                const memory::desc &dst_layer_desc,
                const memory::desc &dst_iter_desc,
                const memory::desc &diff_src_layer_desc,
                const memory::desc &diff_src_iter_desc,
                const memory::desc &diff_weights_layer_desc,
                const memory::desc &diff_weights_iter_desc,
                const memory::desc &diff_bias_desc,
                const memory::desc &diff_dst_layer_desc,
                const memory::desc &diff_dst_iter_desc,
                rnn_flags flags = rnn_flags::undef, float alpha = 0.0f,
                float beta = 0.0f) {
            error::wrap_c_api(
                    dnnl_vanilla_rnn_backward_desc_init(&data,
                            dnnl::convert_to_c(aprop_kind),
                            dnnl::convert_to_c(activation),
                            dnnl::convert_to_c(direction), &src_layer_desc.data,
                            &src_iter_desc.data, &weights_layer_desc.data,
                            &weights_iter_desc.data, &bias_desc.data,
                            &dst_layer_desc.data, &dst_iter_desc.data,
                            &diff_src_layer_desc.data, &diff_src_iter_desc.data,
                            &diff_weights_layer_desc.data,
                            &diff_weights_iter_desc.data, &diff_bias_desc.data,
                            &diff_dst_layer_desc.data, &diff_dst_iter_desc.data,
                            dnnl::convert_to_c(flags), alpha, beta),
                    "could not create an RNN backward descriptor");
        }
    };

    /// Primitive descriptor for RNN backward propagation.
    struct primitive_desc : public rnn_primitive_desc_base {
        primitive_desc() = default;

        primitive_desc(const desc &desc, const engine &e,
                const vanilla_rnn_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : rnn_primitive_desc_base(
                    &desc.data, nullptr, e, hint_fwd_pd.get(), allow_empty) {}

        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e,
                const vanilla_rnn_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : rnn_primitive_desc_base(
                    &desc.data, &attr, e, hint_fwd_pd.get(), allow_empty) {}

        /// Initializes a primitive descriptor for RNN backward
        /// propagation from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : rnn_primitive_desc_base(pd, dnnl::prop_kind::backward,
                    dnnl::algorithm::vanilla_rnn) {}

        /// Queries source layer memory descriptor.
        memory::desc src_layer_desc() const {
            return query_md(query::src_md, 0);
        }

        /// Queries source iteration memory descriptor.
        ///
        /// Returns a zero_md if no src_iter was specified at op_desc
        /// creation time.
        memory::desc src_iter_desc() const {
            return query_md(query::src_md, 1);
        }

        /// Queries weights layer memory descriptor.
        memory::desc weights_layer_desc() const {
            return query_md(query::weights_md, 0);
        }

        /// Queries weights iteration memory descriptor.
        memory::desc weights_iter_desc() const {
            return query_md(query::weights_md, 1);
        }

        /// Queries bias memory descriptor.
        ///
        /// Returns a zero_md if no bias was specified at op_desc
        /// creation time.
        memory::desc bias_desc() const {
            return query_md(query::weights_md, 2);
        }

        /// Queries destination layer memory descriptor.
        memory::desc dst_layer_desc() const {
            return query_md(query::dst_md, 0);
        }

        /// Queries destination iteration memory descriptor.
        ///
        /// Returns a zero_md if no dst_iter was specified at op_desc
        /// creation time.
        memory::desc dst_iter_desc() const {
            return query_md(query::dst_md, 1);
        }

        /// Queries workspace memory descriptor.
        ///
        /// Returns a zero_md if no worspace is required.
        memory::desc workspace_desc() const {
            return query_md(query::workspace_md, 0);
        }

        /// Queries diff source layer memory descriptor.
        memory::desc diff_src_layer_desc() const {
            return query_md(query::diff_src_md, 0);
        }

        /// Queries diff source iteration memory descriptor.
        ///
        /// Returns a zero_md if no diff_src_iter was specified at op_desc
        /// creation time.
        memory::desc diff_src_iter_desc() const {
            return query_md(query::diff_src_md, 1);
        }

        /// Queries diff weights layer memory descriptor.
        memory::desc diff_weights_layer_desc() const {
            return query_md(query::diff_weights_md, 0);
        }

        /// Queries diff weights iteration memory descriptor.
        memory::desc diff_weights_iter_desc() const {
            return query_md(query::diff_weights_md, 1);
        }

        /// Queries diff bias memory descriptor.
        memory::desc diff_bias_desc() const {
            return query_md(query::diff_weights_md, 2);
        }

        /// Queries diff destination layer memory descriptor.
        memory::desc diff_dst_layer_desc() const {
            return query_md(query::diff_dst_md, 0);
        }

        /// Queries diff destination iteration memory descriptor.
        ///
        /// Returns a zero_md if no diff_dst_iter was specified at op_desc
        /// creation time.
        memory::desc diff_dst_iter_desc() const {
            return query_md(query::diff_dst_md, 1);
        }
    };

    vanilla_rnn_backward() = default;

    vanilla_rnn_backward(const primitive_desc &pd) : primitive(pd) {}
};

/// LSTM for forward propagation.
///
/// Implements descriptor, primitive descriptor, and primitive.
struct lstm_forward : public primitive {

    /// Descriptor for LSTM forward propagation.
    struct desc {
        dnnl_rnn_desc_t data;

        /// Initializes an LSTM descriptor for forward propagation using @p
        /// prop_kind, @p direction, and memory descriptors.
        /// @note If @p prop_kind equals #dnnl::forward_training, you must
        /// query a workspace memory descriptor before creating the primitive.
        ///
        /// @p flags is a parameter to the LSTM descriptor and is currently
        /// ignored.
        ///
        /// @p src_iter_desc, @p src_iter_c_desc, @p bias_desc, @p
        /// dst_iter_desc and @p dst_iter_c_desc are allowed to point
        /// to a zero memory descriptor, which would indicate that the
        /// LSTM primitive should not use them.
        ///
        /// @note
        ///     All memory descriptors except @p src_iter_desc can be
        ///     initialized with an #dnnl::memory::format_tag::any value of @p
        ///     format_kind.
        desc(prop_kind aprop_kind, rnn_direction direction,
                const memory::desc &src_layer_desc,
                const memory::desc &src_iter_desc,
                const memory::desc &src_iter_c_desc,
                const memory::desc &weights_layer_desc,
                const memory::desc &weights_iter_desc,
                const memory::desc &bias_desc,
                const memory::desc &dst_layer_desc,
                const memory::desc &dst_iter_desc,
                const memory::desc &dst_iter_c_desc,
                rnn_flags flags = rnn_flags::undef) {
            error::wrap_c_api(
                    dnnl_lstm_forward_desc_init(&data,
                            dnnl::convert_to_c(aprop_kind),
                            dnnl::convert_to_c(direction), &src_layer_desc.data,
                            &src_iter_desc.data, &src_iter_c_desc.data,
                            &weights_layer_desc.data, &weights_iter_desc.data,
                            &bias_desc.data, &dst_layer_desc.data,
                            &dst_iter_desc.data, &dst_iter_c_desc.data,
                            dnnl::convert_to_c(flags)),
                    "could not create an LSTM forward descriptor");
        }
    };

    /// Primitive descriptor for LSTM forward propagation.
    struct primitive_desc : public rnn_primitive_desc_base {
        primitive_desc() = default;

        primitive_desc(
                const desc &desc, const engine &e, bool allow_empty = false)
            : rnn_primitive_desc_base(
                    &desc.data, nullptr, e, nullptr, allow_empty) {}

        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e, bool allow_empty = false)
            : rnn_primitive_desc_base(
                    &desc.data, &attr, e, nullptr, allow_empty) {}

        /// Initializes a primitive descriptor for LSTM forward propagation
        /// from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : rnn_primitive_desc_base(pd, dnnl::prop_kind::forward_training,
                    dnnl::prop_kind::forward_inference,
                    dnnl::algorithm::vanilla_lstm) {}

        /// Queries source layer memory descriptor.
        memory::desc src_layer_desc() const {
            return query_md(query::src_md, 0);
        }

        /// Queries source recurrent hidden state memory descriptor.
        ///
        /// Returns a zero_md if no src_iter was specified at op_desc
        /// creation time.
        memory::desc src_iter_desc() const {
            return query_md(query::src_md, 1);
        }

        /// Queries source recurrent cell state memory descriptor.
        memory::desc src_iter_c_desc() const {
            return query_md(query::src_md, 2);
        }

        /// Queries weights layer memory descriptor.
        memory::desc weights_layer_desc() const {
            return query_md(query::weights_md, 0);
        }

        /// Queries weights iteration memory descriptor.
        memory::desc weights_iter_desc() const {
            return query_md(query::weights_md, 1);
        }

        /// Queries bias memory descriptor.
        ///
        /// Returns a zero_md if no bias was specified at op_desc
        /// creation time.
        memory::desc bias_desc() const {
            return query_md(query::weights_md, 2);
        }

        /// Queries destination layer memory descriptor.
        memory::desc dst_layer_desc() const {
            return query_md(query::dst_md, 0);
        }

        /// Queries destination recurrent hidden state memory descriptor.
        ///
        /// Returns a zero_md if no dst_iter was specified at op_desc
        /// creation time.
        memory::desc dst_iter_desc() const {
            return query_md(query::dst_md, 1);
        }

        /// Queries destination recurrent cell state memory descriptor.
        memory::desc dst_iter_c_desc() const {
            return query_md(query::dst_md, 2);
        }

        /// Queries workspace memory descriptor.
        ///
        /// Returns a zero_md if no worspace is required.
        memory::desc workspace_desc() const {
            return query_md(query::workspace_md, 0);
        }
    };

    lstm_forward() = default;

    lstm_forward(const primitive_desc &pd) : primitive(pd) {}
};

/// LSTM for backward propagation.
///
/// Implements descriptor, primitive descriptor, and primitive.
struct lstm_backward : public primitive {

    /// LSTM descriptor for backward propagation.
    struct desc {
        dnnl_rnn_desc_t data;

        /// Initializes an LSTM descriptor for backward propagation using @p
        /// prop_kind, @p direction, and memory descriptors.
        ///
        /// @p flags is a parameter to the LSTM descriptor and is currently
        /// ignored.
        ///
        /// @note All memory descriptors are allowed to be initialized with
        ///       #dnnl::memory::format_tag::any value of @p format_kind.
        ///
        /// @p src_iter_desc (simultaneously with @p
        /// diff_src_iter_desc), @p src_iter_c_desc (simultaneously
        /// with @p diff_src_iter_c_desc), @p bias_desc
        /// (simultaneously with @p diff_bias_desc), @p dst_iter_desc
        /// (simultaneously with @p diff_src_iter_desc) and @p dst_iter_c_desc
        /// (simultaneously with @p diff_src_iter_c_desc) are allowed
        /// point to a zero memory descriptor, which would indicate
        /// that the LSTM primitive should not use them and consider
        /// them to be zero values.
        desc(prop_kind aprop_kind, rnn_direction direction,
                const memory::desc &src_layer_desc,
                const memory::desc &src_iter_desc,
                const memory::desc &src_iter_c_desc,
                const memory::desc &weights_layer_desc,
                const memory::desc &weights_iter_desc,
                const memory::desc &bias_desc,
                const memory::desc &dst_layer_desc,
                const memory::desc &dst_iter_desc,
                const memory::desc &dst_iter_c_desc,
                const memory::desc &diff_src_layer_desc,
                const memory::desc &diff_src_iter_desc,
                const memory::desc &diff_src_iter_c_desc,
                const memory::desc &diff_weights_layer_desc,
                const memory::desc &diff_weights_iter_desc,
                const memory::desc &diff_bias_desc,
                const memory::desc &diff_dst_layer_desc,
                const memory::desc &diff_dst_iter_desc,
                const memory::desc &diff_dst_iter_c_desc,
                rnn_flags flags = rnn_flags::undef) {
            error::wrap_c_api(
                    dnnl_lstm_backward_desc_init(&data,
                            dnnl::convert_to_c(aprop_kind),
                            dnnl::convert_to_c(direction), &src_layer_desc.data,
                            &src_iter_desc.data, &src_iter_c_desc.data,
                            &weights_layer_desc.data, &weights_iter_desc.data,
                            &bias_desc.data, &dst_layer_desc.data,
                            &dst_iter_desc.data, &dst_iter_c_desc.data,
                            &diff_src_layer_desc.data, &diff_src_iter_desc.data,
                            &diff_src_iter_c_desc.data,
                            &diff_weights_layer_desc.data,
                            &diff_weights_iter_desc.data, &diff_bias_desc.data,
                            &diff_dst_layer_desc.data, &diff_dst_iter_desc.data,
                            &diff_dst_iter_c_desc.data,
                            dnnl::convert_to_c(flags)),
                    "could not create an LSTM backward descriptor");
        }
    };

    /// Primitive descriptor for LSTM backward propagation.
    struct primitive_desc : public rnn_primitive_desc_base {
        primitive_desc() = default;

        primitive_desc(const desc &desc, const engine &e,
                const lstm_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : rnn_primitive_desc_base(
                    &desc.data, nullptr, e, hint_fwd_pd.get(), allow_empty) {}

        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e,
                const lstm_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : rnn_primitive_desc_base(
                    &desc.data, &attr, e, hint_fwd_pd.get(), allow_empty) {}

        /// Initializes a primitive descriptor for LSTM backward
        /// propagation from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : rnn_primitive_desc_base(pd, dnnl::prop_kind::backward,
                    dnnl::algorithm::vanilla_lstm) {}

        /// Queries source layer memory descriptor.
        memory::desc src_layer_desc() const {
            return query_md(query::src_md, 0);
        }

        /// Queries source recurrent hidden state memory descriptor.
        ///
        /// Returns a zero_md if no src_iter was specified at op_desc
        /// creation time.
        memory::desc src_iter_desc() const {
            return query_md(query::src_md, 1);
        }

        /// Queries source recurrent cell state memory descriptor.
        memory::desc src_iter_c_desc() const {
            return query_md(query::src_md, 2);
        }

        /// Queries weights layer memory descriptor.
        memory::desc weights_layer_desc() const {
            return query_md(query::weights_md, 0);
        }

        /// Queries weights iteration memory descriptor.
        memory::desc weights_iter_desc() const {
            return query_md(query::weights_md, 1);
        }

        /// Queries bias memory descriptor.
        ///
        /// Returns a zero_md if no bias was specified at op_desc
        /// creation time.
        memory::desc bias_desc() const {
            return query_md(query::weights_md, 2);
        }

        /// Queries destination layer memory descriptor.
        memory::desc dst_layer_desc() const {
            return query_md(query::dst_md, 0);
        }

        /// Queries destination recurrent hidden state memory descriptor.
        ///
        /// Returns a zero_md if no dst_iter was specified at op_desc
        /// creation time.
        memory::desc dst_iter_desc() const {
            return query_md(query::dst_md, 1);
        }

        /// Queries destination recurrent cell state memory descriptor.
        memory::desc dst_iter_c_desc() const {
            return query_md(query::dst_md, 2);
        }

        /// Queries workspace memory descriptor.
        ///
        /// Returns a zero_md if no worspace is required.
        memory::desc workspace_desc() const {
            return query_md(query::workspace_md, 0);
        }

        /// Queries diff source layer memory descriptor.
        memory::desc diff_src_layer_desc() const {
            return query_md(query::diff_src_md, 0);
        }

        /// Queries diff source recurrent hidden state memory descriptor.
        ///
        /// Returns a zero_md if no diff_src_iter was specified at op_desc
        /// creation time.
        memory::desc diff_src_iter_desc() const {
            return query_md(query::diff_src_md, 1);
        }

        /// Queries diff source recurrent cell state memory descriptor.
        memory::desc diff_src_iter_c_desc() const {
            return query_md(query::diff_src_md, 2);
        }

        /// Queries diff weights layer memory descriptor.
        memory::desc diff_weights_layer_desc() const {
            return query_md(query::diff_weights_md, 0);
        }

        /// Queries diff weights iteration memory descriptor.
        memory::desc diff_weights_iter_desc() const {
            return query_md(query::diff_weights_md, 1);
        }

        /// Queries diff bias memory descriptor.
        memory::desc diff_bias_desc() const {
            return query_md(query::diff_weights_md, 2);
        }

        /// Queries diff destination layer memory descriptor.
        memory::desc diff_dst_layer_desc() const {
            return query_md(query::diff_dst_md, 0);
        }

        /// Queries diff destination recurrent hidden state memory descriptor.
        ///
        /// Returns a zero_md if no diff_dst_iter was specified at op_desc
        /// creation time.
        memory::desc diff_dst_iter_desc() const {
            return query_md(query::diff_dst_md, 1);
        }

        /// Queries diff destination recurrent cell state memory descriptor.
        memory::desc diff_dst_iter_c_desc() const {
            return query_md(query::diff_dst_md, 2);
        }
    };

    lstm_backward() = default;

    // With last iteration (with and without input src_iter)
    lstm_backward(const primitive_desc &pd) : primitive(pd) {}
};

/// GRU for forward propagation.
///
/// Implements descriptor, primitive descriptor, and primitive.
struct gru_forward : public primitive {

    /// Descriptor for GRU forward propagation.
    struct desc {
        dnnl_rnn_desc_t data;

        /// Initializes a GRU descriptor for forward propagation using @p
        /// prop_kind, @p direction, and memory descriptors.
        /// @note If @p prop_kind equals #dnnl::forward_training, you must
        /// query a workspace memory descriptor before creating the primitive.
        ///
        /// @p flags is a parameter to the GRU descriptor and is currently
        /// ignored.
        ///
        /// @p src_iter_desc, @p bias_desc, and @p dst_iter_desc are allowed
        /// to point to a zero memory descriptor, which would indicate that
        /// the GRU primitive should not use them and will default to zero
        /// values.
        ///
        /// @note
        ///     All memory descriptors except @p src_iter_desc can be
        ///     initialized with an #dnnl::memory::format_tag::any value of @p
        ///     format_kind.
        desc(prop_kind aprop_kind, rnn_direction direction,
                const memory::desc &src_layer_desc,
                const memory::desc &src_iter_desc,
                const memory::desc &weights_layer_desc,
                const memory::desc &weights_iter_desc,
                const memory::desc &bias_desc,
                const memory::desc &dst_layer_desc,
                const memory::desc &dst_iter_desc,
                rnn_flags flags = rnn_flags::undef) {
            error::wrap_c_api(
                    dnnl_gru_forward_desc_init(&data,
                            dnnl::convert_to_c(aprop_kind),
                            dnnl::convert_to_c(direction), &src_layer_desc.data,
                            &src_iter_desc.data, &weights_layer_desc.data,
                            &weights_iter_desc.data, &bias_desc.data,
                            &dst_layer_desc.data, &dst_iter_desc.data,
                            dnnl::convert_to_c(flags)),
                    "could not create a GRU forward descriptor");
        }
    };

    /// Primitive descriptor for GRU forward propagation.
    struct primitive_desc : public rnn_primitive_desc_base {
        primitive_desc() = default;

        primitive_desc(
                const desc &desc, const engine &e, bool allow_empty = false)
            : rnn_primitive_desc_base(
                    &desc.data, nullptr, e, nullptr, allow_empty) {}

        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e, bool allow_empty = false)
            : rnn_primitive_desc_base(
                    &desc.data, &attr, e, nullptr, allow_empty) {}

        /// Initializes a primitive descriptor for GRU forward propagation
        /// from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : rnn_primitive_desc_base(pd, dnnl::prop_kind::forward_training,
                    dnnl::prop_kind::forward_inference,
                    dnnl::algorithm::vanilla_gru) {}

        /// Queries source layer memory descriptor.
        memory::desc src_layer_desc() const {
            return query_md(query::src_md, 0);
        }

        /// Queries source iteration memory descriptor.
        ///
        /// Returns a zero_md if no src_iter was specified at op_desc
        /// creation time.
        memory::desc src_iter_desc() const {
            return query_md(query::src_md, 1);
        }

        /// Queries weights layer memory descriptor.
        memory::desc weights_layer_desc() const {
            return query_md(query::weights_md, 0);
        }

        /// Queries weights iteration memory descriptor.
        memory::desc weights_iter_desc() const {
            return query_md(query::weights_md, 1);
        }

        /// Queries bias memory descriptor.
        ///
        /// Returns a zero_md if no bias was specified at op_desc
        /// creation time.
        memory::desc bias_desc() const {
            return query_md(query::weights_md, 2);
        }

        /// Queries destination layer memory descriptor.
        memory::desc dst_layer_desc() const {
            return query_md(query::dst_md, 0);
        }

        /// Queries destination iteration memory descriptor.
        ///
        /// Returns a zero_md if no dst_iter was specified at op_desc
        /// creation time.
        memory::desc dst_iter_desc() const {
            return query_md(query::dst_md, 1);
        }

        /// Queries workspace memory descriptor.
        ///
        /// Returns a zero_md if no worspace is required.
        memory::desc workspace_desc() const {
            return query_md(query::workspace_md, 0);
        }
    };

    gru_forward() = default;

    gru_forward(const primitive_desc &pd) : primitive(pd) {}
};

/// GRU for backward propagation.
///
/// Implements descriptor, primitive descriptor, and primitive.
struct gru_backward : public primitive {

    /// GRU descriptor for backward propagation.
    struct desc {
        dnnl_rnn_desc_t data;

        /// Initializes an GRU descriptor for backward propagation using @p
        /// prop_kind, @p direction, and memory descriptors.
        ///
        /// @p flags is a parameter to the GRU descriptor and is currently
        /// ignored.
        ///
        /// @note All memory descriptors are allowed to be initialized with
        ///       #dnnl::memory::format_tag::any value of @p format_kind.
        ///
        /// @p src_iter_desc (simultaneously with @p diff_src_iter_desc), @p
        /// bias_desc (simultaneously with @p diff_bias_desc), and @p
        /// dst_iter_desc (simultaneously with @p diff_src_iter_desc) are
        /// allowed point to a zero memory descriptor, which would indicate
        /// that the GRU primitive should not use them and consider them to be
        /// zero values.
        desc(prop_kind aprop_kind, rnn_direction direction,
                const memory::desc &src_layer_desc,
                const memory::desc &src_iter_desc,
                const memory::desc &weights_layer_desc,
                const memory::desc &weights_iter_desc,
                const memory::desc &bias_desc,
                const memory::desc &dst_layer_desc,
                const memory::desc &dst_iter_desc,
                const memory::desc &diff_src_layer_desc,
                const memory::desc &diff_src_iter_desc,
                const memory::desc &diff_weights_layer_desc,
                const memory::desc &diff_weights_iter_desc,
                const memory::desc &diff_bias_desc,
                const memory::desc &diff_dst_layer_desc,
                const memory::desc &diff_dst_iter_desc,
                rnn_flags flags = rnn_flags::undef) {
            error::wrap_c_api(
                    dnnl_gru_backward_desc_init(&data,
                            dnnl::convert_to_c(aprop_kind),
                            dnnl::convert_to_c(direction), &src_layer_desc.data,
                            &src_iter_desc.data, &weights_layer_desc.data,
                            &weights_iter_desc.data, &bias_desc.data,
                            &dst_layer_desc.data, &dst_iter_desc.data,
                            &diff_src_layer_desc.data, &diff_src_iter_desc.data,
                            &diff_weights_layer_desc.data,
                            &diff_weights_iter_desc.data, &diff_bias_desc.data,
                            &diff_dst_layer_desc.data, &diff_dst_iter_desc.data,
                            dnnl::convert_to_c(flags)),
                    "could not create an GRU backward descriptor");
        }
    };

    /// Primitive descriptor for GRU backward propagation.
    struct primitive_desc : public rnn_primitive_desc_base {
        primitive_desc() = default;

        primitive_desc(const desc &desc, const engine &e,
                const gru_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : rnn_primitive_desc_base(
                    &desc.data, nullptr, e, hint_fwd_pd.get(), allow_empty) {}

        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e, const gru_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : rnn_primitive_desc_base(
                    &desc.data, &attr, e, hint_fwd_pd.get(), allow_empty) {}

        /// Initializes a primitive descriptor for GRU backward
        /// propagation from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : rnn_primitive_desc_base(pd, dnnl::prop_kind::backward,
                    dnnl::algorithm::vanilla_gru) {}

        /// Queries source layer memory descriptor.
        memory::desc src_layer_desc() const {
            return query_md(query::src_md, 0);
        }

        /// Queries source iter memory descriptor.
        ///
        /// Returns a zero_md if no src_iter was specified at op_desc
        /// creation time.
        memory::desc src_iter_desc() const {
            return query_md(query::src_md, 1);
        }

        /// Queries weights layer memory descriptor.
        memory::desc weights_layer_desc() const {
            return query_md(query::weights_md, 0);
        }

        /// Queries weights iteration memory descriptor.
        memory::desc weights_iter_desc() const {
            return query_md(query::weights_md, 1);
        }

        /// Queries bias memory descriptor.
        ///
        /// Returns a zero_md if no bias was specified at op_desc
        /// creation time.
        memory::desc bias_desc() const {
            return query_md(query::weights_md, 2);
        }

        /// Queries destination layer memory descriptor.
        memory::desc dst_layer_desc() const {
            return query_md(query::dst_md, 0);
        }

        /// Queries destination iteration memory descriptor.
        ///
        /// Returns a zero_md if no dst_iter was specified at op_desc
        /// creation time.
        memory::desc dst_iter_desc() const {
            return query_md(query::dst_md, 1);
        }

        /// Queries workspace memory descriptor.
        ///
        /// Returns a zero_md if no worspace is required.
        memory::desc workspace_desc() const {
            return query_md(query::workspace_md, 0);
        }

        /// Queries diff source layer memory descriptor.
        memory::desc diff_src_layer_desc() const {
            return query_md(query::diff_src_md, 0);
        }

        /// Queries diff source iteration memory descriptor.
        ///
        /// Returns a zero_md if no diff_src_iter was specified at op_desc
        /// creation time.
        memory::desc diff_src_iter_desc() const {
            return query_md(query::diff_src_md, 1);
        }

        /// Queries diff weights layer memory descriptor.
        memory::desc diff_weights_layer_desc() const {
            return query_md(query::diff_weights_md, 0);
        }

        /// Queries diff weights iteration memory descriptor.
        memory::desc diff_weights_iter_desc() const {
            return query_md(query::diff_weights_md, 1);
        }

        /// Queries diff bias memory descriptor.
        memory::desc diff_bias_desc() const {
            return query_md(query::diff_weights_md, 2);
        }

        /// Queries diff destination layer memory descriptor.
        memory::desc diff_dst_layer_desc() const {
            return query_md(query::diff_dst_md, 0);
        }

        /// Queries diff destination iteration memory descriptor.
        ///
        /// Returns a zero_md if no diff_dst_iter was specified at op_desc
        /// creation time.
        memory::desc diff_dst_iter_desc() const {
            return query_md(query::diff_dst_md, 1);
        }
    };

    gru_backward() = default;

    // With last iteration (with and without input src_iter)
    gru_backward(const primitive_desc &pd) : primitive(pd) {}
};

/// LBR_GRU for forward propagation.
///
/// Implements descriptor, primitive descriptor, and primitive.
struct lbr_gru_forward : public primitive {

    /// Descriptor for LBR GRU forward propagation.
    struct desc {
        dnnl_rnn_desc_t data;

        /// Initializes an LBR GRU descriptor for forward propagation using @p
        /// prop_kind, @p direction, and memory descriptors.
        /// @note If @p prop_kind equals #dnnl::forward_training, you must
        /// query a workspace memory descriptor before creating the primitive.
        ///
        /// @p flags is a parameter to the LBR GRU descriptor and is currently
        /// ignored.
        ///
        /// @p src_iter_desc, @p bias_desc, and @p dst_iter_desc are allowed
        /// to point to a zero memory descriptor, which would indicate that
        /// the LBR GRU primitive should not use them and will default to zero
        /// values.
        ///
        /// @note
        ///     All memory descriptors except @p src_iter_desc can be
        ///     initialized with an #dnnl::memory::format_tag::any value of @p
        ///     format_kind.
        desc(prop_kind aprop_kind, rnn_direction direction,
                const memory::desc &src_layer_desc,
                const memory::desc &src_iter_desc,
                const memory::desc &weights_layer_desc,
                const memory::desc &weights_iter_desc,
                const memory::desc &bias_desc,
                const memory::desc &dst_layer_desc,
                const memory::desc &dst_iter_desc,
                rnn_flags flags = rnn_flags::undef) {
            error::wrap_c_api(
                    dnnl_lbr_gru_forward_desc_init(&data,
                            dnnl::convert_to_c(aprop_kind),
                            dnnl::convert_to_c(direction), &src_layer_desc.data,
                            &src_iter_desc.data, &weights_layer_desc.data,
                            &weights_iter_desc.data, &bias_desc.data,
                            &dst_layer_desc.data, &dst_iter_desc.data,
                            dnnl::convert_to_c(flags)),
                    "could not create a Linear-before-reset GRU forward "
                    "descriptor");
        }
    };

    /// Primitive descriptor for LBR_GRU forward propagation.
    struct primitive_desc : public rnn_primitive_desc_base {
        primitive_desc() = default;

        primitive_desc(
                const desc &desc, const engine &e, bool allow_empty = false)
            : rnn_primitive_desc_base(
                    &desc.data, nullptr, e, nullptr, allow_empty) {}

        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e, bool allow_empty = false)
            : rnn_primitive_desc_base(
                    &desc.data, &attr, e, nullptr, allow_empty) {}

        /// Initializes a primitive descriptor for LBR GRU forward propagation
        /// from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : rnn_primitive_desc_base(pd, dnnl::prop_kind::forward_training,
                    dnnl::prop_kind::forward_inference,
                    dnnl::algorithm::lbr_gru) {}

        /// Queries source layer memory descriptor.
        memory::desc src_layer_desc() const {
            return query_md(query::src_md, 0);
        }

        /// Queries source iteration memory descriptor.
        ///
        /// Returns a zero_md if no src_iter was specified at op_desc
        /// creation time.
        memory::desc src_iter_desc() const {
            return query_md(query::src_md, 1);
        }

        /// Queries weights layer memory descriptor.
        memory::desc weights_layer_desc() const {
            return query_md(query::weights_md, 0);
        }

        /// Queries weights iteration memory descriptor.
        memory::desc weights_iter_desc() const {
            return query_md(query::weights_md, 1);
        }

        /// Queries bias memory descriptor.
        ///
        /// Returns a zero_md if no bias was specified at op_desc
        /// creation time.
        memory::desc bias_desc() const {
            return query_md(query::weights_md, 2);
        }

        /// Queries destination layer memory descriptor.
        memory::desc dst_layer_desc() const {
            return query_md(query::dst_md, 0);
        }

        /// Queries destination iteration memory descriptor.
        ///
        /// Returns a zero_md if no dst_iter was specified at op_desc
        /// creation time.
        memory::desc dst_iter_desc() const {
            return query_md(query::dst_md, 1);
        }

        /// Queries workspace memory descriptor.
        ///
        /// Returns a zero_md if no worspace is required.
        memory::desc workspace_desc() const {
            return query_md(query::workspace_md, 0);
        }
    };

    lbr_gru_forward() = default;

    lbr_gru_forward(const primitive_desc &pd) : primitive(pd) {}
};

/// LBR_GRU for backward propagation.
///
/// Implements descriptor, primitive descriptor, and primitive.
struct lbr_gru_backward : public primitive {

    /// LBR_GRU descriptor for backward propagation.
    struct desc {
        dnnl_rnn_desc_t data;

        /// Initializes an LBR_GRU descriptor for backward propagation using @p
        /// prop_kind, @p direction, and memory descriptors.
        ///
        /// @p flags is a parameter to the LBR GRU descriptor and is currently
        /// ignored.
        ///
        /// @note All memory descriptors are allowed to be initialized with
        ///       #dnnl::memory::format_tag::any value of @p format_kind.
        ///
        /// @p src_iter_desc (simultaneously with @p diff_src_iter_desc), @p
        /// bias_desc (simultaneously with @p diff_bias_desc), and @p
        /// dst_iter_desc (simultaneously with @p diff_src_iter_desc) are
        /// allowed point to a zero memory descriptor, which would indicate
        /// that the LBR GRU primitive should not use them and consider them to be
        /// zero values.
        desc(prop_kind aprop_kind, rnn_direction direction,
                const memory::desc &src_layer_desc,
                const memory::desc &src_iter_desc,
                const memory::desc &weights_layer_desc,
                const memory::desc &weights_iter_desc,
                const memory::desc &bias_desc,
                const memory::desc &dst_layer_desc,
                const memory::desc &dst_iter_desc,
                const memory::desc &diff_src_layer_desc,
                const memory::desc &diff_src_iter_desc,
                const memory::desc &diff_weights_layer_desc,
                const memory::desc &diff_weights_iter_desc,
                const memory::desc &diff_bias_desc,
                const memory::desc &diff_dst_layer_desc,
                const memory::desc &diff_dst_iter_desc,
                rnn_flags flags = rnn_flags::undef) {
            error::wrap_c_api(
                    dnnl_lbr_gru_backward_desc_init(&data,
                            dnnl::convert_to_c(aprop_kind),
                            dnnl::convert_to_c(direction), &src_layer_desc.data,
                            &src_iter_desc.data, &weights_layer_desc.data,
                            &weights_iter_desc.data, &bias_desc.data,
                            &dst_layer_desc.data, &dst_iter_desc.data,
                            &diff_src_layer_desc.data, &diff_src_iter_desc.data,
                            &diff_weights_layer_desc.data,
                            &diff_weights_iter_desc.data, &diff_bias_desc.data,
                            &diff_dst_layer_desc.data, &diff_dst_iter_desc.data,
                            dnnl::convert_to_c(flags)),
                    "could not create an LBR_GRU backward descriptor");
        }
    };

    /// Primitive descriptor for LBR_GRU backward propagation.
    struct primitive_desc : public rnn_primitive_desc_base {
        primitive_desc() = default;

        primitive_desc(const desc &desc, const engine &e,
                const lbr_gru_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : rnn_primitive_desc_base(
                    &desc.data, nullptr, e, hint_fwd_pd.get(), allow_empty) {}

        primitive_desc(const desc &desc, const primitive_attr &attr,
                const engine &e,
                const lbr_gru_forward::primitive_desc &hint_fwd_pd,
                bool allow_empty = false)
            : rnn_primitive_desc_base(
                    &desc.data, &attr, e, hint_fwd_pd.get(), allow_empty) {}

        /// Initializes a primitive descriptor for LBR GRU backward
        /// propagation from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : rnn_primitive_desc_base(
                    pd, dnnl::prop_kind::backward, dnnl::algorithm::lbr_gru) {}

        /// Queries source layer memory descriptor.
        memory::desc src_layer_desc() const {
            return query_md(query::src_md, 0);
        }

        /// Queries source iteration memory descriptor.
        ///
        /// Returns a zero_md if no src_iter was specified at op_desc
        /// creation time.
        memory::desc src_iter_desc() const {
            return query_md(query::src_md, 1);
        }

        /// Queries weights layer memory descriptor.
        memory::desc weights_layer_desc() const {
            return query_md(query::weights_md, 0);
        }

        /// Queries weights iteration memory descriptor.
        memory::desc weights_iter_desc() const {
            return query_md(query::weights_md, 1);
        }

        /// Queries bias memory descriptor.
        ///
        /// Returns a zero_md if no bias was specified at op_desc
        /// creation time.
        memory::desc bias_desc() const {
            return query_md(query::weights_md, 2);
        }

        /// Queries destination layer memory descriptor.
        memory::desc dst_layer_desc() const {
            return query_md(query::dst_md, 0);
        }

        /// Queries destination iteration memory descriptor.
        ///
        /// Returns a zero_md if no dst_iter was specified at op_desc
        /// creation time.
        memory::desc dst_iter_desc() const {
            return query_md(query::dst_md, 1);
        }

        /// Queries workspace memory descriptor.
        ///
        /// Returns a zero_md if no worspace is required.
        memory::desc workspace_desc() const {
            return query_md(query::workspace_md, 0);
        }

        /// Queries diff source layer memory descriptor.
        memory::desc diff_src_layer_desc() const {
            return query_md(query::diff_src_md, 0);
        }

        /// Queries diff source iteration memory descriptor.
        ///
        /// Returns a zero_md if no diff_src_iter was specified at op_desc
        /// creation time.
        memory::desc diff_src_iter_desc() const {
            return query_md(query::diff_src_md, 1);
        }

        /// Queries diff weights layer memory descriptor.
        memory::desc diff_weights_layer_desc() const {
            return query_md(query::diff_weights_md, 0);
        }

        /// Queries diff weights iteration memory descriptor.
        memory::desc diff_weights_iter_desc() const {
            return query_md(query::diff_weights_md, 1);
        }

        /// Queries diff bias memory descriptor.
        memory::desc diff_bias_desc() const {
            return query_md(query::diff_weights_md, 2);
        }

        /// Queries diff destination layer memory descriptor.
        memory::desc diff_dst_layer_desc() const {
            return query_md(query::diff_dst_md, 0);
        }

        /// Queries diff destination iteration memory descriptor.
        ///
        /// Returns a zero_md if no diff_dst_iter was specified at op_desc
        /// creation time.
        memory::desc diff_dst_iter_desc() const {
            return query_md(query::diff_dst_md, 1);
        }
    };

    lbr_gru_backward() = default;

    lbr_gru_backward(const primitive_desc &pd) : primitive(pd) {}
};

/// @}

/// @addtogroup cpp_api_shuffle Shuffle
/// A primitive to shuffle data along the axis.
///
/// @sa @ref dev_guide_shuffle in developer guide
/// @sa @ref c_api_shuffle in @ref c_api
/// @{

/// Shuffle for forward propagation.  Implements descriptor, primitive
/// descriptor, and primitive.
struct shuffle_forward : public primitive {

    /// Descriptor for shuffle forward propagation.
    struct desc {
        dnnl_shuffle_desc_t data;

        /// Initializes a shuffle descriptor for forward propagation using @p
        /// prop_kind, memory descriptor @p data_desc, @p axis, and @p
        /// group_size.
        desc(prop_kind aprop_kind, const memory::desc &data_desc, int axis,
                int group_size) {
            error::wrap_c_api(dnnl_shuffle_forward_desc_init(&data,
                                      dnnl::convert_to_c(aprop_kind),
                                      &data_desc.data, axis, group_size),
                    "could not create a shuffle forward descriptor");
        }
    };

    /// Primitive descriptor for shuffle forward propagation.
    struct primitive_desc : public dnnl::primitive_desc {
        primitive_desc() = default;

        primitive_desc(const desc &desc, const engine &e,
                const primitive_attr &aattr = primitive_attr(),
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, &aattr, e, nullptr, allow_empty) {}

        /// Initializes a primitive descriptor for shuffle forward propagation
        /// from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : dnnl::primitive_desc(pd, dnnl::primitive::kind::shuffle,
                    dnnl::prop_kind::forward_training,
                    dnnl::prop_kind::forward_inference) {}

        /// Queries source memory descriptor.
        memory::desc src_desc() const { return query_md(query::src_md, 0); }

        /// Queries destination memory descriptor.
        memory::desc dst_desc() const { return query_md(query::dst_md, 0); }
    };

    shuffle_forward() = default;

    shuffle_forward(const primitive_desc &pd) : primitive(pd) {}
};

/// Shuffle for backward propagation.  Implements descriptor, primitive
/// descriptor, and primitive.
struct shuffle_backward : public primitive {

    // Descriptor for shuffle backward propagation.
    struct desc {
        dnnl_shuffle_desc_t data;

        /// Initializes a shuffle descriptor for backward propagation using
        /// memory descriptor @p diff_data_desc, @p axis, and @p group_size.
        desc(const memory::desc &diff_data_desc, int axis, int group_size) {
            error::wrap_c_api(dnnl_shuffle_backward_desc_init(&data,
                                      &diff_data_desc.data, axis, group_size),
                    "could not create a shuffle backward descriptor");
        }
    };

    // Primitive descriptor for shuffle backward propagation.
    struct primitive_desc : public dnnl::primitive_desc {
        primitive_desc() = default;

        primitive_desc(const desc &desc, const engine &e,
                const shuffle_forward::primitive_desc &hint_fwd_pd,
                const primitive_attr &aattr = primitive_attr(),
                bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, &aattr, e, hint_fwd_pd.get(), allow_empty) {}

        /// Initializes a primitive descriptor for shuffle backward
        /// propagation from a C primitive descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : dnnl::primitive_desc(pd, dnnl::primitive::kind::shuffle,
                    dnnl::prop_kind::backward_data) {}

        /// Queries diff source gradient memory descriptor.
        memory::desc diff_src_desc() const {
            return query_md(query::diff_src_md, 0);
        }

        /// Queries diff destination memory descriptor.
        memory::desc diff_dst_desc() const {
            return query_md(query::diff_dst_md, 0);
        }
    };

    shuffle_backward() = default;

    shuffle_backward(const primitive_desc &pd) : primitive(pd) {}
};

/// @}

/// @addtogroup cpp_api_binary Binary
/// A primitive to perform tensor operations over two tensors.
///
/// @sa @ref dev_guide_binary in developer guide
/// @sa @ref c_api_binary in @ref c_api
/// @{

/// Implements descriptor, primitive descriptor, and primitive
/// for the binary.
struct binary : public primitive {

    /// Descriptor for binary.
    struct desc {
        dnnl_binary_desc_t data;

        /// Initializes a binary descriptor using @p algorithm, memory
        /// descriptors @p src0_desc, @p src1_desc and @p dst_desc.
        desc(algorithm aalgorithm, const memory::desc &src0,
                const memory::desc &src1, const memory::desc &dst) {
            error::wrap_c_api(
                    dnnl_binary_desc_init(&data, dnnl::convert_to_c(aalgorithm),
                            &src0.data, &src1.data, &dst.data),
                    "could not create a binary descriptor");
        }
    };

    struct primitive_desc : public dnnl::primitive_desc {
        primitive_desc() = default;

        /// Initializes a primitive descriptor for binary.
        primitive_desc(
                const desc &desc, const engine &e, bool allow_empty = false)
            : dnnl::primitive_desc(
                    &desc.data, nullptr, e, nullptr, allow_empty) {}

        /// Initializes a primitive descriptor for binary with attributes
        /// defined by @p attr.
        primitive_desc(
                const desc &desc, const primitive_attr &attr, const engine &e)
            : dnnl::primitive_desc(&desc.data, &attr, e, nullptr) {}

        /// Initializes a primitive descriptor for binary from a C primitive
        /// descriptor @p pd.
        primitive_desc(dnnl_primitive_desc_t pd)
            : dnnl::primitive_desc(pd, dnnl::primitive::kind::binary) {}

        /// Queries source 0 memory descriptor.
        memory::desc src0_desc() const { return query_md(query::src_md, 0); }

        /// Queries source 1 memory descriptor.
        memory::desc src1_desc() const { return query_md(query::src_md, 1); }

        /// Queries destination memory descriptor.
        memory::desc dst_desc() const { return query_md(query::dst_md, 0); }
    };

    binary() = default;

    binary(const primitive_desc &pd) : primitive(pd) {}
};

/// @}

/// @} Primitives

/// @} C++ API

// implementation section

/// @cond DO_NOT_DOCUMENT_THIS
inline primitive::primitive(const_dnnl_primitive_desc_t c_pd) {
    dnnl_primitive_t result;
    error::wrap_c_api(dnnl_primitive_create(&result, c_pd),
            "could not create a primitive");
    reset(result);
}

inline primitive::primitive(const primitive_desc &pd) : primitive(pd.get()) {}

inline void primitive::execute(
        stream &astream, const std::unordered_map<int, memory> &args) const {
    std::vector<dnnl_exec_arg_t> c_args;
    c_args.reserve(args.size());
    for (const auto &a : args)
        c_args.push_back({a.first, a.second.get()});

    error::wrap_c_api(dnnl_primitive_execute(get(), astream.get(),
                              (int)c_args.size(), c_args.data()),
            "could not execute a primitive");
}
/// @endcond

} // namespace dnnl

#endif
