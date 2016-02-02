/*
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

#include "config.h"
#include <string.h>

#include "avscale.h"
#include "internal.h"

extern const AVScaleKernel avs_murder_kernel;
extern const AVScaleKernel avs_rgbpack_kernel;
extern const AVScaleKernel avs_rgbunpack_kernel;
extern const AVScaleKernel avs_rgb2yuv_kernel;
extern const AVScaleKernel avs_scale_kernel;
extern const AVScaleKernel avs_yuv2rgb_kernel;

static const AVScaleKernel* avs_kernels[] = {
    &avs_murder_kernel,
    &avs_rgbpack_kernel,
    &avs_rgbunpack_kernel,
    &avs_rgb2yuv_kernel,
    &avs_scale_kernel,
    &avs_yuv2rgb_kernel,
    NULL,
};


const AVScaleKernel *avscale_find_kernel(const char *name)
{
    const AVScaleKernel **k = avs_kernels;

    while (*k) {
        if (!strcmp((*k)->name, name))
            return *k;
        k++;
    }

    return 0;
}

//TODO add dictionary
int avscale_apply_kernel(AVScaleContext *ctx,
                         const char *name,
                         AVScaleFilterStage *stage)
{
    const AVScaleKernel *k = avscale_find_kernel(name);
    if (!k)
        return AVERROR(EINVAL);
    return k->kernel_init(ctx, k, stage, NULL);

}
