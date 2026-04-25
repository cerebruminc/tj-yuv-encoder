const { encodeI420ToJpeg } = require("@cerebruminc/tj-yuv-encoder");

const width = 16;
const height = 16;
const frame = Buffer.alloc((width * height * 3) / 2, 128);
const jpeg = encodeI420ToJpeg(frame, width, height, 85);

if (!Buffer.isBuffer(jpeg)) {
  throw new Error("Expected JPEG output to be a Buffer");
}

if (jpeg.length === 0 || jpeg[0] !== 0xff || jpeg[1] !== 0xd8) {
  throw new Error("Expected JPEG output to start with SOI marker");
}
