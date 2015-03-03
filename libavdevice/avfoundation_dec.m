/*
 * AVFoundation input device
 * Copyright (c) 2015 Luca Barbato
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

#import <AVFoundation/AVFoundation.h>

#include "libavformat/avformat.h"
#include "libavutil/log.h"
#include "libavutil/opt.h"

#include "avdevice.h"


typedef struct AVFoundationCaptureContext {
    AVClass *class;
    int list_devices;
} AVFoundationCaptureContext;

#define AUDIO_DEVICES 1
#define VIDEO_DEVICES 2
#define ALL_DEVICES   AUDIO_DEVICES|VIDEO_DEVICES

#define OFFSET(x) offsetof(AVFoundationCaptureContext, x)
#define DEC AV_OPT_FLAG_DECODING_PARAM
static const AVOption options[] = {
    { "list_devices", "List available devices and exit", OFFSET(list_devices),  AV_OPT_TYPE_INT,    {.i64 = 0 },             0, INT_MAX, DEC, "list_devices" },
    { "all",          "Show all the supported devices",  OFFSET(list_devices),  AV_OPT_TYPE_CONST,  {.i64 = ALL_DEVICES },   0, INT_MAX, DEC, "list_devices" },
    { "audio",        "Show only the audio devices",     OFFSET(list_devices),  AV_OPT_TYPE_CONST,  {.i64 = AUDIO_DEVICES }, 0, INT_MAX, DEC, "list_devices" },
    { "video",        "Show only the video devices",     OFFSET(list_devices),  AV_OPT_TYPE_CONST,  {.i64 = VIDEO_DEVICES }, 0, INT_MAX, DEC, "list_devices" },
    { NULL },
};


static void list_capture_devices_by_type(AVFormatContext *s, NSString *type)
{
    NSArray *devices = [AVCaptureDevice devicesWithMediaType:type];

    av_log(s, AV_LOG_INFO, "Type: %s\n", [type UTF8String]);
    for (AVCaptureDevice *device in devices) {

        av_log(s, AV_LOG_INFO, "uniqueID: %s\nname: %s\nformat:\n",
               [[device uniqueID] UTF8String],
               [[device localizedName] UTF8String]);

        for (AVCaptureDeviceFormat *format in device.formats)
            av_log(s, AV_LOG_INFO, "\t%s\n",
                   [[NSString stringWithFormat:@"%@", format] UTF8String]);
    }
}

static int avfoundation_list_capture_devices(AVFormatContext *s)
{
    AVFoundationCaptureContext *ctx = s->priv_data;

    if (ctx->list_devices & AUDIO_DEVICES)
        list_capture_devices_by_type(s, AVMediaTypeAudio);

    if (ctx->list_devices & VIDEO_DEVICES)
        list_capture_devices_by_type(s, AVMediaTypeVideo);

    return AVERROR_EXIT;
}

const NSString *pat = @"(\\[[^\\]]+\\])";

static int setup_stream(AVFormatContext *s, NSString *uniqueID)
{
    // add the input devices

    // add the output devices

    return 0;
}

static int setup_streams(AVFormatContext *s)
{
    int i, ret;
    NSError *error = nil;
    NSArray *matches;
    NSString *parse_string;
    NSRegularExpression *exp;

    if (s->filename[0] != '[')
        return AVERROR(EINVAL);

    exp = [NSRegularExpression regularExpressionWithPattern:pat
                                                    options:0
                                                      error:&error];
    if (!exp) {
        av_log(s, AV_LOG_ERROR, "%s\n",
               [[error localizedDescription] UTF8String]);
        return AVERROR(ENOMEM);
    }

    parse_string = [NSString stringWithFormat:@"%s", s->filename];

    matches = [exp matchesInString:parse_string options:0
                             range:NSMakeRange(0, [test length])];

    if (matches) {
        // alloc the capture

    }

    for (NSTextCheckingResult *match in matches) {
        NSRange range = [match rangeAtIndex:1];
        NSLog(@"match 1: %@", [parse_string substringWithRange:range]);
        ret = setup_stream(s, [parse_string substringWithRange:range]);
        if (ret < 0) {
            // avfoundation_close
            return ret;
        }
    }

    return 0;
}


static int avfoundation_read_header(AVFormatContext *s)
{
    if (ctx->list_devices)
        return avfoundation_list_capture_devices(s);

    return setup_streams(s);
}

static const AVClass avfoundation_class = {
    .class_name = "AVFoundation AVCaptureDevice indev",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVInputFormat ff_avfoundation_demuxer = {
    .name           = "avfoundation",
    .long_name      = NULL_IF_CONFIG_SMALL("AVFoundation AVCaptureDevice grab"),
    .priv_data_size = sizeof(AVFoundationCaptureContext),
    .read_header    = avfoundation_read_header,
//    .read_packet    = avfoundation_read_packet,
//    .read_close     = avfoundation_read_close,
    .flags          = AVFMT_NOFILE,
    .priv_class     = &avfoundation_class,
};
