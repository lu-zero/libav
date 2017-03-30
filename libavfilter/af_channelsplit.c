/*
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
 * Channel split filter
 *
 * Split an audio stream into per-channel streams.
 */

#include "libavutil/attributes.h"
#include "libavutil/channel_layout.h"
#include "libavutil/internal.h"
#include "libavutil/opt.h"

#include "audio.h"
#include "avfilter.h"
#include "formats.h"
#include "internal.h"

typedef struct ChannelSplitContext {
    const AVClass *class;

    AVChannelLayout ch_layout;
} ChannelSplitContext;

#define OFFSET(x) offsetof(ChannelSplitContext, x)
#define A AV_OPT_FLAG_AUDIO_PARAM
static const AVOption options[] = {
    { "channel_layout", "Input channel layout.", OFFSET(ch_layout),
        AV_OPT_TYPE_CHANNEL_LAYOUT, { .str = "stereo" }, .flags = A },
    { NULL },
};

static const AVClass channelsplit_class = {
    .class_name = "channelsplit filter",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

static av_cold int init(AVFilterContext *ctx)
{
    ChannelSplitContext *s = ctx->priv;
    int nb_channels = s->ch_layout.nb_channels;
    int ret = 0, i;

    for (i = 0; i < nb_channels; i++) {
        AVFilterPad pad  = { 0 };
        ret = av_channel_layout_get_channel(&s->ch_layout, i);
        if (ret < 0)
            goto fail;

        pad.type = AVMEDIA_TYPE_AUDIO;
        pad.name = av_channel_name(ret);

        ff_insert_outpad(ctx, i, &pad);
    }

fail:
    return ret;
}

static int query_formats(AVFilterContext *ctx)
{
    ChannelSplitContext *s = ctx->priv;
    AVFilterChannelLayouts *in_layouts = NULL;
    int i;

    ff_set_common_formats    (ctx, ff_planar_sample_fmts());
    ff_set_common_samplerates(ctx, ff_all_samplerates());

    ff_add_channel_layout(&in_layouts, s->ch_layout.u.mask);
    ff_channel_layouts_ref(in_layouts, &ctx->inputs[0]->out_channel_layouts);

    for (i = 0; i < ctx->nb_outputs; i++) {
        AVFilterChannelLayouts *out_layouts = NULL;
        int ret = av_channel_layout_get_channel(&s->ch_layout, i);
        if (ret < 0)
            return ret;

        ff_add_channel_layout(&out_layouts, ret);
        ff_channel_layouts_ref(out_layouts, &ctx->outputs[i]->in_channel_layouts);
    }

    return 0;
}

static int filter_frame(AVFilterLink *inlink, AVFrame *buf)
{
    AVFilterContext *ctx = inlink->dst;
    int i, ret = 0;

    for (i = 0; i < ctx->nb_outputs; i++) {
        AVFrame *buf_out = av_frame_clone(buf);
        enum AVChannel channel;

        if (!buf_out) {
            ret = AVERROR(ENOMEM);
            break;
        }
        buf_out->data[0] = buf_out->extended_data[0] = buf_out->extended_data[i];
        channel = av_channel_layout_get_channel(&buf->ch_layout, i);
        av_channel_layout_from_mask(&buf_out->ch_layout, 1 << channel);

        ret = ff_filter_frame(ctx->outputs[i], buf_out);
        if (ret < 0)
            break;
    }
    av_frame_free(&buf);
    return ret;
}

static const AVFilterPad avfilter_af_channelsplit_inputs[] = {
    {
        .name           = "default",
        .type           = AVMEDIA_TYPE_AUDIO,
        .filter_frame   = filter_frame,
    },
    { NULL }
};

AVFilter ff_af_channelsplit = {
    .name           = "channelsplit",
    .description    = NULL_IF_CONFIG_SMALL("Split audio into per-channel streams"),
    .priv_size      = sizeof(ChannelSplitContext),
    .priv_class     = &channelsplit_class,

    .init           = init,
    .query_formats  = query_formats,

    .inputs  = avfilter_af_channelsplit_inputs,
    .outputs = NULL,

    .flags   = AVFILTER_FLAG_DYNAMIC_OUTPUTS,
};
