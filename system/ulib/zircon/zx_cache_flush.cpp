// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <zircon/syscalls.h>

#include <zircon/compiler.h>
#include "private.h"

namespace {

template<typename T>
void for_each_cache_line(const void* addr, size_t len, uint32_t line_size,
                         T func) {
    for (uintptr_t p = (uintptr_t)addr & -(uintptr_t)line_size;
         p < (uintptr_t)addr + len;
         p += line_size) {
        func(p);
    }
}

template<typename T>
void for_each_dcache_line(const void* addr, size_t len, T func) {
    for_each_cache_line(addr, len, DATA_CONSTANTS.dcache_line_size, func);
}

template<typename T>
void for_each_icache_line(const void* addr, size_t len, T func) {
    for_each_cache_line(addr, len, DATA_CONSTANTS.icache_line_size, func);
}

}  // anonymous namespace

zx_status_t _zx_cache_flush(const void* addr, size_t len, uint32_t flags) {
    switch (flags) {
    case ZX_CACHE_FLUSH_INSN:
        break;
    case ZX_CACHE_FLUSH_DATA:
        break;
    case ZX_CACHE_FLUSH_DATA | ZX_CACHE_FLUSH_INSN:
        break;
    case ZX_CACHE_FLUSH_DATA | ZX_CACHE_FLUSH_INVALIDATE:
        break;
    case ZX_CACHE_FLUSH_DATA | ZX_CACHE_FLUSH_INVALIDATE | ZX_CACHE_FLUSH_INSN:
        break;
    default:
        return ZX_ERR_INVALID_ARGS;
    }

#if defined(__x86_64__)

    // Nothing needs doing for x86.

#elif defined(__aarch64__)

    if (flags & ZX_CACHE_FLUSH_DATA) {
        if (flags & ZX_CACHE_FLUSH_INVALIDATE) {
            for_each_dcache_line(addr, len, [](uintptr_t p) {
                    // Clean and invalidate data cache to point of coherency.
                    __asm__ volatile("dc civac, %0" :: "r"(p));
                });
        } else {
            for_each_dcache_line(addr, len, [](uintptr_t p) {
                    // Clean data cache (dc) to point of coherency (cvac).
                    __asm__ volatile("dc cvac, %0" :: "r"(p));
                });
        }
    }

    if (flags & ZX_CACHE_FLUSH_INSN) {
        // If we didn't already clean the dcache all the way to the point
        // of coherency, clean it the point to unification.
        if (!(flags & ZX_CACHE_FLUSH_DATA)) {
            for_each_dcache_line(addr, len, [](uintptr_t p) {
                    // Clean data cache (dc) to point of unification (cvau).
                    __asm__ volatile("dc cvau, %0" :: "r"(p));
                });
        }
        // Synchronize the dcache flush to before the icache flush.
        __asm__ volatile("dsb ish");

        for_each_icache_line(addr, len, [](uintptr_t p) {
                // Clean instruction cache (ic) to point of unification (ivau).
                __asm__ volatile("ic ivau, %0" :: "r"(p));
            });
        // Synchronize the icache flush to before future instruction fetches.
        __asm__ volatile("isb sy");
    }

#else

# error what architecture?

#endif

    return ZX_OK;
}

VDSO_INTERFACE_FUNCTION(zx_cache_flush);
