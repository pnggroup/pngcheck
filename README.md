# pngcheck unofficial of 03 Feb 2025

pngcheck is a command-line utility to check PNG image files for validity 
and to give information about metadate inside the file
(apart from the actual image data).

This unofficial version is forked from pngcheck 3.0.3
(see [3.0.3 README](./README-303)) and adds the following new 
features:

- Coding Independent Code Points [`cICP`](https://w3c.github.io/png/#cICP-chunk)
- Mastering Display Color Volume [`mDCV`](https://w3c.github.io/png/#mDCV-chunk)
- Content Light Level Information [`cLLI`](https://w3c.github.io/png/#cLLI-chunk)
- Animated PNG [`acTL`](https://w3c.github.io/png/#acTL-chunk), [`fcTL`](https://w3c.github.io/png/#fcTL-chunk) and [`fdAT`](https://w3c.github.io/png/#fdAT-chunk)

It also warns if [`eXIf`](https://w3c.github.io/png/#eXIf) is found after the image data [`IDAT`](https://w3c.github.io/png/#11IDAT), 
which will be ignored by web browsers
and is [no longer valid](https://w3c.github.io/png/#5ChunkOrdering) in PNG Third Edition.

Sample usage:

```text
$ pngcheck -c -v test_pattern-PQ.png
File: test_pattern-PQ.png (12033 bytes)
  chunk IHDR at offset 0x0000c, length 13
    1024 x 1024 image, 48-bit RGB, non-interlaced
  chunk iCCP at offset 0x00025, length 2181
    profile name = 1, compression method = 0 (deflate)
    compressed profile = 2178 bytes
  chunk cHRM at offset 0x008b6, length 32
    White x = 0.3127 y = 0.329,  Red x = 0.708 y = 0.292
    Green x = 0.17 y = 0.797,  Blue x = 0.131 y = 0.046
  chunk cICP at offset 0x008e2, length 4
    Rec. ITU-R BT.2100-2 perceptual quantization (PQ) system
    White x = 0.3127 y = 0.329,  Red x = 0.708 y = 0.292
    Green x = 0.17 y = 0.797,  Blue x = 0.131 y = 0.046
    Full range
  chunk cLLi at offset 0x008f2, length 8
    Old version of CLLI, do not use
ERRORS DETECTED in test_pattern-PQ.png
```
