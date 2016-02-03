#include "libavutil/common.h"
#include "libavutil/intreadwrite.h"

#include "../internal.h"

static void yuv2rgb(void *ctx,
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
            Y = src[0][i];
            U = src[1][i >> 1] - 128;
            V = src[2][i >> 1] - 128;
            dst[0][3*i+0] = av_clip_uint8(Y + (             91881 * V + 32768 >> 16));
            dst[0][3*i+1] = av_clip_uint8(Y + (-22554 * U - 46802 * V + 32768 >> 16));
            dst[0][3*i+2] = av_clip_uint8(Y + (116130 * U             + 32768 >> 16));
        }
        src[0] += sstrides[0];
        src[1] += sstrides[1] * (j & 1);
        src[2] += sstrides[2] * (j & 1);
        dst[0] += dstrides[0];
    }
}

static int yuv2rgb_kernel_init(AVScaleContext *ctx,
                               const AVScaleKernel *kern,
                               AVScaleFilterStage *stage,
                               AVDictionary *opts)
{
    if (ctx->cur_fmt->component_desc[0].depth <= 8)
        stage->do_common = yuv2rgb;
    else
        return AVERROR(ENOSYS);

    return 0;
}

const AVScaleKernel avs_yuv2rgb_kernel = {
    .name = "yuv2rgb",
    .kernel_init = yuv2rgb_kernel_init,
};
