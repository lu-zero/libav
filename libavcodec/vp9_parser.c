/*
 * Copyright (C) 2014 Ronald S. Bultje
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

#include "libavutil/intreadwrite.h"
#include "parser.h"

typedef struct VP9ParseContext {
    int n_frames; // 1-8
    int size[8];
} VP9ParseContext;

static void vp9_parse_frame(AVCodecParserContext *ctx,
                            const uint8_t *buf, int size)
{
    if (buf[0] & 0x4) {
        ctx->pict_type = AV_PICTURE_TYPE_P;
        ctx->key_frame = 0;
    } else {
        ctx->pict_type = AV_PICTURE_TYPE_I;
        ctx->key_frame = 1;
    }
}

#define case_n(a, rd)                                               \
    case a:                                                         \
        while (n_frames--) {                                        \
            int sz = rd;                                            \
            idx += a;                                               \
            if (sz > size) {                                        \
                s->n_frames = 0;                                    \
                av_log(ctx, AV_LOG_ERROR,                           \
                       "Superframe packet size too big: %d > %d\n", \
                       sz, size);                                   \
                return AVERROR_INVALIDDATA;                         \
            }                                                       \
            if (first) {                                            \
                first = 0;                                          \
                *out_data = data;                                   \
                *out_size = sz;                                     \
                s->n_frames = n_frames;                             \
            } else {                                                \
                s->size[n_frames] = sz;                             \
            }                                                       \
            data += sz;                                             \
            size -= sz;                                             \
        }                                                           \
        vp9_parse_frame(ctx, *out_data, *out_size);                 \
        return *out_size

static int vp9_parse(AVCodecParserContext *ctx, AVCodecContext *avctx,
                     const uint8_t **out_data, int *out_size,
                     const uint8_t *data, int size)
{
    VP9ParseContext *s = ctx->priv_data;
    int marker;

    if (s->n_frames > 0) {
        *out_data = data;
        *out_size = s->size[--s->n_frames];
        vp9_parse_frame(ctx, *out_data, *out_size);

        return s->n_frames > 0 ? *out_size : size /* i.e. include idx tail */;
    }

    if (!size)
        return size;

    marker = data[size - 1];
    if ((marker & 0xe0) == 0xc0) {
        int nbytes   = 1 + ((marker >> 3) & 0x3);
        int n_frames = 1 + (marker & 0x7);
        int idx_sz = 2 + n_frames * nbytes;

        if (size >= idx_sz && data[size - idx_sz] == marker) {
            const uint8_t *idx = data + size + 1 - idx_sz;
            int first = 1;

            switch (nbytes) {
                case_n(1, *idx);
                case_n(2, AV_RL16(idx));
                case_n(3, AV_RL24(idx));
                case_n(4, AV_RL32(idx));
            }
        }
    }

    *out_data = data;
    *out_size = size;
    vp9_parse_frame(ctx, data, size);

    return size;
}

#undef case_n

AVCodecParser ff_vp9_parser = {
    .codec_ids      = { AV_CODEC_ID_VP9 },
    .priv_data_size = sizeof(VP9ParseContext),
    .parser_parse   = vp9_parse,
};
