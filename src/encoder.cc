#include <napi.h>
#include <turbojpeg.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace {

bool IsIntegralNumber(const Napi::Value& value) {
    if (!value.IsNumber()) {
        return false;
    }

    double number = value.As<Napi::Number>().DoubleValue();
    return std::isfinite(number) && std::floor(number) == number;
}

bool ReadPositiveInt(const Napi::Value& value, int* output) {
    if (!IsIntegralNumber(value)) {
        return false;
    }

    double number = value.As<Napi::Number>().DoubleValue();
    if (number <= 0 || number > static_cast<double>(std::numeric_limits<int>::max())) {
        return false;
    }

    *output = static_cast<int>(number);
    return true;
}

Napi::Value ThrowTypeError(Napi::Env env, const std::string& message) {
    Napi::TypeError::New(env, message).ThrowAsJavaScriptException();
    return Napi::Value();
}

Napi::Value ThrowRangeError(Napi::Env env, const std::string& message) {
    Napi::RangeError::New(env, message).ThrowAsJavaScriptException();
    return Napi::Value();
}

bool ReadNonNegativeSize(const Napi::Value& value, size_t* output) {
    if (!IsIntegralNumber(value)) {
        return false;
    }

    double number = value.As<Napi::Number>().DoubleValue();
    if (number < 0 || number > static_cast<double>(std::numeric_limits<size_t>::max())) {
        return false;
    }

    *output = static_cast<size_t>(number);
    return true;
}

bool ReadOptionalBooleanOption(
    Napi::Env env,
    const Napi::Object& options,
    const char* name,
    bool* output) {
    Napi::Value value = options.Get(name);
    if (value.IsUndefined()) {
        return true;
    }

    if (!value.IsBoolean()) {
        ThrowTypeError(env, std::string(name) + " must be a boolean");
        return false;
    }

    *output = value.As<Napi::Boolean>().Value();
    return true;
}

bool ReadOptionalPositiveIntOption(
    Napi::Env env,
    const Napi::Object& options,
    const char* name,
    int* output,
    bool* present) {
    Napi::Value value = options.Get(name);
    if (value.IsUndefined()) {
        *present = false;
        return true;
    }

    *present = true;
    if (!ReadPositiveInt(value, output)) {
        ThrowRangeError(env, std::string(name) + " must be a positive integer");
        return false;
    }

    return true;
}

bool ReadOptionalSizeOption(
    Napi::Env env,
    const Napi::Object& options,
    const char* name,
    size_t* output,
    bool* present) {
    Napi::Value value = options.Get(name);
    if (value.IsUndefined()) {
        *present = false;
        return true;
    }

    *present = true;
    if (!ReadNonNegativeSize(value, output)) {
        ThrowRangeError(env, std::string(name) + " must be a non-negative integer");
        return false;
    }

    return true;
}

bool CheckedMul(size_t left, size_t right, size_t* output) {
    if (left != 0 && right > std::numeric_limits<size_t>::max() / left) {
        return false;
    }

    *output = left * right;
    return true;
}

bool CheckedAdd(size_t left, size_t right, size_t* output) {
    if (left > std::numeric_limits<size_t>::max() - right) {
        return false;
    }

    *output = left + right;
    return true;
}

bool PlaneEndOffset(
    size_t offset,
    size_t stride,
    size_t planeWidth,
    size_t planeHeight,
    size_t* output) {
    size_t rowOffset = 0;
    if (!CheckedMul(planeHeight - 1, stride, &rowOffset)) {
        return false;
    }

    size_t lastRowEnd = 0;
    if (!CheckedAdd(rowOffset, planeWidth, &lastRowEnd)) {
        return false;
    }

    return CheckedAdd(offset, lastRowEnd, output);
}

struct I420Layout {
    bool padOddDimensions = false;
    bool usesCustomLayout = false;
    bool uOffsetProvided = false;
    bool vOffsetProvided = false;
    int yStride = 0;
    int uStride = 0;
    int vStride = 0;
    size_t yOffset = 0;
    size_t uOffset = 0;
    size_t vOffset = 0;
};

}  // namespace

Napi::Value EncodeI420ToJpeg(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 4 || !info[0].IsBuffer() || !info[1].IsNumber() ||
        !info[2].IsNumber() || !info[3].IsNumber()) {
        return ThrowTypeError(
            env,
            "Expected arguments: (i420Buffer, width, height, quality), "
            "(i420Buffer, width, height, quality, padOddDimensions), or "
            "(i420Buffer, width, height, quality, options)");
    }

    Napi::Buffer<uint8_t> i420Buffer = info[0].As<Napi::Buffer<uint8_t>>();

    int width = 0;
    int height = 0;
    if (!ReadPositiveInt(info[1], &width) || !ReadPositiveInt(info[2], &height)) {
        return ThrowRangeError(env, "width and height must be positive integers");
    }

    int quality = 0;
    if (!ReadPositiveInt(info[3], &quality) || quality > 100) {
        return ThrowRangeError(env, "quality must be an integer between 1 and 100");
    }

    I420Layout layout;
    if (info.Length() >= 5) {
        if (info[4].IsBoolean()) {
            layout.padOddDimensions = info[4].As<Napi::Boolean>().Value();
        } else if (info[4].IsObject()) {
            Napi::Object options = info[4].As<Napi::Object>();

            if (!ReadOptionalBooleanOption(env, options, "padOddDimensions", &layout.padOddDimensions)) {
                return Napi::Value();
            }

            bool hasYStride = false;
            bool hasUStride = false;
            bool hasVStride = false;
            if (!ReadOptionalPositiveIntOption(env, options, "yStride", &layout.yStride, &hasYStride) ||
                !ReadOptionalPositiveIntOption(env, options, "uStride", &layout.uStride, &hasUStride) ||
                !ReadOptionalPositiveIntOption(env, options, "vStride", &layout.vStride, &hasVStride)) {
                return Napi::Value();
            }

            bool hasAnyStride = hasYStride || hasUStride || hasVStride;
            if (hasAnyStride && !(hasYStride && hasUStride && hasVStride)) {
                return ThrowTypeError(env, "yStride, uStride, and vStride must be provided together");
            }

            bool hasYOffset = false;
            bool hasUOffset = false;
            bool hasVOffset = false;
            if (!ReadOptionalSizeOption(env, options, "yOffset", &layout.yOffset, &hasYOffset) ||
                !ReadOptionalSizeOption(env, options, "uOffset", &layout.uOffset, &hasUOffset) ||
                !ReadOptionalSizeOption(env, options, "vOffset", &layout.vOffset, &hasVOffset)) {
                return Napi::Value();
            }

            layout.uOffsetProvided = hasUOffset;
            layout.vOffsetProvided = hasVOffset;
            layout.usesCustomLayout = hasAnyStride || hasYOffset || hasUOffset || hasVOffset;
        } else {
            return ThrowTypeError(env, "options must be an object or padOddDimensions must be a boolean");
        }
    }

    bool widthIsOdd = (width % 2) != 0;
    bool heightIsOdd = (height % 2) != 0;

    if ((widthIsOdd || heightIsOdd) && !layout.padOddDimensions) {
        return ThrowRangeError(
            env,
            "width and height must be even integers for I420 (4:2:0); "
            "pass padOddDimensions=true to enable internal padding, or pad/crop before encoding");
    }

    size_t widthSize = static_cast<size_t>(width);
    size_t heightSize = static_cast<size_t>(height);
    if (widthSize > std::numeric_limits<size_t>::max() / heightSize) {
        return ThrowRangeError(env, "I420 buffer size calculation overflowed");
    }

    size_t lumaPlaneSize = widthSize * heightSize;

    // Source chroma dimensions always use ceil(w/2) x ceil(h/2) so that both
    // even and odd source layouts are handled with the same formula.
    size_t chromaWidth = (widthSize + 1) / 2;
    size_t chromaHeight = (heightSize + 1) / 2;
    if (chromaWidth > 0 && chromaHeight > std::numeric_limits<size_t>::max() / chromaWidth) {
        return ThrowRangeError(env, "I420 buffer size calculation overflowed");
    }

    size_t chromaPlaneSize = chromaWidth * chromaHeight;
    if (chromaPlaneSize > std::numeric_limits<size_t>::max() / 2) {
        return ThrowRangeError(env, "I420 buffer size calculation overflowed");
    }

    size_t chromaTotalSize = chromaPlaneSize * 2;
    if (lumaPlaneSize > std::numeric_limits<size_t>::max() - chromaTotalSize) {
        return ThrowRangeError(env, "I420 buffer size calculation overflowed");
    }

    size_t expectedSize = lumaPlaneSize + chromaTotalSize;

    if (layout.yStride == 0) {
        layout.yStride = width;
    }
    if (layout.uStride == 0) {
        layout.uStride = static_cast<int>(chromaWidth);
    }
    if (layout.vStride == 0) {
        layout.vStride = static_cast<int>(chromaWidth);
    }

    if (static_cast<size_t>(layout.yStride) < widthSize ||
        static_cast<size_t>(layout.uStride) < chromaWidth ||
        static_cast<size_t>(layout.vStride) < chromaWidth) {
        return ThrowRangeError(env, "I420 strides must be at least the corresponding plane widths");
    }

    size_t defaultUOffset = 0;
    size_t defaultVOffset = 0;
    size_t yPlaneAllocation = 0;
    size_t uPlaneAllocation = 0;
    if (!CheckedMul(static_cast<size_t>(layout.yStride), heightSize, &yPlaneAllocation) ||
        !CheckedMul(static_cast<size_t>(layout.uStride), chromaHeight, &uPlaneAllocation) ||
        !CheckedAdd(layout.yOffset, yPlaneAllocation, &defaultUOffset)) {
        return ThrowRangeError(env, "I420 buffer size calculation overflowed");
    }
    if (!layout.uOffsetProvided) {
        layout.uOffset = defaultUOffset;
    }
    if (!CheckedAdd(layout.uOffset, uPlaneAllocation, &defaultVOffset)) {
        return ThrowRangeError(env, "I420 buffer size calculation overflowed");
    }
    if (!layout.vOffsetProvided) {
        layout.vOffset = defaultVOffset;
    }

    size_t yEndOffset = 0;
    size_t uEndOffset = 0;
    size_t vEndOffset = 0;
    if (!PlaneEndOffset(layout.yOffset, static_cast<size_t>(layout.yStride), widthSize, heightSize, &yEndOffset) ||
        !PlaneEndOffset(layout.uOffset, static_cast<size_t>(layout.uStride), chromaWidth, chromaHeight, &uEndOffset) ||
        !PlaneEndOffset(layout.vOffset, static_cast<size_t>(layout.vStride), chromaWidth, chromaHeight, &vEndOffset)) {
        return ThrowRangeError(env, "I420 buffer size calculation overflowed");
    }

    size_t requiredSize = std::max(std::max(yEndOffset, uEndOffset), vEndOffset);
    if (!layout.usesCustomLayout && i420Buffer.Length() != expectedSize) {
        return ThrowTypeError(
            env,
            "I420 buffer size mismatch: expected " + std::to_string(expectedSize) +
                " bytes, received " + std::to_string(i420Buffer.Length()) + " bytes");
    }
    if (layout.usesCustomLayout && i420Buffer.Length() < requiredSize) {
        return ThrowTypeError(
            env,
            "I420 buffer size mismatch for strided layout: expected at least " +
                std::to_string(requiredSize) + " bytes, received " +
                std::to_string(i420Buffer.Length()) + " bytes");
    }

    // When odd dimensions are present and padding is enabled, build an
    // internally padded copy whose dimensions are the next-even values.
    // The Y plane is replicated at the right/bottom edge; the chroma planes
    // copy directly because ceil(w/2) == paddedW/2 and ceil(h/2) == paddedH/2.
    std::vector<uint8_t> paddedBuffer;
    const uint8_t* srcPlanes[3] = {
        i420Buffer.Data() + layout.yOffset,
        i420Buffer.Data() + layout.uOffset,
        i420Buffer.Data() + layout.vOffset,
    };
    int strides[3] = { layout.yStride, layout.uStride, layout.vStride };
    int encodeWidth = width;
    int encodeHeight = height;

    if (layout.padOddDimensions && (widthIsOdd || heightIsOdd)) {
        size_t paddedWidth = widthSize + (widthIsOdd ? 1u : 0u);
        size_t paddedHeight = heightSize + (heightIsOdd ? 1u : 0u);

        if (paddedWidth > std::numeric_limits<size_t>::max() / paddedHeight) {
            return ThrowRangeError(env, "I420 padded buffer size calculation overflowed");
        }
        size_t paddedLumaSize = paddedWidth * paddedHeight;
        if (paddedLumaSize > std::numeric_limits<size_t>::max() - chromaTotalSize) {
            return ThrowRangeError(env, "I420 padded buffer size calculation overflowed");
        }
        if (paddedWidth > static_cast<size_t>(std::numeric_limits<int>::max()) ||
            paddedHeight > static_cast<size_t>(std::numeric_limits<int>::max())) {
            return ThrowRangeError(env, "I420 padded dimensions exceed supported range");
        }

        paddedBuffer.resize(paddedLumaSize + chromaTotalSize);

        const uint8_t* srcY = srcPlanes[0];
        const uint8_t* srcU = srcPlanes[1];
        const uint8_t* srcV = srcPlanes[2];

        uint8_t* dstY = paddedBuffer.data();
        uint8_t* dstU = dstY + paddedLumaSize;
        uint8_t* dstV = dstU + chromaPlaneSize;

        // Copy Y rows; replicate the last pixel rightward when width is odd.
        for (size_t row = 0; row < heightSize; ++row) {
            const uint8_t* srcRow = srcY + row * static_cast<size_t>(layout.yStride);
            uint8_t* dstRow = dstY + row * paddedWidth;
            std::copy(srcRow, srcRow + widthSize, dstRow);
            if (widthIsOdd) {
                dstRow[widthSize] = srcRow[widthSize - 1];
            }
        }
        // Replicate the last Y row downward when height is odd.
        if (heightIsOdd) {
            const uint8_t* lastRow = dstY + (heightSize - 1) * paddedWidth;
            uint8_t* extraRow = dstY + heightSize * paddedWidth;
            std::copy(lastRow, lastRow + paddedWidth, extraRow);
        }

        // Chroma planes need no structural change: chromaWidth was computed as
        // (widthSize + 1) / 2, which equals paddedWidth / 2 exactly (since
        // paddedWidth = widthSize + 1 when odd), and similarly for height.
        // The source and padded chroma plane sizes are therefore identical.
        for (size_t row = 0; row < chromaHeight; ++row) {
            std::copy(
                srcU + row * static_cast<size_t>(layout.uStride),
                srcU + row * static_cast<size_t>(layout.uStride) + chromaWidth,
                dstU + row * chromaWidth);
            std::copy(
                srcV + row * static_cast<size_t>(layout.vStride),
                srcV + row * static_cast<size_t>(layout.vStride) + chromaWidth,
                dstV + row * chromaWidth);
        }

        srcPlanes[0] = dstY;
        srcPlanes[1] = dstU;
        srcPlanes[2] = dstV;
        strides[0] = static_cast<int>(paddedWidth);
        strides[1] = static_cast<int>(chromaWidth);
        strides[2] = static_cast<int>(chromaWidth);
        encodeWidth = static_cast<int>(paddedWidth);
        encodeHeight = static_cast<int>(paddedHeight);
    }

    tjhandle jpegCompressor = tjInitCompress();
    if (!jpegCompressor) {
        Napi::Error::New(env, tjGetErrorStr()).ThrowAsJavaScriptException();
        return Napi::Value();
    }

    unsigned char* jpegBuf = nullptr;
    unsigned long jpegSize = 0;

    int result = tjCompressFromYUVPlanes(
        jpegCompressor,
        srcPlanes,
        encodeWidth,
        strides,
        encodeHeight,
        TJSAMP_420,
        &jpegBuf,
        &jpegSize,
        quality,
        TJFLAG_FASTDCT);

    if (result != 0) {
        std::string errorMessage = tjGetErrorStr2(jpegCompressor);
        if (jpegBuf != nullptr) {
            tjFree(jpegBuf);
        }
        tjDestroy(jpegCompressor);
        Napi::Error::New(env, errorMessage).ThrowAsJavaScriptException();
        return Napi::Value();
    }

    tjDestroy(jpegCompressor);

    if (jpegBuf == nullptr || jpegSize == 0) {
        Napi::Error::New(env, "libjpeg-turbo returned an empty JPEG buffer").ThrowAsJavaScriptException();
        return Napi::Value();
    }

    Napi::Buffer<uint8_t> output = Napi::Buffer<uint8_t>::Copy(env, jpegBuf, jpegSize);
    tjFree(jpegBuf);
    return output;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(
        Napi::String::New(env, "encodeI420ToJpeg"),
        Napi::Function::New(env, EncodeI420ToJpeg));
    return exports;
}

NODE_API_MODULE(i420_jpeg_encoder, Init)
