/*
 * H.265/MPEG-4 AVC encoder utilizing AMD's VCE video encoding ASIC
 *
 * Copyright (c) 2015 Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * This file is part of Libav.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * disclaimer below) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <inttypes.h>

#include "libavutil/common.h"
#include "libavutil/opt.h"
#include "libavutil/time.h"
#include "libavutil/thread.h"

#include "avcodec.h"
#include "internal.h"
#include "amf_capi.h"

typedef struct H265VCEContext {
    AVClass *class;
    amfContext *context;
    amfComponent *encoder;

    amf_int32 width;
    amf_int32 height;

    amf_int32 submitted;
    amf_int32 returned;

    int quality_preset;
} H265VCEContext;

static enum AMF_RESULT vce_encode_capi_ret = AMF_NOT_INITIALIZED;
static AVOnce vce_encode_init_once         = AV_ONCE_INIT;

static void vce_encode_static_init(void)
{
    vce_encode_capi_ret = amf_capi_init();
}

static int populate_extradata(AVCodecContext *avcontext, amfData *buffer)
{
    int i;
    unsigned char *data     = amfBufferGetNative(buffer);
    amf_size size           = amfBufferGetSize(buffer);
    unsigned char header[4] = { 0x00, 0x00, 0x00, 0x01 };
    int headerCount         = 0;
    int headerPositions[80];
    int ppsPos = -1;
    int spsPos = -1;
    int ppsLen = 0;
    int spsLen = 0;

    for (i = 0; i + 4 < size; ++i)
        if (data[i + 0] == header[0] && data[i + 1] == header[1] && data[i + 2] == header[2] && data[i + 3] == header[3]) {
            if ((data[i + 4] & 0x1f) == 7)
                spsPos = headerCount;
            if ((data[i + 4] & 0x1f) == 8)
                ppsPos = headerCount;
            headerPositions[headerCount] = i;
            ++headerCount;
        }
    headerPositions[headerCount] = size;

    if (spsPos >= 0)
        spsLen = headerPositions[spsPos + 1] - headerPositions[spsPos];
    if (ppsPos >= 0)
        ppsLen = headerPositions[ppsPos + 1] - headerPositions[ppsPos];

    if (spsLen + ppsLen > 0) {
        avcontext->extradata_size = spsLen + ppsLen;
        avcontext->extradata      = av_malloc(avcontext->extradata_size);
        memcpy(avcontext->extradata, &data[headerPositions[spsPos]], spsLen);
        memcpy(avcontext->extradata + spsLen, &data[headerPositions[ppsPos]], ppsLen);
    }
    return 0;
}

static int vce_encode_init(AVCodecContext *avcontext)
{
    H265VCEContext *d = (H265VCEContext *)(avcontext->priv_data);
    enum AMF_RESULT result;

    ff_thread_once(&vce_encode_init_once, vce_encode_static_init);

    // Check encoding requirements

    if (vce_encode_capi_ret != AMF_OK) {
        av_log(avcontext, AV_LOG_ERROR, "Cannot encode without AMF library\n");
        return -1;
    }

    if (avcontext->pix_fmt != AV_PIX_FMT_YUV420P) {
        av_log(avcontext, AV_LOG_ERROR, "Only YUV420 supported\n");
        return -1;
    }

    result = amfCreateContext(&d->context);
    if (result != AMF_OK) {
        av_log(avcontext, AV_LOG_ERROR, "Failed to create AMFContext\n");
        return -1;
    }

    result = amfCreateComponent(d->context, AMFVideoEncoder_HEVC, &d->encoder);
    if (result != AMF_OK) {
        av_log(avcontext, AV_LOG_ERROR, "Failed to create AMF VCE Encoder\n");
        return -1;
    }

    {
        amf_int32 widthIn       = avcontext->width;
        amf_int32 heightIn      = avcontext->height;
        amf_int64 bitRateIn     = avcontext->bit_rate;
        amf_int64 bFramePattern = avcontext->max_b_frames;
        amf_int32 usage         = AMF_VIDEO_ENCODER_HEVC_USAGE_TRANSCONDING;

        struct AMFSize frameSize                           = { widthIn, heightIn };
        struct AMFRate frameRate                           = { avcontext->time_base.den, avcontext->time_base.num };

        bFramePattern           = (bFramePattern < 0) ? 0 : bFramePattern;
        bFramePattern           = (bFramePattern > 3) ? 3 : bFramePattern;
        avcontext->has_b_frames = (bFramePattern > 0);

        result = amfSetPropertyInt64(d->encoder, AMF_VIDEO_ENCODER_HEVC_USAGE, usage);
        result = amfSetPropertyInt64(d->encoder, AMF_VIDEO_ENCODER_HEVC_TARGET_BITRATE, bitRateIn);
        result = amfSetPropertySize(d->encoder, AMF_VIDEO_ENCODER_HEVC_FRAMESIZE, &frameSize);
        result = amfSetPropertyRate(d->encoder, AMF_VIDEO_ENCODER_HEVC_FRAMERATE, &frameRate);
//        result = amfSetPropertyInt64(d->encoder, AMF_VIDEO_ENCODER_B_PIC_PATTERN, bFramePattern);
        result = amfSetPropertyInt64(d->encoder, AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET, (enum AMF_VIDEO_ENCODER_QUALITY_PRESET_ENUM) d->quality_preset);
//        result = amfSetPropertyBool(d->encoder, AMF_VIDEO_ENCODER_ENFORCE_HRD, 0);
//        result = amfSetPropertyBool(d->encoder, AMF_VIDEO_ENCODER_FILLER_DATA_ENABLE, 0);

//        if (avcontext->flags & CODEC_FLAG_QSCALE) {
//            amf_int64 quality = (amf_int64)avcontext->global_quality / FF_QP2LAMBDA;
//            quality = (quality < 0) ? 0 : quality;
//            quality = (quality > 51) ? 51 : quality;
//            result  = amfSetPropertyInt64(d->encoder, AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD, AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_CONSTRAINED_QP);
//            result  = amfSetPropertyInt64(d->encoder, AMF_VIDEO_ENCODER_QP_I, quality);
//            result  = amfSetPropertyInt64(d->encoder, AMF_VIDEO_ENCODER_QP_P, quality);
//            result  = amfSetPropertyInt64(d->encoder, AMF_VIDEO_ENCODER_QP_B, quality);
//        } else {
            result = amfSetPropertyInt64(d->encoder, AMF_VIDEO_ENCODER_HEVC_RATE_CONTROL_METHOD, AMF_VIDEO_ENCODER_HEVC_RATE_CONTROL_METHOD_PEAK_CONSTRAINED_VBR);
//        }

        switch (avcontext->profile) {
            case FF_PROFILE_HEVC_MAIN:
            default: // FIXME
                result = amfSetPropertyInt64(d->encoder, AMF_VIDEO_ENCODER_HEVC_PROFILE, AMF_VIDEO_ENCODER_HEVC_PROFILE_MAIN);
            break;
            //default:
                printf("\t****************** profile not set to FF_PROFILE_HEVC_MAIN\n");
            break;
        }

        if (avcontext->level != FF_LEVEL_UNKNOWN) {
            result = amfSetPropertyInt64(d->encoder, AMF_VIDEO_ENCODER_HEVC_PROFILE_LEVEL, (amf_int64)avcontext->level);
        }

        result = amfComponentInit(d->encoder, AMF_SURFACE_NV12, widthIn, heightIn);

        if (result != AMF_OK) {
            av_log(avcontext, AV_LOG_ERROR, "Failed to initialize AMF VCE Encoder\n");
            return -1;
        }
    }

    d->width     = avcontext->width;
    d->height    = avcontext->height;
    d->submitted = 0;
    d->returned  = 0;

    {
        // Pull SPS/PPS and put it in extradata
        amfData *data;
        result = amfComponentGetExtraData(d->encoder, &data);
        if (result == AMF_OK) {
            populate_extradata(avcontext, data);
            amfReleaseData(data);
        }
    }

    return 0;
}

static int vce_encode_close(AVCodecContext *avcontext)
{
    H265VCEContext *d = (H265VCEContext *)(avcontext->priv_data);

    amfComponentTerminate(d->encoder);
    amfContextTerminate(d->context);

    return 0;
}

static int trim_headers(unsigned char *data, int size)
{
    if (*((unsigned*)data) == 0x01000000 && data[4] == 0x09)
        return 6;
    return 0;
}

static int vce_encode_frame(AVCodecContext *avcontext, AVPacket *packet, const AVFrame *frame, int *got_packet_ptr)
{
    enum AMF_RESULT result;
    H265VCEContext *d   = (H265VCEContext *)(avcontext->priv_data);
    amfSurface *surface = NULL;
    amfData *out        = NULL;
    int done            = 0;
    int submitDone      = 0;

    if (frame == NULL) {
        result = amfComponentDrain(d->encoder);
    }

    while (done == 0) {
        // If there is a frame get it ready and try to submit
        if ((frame != NULL) && (submitDone == 0)) {
            if (surface == NULL) {
                unsigned char *rasters[3];
                amf_int32 strides[3];
                result = amfAllocSurface(d->context, AMF_MEMORY_DX9, AMF_SURFACE_NV12, d->width, d->height, &surface);
                amfDataSetPts(surface, frame->pts);

                // copy in pixel data
                rasters[0] = frame->data[0];
                rasters[1] = frame->data[1];
                rasters[2] = frame->data[2];
                strides[0] = frame->linesize[0];
                strides[1] = frame->linesize[1];
                strides[2] = frame->linesize[2];

                amfCopyYUV420HostToNV12DX9((unsigned char **)&rasters, (amf_int32 *)&strides, surface);
            }

            result = amfComponentSubmitInput(d->encoder, surface);
            if (result == AMF_OK) {
                amfReleaseSurface(surface);
                surface = NULL;
                d->submitted++;
                submitDone = 1;
                done       = 1;
            } else if (result != AMF_INPUT_FULL)
                av_log(avcontext, AV_LOG_ERROR, "amfComponentSubmitInput: unexpected return code\n");
        }

        // Collect and merge output

        if (out == NULL) {
            result = amfComponentQueryOutput(d->encoder, &out);
            if (result == AMF_OK) {
                d->returned++;
                if ((frame == NULL) || (submitDone == 1))
                    done = 1;
            }
            if ((frame == NULL) && (d->returned == d->submitted))    // All data flushed - no point waiting for output
                done = 1;
        }
        if (done == 0)
            av_usleep(1000);    // Poll
    }

    // Package the output packet

    if (out == NULL) {
        *got_packet_ptr = 0;
    } else {
        int size             = amfBufferGetSize(out);
        unsigned char *datap = amfBufferGetNative(out);

        int offset = trim_headers(datap, size);
        size  -= offset;
        datap += offset;

        if (packet->data != NULL) {
            if (packet->size < size) {
                av_log(avcontext, AV_LOG_ERROR, "User provided packet is too small\n");
                return -1;
            }
        } else {
            // Allocate a buffer, none was provided
            packet->buf  = av_buffer_alloc(size);
            packet->data = packet->buf->data;
        }
        packet->size    = size;
        packet->pts     = amfDataGetPts(out);
        *got_packet_ptr = 1;

        memcpy(packet->data, datap, size);

        // Set the Frame Type
        {
            amf_int64 frameType = 0;
            result = amfGetPropertyInt64(out, AMF_VIDEO_ENCODER_HEVC_OUTPUT_DATA_TYPE, &frameType);
            if (result != AMF_OK)
                av_log(avcontext, AV_LOG_ERROR, "Unable to set frame type\n");

            packet->flags &= ~AV_PKT_FLAG_KEY;

            switch ((enum AMF_VIDEO_ENCODER_HEVC_OUTPUT_DATA_TYPE_ENUM)frameType) {
            case AMF_VIDEO_ENCODER_HEVC_OUTPUT_DATA_TYPE_I:
                packet->flags                    |= AV_PKT_FLAG_KEY;
            default:
                break;
            }
        }
        amfReleaseData(out);
    }
    return 0;
}

static const AVCodecDefault vce_h265_defaults[] = {
    { "b",                "0" },
    { "bf",               "3" },
    { "g",                "-1" },
};

static const AVOption vce_h265_options[] = {
    { "preset", "Use encoder preset (balanced, speed, quality)"
        , offsetof(H265VCEContext, quality_preset), AV_OPT_TYPE_INT
        , { .i64 = AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_BALANCED }, 0 , 0
        , AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_ENCODING_PARAM, "preset" },
    { "balanced", NULL, 0, AV_OPT_TYPE_CONST
        , { .i64 = AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_BALANCED }, 0, 0
        , AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_ENCODING_PARAM, "preset" },
    { "speed", NULL, 0, AV_OPT_TYPE_CONST
        , { .i64 = AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_SPEED }, 0, 0
        , AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_ENCODING_PARAM, "preset" },
    { "quality", NULL, 0, AV_OPT_TYPE_CONST
        , { .i64 = AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_QUALITY }, 0, 0
        , AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_ENCODING_PARAM, "preset" },
    { NULL },
};

static const AVClass vce_h265_class = {
    .class_name = "vce_h265",
    .item_name = av_default_item_name,
    .option = vce_h265_options,
    .version = LIBAVUTIL_VERSION_INT,
};

AVCodec ff_h265_vce_encoder = {
    .name           = "h265_vce",
    .long_name      = NULL_IF_CONFIG_SMALL("H.265 / HEVC / MPEG-H Part 2 (AMD VCE)"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_HEVC,
    .priv_data_size = sizeof(H265VCEContext),
    .init           = vce_encode_init,
    .close          = vce_encode_close,
    .encode2        = vce_encode_frame,
    .pix_fmts       = (const enum AVPixelFormat[]) { AV_PIX_FMT_YUV420P,                          AV_PIX_FMT_NONE},
    .defaults       = vce_h265_defaults,
    .capabilities   = AV_CODEC_CAP_DELAY,
    .priv_class     = &vce_h265_class,
};
