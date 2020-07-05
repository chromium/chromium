#ifndef IE_CONSTANTS_H
#define IE_CONSTANTS_H

namespace InferenceEngine {

// Operand types.
typedef enum {
    FLOAT32 = 0,
    INT32 = 1,
    UINT32 = 2,
    TENSOR_FLOAT32 = 3,
    TENSOR_INT32 = 4,
    TENSOR_QUANT8_ASYMM = 5,
    TENSOR_QUANT8_SYMM_PER_CHANNEL = 11,
    TENSOR_QUANT8_ASYMM_SIGNED = 14,
} data_t;

// Operation types.
typedef enum {
    ADD = 0,
    AVERAGE_POOL_2D = 1,
    CONCATENATION = 2,
    CONV_2D = 3,
    DEPTHWISE_CONV_2D = 4,
    DEPTH_TO_SPACE = 5,
    DEQUANTIZE = 6,
    EMBEDDING_LOOKUP = 7,
    FLOOR = 8,
    FULLY_CONNECTED = 9,
    HASHTABLE_LOOKUP = 10,
    L2_NORMALIZATION = 11,
    L2_POOL_2D = 12,
    LOCAL_RESPONSE_NORMALIZATION = 13,
    LOGISTIC = 14,
    LSH_PROJECTION = 15,
    LSTM = 16,
    MAX_POOL_2D = 17,
    MUL = 18,
    RELU = 19,
    RELU1 = 20,
    RELU6 = 21,
    RESHAPE = 22,
    RESIZE_BILINEAR_NN = 23,
    RNN = 24,
    SOFTMAX = 25,
    SPACE_TO_DEPTH = 26,
    SVDF = 27,
    TANH = 28,
    ARGMAX = 39,
    PRELU = 71,
    ATROUS_CONV_2D = 10003,
    ATROUS_DEPTHWISE_CONV_2D = 10004,
} operation_t;

// Fused activation function types.
typedef enum {
    FUSED_NONE = 0,
    FUSED_RELU = 1,
    FUSED_RELU1 = 2,
    FUSED_RELU6 = 3,
} fuse_t;

// Implicit padding algorithms.
typedef enum {
    PADDING_SAME = 1,
    PADDING_VALID = 2,
} padding_t;

// Execution preferences.
typedef enum {
    PREFER_LOW_POWER = 0,
    PREFER_FAST_SINGLE_ANSWER = 1,
    PREFER_SUSTAINED_SPEED = 2,
    PREFER_ULTRA_LOW_POWER = 3,
} prefer_t;

// Result code.
typedef enum {
    NOT_ERROR = 0,
    OUT_OF_MEMORY = 1,
    INCOMPLETE = 2,
    UNEXPECTED_NULL = 3,
    BAD_DATA = 4,
    OP_FAILED = 5,
    UNMAPPABLE = 5,
    BAD_STATE = 6,
} error_t;

}

#endif // IE_CONSTANTS_H
