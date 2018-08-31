/*
// Copyright (c) 2016 Intel Corporation
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
*/

///////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef CLDNN_H
#define CLDNN_H

// exporting symbols form dynamic library
#ifdef EXPORT_NEURAL_SYMBOLS
#   if defined(_MSC_VER)
//  Microsoft
#      define CLDNN_API __declspec(dllexport)
#   elif defined(__GNUC__)
//  GCC
#      define CLDNN_API __attribute__((visibility("default")))
#   else
#      define CLDNN_API
#      pragma warning Unknown dynamic link import/export semantics.
#   endif
#else //import dll
#   if defined(_MSC_VER)
//  Microsoft
#      define CLDNN_API __declspec(dllimport)
#   elif defined(__GNUC__)
//  GCC
#      define CLDNN_API
#   else
#      define CLDNN_API
#      pragma warning Unknown dynamic link import/export semantics.
#   endif
#endif

#include <stdint.h>
#include <stddef.h>

/// @addtogroup c_api C API
/// @{

/// @defgroup c_memory Memory Management

/// @defgroup c_topology Network Topology

/// @defgroup c_engine Execution Engine

/// @defgroup c_network Network Execution

/// @defgroup c_error Error Handling

/// @defgroup c_version Version Information

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup c_error
/// @{
#define CLDNN_SUCCESS                0
#define CLDNN_ERROR                 -1
#define CLDNN_INVALID_ARG           -2
#define CLDNN_OUT_OF_RESOURCES      -3
#define CLDNN_DEVICE_ERROR          -4
#define CLDNN_UNSUPPORTED_SIZE      -5
#define CLDNN_UNSUPPORTED_FORMAT    -6
#define CLDNN_DIMENSION_MISMATCH    -7
#define CLDNN_ALLOC_SIZE_EXCEEDED   -8
#define CLDNN_GLOBAL_SIZE_EXCEEDED  -9

/// @brief Represents errors status for all API calls
typedef int32_t cldnn_status;
/// @}

/// @addtogroup c_version
/// @{
/// @brief Represents version information of API.
typedef struct
{
    int32_t major;    ///< Major version component (major version of clDNN API interface).
    int32_t minor;    ///< Minor version component (minor version of API interface - correlated with IE API version).
    int32_t build;    ///< Build version component (version/revision of official Open Source drop of clDNN library).
    int32_t revision; ///< Revision version component (incremental identifier of current build/compilation).
} cldnn_version;
/// @}

/// @ingroup c_engine
/// @brief Engine object
typedef struct cldnn_engine_impl* cldnn_engine;

/// @ingroup c_network
/// @brief Event object
typedef struct cldnn_event_impl* cldnn_event;

/// @ingroup c_topology
/// @brief Network topology to be defined by user
typedef struct cldnn_topology_impl* cldnn_topology;

/// @ingroup c_program
/// @brief Compiled program build from @ref cldnn_topology by @ref cldnn_engine
typedef struct cldnn_program_impl* cldnn_program;

/// @ingroup c_network
/// @brief Executable network allocated from @ref cldnn_program
typedef struct cldnn_network_impl* cldnn_network;

/// @ingroup c_memory
/// @brief Memory object
typedef struct cldnn_memory_impl* cldnn_memory;

/// @addtogroup c_engine
/// @{

/// @brief Defines available engine types
typedef enum /*:int32_t*/
{
    cldnn_engine_ocl ///< OpenCL engine
} cldnn_engine_type;

/// @brief Priority modes.
typedef enum /*:int16_t*/
{
    cldnn_priority_disabled,
    cldnn_priority_low,
    cldnn_priority_med,
    cldnn_priority_high
} cldnn_priority_mode_type;

/// @brief Throttle modes.
typedef enum /*:int16_t*/
{
    cldnn_throttle_disabled,
    cldnn_throttle_low,
    cldnn_throttle_med,
    cldnn_throttle_high
} cldnn_throttle_mode_type;

/// @brief Configuration parameters for created engine.
typedef struct
{
    uint32_t enable_profiling;                          ///< Enable per-primitive profiling.
    uint32_t meaningful_kernels_names;                  ///< Generate meaniful names fo OpenCL kernels.
    uint32_t dump_custom_program;                       ///< dump the custom generated program to files 
    const char* compiler_options;                       ///< OpenCL compiler options string.
    const char* single_kernel_name;                     ///< If provided, runs specific layer.
    uint32_t enable_parallelisation;                    ///< Enables parallel execution of primitives which don't depend on each other. Disabled by default.
    const char* engine_log;                             ///< Specifies a file to which engine log should be dumped. Null/empty values means no logging.
    const char* sources_dumps_dir;                      ///< Specifies a directory where sources of cldnn::program objects should be dumped. Null/empty values means no loggins.
    /*cldnn_priority_mode_type*/ int16_t priority_mode; ///< Priority mode (support of OpenCL priority hints in command queue).
    /*cldnn_throttle_mode_type*/ int16_t throttle_mode; ///< Placeholder for throttle mode (support of throttle hints in command queue). It has no effect for now and should be set to cldnn_throttle_disabled.
    uint32_t enable_memory_pool;                        ///< Enables memory usage optimization. memory objects will be reused when possible. 
}  cldnn_engine_configuration;

/// @brief Information about the engine returned by cldnn_get_engine_info().
typedef struct
{
    uint32_t cores_count;              ///< Number of available HW cores.
    uint32_t core_frequency;           ///< Clock frequency in MHz.

    uint64_t max_work_group_size;      ///< Maximum number of work-items in a work-group executing a kernel using the data parallel execution model.
    uint64_t max_local_mem_size;       ///< Maximum size of local memory arena in bytes.
    uint64_t max_global_mem_size;      ///< Maximum size of global device memory in bytes.
    uint64_t max_alloc_mem_size;       ///< Maximum size of memory object allocation in bytes.

    uint64_t max_image2d_width;        ///< Maximum image 2d width supported by the device.
    uint64_t max_image2d_height;       ///< Maximum image 2d height supported by the device.

    // Flags (for layout compatibility fixed size types are used).
    uint8_t supports_fp16;             ///< Does engine support FP16.
    uint8_t supports_fp16_denorms;     ///< Does engine support denormalized FP16.
    uint8_t supports_subgroups_short;  ///< Does engine support cl_intel_subgroups_short.
    uint8_t supports_image;           ///< Does engine support images (CL_DEVICE_IMAGE_SUPPORT cap).
}  cldnn_engine_info;
/// @}

/// @addtogroup c_network
/// @{

/// @brief user-defined event handler callback.
typedef void(*cldnn_event_handler)(void*);

/// @brief Profiling information for an executed network primitive.
/// @details Every @ref cldnn_event associated with @ref cldnn_network_output.
/// can contain one or more profiling information intervals.
typedef struct
{
    const char* name;                   ///< Profiling interval name.
    uint64_t nanoseconds;
} cldnn_profiling_interval;

/// @brief Network build option types.
typedef enum /*:int32_t*/
{
    cldnn_build_option_fusing,                  ///< Allow primitives fusing during network build.
    cldnn_build_option_optimize_data,           ///< Enable implicit reordering for user input.
    cldnn_build_option_debug,                   ///< Enable debug mode.
    cldnn_build_option_outputs,                 ///< User selected list of network outputs.
	cldnn_build_option_learning_config,         ///< User defined learning parameters.
    cldnn_build_option_tuning_config,           ///< Tuning config.
    cldnn_build_option_graph_dumps_dir,         ///< Specifies a directory to which stages of network compilation should be dumped.
    cldnn_build_option_serialization,           ///< Specifies a name of files to which serialization should be dumped.
    cldnn_build_option_load_program             ///< Specifies a name of load_program process.
} cldnn_build_option_type;

/// @brief Tuning modes.
typedef enum /*:int32_t*/
{
    cldnn_tuning_disabled,          ///< Tuning is disabled.
    cldnn_tuning_use_cache,         ///< Tuning using the cached data (no on-line tuning for non-existing data).
    cldnn_tuning_tune_and_cache,    ///< Tuning using the cached data if exist, tune and update cache otherwise.
} cldnn_tuning_mode_type;

/// @brief Tuning config.
struct cldnn_tuning_config
{
    const int32_t mode;             ///< #cldnn_tuning_mode_type.
    const char* cache_file_path;    ///< A path to the tuning cache file.
};

/// @brief Learning params.
struct cldnn_learning_params
{
	const float momentum;
	const float weights_decay;
};

/// @brief Represents network build option.
typedef struct
{
    int32_t type;                       ///< #cldnn_build_option_type.
    const void* data;                   ///< option parameter - e.g list of outputs.
}  cldnn_build_option;

/// @brief Output information for executed @a cldnn_network.
/// @details User should wait for event before accessing the memory.
typedef struct
{
    cldnn_event event;                  ///< Event to be waited.
    cldnn_memory memory;                ///< Output memory.
                                        ///< User should wait for the event before access this field.
} cldnn_network_output;

/// @}

/// @addtogroup c_memory
/// @{

/// @brief Represents memory formats (orders).
/// @n In CNN most of data is describe as 4 dimensional blocks. In Intel(R) clDNN library we describe memory with 4 letters
/// - b - number of blocks in batch. For weights formats: output features - conv, neurons - inner product
/// - f - number of feature maps, features or channels. For weights formats: input features - conv, inputs, inner product
/// - x - spatial, width
/// - y - spatial, height
/// /n
/// For explanation how each format type is implemented in memory we will use naming shown bellow (b=2,f=3,y=3,x=3):
/// \image html layout_memory_representation.jpg
typedef enum /*:int32_t*/
{
    cldnn_format_yxfb,          ///< batch first, feature and than spatials \n \image html yxfb.jpg
    cldnn_format_byxf,          ///< used in bitmaps, input from user i.e b images of RGB format \n \image html byxf.jpg
    cldnn_format_bfyx,          ///< the most common format for activations in clDNN. \n \image html bfyx.jpg
    cldnn_format_fyxb,          ///< format not used inside clDNN, but supported in reorder as extension for user provided formats.
    cldnn_format_os_iyx_osv16,  ///< format used only for convolution weights: os - output feature maps slice, i - input feature maps, yx - spatials, sv16 - 16 values of single slice.
                                ///< \n \image html os_iyx_osv16.jpg
    cldnn_format_bs_xs_xsv8_bsv8, ///< format used only for fully connected weights: bs - batch slice, xs - x slice, bsv8 - 8 values of single slice.
                                  ///< \n \image html bs_xs_xsv8_bsv8.jpg
    cldnn_format_bs_xs_xsv8_bsv16,///< format used only for fully connected weights: bs - batch slice, xs - x slice, bsv16 - 16 values of single slice.
                                  ///< \n \image html bs_xs_xsv8_bsv16.jpg
    cldnn_format_bs_x_bsv16,    ///< format used only for fully connected weights fp16 batch=1 : bs - batch slice (responses slice), bsv16 - 16 values of single batch slice, x - flattened plane of (fyx).
                                ///< \n \image html bs_x_bsv16.jpg
    cldnn_format_bf8_xy16,      ///< format used only for convolution 1x1 input, xy aligned to 16, f aligned to 8
                                ///< \n \image html bf8_xy16.jpg
    cldnn_format_image_2d_weights_c4_fyx_b, ///< image format for weights, image 2d, 4-channel, width size is f*y*x/4 (4-channels filled with fyx data), height is b
                                      ///< \n \image html image_2d_weights_c4_fyx_b.jpg
    cldnn_format_image_2d_weights_c1_b_fyx, ///< image format for weights, image 2d, single channel, width size is b, height is f*y*x
                                      ///< \n \image html image_2d_weights_c1_b_fyx.jpg
    cldnn_format_byxf_af32,           /// < \n format for input for primitives using MMAD
    cldnn_format_os_is_yx_isa8_osv8_isv4, /// < \n format for weights for MMAD convolutions, stored as ((aligned_to_8(O)/8) * (aligned_to_32(I)/32) * Y * X * ( 8 ) * ( 8 ) * ( 4 )
    cldnn_format_format_num,    ///< number of format types
    cldnn_format_any = -1
} cldnn_format_type;

#define CLDNN_FLOAT_TYPE_MASK 0x80
#define CLDNN_UINT_TYPE_MASK 0x40

#define CLDNN_TENSOR_BATCH_DIM_MAX 1
#define CLDNN_TENSOR_FEATURE_DIM_MAX 1
#define CLDNN_TENSOR_SPATIAL_DIM_MAX 2
#define CLDNN_TENSOR_DIM_MAX 8

/// @brief N-dimensional vector. Mostly used to represent memory size.
typedef struct
{
    size_t batch_num;
    size_t feature_num;
    size_t spatial_num;
    int32_t sizes[CLDNN_TENSOR_DIM_MAX];
} cldnn_tensor;

/// @brief Padding information.
typedef struct
{
    cldnn_tensor lower_size; ///< Lower padding sizes. For spatials, it means size of left (X) and top (Y) padding.
    cldnn_tensor upper_size; ///< Upper padding sizes. For spatials, it means size of right (X) and bottom (Y) padding.
    float filling_value;     ///< Filling value for an element of padding. If data type of elements is different than float it is converted
                             ///< to it using round-towards-nearest-even (for floating-point data types) or round-towards-zero (for integral
                             ///< data types).
} cldnn_padding;

/// @brief Data type stored in memory.
typedef enum /*:size_t*/
{
	cldnn_i8  = sizeof(int8_t),
    cldnn_f16 = sizeof(int16_t) | CLDNN_FLOAT_TYPE_MASK,
    cldnn_f32 = sizeof(float) | CLDNN_FLOAT_TYPE_MASK,
    cldnn_u8  = sizeof(uint8_t) | CLDNN_UINT_TYPE_MASK // TODO: move to top of list and re-compile inference engine

} cldnn_data_type;

/// @brief Memory layout description.
typedef struct
{
    size_t data_type;       ///< data type (@ref cldnn_data_type) stored in memory.
    int32_t format;         ///< Memor format (@ref cldnn_format_type)
    cldnn_tensor size;      ///< N-dimensional vector describes size (in elements) of memory (excluding padding).
    cldnn_padding padding;  ///< Explicitly added padding to memory buffer.
} cldnn_layout;
/// @}

/// @addtogroup c_topology
/// @{

/// @brief Represents reference to an array of floats.
typedef struct
{
    const float* data; ///< Pointer to float array.
    size_t size;       ///< Size (in floats) of the array.
} cldnn_float_arr;

/// @brief Represents reference to an array of uint16_t.
typedef struct
{
    const uint16_t* data; ///< Pointer to uint16_t array.
    size_t size;       ///< Size (in uint16_t) of the array.
} cldnn_uint16_t_arr;

/// @brief Represents reference to an array of tensor.
typedef struct
{
    const cldnn_tensor* data; ///< Pointer to tensor array.
    size_t size;       ///< Size (in tensor) of the array.
} cldnn_tensor_arr;

/// @brief Globally unique primitive's type id
typedef const struct cldnn_primitive_type* cldnn_primitive_type_id;

/// @brief Unique @p id of a primitive within a topology.
typedef const char* cldnn_primitive_id;

/// @brief Represents reference to an array of primitive ids.
typedef struct
{
    const cldnn_primitive_id* data; ///< Pointer to ids array.
    size_t size;                    ///< Number of ids in the array.
} cldnn_primitive_id_arr;

/// @brief Custom primitive kernel source code
typedef const char*  cldnn_kernel_code;
/// @brief Custom primitive kernel source code array
typedef cldnn_kernel_code* cldnn_kernels_code;
/// @brief Custom primitive kernel entry point
typedef const char*  cldnn_kernel_entry_point;
/// @brief Custom primitive kernel build options
typedef const char*  cldnn_kernel_build_options;
/// @brief Custom primitive kernel workgroup sizes
typedef const size_t*  cldnn_work_group_sizes;

/// @brief Custom primitive kernel argument type
typedef enum cldnn_arg_type_t
{
    arg_input,
    arg_output,
} cldnn_arg_type;

/// @brief Custom primitive kernel argument index
typedef uint32_t cldnn_arg_index;

/// @brief Custom primitive kernel argument type
typedef struct cldnn_arg_t
{
    cldnn_arg_type arg_type;
    cldnn_arg_index index;
} cldnn_arg;

/// @brief Custom primitive kernel argument array
typedef const cldnn_arg* cldnn_kernel_arguments;

/// @brief activation functions
typedef enum cldnn_activation_func_t
{
    activation_none,                    // val
    activation_logistic,                // 1/(1 + exp(-val))
    activation_hyperbolic_tan,          // tanh(val)
    activation_relu,                    // max(0, val)
    activation_relu_negative_slope,     // max(0, val) + a * min(0, val)    (a is additional param)
    activation_clamp,                   // max(a, min(b, val)               (a,b are additional param)
    activation_softrelu,                // log(1 + exp(val))
    activation_abs,                     // abs(val)
    activation_linear,                  // a*val + b                        (a,b are additional params) 
    activation_square,                  // val*val
    activation_sqrt,                    // sqrt(val)
    activation_elu,                     // max(0, val) + a * (exp(min(0, val) - 1) (a is additional param)
} cldnn_activation_func;

/// @brief activation gradient functions
typedef enum cldnn_activation_grad_func_t
{
    activation_grad_none,                    // val
    activation_grad_relu,                    // val * (input > 0)
    activation_grad_relu_negative_slope,     // val * ((input > 0) + a * (input <= 0)    (a is additional param)
} cldnn_activation_grad_func;

/// @brief activation additional params
typedef struct cldnn_activation_additional_params_t
{
    float a, b;
} cldnn_activation_additional_params;


/// @brief reorder mean operation modes
typedef enum cldnn_reorder_mean_mode_t
{
    mean_none,                    // val
    mean_subtract,                // val - mean
    mean_mul,                     // val * mean
    mean_div,                     // val/mean
} cldnn_reorder_mean_mode;

/// @brief Begin primitive description definition
/// @details Defines @p 'cldnn_primitive_type_desc' structure with first 5 fields
/// common for all primitive descriptors. Other fields should be added after this macro.
/// primitive descriptor definition should be closed by @ref CLDNN_END_PRIMITIVE_DESC.
#define CLDNN_BEGIN_PRIMITIVE_DESC(PType) struct cldnn_##PType##_desc {\
    cldnn_primitive_type_id type; /**< @brief Primitive type identificator. */\
    cldnn_primitive_id id;        /**< @brief Primitive id unique within a topology. */\
    cldnn_primitive_id_arr input; /**< @brief Input primitives ids. */\
    cldnn_padding output_padding; /**< @brief Output padding information. */

/// @brief Close primitive descriptor definition.
#define CLDNN_END_PRIMITIVE_DESC(PType) };

#define CLDNN_PRIMITIVE_DESC(PType) cldnn_##PType##_desc

/// @brief Basic primitive descriptor structure.
CLDNN_BEGIN_PRIMITIVE_DESC(primitive)
CLDNN_END_PRIMITIVE_DESC(primitive)

/// @}

/// @addtogroup c_version
/// @{
/// @brief Get information about version of clDNN.
CLDNN_API cldnn_version cldnn_get_version(cldnn_status* status);
/// @}

/// @addtogroup c_topology
/// @{

/// @brief Create empty network topology
CLDNN_API cldnn_topology cldnn_create_topology(cldnn_status* status);

/// @brief Add new primitive to the topology.
/// @param[in] dto The pointer to a structure defined by @ref CLDNN_BEGIN_PRIMITIVE_DESC and @ref CLDNN_END_PRIMITIVE_DESC
CLDNN_API void cldnn_add_primitive(cldnn_topology topology, const struct CLDNN_PRIMITIVE_DESC(primitive)* dto, cldnn_status* status);

/// @brief Change input layout of the topology.
/// @param[in] id of the input layout in the topology
/// @param[in] new_layout of the input layout
CLDNN_API void cldnn_change_input_layout(cldnn_topology topology, cldnn_primitive_id id, cldnn_layout new_layout, cldnn_status* status);

/// @brief Return all primitives id from topology.
/// @details Function fills user provided buffer by primitive ids. Each id is followed by '\0'.
/// @param[in] ids Pointer to user-allocated buffer to store names.
/// @param[in] size Size (in chars) of the buffer.
/// @param[out] size_ret Required size (in chars) to store result.
CLDNN_API void cldnn_get_primitive_ids(cldnn_topology topology, char* ids, size_t size, size_t* size_ret, cldnn_status* status);

/// @brief Increment reference counter for the topology object.
CLDNN_API void cldnn_retain_topology(cldnn_topology topology, cldnn_status* status);

/// @brief Decrement reference counter for the topology object. Deletes object when counter becomes zero.
CLDNN_API void cldnn_release_topology(cldnn_topology topology, cldnn_status* status);
/// @}

/// @addtogroup c_engine
/// @{

/// @brief number of available engines of the particular type
CLDNN_API uint32_t cldnn_get_engine_count(/*cldnn_engine_type*/ int32_t type, cldnn_status* status);

/// @brief Release pending memory allocated in OpenCL context.
/// @param[in] type Engine type @ref cldnn_engine_type. Only OCL engine is supported.
/// @details OpenCL does not guarantee that the memory will be released (even with cl:Buffers releaed).
/// Use this function to force releasing whole pending memory.
CLDNN_API void cldnn_release_pending_memory(cldnn_engine engine, cldnn_status* status);

/// @brief Create new engine of the specified @p type, @p engine_num, and @p configuration options.
/// @param[in] type Engine type @ref cldnn_engine_type. Only OCL engine is supported.
/// @param[in] engine_num Engine index. Should be 0.
/// @param[in] configuration Pointer to engine configuration options.
CLDNN_API cldnn_engine cldnn_create_engine(/*cldnn_engine_type*/ int32_t type, uint32_t engine_num, const cldnn_engine_configuration* configuration, cldnn_status* status);

/// @brief Increment reference counter for the engine object.
CLDNN_API void cldnn_retain_engine(cldnn_engine engine, cldnn_status* status);

/// @brief Decrement reference counter for the engine object. Deletes object when counter becomes zero.
CLDNN_API void cldnn_release_engine(cldnn_engine engine, cldnn_status* status);

/// @brief Returns engine information. See @ref cldnn_engine_info for details.
CLDNN_API cldnn_engine_info cldnn_get_engine_info(cldnn_engine engine, cldnn_status* status);

/// @brief Returns the @ref cldnn_engine_type for the particular engine
CLDNN_API /*cldnn_engine_type*/ int32_t cldnn_get_engine_type(cldnn_engine engine, cldnn_status* status);

/// @brief Returns total size of all resources allocated using given engine
CLDNN_API int64_t cldnn_get_temp_used_device_memory_size(cldnn_engine engine, cldnn_status* status);
/// @}

/// @brief Returns max size of resources allocated using given engine
CLDNN_API int64_t cldnn_get_max_used_device_memory_size(cldnn_engine engine, cldnn_status* status);

/// @addtogroup c_network
/// @{

/// @brief Creates an event which can be set by user.
CLDNN_API cldnn_event cldnn_create_user_event(cldnn_engine engine, cldnn_status* status);

/// @brief Checks if an event was created by user.
CLDNN_API int32_t cldnn_is_user_event(cldnn_event event, cldnn_status* status);

/// @brief Increment reference counter for the event object.
CLDNN_API void cldnn_retain_event(cldnn_event event, cldnn_status* status);

/// @brief Decrement reference counter for the event object. Deletes object when counter becomes zero.
CLDNN_API void cldnn_release_event(cldnn_event event, cldnn_status* status);

/// @brief Waits for event completion or error.
CLDNN_API void cldnn_wait_for_event(cldnn_event event, cldnn_status* status);

/// @brief Set event status to @p completed.
CLDNN_API void cldnn_set_event(cldnn_event event, cldnn_status* status);

/// @brief Register call back to be called on event completion.
/// @param[in] handler Pointer to @ref cldnn_event_handler call-back function.
/// @param[in] param user-defined value to be passed to the call back function.
CLDNN_API void cldnn_add_event_handler(cldnn_event event, cldnn_event_handler handler, void* param, cldnn_status* status);

/// @brief Returns the profiling information for an network primitive associated with event.
/// @param[in] profiling Pointer to the array of @ref cldnn_profiling_interval where information to be stored.
/// @param[in] size Number of elements in the array of @ref cldnn_profiling_interval.
/// @param[out] size_ret Number of elements required to store profiling information.
CLDNN_API void cldnn_get_event_profiling_info(cldnn_event event, cldnn_profiling_interval* profiling, size_t size, size_t* size_ret, cldnn_status* status);
/// @}

/// @addtogroup c_program
/// @{

/// @brief Builds executable program based on user-defined @p topology by specified @p engine.
/// @param[in] engine The engine which will be used to build the program.
/// @param[in] topology The user-defined topology on which the network will be based.
/// @param[in] options The pointer of array of @ref cldnn_build_option which define network build options.
/// @param[in] options_num Number of elements in the @p options array.
CLDNN_API cldnn_program cldnn_build_program(cldnn_engine engine, cldnn_topology topology, cldnn_build_option* options, size_t options_num, cldnn_status* status);

/// @brief Increment reference counter for the program object.
CLDNN_API void cldnn_retain_program(cldnn_program program, cldnn_status* status);

/// @brief Decrement reference counter for the program object. Deletes object when counter becomes zero.
CLDNN_API void cldnn_release_program(cldnn_program program, cldnn_status* status);
/// @}

/// @addtogroup c_network
/// @{

/// @brief Builds and allocates executable network based on user-defined @p topology by specified @p engine. This is a shorthand for cldnn_build_program and cldnn_allocate_network.
/// @param[in] engine The engine which will be used to build the metwork.
/// @param[in] topology The user-defined topology on which the network will be based.
/// @param[in] options The pointer of array of @ref cldnn_build_option which define network build options.
/// @param[in] options_num Number of elements in the @p options array.
CLDNN_API        cldnn_network cldnn_build_network(cldnn_engine engine, cldnn_topology topology, cldnn_build_option* options, size_t options_num, cldnn_status* status);

/// @brief Allocates memory for a new network which will be able to execute specified @p program.
/// @param[in] program The program object which holds binaries compiled from some topology and engine. Multiple network objects can share the same program.
CLDNN_API        cldnn_network cldnn_allocate_network(cldnn_program program, cldnn_status* status);

/// @brief Increment reference counter for the network object.
CLDNN_API                 void cldnn_retain_network(cldnn_network network, cldnn_status* status);

/// @brief Decrement reference counter for the network object. Deletes object when counter becomes zero.
CLDNN_API                 void cldnn_release_network(cldnn_network network, cldnn_status* status);

/// @brief Provides user input data to the network (for @p input_layout primitives).
/// @param[in] id Primitive @p id of @p input_layout primitive defined in @p topology.
/// @param[in] mem Memory object with user data which @p layout matches the @p input_layout defined in @p topology.
/// @details User should set the input data for every @p input_layout primitive defined in @p topology
/// by calling this function before call to cldnn_execute_network().
CLDNN_API                 void cldnn_set_network_input(cldnn_network network, cldnn_primitive_id id, cldnn_memory mem, cldnn_status* status);

/// @brief Sets learning rate for training primitives in network.
/// @param[in] lr Learning rate.
CLDNN_API void cldnn_set_learning_rate(cldnn_network network, float lr, cldnn_status* status);

/// @brief Returns learning rate value.
CLDNN_API float cldnn_get_learning_rate(cldnn_network network, cldnn_status* status);

/// @brief Returns information about particular primitive.
/// @details Function fills user provided buffer by primitive description.
/// @param[in] id Primitive @p id of @p input_layout primitive defined in @p topology.
/// @param[in] info Pointer to user-allocated buffer to store names.
/// @param[in] size Size (in chars) of the buffer.
/// @param[out] size_ret Required size (in chars) to store result.
/// @returns pointer to array of chars with detailed information about particular primitive.
CLDNN_API void cldnn_get_primitive_info(cldnn_network network, cldnn_primitive_id id, char* info, size_t size, size_t* size_ret, cldnn_status* status);

/// @brief Returns @p engine associated with the @p network.
CLDNN_API         cldnn_engine cldnn_get_network_engine(cldnn_network network, cldnn_status* status);

/// @brief Returns @p program associated with the @p network.
CLDNN_API        cldnn_program cldnn_get_network_program(cldnn_network network, cldnn_status* status);

/// @brief Returns names of network outputs.
/// @details Function fills user provided buffer by primitive names. Each name is followed by '\0'.
/// Empty name "\0\0" means end of data.
/// @param[in] names Pointer to user-allocated buffer to store names.
/// @param[in] size Size (in chars) of the buffer.
/// @param[out] size_ret Required size (in chars) to store result.
CLDNN_API                 void cldnn_get_network_output_names(cldnn_network network, char* names, size_t size, size_t* size_ret, cldnn_status* status);

/// @brief Returns names of executed primitives.
/// @details Function fills user provided buffer by primitive names. Each name is followed by '\0'.
/// Empty name "\0\0" means end of data.
/// @param[in] names Pointer to user-allocated buffer to store names.
/// @param[in] size Size (in chars) of the buffer.
/// @param[out] size_ret Required size (in chars) to store result.
CLDNN_API                 void cldnn_get_network_executed_primitive_names(cldnn_network network, char* names, size_t size, size_t* size_ret, cldnn_status* status);

/// @brief Returns names of all primitives in network.
/// @details Function fills user provided buffer by primitive names. Each name is followed by '\0'.
/// Empty name "\0\0" means end of data.
/// @param[in] names Pointer to user-allocated buffer to store names.
/// @param[in] size Size (in chars) of the buffer.
/// @param[out] size_ret Required size (in chars) to store result.
CLDNN_API                 void cldnn_get_network_all_primitive_names(cldnn_network network, char* names, size_t size, size_t* size_ret, cldnn_status* status);

/// @brief Returns names of all primitives in network before graph optimization.
/// @details Function fills user provided buffer by primitive names. Each name is followed by '\0'.
/// Empty name "\0\0" means end of data.
/// @param[in] names Pointer to user-allocated buffer to store names.
/// @param[in] size Size (in chars) of the buffer.
/// @param[out] size_ret Required size (in chars) to store result.
CLDNN_API                 void cldnn_get_network_all_primitive_org_names(cldnn_network network, char* names, size_t size, size_t* size_ret, cldnn_status* status);

/// @brief Executes network.
/// @details User should call cldnn_set_network_input() for every @p input_layout defined in tho source @p topology.
/// Function returns immediately, even if @p dependencies are not set yet.
/// @params dependencies Pointer to an array of @ref cldnn_events to be waited for network execution.
/// @param deps_num Number of elements in the @p dependencies array.
CLDNN_API                 void cldnn_execute_network(cldnn_network network, cldnn_event* dependencies, size_t deps_num, cldnn_status* status);

/// @brief Returns executed network output information.
/// @details User should call this function after cldnn_execute_network() to get result of network execution.
/// @param name Output name to get the result.
/// @returns @ref cldnn_network_output structure with the output information.
/// To work with the result of this function, user should first wait for cldnn_network_output::event
/// before getting an access to cldnn_network_output::memory.
CLDNN_API cldnn_network_output cldnn_get_network_output(cldnn_network network, const char* name, cldnn_status* status);

/// @brief Returns @ref memory corresponding to output with @p name.
/// @details User can call this function even before calling cldnn_execute_network(), but then content of memory is uninitialized.
/// @param name Output name to get the result.
/// @returns @ref cldnn_memory structure with the output information.
CLDNN_API cldnn_memory cldnn_get_network_output_memory(cldnn_network network, const char* name, cldnn_status* status);

/// @brief Returns @ref event corresponding to output with @p name.
/// @details User can call this function even before calling cldnn_execute_network(), but then content of memory is uninitialized.
/// @param name Output name to get the result.
/// @returns @ref cldnn_event structure with the output information.
CLDNN_API cldnn_event cldnn_get_network_output_event(cldnn_network network, const char* name, cldnn_status* status);
/// @}

/// @addtogroup c_memory
/// @{

/// @brief Allocate memory on @p engine using specified @p layout.
CLDNN_API cldnn_memory cldnn_allocate_memory(cldnn_engine engine, cldnn_layout layout, cldnn_status* status);
/// @brief Create memory object attached to the buffer allocated by user.
/// @note User is responsible for buffer deallocation. Buffer lifetime should be bigger than lifetime of the memory object.
CLDNN_API cldnn_memory cldnn_attach_memory(cldnn_layout layout, void* pointer, size_t size, cldnn_status* status);
/// @brief Checks if two memory objects refer to the same underlaying buffer.
CLDNN_API int32_t cldnn_is_the_same_buffer(cldnn_memory mem1, cldnn_memory mem2, cldnn_status* status);
/// @brief Increment reference counter for the memory object.
CLDNN_API void cldnn_retain_memory(cldnn_memory memory, cldnn_status* status);
/// @brief Decrement reference counter for the memory object. Deletes object when counter becomes zero.
CLDNN_API void cldnn_release_memory(cldnn_memory memory, cldnn_status* status);
/// @brief Locks memory buffer. Provides direct access to memory data.
/// @returns Direct pointer to the memory data.
CLDNN_API void* cldnn_lock_memory(cldnn_memory memory, cldnn_status* status);
/// @brief Unlocks memory locked by cldnn_lock_memory(cldnn_memory memory, cldnn_status* status).
CLDNN_API void cldnn_unlock_memory(cldnn_memory memory, cldnn_status* status);
/// @brief Returns memory layout
/// @returns @ref cldnn_layout which describes memory.
CLDNN_API cldnn_layout cldnn_get_memory_layout(cldnn_memory memory, cldnn_status* status);
/// @brief Returns reference to the engine associated with memory object.
/// @returns The engine associated with memory object. Or NULL if memory was attached to user-allocated buffer.
CLDNN_API cldnn_engine cldnn_get_memory_engine(cldnn_memory memory, cldnn_status* status);
/// @brief converts float(32 bit) to half_t(fp16 bit)
/// @returns 16bit half_t
CLDNN_API uint16_t cldnn_float_to_half(float,cldnn_status*);
/// @brief converts  half_t(f16 bit) to float(32 bit) 
/// @returns 32bit float
CLDNN_API float cldnn_half_to_float(uint16_t, cldnn_status*);

/// @}

/// @addtogroup c_error
/// @{

/// @brief If cldnn function returns status different than CLDNN_SUCCESS, user call this function to get more details.
/// @returns pointer to array of chars with more detailed description of last error.
/// @note If sequence of error occure, description of only last error will avaiable
CLDNN_API const char* cldnn_get_last_error_message();
/// @}

#ifdef __cplusplus
}
#endif

/// @}

//primitives
#ifdef __cplusplus
#define CLDNN_DECLARE_PRIMITIVE_TYPE_ID(PType) extern "C" CLDNN_API cldnn_primitive_type_id cldnn_##PType##_type_id(cldnn_status* status)
#else
#define CLDNN_DECLARE_PRIMITIVE_TYPE_ID(PType) CLDNN_API cldnn_primitive_type_id cldnn_##PType##_type_id(cldnn_status* status)
#endif


#endif /* CLDNN_H */
