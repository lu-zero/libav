/*
 * VC-2 (vc2hqdecode) video decoder
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

#include <string.h>
#include <vc2hqdecode/vc2hqdecode.h>

#include "libavutil/common.h"
#include "libavutil/log.h"

#include "avcodec.h"
#include "internal.h"

typedef struct VC2hqdecodeContext {
    VC2DecoderParamsUser params;
    VC2DecoderOutputFormat fmt;
    VC2DecoderHandle  handle;
    VC2DecoderLoggers loggers;
    int ostride[3];
} VC2hqdecodeContext;

static void vc2hqdecode_error_callback(char *msg, void *opaque)
{
    av_log(NULL, AV_LOG_ERROR, "%s\n", msg);
}

static void vc2hqdecode_warn_callback(char *msg, void *opaque)
{
    av_log(NULL, AV_LOG_WARNING, "%s\n", msg);
}

static void vc2hqdecode_info_callback(char *msg, void *opaque)
{
    av_log(NULL, AV_LOG_INFO, "%s\n", msg);
}

static void vc2hqdecode_debug_callback(char *msg, void *opaque)
{
    av_log(NULL, AV_LOG_DEBUG, "%s\n", msg);
}

static int vc2hqdecode_pix_fmt(AVCodecContext *avctx, VC2DecoderOutputFormat *fmt)
{
    switch (fmt->signal_range) {
    case VC2DECODER_PSR_10BITVID:
        switch (fmt->source_sampling) {
        case VC2DECODER_CDS_420:
            return AV_PIX_FMT_YUV420P10LE;
        case VC2DECODER_CDS_422:
            return AV_PIX_FMT_YUV422P10LE;
        case VC2DECODER_CDS_444:
            return AV_PIX_FMT_YUV444P10LE;
        default:
            goto fail;
        }
    case VC2DECODER_PSR_12BITVID:
        switch (fmt->source_sampling) {
        case VC2DECODER_CDS_420:
            return AV_PIX_FMT_YUV420P12LE;
        case VC2DECODER_CDS_422:
            return AV_PIX_FMT_YUV422P12LE;
        case VC2DECODER_CDS_444:
            return AV_PIX_FMT_YUV444P12LE;
        default:
            goto fail;
        }
        default:
            goto fail;
    }

fail:
    av_log(avctx, AV_LOG_ERROR, "Unsupported format\n");
    return AVERROR(ENOSYS);
}

static av_cold int vc2hqdecode_decode_init(AVCodecContext *avctx)
{
    VC2hqdecodeContext *vcc = avctx->priv_data;

    vc2decode_init();

    vcc->loggers.error = vc2hqdecode_error_callback;
    vcc->loggers.warn  = vc2hqdecode_warn_callback;
    vcc->loggers.info  = vc2hqdecode_info_callback;
    vcc->loggers.debug = vc2hqdecode_debug_callback;

    vc2decoder_init_logging(vcc->loggers);

    vcc->handle = vc2decode_create();

    // FIXME read from the bitstream
    avctx->pix_fmt = AV_PIX_FMT_YUV422P12LE;

    memset(&vcc->fmt,    0, sizeof(vcc->fmt));
    memset(&vcc->params, 0, sizeof(vcc->params));
    vcc->params.threads = avctx->thread_count;

    if (vc2decode_set_parameters(vcc->handle, vcc->params) != VC2DECODER_OK)
        return AVERROR(EINVAL);

    return 0;
}

static int vc2hqdecode_decode_frame(AVCodecContext *avctx, void *data,
                                    int *got_frame, AVPacket *avpkt)
{
    VC2hqdecodeContext *vcc = avctx->priv_data;
    AVFrame *avframe = data;
    uint8_t *dp = avpkt->data;
    int size = avpkt->size;
    VC2DecoderResult res;

    res = vc2decode_synchronise(vcc->handle, (char **) &dp, size, 0);

    if (res == VC2DECODER_OK_RECONFIGURED) { // Update the size
        size -= dp - avpkt->data;
    } else { // Rewind the pointer.
        dp = avpkt->data;
    }

    res = vc2decode_get_output_format(vcc->handle, &vcc->fmt);
    if (res != VC2DECODER_OK)
        return AVERROR_INVALIDDATA;

    avctx->pix_fmt = vc2hqdecode_pix_fmt(avctx, &vcc->fmt);
    if (avctx->pix_fmt < 0)
        return avctx->pix_fmt;

    ff_set_dimensions(avctx, vcc->fmt.width, vcc->fmt.height);

    ff_get_buffer(avctx, avframe, 0);

    vcc->ostride[0] = avframe->linesize[0] / 2;
    vcc->ostride[1] = avframe->linesize[1] / 2;
    vcc->ostride[2] = avframe->linesize[2] / 2;

    res = vc2decode_decode_one_picture(vcc->handle,
                                       (char **) &dp, size,
                                       (uint16_t **) avframe->data,
                                       vcc->ostride, 0);

    switch (res) {
    case VC2DECODER_OK_EOS:
        return AVERROR_EOF;
    case VC2DECODER_OK_PICTURE:
        *got_frame = 1;
        return avpkt->size;
    case VC2DECODER_OK_RECONFIGURED:
        // vc2decode_synchronise would had swallowed it before.
    default:
        return AVERROR_BUG;
    }

    return avpkt->size;
}

static av_cold int vc2hqdecode_decode_close(AVCodecContext *avctx)
{
    VC2hqdecodeContext *s = avctx->priv_data;

    vc2decode_destroy(s->handle);
    return 0;
}

AVCodec ff_libvc2hqdecode_decoder = {
    .name           = "libvc2hqdecode",
    .long_name      = NULL_IF_CONFIG_SMALL("VC-2 (vc2hqdecode)"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_DIRAC,
    .priv_data_size = sizeof(VC2hqdecodeContext),
    .init           = vc2hqdecode_decode_init,
    .decode         = vc2hqdecode_decode_frame,
    .close          = vc2hqdecode_decode_close,
    .capabilities   = AV_CODEC_CAP_DELAY | AV_CODEC_CAP_DR1 | AV_CODEC_CAP_AUTO_THREADS,
    .caps_internal  = FF_CODEC_CAP_SETS_PKT_DTS | FF_CODEC_CAP_INIT_THREADSAFE |
                      FF_CODEC_CAP_INIT_CLEANUP,
};
