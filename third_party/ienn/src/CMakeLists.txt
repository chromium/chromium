# Copyright (C) 2018-2020 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET_NAME ie_nn_c_api)
#set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")

file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

# create library

add_library(${TARGET_NAME} SHARED ${HEADERS} ${SOURCES})

target_link_libraries(${TARGET_NAME} PRIVATE ${InferenceEngine_LIBRARIES})

target_include_directories(${TARGET_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${InferenceEngine_INCLUDE_DIRS}")

#add_cpplint_target(${TARGET_NAME}_cpplint FOR_TARGETS ${TARGET_NAME})


MESSAGE( STATUS "==============this var key = ${InferenceEngine_INCLUDE_DIRS}.")

# export

export(TARGETS ${TARGET_NAME} NAMESPACE IE:: APPEND FILE "${CMAKE_BINARY_DIR}/targets.cmake")

# install

#install(TARGETS ${TARGET_NAME}
#        RUNTIME DESTINATION ${IE_CPACK_LIBRARY_PATH} COMPONENT core
#        ARCHIVE DESTINATION ${IE_CPACK_LIBRARY_PATH} COMPONENT core
#        LIBRARY DESTINATION ${IE_CPACK_LIBRARY_PATH} COMPONENT core)

#install(DIRECTORY ${InferenceEngine_C_API_SOURCE_DIR}/include/
#        DESTINATION ${IE_CPACK_IE_DIR}/include/
#        COMPONENT core)

macro(disable_deprecated_warnings)
    if(WIN32)
        if(CMAKE_CXX_COMPILER_ID MATCHES Intel)
            set(ie_c_cxx_deprecated "/Qdiag-disable:1478,1786")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES MSVC)
            set(ie_c_cxx_deprecated "/wd4996")
        endif()
    else()
        if(CMAKE_CXX_COMPILER_ID STREQUAL Intel)
            set(ie_c_cxx_deprecated "-diag-disable=1478,1786")
        else()
            set(ie_c_cxx_deprecated "-Wno-deprecated-declarations")
        endif()
    endif()

    if(NOT ie_c_cxx_deprecated)
        message(WARNING "Unsupported CXX compiler ${CMAKE_CXX_COMPILER_ID}")
    endif()

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ie_c_cxx_deprecated}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ie_c_cxx_deprecated}")
endmacro()

function(nn_builder_disable_warnings)
    disable_deprecated_warnings()
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR} PROPERTIES COMPILE_FLAGS "${ie_c_cxx_deprecated}")
endfunction()

nn_builder_disable_warnings()
