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
        s->w[i] = ctx->cur_w >> ctx->cur_fmt.component_desc[i].h_sub_log;
        s->h[i] = ctx->cur_h >> ctx->cur_fmt.component_desc[i].v_sub_log;
    }

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

    // XXX luzero stabs anton because avformaton is not refcounted
    ctx->src_fmt = src->formaton;
    ctx->dst_fmt = dst->formaton;
    // XXX luzero blames kostya
    // TODO av_formaton_clone and/or av_formaton_ref
    ctx->cur_w   = src->width;
    ctx->cur_h   = src->height;
    ctx->dst_w   = dst->width;
    ctx->dst_h   = dst->height;
    ctx->cur_fmt = *ctx->src_fmt;

    if (ctx->src_fmt->space == ctx->dst_fmt->space) {
        if ( ctx->src_fmt->component_desc[0].packed &&
            !ctx->dst_fmt->component_desc[0].packed) {
            if ((ret = prepare_next_stage(ctx, &stage, "rgbunp")) < 0)
                return ret;
        } else if (ctx->src_fmt->component_desc[0].step != ctx->dst_fmt->component_desc[0].step) {
            if ((ret = prepare_next_stage(ctx, &stage, "rgbunp")) < 0)
                return ret;
            if (ctx->cur_w != ctx->dst_w || ctx->cur_h != ctx->dst_h)
                if ((ret = prepare_next_stage(ctx, &stage, "scale")) < 0)
                    return ret;
            if ((ret = prepare_next_stage(ctx, &stage, "rgbpck")) < 0)
                return ret;
        } else {
            if ((ret = prepare_next_stage(ctx, &stage, "murder")) < 0)
                return ret;
        }
    } else if (ctx->src_fmt->space == AVS_RGB &&
               ctx->dst_fmt->space == AVS_YUV) {
        if ((ret = prepare_next_stage(ctx, &stage, "rgbunp")) < 0)
            return ret;
        if (ctx->cur_w != ctx->dst_w || ctx->cur_h != ctx->dst_h) {
            if ((ret = prepare_next_stage(ctx, &stage, "scale")) < 0)
                return ret;
        }
        if ((ret = prepare_next_stage(ctx, &stage, "rgb2yuv")) < 0)
            return ret;
    } else
        return AVERROR(ENOSYS);

    ctx->tail = stage;

    return 0;
}

uint8_t *avscale_get_component_ptr(AVFrame *src, int component_id)
{ // currently a simple hack - it has to be extended for e.g. NV12
    if (component_id >= src->formaton->nb_components)
        return 0;
    if (!src->formaton->component_desc[component_id].packed)
        return src->data[src->formaton->component_desc[component_id].plane];
    else
        return src->data[0] + src->formaton->component_desc[component_id].off;
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
        av_log(ctx, AV_LOG_VERBOSE, "build chain ret = %d\n",
               ret);
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

void avscale_free_context(AVScaleContext *ctx)
{
    AVScaleFilterStage *s, *next;

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
}

