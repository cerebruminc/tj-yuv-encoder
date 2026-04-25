import { Buffer } from "node:buffer";

export declare function encodeI420ToJpeg(
  i420Buffer: Buffer,
  width: number,
  height: number,
  quality: number,
): Buffer;

export default encodeI420ToJpeg;
