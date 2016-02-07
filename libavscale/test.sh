#!/bin/sh

# rgb2yuv
echo "murder (rgb24)"
./libavscale/any2any-test ../libavscale/sample.ppm murder.ppm

echo "scale rgb24"
./libavscale/any2any-test ../libavscale/sample.ppm scaled_murder.ppm none 1000x100

echo "rgb2yuv"
./libavscale/any2any-test ../libavscale/sample.ppm rgb2yuv.pgm

echo "rgb2yuv + scale"
./libavscale/any2any-test ../libavscale/sample.ppm scaled_rgb2yuv.pgm none 100x100


# rgb2rgba
echo "rgb2rgba"
./libavscale/any2any-test ../libavscale/sample.ppm rgba.ppm rgba

echo "rgb2bgra"
./libavscale/any2any-test ../libavscale/sample.ppm bgra.ppm bgra


# yuv2rgb
echo "murder (yuv)"
./libavscale/any2any-test rgb2yuv.pgm murder.pgm

echo "scale yuv420p"
./libavscale/any2any-test rgb2yuv.pgm scaled_murder.pgm none 1000x2000

echo "yuv2rgb"
./libavscale/any2any-test rgb2yuv.pgm yuv2rgb.ppm

echo "yuv2rgb + scale"
./libavscale/any2any-test rgb2yuv.pgm scaled_yuv2rgb.ppm none 2000x1000

echo "yuv2bgr"
./libavscale/any2any-test rgb2yuv.pgm yuv2bgr.ppm bgr24

echo "yuv2bgr + scale"
./libavscale/any2any-test rgb2yuv.pgm scaled_yuv2bgr.ppm bgr24 100x100

