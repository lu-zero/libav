/*
 * Copyright (c) 2012 Google, Inc.
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
 * audio channel mapping filter
 */

#include <ctype.h>

#include "libavutil/avstring.h"
#include "libavutil/channel_layout.h"
#include "libavutil/common.h"
#include "libavutil/mathematics.h"
#include "libavutil/opt.h"
#include "libavutil/samplefmt.h"

#include "audio.h"
#include "avfilter.h"
#include "formats.h"
#include "internal.h"

struct ChannelMap {
    enum AVChannel in_channel;
    enum AVChannel out_channel;
    int in_channel_idx;
    int out_channel_idx;
};

enum MappingMode {
    MAP_NONE,
    MAP_ONE_INT,
    MAP_ONE_STR,
    MAP_PAIR_INT_INT,
    MAP_PAIR_INT_STR,
    MAP_PAIR_STR_INT,
    MAP_PAIR_STR_STR
};

#define MAX_CH 64
typedef struct ChannelMapContext {
    const AVClass *class;
    char *mapping_str;
    char *channel_layout_str;
    AVChannelLayout ch_layout;
    struct ChannelMap map[MAX_CH];
    int nch;
    enum MappingMode mode;
} ChannelMapContext;

#define OFFSET(x) offsetof(ChannelMapContext, x)
#define A AV_OPT_FLAG_AUDIO_PARAM
static const AVOption options[] = {
    { "map", "A comma-separated list of input channel numbers in output order.",
          OFFSET(mapping_str),        AV_OPT_TYPE_STRING, .flags = A },
    { "channel_layout", "Output channel layout.",
          OFFSET(channel_layout_str), AV_OPT_TYPE_STRING, .flags = A },
    { NULL },
};

static const AVClass channelmap_class = {
    .class_name = "channel map filter",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

static char* split(char *message, char delim) {
    char *next = strchr(message, delim);
    if (next)
      *next++ = '\0';
    return next;
}

static int get_channel_idx(char **map, int *ch, char delim, int max_ch)
{
    char *next = split(*map, delim);
    int len;
    int n = 0;
    if (!next && delim == '-')
        return AVERROR(EINVAL);
    len = strlen(*map);
    sscanf(*map, "%d%n", ch, &n);
    if (n != len)
        return AVERROR(EINVAL);
    if (*ch < 0 || *ch > max_ch)
        return AVERROR(EINVAL);
    *map = next;
    return 0;
}

static int get_channel(char **map, enum AVChannel *ch, char delim)
{
    AVChannelLayout ch_layout;
    char *next = split(*map, delim);
    if (!next && delim == '-')
        return AVERROR(EINVAL);

    av_channel_layout_uninit(&ch_layout);
    av_channel_layout_from_string(&ch_layout, *map);

    if (ch_layout.nb_channels != 1)
        return AVERROR(EINVAL);

    *ch = av_channel_layout_channel_index(&ch_layout, 0);
    *map = next;
    return 0;
}

static av_cold int channelmap_init(AVFilterContext *ctx)
{
    ChannelMapContext *s = ctx->priv;
    char *mapping, separator = '|';
    int map_entries = 0;
    enum MappingMode mode;
    uint64_t out_ch_mask = 0;
    int i;

    mapping = s->mapping_str;

    if (!mapping) {
        mode = MAP_NONE;
    } else {
        char *dash = strchr(mapping, '-');
        if (!dash) {  // short mapping
            if (av_isdigit(*mapping))
                mode = MAP_ONE_INT;
            else
                mode = MAP_ONE_STR;
        } else if (av_isdigit(*mapping)) {
            if (av_isdigit(*(dash+1)))
                mode = MAP_PAIR_INT_INT;
            else
                mode = MAP_PAIR_INT_STR;
        } else {
            if (av_isdigit(*(dash+1)))
                mode = MAP_PAIR_STR_INT;
            else
                mode = MAP_PAIR_STR_STR;
        }
    }

    if (mode != MAP_NONE) {
        char *sep = mapping;
        map_entries = 1;
        while ((sep = strchr(sep, separator))) {
            if (*++sep)  // Allow trailing comma
                map_entries++;
        }
    }

    if (map_entries > MAX_CH) {
        av_log(ctx, AV_LOG_ERROR, "Too many channels mapped: '%d'.\n", map_entries);
        return AVERROR(EINVAL);
    }

    for (i = 0; i < map_entries; i++) {
        int in_ch_idx = -1, out_ch_idx = -1;
        enum AVChannel in_ch = 0, out_ch = 0;
        static const char err[] = "Failed to parse channel map\n";
        switch (mode) {
        case MAP_ONE_INT:
            if (get_channel_idx(&mapping, &in_ch_idx, separator, MAX_CH) < 0) {
                av_log(ctx, AV_LOG_ERROR, err);
                return AVERROR(EINVAL);
            }
            s->map[i].in_channel_idx  = in_ch_idx;
            s->map[i].out_channel_idx = i;
            break;
        case MAP_ONE_STR:
            if (get_channel(&mapping, &in_ch, separator) < 0) {
                av_log(ctx, AV_LOG_ERROR, err);
                return AVERROR(EINVAL);
            }
            s->map[i].in_channel      = in_ch;
            s->map[i].out_channel_idx = i;
            break;
        case MAP_PAIR_INT_INT:
            if (get_channel_idx(&mapping, &in_ch_idx, '-', MAX_CH) < 0 ||
                get_channel_idx(&mapping, &out_ch_idx, separator, MAX_CH) < 0) {
                av_log(ctx, AV_LOG_ERROR, err);
                return AVERROR(EINVAL);
            }
            s->map[i].in_channel_idx  = in_ch_idx;
            s->map[i].out_channel_idx = out_ch_idx;
            break;
        case MAP_PAIR_INT_STR:
            if (get_channel_idx(&mapping, &in_ch_idx, '-', MAX_CH) < 0 ||
                get_channel(&mapping, &out_ch, separator) < 0 ||
                (1 << out_ch) & out_ch_mask) {
                av_log(ctx, AV_LOG_ERROR, err);
                return AVERROR(EINVAL);
            }
            s->map[i].in_channel_idx  = in_ch_idx;
            s->map[i].out_channel     = out_ch;
            out_ch_mask |= (1 << out_ch);
            break;
        case MAP_PAIR_STR_INT:
            if (get_channel(&mapping, &in_ch, '-') < 0 ||
                get_channel_idx(&mapping, &out_ch_idx, separator, MAX_CH) < 0) {
                av_log(ctx, AV_LOG_ERROR, err);
                return AVERROR(EINVAL);
            }
            s->map[i].in_channel      = in_ch;
            s->map[i].out_channel_idx = out_ch_idx;
            break;
        case MAP_PAIR_STR_STR:
            if (get_channel(&mapping, &in_ch, '-') < 0 ||
                get_channel(&mapping, &out_ch, separator) < 0 ||
                (1 << out_ch) & out_ch_mask) {
                av_log(ctx, AV_LOG_ERROR, err);
                return AVERROR(EINVAL);
            }
            s->map[i].in_channel = in_ch;
            s->map[i].out_channel = out_ch;
            out_ch_mask |= (1 << out_ch);
            break;
        }
    }
    s->mode          = mode;
    s->nch           = map_entries;
    if (out_ch_mask)
        av_channel_layout_from_mask(&s->ch_layout, out_ch_mask);
    else
        av_channel_layout_default(&s->ch_layout, map_entries);

    if (s->channel_layout_str) {
        int ret;
        AVChannelLayout fmt;
        av_channel_layout_uninit(&fmt);
        ret = av_channel_layout_from_string(&fmt, s->channel_layout_str);
        if (ret < 0) {
            av_log(ctx, AV_LOG_ERROR, "Error parsing channel layout: '%s'.\n",
                   s->channel_layout_str);
            return AVERROR(EINVAL);
        }
        if (mode == MAP_NONE) {
            int i;
            s->nch = fmt.nb_channels;
            for (i = 0; i < s->nch; i++) {
                s->map[i].in_channel_idx  = i;
                s->map[i].out_channel_idx = i;
            }
        } else if (out_ch_mask && av_channel_layout_compare(&s->ch_layout, &fmt)) {
            char *chlstr = av_channel_layout_describe(&s->ch_layout);
            av_log(ctx, AV_LOG_ERROR,
                   "Output channel layout '%s' does not match the list of channel mapped: '%s'.\n",
                   s->channel_layout_str, chlstr);
            av_free(chlstr);
            return AVERROR(EINVAL);
        } else if (s->nch != fmt.nb_channels) {
            av_log(ctx, AV_LOG_ERROR,
                   "Output channel layout %s does not match the number of channels mapped %d.\n",
                   s->channel_layout_str, s->nch);
            return AVERROR(EINVAL);
        }
        ret = av_channel_layout_copy(&s->ch_layout, &fmt);
        if (ret < 0)
            return ret;
    }
    if (av_channel_layout_check(&s->ch_layout)) {
        av_log(ctx, AV_LOG_ERROR, "Output channel layout is not set and "
               "cannot be guessed from the maps.\n");
        return AVERROR(EINVAL);
    }

    if (mode == MAP_PAIR_INT_STR || mode == MAP_PAIR_STR_STR) {
        for (i = 0; i < s->nch; i++) {
            s->map[i].out_channel_idx =
                av_channel_layout_channel_index(&s->ch_layout, s->map[i].out_channel);
        }
    }

    return 0;
}

static int channelmap_query_formats(AVFilterContext *ctx)
{
    ChannelMapContext *s = ctx->priv;
    AVFilterChannelLayouts *channel_layouts = NULL;

    ff_add_channel_layout(&channel_layouts, s->ch_layout.u.mask);

    ff_set_common_formats(ctx, ff_planar_sample_fmts());
    ff_set_common_samplerates(ctx, ff_all_samplerates());
    ff_channel_layouts_ref(ff_all_channel_layouts(), &ctx->inputs[0]->out_channel_layouts);
    ff_channel_layouts_ref(channel_layouts,          &ctx->outputs[0]->in_channel_layouts);

    return 0;
}

static int channelmap_filter_frame(AVFilterLink *inlink, AVFrame *buf)
{
    AVFilterContext  *ctx = inlink->dst;
    AVFilterLink *outlink = ctx->outputs[0];
    const ChannelMapContext *s = ctx->priv;
    const int nch_in = inlink->ch_layout.nb_channels;
    const int nch_out = s->nch;
    int ret, ch;
    uint8_t *source_planes[MAX_CH];

    memcpy(source_planes, buf->extended_data,
           nch_in * sizeof(source_planes[0]));

    if (nch_out > nch_in) {
        if (nch_out > FF_ARRAY_ELEMS(buf->data)) {
            uint8_t **new_extended_data =
                av_mallocz(nch_out * sizeof(*buf->extended_data));
            if (!new_extended_data) {
                av_frame_free(&buf);
                return AVERROR(ENOMEM);
            }
            if (buf->extended_data == buf->data) {
                buf->extended_data = new_extended_data;
            } else {
                av_free(buf->extended_data);
                buf->extended_data = new_extended_data;
            }
        } else if (buf->extended_data != buf->data) {
            av_free(buf->extended_data);
            buf->extended_data = buf->data;
        }
    }

    for (ch = 0; ch < nch_out; ch++) {
        buf->extended_data[s->map[ch].out_channel_idx] =
            source_planes[s->map[ch].in_channel_idx];
    }

    if (buf->data != buf->extended_data)
        memcpy(buf->data, buf->extended_data,
           FFMIN(FF_ARRAY_ELEMS(buf->data), nch_out) * sizeof(buf->data[0]));

    ret = av_channel_layout_copy(&buf->ch_layout, &outlink->ch_layout);
    if (ret < 0)
        return ret;

    return ff_filter_frame(outlink, buf);
}

static int channelmap_config_input(AVFilterLink *inlink)
{
    AVFilterContext *ctx = inlink->dst;
    ChannelMapContext *s = ctx->priv;
    int nb_channels = inlink->ch_layout.nb_channels;
    int i, err = 0;

    for (i = 0; i < s->nch; i++) {
        struct ChannelMap *m = &s->map[i];

        if (s->mode == MAP_PAIR_STR_INT || s->mode == MAP_PAIR_STR_STR) {
            m->in_channel_idx = av_channel_layout_channel_index(&inlink->ch_layout,
                                                                m->in_channel);
        }

        if (m->in_channel_idx < 0 || m->in_channel_idx >= nb_channels) {
            char *chlstr = av_channel_layout_describe(&inlink->ch_layout);
            if (m->in_channel) {
                av_log(ctx, AV_LOG_ERROR,
                       "input channel '%s' not available from input layout '%s'\n",
                       av_channel_name(m->in_channel), chlstr);
            } else {
                av_log(ctx, AV_LOG_ERROR,
                       "input channel #%d not available from input layout '%s'\n",
                       m->in_channel_idx, chlstr);
            }
            err = AVERROR(EINVAL);
            av_free(chlstr);
        }
    }

    return err;
}

static const AVFilterPad avfilter_af_channelmap_inputs[] = {
    {
        .name           = "default",
        .type           = AVMEDIA_TYPE_AUDIO,
        .filter_frame   = channelmap_filter_frame,
        .config_props   = channelmap_config_input
    },
    { NULL }
};

static const AVFilterPad avfilter_af_channelmap_outputs[] = {
    {
        .name = "default",
        .type = AVMEDIA_TYPE_AUDIO
    },
    { NULL }
};

AVFilter ff_af_channelmap = {
    .name          = "channelmap",
    .description   = NULL_IF_CONFIG_SMALL("Remap audio channels."),
    .init          = channelmap_init,
    .query_formats = channelmap_query_formats,
    .priv_size     = sizeof(ChannelMapContext),
    .priv_class    = &channelmap_class,

    .inputs        = avfilter_af_channelmap_inputs,
    .outputs       = avfilter_af_channelmap_outputs,
};
