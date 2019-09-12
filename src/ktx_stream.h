/* -*- tab-width: 4; -*- */
/* vi: set sw=2 ts=4 expandtab: */

/*
 * Copyright (c) 2010-2018 The Khronos Group Inc.
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
 */

/**
 * @internal
 * @file
 * @~English
 *
 * @brief Interface of ktxStream for memory.
 *
 * @author Maksim Kolesin
 * @author Georg Kolling, Imagination Technology
 * @author Mark Callow, HI Corporation
 */

#ifndef MEMSTREAM_H
#define MEMSTREAM_H

#include <sys/types.h>
#include "ktx_texture.h"

/*
 * This is unsigned to allow ktxmemstreams to use the
 * full amount of memory available. Platforms will
 * limit the size of ktxfilestreams to, e.g, MAX_LONG
 * on 32-bit and ktxfilestreams raises errors if
 * offset values exceed the limits. This choice may
 * need to be revisited if we ever start needing -ve
 * offsets.
 *
 * Should the 2GB file size handling limit on 32-bit
 * platforms become a problem, ktxfilestream will have
 * to be changed to explicitly handle large files by
 * using the 64-bit stream functions.
 */

typedef size_t ktx_off_t;
typedef struct ktxMem ktxMem;
typedef struct ktxStream ktxStream;

enum streamType
{
  eStreamTypeFile = 1,
  eStreamTypeMemory = 2
};

/**
 * @internal
 * @~English
 * @brief type for a pointer to a stream reading function
 */
typedef KTX_error_code (*ktxStream_read)(ktxStream* str,
                                         void* dst,
                                         const ktx_size_t count);
/**
 * @internal
 * @~English
 * @brief type for a pointer to a stream reading function
 */
typedef KTX_error_code (*ktxStream_write)(ktxStream* str,
                                          const void* src,
                                          const ktx_size_t size,
                                          const ktx_size_t count);
/**
 * @internal
 * @~English
 * @brief type for a pointer to a stream position query function
 */
typedef KTX_error_code (*ktxStream_getpos)(ktxStream* str,
                                           ktx_off_t* const offset);

/**
 * @internal
 * @~English
 * @brief type for a pointer to a stream size query function
 */
typedef KTX_error_code (*ktxStream_getsize)(ktxStream* str,
                                            ktx_size_t* const size);

/**
 * @internal
 * @~English
 * @brief Destruct a stream
 */
typedef void (*ktxStream_destruct)(ktxStream* str);

/**
 * @internal
 * @~English
 * @brief KTX stream class
 */
struct ktxStream
{
  ktxStream_read read; /*!< @internal pointer to function for reading bytes. */
  ktxStream_getpos getpos; /*!< @internal pointer to function for getting
                              current position in stream. */
  ktxStream_getsize
    getsize; /*!< @internal pointer to function for querying size. */
  ktxStream_destruct destruct; /*!< @internal destruct the stream. */

  enum streamType type;
  union {
    FILE* file;
    ktxMem* mem;
  } data;                     /**< @internal pointer to the stream data. */
  ktx_bool_t closeOnDestruct; /**< @internal Close FILE* or dispose of memory on
                                 destruct. */
};

/*
 * Initialize a ktxStream to a ktxMemStream with internally
 * allocated memory. Can be read or written.
 */
KTX_error_code
ktxMemStream_construct(ktxStream* str, ktx_bool_t freeOnDestruct);
/*
 * Initialize a ktxStream to a read-only ktxMemStream reading
 * from an array of bytes.
 */
KTX_error_code
ktxMemStream_construct_ro(ktxStream* str,
                          const ktx_uint8_t* pBytes,
                          const ktx_size_t size);
void
ktxMemStream_destruct(ktxStream* str);

KTX_error_code
ktxMemStream_getdata(ktxStream* str, ktx_uint8_t** ppBytes);

#endif /* MEMSTREAM_H */
