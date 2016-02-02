NAME = avscale

HEADERS = avscale.h         \
          version.h         \

KERNELS += kernels/murder.o           \
           kernels/rgb2yuv.o          \
           kernels/rgbpack.o          \
           kernels/rgbunpack.o        \
           kernels/scale.o            \

OBJS = avscale.o                                                        \
       internal.o                                                       \
       utils.o                                                          \
       $(KERNELS)

TESTPROGS = avscale
