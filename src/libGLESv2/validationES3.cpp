#include "precompiled.h"
//
// Copyright (c) 2013-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES3.cpp: Validation functions for OpenGL ES 3.0 entry point parameters

#include "libGLESv2/validationES3.h"
#include "libGLESv2/validationES.h"
#include "libGLESv2/Context.h"
#include "libGLESv2/Texture.h"
#include "libGLESv2/Framebuffer.h"
#include "libGLESv2/Renderbuffer.h"
#include "libGLESv2/formatutils.h"
#include "libGLESv2/main.h"
#include "libGLESv2/FramebufferAttachment.h"

#include "common/mathutil.h"

namespace gl
{

bool ValidateES3TexImageParameters(gl::Context *context, GLenum target, GLint level, GLenum internalformat, bool isCompressed, bool isSubImage,
                                   GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                   GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
    if (!ValidTexture2DDestinationTarget(context, target))
    {
        return gl::error(GL_INVALID_ENUM, false);
    }

    // Validate image size
    if (!ValidImageSize(context, target, level, width, height, depth))
    {
        return gl::error(GL_INVALID_VALUE, false);
    }

    // Verify zero border
    if (border != 0)
    {
        return gl::error(GL_INVALID_VALUE, false);
    }

    if (xoffset < 0 || yoffset < 0 || zoffset < 0 ||
        std::numeric_limits<GLsizei>::max() - xoffset < width ||
        std::numeric_limits<GLsizei>::max() - yoffset < height ||
        std::numeric_limits<GLsizei>::max() - zoffset < depth)
    {
        return gl::error(GL_INVALID_VALUE, false);
    }

    const gl::Caps &caps = context->getCaps();

    gl::Texture *texture = NULL;
    bool textureCompressed = false;
    GLenum textureInternalFormat = GL_NONE;
    GLint textureLevelWidth = 0;
    GLint textureLevelHeight = 0;
    GLint textureLevelDepth = 0;
    switch (target)
    {
      case GL_TEXTURE_2D:
        {
            if (static_cast<GLuint>(width) > (caps.max2DTextureSize >> level) ||
                static_cast<GLuint>(height) > (caps.max2DTextureSize >> level))
            {
                return gl::error(GL_INVALID_VALUE, false);
            }

            gl::Texture2D *texture2d = context->getTexture2D();
            if (texture2d)
            {
                textureCompressed = texture2d->isCompressed(level);
                textureInternalFormat = texture2d->getInternalFormat(level);
                textureLevelWidth = texture2d->getWidth(level);
                textureLevelHeight = texture2d->getHeight(level);
                textureLevelDepth = 1;
                texture = texture2d;
            }
        }
        break;

      case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
      case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
      case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
      case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
      case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
      case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        {
            if (!isSubImage && width != height)
            {
                return gl::error(GL_INVALID_VALUE, false);
            }

            if (static_cast<GLuint>(width) > (caps.maxCubeMapTextureSize >> level))
            {
                return gl::error(GL_INVALID_VALUE, false);
            }

            gl::TextureCubeMap *textureCube = context->getTextureCubeMap();
            if (textureCube)
            {
                textureCompressed = textureCube->isCompressed(target, level);
                textureInternalFormat = textureCube->getInternalFormat(target, level);
                textureLevelWidth = textureCube->getWidth(target, level);
                textureLevelHeight = textureCube->getHeight(target, level);
                textureLevelDepth = 1;
                texture = textureCube;
            }
        }
        break;

      case GL_TEXTURE_3D:
        {
            if (static_cast<GLuint>(width) > (caps.max3DTextureSize >> level) ||
                static_cast<GLuint>(height) > (caps.max3DTextureSize >> level) ||
                static_cast<GLuint>(depth) > (caps.max3DTextureSize >> level))
            {
                return gl::error(GL_INVALID_VALUE, false);
            }

            gl::Texture3D *texture3d = context->getTexture3D();
            if (texture3d)
            {
                textureCompressed = texture3d->isCompressed(level);
                textureInternalFormat = texture3d->getInternalFormat(level);
                textureLevelWidth = texture3d->getWidth(level);
                textureLevelHeight = texture3d->getHeight(level);
                textureLevelDepth = texture3d->getDepth(level);
                texture = texture3d;
            }
        }
        break;

        case GL_TEXTURE_2D_ARRAY:
          {
              if (static_cast<GLuint>(width) > (caps.max2DTextureSize >> level) ||
                  static_cast<GLuint>(height) > (caps.max2DTextureSize >> level) ||
                  static_cast<GLuint>(depth) > (caps.maxArrayTextureLayers >> level))
              {
                  return gl::error(GL_INVALID_VALUE, false);
              }

              gl::Texture2DArray *texture2darray = context->getTexture2DArray();
              if (texture2darray)
              {
                  textureCompressed = texture2darray->isCompressed(level);
                  textureInternalFormat = texture2darray->getInternalFormat(level);
                  textureLevelWidth = texture2darray->getWidth(level);
                  textureLevelHeight = texture2darray->getHeight(level);
                  textureLevelDepth = texture2darray->getLayers(level);
                  texture = texture2darray;
              }
          }
          break;

      default:
        return gl::error(GL_INVALID_ENUM, false);
    }

    if (!texture)
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    if (texture->isImmutable() && !isSubImage)
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    // Validate texture formats
    GLenum actualInternalFormat = isSubImage ? textureInternalFormat : internalformat;
    if (isCompressed)
    {
        if (!ValidCompressedImageSize(context, actualInternalFormat, width, height))
        {
            return gl::error(GL_INVALID_OPERATION, false);
        }

        if (!gl::IsFormatCompressed(actualInternalFormat))
        {
            return gl::error(GL_INVALID_ENUM, false);
        }

        if (target == GL_TEXTURE_3D)
        {
            return gl::error(GL_INVALID_OPERATION, false);
        }
    }
    else
    {
        // Note: dEQP 2013.4 expects an INVALID_VALUE error for TexImage3D with an invalid
        // internal format. (dEQP-GLES3.functional.negative_api.texture.teximage3d)
        if (!gl::IsValidInternalFormat(actualInternalFormat, context->getExtensions(), context->getClientVersion()) ||
            !gl::IsValidFormat(format, context->getExtensions(), context->getClientVersion()) ||
            !gl::IsValidType(type, context->getExtensions(), context->getClientVersion()))
        {
            return gl::error(GL_INVALID_ENUM, false);
        }

        if (!gl::IsValidFormatCombination(actualInternalFormat, format, type, context->getExtensions(), context->getClientVersion()))
        {
            return gl::error(GL_INVALID_OPERATION, false);
        }

        if (target == GL_TEXTURE_3D && (format == GL_DEPTH_COMPONENT || format == GL_DEPTH_STENCIL))
        {
            return gl::error(GL_INVALID_OPERATION, false);
        }
    }

    // Validate sub image parameters
    if (isSubImage)
    {
        if (isCompressed != textureCompressed)
        {
            return gl::error(GL_INVALID_OPERATION, false);
        }

        if (isCompressed)
        {
            if ((width % 4 != 0 && width != textureLevelWidth) ||
                (height % 4 != 0 && height != textureLevelHeight))
            {
                return gl::error(GL_INVALID_OPERATION, false);
            }
        }

        if (width == 0 || height == 0 || depth == 0)
        {
            return false;
        }

        if (xoffset < 0 || yoffset < 0 || zoffset < 0)
        {
            return gl::error(GL_INVALID_VALUE, false);
        }

        if (std::numeric_limits<GLsizei>::max() - xoffset < width ||
            std::numeric_limits<GLsizei>::max() - yoffset < height ||
            std::numeric_limits<GLsizei>::max() - zoffset < depth)
        {
            return gl::error(GL_INVALID_VALUE, false);
        }

        if (xoffset + width > textureLevelWidth ||
            yoffset + height > textureLevelHeight ||
            zoffset + depth > textureLevelDepth)
        {
            return gl::error(GL_INVALID_VALUE, false);
        }
    }

    // Check for pixel unpack buffer related API errors
    gl::Buffer *pixelUnpackBuffer = context->getState().getTargetBuffer(GL_PIXEL_UNPACK_BUFFER);
    if (pixelUnpackBuffer != NULL)
    {
        // ...the data would be unpacked from the buffer object such that the memory reads required
        // would exceed the data store size.
        size_t widthSize = static_cast<size_t>(width);
        size_t heightSize = static_cast<size_t>(height);
        size_t depthSize = static_cast<size_t>(depth);
        GLenum sizedFormat = gl::IsSizedInternalFormat(actualInternalFormat) ? actualInternalFormat
                                                                             : gl::GetSizedInternalFormat(actualInternalFormat, type);

        size_t pixelBytes = static_cast<size_t>(gl::GetPixelBytes(sizedFormat));

        if (!rx::IsUnsignedMultiplicationSafe(widthSize, heightSize) ||
            !rx::IsUnsignedMultiplicationSafe(widthSize * heightSize, depthSize) ||
            !rx::IsUnsignedMultiplicationSafe(widthSize * heightSize * depthSize, pixelBytes))
        {
            // Overflow past the end of the buffer
            return gl::error(GL_INVALID_OPERATION, false);
        }

        size_t copyBytes = widthSize * heightSize * depthSize * pixelBytes;
        size_t offset = reinterpret_cast<size_t>(pixels);

        if (!rx::IsUnsignedAdditionSafe(offset, copyBytes) ||
            ((offset + copyBytes) > static_cast<size_t>(pixelUnpackBuffer->getSize())))
        {
            // Overflow past the end of the buffer
            return gl::error(GL_INVALID_OPERATION, false);
        }

        // ...data is not evenly divisible into the number of bytes needed to store in memory a datum
        // indicated by type.
        size_t dataBytesPerPixel = static_cast<size_t>(gl::GetTypeBytes(type));

        if ((offset % dataBytesPerPixel) != 0)
        {
            return gl::error(GL_INVALID_OPERATION, false);
        }

        // ...the buffer object's data store is currently mapped.
        if (pixelUnpackBuffer->isMapped())
        {
            return gl::error(GL_INVALID_OPERATION, false);
        }
    }

    return true;
}

bool ValidateES3CopyTexImageParameters(gl::Context *context, GLenum target, GLint level, GLenum internalformat,
                                       bool isSubImage, GLint xoffset, GLint yoffset, GLint zoffset,
                                       GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
    GLenum textureInternalFormat;
    if (!ValidateCopyTexImageParametersBase(context, target, level, internalformat, isSubImage,
                                            xoffset, yoffset, zoffset, x, y, width, height,
                                            border, &textureInternalFormat))
    {
        return false;
    }

    gl::Framebuffer *framebuffer = context->getState().getReadFramebuffer();

    if (framebuffer->completeness() != GL_FRAMEBUFFER_COMPLETE)
    {
        return gl::error(GL_INVALID_FRAMEBUFFER_OPERATION, false);
    }

    if (context->getState().getReadFramebuffer()->id() != 0 && framebuffer->getSamples() != 0)
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    gl::FramebufferAttachment *source = framebuffer->getReadColorbuffer();
    GLenum colorbufferInternalFormat = source->getInternalFormat();

    if (isSubImage)
    {
        if (!gl::IsValidCopyTexImageCombination(textureInternalFormat, colorbufferInternalFormat,
                                                context->getState().getReadFramebuffer()->id(),
                                                context->getClientVersion()))
        {
            return gl::error(GL_INVALID_OPERATION, false);
        }
    }
    else
    {
        if (!gl::IsValidCopyTexImageCombination(internalformat, colorbufferInternalFormat,
                                                context->getState().getReadFramebuffer()->id(),
                                                context->getClientVersion()))
        {
            return gl::error(GL_INVALID_OPERATION, false);
        }
    }

    // If width or height is zero, it is a no-op.  Return false without setting an error.
    return (width > 0 && height > 0);
}

bool ValidateES3TexStorageParameters(gl::Context *context, GLenum target, GLsizei levels, GLenum internalformat,
                                     GLsizei width, GLsizei height, GLsizei depth)
{
    if (width < 1 || height < 1 || depth < 1 || levels < 1)
    {
        return gl::error(GL_INVALID_VALUE, false);
    }

    if (levels > gl::log2(std::max(std::max(width, height), depth)) + 1)
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    const gl::Caps &caps = context->getCaps();

    gl::Texture *texture = NULL;
    switch (target)
    {
      case GL_TEXTURE_2D:
        {
            texture = context->getTexture2D();

            if (static_cast<GLuint>(width) > caps.max2DTextureSize ||
                static_cast<GLuint>(height) > caps.max2DTextureSize)
            {
                return gl::error(GL_INVALID_VALUE, false);
            }
        }
        break;

      case GL_TEXTURE_CUBE_MAP:
        {
            texture = context->getTextureCubeMap();

            if (width != height)
            {
                return gl::error(GL_INVALID_VALUE, false);
            }

            if (static_cast<GLuint>(width) > caps.maxCubeMapTextureSize)
            {
                return gl::error(GL_INVALID_VALUE, false);
            }
        }
        break;

      case GL_TEXTURE_3D:
        {
            texture = context->getTexture3D();

            if (static_cast<GLuint>(width) > caps.max3DTextureSize ||
                static_cast<GLuint>(height) > caps.max3DTextureSize ||
                static_cast<GLuint>(depth) > caps.max3DTextureSize)
            {
                return gl::error(GL_INVALID_VALUE, false);
            }
        }
        break;

      case GL_TEXTURE_2D_ARRAY:
        {
            texture = context->getTexture2DArray();

            if (static_cast<GLuint>(width) > caps.max2DTextureSize ||
                static_cast<GLuint>(height) > caps.max2DTextureSize ||
                static_cast<GLuint>(depth) > caps.maxArrayTextureLayers)
            {
                return gl::error(GL_INVALID_VALUE, false);
            }
        }
        break;

      default:
        return gl::error(GL_INVALID_ENUM, false);
    }

    if (!texture || texture->id() == 0)
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    if (texture->isImmutable())
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    if (!gl::IsValidInternalFormat(internalformat, context->getExtensions(), context->getClientVersion()))
    {
        return gl::error(GL_INVALID_ENUM, false);
    }

    if (!gl::IsSizedInternalFormat(internalformat))
    {
        return gl::error(GL_INVALID_ENUM, false);
    }

    return true;
}

bool ValidateFramebufferTextureLayer(const gl::Context *context, GLenum target, GLenum attachment,
                                     GLuint texture, GLint level, GLint layer)
{
    if (context->getClientVersion() < 3)
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    if (layer < 0)
    {
        return gl::error(GL_INVALID_VALUE, false);
    }

    if (!ValidateFramebufferTextureBase(context, target, attachment, texture, level))
    {
        return false;
    }

    const gl::Caps &caps = context->getCaps();
    if (texture != 0)
    {
        gl::Texture *tex = context->getTexture(texture);
        ASSERT(tex);

        switch (tex->getTarget())
        {
          case GL_TEXTURE_2D_ARRAY:
            {
                if (level > gl::log2(caps.max2DTextureSize))
                {
                    return gl::error(GL_INVALID_VALUE, false);
                }

                if (static_cast<GLuint>(layer) >= caps.maxArrayTextureLayers)
                {
                    return gl::error(GL_INVALID_VALUE, false);
                }

                gl::Texture2DArray *texArray = static_cast<gl::Texture2DArray *>(tex);
                if (texArray->isCompressed(level))
                {
                    return gl::error(GL_INVALID_OPERATION, false);
                }
            }
            break;

          case GL_TEXTURE_3D:
            {
                if (level > gl::log2(caps.max3DTextureSize))
                {
                    return gl::error(GL_INVALID_VALUE, false);
                }

                if (static_cast<GLuint>(layer) >= caps.max3DTextureSize)
                {
                    return gl::error(GL_INVALID_VALUE, false);
                }

                gl::Texture3D *tex3d = static_cast<gl::Texture3D *>(tex);
                if (tex3d->isCompressed(level))
                {
                    return gl::error(GL_INVALID_OPERATION, false);
                }
            }
            break;

          default:
            return gl::error(GL_INVALID_OPERATION, false);
        }
    }

    return true;
}

bool ValidES3ReadFormatType(gl::Context *context, GLenum internalFormat, GLenum format, GLenum type)
{
    switch (format)
    {
      case GL_RGBA:
        switch (type)
        {
          case GL_UNSIGNED_BYTE:
            break;
          case GL_UNSIGNED_INT_2_10_10_10_REV:
            if (internalFormat != GL_RGB10_A2)
            {
                return false;
            }
            break;
          case GL_FLOAT:
            if (gl::GetComponentType(internalFormat) != GL_FLOAT)
            {
                return false;
            }
            break;
          default:
            return false;
        }
        break;
      case GL_RGBA_INTEGER:
        switch (type)
        {
          case GL_INT:
            if (gl::GetComponentType(internalFormat) != GL_INT)
            {
                return false;
            }
            break;
          case GL_UNSIGNED_INT:
            if (gl::GetComponentType(internalFormat) != GL_UNSIGNED_INT)
            {
                return false;
            }
            break;
          default:
            return false;
        }
        break;
      case GL_BGRA_EXT:
        switch (type)
        {
          case GL_UNSIGNED_BYTE:
          case GL_UNSIGNED_SHORT_4_4_4_4_REV_EXT:
          case GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT:
            break;
          default:
            return false;
        }
        break;
      case GL_RG_EXT:
      case GL_RED_EXT:
        if (!context->getExtensions().textureRG)
        {
            return false;
        }
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            break;
        default:
            return false;
        }
        break;
      default:
        return false;
    }
    return true;
}

bool ValidateInvalidateFramebufferParameters(gl::Context *context, GLenum target, GLsizei numAttachments,
                                             const GLenum* attachments)
{
    bool defaultFramebuffer = false;

    switch (target)
    {
      case GL_DRAW_FRAMEBUFFER:
      case GL_FRAMEBUFFER:
        defaultFramebuffer = context->getState().getDrawFramebuffer()->id() == 0;
        break;
      case GL_READ_FRAMEBUFFER:
        defaultFramebuffer = context->getState().getReadFramebuffer()->id() == 0;
        break;
      default:
        return gl::error(GL_INVALID_ENUM, false);
    }

    for (int i = 0; i < numAttachments; ++i)
    {
        if (attachments[i] >= GL_COLOR_ATTACHMENT0 && attachments[i] <= GL_COLOR_ATTACHMENT15)
        {
            if (defaultFramebuffer)
            {
                return gl::error(GL_INVALID_ENUM, false);
            }

            if (attachments[i] >= GL_COLOR_ATTACHMENT0 + context->getCaps().maxColorAttachments)
            {
                return gl::error(GL_INVALID_OPERATION, false);
            }
        }
        else
        {
            switch (attachments[i])
            {
              case GL_DEPTH_ATTACHMENT:
              case GL_STENCIL_ATTACHMENT:
              case GL_DEPTH_STENCIL_ATTACHMENT:
                if (defaultFramebuffer)
                {
                    return gl::error(GL_INVALID_ENUM, false);
                }
                break;
              case GL_COLOR:
              case GL_DEPTH:
              case GL_STENCIL:
                if (!defaultFramebuffer)
                {
                    return gl::error(GL_INVALID_ENUM, false);
                }
                break;
              default:
                return gl::error(GL_INVALID_ENUM, false);
            }
        }
    }

    return true;
}

bool ValidateClearBuffer(const gl::Context *context)
{
    if (context->getClientVersion() < 3)
    {
        return gl::error(GL_INVALID_OPERATION, false);
    }

    const gl::Framebuffer *fbo = context->getState().getDrawFramebuffer();
    if (!fbo || fbo->completeness() != GL_FRAMEBUFFER_COMPLETE)
    {
        return gl::error(GL_INVALID_FRAMEBUFFER_OPERATION, false);
    }

    return true;
}

}
