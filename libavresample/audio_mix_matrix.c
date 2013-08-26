/*
 * Copyright (C) 2011 Michael Niedermayer (michaelni@gmx.at)
 * Copyright (c) 2012 Justin Ruggles <justin.ruggles@gmail.com>
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

#include <stdint.h>

#include "libavutil/channel_layout.h"
#include "libavutil/common.h"
#include "libavutil/libm.h"
#include "libavutil/samplefmt.h"
#include "avresample.h"
#include "internal.h"
#include "audio_data.h"
#include "audio_mix.h"

#define SQRT3_2      1.22474487139158904909  /* sqrt(3/2) */

static av_always_inline int even(uint64_t layout)
{
    return (!layout || !!(layout & (layout - 1)));
}

static int sane_layout(const AVChannelLayout *layout)
{
    /* check that there is at least 1 front speaker */
    if (!av_channel_layout_subset(layout, AV_CH_LAYOUT_SURROUND))
        return 0;

    /* check for left/right symmetry */
    if (!even(av_channel_layout_subset(layout, (AV_CH_FRONT_LEFT           | AV_CH_FRONT_RIGHT)))           ||
        !even(av_channel_layout_subset(layout, (AV_CH_SIDE_LEFT            | AV_CH_SIDE_RIGHT)))            ||
        !even(av_channel_layout_subset(layout, (AV_CH_BACK_LEFT            | AV_CH_BACK_RIGHT)))            ||
        !even(av_channel_layout_subset(layout, (AV_CH_FRONT_LEFT_OF_CENTER | AV_CH_FRONT_RIGHT_OF_CENTER))) ||
        !even(av_channel_layout_subset(layout, (AV_CH_TOP_FRONT_LEFT       | AV_CH_TOP_FRONT_RIGHT)))       ||
        !even(av_channel_layout_subset(layout, (AV_CH_TOP_BACK_LEFT        | AV_CH_TOP_BACK_RIGHT)))        ||
        !even(av_channel_layout_subset(layout, (AV_CH_STEREO_LEFT          | AV_CH_STEREO_RIGHT)))          ||
        !even(av_channel_layout_subset(layout, (AV_CH_WIDE_LEFT            | AV_CH_WIDE_RIGHT)))            ||
        !even(av_channel_layout_subset(layout, (AV_CH_SURROUND_DIRECT_LEFT | AV_CH_SURROUND_DIRECT_RIGHT))))
        return 0;

    return 1;
}

#define IDX_OUT(ch) (av_channel_layout_channel_index(out_layout, ch))

int avresample_build_matrix2(const AVChannelLayout *in_layout,
                             const AVChannelLayout *out_layout,
                             double center_mix_level, double surround_mix_level,
                             double lfe_mix_level, int normalize,
                             double *matrix, int stride,
                             enum AVMatrixEncoding matrix_encoding)
{
    static const AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    int i, j;
    double maxcoef = 0;
    int idx_in, idx_out, idx_r, idx_l;
    int in_channels  = in_layout->nb_channels;
    int out_channels = out_layout->nb_channels;;

    /* check if layouts are supported */
    if (!av_channel_layout_check(in_layout) ||
        in_channels > AVRESAMPLE_MAX_CHANNELS)
        return AVERROR(EINVAL);
    if (!av_channel_layout_check(out_layout) ||
        out_channels > AVRESAMPLE_MAX_CHANNELS)
        return AVERROR(EINVAL);

    /* check if layouts are unbalanced or abnormal */
    if (!sane_layout(in_layout) || !sane_layout(out_layout))
        return AVERROR_PATCHWELCOME;

    if (out_channels == 2                  &&
        IDX_OUT(AV_CHAN_STEREO_LEFT)  >= 0 &&
        IDX_OUT(AV_CHAN_STEREO_RIGHT) >= 0)
        out_layout = &stereo;

    memset(matrix, 0, out_channels * stride * sizeof(*matrix));

    for (idx_in = 0; idx_in < in_channels; idx_in++) {
        enum AVChannel in_ch = av_channel_layout_get_channel(in_layout, idx_in);

        idx_out = IDX_OUT(in_ch);

        /* check if the input channel is also present in output */
        if (idx_out >= 0) {
            if (in_ch == AV_CHAN_FRONT_CENTER &&
                av_channel_layout_subset(in_layout,   AV_CH_LAYOUT_STEREO) == AV_CH_LAYOUT_STEREO &&
                !av_channel_layout_subset(out_layout, AV_CH_LAYOUT_STEREO)) {
                /* mix left/right/center to center */
                matrix[idx_out * stride + idx_in] = center_mix_level * M_SQRT2;
            } else {
                /* just copy it */
                matrix[idx_out * stride + idx_in] = 1.0;
            }

            continue;
        }

        /* the input channel is not present in the output layout */

        /* mix front center to front left/right */
        if (in_ch == AV_CHAN_FRONT_CENTER) {
            int idx_l = IDX_OUT(AV_CHAN_FRONT_LEFT);
            int idx_r = IDX_OUT(AV_CHAN_FRONT_RIGHT);
            if (idx_l >= 0 && idx_r >= 0) {
                matrix[idx_l * stride + idx_in] += M_SQRT1_2;
                matrix[idx_r * stride + idx_in] += M_SQRT1_2;
            }
        }

        /* mix front left/right to center */
        if (in_ch == AV_CHAN_FRONT_LEFT || in_ch == AV_CHAN_FRONT_RIGHT) {
            idx_out = IDX_OUT(AV_CHAN_FRONT_CENTER);
            if (idx_out >= 0)
                matrix[idx_out * stride + idx_in] += M_SQRT1_2;
        }

        /* mix back center to back, side, or front */
        if (in_ch == AV_CHAN_BACK_CENTER) {
            int idx_l = IDX_OUT(AV_CHAN_BACK_LEFT);
            int idx_r = IDX_OUT(AV_CHAN_BACK_RIGHT);
            if (idx_l >= 0 && idx_r >= 0) {
                matrix[idx_l * stride + idx_in] += M_SQRT1_2;
                matrix[idx_r * stride + idx_in] += M_SQRT1_2;
                continue;
            }

            idx_l = IDX_OUT(AV_CHAN_SIDE_LEFT);
            idx_r = IDX_OUT(AV_CHAN_SIDE_RIGHT);
            if (idx_l >= 0 && idx_r >= 0) {
                matrix[idx_l * stride + idx_in] += M_SQRT1_2;
                matrix[idx_r * stride + idx_in] += M_SQRT1_2;
                continue;
            }

            idx_l = IDX_OUT(AV_CHAN_FRONT_LEFT);
            idx_r = IDX_OUT(AV_CHAN_FRONT_RIGHT);
            if (idx_l >= 0 && idx_r >= 0) {
                if (matrix_encoding == AV_MATRIX_ENCODING_DOLBY ||
                    matrix_encoding == AV_MATRIX_ENCODING_DPLII) {
                    if (!av_channel_layout_subset(out_layout, AV_CH_BACK_LEFT | AV_CH_SIDE_LEFT) &&
                         av_channel_layout_subset(in_layout,  AV_CH_BACK_LEFT | AV_CH_SIDE_LEFT)) {
                        matrix[idx_l * stride + idx_in] -= surround_mix_level * M_SQRT1_2;
                        matrix[idx_r * stride + idx_in] += surround_mix_level * M_SQRT1_2;
                    } else {
                        matrix[idx_l * stride + idx_in] -= surround_mix_level;
                        matrix[idx_r * stride + idx_in] += surround_mix_level;
                    }
                } else {
                    matrix[idx_l * stride + idx_in] += surround_mix_level * M_SQRT1_2;
                    matrix[idx_r * stride + idx_in] += surround_mix_level * M_SQRT1_2;
                }
                continue;
            }

            idx_out = IDX_OUT(AV_CHAN_FRONT_CENTER);
            if (idx_out >= 0) {
                matrix[idx_out * stride + idx_in] += surround_mix_level * M_SQRT1_2;
                continue;
            }
        }

        /* mix back left/right to back center, side, or front */
        if (in_ch == AV_CHAN_BACK_LEFT || in_ch == AV_CHAN_BACK_RIGHT) {
            idx_out = IDX_OUT(AV_CHAN_BACK_CENTER);
            if (idx_out >= 0) {
                matrix[idx_out * stride + idx_in] += M_SQRT1_2;
                continue;
            }

            idx_out = IDX_OUT(in_ch == AV_CHAN_BACK_LEFT ? AV_CHAN_SIDE_LEFT :
                                                           AV_CHAN_SIDE_RIGHT);
            if (idx_out >= 0) {
                /* if side channels do not exist in the input, just copy back
                   channels to side channels, otherwise mix back into side */
                if (av_channel_layout_subset(in_layout, AV_CH_SIDE_LEFT))
                    matrix[idx_out * stride + idx_in] += M_SQRT1_2;
                else
                    matrix[idx_out * stride + idx_in] += 1.0;

                continue;
            }

            idx_l = IDX_OUT(AV_CHAN_FRONT_LEFT);
            idx_r = IDX_OUT(AV_CHAN_FRONT_RIGHT);
            if (idx_l >= 0 && idx_r >= 0) {
                if (matrix_encoding == AV_MATRIX_ENCODING_DOLBY) {
                    matrix[idx_l * stride + idx_in] -= surround_mix_level * M_SQRT1_2;
                    matrix[idx_r * stride + idx_in] += surround_mix_level * M_SQRT1_2;
                } else if (matrix_encoding == AV_MATRIX_ENCODING_DPLII) {
                    if (in_ch == AV_CHAN_BACK_LEFT) {
                        matrix[idx_l * stride + idx_in] -= surround_mix_level * SQRT3_2;
                        matrix[idx_r * stride + idx_in] += surround_mix_level * M_SQRT1_2;
                    } else {
                        matrix[idx_l * stride + idx_in] -= surround_mix_level * M_SQRT1_2;
                        matrix[idx_r * stride + idx_in] += surround_mix_level * SQRT3_2;
                    }
                } else {
                    idx_out = (in_ch == AV_CHAN_BACK_LEFT) ? idx_l : idx_r;
                    matrix[idx_out * stride + idx_in] += surround_mix_level;
                }

                continue;
            }

            idx_out = IDX_OUT(AV_CHAN_FRONT_CENTER);
            if (idx_out >= 0) {
                matrix[idx_out * stride + idx_in] += surround_mix_level * M_SQRT1_2;
                continue;
            }
        }

        /* mix side left/right into back or front */
        if (in_ch == AV_CHAN_SIDE_LEFT || in_ch == AV_CHAN_SIDE_RIGHT) {
            idx_out = IDX_OUT(in_ch == AV_CHAN_SIDE_LEFT ? AV_CHAN_BACK_LEFT :
                                                           AV_CHAN_BACK_RIGHT);
            if (idx_out >= 0) {
                /* if back channels do not exist in the input, just copy side
                   channels to back channels, otherwise mix side into back */
                if (av_channel_layout_subset(in_layout, AV_CH_BACK_LEFT))
                    matrix[idx_out * stride + idx_in] += M_SQRT1_2;
                else
                    matrix[idx_out * stride + idx_in] += 1.0;

                continue;
            }

            idx_out = IDX_OUT(AV_CHAN_BACK_CENTER);
            if (idx_out >= 0) {
                matrix[idx_out * stride + idx_in] += M_SQRT1_2;
                continue;
            }


            idx_l = IDX_OUT(AV_CHAN_FRONT_LEFT);
            idx_r = IDX_OUT(AV_CHAN_FRONT_RIGHT);
            if (idx_l >= 0 && idx_r >= 0) {
                if (matrix_encoding == AV_MATRIX_ENCODING_DOLBY) {
                    matrix[idx_l * stride + idx_in] -= surround_mix_level * M_SQRT1_2;
                    matrix[idx_r * stride + idx_in] += surround_mix_level * M_SQRT1_2;
                } else if (matrix_encoding == AV_MATRIX_ENCODING_DPLII) {
                    if (in_ch == AV_CHAN_SIDE_LEFT) {
                        matrix[idx_l * stride + idx_in] -= surround_mix_level * SQRT3_2;
                        matrix[idx_r * stride + idx_in] += surround_mix_level * M_SQRT1_2;
                    } else {
                        matrix[idx_l * stride + idx_in] -= surround_mix_level * M_SQRT1_2;
                        matrix[idx_r * stride + idx_in] += surround_mix_level * SQRT3_2;
                    }
                } else {
                    idx_out = (in_ch == AV_CHAN_SIDE_LEFT) ? idx_l : idx_r;
                    matrix[idx_out * stride + idx_in] += surround_mix_level;
                }

                continue;
            }

            idx_out = IDX_OUT(AV_CHAN_FRONT_CENTER);
            if (idx_out >= 0) {
                matrix[idx_out * stride + idx_in] += surround_mix_level * M_SQRT1_2;
                continue;
            }
        }

        /* mix left-of-center/right-of-center into front left/right or center */
        if (in_ch == AV_CHAN_FRONT_LEFT_OF_CENTER ||
            in_ch == AV_CHAN_FRONT_RIGHT_OF_CENTER) {
            idx_out = IDX_OUT(in_ch == AV_CHAN_FRONT_LEFT_OF_CENTER ?
                              AV_CHAN_FRONT_LEFT : AV_CHAN_FRONT_RIGHT);
            if (idx_out >= 0) {
                matrix[idx_out * stride + idx_in] += 1.0;
                continue;
            }

            idx_out = IDX_OUT(AV_CHAN_FRONT_CENTER);
            if (idx_out >= 0) {
                matrix[idx_out * stride + idx_in] += M_SQRT1_2;
                continue;
            }
        }

        /* mix LFE into front left/right or center */
        if (in_ch == AV_CHAN_LOW_FREQUENCY) {
            idx_out = IDX_OUT(AV_CHAN_FRONT_CENTER);
            if (idx_out >= 0) {
                matrix[idx_out * stride + idx_in] += lfe_mix_level;
                continue;
            }

            idx_l = IDX_OUT(AV_CHAN_FRONT_LEFT);
            idx_r = IDX_OUT(AV_CHAN_FRONT_RIGHT);
            if (idx_l >= 0 && idx_r >= 0) {
                matrix[idx_l * stride + idx_in] += lfe_mix_level * M_SQRT1_2;
                matrix[idx_r * stride + idx_in] += lfe_mix_level * M_SQRT1_2;
                continue;
            }
        }
    }

    /* calculate maximum per-channel coefficient sum */
    for (idx_out = 0; idx_out < out_channels; idx_out++) {
        double sum = 0;
        for (idx_in = 0; idx_in < in_channels; idx_in++)
            sum += fabs(matrix[idx_out * stride + idx_in]);
        maxcoef = FFMAX(maxcoef, sum);
    }

    /* normalize */
    if (normalize && maxcoef > 1.0) {
        for (i = 0; i < out_channels; i++)
            for (j = 0; j < in_channels; j++)
                matrix[i * stride + j] /= maxcoef;
    }

    return 0;
}

#if FF_API_OLD_CHANNEL_LAYOUT
int avresample_build_matrix(uint64_t in_layout, uint64_t out_layout,
                            double center_mix_level, double surround_mix_level,
                            double lfe_mix_level, int normalize,
                            double *matrix_out, int stride,
                            enum AVMatrixEncoding matrix_encoding)
{
    AVChannelLayout in_chl, out_chl;

    av_channel_layout_from_mask(&in_chl,  in_layout);
    av_channel_layout_from_mask(&out_chl, out_layout);
    return avresample_build_matrix2(&in_chl, &out_chl, center_mix_level,
                                    surround_mix_level, lfe_mix_level, normalize,
                                    matrix_out, stride, matrix_encoding);
}
#endif
