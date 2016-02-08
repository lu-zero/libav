#include <string.h>
#include "../internal.h"
#include "libavutil/mem.h"

typedef struct RGBUnpackContext {
    int roff, goff, boff;
    int step;
    int nb_comp;
    int needs_alpha;
} RGBUnpackContext;

static void rgbunpack(void *ctx_,
                      uint8_t *src[AVSCALE_MAX_COMPONENTS],
                      int sstrides[AVSCALE_MAX_COMPONENTS],
                      uint8_t *dst[AVSCALE_MAX_COMPONENTS],
                      int dstrides[AVSCALE_MAX_COMPONENTS],
                      int w, int h)
{
    RGBUnpackContext *ctx = ctx_;
    uint8_t *rgb[4];
    int i, j, c;

    rgb[0] = src[0] + ctx->roff;
    rgb[1] = src[0] + ctx->goff;
    rgb[2] = src[0] + ctx->boff;

    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            for (c = 0; c < ctx->nb_comp; c++) {
                dst[c][i] = rgb[c][0];
                rgb[c] += ctx->step - ctx->roff;
            }
        }
        for (c = 0; c < ctx->nb_comp; c++) {
            rgb[c] += sstrides[0] - w * ctx->step; // - ctx->roff?
            dst[c] += dstrides[c];
        }
    }
}

static void alphagen(void *ctx,
                     uint8_t *src, int sstride,
                     uint8_t *dst, int dstride,
                     int w, int h)
{
    memset(dst, 0xFF, dstride * h);
}

static void rgbunpack_free(AVScaleFilterStage *stage)
{
    av_freep(&stage->do_common_ctx);
}

static int rgbunpack_kernel_init(AVScaleContext *ctx,
                                 const AVScaleKernel *kern,
                                 AVScaleFilterStage *stage,
                                 AVDictionary *opts)
{
    RGBUnpackContext *ruc;

    stage->do_common = rgbunpack;
    stage->deinit    = rgbunpack_free;
    stage->do_common_ctx = av_malloc(sizeof(RGBUnpackContext));
    if (!stage->do_common_ctx)
        return AVERROR(ENOMEM);

    ruc = stage->do_common_ctx;
    ruc->roff = ctx->cur_fmt->component[0].offset;
    ruc->goff = ctx->cur_fmt->component[1].offset;
    ruc->boff = ctx->cur_fmt->component[2].offset;
    ruc->step = ctx->cur_fmt->pixel_size;
    ruc->nb_comp = ctx->cur_fmt->nb_components;

    // create alpha if source formaton doesn't have it and dst needs it
    if (ctx->cur_fmt->nb_components == 3 && ctx->dst_fmt->nb_components == 4)
        stage->do_component[ctx->dst_fmt->component[3].offset] = alphagen;

    return 0;
}

const AVScaleKernel avs_rgbunpack_kernel = {
    .name = "rgbunpack",
    .kernel_init = rgbunpack_kernel_init,
};
