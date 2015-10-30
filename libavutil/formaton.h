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

#ifndef AVUTIL_FORMATON_H
#define AVUTIL_FORMATON_H

#include <stdint.h>

#include "pixdesc.h"

typedef struct AVChromaton {
    int plane;

    /**
     * might serve useful for packed formats - e.g. 4 or 2 bytes per entry
     */
    int step;

    /**
     * subsampling information
     */
    int h_sub_log, v_sub_log;

    /**
     * offset to the starting element, e.g. 0 for Y, 1 for U and 3 for V in YUYV
     */
    int off;

    /**
     * component shift for packed, e.g. for RGB565 it will be 11,5,0
     */
    int shift;

    /**
     * bits per component, e.g. for packed RGB565 you'll have 5,6,5
     */
    int depth;

    /**
     * if component is packed with others (e.g. RGB24 - 1,1,1, NV12 - 0,1,1)
     */
    int packed;

    /**
     * offset to the next element - e.g. 2 for Y and 4 for U and V in YUYV
     */
    int next;
} AVChromaton;

typedef struct AVPixelFormaton {
    const char *name;

    unsigned flags; // has alpha, uses BE order, uses palette etc

    enum AVColorRange range;
    enum AVColorPrimaries primaries;
    enum AVColorTransferCharacteristic transfer;
    enum AVColorSpace space;
    enum AVChromaLocation location;

    int nb_components;
#define AVSCALE_MAX_COMPONENTS 5
    AVChromaton component_desc[AVSCALE_MAX_COMPONENTS];
} AVPixelFormaton;

AVPixelFormaton *av_formaton_from_pixfmt(enum AVPixelFormat pix_fmt);
void av_formaton_free(AVPixelFormaton **formaton);


#endif /* AVUTIL_FORMATON_H */
