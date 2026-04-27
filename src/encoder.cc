#include <napi.h>
#include <turbojpeg.h>

#include <cmath>
#include <limits>
#include <string>

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
        return ThrowTypeError(env, "Expected arguments: (i420Buffer, width, height, quality)");
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

    size_t widthSize = static_cast<size_t>(width);
    size_t heightSize = static_cast<size_t>(height);
    if (widthSize > std::numeric_limits<size_t>::max() / heightSize) {
        return ThrowRangeError(env, "I420 buffer size calculation overflowed");
    }

    size_t lumaPlaneSize = widthSize * heightSize;

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

    tjhandle jpegCompressor = tjInitCompress();
    if (!jpegCompressor) {
        Napi::Error::New(env, tjGetErrorStr()).ThrowAsJavaScriptException();
        return env.Null();
    }

    unsigned char* jpegBuf = nullptr;
    unsigned long jpegSize = 0;

    int result = tjCompressFromYUV(
        jpegCompressor,
        i420Buffer.Data(),
        width,
        1,
        height,
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
