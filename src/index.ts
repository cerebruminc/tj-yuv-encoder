import { join } from "node:path";

interface NativeBinding {
  encodeI420ToJpeg(
    i420Buffer: Buffer,
    width: number,
    height: number,
    quality: number,
    padOddDimensions: boolean,
  ): Buffer;
}

const bindingPath = join(__dirname, "..", "build", "Release", "i420_jpeg_encoder.node");
const nativeBinding = require(bindingPath) as Partial<NativeBinding>;

if (typeof nativeBinding.encodeI420ToJpeg !== "function") {
  throw new Error("Native binding did not export encodeI420ToJpeg");
}

const nativeEncodeI420ToJpeg: NativeBinding["encodeI420ToJpeg"] =
  nativeBinding.encodeI420ToJpeg;

export function encodeI420ToJpeg(
  i420Buffer: Buffer,
  width: number,
  height: number,
  quality: number,
  padOddDimensions = false,
): Buffer {
  return nativeEncodeI420ToJpeg(i420Buffer, width, height, quality, padOddDimensions);
}

export default encodeI420ToJpeg;
