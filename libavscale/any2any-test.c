#include <string.h>
#include <stdio.h>

#include "libavutil/frame.h"
#include "libavutil/mem.h"

#include "avscale.h"

int main(int argc, char **argv)
{
    int w, h;
    FILE *in, *out;
    AVFrame *src = NULL, *dst = NULL;
    AVScaleContext *avsctx;
    int ret = AVERROR(ENOMEM);
    int i;
    int input_yuv, output_yuv;

    if (argc < 3) {
        printf("usage: %s infile.pnm outfile.{ppm,pgm}\n", argv[0]);
        return AVERROR(EINVAL);
    }
    in = fopen(argv[1], "rb");
    if (!in) {
        fprintf(stderr, "Cannot open input file\n");
        return AVERROR(ENOSYS);
    }
    out = fopen(argv[2], "wb");
    if (!out) {
        fprintf(stderr, "Cannot open output file\n");
        ret = AVERROR(ENOSYS);
        goto end;
    }
    input_yuv  = !!strstr(argv[1], ".pgm");
    output_yuv = !!strstr(argv[2], ".pgm");

    src = av_frame_alloc();
    dst = av_frame_alloc();
    if (!src || !dst)
        goto end;
    av_frame_unref(src);
    av_frame_unref(dst);

    if (input_yuv) {
        fscanf(in, "P5\n%d %d\n255\n", &w, &h);
        h = h - h / 3;
        src->format = AV_PIX_FMT_YUV420P;
    } else {
        fscanf(in, "P6\n%d %d\n255\n", &w, &h);
        src->format = AV_PIX_FMT_RGB24;
    }
    printf("converting %dx%d pic...\n", w, h);

    ret = av_frame_get_buffer(src, 1);
    if (ret < 0)
        goto end;

    fread(src->data[0], src->linesize[0], h, in);

    dst->width  = src->width;
    dst->height = src->height;
    if (output_yuv)
        dst->format = AV_PIX_FMT_YUV420P;
    else
        dst->format = AV_PIX_FMT_RGB24;
    ret = av_frame_get_buffer(dst, 1);
    if (ret < 0)
        goto end;

    avsctx = avscale_alloc_context();
    if (!avsctx)
        goto end;

    ret = avscale_process_frame(avsctx, src, dst);
    if (ret < 0) {
        printf("Failed\n");
        goto end;
    }

    w = dst->width;
    h = dst->height;
    printf("Success %dx%d\n", w, h);
    if (output_yuv) {
        fprintf(out, "P5\n%d %d\n255\n", w, h + h / 2);
        fwrite(dst->data[0], dst->linesize[0], h, out);
        for (i = 0; i < h / 2; i++) {
            fwrite(dst->data[1] + i * dst->linesize[1], w / 2, 1, out);
            fwrite(dst->data[2] + i * dst->linesize[2], w / 2, 1, out);
        }
    } else {
        fprintf(out, "P6\n%d %d\n255\n", w, h);
        fwrite(dst->data[0], dst->linesize[0], h, out);
    }

    ret = 0;

end:
    av_frame_free(&src);
    av_frame_free(&dst);

    avscale_free(&avsctx);
    if (in)
        fclose(in);
    if (out)
        fclose(out);

    return 0;
}
