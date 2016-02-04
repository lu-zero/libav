#include <string.h>

#include "libavutil/intreadwrite.h"
#include "libavutil/mem.h"

#include "../internal.h"

typedef struct RGBPackContext {
    int off[3], shift[3];
    int step;
    int inbpp;
    int be;
} RGBPackContext;

static void rgbpack_fields(void *ctx_,
                           uint8_t *src[AVSCALE_MAX_COMPONENTS],
                           int sstrides[AVSCALE_MAX_COMPONENTS],
                           uint8_t *dst[AVSCALE_MAX_COMPONENTS],
                           int dstrides[AVSCALE_MAX_COMPONENTS],
                           int w, int h)
{
    RGBPackContext *ctx = ctx_;
    uint8_t *rgb[3], *dest;
    unsigned val;
    int i, j, c;

    rgb[0] = src[0];
    rgb[1] = src[1];
    rgb[2] = src[2];
    dest   = dst[0];

    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            val = 0;
            if (ctx->inbpp <= 8) {
                for (c = 0; c < 3; c++)
                    val |= rgb[c][i] << ctx->shift[c];
            } else {
                for (c = 0; c < 3; c++)
                    val |= AV_RN16(rgb[c] + i * 2) << ctx->shift[c];
            }
            switch (ctx->step) {
            case 1:
                dest[i] = val;
                break;
            case 2:
                if (ctx->be) AV_WB16(dest + i * 2, val);
                else         AV_WL16(dest + i * 2, val);
                break;
            case 4:
                if (ctx->be) AV_WB32(dest + i * 4, val);
                else         AV_WL32(dest + i * 4, val);
                break;
            }
        }
        for (c = 0; c < 3; c++)
            rgb[c] += sstrides[0];
        dest += dstrides[0];
    }
}

static void rgbpack24(void *ctx_,
                      uint8_t *src[AVSCALE_MAX_COMPONENTS],
                      int sstrides[AVSCALE_MAX_COMPONENTS],
                      uint8_t *dst[AVSCALE_MAX_COMPONENTS],
                      int dstrides[AVSCALE_MAX_COMPONENTS],
                      int w, int h)
{
    RGBPackContext *ctx = ctx_;
    uint8_t *rgb[3], *dest;
    int i, j, c;

    rgb[0] = src[0];
    rgb[1] = src[1];
    rgb[2] = src[2];
    dest   = dst[0];

    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            for (c = 0; c < 3; c++)
                dest[ctx->off[c]] = (ctx->inbpp <= 8) ? rgb[c][i]
                                                      : AV_RN16(rgb[c] + i * 2) >> (ctx->inbpp - 8);
            dest += ctx->step;
        }
        for (c = 0; c < 3; c++)
            rgb[c] += sstrides[0];
        dest += dstrides[0] - w * ctx->step;
    }
}

static void rgbpck_free(AVScaleFilterStage *stage)
{
    av_freep(&stage->do_common_ctx);
}

static int rgbpack_kernel_init(AVScaleContext *ctx,
                               const AVScaleKernel *kern,
                               AVScaleFilterStage *stage,
                               AVDictionary *opts)
{
    RGBPackContext *rc;
    int i;

    if (!ctx->dst_fmt->component[0].next)
        stage->do_common = rgbpack_fields;
    else
        stage->do_common = rgbpack24;

    stage->deinit    = rgbpck_free;
    stage->do_common_ctx = av_malloc(sizeof(RGBPackContext));
    if (!stage->do_common_ctx)
        return AVERROR(ENOMEM);

    rc = stage->do_common_ctx;
    for (i = 0; i < 3; i++) {
        rc->off[i]   = ctx->dst_fmt->component[i].offset;
        rc->shift[i] = ctx->dst_fmt->component[i].shift;
    }
    rc->step  = ctx->dst_fmt->pixel_size;
    rc->be    = ctx->dst_fmt->flags & AV_PIX_FORMATON_FLAG_BE;
    rc->inbpp = ctx->cur_fmt->component[0].depth;

    return 0;
}

const AVScaleKernel avs_rgbpack_kernel = {
    .name = "rgbpack",
    .kernel_init = rgbpack_kernel_init,
};
