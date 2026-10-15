# QR encoder provenance

Project Nayuki QR Code generator library, pinned release **v1.8.0**:

- [qrcodegen.h](https://github.com/nayuki/QR-Code-generator/blob/v1.8.0/c/qrcodegen.h), SHA-256 `e82df4bff37d18b5863b9e7486fe6bda1b6cda8c3b9ecebfec473907265cb589`.
- [qrcodegen.c](https://github.com/nayuki/QR-Code-generator/blob/v1.8.0/c/qrcodegen.c), SHA-256 `300eff07ee25baaa7578f20284411638154716379437391e7e689c0e6ce81403`.

The upstream C implementation is copied byte-for-byte as `qrcodegen.cpp` so
ESPHome compiles it with the component's C++ sources. The header supplies its
original C linkage. Both files retain the complete upstream MIT license notice.
Upstream formatting, including trailing whitespace, is retained for exact hash
verification; project-authored whitespace checks exclude these two vendor files.
No runtime download, allocation, font or image is required.

The display uses fixed version 3, medium error correction, automatic mask
selection and no error-correction boosting. Each caller buffer is 107 bytes.
The renderer adds a white four-module quiet zone on every side; 3px modules on
Atom produce a 111px square and 6px modules on Waveshare produce a 222px square.
The payload is exactly the existing `http://<device-hostname>/` local page URL.
Encoding failure falls back to the complete manual address/website guidance.
