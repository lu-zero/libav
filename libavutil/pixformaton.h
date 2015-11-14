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

#ifndef AVUTIL_PIXFORMATON_H
#define AVUTIL_PIXFORMATON_H

#include "pixdesc.h"

/**
 * The format is big-endian
 */
#define AV_PIX_FORMATON_FLAG_BE     (1 << 0)
/**
 * There is an alpha component
 *
 * @note: The alpha component is always the last component
 */
#define AV_PIX_FORMATON_FLAG_ALPHA  (1 << 1)
/**
 * The format is palettized
 *
 * The components describe the palette.
 */
#define AV_PIX_FORMATON_FLAG_PAL    (1 << 2)

/**
 * Component description
 *
 * The structure describes a single component in a pixel format.
 */
typedef struct AVPixelChromaton {
    /**
     * Index of the plane in which the component is located.
     */
    int plane;

    /**
     * The base-2 logarithm of the ratio between the component with the highest
     * resolution and this component.
     */
    int h_sub_log, v_sub_log;

    /**
     * Bit offset
     *
     * It is set if the component is not byte-aligned.
     *
     * The distance in bits from the start of the pixel.
     *
     * examples:
     *
     * For RGB565 it will be 11,5,0
     */
    int shift;

    /**
     * Component size in bits
     *
     * examples:
     *
     * For packed RGB565 you'll have 5,6,5
     */
    int depth;

    /**
     * Byte offset to the starting element
     *
     * It is set if the component is byte-aligned.
     *
     * The distance in bytes from the start of this plane to the
     * first element of this component.
     *
     * examples:
     *
     * For YUYV: 0 for Y, 1 for U and 3 for V.
     *
     */
    int offset;

    /**
     * Byte offset to the next element
     *
     * It is set if the component is byte-aligned.
     *
     * examples:
     *
     * For YUYV: 2 for Y and 4 for U and V
     */
    int next;

    /**
     * Set if the component shares the plane with another
     *
     * examples:
     *
     * For RGB24 - 1,1,1
     * For NV12 - 0,1,1
     */
    int packed;
} AVPixelChromaton;


/**
 * Pixel format description
 *
 * The structure describes the pixel format as whole.
 *
 * It expects at many as AV_PIX_FORMATON_COMPONENTS components.
 */
typedef struct AVPixelFormaton {
    /**
     * Defines how to interpret the components
     *
     * The components in the formaton are enumerated in fixed order
     * depending on the model.
     *
     * examples:
     *
     * For YUYV it is AV_COL_MODEL_YUV and the first component is
     * Y, the second U and the third V.
     *
     * For BGRA it is AV_COL_MODEL_RGB and the first component is
     * R, the second G, the third B and the last Alpha.
     */
    enum AVColorModel model;

    /**
     * Or-ed AV_PIX_FORMATON_FLAG_
     */
    unsigned flags;
    /**
     * Size of all the pixel components packed as one element including
     * padding.
     *
     * Useful to move to the next pixel in packeted formats.
     * Useful to move to the next pixel in non-planar formats and
     * to compute the palette size.
     *
     * It is set to 0 for planar and quasi-planar formats.
     *
     * examples:
     *
     * For NV12 it is set to 0
     * For YUYV it is set to 4
     */
    int pixel_next;

    /**
     * Number of entries in the palette if a palette is present.
     */
    int nb_palette_entries;


    /**
     * Standard-specific model details
     */
    enum AVColorRange range;
    enum AVColorPrimaries primaries;
    enum AVColorTransferCharacteristic transfer;
    enum AVColorSpace space;
    enum AVChromaLocation location;

    int nb_components;
#define AV_PIX_FORMATON_COMPONENTS 5
    AVPixelChromaton component_desc[AV_PIX_FORMATON_COMPONENTS];
} AVPixelFormaton;

AVPixelFormaton *av_formaton_from_pixfmt(enum AVPixelFormat pix_fmt);
void av_formaton_free(AVPixelFormaton **formaton);

#endif /* AVUTIL_PIXFORMATON_H */
