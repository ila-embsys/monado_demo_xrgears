/* -*- tab-width: 4; -*- */
/* vi: set sw=2 ts=4 expandtab: */

/*
 * ©2018 Mark Callow.
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
 * @file writer.c
 * @~English
 *
 * @brief ktxTexture implementation.
 *
 * @author Mark Callow, www.edgewise-consulting.com
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "ktx_texture.h"
#include "ktx_stream.h"

typedef enum GlFormatSizeFlagBits
{
  GL_FORMAT_SIZE_PACKED_BIT = 0x00000001,
  GL_FORMAT_SIZE_COMPRESSED_BIT = 0x00000002,
  GL_FORMAT_SIZE_PALETTIZED_BIT = 0x00000004,
  GL_FORMAT_SIZE_DEPTH_BIT = 0x00000008,
  GL_FORMAT_SIZE_STENCIL_BIT = 0x00000010,
} GlFormatSizeFlagBits;

typedef unsigned int GlFormatSizeFlags;

typedef struct GlFormatSize
{
  GlFormatSizeFlags flags;
  unsigned int paletteSizeInBits;
  unsigned int blockSizeInBits;
  unsigned int blockWidth;  // in texels
  unsigned int blockHeight; // in texels
  unsigned int blockDepth;  // in texels
} GlFormatSize;

/**
 * @internal
 * @~English
 * @brief Internal ktxTexture structure.
 *
 * This is kept hidden to avoid burdening applications with the definitions
 * of GlFormatSize and ktxStream.
 */
typedef struct _ktxTextureInt
{
  ktxTexture super;        /*!< Base ktxTexture class. */
  GlFormatSize formatInfo; /*!< Info about the image data format. */
  // The following are needed because image data reading can be delayed.
  ktx_uint32_t glTypeSize; /*!< Size of the image data type in bytes. */
  ktxStream stream;        /*!< Stream connected to KTX source. */
} ktxTextureInt;

ktx_size_t
ktxTexture_GetSize(ktxTexture* This);
KTX_error_code
ktxTexture_LoadImageData(ktxTexture* This,
                         ktx_uint8_t* pBuffer,
                         ktx_size_t bufSize);

static ktx_uint32_t
padRow(ktx_uint32_t* rowBytes);


/**
 * @memberof ktxTexture @private
 * @brief Construct a ktxTexture from a ktxStream reading from a KTX source.
 *
 * The caller constructs the stream inside the ktxTextureInt before calling
 * this.
 *
 * The create flag KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT should not be set,
 * if the ktxTexture is ultimately to be uploaded to OpenGL or Vulkan. This
 * will minimize memory usage by allowing, for example, loading the images
 * directly from the source into a Vulkan staging buffer.
 *
 * The create flag KTX_TEXTURE_CREATE_RAW_KVDATA_BIT should not be used. It is
 * provided solely to enable implementation of the @e libktx v1 API on top of
 * ktxTexture.
 *
 * @param[in] This pointer to a ktxTextureInt-sized block of memory to
 *                 initialize.
 * @param[in] createFlags bitmask requesting specific actions during creation.
 *
 * @return      KTX_SUCCESS on success, other KTX_* enum values on error.
 *
 * @exception KTX_FILE_DATA_ERROR
 *                              Source data is inconsistent with the KTX
 *                              specification.
 * @exception KTX_FILE_READ_ERROR
 *                              An error occurred while reading the source.
 * @exception KTX_FILE_UNEXPECTED_EOF
 *                              Not enough data in the source.
 * @exception KTX_OUT_OF_MEMORY Not enough memory to load either the images or
 *                              the key-value data.
 * @exception KTX_UNKNOWN_FILE_FORMAT
 *                              The source is not in KTX format.
 * @exception KTX_UNSUPPORTED_TEXTURE_TYPE
 *                              The source describes a texture type not
 *                              supported by OpenGL or Vulkan, e.g, a 3D array.
 */
static KTX_error_code
ktxTextureInt_constructFromStream(ktxTextureInt* This,
                                  ktxTextureCreateFlags createFlags)
{
  ktxTexture* super = (ktxTexture*)This;
  KTX_error_code result;
  KTX_header header;
  KTX_supplemental_info suppInfo;
  ktxStream* stream;
  ktx_off_t pos;
  ktx_size_t size;

  assert(This != NULL);
  assert(This->stream.data.mem != NULL);
  assert(This->stream.type == eStreamTypeFile ||
         This->stream.type == eStreamTypeMemory);
  stream = &This->stream;

  // Read header.
  result = stream->read(stream, &header, KTX_HEADER_SIZE);
  if (result != KTX_SUCCESS)
    return result;

  // Hardcode header info for xrgears
  suppInfo.textureDimension = 2;
  suppInfo.compressed = 1;
  header.numberOfMipmapLevels = 1;

  /*
   * Initialize from header info.
   */
  super->glFormat = header.glFormat;
  super->glInternalformat = header.glInternalformat;
  super->glType = header.glType;
  super->glBaseInternalformat = header.glBaseInternalformat;
  super->numDimensions = suppInfo.textureDimension;
  super->baseWidth = header.pixelWidth;
  assert(suppInfo.textureDimension > 0 && suppInfo.textureDimension < 4);
  switch (suppInfo.textureDimension) {
  case 1: super->baseHeight = super->baseDepth = 1; break;
  case 2:
    super->baseHeight = header.pixelHeight;
    super->baseDepth = 1;
    break;
  case 3:
    super->baseHeight = header.pixelHeight;
    super->baseDepth = header.pixelDepth;
    break;
  }
  if (header.numberOfArrayElements > 0) {
    super->numLayers = header.numberOfArrayElements;
  } else {
    super->numLayers = 1;
  }
  super->numFaces = header.numberOfFaces;
  super->numLevels = header.numberOfMipmapLevels;
  super->isCompressed = suppInfo.compressed;
  This->glTypeSize = header.glTypeSize;

  /*
   * Get the size of the image data.
   */
  result = stream->getsize(stream, &size);
  if (result == KTX_SUCCESS) {
    result = stream->getpos(stream, &pos);
    if (result == KTX_SUCCESS)
      super->dataSize = size -
                        pos
                        /* Remove space for faceLodSize fields */
                        - super->numLevels * sizeof(ktx_uint32_t);
  }

  /*
   * Load the images, if requested.
   */
  if (result == KTX_SUCCESS &&
      (createFlags & KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT)) {
    result = ktxTexture_LoadImageData((ktxTexture*)super, NULL, 0);
  }
  return result;
}

/**
 * @memberof ktxTexture @private
 * @brief Construct a ktxTexture from KTX-formatted data in memory.
 *
 * See ktxTextureInt_constructFromStream for details.
 *
 * @param[in] This  pointer to a ktxTextureInt-sized block of memory to
 *                  initialize.
 * @param[in] bytes pointer to the memory containing the serialized KTX data.
 * @param[in] size  length of the KTX data in bytes.
 * @param[in] createFlags bitmask requesting specific actions during creation.
 *
 * @return      KTX_SUCCESS on success, other KTX_* enum values on error.
 *
 * @exception KTX_INVALID_VALUE Either @p bytes is NULL or @p size is 0.
 *
 * For other exceptions, see ktxTexture_constructFromStream().
 */
static KTX_error_code
ktxTextureInt_constructFromMemory(ktxTextureInt* This,
                                  const ktx_uint8_t* bytes,
                                  ktx_size_t size,
                                  ktxTextureCreateFlags createFlags)
{
  KTX_error_code result;

  if (bytes == NULL || size == 0)
    return KTX_INVALID_VALUE;

  memset(This, 0, sizeof(*This));

  result = ktxMemStream_construct_ro(&This->stream, bytes, size);
  if (result == KTX_SUCCESS)
    result = ktxTextureInt_constructFromStream(This, createFlags);

  return result;
}

/**
 * @memberof ktxTexture @private
 * @~English
 * @brief Free the memory associated with the texture contents
 *
 * @param[in] This pointer to the ktxTextureInt whose texture contents are
 *                 to be freed.
 */
void
ktxTextureInt_destruct(ktxTextureInt* This)
{
  ktxTexture* super = (ktxTexture*)This;
  if (This->stream.data.file != NULL)
    This->stream.destruct(&This->stream);
  // if (super->kvDataHead != NULL)
  //      ktxHashList_Destruct(&super->kvDataHead);
  if (super->kvData != NULL)
    free(super->kvData);
  if (super->pData != NULL)
    free(super->pData);
}

/**
 * @defgroup reader Reader
 * @brief Read KTX-formatted data.
 * @{
 */

/**
 * @memberof ktxTexture
 * @~English
 * @brief Create a ktxTexture from KTX-formatted data in memory.
 *
 * The address of a newly created ktxTexture reflecting the contents of the
 * serialized KTX data is written to the location pointed at by @p newTex.
 *
 * The create flag KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT should not be set,
 * if the ktxTexture is ultimately to be uploaded to OpenGL or Vulkan. This
 * will minimize memory usage by allowing, for example, loading the images
 * directly from the source into a Vulkan staging buffer.
 *
 * The create flag KTX_TEXTURE_CREATE_RAW_KVDATA_BIT should not be used. It is
 * provided solely to enable implementation of the @e libktx v1 API on top of
 * ktxTexture.
 *
 * @param[in] bytes pointer to the memory containing the serialized KTX data.
 * @param[in] size  length of the KTX data in bytes.
 * @param[in] createFlags bitmask requesting specific actions during creation.
 * @param[in,out] newTex  pointer to a location in which store the address of
 *                        the newly created texture.
 *
 * @return      KTX_SUCCESS on success, other KTX_* enum values on error.
 *
 * @exception KTX_INVALID_VALUE Either @p bytes is NULL or @p size is 0.
 *
 * For other exceptions, see ktxTexture_CreateFromStdioStream().
 */
KTX_error_code
ktxTexture_CreateFromMemory(const ktx_uint8_t* bytes,
                            ktx_size_t size,
                            ktxTextureCreateFlags createFlags,
                            ktxTexture** newTex)
{
  KTX_error_code result;
  if (newTex == NULL)
    return KTX_INVALID_VALUE;

  ktxTextureInt* tex = (ktxTextureInt*)malloc(sizeof(ktxTextureInt));
  if (tex == NULL)
    return KTX_OUT_OF_MEMORY;

  result = ktxTextureInt_constructFromMemory(tex, bytes, size, createFlags);
  if (result == KTX_SUCCESS)
    *newTex = (ktxTexture*)tex;
  else {
    free(tex);
    *newTex = NULL;
  }
  return result;
}

/**
 * @memberof ktxTexture
 * @~English
 * @brief Destroy a ktxTexture object.
 *
 * This frees the memory associated with the texture contents and the memory
 * of the ktxTexture object. This does @e not delete any OpenGL or Vulkan
 * texture objects created by ktxTexture_GLUpload or ktxTexture_VkUpload.
 *
 * @param[in] This pointer to the ktxTexture object to destroy
 */
void
ktxTexture_Destroy(ktxTexture* This)
{
  ktxTextureInt_destruct((ktxTextureInt*)This);
  free(This);
}

/**
 * @memberof ktxTexture
 * @~English
 * @brief Return the total size of the texture image data in bytes.
 *
 * @param[in] This pointer to the ktxTexture object of interest.
 */
ktx_size_t
ktxTexture_GetSize(ktxTexture* This)
{
  assert(This != NULL);
  return This->dataSize;
}

/**
 * @memberof ktxTexture
 * @~English
 * @brief Calculate & return the size in bytes of an image at the specified
 *        mip level.
 *
 * For arrays, this is the size of layer, for cubemaps, the size of a face
 * and for 3D textures, the size of a depth slice.
 *
 * The size reflects the padding of each row to KTX_GL_UNPACK_ALIGNMENT.
 *
 * @param[in]     This     pointer to the ktxTexture object of interest.
 * @param[in]     level    level of interest. *
 */
ktx_size_t
ktxTexture_GetImageSize(ktxTexture* This, ktx_uint32_t level)
{
  GlFormatSize* formatInfo;
  struct blockCount
  {
    ktx_uint32_t x, y, z;
  } blockCount;
  ktx_uint32_t blockSizeInBytes;
  ktx_uint32_t rowBytes;

  assert(This != NULL);

  formatInfo = &((ktxTextureInt*)This)->formatInfo;
  blockCount.x = MAX(1, (This->baseWidth / formatInfo->blockWidth) >> level);
  blockCount.y = MAX(1, (This->baseHeight / formatInfo->blockHeight) >> level);
  blockSizeInBytes = formatInfo->blockSizeInBits / 8;

  if (formatInfo->flags & GL_FORMAT_SIZE_COMPRESSED_BIT) {
    assert(This->isCompressed);
    return blockCount.x * blockCount.y * blockSizeInBytes;
  } else {
    rowBytes = blockCount.x * blockSizeInBytes;
    (void)padRow(&rowBytes);
    return rowBytes * blockCount.y;
  }
}

/**
 * @memberof ktxTexture
 * @~English
 * @brief Load all the image data from the ktxTexture's source.
 *
 * The data is loaded into the provided buffer or to an internally allocated
 * buffer, if @p pBuffer is @c NULL.
 *
 * @param[in] This pointer to the ktxTexture object of interest.
 * @param[in] pBuffer pointer to the buffer in which to load the image data.
 * @param[in] bufSize size of the buffer pointed at by @p pBuffer.
 *
 * @return      KTX_SUCCESS on success, other KTX_* enum values on error.
 *
 * @exception KTX_INVALID_VALUE @p This is NULL.
 * @exception KTX_INVALID_VALUE @p bufSize is less than the the image data size.
 * @exception KTX_INVALID_OPERATION
 *                              The data has already been loaded or the
 *                              ktxTexture was not created from a KTX source.
 * @exception KTX_OUT_OF_MEMORY Insufficient memory for the image data.
 */
KTX_error_code
ktxTexture_LoadImageData(ktxTexture* This,
                         ktx_uint8_t* pBuffer,
                         ktx_size_t bufSize)
{
  ktxTextureInt* subthis = (ktxTextureInt*)This;
  ktx_uint32_t miplevel;
  ktx_uint8_t* pDest;
  KTX_error_code result = KTX_SUCCESS;

  if (This == NULL)
    return KTX_INVALID_VALUE;

  if (subthis->stream.data.file == NULL)
    // This Texture not created from a stream or images already loaded;
    return KTX_INVALID_OPERATION;

  if (pBuffer == NULL) {
    This->pData = malloc(This->dataSize);
    if (This->pData == NULL)
      return KTX_OUT_OF_MEMORY;
    pDest = This->pData;
  } else if (bufSize < This->dataSize) {
    return KTX_INVALID_VALUE;
  } else {
    pDest = pBuffer;
  }

  // Need to loop through for correct byte swapping
  for (miplevel = 0; miplevel < This->numLevels; ++miplevel) {
    ktx_uint32_t faceLodSize;
    ktx_uint32_t faceLodSizePadded;
    ktx_uint32_t face;
    ktx_uint32_t innerIterations;

    result = subthis->stream.read(&subthis->stream, &faceLodSize,
                                  sizeof(ktx_uint32_t));
    if (result != KTX_SUCCESS) {
      goto cleanup;
    }

    faceLodSizePadded = faceLodSize;

    innerIterations = 1;
    for (face = 0; face < innerIterations; ++face) {
      result = subthis->stream.read(&subthis->stream, pDest, faceLodSizePadded);
      if (result != KTX_SUCCESS) {
        goto cleanup;
      }

      pDest += faceLodSizePadded;
    }
  }

cleanup:
  // No further need for This->
  subthis->stream.destruct(&subthis->stream);
  return result;
}

/**
 * @internal
 * @brief  Calculate and apply the padding needed to comply with
 *         KTX_GL_UNPACK_ALIGNMENT.
 *
 * For uncompressed textures, KTX format specifies KTX_GL_UNPACK_ALIGNMENT = 4.
 *
 * @param[in,out] rowBytes    pointer to variable containing the packed no. of
 *                            bytes in a row. The no. of bytes after padding
 *                            is written into this location.
 * @return the no. of bytes of padding.
 */
static ktx_uint32_t
padRow(ktx_uint32_t* rowBytes)
{
  ktx_uint32_t rowPadding;

  assert(rowBytes != NULL);

  rowPadding = _KTX_PAD_UNPACK_ALIGN_LEN(*rowBytes);
  *rowBytes += rowPadding;
  return rowPadding;
}

/**
 * @memberof ktxTexture @private
 * @~English
 * @brief Calculate the size of an array layer at the specified mip level.
 *
 * The size of a layer is the size of an image * either the number of faces
 * or the number of depth slices. This is the size of a layer as needed to
 * find the offset within the array of images of a level and layer so the size
 * reflects any @c cubePadding.
 *
 * @param[in]  This     pointer to the ktxTexture object of interest.
 * @param[in] level     level whose layer size to return.
 *
 * @return the layer size in bytes.
 */
static inline ktx_size_t
ktxTexture_layerSize(ktxTexture* This, ktx_uint32_t level)
{
  /*
   * As there are no 3D cubemaps, the image's z block count will always be
   * 1 for cubemaps and numFaces will always be 1 for 3D textures so the
   * multiply is safe. 3D cubemaps, if they existed, would require
   * imageSize * (blockCount.z + This->numFaces);
   */
  GlFormatSize* formatInfo;
  ktx_uint32_t blockCountZ;
  ktx_size_t imageSize, layerSize;

  assert(This != NULL);

  formatInfo = &((ktxTextureInt*)This)->formatInfo;
  blockCountZ = MAX(1, (This->baseDepth / formatInfo->blockDepth) >> level);
  imageSize = ktxTexture_GetImageSize(This, level);
  layerSize = imageSize * blockCountZ;
  return layerSize * This->numFaces;
}

/**
 * @memberof ktxTexture @private
 * @~English
 * @brief Calculate the size of the specified mip level.
 *
 * The size of a level is the size of a layer * the number of layers.
 *
 * @param[in]  This     pointer to the ktxTexture object of interest.
 * @param[in] level     level whose layer size to return.
 *
 * @return the level size in bytes.
 */
ktx_size_t
ktxTexture_levelSize(ktxTexture* This, ktx_uint32_t level)
{
  assert(This != NULL);
  return ktxTexture_layerSize(This, level) * This->numLayers;
}

/**
 * @memberof ktxTexture @private
 * @~English
 * @brief Calculate the size of the image data for the specified number
 *        of levels.
 *
 * The data size is the sum of the sizes of each level up to the number
 * specified and includes any @c mipPadding.
 *
 * @param[in] This     pointer to the ktxTexture object of interest.
 * @param[in] levels   number of levels whose data size to return.
 *
 * @return the data size in bytes.
 */
static inline ktx_size_t
ktxTexture_dataSize(ktxTexture* This, ktx_uint32_t levels)
{
  ktx_uint32_t i;
  ktx_size_t dataSize = 0;

  assert(This != NULL);
  for (i = 0; i < levels; i++) {
    ktx_size_t levelSize = ktxTexture_levelSize(This, i);
    dataSize += levelSize;
  }
  return dataSize;
}

/**
 * @memberof ktxTexture
 * @~English
 * @brief Find the offset of an image within a ktxTexture's image data.
 *
 * As there is no such thing as a 3D cubemap we make the 3rd location parameter
 * do double duty.
 *
 * @param[in]     This      pointer to the ktxTexture object of interest.
 * @param[in]     level     mip level of the image.
 * @param[in]     layer     array layer of the image.
 * @param[in]     faceSlice cube map face or depth slice of the image.
 * @param[in,out] pOffset   pointer to location to store the offset.
 *
 * @return  KTX_SUCCESS on success, other KTX_* enum values on error.
 *
 * @exception KTX_INVALID_OPERATION
 *                         @p level, @p layer or @p faceSlice exceed the
 *                         dimensions of the texture.
 * @exception KTX_INVALID_VALID @p This is NULL.
 */
KTX_error_code
ktxTexture_GetImageOffset(ktxTexture* This,
                          ktx_uint32_t level,
                          ktx_uint32_t layer,
                          ktx_uint32_t faceSlice,
                          ktx_size_t* pOffset)
{
  if (This == NULL)
    return KTX_INVALID_VALUE;

  if (level >= This->numLevels || layer >= This->numLayers)
    return KTX_INVALID_OPERATION;


  ktx_uint32_t maxSlice = MAX(1, This->baseDepth >> level);
  if (faceSlice >= maxSlice)
    return KTX_INVALID_OPERATION;


  // Get the size of the data up to the start of the indexed level.
  *pOffset = ktxTexture_dataSize(This, level);

  // All layers, faces & slices within a level are the same size.
  if (layer != 0) {
    ktx_size_t layerSize;
    layerSize = ktxTexture_layerSize(This, level);
    *pOffset += layer * layerSize;
  }
  if (faceSlice != 0) {
    ktx_size_t imageSize;
    imageSize = ktxTexture_GetImageSize(This, level);
    *pOffset += faceSlice * imageSize;
  }

  return KTX_SUCCESS;
}
