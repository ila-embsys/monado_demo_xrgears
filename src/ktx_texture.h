/* -*- tab-width: 4; -*- */
/* vi: set sw=2 ts=4 expandtab: */

#ifndef KTX_H_A55A6F00956F42F3A137C11929827FE1
#define KTX_H_A55A6F00956F42F3A137C11929827FE1

/*
 * ©2010-2018 The Khronos Group, Inc.
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
 *
 * See the accompanying LICENSE.md for licensing details for all files in
 * the KTX library and KTX loader tests.
 */

/**
 * @file
 * @~English
 *
 * @brief Declares the public functions and structures of the
 *        KTX API.
 *
 * @author Mark Callow, Edgewise Consulting and while at HI Corporation
 * @author Based on original work by Georg Kolling, Imagination Technology
 *
 * @version 3.0
 *
 * @todo Find a way so that applications do not have to define KTX_OPENGL{,_ES*}
 *       when using the library.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

/* To avoid including <KHR/khrplatform.h> define our own types. */
typedef unsigned char ktx_uint8_t;
typedef bool ktx_bool_t;
typedef uint16_t ktx_uint16_t;
typedef int16_t ktx_int16_t;
typedef uint32_t ktx_uint32_t;
typedef int32_t ktx_int32_t;
typedef size_t ktx_size_t;


/* This will cause compilation to fail if size of uint32 != 4. */
typedef unsigned char ktx_uint32_t_SIZE_ASSERT[sizeof(ktx_uint32_t) == 4];

/*
 * This #if allows libktx to be compiled with strict c99. It avoids
 * compiler warnings or even errors when a gl.h is already included.
 * "Redefinition of (type) is a c11 feature". Obviously this doesn't help if
 * gl.h comes after. However nobody has complained about the unguarded typedefs
 * since they were introduced so this is unlikely to be a problem in practice.
 * Presumably everybody is using platform default compilers not c99 or else
 * they are using C++.
 */
#if !defined(GL_NO_ERROR)
/*
 * To avoid having to including gl.h ...
 */
typedef unsigned char GLboolean;
typedef unsigned int GLenum;
typedef int GLint;
typedef int GLsizei;
typedef unsigned int GLuint;
typedef unsigned char GLubyte;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @~English
 * @brief Key String for standard orientation value.
 */
#define KTX_ORIENTATION_KEY "KTXorientation"
/**
 * @~English
 * @brief Standard format for 2D orientation value.
 */
#define KTX_ORIENTATION2_FMT "S=%c,T=%c"
/**
 * @~English
 * @brief Standard format for 3D orientation value.
 */
#define KTX_ORIENTATION3_FMT "S=%c,T=%c,R=%c"
/**
 * @~English
 * @brief Required unpack alignment
 */
#define KTX_GL_UNPACK_ALIGNMENT 4

#define KTX_TRUE true
#define KTX_FALSE false

/**
 * @~English
 * @brief Error codes returned by library functions.
 */
typedef enum KTX_error_code_t
{
  KTX_SUCCESS = 0,      /*!< Operation was successful. */
  KTX_FILE_DATA_ERROR,  /*!< The data in the file is inconsistent with the spec.
                         */
  KTX_FILE_OPEN_FAILED, /*!< The target file could not be opened. */
  KTX_FILE_OVERFLOW,    /*!< The operation would exceed the max file size. */
  KTX_FILE_READ_ERROR,  /*!< An error occurred while reading from the file. */
  KTX_FILE_SEEK_ERROR,  /*!< An error occurred while seeking in the file. */
  KTX_FILE_UNEXPECTED_EOF, /*!< File does not have enough data to satisfy
                              request. */
  KTX_FILE_WRITE_ERROR,    /*!< An error occurred while writing to the file. */
  KTX_GL_ERROR,            /*!< GL operations resulted in an error. */
  KTX_INVALID_OPERATION, /*!< The operation is not allowed in the current state.
                          */
  KTX_INVALID_VALUE,     /*!< A parameter value was not valid */
  KTX_NOT_FOUND,         /*!< Requested key was not found */
  KTX_OUT_OF_MEMORY,     /*!< Not enough memory to complete the operation. */
  KTX_UNKNOWN_FILE_FORMAT,      /*!< The file not a KTX file */
  KTX_UNSUPPORTED_TEXTURE_TYPE, /*!< The KTX file specifies an unsupported
                                   texture type. */
} KTX_error_code;

#define KTX_IDENTIFIER_REF                                                     \
  {                                                                            \
    0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A     \
  }
#define KTX_ENDIAN_REF (0x04030201)
#define KTX_ENDIAN_REF_REV (0x01020304)
#define KTX_HEADER_SIZE (64)

/**
 * @~English
 * @brief Result codes returned by library functions.
 */
typedef enum KTX_error_code_t ktxResult;

/**
 * @class ktxHashList
 * @~English
 * @brief Opaque handle to a ktxHashList.
 */
typedef struct ktxKVListEntry* ktxHashList;

/**
 * @class ktxTexture
 * @~English
 * @brief Class representing a texture.
 *
 * ktxTextures should be created only by one of the ktxTexture_Create*
 * functions and these fields should be considered read-only.
 */
typedef struct
{
  ktx_uint32_t glFormat; /*!< Format of the texture data, e.g., GL_RGB. */
  ktx_uint32_t glInternalformat;     /*!< Internal format of the texture data,
                                            e.g., GL_RGB8. */
  ktx_uint32_t glBaseInternalformat; /*!< Base format of the texture data,
                                            e.g., GL_RGB. */
  ktx_uint32_t glType; /*!< Type of the texture data, e.g, GL_UNSIGNED_BYTE.*/
  ktx_bool_t isCompressed; /*!< KTX_TRUE if @c glInternalFormat is that of
                                    a compressed texture. */

  ktx_uint32_t baseWidth;     /*!< Width of the base level of the texture. */
  ktx_uint32_t baseHeight;    /*!< Height of the base level of the texture. */
  ktx_uint32_t baseDepth;     /*!< Depth of the base level of the texture. */
  ktx_uint32_t numDimensions; /*!< Number of dimensions in the texture: 1, 2
                                     or 3. */
  ktx_uint32_t numLevels; /*!< Number of mip levels in the texture. Should be
                                 1, if @c generateMipmaps is KTX_TRUE. Can be
                                 less than a full pyramid but always starts at
                                 the base level. */
  ktx_uint32_t numLayers; /*!< Number of array layers in the texture. */
  ktx_uint32_t numFaces;  /*!< Number of faces, 6 for cube maps, 1 otherwise.*/
  ktxHashList kvDataHead; /*!< Head of the hash list of metadata. */
  ktx_uint32_t kvDataLen; /*!< Length of the metadata, if it has been
                                 extracted in its raw form, otherwise 0. */
  ktx_uint8_t* kvData;    /*!< Pointer to the metadata, if it has been extracted
                                 in its raw form, otherwise NULL. */
  ktx_size_t dataSize;    /*!< Length of the image data in bytes. */
  ktx_uint8_t* pData;     /*!< Pointer to the image data. */
} ktxTexture;


/**
 * @memberof ktxTexture
 * @~English
 * @brief Structure for passing texture information to ktxTexture_Create().
 *
 * @sa ktxTexture_Create()
 */
typedef struct
{
  ktx_uint32_t glInternalformat; /*!< Internal format for the texture, e.g.,
                                        GL_RGB8. */
  ktx_uint32_t baseWidth;        /*!< Width of the base level of the texture. */
  ktx_uint32_t baseHeight;    /*!< Height of the base level of the texture. */
  ktx_uint32_t baseDepth;     /*!< Depth of the base level of the texture. */
  ktx_uint32_t numDimensions; /*!< Number of dimensions in the texture, 1, 2
                                     or 3. */
  ktx_uint32_t numLevels; /*!< Number of mip levels in the texture. Should be
                                 1 if @c generateMipmaps is KTX_TRUE; */
  ktx_uint32_t numLayers; /*!< Number of array layers in the texture. */
  ktx_uint32_t numFaces;  /*!< Number of faces: 6 for cube maps, 1 otherwise. */
  ktx_bool_t isArray;     /*!< Set to KTX_TRUE if the texture is to be an
                                 array texture. Means OpenGL will use a
                                 GL_TEXTURE_*_ARRAY target. */
  ktx_bool_t generateMipmaps; /*!< Set to KTX_TRUE if mipmaps should be
                                     generated for the texture when loading
                                     into OpenGL. */
} ktxTextureCreateInfo;

/**
 * @memberof ktxTexture
 * @~English
 * @brief Enum for requesting, or not, allocation of storage for images.
 *
 * @sa ktxTexture_Create()
 */
typedef enum
{
  KTX_TEXTURE_CREATE_NO_STORAGE = 0,   /*!< Don't allocate any image storage. */
  KTX_TEXTURE_CREATE_ALLOC_STORAGE = 1 /*!< Allocate image storage. */
} ktxTextureCreateStorageEnum;

/**
 * @memberof ktxTexture
 * @~English
 * @brief Flags for requesting services during creation.
 *
 * @sa ktxTexture_CreateFrom*
 */
enum ktxTextureCreateFlagBits
{
  KTX_TEXTURE_CREATE_NO_FLAGS = 0x00,
  KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT = 0x01,
  /*!< Load the images from the KTX source. */
  KTX_TEXTURE_CREATE_RAW_KVDATA_BIT = 0x02,
  /*!< Load the raw key-value data instead of
                                        creating a @c ktxHashList from it. */
  KTX_TEXTURE_CREATE_SKIP_KVDATA_BIT = 0x04
  /*!< Skip any key-value data. This overrides
                                        the RAW_KVDATA_BIT. */
};
/**
 * @memberof ktxTexture
 * @~English
 * @brief Type for TextureCreateFlags parameters.
 *
 * @sa ktxTexture_CreateFrom*()
 */
typedef ktx_uint32_t ktxTextureCreateFlags;

#define KTXAPIENTRY
#define KTXAPIENTRYP KTXAPIENTRY*
/**
 * @memberof ktxTexture
 * @~English
 * @brief Signature of function called by the <tt>ktxTexture_Iterate*</tt>
 *        functions to receive image data.
 *
 * The function parameters are used to pass values which change for each image.
 * Obtain values which are uniform across all images from the @c ktxTexture
 * object.
 *
 * @param [in] miplevel        MIP level from 0 to the max level which is
 *                             dependent on the texture size.
 * @param [in] face            usually 0; for cube maps, one of the 6 cube
 *                             faces in the order +X, -X, +Y, -Y, +Z, -Z,
 *                             0 to 5.
 * @param [in] width           width of the image.
 * @param [in] height          height of the image or, for 1D textures
 *                             textures, 1.
 * @param [in] depth           depth of the image or, for 1D & 2D
 *                             textures, 1.
 * @param [in] faceLodSize     number of bytes of data pointed at by
 *                             @p pixels.
 * @param [in] pixels          pointer to the image data.
 * @param [in,out] userdata    pointer for the application to pass data to and
 *                             from the callback function.
 */
/* Don't use KTXAPIENTRYP to avoid a Doxygen bug. */
typedef KTX_error_code(KTXAPIENTRY* PFNKTXITERCB)(int miplevel,
                                                  int face,
                                                  int width,
                                                  int height,
                                                  int depth,
                                                  ktx_uint32_t faceLodSize,
                                                  void* pixels,
                                                  void* userdata);

/*
 * See the implementation files for the full documentation of the following
 * functions.
 */

/*
 * Creates a ktxTexture from a block of memory containing KTX-formatted data.
 */
KTX_error_code
ktxTexture_CreateFromMemory(const ktx_uint8_t* bytes,
                            ktx_size_t size,
                            ktxTextureCreateFlags createFlags,
                            ktxTexture** newTex);
/*
 * Destroys a ktxTexture object.
 */
void
ktxTexture_Destroy(ktxTexture* This);


/*
 * Returns the offset of the image for the specified mip level, array layer
 * and face or depth slice within the image data of a ktxTexture object.
 */
KTX_error_code
ktxTexture_GetImageOffset(ktxTexture* This,
                          ktx_uint32_t level,
                          ktx_uint32_t layer,
                          ktx_uint32_t faceSlice,
                          ktx_size_t* pOffset);

/*
 * Returns the size of all the image data of a ktxTexture object in bytes.
 */
ktx_size_t
ktxTexture_GetSize(ktxTexture* This);

/*
 * Returns the size of an image at the specified level.
 */
ktx_size_t
ktxTexture_GetImageSize(ktxTexture* This, ktx_uint32_t level);

/*
 * Loads the image data into a ktxTexture object from the KTX-formatted source.
 * Used when the image data was not loaded during ktxTexture_CreateFrom*.
 */
KTX_error_code
ktxTexture_LoadImageData(ktxTexture* This,
                         ktx_uint8_t* pBuffer,
                         ktx_size_t bufSize);

#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

#define KTX2_IDENTIFIER_REF                                                    \
  {                                                                            \
    0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A     \
  }
#define KTX2_HEADER_SIZE (64)

/**
 * @internal
 * @~English
 * @brief KTX file header
 *
 * See the KTX specification for descriptions
 */
typedef struct KTX_header
{
  ktx_uint8_t identifier[12];
  ktx_uint32_t endianness;
  ktx_uint32_t glType;
  ktx_uint32_t glTypeSize;
  ktx_uint32_t glFormat;
  ktx_uint32_t glInternalformat;
  ktx_uint32_t glBaseInternalformat;
  ktx_uint32_t pixelWidth;
  ktx_uint32_t pixelHeight;
  ktx_uint32_t pixelDepth;
  ktx_uint32_t numberOfArrayElements;
  ktx_uint32_t numberOfFaces;
  ktx_uint32_t numberOfMipmapLevels;
  ktx_uint32_t bytesOfKeyValueData;
} KTX_header;

/**
 * @internal
 * @~English
 * @brief Structure for supplemental information about the texture.
 *
 * _ktxCheckHeader returns supplemental information about the texture in this
 * structure that is derived during checking of the file header.
 */
typedef struct KTX_supplemental_info
{
  ktx_uint8_t compressed;
  ktx_uint8_t generateMipmaps;
  ktx_uint16_t textureDimension;
} KTX_supplemental_info;
/**
 * @internal
 * @var ktx_uint8_t KTX_supplemental_info::compressed
 * @~English
 * @brief KTX_TRUE, if this a compressed texture, KTX_FALSE otherwise?
 */
/**
 * @internal
 * @var ktx_uint8_t KTX_supplemental_info::generateMipmaps
 * @~English
 * @brief KTX_TRUE, if mipmap generation is required, KTX_FALSE otherwise.
 */
/**
 * @internal
 * @var ktx_uint16_t KTX_supplemental_info::textureDimension
 * @~English
 * @brief The number of dimensions, 1, 2 or 3, of data in the texture image.
 */

/*
 * Pad nbytes to next multiple of n
 */

/*
 * Calculate bytes of of padding needed to reach next multiple of n.
 */
/* Equivalent to (n * ceil(nbytes / n)) - nbytes */
#define _KTX_PADN_LEN(n, nbytes) ((n - 1) - (nbytes + ((n - 1) & (n - 1))))

/*
 * Calculate bytes of of padding needed to reach KTX_GL_UNPACK_ALIGNMENT.
 */
#define _KTX_PAD_UNPACK_ALIGN_LEN(nbytes)                                      \
  _KTX_PADN_LEN(KTX_GL_UNPACK_ALIGNMENT, nbytes)

/*
 ======================================
     Internal ktxTexture functions
 ======================================
*/
ktx_size_t
ktxTexture_levelSize(ktxTexture* This, ktx_uint32_t level);

#ifdef __cplusplus
}
#endif

#endif /* KTX_H_A55A6F00956F42F3A137C11929827FE1 */
