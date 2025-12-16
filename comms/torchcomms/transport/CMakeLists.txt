# Copyright (c) Meta Platforms, Inc. and affiliates.
# Extension: torchcomms._transport
file(GLOB TORCHCOMMS_TRANSPORT_SOURCES
    "comms/torchcomms/transport/*.cpp"
)

# Support both CUDA and ROCm/HIP
if(DEFINED ENV{ROCM_PATH} OR EXISTS "/opt/rocm")
    set(USE_ROCM ON)
    if(DEFINED ENV{ROCM_PATH})
        set(ROCM_PATH $ENV{ROCM_PATH})
    else()
        set(ROCM_PATH "/opt/rocm")
    endif()
    find_package(hip REQUIRED)
    message(STATUS "Using ROCm/HIP at ${ROCM_PATH}")
    add_compile_definitions(__HIP_PLATFORM_AMD__ USE_ROCM)
else()
    set(USE_ROCM OFF)
    find_package(CUDA REQUIRED)

    if(NOT DEFINED ENV{NVCC_GENCODE})
        set(ENV{NVCC_GENCODE} "-gencode=arch=compute_90,code=sm_90")
        message(STATUS "ENV NVCC_GENCODE: $ENV{NVCC_GENCODE}")
    endif()
    set(NVCC_GENCODE $ENV{NVCC_GENCODE})
    message(STATUS "NVCC_GENCODE: ${NVCC_GENCODE}")

    if(NOT DEFINED ENV{TORCH_CUDA_ARCH_LIST})
        set(ENV{TORCH_CUDA_ARCH_LIST} "9.0a")
    endif()
    set(TORCH_CUDA_ARCH_LIST $ENV{TORCH_CUDA_ARCH_LIST})
    message(STATUS "TORCH_CUDA_ARCH_LIST: ${TORCH_CUDA_ARCH_LIST}")
    add_compile_options($<$<COMPILE_LANGUAGE:CUDA>:${NVCC_GENCODE}>)
endif()

add_compile_definitions(MOCK_SCUBA_DATA CTRAN_DISABLE_TCPDM FMT_USE_CONSTEXPR)

# CTranIB
add_subdirectory(comms/utils)
add_subdirectory(comms/ctran)

# Get folly LDFLAGS using pkg-config
set(ENV{PKG_CONFIG_PATH} "${CONDA_LIB}/pkgconfig")
execute_process(
    COMMAND pkg-config --libs --static libfolly
    OUTPUT_VARIABLE FOLLY_LDFLAGS_RAW
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE PKG_RESULT
)
if(NOT PKG_RESULT EQUAL 0)
    message(FATAL_ERROR "pkg-config for libfolly failed")
endif()
separate_arguments(FOLLY_LDFLAGS NATIVE_COMMAND "${FOLLY_LDFLAGS_RAW}")
# Remove any -l:libglog.a from FOLLY_LDFLAGS
list(FILTER FOLLY_LDFLAGS EXCLUDE REGEX ".*libglog.*")

add_library(torchcomms_transport MODULE
    ${TORCHCOMMS_TRANSPORT_SOURCES}
)
set_target_properties(torchcomms_transport PROPERTIES
    PREFIX ""
    OUTPUT_NAME "_transport"
    SUFFIX ".${Python3_SOABI}.so"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/comms/torchcomms"
)
if(NOT USE_ROCM)
    set_target_properties(torchcomms_transport PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON
        CUDA_RESOLVE_DEVICE_SYMBOLS ON
    )
endif()
set_target_properties(torchcomms_transport PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
target_include_directories(torchcomms_transport PRIVATE
    ${ROOT}
    ${CONDA_INCLUDE}
    ${Python3_INCLUDE_DIRS}
)
target_compile_features(torchcomms_transport PRIVATE cxx_std_20)
target_link_directories(torchcomms_transport PRIVATE ${CONDA_LIB})
target_link_libraries(torchcomms_transport PRIVATE
    ${TORCH_LIBRARIES}
    ${TORCH_PYTHON_LIB}
    ctran
    utils
    ${FOLLY_LDFLAGS}
)

if(USE_SYSTEM_LIBS)
    target_link_libraries(torchcomms_transport PRIVATE
        "-lfolly"
        "-lgflags"
        "-lglog"
        "-lboost_program_options"
        "-lboost_filesystem"
        "-lboost_context"
        "-lfmt"
    )
else()
    target_link_libraries(torchcomms_transport PRIVATE
        "-l:libfolly.a"
        "-l:libgflags.a"
        "-l:libglog.a"
        "-l:libboost_program_options.a"
        "-l:libboost_filesystem.a"
        "-l:libboost_context.a"
        "-l:libfmt.a"
    )
endif()

if(USE_ROCM)
    target_link_libraries(torchcomms_transport PRIVATE hip::host)
elseif(CUDA_FOUND)
    target_link_libraries(torchcomms_transport PRIVATE CUDA::cudart)
endif()

install(TARGETS torchcomms_transport
    LIBRARY DESTINATION .
)
