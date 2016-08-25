/*
 * Copyright (c) 2010, Google, Inc.
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
 * AV1 decoder support via libaom
 */

#include <aom/aom_decoder.h>
#include <aom/aomdx.h>

#include "libavutil/common.h"
#include "libavutil/imgutils.h"

#include "avcodec.h"
#include "internal.h"
#include "libaom.h"

typedef struct AV1DecodeContext {
    struct aom_codec_ctx decoder;
} AV1DecodeContext;

static av_cold int aom_init(AVCodecContext *avctx,
                            const struct aom_codec_iface *iface)
{
    AV1DecodeContext *ctx = avctx->priv_data;
    struct aom_codec_dec_cfg deccfg = {
        /* token partitions+1 would be a decent choice */
        .threads = FFMIN(avctx->thread_count, 16)
    };

    av_log(avctx, AV_LOG_INFO, "%s\n", aom_codec_version_str());
    av_log(avctx, AV_LOG_VERBOSE, "%s\n", aom_codec_build_config());

    if (aom_codec_dec_init(&ctx->decoder, iface, &deccfg, 0) != AOM_CODEC_OK) {
        const char *error = aom_codec_error(&ctx->decoder);
        av_log(avctx, AV_LOG_ERROR, "Failed to initialize decoder: %s\n",
               error);
        return AVERROR(EINVAL);
    }

    return 0;
}

static int aom_decode(AVCodecContext *avctx, void *data, int *got_frame,
                      AVPacket *avpkt)
{
    AV1DecodeContext *ctx = avctx->priv_data;
    AVFrame *picture = data;
    const void *iter = NULL;
    struct aom_image *img;
    int ret;

    if (aom_codec_decode(&ctx->decoder, avpkt->data, avpkt->size, NULL) !=
        AOM_CODEC_OK) {
        const char *error  = aom_codec_error(&ctx->decoder);
        const char *detail = aom_codec_error_detail(&ctx->decoder);

        av_log(avctx, AV_LOG_ERROR, "Failed to decode frame: %s\n", error);
        if (detail)
            av_log(avctx, AV_LOG_ERROR, "  Additional information: %s\n",
                   detail);
        return AVERROR_INVALIDDATA;
    }

    if ((img = aom_codec_get_frame(&ctx->decoder, &iter))) {
        avctx->pix_fmt = ff_aom_imgfmt_to_pixfmt(img->fmt, img->bit_depth);
        if (avctx->pix_fmt == AV_PIX_FMT_NONE) {
            av_log(avctx, AV_LOG_ERROR, "Unsupported output colorspace (%d)\n",
                   img->fmt);
            return AVERROR_INVALIDDATA;
        }

        if ((int) img->d_w != avctx->width || (int) img->d_h != avctx->height) {
            av_log(avctx, AV_LOG_INFO, "dimension change! %dx%d -> %dx%d\n",
                   avctx->width, avctx->height, img->d_w, img->d_h);
            ret = ff_set_dimensions(avctx, img->d_w, img->d_h);
            if (ret < 0)
                return ret;
        }
        if ((ret = ff_get_buffer(avctx, picture, 0)) < 0)
            return ret;
        av_image_copy(picture->data, picture->linesize, (const uint8_t **) img->planes,
                      img->stride, avctx->pix_fmt, img->d_w, img->d_h);
        switch (img->range) {
        case AOM_CR_STUDIO_RANGE:
            picture->color_range = AVCOL_RANGE_MPEG;
            break;
        case AOM_CR_FULL_RANGE:
            picture->color_range = AVCOL_RANGE_JPEG;
            break;
        }
        *got_frame           = 1;
    }
    return avpkt->size;
}

static av_cold int aom_free(AVCodecContext *avctx)
{
    AV1DecodeContext *ctx = avctx->priv_data;
    aom_codec_destroy(&ctx->decoder);
    return 0;
}

static av_cold int av1_init(AVCodecContext *avctx)
{
    return aom_init(avctx, &aom_codec_av1_dx_algo);
}

AVCodec ff_libaom_av1_decoder = {
    .name           = "libaom",
    .long_name      = NULL_IF_CONFIG_SMALL("libaom AV1"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_AV1,
    .priv_data_size = sizeof(AV1DecodeContext),
    .init           = av1_init,
    .close          = aom_free,
    .decode         = aom_decode,
    .capabilities   = AV_CODEC_CAP_AUTO_THREADS | AV_CODEC_CAP_DR1,
};
