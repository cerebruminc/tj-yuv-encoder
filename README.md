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
| `i420Buffer` | `Buffer` | ✓ | Packed planar I420 data |
| `width` | `number` | ✓ | Frame width in pixels (positive integer) |
| `height` | `number` | ✓ | Frame height in pixels (positive integer) |
| `quality` | `number` | ✓ | JPEG quality from 1–100 |
| `padOddDimensions` | `boolean` | – | Default `false`. When `true`, odd `width`/`height` are accepted and the encoder pads the luma plane with a replicated edge column/row to reach even dimensions before compressing. |

### Dimension requirements

`width` and `height` must be positive integers. Because I420 uses 4:2:0 subsampling, each chroma (U/V) plane must cover an integer number of 2×2 luma blocks.

**Even dimensions (default):** `width` and `height` must both be even. The expected source buffer size is `width * height + 2 * (width / 2) * (height / 2)`.

**Odd dimensions with `padOddDimensions = true`:** Any positive integer dimensions are accepted. The expected source buffer size uses `ceil` for the chroma planes: `width * height + 2 * ceil(width / 2) * ceil(height / 2)`. The encoder internally pads the luma plane by replicating the last column and/or row so that libjpeg-turbo receives an even-dimensioned frame. As a result, **the output JPEG dimensions are rounded up to the nearest even values** (e.g., a 15×17 source produces a 16×18 JPEG).

`encodeI420ToJpeg` throws standard JavaScript errors when arguments are invalid, the I420 buffer size does not match the expected packed layout, or `libjpeg-turbo` reports a compression failure.

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
