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
    return env.Null();
}

Napi::Value ThrowRangeError(Napi::Env env, const std::string& message) {
    Napi::RangeError::New(env, message).ThrowAsJavaScriptException();
    return env.Null();
}

}  // namespace

Napi::Value EncodeI420ToJpeg(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 4 || !info[0].IsBuffer() || !info[1].IsNumber() ||
        !info[2].IsNumber() || !info[3].IsNumber()) {
        return ThrowTypeError(env, "Expected arguments: (i420Buffer, width, height, quality) or (i420Buffer, width, height, quality, padOddDimensions)");
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

    bool padOddDimensions = false;
    if (info.Length() >= 5) {
        if (!info[4].IsBoolean()) {
            return ThrowTypeError(env, "padOddDimensions must be a boolean");
        }
        padOddDimensions = info[4].As<Napi::Boolean>().Value();
    }

    bool widthIsOdd = (width % 2) != 0;
    bool heightIsOdd = (height % 2) != 0;

    if ((widthIsOdd || heightIsOdd) && !padOddDimensions) {
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
    if (i420Buffer.Length() != expectedSize) {
        return ThrowTypeError(
            env,
            "I420 buffer size mismatch: expected " + std::to_string(expectedSize) +
                " bytes, received " + std::to_string(i420Buffer.Length()) + " bytes");
    }

    // When odd dimensions are present and padding is enabled, build an
    // internally padded copy whose dimensions are the next-even values.
    // The Y plane is replicated at the right/bottom edge; the chroma planes
    // copy directly because ceil(w/2) == paddedW/2 and ceil(h/2) == paddedH/2.
    std::vector<uint8_t> paddedBuffer;
    const uint8_t* yuvData = i420Buffer.Data();
    int encodeWidth = width;
    int encodeHeight = height;

    if (padOddDimensions && (widthIsOdd || heightIsOdd)) {
        size_t paddedWidth = widthSize + (widthIsOdd ? 1u : 0u);
        size_t paddedHeight = heightSize + (heightIsOdd ? 1u : 0u);

        if (paddedWidth > std::numeric_limits<size_t>::max() / paddedHeight) {
            return ThrowRangeError(env, "I420 padded buffer size calculation overflowed");
        }
        size_t paddedLumaSize = paddedWidth * paddedHeight;
        if (paddedLumaSize > std::numeric_limits<size_t>::max() - chromaTotalSize) {
            return ThrowRangeError(env, "I420 padded buffer size calculation overflowed");
        }

        paddedBuffer.resize(paddedLumaSize + chromaTotalSize);

        const uint8_t* srcY = i420Buffer.Data();
        const uint8_t* srcU = srcY + lumaPlaneSize;
        const uint8_t* srcV = srcU + chromaPlaneSize;

        uint8_t* dstY = paddedBuffer.data();
        uint8_t* dstU = dstY + paddedLumaSize;
        uint8_t* dstV = dstU + chromaPlaneSize;

        // Copy Y rows; replicate the last pixel rightward when width is odd.
        for (size_t row = 0; row < heightSize; ++row) {
            const uint8_t* srcRow = srcY + row * widthSize;
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
        std::copy(srcU, srcU + chromaPlaneSize, dstU);
        std::copy(srcV, srcV + chromaPlaneSize, dstV);

        yuvData = paddedBuffer.data();
        encodeWidth = static_cast<int>(paddedWidth);
        encodeHeight = static_cast<int>(paddedHeight);
    }

    tjhandle jpegCompressor = tjInitCompress();
    if (!jpegCompressor) {
        Napi::Error::New(env, tjGetErrorStr()).ThrowAsJavaScriptException();
        return env.Null();
    }

    unsigned char* jpegBuf = nullptr;
    unsigned long jpegSize = 0;

    int result = tjCompressFromYUV(
        jpegCompressor,
        yuvData,
        encodeWidth,
        1,
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
        return env.Null();
    }

    tjDestroy(jpegCompressor);

    if (jpegBuf == nullptr || jpegSize == 0) {
        Napi::Error::New(env, "libjpeg-turbo returned an empty JPEG buffer").ThrowAsJavaScriptException();
        return env.Null();
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
