# @cerebruminc/i420-jpeg-encoder

Synchronous Node.js native encoder for converting planar `I420`/YUV420 frames directly to JPEG using `libjpeg-turbo`.

## System Prerequisites

Install `libjpeg-turbo` before building the addon:

```sh
brew install jpeg-turbo
```

```sh
sudo apt-get update
sudo apt-get install libturbojpeg0-dev pkg-config
```

The build expects `pkg-config --cflags --libs libturbojpeg` to resolve the TurboJPEG headers and library.

To verify local native prerequisites before building:

```sh
npm run check:native-deps
```

## Install

```sh
npm install @cerebruminc/i420-jpeg-encoder
```

This package targets Node.js `^20.0.0 || ^22.0.0 || ^24.0.0` and uses N-API through `node-addon-api`.

## Usage

```ts
import { encodeI420ToJpeg } from "@cerebruminc/i420-jpeg-encoder";

const width = 1280;
const height = 720;
const quality = 85;
const i420Buffer = Buffer.alloc((width * height * 3) / 2);

const jpegBuffer = encodeI420ToJpeg(i420Buffer, width, height, quality);
```

### Arguments

| Argument | Type | Required | Description |
|---|---|---|---|
| `i420Buffer` | `Buffer` | ✓ | Planar I420 data, either tightly packed or row-strided |
| `width` | `number` | ✓ | Frame width in pixels (positive integer) |
| `height` | `number` | ✓ | Frame height in pixels (positive integer) |
| `quality` | `number` | ✓ | JPEG quality from 1–100 |
| `options` | `boolean \| object` | – | A boolean keeps the old `padOddDimensions` behavior. An object may include `padOddDimensions`, `yStride`, `uStride`, `vStride`, `yOffset`, `uOffset`, and `vOffset`. If any of `yStride`, `uStride`, or `vStride` is provided, all three must be provided together; otherwise omit all three. |

### Dimension requirements

`width` and `height` must be positive integers. Because I420 uses 4:2:0 subsampling, each chroma (U/V) plane must cover an integer number of 2×2 luma blocks.

**Even dimensions (default):** `width` and `height` must both be even. The expected source buffer size is `width * height + 2 * (width / 2) * (height / 2)`.

**Odd dimensions with `padOddDimensions = true`:** Any positive integer dimensions are accepted. The expected source buffer size uses `ceil` for the chroma planes: `width * height + 2 * ceil(width / 2) * ceil(height / 2)`. The encoder internally pads the luma plane by replicating the last column and/or row so that libjpeg-turbo receives an even-dimensioned frame. As a result, **the output JPEG dimensions are rounded up to the nearest even values** (e.g., a 15×17 source produces a 16×18 JPEG).

### Strided buffers

Tightly packed I420 does not need options. For row-strided I420 buffers, pass all three plane strides:

```ts
const jpegBuffer = encodeI420ToJpeg(i420Buffer, width, height, quality, {
  yStride,
  uStride,
  vStride,
});
```

By default, strided planes are assumed to be contiguous inside `i420Buffer`: Y starts at offset `0`, U starts at `yStride * height`, and V starts at `uOffset + uStride * ceil(height / 2)`. If the contiguous I420 image starts later in the buffer, you can pass `yOffset` only and the U/V plane offsets will be inferred from that starting point using the supplied strides and dimensions. Pass `uOffset` and `vOffset` as well when the chroma planes are not laid out contiguously after Y, for example if U and V are stored at independent locations in the buffer.

`encodeI420ToJpeg` throws standard JavaScript errors when arguments are invalid, the I420 buffer size does not match the expected layout, or `libjpeg-turbo` reports a compression failure.

## Development

Install dependencies:

```sh
npm install
```

Build the native addon and TypeScript wrapper:

```sh
npm run build
```

Run the Jest test suite:

```sh
npm test
```

The tests cover both native error handling and a color I420 fixture that is decoded back from JPEG to verify chroma data survives the encode path.

Clean generated build output:

```sh
npm run clean
```

## License

GNU General Public License v3.0 only. See `LICENSE`.
