// Copyright (c) Meta Platforms, Inc. and affiliates.

#if defined(__HIP_PLATFORM_AMD__) || defined(USE_ROCM)
#include <hip/hip_fp16.h>
#include <hip/hip_bf16.h>
using alltoallimpl_bfloat16_t = __hip_bfloat16;
#define __CUDA_BF16_TYPES_EXIST__ 1
#else
#include <cuda_fp16.h>
#include <cuda_bf16.h>
using alltoallimpl_bfloat16_t = __nv_bfloat16;
#endif
#include <cstddef>
#include <memory>

#include "comms/ctran/CtranComm.h"
#include "comms/ctran/algos/AllToAll/AllToAllImpl.h"
#include "comms/ctran/algos/CtranAlgo.h"
#include "comms/ctran/algos/CtranAlgoDev.h"
#include "comms/ctran/gpe/CtranGpe.h"
#include "comms/utils/cvars/nccl_cvars.h"

namespace ctran::alltoall {
void* alltoallKerns[commNumTypes] = {
    (void*)ncclKernelAllToAll<int8_t>,
    (void*)ncclKernelAllToAll<uint8_t>,
    (void*)ncclKernelAllToAll<int32_t>,
    (void*)ncclKernelAllToAll<uint32_t>,
    (void*)ncclKernelAllToAll<int64_t>,
    (void*)ncclKernelAllToAll<uint64_t>,
    (void*)ncclKernelAllToAll<half>,
    (void*)ncclKernelAllToAll<float>,
    (void*)ncclKernelAllToAll<double>,
#if defined(__CUDA_BF16_TYPES_EXIST__)
    (void*)ncclKernelAllToAll<alltoallimpl_bfloat16_t>,
#endif
#if defined(__CUDA_FP8_TYPES_EXIST__) && defined(NCCL_ENABLE_FP8)
    (void*)ncclKernelAllToAll<__nv_fp8_e4m3>,
    (void*)ncclKernelAllToAll<__nv_fp8_e5m2>,
#endif
};

unsigned int bestThreadBlockSize = 0;
commResult_t setupKernelConfig(
    const void* sendbuff,
    void* recvbuff,
    const size_t count,
    commDataType_t datatype,
    CtranComm* comm,
    cudaStream_t stream,
    KernelConfig& config) {
  // If first time call, query cuda recommended blockSize
  if (bestThreadBlockSize == 0) {
    int minGridSize = 0;
    FB_CUDACHECK(cudaOccupancyMaxPotentialBlockSize(
        &minGridSize,
        (int*)&bestThreadBlockSize,
        reinterpret_cast<const void*>(alltoallKerns[datatype]),
        0 /* dynamicSMemSize */,
        0 /* blockSizeLimit */));
  }

  // Allow user to customize thread block size if specified
  config.numThreads = NCCL_CTRAN_ALLTOALL_THREAD_BLOCK_SIZE > 0
      ? NCCL_CTRAN_ALLTOALL_THREAD_BLOCK_SIZE
      : bestThreadBlockSize;

  // Use specified grid size if specified and in limit; otherwise use default
  if (NCCL_CTRAN_ALLTOALL_NUM_THREAD_BLOCKS < 1 ||
      NCCL_CTRAN_ALLTOALL_NUM_THREAD_BLOCKS > CTRAN_ALGO_MAX_THREAD_BLOCKS) {
    // Calculate default grid size based on block size
    unsigned int gridSize = (count + config.numThreads - 1) / config.numThreads;
    if (gridSize > CTRAN_ALGO_MAX_THREAD_BLOCKS) {
      gridSize = CTRAN_ALGO_MAX_THREAD_BLOCKS;
    }
    config.numBlocks = gridSize;
  } else {
    config.numBlocks = NCCL_CTRAN_ALLTOALL_NUM_THREAD_BLOCKS;
  }

  // gridSize must be even number, because we split blocks into two sets of
  // groups, one for sends and the other for receives, each send and receive
  // pair must use the same number of blocks
  if (config.numBlocks % 2) {
    config.numBlocks += 1;
  }

  config.args.devState_d = comm->ctran_->algo->getDevState();
  config.args.collective.alltoall.sendbuff = sendbuff;
  config.args.collective.alltoall.recvbuff = recvbuff;
  config.args.collective.alltoall.datatype = datatype;
  config.args.collective.alltoall.count = count;

  return commSuccess;
}
} // namespace ctran::alltoall
