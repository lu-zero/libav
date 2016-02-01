#include <string.h>
#include "../internal.h"
#include "libavutil/mem.h"

typedef struct RGBPackContext {
    int roff, goff, boff;
    int step;
} RGBPackContext;

static void rgbpack(void *ctx_,
                    uint8_t *src[AVSCALE_MAX_COMPONENTS],
                    int sstrides[AVSCALE_MAX_COMPONENTS],
                    uint8_t *dst[AVSCALE_MAX_COMPONENTS],
                    int dstrides[AVSCALE_MAX_COMPONENTS],
                    int w, int h)
{
    RGBPackContext *ctx = ctx_;
    uint8_t *rgb[3];
    int i, j, c;

    rgb[0] = src[0] + ctx->roff;
    rgb[1] = src[0] + ctx->goff;
    rgb[2] = src[0] + ctx->boff;

    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            for (c = 0; c < 3; c++) {
                dst[c][i] = rgb[c][0];
                rgb[c] += ctx->step;
            }
        }
        for (c = 0; c < 3; c++) {
            rgb[c] += sstrides[0] - w * ctx->step;
            dst[c] += dstrides[c];
        }
    }
}

static void rgbpack_free(AVScaleFilterStage *stage)
{
    int i;
    for (i = 0; i < 3; i++) {
        av_freep(&stage->dst[i]);
        stage->dst_stride[i] = 0;
    }
    av_freep(&stage->do_common_ctx);
}

static int rgbpack_kernel_init(AVScaleContext *ctx,
                               const AVScaleKernel *kern,
                               AVScaleFilterStage *stage,
                               AVDictionary *opts)
{
    RGBPackContext *rpc;
    int i;
    int dstride = (ctx->cur_w + 31) & ~31;

    stage->do_common = rgbpack;
    stage->deinit    = rgbpack_free;
    stage->do_common_ctx = av_malloc(sizeof(RGBPackContext));
    if (!stage->do_common_ctx)
        return AVERROR(ENOMEM);

    rpc = stage->do_common_ctx;
    rpc->roff = ctx->dst_fmt->component_desc[0].offset;
    rpc->goff = ctx->dst_fmt->component_desc[1].offset;
    rpc->boff = ctx->dst_fmt->component_desc[2].offset;
    rpc->step = ctx->dst_fmt->pixel_next;

    //todo not allocate temp buffer for planar final output
    for (i = 0; i < 3; i++) {
        stage->dst[i] = av_mallocz(ctx->cur_h * dstride);
        if (!stage->dst[i])
            return AVERROR(ENOMEM);
        stage->dst_stride[i] = dstride;
    }

    return 0;
}

const AVScaleKernel avs_rgbpack_kernel = {
    .name = "rgbpack",
    .kernel_init = rgbpack_kernel_init,
};
