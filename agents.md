All the code that is pertinent to this work is in 'sample'
The library that the code in 'sample' uses the most is 'include/nndll.h'

sample/src/   - C++ source code
sample/web/   - HTML/JS/CSS served by the web server
sample/ml/    - YOLO inference daemon + model weights

Furuno_Sample is the main entry point to the program
Web_Server is the primary user interface to the Furuno_sample
Web_server will call pattern_control in a new thread when needed.
stb_image_write is the function that FurunoSample uses to create pngs.

Runtime output directories (created automatically):
  raw/csv/              - radar sweep CSV data (persistent)
  images/unprocessed/   - mono PPI PNGs (persistent)
  images/echo_trail/    - trail PNGs (cleared on startup)
  images/yolo_detect/   - YOLO detection PNGs + labels/ (cleared on startup)
