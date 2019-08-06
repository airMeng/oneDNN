/*******************************************************************************
* Copyright 2018 Intel Corporation
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

#ifndef PRIMITIVE_EXEC_TYPES_HPP
#define PRIMITIVE_EXEC_TYPES_HPP

#include <unordered_map>

#include "mkldnn_types.h"

#include "c_types_map.hpp"
#include "memory.hpp"
#include "memory_storage.hpp"

#define CTX_IN_STORAGE(arg) \
    (ctx.input(arg) ? *(ctx.input(arg)->memory_storage()) \
                    : memory_storage_t::empty_storage())

#define CTX_OUT_STORAGE(arg) \
    (ctx.output(arg) ? *(ctx.output(arg)->memory_storage()) \
                     : memory_storage_t::empty_storage())

namespace mkldnn {
namespace impl {

struct memory_arg_t {
    memory_t *mem;
    bool is_const;
};

using exec_args_t = std::unordered_map<int, memory_arg_t>;

status_t cvt_primtive_args(const primitive_desc_t *pd, int nargs,
        const mkldnn_exec_arg_t *c_args, exec_args_t &args);

/** Primitive execution context (helps passing stream, memories, and events. */
struct exec_ctx_t {
    exec_ctx_t(stream_t *stream = nullptr) : stream_(stream) {}
    exec_ctx_t(stream_t *stream, exec_args_t &&args)
        : stream_(stream), args_(std::move(args)) {}
    exec_ctx_t(const exec_ctx_t &other, exec_args_t &&args)
        : stream_(other.stream_)
        , args_(std::move(args))
        , memory_storage_mapping_(other.memory_storage_mapping_) {}

    stream_t *stream() const { return stream_; }
    const exec_args_t &args() const { return args_; }

    memory_t *input(int arg) const;
    memory_t *output(int arg) const;
    memory_t *memory(int arg) const;

    void register_memory_storage_mapping(
            const memory_storage_t *mem_storage, void *data);
    void *host_ptr(int arg) const;
    void *host_ptr(const memory_storage_t *mem_storage) const;

    void *map_memory_storage(const memory_storage_t *storage) const;
    void unmap_memory_storage(
            const memory_storage_t *storage, void *mapped_ptr) const;

private:
    stream_t *stream_;
    exec_args_t args_;

    std::unordered_map<const memory_storage_impl_t *, void *>
            memory_storage_mapping_;
};

} // namespace impl
} // namespace mkldnn

#endif
