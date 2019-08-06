/*******************************************************************************
* Copyright 2019 Intel Corporation
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

#include <CL/sycl.hpp>

#include "common/utils.hpp"
#include "sycl/sycl_ocl_gpu_kernel.hpp"
#include "sycl/sycl_stream.hpp"
#include "sycl/sycl_utils.hpp"

namespace mkldnn {
namespace impl {
namespace sycl {

static void set_scalar_arg(
        cl::sycl::handler &cgh, int index, size_t size, void *value) {
    switch (size) {
        case sizeof(uint8_t):
            cgh.set_arg(index, *static_cast<uint8_t *>(value));
            break;
        case sizeof(uint16_t):
            cgh.set_arg(index, *static_cast<uint16_t *>(value));
            break;
        case sizeof(uint32_t):
            cgh.set_arg(index, *static_cast<uint32_t *>(value));
            break;
        case sizeof(uint64_t):
            cgh.set_arg(index, *static_cast<uint64_t *>(value));
            break;
        default:
            assert(!"Please add another case");
            throw std::runtime_error("Internal error");
    }
}

sycl_ocl_gpu_kernel_t::~sycl_ocl_gpu_kernel_t() {
    if (ocl_kernel_) OCL_CHECK_V(clReleaseKernel(ocl_kernel_));
}

status_t sycl_ocl_gpu_kernel_t::parallel_for(stream_t &stream,
        const compute::nd_range_t &range,
        const compute::kernel_arg_list_t &arg_list) const {
    if (range.is_zero()) return status::success;

    auto *sycl_stream = utils::downcast<sycl::sycl_stream_t *>(&stream);
    auto *sycl_engine
            = utils::downcast<sycl::sycl_gpu_engine_t *>(sycl_stream->engine());
    auto &queue = sycl_stream->queue();
    cl::sycl::kernel sycl_kernel(ocl_kernel_, sycl_engine->context());
    queue.submit([&](cl::sycl::handler &cgh) {
        for (int i = 0; i < arg_list.nargs(); ++i) {
            auto &arg = arg_list.get(i);
            if (arg.is_global()) {
                auto *mem_storage
                        = static_cast<const memory_storage_t *>(arg.value());
                if (*mem_storage) {
                    auto *sycl_mem_storage = utils::downcast<
                            const sycl_memory_storage_base_t *>(
                            mem_storage->impl());
                    switch (sycl_mem_storage->memory_api_kind()) {
                        case memory_api_kind_t::buffer: {
                            auto *m = utils::downcast<
                                    const sycl_buffer_memory_storage_t *>(
                                    mem_storage->impl());
                            auto &sycl_buf = m->buffer();
                            cgh.set_arg((int)i,
                                    sycl_buf.get_access<
                                            cl::sycl::access::mode::read_write>(
                                            cgh));
                            break;
                        }
#ifdef MKLDNN_SYCL_INTEL
                        case memory_api_kind_t::usm: {
                            auto *m = utils::downcast<
                                    const sycl_usm_memory_storage_t *>(
                                    mem_storage->impl());
                            cgh.set_arg((int)i, m->usm_ptr());
                            break;
                        }
#endif
                        default: assert(!"not expected");
                    }
                } else {
                    cgh.set_arg((int)i, nullptr);
                }
            } else {
                // XXX: workaround for bug in the SYCL library:
                // set_arg() does not work with constant scalars
                set_scalar_arg(cgh, (int)i, arg.size(),
                        const_cast<void *>(arg.value()));
            }
        }
        if (range.local_range()) {
            auto sycl_nd_range = to_sycl_nd_range(range);
            cgh.parallel_for(sycl_nd_range, sycl_kernel);
        } else {
            auto sycl_range = to_sycl_range(range);
            cgh.parallel_for(sycl_range, sycl_kernel);
        }
    });
    return status::success;
}

} // namespace sycl
} // namespace impl
} // namespace mkldnn
