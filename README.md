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

## Install

```sh
npm install @cerebruminc/i420-jpeg-encoder
```

This package targets Node.js `^20.0.0` and uses N-API through `node-addon-api`.

## Usage

```ts
import { encodeI420ToJpeg } from "@cerebruminc/i420-jpeg-encoder";

const width = 1280;
const height = 720;
const quality = 85;
const i420Buffer = Buffer.alloc((width * height * 3) / 2);

const jpegBuffer = encodeI420ToJpeg(i420Buffer, width, height, quality);
```

`width` and `height` must be positive even integers for `I420`/`TJSAMP_420` input. `encodeI420ToJpeg` throws standard JavaScript errors when arguments are invalid, the `I420` buffer size does not match `width * height * 1.5`, or `libjpeg-turbo` reports a compression failure.

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
