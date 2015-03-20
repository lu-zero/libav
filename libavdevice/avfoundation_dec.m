/*
 * AVFoundation input device
 * Copyright (c) 2015 Luca Barbato
 *                    Alexandre Lision
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
#include <pthread.h>

#include "libavformat/avformat.h"
#include "libavutil/log.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavformat/internal.h"
#include "libavutil/time.h"
#include "libavutil/mathematics.h"

#include "avdevice.h"

typedef struct AVFoundationCaptureContext {
    AVClass         *class;
    int             list_devices;
    CFTypeRef       session;        /** AVCaptureSession*/
    char*           video_size;     /**< String describing video size,
                                        set by a private option. */
    char*           pixel_format;   /**< Set by a private option. */
    int             list_format;    /**< Set by a private option. */
    char*           framerate;      /**< Set by a private option. */

    int             video_stream_index;

    int64_t         first_pts;
    int             frames_captured;
    int             audio_frames_captured;
    pthread_mutex_t frame_lock;
    pthread_cond_t  frame_wait_cond;

    CFTypeRef           avf_delegate;   /** AVFFrameReceiver */
    CFTypeRef           video_output;   /** AVCaptureVideoDataOutput */
    CVImageBufferRef    current_frame;  /** CVImageBufferRef */

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

static void lock_frames(AVFoundationCaptureContext* ctx)
{
    pthread_mutex_lock(&ctx->frame_lock);
}

static void unlock_frames(AVFoundationCaptureContext* ctx)
{
    pthread_mutex_unlock(&ctx->frame_lock);
}

/** FrameReceiver class - delegate for AVCaptureSession
 */
@interface AVFFrameReceiver : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
{
    AVFoundationCaptureContext* _context;
}

- (id)initWithContext:(AVFoundationCaptureContext*)context;

- (void)  captureOutput:(AVCaptureOutput *)captureOutput
  didOutputSampleBuffer:(CMSampleBufferRef)videoFrame
         fromConnection:(AVCaptureConnection *)connection;

@end

@implementation AVFFrameReceiver

- (id)initWithContext:(AVFoundationCaptureContext*)context
{
    if (self = [super init]) {
        _context = context;
    }
    return self;
}

- (void)  captureOutput:(AVCaptureOutput *)captureOutput
  didOutputSampleBuffer:(CMSampleBufferRef)videoFrame
         fromConnection:(AVCaptureConnection *)connection
{
    lock_frames(_context);

    if (_context->current_frame != nil) {
        CFRelease(_context->current_frame);
    }

    _context->current_frame = CFRetain(CMSampleBufferGetImageBuffer(videoFrame));

    pthread_cond_signal(&_context->frame_wait_cond);

    unlock_frames(_context);

    ++_context->frames_captured;
}

@end

NSString *pat = @"(\b[A-Z0-9]+\b)";

static int setup_stream(AVFormatContext *s, AVCaptureDevice *device)
{
    NSLog(@"setting up stream for device %@ ID\n", [device uniqueID]);

    AVFoundationCaptureContext *ctx = s->priv_data;
    NSError *__autoreleasing error = nil;
    AVCaptureDeviceInput *input;
    AVCaptureSession *session = (__bridge AVCaptureSession*)ctx->session;
    input = [AVCaptureDeviceInput deviceInputWithDevice:device
                                                  error:&error];
    // add the input devices
    if (!input) {
        av_log(s, AV_LOG_ERROR, "%s\n",
               [[error localizedDescription] UTF8String]);
        return AVERROR_UNKNOWN;
    }

    if ([session canAddInput:input]) {
        [session addInput:input];
    } else {
        av_log(s, AV_LOG_ERROR, "can't add video input to capture session\n");
        return -1;
    }

    // add the output devices
    if ([device hasMediaType:AVMediaTypeVideo]) {

        AVCaptureVideoDataOutput* out = [[AVCaptureVideoDataOutput alloc] init];
        if (!out) {
            av_log(s, AV_LOG_ERROR, "Failed to init AV video output\n");
            return -1;
        }

        [out setAlwaysDiscardsLateVideoFrames:YES];
        out.videoSettings = @{(id)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA)};

        //[out setVideoSettings:nil];

        AVFFrameReceiver* delegate = [[AVFFrameReceiver alloc] initWithContext:ctx];

        dispatch_queue_t queue = dispatch_queue_create("avf_queue", NULL);
        [out setSampleBufferDelegate:delegate queue:queue];

        ctx->avf_delegate = (__bridge_retained CFTypeRef) delegate;

        if ([session canAddOutput:out]) {
            [session addOutput:out];
            ctx->video_output = (__bridge_retained CFTypeRef) out;
        } else {
            av_log(s, AV_LOG_ERROR, "can't add video output to capture session\n");
            return -1;
        }
        NSLog(@"%@", device);
    }

/**    if ([device hasMediaType:AVMediaTypeAudio]) {
        AVCaptureAudioDataOutput *out =
            [[AVCaptureAudioDataOutput alloc] init];

        out.audioSettings = nil;
        [session addOutput:out];

        NSLog(@"%@ %@", device, out.audioSettings);
    }
*/
    return 0;
}

static int get_video_config(AVFormatContext *s)
{
    AVFoundationCaptureContext *ctx = (AVFoundationCaptureContext*)s->priv_data;
    CVImageBufferRef image_buffer;
    CGSize image_buffer_size;
    AVStream* stream = avformat_new_stream(s, NULL);

    if (!stream) {
        av_log(s, AV_LOG_ERROR, "Failed to create AVStream\n");
        return -1;
    }

    // Take stream info from the first frame.
    while (ctx->frames_captured < 1) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, YES);
    }

    lock_frames(ctx);

    ctx->video_stream_index = stream->index;

    avpriv_set_pts_info(stream, 64, 1, 1000000);

    image_buffer = ctx->current_frame;
    image_buffer_size = CVImageBufferGetEncodedSize(image_buffer);

    av_log(s, AV_LOG_ERROR, "Update stream info...\n");
    stream->codec->codec_id   = AV_CODEC_ID_RAWVIDEO;
    stream->codec->codec_type = AVMEDIA_TYPE_VIDEO;
    stream->codec->width      = (int)image_buffer_size.width;
    stream->codec->height     = (int)image_buffer_size.height;
    // No support for pixel formats for now using default one
    stream->codec->pix_fmt    = AV_PIX_FMT_BGR32;

    CFRelease(ctx->current_frame);
    ctx->current_frame = nil;

    unlock_frames(ctx);

    return 0;
}

static void destroy_context(AVFoundationCaptureContext* ctx)
{
    NSLog(@"Destroying context");

    AVCaptureSession *session = (__bridge AVCaptureSession*)ctx->session;
    [session stopRunning];

    ctx->session = NULL;


    pthread_mutex_destroy(&ctx->frame_lock);
    pthread_cond_destroy(&ctx->frame_wait_cond);

    if (ctx->current_frame) {
        CFRelease(ctx->current_frame);
    }
}

static int setup_default_stream(AVFormatContext *s)
{
    AVCaptureDevice *device;
    for (NSString *type in @[AVMediaTypeVideo]) {
        device = [AVCaptureDevice defaultDeviceWithMediaType:type];
        if (device)
            return setup_stream(s, device);
    }

    return -1;
}

static int setup_streams(AVFormatContext *s)
{
    NSLog(@"setting streams");
    AVFoundationCaptureContext *ctx = s->priv_data;
    int ret;
    NSError *__autoreleasing error = nil;
    NSArray *matches;
    NSString *filename;
    AVCaptureDevice *device;
    NSRegularExpression *exp;

    pthread_mutex_init(&ctx->frame_lock, NULL);
    pthread_cond_init(&ctx->frame_wait_cond, NULL);

    ctx->session = (__bridge_retained CFTypeRef)[[AVCaptureSession alloc] init];

    if (s->filename[0] != '[') {
        ret = setup_default_stream(s);
    } else {
        exp = [NSRegularExpression regularExpressionWithPattern:pat
                                                        options:0
                                                          error:&error];
        if (!exp) {
            av_log(s, AV_LOG_ERROR, "%s\n",
                   [[error localizedDescription] UTF8String]);
            return AVERROR(ENOMEM);
        }

        filename = [NSString stringWithFormat:@"%s", s->filename];
        av_log(s, AV_LOG_INFO, "device name: %s\n",[filename UTF8String]);

        matches = [exp matchesInString:filename options:0
                                 range:NSMakeRange(0, [filename length])];

        if (matches.count > 0) {
            for (NSTextCheckingResult *match in matches) {
                NSRange range = [match rangeAtIndex:0];
                NSString *uniqueID = [filename substringWithRange:range];
                av_log(s, AV_LOG_INFO, "opening device with ID: %s\n",[uniqueID UTF8String]);
                if (!(device = [AVCaptureDevice deviceWithUniqueID:uniqueID])) {
                    // report error
                    av_log(s, AV_LOG_ERROR, "Device with name %s not found",[filename UTF8String]);
                    return AVERROR(EINVAL);
                }
                ret = setup_stream(s, device);
                if (ret < 0) {
                    // avfoundation_close
                    return ret;
                }
            }
        } else {
            av_log(s, AV_LOG_ERROR, "No matches for %s",[filename UTF8String]);
            ret = setup_default_stream(s);
        }
    }

    if (ret < 0) {
        av_log(s, AV_LOG_ERROR, "No device could be added");
        return ret;
    }

    av_log(s, AV_LOG_INFO, "Starting session!\n");
    [(__bridge AVCaptureSession*)ctx->session startRunning];

    av_log(s, AV_LOG_INFO, "Checking video config\n");
    if (get_video_config(s)) {
        destroy_context(ctx);
        return AVERROR(EIO);
    }

    return 0;
}


static int avfoundation_read_header(AVFormatContext *s)
{
    AVFoundationCaptureContext *ctx = s->priv_data;
    ctx->first_pts = av_gettime();
    if (ctx->list_devices)
        return avfoundation_list_capture_devices(s);

    return setup_streams(s);
}

static int avfoundation_read_packet(AVFormatContext *s, AVPacket *pkt)
{
    AVFoundationCaptureContext* ctx = (AVFoundationCaptureContext*)s->priv_data;

    do {
        lock_frames(ctx);

        if (ctx->current_frame != nil) {
            if (av_new_packet(pkt, (int)CVPixelBufferGetDataSize(ctx->current_frame)) < 0) {
                return AVERROR(EIO);
            }

            pkt->pts = pkt->dts = av_rescale_q(av_gettime() - ctx->first_pts,
                                               AV_TIME_BASE_Q,
                                               (AVRational){1, 1000000});
            pkt->stream_index  = ctx->video_stream_index;
            pkt->flags        |= AV_PKT_FLAG_KEY;

            CVPixelBufferLockBaseAddress(ctx->current_frame, 0);

            void* data = CVPixelBufferGetBaseAddress(ctx->current_frame);
            memcpy(pkt->data, data, pkt->size);

            CVPixelBufferUnlockBaseAddress(ctx->current_frame, 0);
            CFRelease(ctx->current_frame);
            ctx->current_frame = nil;
        } else {
            pkt->data = NULL;
            pthread_cond_wait(&ctx->frame_wait_cond, &ctx->frame_lock);
        }

        unlock_frames(ctx);
    } while (!pkt->data);

    return 0;
}

static int avfoundation_read_close(AVFormatContext *s)
{
    NSLog(@"Closing session...");
    AVFoundationCaptureContext *ctx = s->priv_data;
    destroy_context(ctx);
    return 0;
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
    .read_packet    = avfoundation_read_packet,
    .read_close     = avfoundation_read_close,
    .flags          = AVFMT_NOFILE,
    .priv_class     = &avfoundation_class,
};
