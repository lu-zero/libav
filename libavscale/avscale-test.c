#include <string.h>
#include <stdio.h>

#include "libavutil/frame.h"
#include "libavutil/mem.h"

#include "avscale.h"

int main(int argc, char **argv)
{
    int w, h;
    FILE *in, *out;
    //FIXME
    AVFrame *src, *dst;
    AVScaleContext *avsctx;
    int copy;
    int bpc = 3;
    int ret;
    int i;

    if (argc < 3) {
        printf("usage: %s infile.pnm outfile.{ppm,pgm}\n", argv[0]);
        return 0;
    }
    in  = fopen(argv[1], "rb");
    out = fopen(argv[2], "wb");
    copy = !!strstr(argv[2], ".ppm");

    fscanf(in, "P6\n%d %d\n255\n", &w, &h);
    printf("converting %dx%d pic...\n", w, h);

    src = av_frame_alloc();
    dst = av_frame_alloc();
    if (!src || !dst) {
        av_frame_free(&src);
        av_frame_free(&dst);
        return 1;
    }

    src->width = w;
    src->height = h;
    src->linesize[0] = w * bpc;
    src->formaton = av_pixformaton_from_pixfmt(AV_PIX_FMT_RGB24);
    src->data[0] = av_malloc(src->linesize[0] * h);

    fread(src->data[0], src->linesize[0], h, in);

    dst->width = w;
    dst->height = h;
    if (!copy && w > 550) {
        dst->width  = (dst->width  / 3 + 3) & ~3;
        dst->height = (dst->height / 3 + 3) & ~3;
    }
    if (copy) {
        dst->data[0] = av_malloc(dst->width * dst->height * 3);
        dst->linesize[0] = dst->width * 3;
        dst->formaton =  av_pixformaton_from_pixfmt(AV_PIX_FMT_RGB24);
        dst->data[1] = dst->data[2] = 0;
    } else {
        dst->data[0] = av_malloc(dst->width * dst->height);
        dst->linesize[0] = dst->width;
        dst->data[1] = av_malloc(dst->width * dst->height / 4);
        dst->linesize[1] = dst->width / 2;
        dst->data[2] = av_malloc(dst->width * dst->height / 4);
        dst->linesize[2] = dst->width / 2;
        dst->formaton =  av_pixformaton_from_pixfmt(AV_PIX_FMT_YUV420P);
    }
    avsctx = avscale_alloc_context();
    if (!avsctx)
        return AVERROR(ENOMEM);

    ret = avscale_process_frame(avsctx, src, dst);
    printf(ret ? "Failed\n" : "Succeeded\n");
    w = dst->width;
    h = dst->height;
    if (copy) {
        fprintf(out, "P6\n%d %d\n255\n",w,h);
        fwrite(dst->data[0], w * 3, h, out);
    } else {
        fprintf(out, "P5\n%d %d\n255\n",w,h+h/2);
        fwrite(dst->data[0], w, h, out);
        for (i = 0; i < h / 2; i++) {
            fwrite(dst->data[1] + i * dst->linesize[1], w/2, 1, out);
            fwrite(dst->data[2] + i * dst->linesize[2], w/2, 1, out);
        }
    }
    av_free(src->data[0]);
    for (i = 0; i < 3; i++)
        av_free(dst->data[i]);
    fclose(in);
    fclose(out);
    avscale_free(&avsctx);

    return 0;
}
