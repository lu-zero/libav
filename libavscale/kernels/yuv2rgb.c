#include "libavutil/common.h"
#include "libavutil/intreadwrite.h"

#include "../internal.h"

typedef struct YUV2RGBContext {
    const uint32_t (*coeffs)[3];
} YUV2RGBContext;


#define S(x)    (x) * (1 << 16)
#define RND(x) ((x) + (1 << 15)) >> 16

static const uint32_t bt601_coeffs[3][3] = {
    { S(1), S( 0       ), S( 1.13983f) },
    { S(1), S(-0.39465f), S(-0.5806f ) },
    { S(1), S( 2.03211f), S( 0       ) },
};

static const uint32_t bt709_coeffs[3][3] = {
    { S(1), S( 0       ), S( 1.28033f) },
    { S(1), S(-0.21482f), S(-0.38059f) },
    { S(1), S( 2.12798f), S( 0       ) },
};

static void yuv2rgb(void *ctx,
                    uint8_t *src[AVSCALE_MAX_COMPONENTS],
                    int sstrides[AVSCALE_MAX_COMPONENTS],
                    uint8_t *dst[AVSCALE_MAX_COMPONENTS],
                    int dstrides[AVSCALE_MAX_COMPONENTS],
                    int w, int h)
{
    YUV2RGBContext *yuvctx = ctx;
    int i, j, e;
    int yuv[3], r, g, b;

    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            //TODO range
            r = src[0][i];
            g = src[1][i >> 1] - 128;
            b = src[2][i >> 1] - 128;

            for (e = 0; e < 3; e++)
                yuv[e] = RND(yuvctx->coeffs[e][0] * r +
                             yuvctx->coeffs[e][1] * g +
                             yuvctx->coeffs[e][2] * b);

            // offset for rgb/bgr are already applied
            dst[0][3 * i] = av_clip_uint8(yuv[0]);
            dst[1][3 * i] = av_clip_uint8(yuv[1]);
            dst[2][3 * i] = av_clip_uint8(yuv[2]);
#if 0
            av_log(ctx, AV_LOG_WARNING,
                   "0x%02X 0x%02X 0x%02X -> 0x%02X 0x%02X 0x%02X\n",
                   src[0][i],     src[1][i >> 1],    src[2][i >> 1],
                   dst[0][3 * i], dst[0][3 * i + 1], dst[0][3 * i + 2]);
#endif
        }
        src[0] += sstrides[0];
        if (j & 1) {
            src[1] += sstrides[1];
            src[2] += sstrides[2];
        }
        dst[0] += dstrides[0];
        dst[1] += dstrides[1];
        dst[2] += dstrides[2];
    }
}

static void yuv2rgb_deinit(AVScaleFilterStage *stage)
{
    av_freep(&stage->do_common_ctx);
}

static int yuv2rgb_kernel_init(AVScaleContext *ctx,
                               const AVScaleKernel *kern,
                               AVScaleFilterStage *stage,
                               AVDictionary *opts)
{
    YUV2RGBContext *yuvctx;

    if (ctx->cur_fmt->component[0].depth <= 8)
        stage->do_common = yuv2rgb;
    else
        return AVERROR(ENOSYS);

    stage->do_common_ctx = av_malloc(sizeof(YUV2RGBContext));
    yuvctx = (YUV2RGBContext *)stage->do_common_ctx;
    if (!yuvctx)
        return AVERROR(ENOMEM);

    if (ctx->cur_fmt->space == AVCOL_SPC_BT470BG ||
        ctx->cur_fmt->space == AVCOL_SPC_SMPTE170M)
        yuvctx->coeffs = bt601_coeffs;
    else
        yuvctx->coeffs = bt709_coeffs;

    stage->deinit = yuv2rgb_deinit;

    return 0;
}

const AVScaleKernel avs_yuv2rgb_kernel = {
    .name = "yuv2rgb",
    .kernel_init = yuv2rgb_kernel_init,
};
