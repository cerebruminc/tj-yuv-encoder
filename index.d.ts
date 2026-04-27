import { Buffer } from "node:buffer";

export interface I420JpegEncodeOptions {
  padOddDimensions?: boolean;
  yStride?: number;
  uStride?: number;
  vStride?: number;
  yOffset?: number;
  uOffset?: number;
  vOffset?: number;
}

export declare function encodeI420ToJpeg(
  i420Buffer: Buffer,
  width: number,
  height: number,
  quality: number,
  options?: boolean | I420JpegEncodeOptions,
): Buffer;

export default encodeI420ToJpeg;
