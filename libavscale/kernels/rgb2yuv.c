#include "libavutil/common.h"
#include "libavutil/intreadwrite.h"

#include "../internal.h"

typedef struct RGB2YUVContext {
    const uint32_t (*coeffs)[3];
} RGB2YUVContext;


#define S(x)  (x) * (1 << 16)
#define RND(x) ((x) + (1 << 15)) >> 16

static const uint32_t bt601_coeffs[3][3] = {
    { S( 0.299f),   S( 0.587f),   S( 0.114f)   },
    { S(-0.14713f), S(-0.28886f), S( 0.436f)   },
    { S( 0.615f),   S(-0.51499f), S(-0.10001f) },
};

static const uint32_t bt709_coeffs[3][3] = {
    { S( 0.2126f),  S( 0.7152f),  S( 0.0722f)  },
    { S(-0.09991f), S(-0.33609f), S( 0.436f)   },
    { S( 0.615f),   S(-0.55861f), S(-0.05639f) },
};

static void rgb2yuv420(void *ctx,
                       uint8_t *src[AVSCALE_MAX_COMPONENTS],
                       int sstrides[AVSCALE_MAX_COMPONENTS],
                       uint8_t *dst[AVSCALE_MAX_COMPONENTS],
                       int dstrides[AVSCALE_MAX_COMPONENTS],
                       int w, int h)
{
    RGB2YUVContext *rgbctx = ctx;
    int i, j;
    int Y, U, V;

    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            Y = RND(rgbctx->coeffs[0][0] * src[0][i] +
                    rgbctx->coeffs[0][1] * src[1][i] +
                    rgbctx->coeffs[0][2] * src[2][i]);
            dst[0][i] = av_clip_uint8(Y);
            if (!(j & 1) && !(i & 1)) {
                int r, g, b;
                // average to deal with subsampling
                r = (src[0][i]     + src[0][sstrides[0] + i] +
                     src[0][i + 1] + src[0][sstrides[0] + i + 1]) / 4;
                g = (src[1][i]     + src[1][sstrides[1] + i] +
                     src[1][i + 1] + src[1][sstrides[1] + i + 1]) / 4;
                b = (src[2][i]     + src[2][sstrides[2] + i] +
                     src[2][i + 1] + src[2][sstrides[2] + i + 1]) / 4;
                //av_log(ctx, AV_LOG_INFO, "0x%02X 0x%02X 0x%02X\n", r, g, b);
                U = RND(rgbctx->coeffs[1][0] * r +
                        rgbctx->coeffs[1][1] * g +
                        rgbctx->coeffs[1][2] * b);
                V = RND(rgbctx->coeffs[2][0] * r +
                        rgbctx->coeffs[2][1] * g +
                        rgbctx->coeffs[2][2] * b);
                dst[1][i >> 1] = av_clip_uint8(U + 128);
                dst[2][i >> 1] = av_clip_uint8(V + 128);
            }
#if 0
            av_log(ctx, AV_LOG_ERROR,
                   "0x%02X 0x%02X 0x%02X -> 0x%02X 0x%02X 0x%02X\n",
                   src[0][i], src[1][i],     src[2][i],
                   dst[0][i], dst[1][i >> 1],dst[2][i >> 1]);
#endif
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
    RGB2YUVContext *rgbctx = ctx;
    int i, j;
    int Y, U, V;

    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            Y = RND(rgbctx->coeffs[0][0] * READ_RGB(0, i, 0) +
                    rgbctx->coeffs[0][1] * READ_RGB(1, i, 0) +
                    rgbctx->coeffs[0][2] * READ_RGB(2, i, 0)) >> 2;
            dst[0][i] = av_clip_uint8(Y);
            if (!(j & 1) && !(i & 1)) {
                int r, g, b;
                r =   READ_RGB(0, i,     0) + READ_RGB(0, i,     1)
                    + READ_RGB(0, i + 1, 0) + READ_RGB(0, i + 1, 1);
                g =   READ_RGB(1, i,     0) + READ_RGB(1, i,     1)
                    + READ_RGB(1, i + 1, 0) + READ_RGB(1, i + 1, 1);
                b =   READ_RGB(2, i,     0) + READ_RGB(2, i,     1)
                    + READ_RGB(2, i + 1, 0) + READ_RGB(2, i + 1, 1);
                U = RND(rgbctx->coeffs[1][0] * r +
                        rgbctx->coeffs[1][1] * g +
                        rgbctx->coeffs[1][2] * b) >> 2;
                V = RND(rgbctx->coeffs[2][0] * r +
                        rgbctx->coeffs[2][1] * g +
                        rgbctx->coeffs[2][2] * b) >> 2;
                dst[1][i >> 1] = av_clip_uint8(U + 128);
                dst[2][i >> 1] = av_clip_uint8(V + 128);
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

static void copy_alpha(void *ctx,
                       uint8_t *src, int sstride,
                       uint8_t *dst, int dstride,
                       int w, int h)
{
    memcpy(dst, src, dstride * h);
}

static void rgb2yuv_deinit(AVScaleFilterStage *stage)
{
    av_freep(&stage->do_common_ctx);
}

static int rgb2yuv_kernel_init(AVScaleContext *ctx,
                               const AVScaleKernel *kern,
                               AVScaleFilterStage *stage,
                               AVDictionary *opts)
{
    RGB2YUVContext *rgbctx;

    if (ctx->cur_fmt->component[0].depth <= 8)
        stage->do_common = rgb2yuv420;
    else if (ctx->cur_fmt->component[0].depth <= 16)
        stage->do_common = rgb10_to_yuv420;
    else
        return AVERROR(ENOSYS);

    if (ctx->dst_fmt->nb_components == 4)
        stage->do_component[3] = copy_alpha;

    stage->do_common_ctx = av_malloc(sizeof(RGB2YUVContext));
    rgbctx = (RGB2YUVContext *)stage->do_common_ctx;
    if (!rgbctx)
        return AVERROR(ENOMEM);

    rgbctx->coeffs = bt709_coeffs;

    stage->deinit = rgb2yuv_deinit;

    return 0;
}

const AVScaleKernel avs_rgb2yuv_kernel = {
    .name = "rgb2yuv",
    .kernel_init = rgb2yuv_kernel_init,
};
