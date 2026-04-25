const { spawnSync } = require("node:child_process");

function commandExists(command) {
  const result = spawnSync(command, ["--version"], { stdio: "ignore" });
  return result.status === 0;
}

function hasTurboJpegPkgConfig() {
  const result = spawnSync("pkg-config", ["--exists", "libturbojpeg"], {
    stdio: "ignore",
  });
  return result.status === 0;
}

if (process.env.I420_JPEG_ENCODER_SKIP_NATIVE_DEPS_CHECK === "1") {
  process.exit(0);
}

if (!commandExists("pkg-config") || !hasTurboJpegPkgConfig()) {
  console.error(`
Unable to find libturbojpeg with pkg-config.

Install native prerequisites before building @cerebruminc/i420-jpeg-encoder:

  macOS:
    brew install jpeg-turbo

  Debian/Ubuntu:
    sudo apt-get update
    sudo apt-get install libturbojpeg0-dev pkg-config

The build expects:
  pkg-config --cflags --libs libturbojpeg
`);
  process.exit(1);
}
