# Preview image fixture

`preview-gradient.jp2` is a generated 512×512 RGB gradient, encoded as JPEG 2000
with macOS ImageIO (`sips -s format public.jpeg-2000 -s formatOptions 75`).
Pixel (x, y) is `(x % 256, y % 256, (x + y) % 256)`. No external image is used.

`test-picker-navigation.cpp` embeds it in temporary PDFs to exercise concurrent
JPEG 2000 decoding by the reader, title index, preview and prefetch workers.
Without process-wide MuPDF locks this reproduces the OpenJPEG allocator crash.
