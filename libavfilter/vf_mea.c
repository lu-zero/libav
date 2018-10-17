/*
 * Copyright (c) 2018 Luca Barbato
 *
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

/**
 * @file
 * measure one video on top of another
 */

#include "avfilter.h"
#include "formats.h"
#include "libavutil/common.h"
#include "libavutil/eval.h"
#include "libavutil/avstring.h"
#include "libavutil/avassert.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/mathematics.h"
#include "libavutil/opt.h"
#include "internal.h"
#include "video.h"

#include <mea.h>

typedef struct MeasureContext {
    const AVClass *class;
    char *out_path;
    FILE *out_file;
    MeaContext *m;

    AVFrame *ref;
    AVFrame *rec;
} MeasureContext;

static av_cold int init(AVFilterContext *ctx)
{
    MeasureContext *s = ctx->priv;

    if (!s->out_path) {
        s->out_file = stderr;
    } else {
        s->out_file = fopen(s->out_path, "w");
        if (!s->out_file)
            return AVERROR(EIO);
    }

    s->m = mea_context_new();
    if (!s->m) {
        if (s->out_path)
            fclose(s->out_file);
        return AVERROR(ENOMEM);
    }

    fprintf(s->out_file, "%s, %s, %s, %s\n",
            "ms-ssim",
            "psnr-y",
            "psnr-u",
            "psnr-v");

    return 0;
}

static av_cold void uninit(AVFilterContext *ctx)
{
    MeasureContext *s = ctx->priv;

    mea_context_drop(&s->m);

    av_frame_free(&s->ref);
    av_frame_free(&s->rec);
}

static int query_formats(AVFilterContext *ctx)
{
    static const enum AVPixelFormat pix[] = { AV_PIX_FMT_YUV420P,  AV_PIX_FMT_NONE };
    AVFilterFormats *formats = ff_make_format_list(pix);

    ff_formats_ref(formats, &ctx->inputs [0]->out_formats);
    ff_formats_ref(formats, &ctx->inputs [1]->out_formats);
    ff_formats_ref(formats, &ctx->outputs[0]->in_formats );

    return 0;
}

static int config_input_ref(AVFilterLink *inlink)
{

    return 0;
}

static int config_input_rec(AVFilterLink *inlink)
{

    return 0;
}

static int config_output(AVFilterLink *outlink)
{
    AVFilterContext *ctx = outlink->src;

    outlink->w = ctx->inputs[0]->w;
    outlink->h = ctx->inputs[0]->h;
    outlink->time_base = ctx->inputs[0]->time_base;

    return 0;
}

static int filter_frame_ref(AVFilterLink *inlink, AVFrame *frame)
{
    MeasureContext *s = inlink->dst->priv;

    s->ref         = frame;

    return 0;
}

static int filter_frame_rec(AVFilterLink *inlink, AVFrame *frame)
{
    MeasureContext *s = inlink->dst->priv;

    s->rec    = frame;

    return 0;
}

static int output_frame(AVFilterContext *ctx)
{
    MeasureContext *s = ctx->priv;
    AVFilterLink *outlink = ctx->outputs[0];
    int ret = ff_filter_frame(outlink, s->ref);
    s->ref = NULL;
    s->rec = NULL;

    return ret;
}

static int map_frame(MeaFrame *m, AVFrame *f) {
    int i;
    const AVPixFmtDescriptor * desc = av_pix_fmt_desc_get(f->format);
    if (!desc)
        return AVERROR_BUG;

    for (i = 0; i < 3; i++) {
        int shift_w = i > 0 ? desc->log2_chroma_w : 0;
        int shift_h = i > 0 ? desc->log2_chroma_h : 0;
        m->planes[i].data = f->data[i];
        m->planes[i].width  = f->width >> shift_w;
        m->planes[i].height = f->height >> shift_h;
        m->planes[i].stride = f->linesize[i];
    }

    return 0;
}

static void measure_frames(MeasureContext *s)
{
    MeaFrame ref = (MeaFrame) { 0 };
    MeaFrame rec = (MeaFrame) { 0 };
    MeaFrameQuality q = (MeaFrameQuality) { { 0 } };

    map_frame(&ref, s->ref);
    map_frame(&rec, s->rec);

    mea_frame_process(s->m, &ref, &rec, &q);

    fprintf(s->out_file, "%f, %f, %f, %f\n",
            q.ssim,
            q.psnr[0],
            q.psnr[1],
            q.psnr[2]);
}

static int request_frame(AVFilterLink *outlink)
{
    AVFilterContext *ctx = outlink->src;
    MeasureContext    *s = ctx->priv;
    int ret = 0;

    /* get a frame on the ref input */
    if (!s->ref) {
        ret = ff_request_frame(ctx->inputs[0]);
        if (ret < 0)
            return ret;
    }

    /* get a frame on he rec input */
    if (!s->rec) {
        ret = ff_request_frame(ctx->inputs[1]);
        if (ret < 0)
            return ret;
    }

    measure_frames(s);

    return output_frame(ctx);
}

#define OFFSET(x) offsetof(MeasureContext, x)
#define FLAGS AV_OPT_FLAG_VIDEO_PARAM
static const AVOption options[] = {
    {"out_file", "File where to store the per-frame measurements", OFFSET(out_path), AV_OPT_TYPE_STRING, {.str=NULL}, 0, 0, FLAGS },
    { NULL },
};

static const AVClass measure_class = {
    .class_name = "measure",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

static const AVFilterPad avfilter_vf_measure_inputs[] = {
    {
        .name         = "ref",
        .type         = AVMEDIA_TYPE_VIDEO,
        .config_props = config_input_ref,
        .filter_frame = filter_frame_ref,
        .needs_fifo   = 1,
    },
    {
        .name         = "rec",
        .type         = AVMEDIA_TYPE_VIDEO,
        .config_props = config_input_rec,
        .filter_frame = filter_frame_rec,
        .needs_fifo   = 1,
    },
    { NULL }
};

static const AVFilterPad avfilter_vf_measure_outputs[] = {
    {
        .name          = "default",
        .type          = AVMEDIA_TYPE_VIDEO,
        .config_props  = config_output,
        .request_frame = request_frame,
    },
    { NULL }
};

AVFilter ff_vf_measure = {
    .name      = "measure",
    .description = NULL_IF_CONFIG_SMALL("Measure the difference between two streams by some metrics"),

    .init      = init,
    .uninit    = uninit,

    .priv_size = sizeof(MeasureContext),
    .priv_class = &measure_class,

    .query_formats = query_formats,

    .inputs    = avfilter_vf_measure_inputs,
    .outputs   = avfilter_vf_measure_outputs,
};
