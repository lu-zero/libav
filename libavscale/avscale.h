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

#ifndef AVSCALE_AVSCALE_H
#define AVSCALE_AVSCALE_H

#include <stdint.h>

#include "libavutil/frame.h"
#include "libavutil/pixdesc.h"

#define AVSCALE_MAX_COMPONENTS 5

typedef struct AVChromaton {
    int plane;
    int h_sub_log, v_sub_log; ///< subsampling information
    int off; ///< offset to the starting element - e.g. 0 for Y, 1 for U and 3 for V in YUYV
    int shift; ///< component shift for packed, e.g. for RGB565 it will be 11,5,0
    int bpp;   ///< bits per component, e.g. for packed RGB565 you'll have 5,6,5
    int packed; ///< if component is packed with others (e.g. RGB24 - 1,1,1, NV12 - 0,1,1)
    int next; ///< offset to the next element - e.g. 2 for Y and 4 for U and V in YUYV
} AVChromaton;

typedef struct AVPixelFormaton {
    const char *name;

    unsigned flags; // has alpha, uses BE order, uses palette etc
    int entry_size; // might serve useful for packed formats - e.g. 4 or 2 bytes per entry

    enum AVColorRange range;
    enum AVColorPrimaries primaries;
    enum AVColorTransferCharacteristic trc;
    enum AVColorSpace colorspace;
    enum AVChromaLocation chroma_location;

    int nb_components;
    AVChromaton component_desc[AVSCALE_MAX_COMPONENTS];
} AVPixelFormaton;

typedef struct AVScaleContext AVScaleContext;

/**
 * Allocate an empty AVScaleContext.
 *
 * This can be configured using AVOption and passed to avscale_init_context()
 * or to avscale_build_chain() to initialize it early,
 * or used as-is directly in avscale_process_frame().
 *
 * For filling see AVOptions, options.c.
 *
 * @return NULL on failure or a pointer to a newly allocated AVScaleContext
 *
 * @see avscale_init_context
 * @see avscale_build_chain
 * @see avscale_process_frame
 * @see avscale_free_context
 */
AVScaleContext *avscale_alloc_context(void);

/**
 * Initialize the avscaler context by allocating the pixel format conversion
 * chain and the scaling kernel.
 *
 * @param ctx The context to initialize.
 *
 * @return zero or positive value on success, a negative value on error
 * @see avscale_build_chain
 * @see avscale_process_frame
 */
int avscale_init_context(AVScaleContext *ctx);

/**
 * Free the avscaler context AVScaleContext.
 * If AVScaleContext is NULL, then does nothing.
 *
 * @param ctx The context to free.
 */
void avscale_free_context(AVScaleContext *ctx);

/**
 * Build a conversion chain using the information contained in the
 * source and destination AVFrame.
 *
 * @param ctx The config to configure
 * @param src The source frame
 * @param dst The destination frame
 */
int avscale_build_chain(AVScaleContext *ctx, AVFrame *src, AVFrame *dst);

/**
 * Scale the image provided by an AVFrame in src and put the result
 * in dst.
 *
 * If the scaling context is already configured (e.g. by calling
 * avscale_init_context()) or the frame pixel format and dimensions
 * do not match the current context the function would reconfigure
 * it before scaling.
 *
 * @param c         The scaling context previously created
 *                  with avscale_alloc_context()
 * @param dst       The destination frame
 * @param src       The source frame
 * @return          0 on successo or AVERROR
 */
int avscale_process_frame(AVScaleContext *c, AVFrame *dst, AVFrame *src);

#endif /* AVSCALE_AVSCALE_H */

