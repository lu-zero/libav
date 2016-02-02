#include <strings.h>
#include "../internal.h"

static void component_murder(void *ctx,
                             uint8_t *src, int sstride,
                             uint8_t *dst, int dstride,
                             int w, int h)
{
    if (!src || !dst)
        return;
    memcpy(dst, src, sstride * h);
}

static int murder_kernel_init(AVScaleContext *ctx,
                              const AVScaleKernel *kern,
                              AVScaleFilterStage *stage,
                              AVDictionary *opts)
{
    int i, n;
    n = ctx->cur_fmt->nb_components;
    if (ctx->cur_fmt->component_desc[0].packed)
        n = 1;
    for (i = 0; i < n; i++)
        stage->do_component[i] = component_murder;
    return 0;
}

static const AVScaleKernel avs_murder_kernel = {
    .name = "murder",
    .kernel_init = murder_kernel_init,
};
