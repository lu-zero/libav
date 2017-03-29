/*
 * Copyright (c) 2006 Michael Niedermayer <michaelni@gmx.at>
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
 * audio channel layout utility functions
 */

#include <stdint.h>

#include "avstring.h"
#include "avutil.h"
#include "channel_layout.h"
#include "common.h"

static const char * const channel_names[] = {
    [AV_CHAN_FRONT_LEFT              ] = "FL",
    [AV_CHAN_FRONT_RIGHT             ] = "FR",
    [AV_CHAN_FRONT_CENTER            ] = "FC",
    [AV_CHAN_LOW_FREQUENCY           ] = "LFE",
    [AV_CHAN_BACK_LEFT               ] = "BL",
    [AV_CHAN_BACK_RIGHT              ] = "BR",
    [AV_CHAN_FRONT_LEFT_OF_CENTER    ] = "FLC",
    [AV_CHAN_FRONT_RIGHT_OF_CENTER   ] = "FRC",
    [AV_CHAN_BACK_CENTER             ] = "BC",
    [AV_CHAN_SIDE_LEFT               ] = "SL",
    [AV_CHAN_SIDE_RIGHT              ] = "SR",
    [AV_CHAN_TOP_CENTER              ] = "TC",
    [AV_CHAN_TOP_FRONT_LEFT          ] = "TFL",
    [AV_CHAN_TOP_FRONT_CENTER        ] = "TFC",
    [AV_CHAN_TOP_FRONT_RIGHT         ] = "TFR",
    [AV_CHAN_TOP_BACK_LEFT           ] = "TBL",
    [AV_CHAN_TOP_BACK_CENTER         ] = "TBC",
    [AV_CHAN_TOP_BACK_RIGHT          ] = "TBR",
    [AV_CHAN_STEREO_LEFT             ] = "DL",
    [AV_CHAN_STEREO_RIGHT            ] = "DR",
    [AV_CHAN_WIDE_LEFT               ] = "WL",
    [AV_CHAN_WIDE_RIGHT              ] = "WR",
    [AV_CHAN_SURROUND_DIRECT_LEFT    ] = "SDL",
    [AV_CHAN_SURROUND_DIRECT_RIGHT   ] = "SDR",
    [AV_CHAN_LOW_FREQUENCY_2         ] = "LFE2",
};

const char *av_channel_name(enum AVChannel channel_id)
{
    if (channel_id < 0 || channel_id >= FF_ARRAY_ELEMS(channel_names))
        return "?";
    return channel_names[channel_id];
}

static const struct {
    const char *name;
    AVChannelLayout layout;
} channel_layout_map[] = {
    { "mono",          AV_CHANNEL_LAYOUT_MONO                },
    { "stereo",        AV_CHANNEL_LAYOUT_STEREO              },
    { "stereo",        AV_CHANNEL_LAYOUT_STEREO_DOWNMIX      },
    { "2.1",           AV_CHANNEL_LAYOUT_2POINT1             },
    { "3.0",           AV_CHANNEL_LAYOUT_SURROUND            },
    { "3.0(back)",     AV_CHANNEL_LAYOUT_2_1                 },
    { "3.1",           AV_CHANNEL_LAYOUT_3POINT1             },
    { "4.0",           AV_CHANNEL_LAYOUT_4POINT0             },
    { "quad",          AV_CHANNEL_LAYOUT_QUAD                },
    { "quad(side)",    AV_CHANNEL_LAYOUT_2_2                 },
    { "4.1",           AV_CHANNEL_LAYOUT_4POINT1             },
    { "5.0",           AV_CHANNEL_LAYOUT_5POINT0             },
    { "5.0",           AV_CHANNEL_LAYOUT_5POINT0_BACK        },
    { "5.1",           AV_CHANNEL_LAYOUT_5POINT1             },
    { "5.1",           AV_CHANNEL_LAYOUT_5POINT1_BACK        },
    { "6.0",           AV_CHANNEL_LAYOUT_6POINT0             },
    { "6.0(front)",    AV_CHANNEL_LAYOUT_6POINT0_FRONT       },
    { "hexagonal",     AV_CHANNEL_LAYOUT_HEXAGONAL           },
    { "6.1",           AV_CHANNEL_LAYOUT_6POINT1             },
    { "6.1",           AV_CHANNEL_LAYOUT_6POINT1_BACK        },
    { "6.1(front)",    AV_CHANNEL_LAYOUT_6POINT1_FRONT       },
    { "7.0",           AV_CHANNEL_LAYOUT_7POINT0             },
    { "7.0(front)",    AV_CHANNEL_LAYOUT_7POINT0_FRONT       },
    { "7.1",           AV_CHANNEL_LAYOUT_7POINT1             },
    { "7.1(wide)",     AV_CHANNEL_LAYOUT_7POINT1_WIDE        },
    { "7.1(wide)",     AV_CHANNEL_LAYOUT_7POINT1_WIDE_BACK   },
    { "octagonal",     AV_CHANNEL_LAYOUT_OCTAGONAL           },
    { "hexadecagonal", AV_CHANNEL_LAYOUT_HEXADECAGONAL       },
    { "downmix",       AV_CHANNEL_LAYOUT_STEREO_DOWNMIX,     },
    { 0 }
};

static uint64_t get_channel_layout_single(const char *name, int name_len)
{
    int i;
    char *end;
    int64_t layout;

    for (i = 0; i < FF_ARRAY_ELEMS(channel_layout_map) - 1; i++) {
        if (strlen(channel_layout_map[i].name) == name_len &&
            !memcmp(channel_layout_map[i].name, name, name_len))
            return channel_layout_map[i].layout.u.mask;
    }
    for (i = 0; i < FF_ARRAY_ELEMS(channel_names); i++)
        if (channel_names[i] &&
            strlen(channel_names[i]) == name_len &&
            !memcmp(channel_names[i], name, name_len))
            return (int64_t)1 << i;
    i = strtol(name, &end, 10);
    if (end - name == name_len ||
        (end + 1 - name == name_len && *end  == 'c'))
        return av_get_default_channel_layout(i);
    layout = strtoll(name, &end, 0);
    if (end - name == name_len)
        return FFMAX(layout, 0);
    return 0;
}

uint64_t av_get_channel_layout(const char *name)
{
    const char *n, *e;
    const char *name_end = name + strlen(name);
    int64_t layout = 0, layout_single;

    for (n = name; n < name_end; n = e + 1) {
        for (e = n; e < name_end && *e != '+' && *e != '|'; e++);
        layout_single = get_channel_layout_single(n, e - n);
        if (!layout_single)
            return 0;
        layout |= layout_single;
    }
    return layout;
}

void av_get_channel_layout_string(char *buf, int buf_size,
                                  int nb_channels, uint64_t channel_layout)
{
    int i;

    if (nb_channels <= 0)
        nb_channels = av_get_channel_layout_nb_channels(channel_layout);

    for (i = 0; channel_layout_map[i].name; i++)
        if (nb_channels    == channel_layout_map[i].layout.nb_channels &&
            channel_layout == channel_layout_map[i].layout.u.mask) {
            av_strlcpy(buf, channel_layout_map[i].name, buf_size);
            return;
        }

    snprintf(buf, buf_size, "%d channels", nb_channels);
    if (channel_layout) {
        int i, ch;
        av_strlcat(buf, " (", buf_size);
        for (i = 0, ch = 0; i < 64; i++) {
            if ((channel_layout & (UINT64_C(1) << i))) {
                const char *name = av_channel_name(i);
                if (name) {
                    if (ch > 0)
                        av_strlcat(buf, "|", buf_size);
                    av_strlcat(buf, name, buf_size);
                }
                ch++;
            }
        }
        av_strlcat(buf, ")", buf_size);
    }
}

int av_get_channel_layout_nb_channels(uint64_t channel_layout)
{
    return av_popcount64(channel_layout);
}

uint64_t av_get_default_channel_layout(int nb_channels)
{
    switch(nb_channels) {
    case 1: return AV_CH_LAYOUT_MONO;
    case 2: return AV_CH_LAYOUT_STEREO;
    case 3: return AV_CH_LAYOUT_SURROUND;
    case 4: return AV_CH_LAYOUT_QUAD;
    case 5: return AV_CH_LAYOUT_5POINT0;
    case 6: return AV_CH_LAYOUT_5POINT1;
    case 7: return AV_CH_LAYOUT_6POINT1;
    case 8: return AV_CH_LAYOUT_7POINT1;
    case 16: return AV_CH_LAYOUT_HEXADECAGONAL;
    default: return 0;
    }
}

int av_get_channel_layout_channel_index(uint64_t channel_layout,
                                        uint64_t channel)
{
    if (!(channel_layout & channel) ||
        av_get_channel_layout_nb_channels(channel) != 1)
        return AVERROR(EINVAL);
    channel_layout &= channel - 1;
    return av_get_channel_layout_nb_channels(channel_layout);
}

const char *av_get_channel_name(uint64_t channel)
{
    int i;
    if (av_get_channel_layout_nb_channels(channel) != 1)
        return NULL;
    for (i = 0; i < 64; i++)
        if ((1ULL<<i) & channel)
            return av_channel_name(i);
    return NULL;
}

uint64_t av_channel_layout_extract_channel(uint64_t channel_layout, int index)
{
    int i;

    if (av_get_channel_layout_nb_channels(channel_layout) <= index)
        return 0;

    for (i = 0; i < 64; i++) {
        if ((1ULL << i) & channel_layout && !index--)
            return 1ULL << i;
    }
    return 0;
}

void av_channel_layout_from_mask(AVChannelLayout *channel_layout,
                                 uint64_t mask)
{
    av_channel_layout_uninit(channel_layout);
    channel_layout->order       = AV_CHANNEL_ORDER_NATIVE;
    channel_layout->nb_channels = av_popcount64(mask);
    channel_layout->u.mask      = mask;
}

int av_channel_layout_from_string(AVChannelLayout *channel_layout,
                                  const char *str)
{
    int i;

    for (i = 0; i < FF_ARRAY_ELEMS(channel_layout_map); i++) {
        if (!strcmp(str, channel_layout_map[i].name)) {
            *channel_layout = channel_layout_map[i].layout;
            return 0;
        }
    }

    if (sscanf(str, "%d channels", &channel_layout->nb_channels) == 1) {
        channel_layout->order = AV_CHANNEL_ORDER_UNSPEC;
        return 0;
    }

    //TODO
    return 0;
}

void av_channel_layout_uninit(AVChannelLayout *channel_layout)
{
    if (channel_layout->order == AV_CHANNEL_ORDER_CUSTOM)
        av_freep(&channel_layout->u.map);
    memset(channel_layout, 0, sizeof(*channel_layout));
}

int av_channel_layout_copy(AVChannelLayout *dst, const AVChannelLayout *src)
{
    *dst = *src;
    if (src->order == AV_CHANNEL_ORDER_CUSTOM) {
        dst->u.map = av_malloc(src->nb_channels * sizeof(*dst->u.map));
        if (!dst->u.map)
            return AVERROR(ENOMEM);
        memcpy(dst->u.map, src->u.map, src->nb_channels * sizeof(*src->u.map));
    }
    return 0;
}

char *av_channel_layout_describe(AVChannelLayout *channel_layout)
{
    int i;

    switch (channel_layout->order) {
    case AV_CHANNEL_ORDER_NATIVE:
        for (i = 0; channel_layout_map[i].name; i++)
            if (channel_layout->u.mask == channel_layout_map[i].layout.u.mask)
                return av_strdup(channel_layout_map[i].name);
        // fall-through
    case AV_CHANNEL_ORDER_CUSTOM: {
        // max 4 bytes for channel name + a separator
        int size = 5 * channel_layout->nb_channels + 1;
        char *ret;
        int i;

        ret = av_mallocz(size);
        if (!ret)
            return NULL;

        for (i = 0; i < channel_layout->nb_channels; i++) {
            enum AVChannel ch = av_channel_layout_get_channel(channel_layout, i);
            const char *ch_name = av_channel_name(ch);

            if (i)
                av_strlcat(ret, "|", size);
            av_strlcat(ret, ch_name, size);
        }
        return ret;
        }
    case AV_CHANNEL_ORDER_UNSPEC: {
        char buf[64];
        snprintf(buf, sizeof(buf), "%d channels", channel_layout->nb_channels);
        return av_strdup(buf);
        }
    default:
        return NULL;
    }
}

enum AVChannel av_channel_layout_get_channel(const AVChannelLayout *channel_layout,
                                             int idx)
{
    int i;

    if (idx < 0 || idx >= channel_layout->nb_channels)
        return AVERROR(EINVAL);

    switch (channel_layout->order) {
    case AV_CHANNEL_ORDER_CUSTOM:
        return channel_layout->u.map[idx];
    case AV_CHANNEL_ORDER_NATIVE:
        for (i = 0; i < 64; i++) {
            if ((1ULL << i) & channel_layout->u.mask && !idx--)
                return i;
        }
    default:
        return AVERROR(EINVAL);
    }
}

int av_channel_layout_channel_index(const AVChannelLayout *channel_layout,
                                    enum AVChannel channel)
{
    int i;

    switch (channel_layout->order) {
    case AV_CHANNEL_ORDER_CUSTOM:
        for (i = 0; i < channel_layout->nb_channels; i++)
            if (channel_layout->u.map[i] == channel)
                return i;
        return AVERROR(EINVAL);
    case AV_CHANNEL_ORDER_NATIVE: {
        uint64_t mask = channel_layout->u.mask;
        if (!(mask & (1ULL << channel)))
            return AVERROR(EINVAL);
        mask &= (1ULL << channel) - 1;
        return av_popcount64(mask);
        }
    default:
        return AVERROR(EINVAL);
    }
}

int av_channel_layout_check(const AVChannelLayout *channel_layout)
{
    if (!channel_layout || channel_layout->nb_channels <= 0)
        return 0;

    switch (channel_layout->order) {
    case AV_CHANNEL_ORDER_NATIVE:
        return av_popcount64(channel_layout->u.mask) == channel_layout->nb_channels;
    case AV_CHANNEL_ORDER_CUSTOM:
        return !!channel_layout->u.map;
    case AV_CHANNEL_ORDER_UNSPEC:
        return 1;
    default:
        return 0;
    }
}

int av_channel_layout_compare(const AVChannelLayout *chl, const AVChannelLayout *chl1)
{
    int i;

    /* different channel counts -> not equal */
    if (chl->nb_channels != chl1->nb_channels)
        return 1;

    /* if only one is unspecified -> not equal */
    if ((chl->order  == AV_CHANNEL_ORDER_UNSPEC) !=
        (chl1->order == AV_CHANNEL_ORDER_UNSPEC))
        return 1;
    /* both are unspecified -> equal */
    else if (chl->order == AV_CHANNEL_ORDER_UNSPEC)
        return 0;

    /* can compare masks directly */
    if (chl->order != AV_CHANNEL_ORDER_CUSTOM &&
        chl->order == chl1->order)
        return chl->u.mask != chl1->u.mask;

    /* compare channel by channel */
    for (i = 0; i < chl->nb_channels; i++)
        if (av_channel_layout_get_channel(chl,  i) !=
            av_channel_layout_get_channel(chl1, i))
            return 1;
    return 0;
}

void av_channel_layout_default(AVChannelLayout *ch_layout, int nb_channels)
{
    av_channel_layout_uninit(ch_layout);
    switch (nb_channels) {
    case 1: *ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_MONO;     break;
    case 2: *ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO;   break;
    case 3: *ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_SURROUND; break;
    case 4: *ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_QUAD;     break;
    case 5: *ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_5POINT0;  break;
    case 6: *ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_5POINT1;  break;
    case 7: *ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_6POINT1;  break;
    case 8: *ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_7POINT1;  break;
    default:
        ch_layout->order       = AV_CHANNEL_ORDER_UNSPEC;
        ch_layout->nb_channels = nb_channels;
    }
}

uint64_t av_channel_layout_subset(const AVChannelLayout *channel_layout,
                                  uint64_t mask)
{
    uint64_t ret;
    int i;

    if (channel_layout->order == AV_CHANNEL_ORDER_NATIVE)
        return channel_layout->u.mask & mask;

    for (i = 0; i < 64; i++)
        if (mask & (1ULL << i) &&
            av_channel_layout_channel_index(channel_layout, i) >= 0)
            ret |= (1ULL << i);

    return ret;
}
