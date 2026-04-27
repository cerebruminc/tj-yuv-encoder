const jpegJs = require("jpeg-js");
const { encodeI420ToJpeg } = require("../dist");

function createI420Buffer(width, height) {
  const ySize = width * height;
  const chromaSize = Math.ceil(width / 2) * Math.ceil(height / 2);
  const buffer = Buffer.alloc(ySize + chromaSize * 2);

  buffer.fill(128, 0, ySize);
  buffer.fill(128, ySize, ySize + chromaSize);
  buffer.fill(128, ySize + chromaSize);

  return buffer;
}

function clampByte(value) {
  return Math.max(0, Math.min(255, Math.round(value)));
}

function rgbToYuv([red, green, blue]) {
  return {
    y: clampByte(0.299 * red + 0.587 * green + 0.114 * blue),
    u: clampByte(-0.168736 * red - 0.331264 * green + 0.5 * blue + 128),
    v: clampByte(0.5 * red - 0.418688 * green - 0.081312 * blue + 128),
  };
}

function createColorBarsI420(width, height) {
  const colors = [
    [220, 24, 24],
    [24, 200, 48],
    [24, 72, 220],
    [230, 220, 24],
  ];
  const ySize = width * height;
  const chromaWidth = width / 2;
  const buffer = Buffer.alloc((width * height * 3) / 2);

  const colorAt = (x) => colors[Math.floor((x * colors.length) / width)];

  for (let y = 0; y < height; y += 1) {
    for (let x = 0; x < width; x += 1) {
      buffer[y * width + x] = rgbToYuv(colorAt(x)).y;
    }
  }

  for (let y = 0; y < height; y += 2) {
    for (let x = 0; x < width; x += 2) {
      const samples = [
        rgbToYuv(colorAt(x)),
        rgbToYuv(colorAt(x + 1)),
        rgbToYuv(colorAt(x)),
        rgbToYuv(colorAt(x + 1)),
      ];
      const u = clampByte(samples.reduce((sum, sample) => sum + sample.u, 0) / samples.length);
      const v = clampByte(samples.reduce((sum, sample) => sum + sample.v, 0) / samples.length);
      const chromaIndex = (y / 2) * chromaWidth + x / 2;

      buffer[ySize + chromaIndex] = u;
      buffer[ySize + ySize / 4 + chromaIndex] = v;
    }
  }

  return buffer;
}

function sampleRgb(decoded, x, y) {
  const index = (y * decoded.width + x) * 4;
  return {
    red: decoded.data[index],
    green: decoded.data[index + 1],
    blue: decoded.data[index + 2],
  };
}

describe("encodeI420ToJpeg", () => {
  it("encodes a valid I420 frame into a JPEG buffer", () => {
    const jpeg = encodeI420ToJpeg(createI420Buffer(16, 16), 16, 16, 85);

    expect(Buffer.isBuffer(jpeg)).toBe(true);
    expect(jpeg.length).toBeGreaterThan(0);
    expect(jpeg[0]).toBe(0xff);
    expect(jpeg[1]).toBe(0xd8);
    expect(jpeg[jpeg.length - 2]).toBe(0xff);
    expect(jpeg[jpeg.length - 1]).toBe(0xd9);
  });

  it("preserves chroma data from a real color I420 fixture", () => {
    const width = 32;
    const height = 16;
    const jpeg = encodeI420ToJpeg(createColorBarsI420(width, height), width, height, 95);
    const decoded = jpegJs.decode(jpeg, { useTArray: true });

    expect(decoded.width).toBe(width);
    expect(decoded.height).toBe(height);

    const redBar = sampleRgb(decoded, 4, 8);
    const greenBar = sampleRgb(decoded, 12, 8);
    const blueBar = sampleRgb(decoded, 20, 8);
    const yellowBar = sampleRgb(decoded, 28, 8);

    expect(redBar.red).toBeGreaterThan(redBar.green + 40);
    expect(redBar.red).toBeGreaterThan(redBar.blue + 40);
    expect(greenBar.green).toBeGreaterThan(greenBar.red + 40);
    expect(greenBar.green).toBeGreaterThan(greenBar.blue + 40);
    expect(blueBar.blue).toBeGreaterThan(blueBar.red + 40);
    expect(blueBar.blue).toBeGreaterThan(blueBar.green + 40);
    expect(yellowBar.red).toBeGreaterThan(yellowBar.blue + 40);
    expect(yellowBar.green).toBeGreaterThan(yellowBar.blue + 40);
  });

  it("throws a handled JS exception for an undersized I420 buffer", () => {
    expect(() => encodeI420ToJpeg(Buffer.alloc(16 * 16), 16, 16, 85)).toThrow(
      /I420 buffer size mismatch/,
    );
  });

  it("throws a handled JS exception for invalid dimensions", () => {
    expect(() => encodeI420ToJpeg(Buffer.alloc(0), 0, 16, 85)).toThrow(
      /width and height must be positive integers/,
    );

    expect(() => encodeI420ToJpeg(createI420Buffer(16, 16), 15.5, 16, 85)).toThrow(
      /width and height must be positive integers/,
    );
  });

  it("throws a handled JS exception for odd dimensions when padOddDimensions is false", () => {
    expect(() => encodeI420ToJpeg(createI420Buffer(15, 16), 15, 16, 85)).toThrow(
      /width and height must be even integers for I420/,
    );

    expect(() => encodeI420ToJpeg(createI420Buffer(16, 17), 16, 17, 85)).toThrow(
      /width and height must be even integers for I420/,
    );

    expect(() => encodeI420ToJpeg(createI420Buffer(15, 16), 15, 16, 85, false)).toThrow(
      /width and height must be even integers for I420/,
    );
  });

  it("encodes odd-width I420 frames when padOddDimensions is true", () => {
    const width = 15;
    const height = 16;
    const jpeg = encodeI420ToJpeg(createI420Buffer(width, height), width, height, 85, true);
    const decoded = jpegJs.decode(jpeg, { useTArray: true });

    // Output JPEG dimensions are padded to the next even values.
    expect(decoded.width).toBe(width + 1);
    expect(decoded.height).toBe(height);
  });

  it("encodes odd-height I420 frames when padOddDimensions is true", () => {
    const width = 16;
    const height = 15;
    const jpeg = encodeI420ToJpeg(createI420Buffer(width, height), width, height, 85, true);
    const decoded = jpegJs.decode(jpeg, { useTArray: true });

    expect(decoded.width).toBe(width);
    expect(decoded.height).toBe(height + 1);
  });

  it("encodes odd-width-and-height I420 frames when padOddDimensions is true", () => {
    const width = 15;
    const height = 17;
    const jpeg = encodeI420ToJpeg(createI420Buffer(width, height), width, height, 85, true);
    const decoded = jpegJs.decode(jpeg, { useTArray: true });

    expect(decoded.width).toBe(width + 1);
    expect(decoded.height).toBe(height + 1);
  });

  it("padOddDimensions=true is a no-op for even-dimension frames", () => {
    const jpeg = encodeI420ToJpeg(createI420Buffer(16, 16), 16, 16, 85, true);
    const decoded = jpegJs.decode(jpeg, { useTArray: true });

    expect(decoded.width).toBe(16);
    expect(decoded.height).toBe(16);
  });

  it("throws a handled JS exception for invalid quality", () => {
    expect(() => encodeI420ToJpeg(createI420Buffer(16, 16), 16, 16, 0)).toThrow(
      /quality must be an integer between 1 and 100/,
    );

    expect(() => encodeI420ToJpeg(createI420Buffer(16, 16), 16, 16, 101)).toThrow(
      /quality must be an integer between 1 and 100/,
    );
  });
});
