/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "JSDOMPromiseDeferred.h"
#include "ScriptWrappable.h"
#include <wtf/Optional.h>
#include <wtf/RefCounted.h>
#include <wtf/Variant.h>

namespace WebCore {

class Blob;
class HTMLCanvasElement;
class HTMLImageElement;
class HTMLVideoElement;
class ImageBuffer;
class ImageData;
class IntRect;
class PendingImageBitmap;
class ScriptExecutionContext;
struct ImageBitmapOptions;

class ImageBitmap : public ScriptWrappable, public RefCounted<ImageBitmap> {
public:
    using Source = Variant<
        RefPtr<HTMLImageElement>,
        RefPtr<HTMLCanvasElement>,
        RefPtr<HTMLVideoElement>, // FIXME: Make conditional
        RefPtr<ImageBitmap>,
        RefPtr<Blob>,
        RefPtr<ImageData>
    >;

    using Promise = DOMPromiseDeferred<IDLInterface<ImageBitmap>>;

    static void createPromise(ScriptExecutionContext&, Source&&, ImageBitmapOptions&&, Promise&&);
    static void createPromise(ScriptExecutionContext&, Source&&, ImageBitmapOptions&&, int sx, int sy, int sw, int sh, Promise&&);

    ~ImageBitmap();

    unsigned width() const;
    unsigned height() const;
    void close();

    bool isDetached() const { return m_detached; }

private:
    friend class PendingImageBitmap;

    static Ref<ImageBitmap> create();
    ImageBitmap();

    static void createPromise(ScriptExecutionContext&, RefPtr<HTMLImageElement>&, ImageBitmapOptions&&, std::optional<IntRect>, Promise&&);
    static void createPromise(ScriptExecutionContext&, RefPtr<HTMLCanvasElement>&, ImageBitmapOptions&&, std::optional<IntRect>, Promise&&);
    static void createPromise(ScriptExecutionContext&, RefPtr<HTMLVideoElement>&, ImageBitmapOptions&&, std::optional<IntRect>, Promise&&);
    static void createPromise(ScriptExecutionContext&, RefPtr<ImageBitmap>&, ImageBitmapOptions&&, std::optional<IntRect>, Promise&&);
    static void createPromise(ScriptExecutionContext&, RefPtr<Blob>&, ImageBitmapOptions&&, std::optional<IntRect>, Promise&&);
    static void createPromise(ScriptExecutionContext&, RefPtr<ImageData>&, ImageBitmapOptions&&, std::optional<IntRect>, Promise&&);

    std::unique_ptr<ImageBuffer> m_bitmapData;
    bool m_detached { false };
};

}
