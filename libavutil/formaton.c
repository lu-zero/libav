/*
 * Copyright (c) 2015 Kostya Shishkov
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.GPLv3.  If not see
 * <http://www.gnu.org/licenses/>.
 */

#include <string.h>

#include "formaton.h"
#include "mem.h"
#include "pixdesc.h"
#include "pixfmt.h"

AVPixelFormaton *av_formaton_from_pixfmt(enum AVPixelFormat pix_fmt)
{
    AVPixelFormaton *formaton = av_mallocz(sizeof(AVPixelFormaton));
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(pix_fmt);
    int i;

    if (!formaton)
        return NULL;
    if (!desc)
        goto fail;

    formaton->name = strdup(desc->name);
    if (!formaton->name)
        goto fail;
    formaton->flags = desc->flags;

    // XXX luzero disapproves
    if (strchr(desc->name, 'j'))
        formaton->range = AVCOL_RANGE_JPEG;
    else
        formaton->range = AVCOL_RANGE_UNSPECIFIED;
    formaton->primaries = AVCOL_PRI_UNSPECIFIED;
    formaton->transfer  = AVCOL_TRC_UNSPECIFIED;
    formaton->space     = AVCOL_SPC_UNSPECIFIED;
    formaton->location  = AVCHROMA_LOC_UNSPECIFIED;

    formaton->nb_components = desc->nb_components;

    for (i = 0; i < formaton->nb_components; i++) {
        AVChromaton *chromaton = &formaton->component_desc[i];
        const AVComponentDescriptor *comp = &desc->comp[i];

        chromaton->plane = comp->plane;
        chromaton->step  = comp->step;
        chromaton->h_sub_log = desc->log2_chroma_w;
        chromaton->v_sub_log = desc->log2_chroma_h;
        chromaton->off = comp->offset;
        chromaton->shift = comp->shift;
        chromaton->depth = comp->depth;
        chromaton->packed = 0; // XXX luzero does not remember
        chromaton->next = 0; // XXX luzero does not remember
    }

    return formaton;

fail:
    av_formaton_free(&formaton);
    return NULL;
}

void av_formaton_free(AVPixelFormaton **formaton)
{
    if (!formaton || !*formaton)
        return;

    av_freep(&(*formaton)->name);
    av_freep(formaton);
}
