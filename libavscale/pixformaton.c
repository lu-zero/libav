/*
 * Copyright (c) 2015 Kostya Shishkov
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

#include "libavutil/avstring.h"
#include "libavutil/buffer.h"
#include "libavutil/mem.h"
#include "libavutil/pixdesc.h"
#include "libavutil/pixfmt.h"

#include "pixformaton.h"

typedef struct AVPixelFormatonRefInternal {
    AVPixelFormaton *pf;
    AVBufferRef *ref;
} AVPixelFormatonRefInternal;

AVPixelFormatonRef *av_pixformaton_alloc(void)
{
    AVPixelFormaton *pf;
    AVBufferRef *buf = NULL;

    AVPixelFormatonRefInternal *pref = NULL;

    buf = av_buffer_allocz(sizeof(*pf));
    if (!buf)
        return NULL;

    pref = av_mallocz(sizeof(*pref));
    if (!pref)
        goto fail;

    pf = (AVPixelFormaton *)buf->data;

    pref->pf  = pf;
    pref->ref = buf;

    return (AVPixelFormatonRef*)pref;
fail:
    av_buffer_unref(&buf);
    av_freep(&pref);
    return NULL;
}

AVPixelFormatonRef *av_pixformaton_ref(AVPixelFormatonRef *pf)
{
    AVPixelFormatonRefInternal *src = (AVPixelFormatonRefInternal*)pf;
    AVPixelFormatonRefInternal *dst;

    dst = av_mallocz(sizeof(*dst));
    if (!dst)
        return NULL;

    dst->ref = av_buffer_ref(src->ref);
    if (!dst->ref) {
        av_freep(&dst);
        return NULL;
    }

    dst->pf = src->pf;

    return (AVPixelFormatonRef*)dst;
}

void av_pixformaton_unref(AVPixelFormatonRef **pref)
{
    AVPixelFormatonRefInternal *pref_int = (AVPixelFormatonRefInternal*)*pref;

    if (!pref_int)
        return;

    av_buffer_unref(&pref_int->ref);
    av_freep(&pref_int);
}

AVPixelFormatonRef *av_pixformaton_from_frame(const AVFrame *frame)
{
    AVPixelFormaton *pf;
    AVPixelFormatonRef *pref;
    const AVPixFmtDescriptor *desc;

    int i;

    desc = av_pix_fmt_desc_get(frame->format);
    if (!desc)
        return NULL;

    pref = av_pixformaton_alloc();
    if (!pref)
        return NULL;

    pf = pref->pf;

    if (desc->flags & AV_PIX_FMT_FLAG_BE)
        pf->flags |= AV_PIX_FORMATON_FLAG_BE;
    if (desc->flags & AV_PIX_FMT_FLAG_ALPHA)
        pf->flags |= AV_PIX_FORMATON_FLAG_ALPHA;
    if (desc->flags & AV_PIX_FMT_FLAG_PAL)
        pf->flags |= AV_PIX_FORMATON_FLAG_PALETTE;

    if (desc->flags & AV_PIX_FMT_FLAG_RGB)
        pf->model = AVCOL_MODEL_RGB;
    else
        pf->model = AVCOL_MODEL_YUV;

    pf->full_range = !!av_strstart(desc->name, "yuvj", NULL) ||
                     frame->color_range == AVCOL_RANGE_JPEG;

    //TODO do we really all color spaces in the world? we should offer
    // a list of available coefficients and convert back and forth
    pf->primaries = frame->color_primaries;
    pf->transfer  = frame->color_trc;
    pf->space     = frame->colorspace;
    pf->location  = frame->chroma_location;

    pf->nb_components = desc->nb_components;

    if (!(desc->flags & AV_PIX_FMT_FLAG_PLANAR))
        pf->pixel_size = (desc->comp[0].depth * desc->nb_components + 7) >> 3;

    for (i = 0; i < pf->nb_components; i++) {
        AVPixelChromaton *chromaton = &pf->component[i];
        const AVComponentDescriptor *comp = &desc->comp[i];

        chromaton->plane  = comp->plane;
        chromaton->next   = comp->step;
        chromaton->h_sub  = i > 0 && i < 3 ? desc->log2_chroma_w : 0;
        chromaton->v_sub  = i > 0 && i < 3 ? desc->log2_chroma_h : 0;
        chromaton->offset = comp->offset;
        chromaton->shift  = comp->shift;
        chromaton->depth  = comp->depth;
        chromaton->packed = !(desc->flags & AV_PIX_FMT_FLAG_PLANAR);
    }

    return pref;
}
