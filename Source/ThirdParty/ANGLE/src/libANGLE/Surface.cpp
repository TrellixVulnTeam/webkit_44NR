//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Surface.cpp: Implements the egl::Surface class, representing a drawing surface
// such as the client area of a window, including any back buffers.
// Implements EGLSurface and related functionality. [EGL 1.4] section 2.2 page 3.

#include "libANGLE/Surface.h"

#include <EGL/eglext.h>

#include <iostream>

#include "libANGLE/Config.h"
#include "libANGLE/Display.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/Texture.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/EGLImplFactory.h"

namespace egl
{

SurfaceState::SurfaceState(const egl::Config *configIn)
    : defaultFramebuffer(nullptr), config(configIn)
{
}

Surface::Surface(EGLint surfaceType, const egl::Config *config, const AttributeMap &attributes)
    : FramebufferAttachmentObject(),
      mState(config),
      mImplementation(nullptr),
      mCurrentCount(0),
      mDestroyed(false),
      mType(surfaceType),
      mPostSubBufferRequested(false),
      mFixedSize(false),
      mFixedWidth(0),
      mFixedHeight(0),
      mTextureFormat(EGL_NO_TEXTURE),
      mTextureTarget(EGL_NO_TEXTURE),
      // FIXME: Determine actual pixel aspect ratio
      mPixelAspectRatio(static_cast<EGLint>(1.0 * EGL_DISPLAY_SCALING)),
      mRenderBuffer(EGL_BACK_BUFFER),
      mSwapBehavior(EGL_NONE),
      mOrientation(0),
      mTexture(),
      mBackFormat(config->renderTargetFormat),
      mDSFormat(config->depthStencilFormat)
{
    mPostSubBufferRequested = (attributes.get(EGL_POST_SUB_BUFFER_SUPPORTED_NV, EGL_FALSE) == EGL_TRUE);
    mFlexibleSurfaceCompatibilityRequested =
        (attributes.get(EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE, EGL_FALSE) == EGL_TRUE);

    mDirectComposition = (attributes.get(EGL_DIRECT_COMPOSITION_ANGLE, EGL_FALSE) == EGL_TRUE);

    mFixedSize = (attributes.get(EGL_FIXED_SIZE_ANGLE, EGL_FALSE) == EGL_TRUE);
    if (mFixedSize)
    {
        mFixedWidth  = static_cast<size_t>(attributes.get(EGL_WIDTH, 0));
        mFixedHeight = static_cast<size_t>(attributes.get(EGL_HEIGHT, 0));
    }

    if (mType != EGL_WINDOW_BIT)
    {
        mTextureFormat = static_cast<EGLenum>(attributes.get(EGL_TEXTURE_FORMAT, EGL_NO_TEXTURE));
        mTextureTarget = static_cast<EGLenum>(attributes.get(EGL_TEXTURE_TARGET, EGL_NO_TEXTURE));
    }

    mOrientation = static_cast<EGLint>(attributes.get(EGL_SURFACE_ORIENTATION_ANGLE, 0));
}

Surface::~Surface()
{
    if (mTexture.get())
    {
        if (mImplementation)
        {
            mImplementation->releaseTexImage(EGL_BACK_BUFFER);
        }
        mTexture->releaseTexImageFromSurface();
        mTexture.set(nullptr);
    }

    if (mState.defaultFramebuffer)
        SafeDelete(mState.defaultFramebuffer);
    if (mImplementation)
        SafeDelete(mImplementation);
}

void Surface::destroy(const Display *display)
{
    if (mState.defaultFramebuffer)
    {
        mState.defaultFramebuffer->destroyDefault(display);
    }
    if (mImplementation)
    {
        mImplementation->destroy(rx::SafeGetImpl(display));
    }
    delete this;
}

Error Surface::initialize(const Display &display)
{
    ANGLE_TRY(mImplementation->initialize(display.getImplementation()));

    // Initialized here since impl is nullptr in the constructor.
    // Must happen after implementation initialize for Android.
    mSwapBehavior = mImplementation->getSwapBehavior();

    // Must happen after implementation initialize for OSX.
    mState.defaultFramebuffer = createDefaultFramebuffer();
    ASSERT(mState.defaultFramebuffer != nullptr);

    return Error(EGL_SUCCESS);
}

void Surface::setIsCurrent(Display *display, bool isCurrent)
{
    if (isCurrent)
    {
        mCurrentCount++;
        return;
    }

    ASSERT(mCurrentCount > 0);
    mCurrentCount--;
    if (mCurrentCount == 0 && mDestroyed)
    {
        destroy(display);
    }
}

void Surface::onDestroy(Display *display)
{
    mDestroyed = true;
    if (mCurrentCount == 0)
    {
        destroy(display);
    }
}

EGLint Surface::getType() const
{
    return mType;
}

Error Surface::swap(const Display &display)
{
    return mImplementation->swap(display.getImplementation());
}

Error Surface::swapWithDamage(EGLint *rects, EGLint n_rects)
{
    return mImplementation->swapWithDamage(rects, n_rects);
}

Error Surface::postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height)
{
    return mImplementation->postSubBuffer(x, y, width, height);
}

Error Surface::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    return mImplementation->querySurfacePointerANGLE(attribute, value);
}

EGLint Surface::isPostSubBufferSupported() const
{
    return mPostSubBufferRequested && mImplementation->isPostSubBufferSupported();
}

void Surface::setSwapInterval(EGLint interval)
{
    mImplementation->setSwapInterval(interval);
}

const Config *Surface::getConfig() const
{
    return mState.config;
}

EGLint Surface::getPixelAspectRatio() const
{
    return mPixelAspectRatio;
}

EGLenum Surface::getRenderBuffer() const
{
    return mRenderBuffer;
}

EGLenum Surface::getSwapBehavior() const
{
    return mSwapBehavior;
}

EGLenum Surface::getTextureFormat() const
{
    return mTextureFormat;
}

EGLenum Surface::getTextureTarget() const
{
    return mTextureTarget;
}

EGLint Surface::isFixedSize() const
{
    return mFixedSize;
}

EGLint Surface::getWidth() const
{
    return mFixedSize ? static_cast<EGLint>(mFixedWidth) : mImplementation->getWidth();
}

EGLint Surface::getHeight() const
{
    return mFixedSize ? static_cast<EGLint>(mFixedHeight) : mImplementation->getHeight();
}

Error Surface::bindTexImage(gl::Texture *texture, EGLint buffer)
{
    ASSERT(!mTexture.get());
    ANGLE_TRY(mImplementation->bindTexImage(texture, buffer));

    texture->bindTexImageFromSurface(this);
    mTexture.set(texture);

    return Error(EGL_SUCCESS);
}

Error Surface::releaseTexImage(EGLint buffer)
{
    ANGLE_TRY(mImplementation->releaseTexImage(buffer));

    ASSERT(mTexture.get());
    mTexture->releaseTexImageFromSurface();
    mTexture.set(nullptr);

    return Error(EGL_SUCCESS);
}

Error Surface::getSyncValues(EGLuint64KHR *ust, EGLuint64KHR *msc, EGLuint64KHR *sbc)
{
    return mImplementation->getSyncValues(ust, msc, sbc);
}

void Surface::releaseTexImageFromTexture()
{
    ASSERT(mTexture.get());
    mTexture.set(nullptr);
}

gl::Extents Surface::getAttachmentSize(const gl::FramebufferAttachment::Target & /*target*/) const
{
    return gl::Extents(getWidth(), getHeight(), 1);
}

const gl::Format &Surface::getAttachmentFormat(
    const gl::FramebufferAttachment::Target &target) const
{
    return (target.binding() == GL_BACK ? mBackFormat : mDSFormat);
}

GLsizei Surface::getAttachmentSamples(const gl::FramebufferAttachment::Target &target) const
{
    return getConfig()->samples;
}

GLuint Surface::getId() const
{
    UNREACHABLE();
    return 0;
}

gl::Framebuffer *Surface::createDefaultFramebuffer()
{
    return new gl::Framebuffer(this);
}

WindowSurface::WindowSurface(rx::EGLImplFactory *implFactory,
                             const egl::Config *config,
                             EGLNativeWindowType window,
                             const AttributeMap &attribs)
    : Surface(EGL_WINDOW_BIT, config, attribs)
{
    mImplementation = implFactory->createWindowSurface(mState, window, attribs);
}

WindowSurface::~WindowSurface()
{
}

PbufferSurface::PbufferSurface(rx::EGLImplFactory *implFactory,
                               const Config *config,
                               const AttributeMap &attribs)
    : Surface(EGL_PBUFFER_BIT, config, attribs)
{
    mImplementation = implFactory->createPbufferSurface(mState, attribs);
}

PbufferSurface::PbufferSurface(rx::EGLImplFactory *implFactory,
                               const Config *config,
                               EGLenum buftype,
                               EGLClientBuffer clientBuffer,
                               const AttributeMap &attribs)
    : Surface(EGL_PBUFFER_BIT, config, attribs)
{
    mImplementation =
        implFactory->createPbufferFromClientBuffer(mState, buftype, clientBuffer, attribs);
}

PbufferSurface::~PbufferSurface()
{
}

PixmapSurface::PixmapSurface(rx::EGLImplFactory *implFactory,
                             const Config *config,
                             NativePixmapType nativePixmap,
                             const AttributeMap &attribs)
    : Surface(EGL_PIXMAP_BIT, config, attribs)
{
    mImplementation = implFactory->createPixmapSurface(mState, nativePixmap, attribs);
}

PixmapSurface::~PixmapSurface()
{
}

}  // namespace egl
