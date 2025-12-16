# Copyright (c) Meta Platforms, Inc. and affiliates.

# Support both CUDA and ROCm/HIP
if(DEFINED ENV{ROCM_PATH} OR EXISTS "/opt/rocm")
    set(USE_ROCM ON)
    if(DEFINED ENV{ROCM_PATH})
        set(ROCM_PATH $ENV{ROCM_PATH})
    else()
        set(ROCM_PATH "/opt/rocm")
    endif()
    find_package(hip REQUIRED)
    set(GPU_INCLUDE_DIRS "${ROCM_PATH}/include")
    add_compile_definitions(__HIP_PLATFORM_AMD__ USE_ROCM)
else()
    set(USE_ROCM OFF)
    find_package(CUDA REQUIRED)
    set(GPU_INCLUDE_DIRS ${CUDA_INCLUDE_DIRS})
endif()

file(GLOB_RECURSE CTRAN_SOURCES
    "*.cc"
)
# Only include .cu files for CUDA, not for ROCm
if(NOT USE_ROCM)
    file(GLOB_RECURSE CTRAN_CU_SOURCES "*.cu")
    list(APPEND CTRAN_SOURCES ${CTRAN_CU_SOURCES})
endif()

list(FILTER CTRAN_SOURCES EXCLUDE REGEX ".*/tests/.*")
list(FILTER CTRAN_SOURCES EXCLUDE REGEX ".*/benchmarks/.*")
if (NOT DEFINED ENABLE_TCPDM OR NOT ENABLE_TCPDM)
    add_compile_definitions(CTRAN_DISABLE_TCPDM)
    list(FILTER CTRAN_SOURCES EXCLUDE REGEX ".*/ctran/backends/tcpdevmem/.*")
endif()

if(NOT USE_ROCM)
    list(FILTER CMAKE_CUDA_FLAGS EXCLUDE REGEX ".*arch=compute_50,code=sm_50")
    list(FILTER CMAKE_CUDA_FLAGS EXCLUDE REGEX ".*arch=compute_60,code=sm_60")
    list(FILTER CMAKE_CUDA_FLAGS EXCLUDE REGEX ".*arch=compute_70,code=sm_70")
    list(FILTER CMAKE_CUDA_FLAGS EXCLUDE REGEX ".*arch=compute_75,code=sm_75")
    message(status "CMAKE_CUDA_FLAGS: ${CMAKE_CUDA_FLAGS}")
endif()

# genctran.py generates source files (*.cu) for template instantiations across
# different algorithms, data types, and operations.
# Skip for ROCm as the .cu files are not used
if(NOT USE_ROCM)
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/algos/genctran.py ${CMAKE_CURRENT_BINARY_DIR}/gensrc
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    file(GLOB CTRAN_GENSRC_FILES "${CMAKE_CURRENT_BINARY_DIR}/gensrc/*.cu")
endif()

add_library(ctran OBJECT
  ${CTRAN_GENSRC_FILES}
  ${CTRAN_SOURCES}
)

target_compile_features(ctran PRIVATE cxx_std_20)

target_compile_options(ctran PRIVATE
    -fPIC
)

if(NOT USE_ROCM)
    target_compile_options(ctran PRIVATE
        # copied from nccl
        $<$<COMPILE_LANGUAGE:CUDA>:--expt-extended-lambda -Xptxas -maxrregcount=96 -Xfatbin -compress-all>
    )
    if (NOT DEFINED ENABLE_TCPDM OR NOT ENABLE_TCPDM)
        target_compile_options(ctran PRIVATE
            # copied from nccl
            $<$<COMPILE_LANGUAGE:CUDA>:--compiler-options "-DCTRAN_DISABLE_TCPDM">
        )
    endif()
    set_target_properties(ctran PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON
        CUDA_RESOLVE_DEVICE_SYMBOLS ON
    )
endif()

set_target_properties(ctran PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(ctran PRIVATE
    ${ROOT}
    ${CONDA_INCLUDE}
    ${GPU_INCLUDE_DIRS}
)
