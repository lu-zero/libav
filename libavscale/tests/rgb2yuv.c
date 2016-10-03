#include <string.h>
#include <stdio.h>

#include "libavutil/frame.h"
#include "libavutil/mem.h"

#include "libavscale/avscale.h"

int main(int argc, char **argv)
{
    int w, h;
    FILE *in, *out;
    AVFrame *src = NULL, *dst = NULL;
    AVScaleContext *avsctx;
    int copy;
    int bpc = 3;
    int ret = AVERROR(ENOMEM);
    int i;

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
    copy = !!strstr(argv[2], ".ppm");

    fscanf(in, "P6\n%d %d\n255\n", &w, &h);
    printf("converting %dx%d pic... (copy %d)\n", w, h, copy);

    src = av_frame_alloc();
    dst = av_frame_alloc();
    if (!src || !dst)
        goto end;
    av_frame_unref(src);
    av_frame_unref(dst);

    src->width       = w;
    src->height      = h;
    src->linesize[0] = w * bpc;
    src->data[0]     = av_malloc(src->linesize[0] * h);
    if (!src->data[0])
        goto end;

    fread(src->data[0], src->linesize[0], h, in);

    dst->width  = w;
    dst->height = h;
    if (!copy && w > 550) {
        dst->width  = (dst->width / 3 + 3) & ~3;
        dst->height = (dst->height / 3 + 3) & ~3;
    }
    if (copy) {
        dst->linesize[0] = dst->width * bpc;
        dst->data[0] = av_malloc(dst->linesize[0] * dst->height);
        if (!dst->data[0])
            goto end;
        dst->data[1]     = dst->data[2] = 0;
    } else {
        dst->data[0]     = av_malloc(dst->width * dst->height);
        dst->linesize[0] = dst->width;
        dst->data[1]     = av_malloc(dst->width * dst->height / 4);
        dst->linesize[1] = dst->width / 2;
        dst->data[2]     = av_malloc(dst->width * dst->height / 4);
        dst->linesize[2] = dst->width / 2;
        if (!dst->data[0] || !dst->data[1] || !dst->data[2])
            goto end;
    }
    avsctx = avscale_alloc_context();
    if (!avsctx)
        goto end;

    ret = avscale_convert_frame(avsctx, dst, src);
    if (ret < 0) {
        printf("Failed\n");
        goto end;
    }

    w = dst->width;
    h = dst->height;
    printf("Success %dx%d\n", w, h);
    if (copy) {
        fprintf(out, "P6\n%d %d\n255\n", w, h);
        fwrite(dst->data[0], w * 3, h, out);
    } else {
        fprintf(out, "P5\n%d %d\n255\n", w, h + h / 2);
        fwrite(dst->data[0], w, h, out);
        for (i = 0; i < h / 2; i++) {
            fwrite(dst->data[1] + i * dst->linesize[1], w / 2, 1, out);
            fwrite(dst->data[2] + i * dst->linesize[2], w / 2, 1, out);
        }
    }

    ret = 0;

end:
    if (src)
        av_freep(&src->data[0]);
    if (dst)
        for (i = 0; i < 3; i++)
            av_freep(&dst->data[i]);
    av_frame_free(&src);
    av_frame_free(&dst);

    avscale_free(&avsctx);
    if (in)
        fclose(in);
    if (out)
        fclose(out);

    return 0;
}
