#include <string.h>

#include "libavutil/mem.h"
#include "libavutil/common.h"

#include "../internal.h"

typedef struct ScaleContext {
    int dst_w, dst_h;
} ScaleContext;

/* nearest neighbour */
static void component_scale(void *ctx_,
                            uint8_t *src, int sstride,
                            uint8_t *dst, int dstride,
                            int w, int h)
{
    ScaleContext *ctx = ctx_;
    int i, j;
    int x0, y0;
    if (!src || !dst)
        return;

    for (j = 0; j < ctx->dst_h; j++) {
        y0 = (j * h / ctx->dst_h) * sstride;
        for (i = 0; i < ctx->dst_w; i++) {
            x0 = i * w / ctx->dst_w;
            dst[i] = src[x0 + y0];
        }
        dst += dstride;
    }
}

static void scale_deinit(AVScaleFilterStage *stage)
{
    int i;

    for (i = 0; i < AVSCALE_MAX_COMPONENTS; i++) {
        av_freep(&stage->dst[i]);
        stage->dst_stride[i] = 0;
        av_freep(&stage->do_component_ctx[i]);
    }
}

static int scale_kernel_init(AVScaleContext *ctx,
                             const AVScaleKernel *kern,
                             AVScaleFilterStage *stage,
                             AVDictionary *opts)
{
    ScaleContext *sc;
    int i, n, dstride;

    n = ctx->dst_fmt->nb_components;
    if (ctx->cur_fmt->component_desc[0].depth > 8) {
        av_log(ctx, AV_LOG_ERROR, "scale returning %d\n", AVERROR(ENOSYS));
        return AVERROR(ENOSYS);
    }

    stage->deinit = scale_deinit;

    for (i = 0; i < n; i++) {
        stage->do_component[i] = component_scale;

        stage->do_component_ctx[i] = av_malloc(sizeof(ScaleContext));
        sc = (ScaleContext*)stage->do_component_ctx[i];
        if (!sc)
            return AVERROR(ENOMEM);
        sc->dst_w = AV_CEIL_RSHIFT(ctx->dst_w,
                                   ctx->cur_fmt->component_desc[i].h_sub_log);
        sc->dst_h = AV_CEIL_RSHIFT(ctx->dst_h,
                                   ctx->cur_fmt->component_desc[i].v_sub_log);

        dstride = (sc->dst_w + 31) & ~31;

        stage->dst[i] = av_mallocz(sc->dst_h * dstride);
        if (!stage->dst[i])
            return AVERROR(ENOMEM);
        stage->dst_stride[i] = dstride;
    }

    ctx->cur_w = ctx->dst_w;
    ctx->cur_h = ctx->dst_h;

    return 0;
}

const AVScaleKernel avs_scale_kernel = {
    .name = "scale",
    .kernel_init = scale_kernel_init,
};
