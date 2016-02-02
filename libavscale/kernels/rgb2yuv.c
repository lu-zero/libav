#include "libavutil/intreadwrite.h"

#include "../internal.h"

static inline int clip(int a)
{
    if (a < 0)   return 0;
    if (a > 255) return 255;
                 return a;
}

static void rgb2yuv420(void *ctx,
                       uint8_t *src[AVSCALE_MAX_COMPONENTS],
                       int sstrides[AVSCALE_MAX_COMPONENTS],
                       uint8_t *dst[AVSCALE_MAX_COMPONENTS],
                       int dstrides[AVSCALE_MAX_COMPONENTS],
                       int w, int h)
{
    int i, j;
    int Y, U, V;

    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            //TODO coefficients
            Y = (int)(0.299f * src[0][i] + 0.587f * src[1][i] + 0.114f * src[2][i]);
            dst[0][i] = clip(Y);
            if (!(j & 1) && !(i & 1)) {
                int r, g, b;
                r =   src[0][i]     + src[0][sstrides[0] + i]
                    + src[0][i + 1] + src[0][sstrides[0] + i + 1];
                g =   src[1][i]     + src[1][sstrides[1] + i]
                    + src[1][i + 1] + src[1][sstrides[1] + i + 1];
                b =   src[2][i]     + src[2][sstrides[2] + i]
                    + src[2][i + 1] + src[2][sstrides[2] + i + 1];
                U = (int)(-0.14713f * r - 0.28886f * g + 0.436f   * b);
                V = (int)( 0.615f   * r - 0.51499f * g - 0.10001f * b);
                dst[1][i >> 1] = clip(U + 128);
                dst[2][i >> 1] = clip(V + 128);
            }
        }
        src[0] += sstrides[0];
        src[1] += sstrides[1];
        src[2] += sstrides[2];
        dst[0] += dstrides[0];
        if (j & 1) {
            dst[1] += dstrides[1];
            dst[2] += dstrides[2];
        }
    }
}

#define READ_RGB(c, x, y) AV_RN16(src[c] + (x) * 2 + (y) * sstrides[c])
static void rgb10_to_yuv420(void *ctx,
                            uint8_t *src[AVSCALE_MAX_COMPONENTS],
                            int sstrides[AVSCALE_MAX_COMPONENTS],
                            uint8_t *dst[AVSCALE_MAX_COMPONENTS],
                            int dstrides[AVSCALE_MAX_COMPONENTS],
                            int w, int h)
{
    int i, j;
    int Y, U, V;

    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            //TODO coefficients
            Y = (int)(  0.299f * READ_RGB(0, i, 0)
                      + 0.587f * READ_RGB(1, i, 0)
                      + 0.114f * READ_RGB(2, i, 0));
            dst[0][i] = clip(Y >> 2);
            if (!(j & 1) && !(i & 1)) {
                int r, g, b;
                r =   READ_RGB(0, i,     0) + READ_RGB(0, i,     1)
                    + READ_RGB(0, i + 1, 0) + READ_RGB(0, i + 1, 1);
                g =   READ_RGB(1, i,     0) + READ_RGB(1, i,     1)
                    + READ_RGB(1, i + 1, 0) + READ_RGB(1, i + 1, 1);
                b =   READ_RGB(2, i,     0) + READ_RGB(2, i,     1)
                    + READ_RGB(2, i + 1, 0) + READ_RGB(2, i + 1, 1);
                U = (int)(-0.14713f * r - 0.28886f * g + 0.436f   * b) >> 2;
                V = (int)( 0.615f   * r - 0.51499f * g - 0.10001f * b) >> 2;
                dst[1][i >> 1] = clip(U + 128);
                dst[2][i >> 1] = clip(V + 128);
            }
        }
        src[0] += sstrides[0];
        src[1] += sstrides[1];
        src[2] += sstrides[2];
        dst[0] += dstrides[0];
        if (j & 1) {
            dst[1] += dstrides[1];
            dst[2] += dstrides[2];
        }
    }
}

static int rgb2yuv_kernel_init(AVScaleContext *ctx,
                               const AVScaleKernel *kern,
                               AVScaleFilterStage *stage,
                               AVDictionary *opts)
{
    if (ctx->cur_fmt->component_desc[0].depth <= 8)
        stage->do_common = rgb2yuv420;
    else if (ctx->cur_fmt->component_desc[0].depth <= 16)
        stage->do_common = rgb10_to_yuv420;
    else
        return AVERROR(ENOSYS);

    return 0;
}

const AVScaleKernel avs_rgb2yuv_kernel = {
    .name = "rgb2yuv",
    .kernel_init = rgb2yuv_kernel_init,
};
