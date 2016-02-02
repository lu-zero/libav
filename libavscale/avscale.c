/*
 * Copyright (c) 2015 Kostya Shishkov
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdint.h>
#include <string.h>

#include "libavutil/mem.h"
#include "internal.h"

AVScaleContext *avscale_alloc_context(void)
{
    return av_mallocz(sizeof(AVScaleContext));
}

static int prepare_next_stage(AVScaleContext *ctx, AVScaleFilterStage **stage,
                              const char *name)
{
    int ret;
    AVScaleFilterStage *s;
    int i;

    s = av_mallocz(sizeof(*s));
    if (!s)
        return AVERROR(ENOMEM);

    if (!ctx->head)
        ctx->head = s;

    for (i = 0; i < AVSCALE_MAX_COMPONENTS; i++) {
        s->w[i] = ctx->cur_w >> ctx->cur_fmt->component_desc[i].h_sub_log;
        s->h[i] = ctx->cur_h >> ctx->cur_fmt->component_desc[i].v_sub_log;
    }

    av_log(ctx, AV_LOG_WARNING, "kernel %s\n", name);
    if ((ret = avscale_apply_kernel(ctx, name, s)) < 0)
        goto err;

    if (*stage)
        (*stage)->next = s;
    *stage = s;
    return 0;
err:
    if (s->deinit)
        s->deinit(s);
    av_free(s);
    return ret;
}

// FIXME: proof of a concept
int avscale_build_chain(AVScaleContext *ctx, AVFrame *src, AVFrame *dst)
{
    AVScaleFilterStage *stage = 0;
    int ret;

    // FIXME "leaking like no tomorrow"
    ctx->src_fmt_ref = av_pixformaton_ref(src->formaton);
    ctx->dst_fmt_ref = av_pixformaton_ref(dst->formaton);
    ctx->src_fmt = ctx->src_fmt_ref->pf;
    ctx->dst_fmt = ctx->dst_fmt_ref->pf;
    ctx->cur_w   = src->width;
    ctx->cur_h   = src->height;
    ctx->dst_w   = dst->width;
    ctx->dst_h   = dst->height;
    ctx->cur_fmt = ctx->src_fmt;

    /*XXX proper generic approach would determine first
     * 1) if you convert packed->planar, planar->packed or same->same
     * 2) if it needs scaling
     * 3) if it needs format conversion
     * 4) if it needs color matrix
     * 5) maybe gamma
     * and only then build proper chain */

    /* Same color model */
    if (ctx->src_fmt->model == ctx->dst_fmt->model) {
        /* RGB packed to planar */
        if ( ctx->src_fmt->component_desc[0].packed &&
            !ctx->dst_fmt->component_desc[0].packed) {
            if ((ret = prepare_next_stage(ctx, &stage, "rgbunpack")) < 0)
                return ret;
        /* Diffrent RGB format OR different sizes */
        } else if ((ctx->src_fmt->pixel_size != ctx->dst_fmt->pixel_size) ||
                   (ctx->cur_w != ctx->dst_w || ctx->cur_h != ctx->dst_h)) {
            if ((ret = prepare_next_stage(ctx, &stage, "rgbunpack")) < 0)
                return ret;
            if (ctx->cur_w != ctx->dst_w || ctx->cur_h != ctx->dst_h)
                if ((ret = prepare_next_stage(ctx, &stage, "scale")) < 0)
                    return ret;
            if ((ret = prepare_next_stage(ctx, &stage, "rgbpack")) < 0)
                return ret;
        /* Same format */
        } else {
            if ((ret = prepare_next_stage(ctx, &stage, "murder")) < 0)
                return ret;
        }
    /* Input RGB, Output YUV */
    } else if (ctx->src_fmt->model == AVCOL_MODEL_RGB &&
               ctx->dst_fmt->model == AVCOL_MODEL_YUV) {
        if ((ret = prepare_next_stage(ctx, &stage, "rgbunpack")) < 0)
            return ret;
        if (ctx->cur_w != ctx->dst_w || ctx->cur_h != ctx->dst_h) {
            if ((ret = prepare_next_stage(ctx, &stage, "scale")) < 0)
                return ret;
        }
        if ((ret = prepare_next_stage(ctx, &stage, "rgb2yuv")) < 0)
            return ret;
    /* Input YUV, Output RGB */
    } else if (ctx->src_fmt->model == AVCOL_MODEL_YUV &&
               ctx->dst_fmt->model == AVCOL_MODEL_RGB) {
        if (ctx->cur_w != ctx->dst_w || ctx->cur_h != ctx->dst_h) {
            if ((ret = prepare_next_stage(ctx, &stage, "scale")) < 0)
                return ret;
        }
        if ((ret = prepare_next_stage(ctx, &stage, "yuv2rgb")) < 0)
            return ret;
        if (ctx->dst_fmt->component_desc[0].packed) {
            if ((ret = prepare_next_stage(ctx, &stage, "rgbpack")) < 0)
                return ret;
        }
    } else {
        return AVERROR(ENOSYS);
    }

    ctx->tail = stage;

    return 0;
}

uint8_t *avscale_get_component_ptr(AVFrame *src, int component_id)
{ // currently a simple hack - it has to be extended for e.g. NV12
    if (component_id >= src->formaton->pf->nb_components)
        return 0;
    if (!src->formaton->pf->component_desc[component_id].packed)
        return src->data[src->formaton->pf->component_desc[component_id].plane];
    else
        return src->data[0] + src->formaton->pf->component_desc[component_id].offset;
}

int avscale_get_component_stride(AVFrame *src, int component_id)
{
    if (src->linesize[component_id])
        return src->linesize[component_id];
    else
        return src->linesize[0];
}

int avscale_process_frame(AVScaleContext *ctx, AVFrame *srcf, AVFrame *dstf)
{
    int ret;
    const AVScaleFilterStage *stage;

    int i;

    uint8_t *src[AVSCALE_MAX_COMPONENTS];
    int  sstride[AVSCALE_MAX_COMPONENTS];
    uint8_t *dst[AVSCALE_MAX_COMPONENTS];
    int  dstride[AVSCALE_MAX_COMPONENTS];
    uint8_t *src2[AVSCALE_MAX_COMPONENTS];
    uint8_t *dst2[AVSCALE_MAX_COMPONENTS];

    if (!ctx->head) {
        if ((ret = avscale_build_chain(ctx, srcf, dstf)) < 0)
            return ret;
        av_log(ctx, AV_LOG_INFO, "build chain ret = %d\n", ret);
    }

    stage = ctx->head;

    for (i = 0; i < AVSCALE_MAX_COMPONENTS; i++) {
        src[i]     = avscale_get_component_ptr(srcf, i);
        sstride[i] = avscale_get_component_stride(srcf, i);
    }

    while (stage) {
        for (i = 0; i < AVSCALE_MAX_COMPONENTS; i++) {
            if (stage->src[i]) {
                src[i]     = stage->src[i];
                sstride[i] = stage->src_stride[i];
            }
            if (stage->dst[i]) {
                dst[i]     = stage->dst[i];
                dstride[i] = stage->dst_stride[i];
            } else {
                dst[i]     = avscale_get_component_ptr(dstf, i);
                dstride[i] = avscale_get_component_stride(dstf, i);
            }
        }
        /* copy pointers */
        memcpy(src2, src, sizeof(src2));
        memcpy(dst2, dst, sizeof(dst2));

        if (stage->do_common)
            stage->do_common(stage->do_common_ctx,
                             src2, sstride, dst2, dstride,
                             stage->w[0], stage->h[0]);
        for (i = 0; i < AVSCALE_MAX_COMPONENTS; i++)
            if (stage->do_component[i])
                stage->do_component[i](stage->do_component_ctx[i],
                                       src2[i], sstride[i],
                                       dst2[i], dstride[i],
                                       stage->w[i], stage->h[i]);

        // this stage output buffers are likely to be next stage input
        for (i = 0; i < AVSCALE_MAX_COMPONENTS; i++) {
            src[i]     = dst[i];
            sstride[i] = dstride[i];
        }
        stage = stage->next;
    }

    return 0;
}

void avscale_free(AVScaleContext **pctx)
{
    AVScaleContext *ctx;
    AVScaleFilterStage *s, *next;

    ctx = *pctx;
    if (!ctx)
        return;

    s = ctx->head;

    while (s) {
        next = s->next;
        if (s->deinit)
            s->deinit(s);
        av_free(s);
        s = next;
    }
    ctx->head = ctx->tail = 0;

    *pctx = NULL;
}

