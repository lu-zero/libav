NAME = avscale

HEADERS = avscale.h         \
          version.h         \

KERNELS += kernels/rgbunpack.o        \
           kernels/yuv2rgb.o

OBJS = avscale.o                                                        \
       internal.o                                                       \
       utils.o                                                          \
       $(KERNELS)
