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

#include <string.h>
#ifdef _WIN32
#include <malloc.h>
#include <windows.h>
#endif
#include <limits.h>

#include "mkldnn.h"
#include "utils.hpp"

#if defined(MKLDNN_X86_64)
#include "xmmintrin.h"
#endif

namespace mkldnn {
namespace impl {

int mkldnn_getenv(const char *name, char *buffer, int buffer_size) {
    if (name == NULL || buffer_size < 0 || (buffer == NULL && buffer_size > 0))
        return INT_MIN;

    int result = 0;
    int term_zero_idx = 0;
    size_t value_length = 0;

#ifdef _WIN32
    value_length = GetEnvironmentVariable(name, buffer, buffer_size);
#else
    const char *value = ::getenv(name);
    value_length = value == NULL ? 0 : strlen(value);
#endif

    if (value_length > INT_MAX)
        result = INT_MIN;
    else {
        int int_value_length = (int)value_length;
        if (int_value_length >= buffer_size) {
            result = -int_value_length;
        } else {
            term_zero_idx = int_value_length;
            result = int_value_length;
#ifndef _WIN32
            if (value)
                strncpy(buffer, value, buffer_size - 1);
#endif
        }
    }

    if (buffer != NULL)
        buffer[term_zero_idx] = '\0';
    return result;
}

static bool dump_jit_code;
static bool initialized;

bool mkldnn_jit_dump() {
    if (!initialized) {
        const int len = 2;
        char env_dump[len] = {0};
        dump_jit_code =
            mkldnn_getenv("MKLDNN_JIT_DUMP", env_dump, len) == 1
            && atoi(env_dump) == 1;
        initialized = true;
    }
    return dump_jit_code;
}

FILE *mkldnn_fopen(const char *filename, const char *mode) {
#ifdef _WIN32
    FILE *fp = NULL;
    return fopen_s(&fp, filename, mode) ? NULL : fp;
#else
    return fopen(filename, mode);
#endif
}

thread_local unsigned int mxcsr_save;

void set_rnd_mode(round_mode_t rnd_mode) {
#if defined(MKLDNN_X86_64)
    mxcsr_save = _mm_getcsr();
    unsigned int mxcsr = mxcsr_save & ~(3u << 13);
    switch (rnd_mode) {
    case round_mode::nearest: mxcsr |= (0u << 13); break;
    case round_mode::down: mxcsr |= (1u << 13); break;
    default: assert(!"unreachable");
    }
    if (mxcsr != mxcsr_save) _mm_setcsr(mxcsr);
#else
    UNUSED(rnd_mode);
#endif
}

void restore_rnd_mode() {
#if defined(MKLDNN_X86_64)
    _mm_setcsr(mxcsr_save);
#endif
}

void *malloc(size_t size, int alignment) {
    void *ptr;

#ifdef _WIN32
    ptr = _aligned_malloc(size, alignment);
    int rc = ptr ? 0 : -1;
#else
    int rc = ::posix_memalign(&ptr, alignment, size);
#endif

    return (rc == 0) ? ptr : 0;
}

void free(void *p) {
#ifdef _WIN32
    _aligned_free(p);
#else
    ::free(p);
#endif
}

// Atomic operations
int32_t mkldnn_fetch_and_add(int32_t *dst, int32_t val) {
#ifdef _WIN32
    return InterlockedExchangeAdd(reinterpret_cast<long*>(dst), val);
#else
    return __sync_fetch_and_add(dst, val);
#endif
}

}
}

mkldnn_status_t mkldnn_set_jit_dump(int dump) {
    using namespace mkldnn::impl::status;
    if (dump < 0) return invalid_arguments;
    mkldnn::impl::dump_jit_code = dump;
    mkldnn::impl::initialized = true;
    return success;
}
