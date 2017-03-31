/*
 * Sony OpenMG (OMA) common data
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

#include "internal.h"
#include "oma.h"
#include "libavcodec/avcodec.h"
#include "libavutil/channel_layout.h"

const uint16_t ff_oma_srate_tab[8] = { 320, 441, 480, 882, 960, 0 };

const AVCodecTag ff_oma_codec_tags[] = {
    { AV_CODEC_ID_ATRAC3,      OMA_CODECID_ATRAC3  },
    { AV_CODEC_ID_ATRAC3P,     OMA_CODECID_ATRAC3P },
    { AV_CODEC_ID_MP3,         OMA_CODECID_MP3     },
    { AV_CODEC_ID_PCM_S16BE,   OMA_CODECID_LPCM    },
    { 0 },
};

/** map ATRAC-X channel id to internal channel layout */
const AVChannelLayout ff_oma_chid_to_native_layout[7] = {
    AV_CHANNEL_LAYOUT_MONO,
    AV_CHANNEL_LAYOUT_STEREO,
    AV_CHANNEL_LAYOUT_SURROUND,
    AV_CHANNEL_LAYOUT_4POINT0,
    AV_CHANNEL_LAYOUT_5POINT1_BACK,
    AV_CHANNEL_LAYOUT_6POINT1_BACK,
    AV_CHANNEL_LAYOUT_7POINT1
};
