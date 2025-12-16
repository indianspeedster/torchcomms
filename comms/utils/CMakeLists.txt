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
else()
    set(USE_ROCM OFF)
    find_package(CUDA REQUIRED)
    set(GPU_INCLUDE_DIRS ${CUDA_INCLUDE_DIRS})
endif()

file(GLOB_RECURSE UTILS_SOURCES
    "*.cc"
)
list(FILTER UTILS_SOURCES EXCLUDE REGEX ".*/tests/.*")

add_library(utils OBJECT
    ${UTILS_SOURCES}
)

target_compile_features(utils PRIVATE cxx_std_20)
target_compile_options(utils PRIVATE
    -fPIC
)
target_include_directories(utils PUBLIC
    ${ROOT}
    ${CONDA_INCLUDE}
    ${GPU_INCLUDE_DIRS}
)
