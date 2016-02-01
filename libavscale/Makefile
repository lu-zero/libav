NAME = avscale

HEADERS = avscale.h         \
          version.h         \

KERNELS += kernels/rgbpack.o          \
           kernels/rgbunpack.o        \
           kernels/yuv2rgb.o

OBJS = avscale.o                                                        \
       internal.o                                                       \
       utils.o                                                          \
       $(KERNELS)

TESTPROGS = avscale
