{
  "targets": [
    {
      "target_name": "i420_jpeg_encoder",
      "sources": [ "src/encoder.cc" ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "defines": [
        "NAPI_VERSION=8"
      ],
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "cflags": [
        "<!@(pkg-config --cflags libturbojpeg)"
      ],
      "cflags_cc": [
        "-std=c++17"
      ],
      "xcode_settings": {
        "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
        "CLANG_CXX_LIBRARY": "libc++",
        "MACOSX_DEPLOYMENT_TARGET": "10.15",
        "OTHER_CPLUSPLUSFLAGS": [
          "-std=c++17",
          "<!@(pkg-config --cflags libturbojpeg)"
        ]
      },
      "libraries": [
        "<!@(pkg-config --libs libturbojpeg)"
      ]
    }
  ]
}
