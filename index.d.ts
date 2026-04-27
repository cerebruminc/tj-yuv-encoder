import { Buffer } from "node:buffer";

export declare function encodeI420ToJpeg(
  i420Buffer: Buffer,
  width: number,
  height: number,
  quality: number,
  padOddDimensions?: boolean,
): Buffer;

export default encodeI420ToJpeg;
