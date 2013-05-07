/*
 * Copyright (c) 2006 Michael Niedermayer <michaelni@gmx.at>
 * Copyright (c) 2008 Peter Ross
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

#ifndef AVUTIL_CHANNEL_LAYOUT_H
#define AVUTIL_CHANNEL_LAYOUT_H

#include <stdint.h>

#include "version.h"
#include "attributes.h"

/**
 * @file
 * audio channel layout utility functions
 */

/**
 * @addtogroup lavu_audio
 * @{
 */

enum AVChannel {
    AV_CHAN_FRONT_LEFT,
    AV_CHAN_FRONT_RIGHT,
    AV_CHAN_FRONT_CENTER,
    AV_CHAN_LOW_FREQUENCY,
    AV_CHAN_BACK_LEFT,
    AV_CHAN_BACK_RIGHT,
    AV_CHAN_FRONT_LEFT_OF_CENTER,
    AV_CHAN_FRONT_RIGHT_OF_CENTER,
    AV_CHAN_BACK_CENTER,
    AV_CHAN_SIDE_LEFT,
    AV_CHAN_SIDE_RIGHT,
    AV_CHAN_TOP_CENTER,
    AV_CHAN_TOP_FRONT_LEFT,
    AV_CHAN_TOP_FRONT_CENTER,
    AV_CHAN_TOP_FRONT_RIGHT,
    AV_CHAN_TOP_BACK_LEFT,
    AV_CHAN_TOP_BACK_CENTER,
    AV_CHAN_TOP_BACK_RIGHT,
    ///< Stereo downmix.
    AV_CHAN_STEREO_LEFT = 29,
    ///< See AV_CH_STEREO_LEFT.
    AV_CHAN_STEREO_RIGHT,
    AV_CHAN_WIDE_LEFT,
    AV_CHAN_WIDE_RIGHT,
    AV_CHAN_SURROUND_DIRECT_LEFT,
    AV_CHAN_SURROUND_DIRECT_RIGHT,
    AV_CHAN_LOW_FREQUENCY_2,
};

enum AVChannelOrder {
    /**
     * The native channel order, i.e. the channels are in the same order in
     * which they are defined in the AVChannel enum.
     */
    AV_CHANNEL_ORDER_NATIVE,
    /**
     * The channel order does not correspond to any other predefined order and
     * is stored as an explicit map.
     */
    AV_CHANNEL_ORDER_CUSTOM,
    /**
     * Only the channel count is specified, without any further information
     * about the channels.
     */
    AV_CHANNEL_ORDER_UNSPEC,
};


/**
 * @defgroup channel_masks Audio channel masks
 * @{
 */
#define AV_CH_FRONT_LEFT             (1ULL << AV_CHAN_FRONT_LEFT           )
#define AV_CH_FRONT_RIGHT            (1ULL << AV_CHAN_FRONT_RIGHT          )
#define AV_CH_FRONT_CENTER           (1ULL << AV_CHAN_FRONT_CENTER         )
#define AV_CH_LOW_FREQUENCY          (1ULL << AV_CHAN_LOW_FREQUENCY        )
#define AV_CH_BACK_LEFT              (1ULL << AV_CHAN_BACK_LEFT            )
#define AV_CH_BACK_RIGHT             (1ULL << AV_CHAN_BACK_RIGHT           )
#define AV_CH_FRONT_LEFT_OF_CENTER   (1ULL << AV_CHAN_FRONT_LEFT_OF_CENTER )
#define AV_CH_FRONT_RIGHT_OF_CENTER  (1ULL << AV_CHAN_FRONT_RIGHT_OF_CENTER)
#define AV_CH_BACK_CENTER            (1ULL << AV_CHAN_BACK_CENTER          )
#define AV_CH_SIDE_LEFT              (1ULL << AV_CHAN_SIDE_LEFT            )
#define AV_CH_SIDE_RIGHT             (1ULL << AV_CHAN_SIDE_RIGHT           )
#define AV_CH_TOP_CENTER             (1ULL << AV_CHAN_TOP_CENTER           )
#define AV_CH_TOP_FRONT_LEFT         (1ULL << AV_CHAN_TOP_FRONT_LEFT       )
#define AV_CH_TOP_FRONT_CENTER       (1ULL << AV_CHAN_TOP_FRONT_CENTER     )
#define AV_CH_TOP_FRONT_RIGHT        (1ULL << AV_CHAN_TOP_FRONT_RIGHT      )
#define AV_CH_TOP_BACK_LEFT          (1ULL << AV_CHAN_TOP_BACK_LEFT        )
#define AV_CH_TOP_BACK_CENTER        (1ULL << AV_CHAN_TOP_BACK_CENTER      )
#define AV_CH_TOP_BACK_RIGHT         (1ULL << AV_CHAN_TOP_BACK_RIGHT       )
#define AV_CH_STEREO_LEFT            (1ULL << AV_CHAN_STEREO_LEFT          )
#define AV_CH_STEREO_RIGHT           (1ULL << AV_CHAN_STEREO_RIGHT         )
#define AV_CH_WIDE_LEFT              (1ULL << AV_CHAN_WIDE_LEFT            )
#define AV_CH_WIDE_RIGHT             (1ULL << AV_CHAN_WIDE_RIGHT           )
#define AV_CH_SURROUND_DIRECT_LEFT   (1ULL << AV_CHAN_SURROUND_DIRECT_LEFT )
#define AV_CH_SURROUND_DIRECT_RIGHT  (1ULL << AV_CHAN_SURROUND_DIRECT_RIGHT)
#define AV_CH_LOW_FREQUENCY_2        (1ULL << AV_CHAN_LOW_FREQUENCY_2      )

#if FF_API_OLD_CHANNEL_LAYOUT
/** Channel mask value used for AVCodecContext.request_channel_layout
    to indicate that the user requests the channel order of the decoder output
    to be the native codec channel order.
    @deprecated channel order is now indicated in a special field in
                AVChannelLayout
    */
#define AV_CH_LAYOUT_NATIVE          0x8000000000000000ULL
#endif

/**
 * @}
 * @defgroup channel_mask_c Audio channel convenience macros
 * @{
 * */
#define AV_CH_LAYOUT_MONO              (AV_CH_FRONT_CENTER)
#define AV_CH_LAYOUT_STEREO            (AV_CH_FRONT_LEFT|AV_CH_FRONT_RIGHT)
#define AV_CH_LAYOUT_2POINT1           (AV_CH_LAYOUT_STEREO|AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_2_1               (AV_CH_LAYOUT_STEREO|AV_CH_BACK_CENTER)
#define AV_CH_LAYOUT_SURROUND          (AV_CH_LAYOUT_STEREO|AV_CH_FRONT_CENTER)
#define AV_CH_LAYOUT_3POINT1           (AV_CH_LAYOUT_SURROUND|AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_4POINT0           (AV_CH_LAYOUT_SURROUND|AV_CH_BACK_CENTER)
#define AV_CH_LAYOUT_4POINT1           (AV_CH_LAYOUT_4POINT0|AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_2_2               (AV_CH_LAYOUT_STEREO|AV_CH_SIDE_LEFT|AV_CH_SIDE_RIGHT)
#define AV_CH_LAYOUT_QUAD              (AV_CH_LAYOUT_STEREO|AV_CH_BACK_LEFT|AV_CH_BACK_RIGHT)
#define AV_CH_LAYOUT_5POINT0           (AV_CH_LAYOUT_SURROUND|AV_CH_SIDE_LEFT|AV_CH_SIDE_RIGHT)
#define AV_CH_LAYOUT_5POINT1           (AV_CH_LAYOUT_5POINT0|AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_5POINT0_BACK      (AV_CH_LAYOUT_SURROUND|AV_CH_BACK_LEFT|AV_CH_BACK_RIGHT)
#define AV_CH_LAYOUT_5POINT1_BACK      (AV_CH_LAYOUT_5POINT0_BACK|AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_6POINT0           (AV_CH_LAYOUT_5POINT0|AV_CH_BACK_CENTER)
#define AV_CH_LAYOUT_6POINT0_FRONT     (AV_CH_LAYOUT_2_2|AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER)
#define AV_CH_LAYOUT_HEXAGONAL         (AV_CH_LAYOUT_5POINT0_BACK|AV_CH_BACK_CENTER)
#define AV_CH_LAYOUT_6POINT1           (AV_CH_LAYOUT_5POINT1|AV_CH_BACK_CENTER)
#define AV_CH_LAYOUT_6POINT1_BACK      (AV_CH_LAYOUT_5POINT1_BACK|AV_CH_BACK_CENTER)
#define AV_CH_LAYOUT_6POINT1_FRONT     (AV_CH_LAYOUT_6POINT0_FRONT|AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_7POINT0           (AV_CH_LAYOUT_5POINT0|AV_CH_BACK_LEFT|AV_CH_BACK_RIGHT)
#define AV_CH_LAYOUT_7POINT0_FRONT     (AV_CH_LAYOUT_5POINT0|AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER)
#define AV_CH_LAYOUT_7POINT1           (AV_CH_LAYOUT_5POINT1|AV_CH_BACK_LEFT|AV_CH_BACK_RIGHT)
#define AV_CH_LAYOUT_7POINT1_WIDE      (AV_CH_LAYOUT_5POINT1|AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER)
#define AV_CH_LAYOUT_7POINT1_WIDE_BACK (AV_CH_LAYOUT_5POINT1_BACK|AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER)
#define AV_CH_LAYOUT_OCTAGONAL         (AV_CH_LAYOUT_5POINT0|AV_CH_BACK_LEFT|AV_CH_BACK_CENTER|AV_CH_BACK_RIGHT)
#define AV_CH_LAYOUT_HEXADECAGONAL     (AV_CH_LAYOUT_OCTAGONAL|AV_CH_WIDE_LEFT|AV_CH_WIDE_RIGHT|AV_CH_TOP_BACK_LEFT|AV_CH_TOP_BACK_RIGHT|AV_CH_TOP_BACK_CENTER|AV_CH_TOP_FRONT_CENTER|AV_CH_TOP_FRONT_LEFT|AV_CH_TOP_FRONT_RIGHT)
#define AV_CH_LAYOUT_STEREO_DOWNMIX    (AV_CH_STEREO_LEFT|AV_CH_STEREO_RIGHT)

enum AVMatrixEncoding {
    AV_MATRIX_ENCODING_NONE,
    AV_MATRIX_ENCODING_DOLBY,
    AV_MATRIX_ENCODING_DPLII,
    AV_MATRIX_ENCODING_DPLIIX,
    AV_MATRIX_ENCODING_DPLIIZ,
    AV_MATRIX_ENCODING_DOLBYEX,
    AV_MATRIX_ENCODING_DOLBYHEADPHONE,
    AV_MATRIX_ENCODING_NB
};

/**
 * @}
 */

/**
 * An AVChannelLayout holds information about the channel layout of audio data.
 *
 * A channel layout here is defined as a set of channels ordered in a specific
 * way (unless the channel order is AV_CHANNEL_ORDER_UNSPEC, in which case an
 * AVChannelLayout carries only the channel count).
 *
 * Unlike most structures in Libav, sizeof(AVChannelLayout) is a part of the
 * public ABI and may be used by the caller. E.g. it may be allocated on stack.
 * No new fields may be added to it without a major version bump.
 *
 * An AVChannelLayout can be constructed using the convenience function
 * av_channel_layout_from_mask() / av_channel_layout_from_string(), or it can be
 * built manually by the caller.
 */
typedef struct AVChannelLayout {
    /**
     * Channel order used in this layout.
     */
    enum AVChannelOrder order;
    /**
     * Number of channels in this layout.
     */
    int nb_channels;
    /**
     * Details about which channels are present in this layout.
     * For AV_CHANNEL_ORDER_UNSPEC, this field is undefined and must not be
     * used.
     */
    union {
        /**
         * This member must be used for AV_CHANNEL_ORDER_NATIVE.
         * It is a bitmask, where the position of each set bit means that the
         * AVChannel with the corresponding value is present.
         *
         * I.e. when (mask & (1 << AV_CHAN_FOO)) is non-zero, then AV_CHAN_FOO
         * is present in the layout. Otherwise it is not present.
         *
         * @note when a channel layout using a bitmask is constructed or
         * modified manually (i.e.  not using any of the av_channel_layout_*
         * functions), the code doing it must ensure that the number of set bits
         * is equal to nb_channels.
         */
        uint64_t mask;
        /**
         * This member must be used when the channel order is
         * AV_CHANNEL_ORDER_CUSTOM. It is a nb_channels-sized array, with each
         * element signalling the presend of the AVChannel with the
         * corresponding value.
         *
         * I.e. when map[i] is equal to AV_CHAN_FOO, then AV_CH_FOO is the ith
         * channel in the audio data.
         */
        uint8_t *map;
    } u;
} AVChannelLayout;

#define AV_CHANNEL_LAYOUT_MONO \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 1,  .u = { .mask = AV_CH_LAYOUT_MONO }}
#define AV_CHANNEL_LAYOUT_STEREO \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 2,  .u = { .mask = AV_CH_LAYOUT_STEREO }}
#define AV_CHANNEL_LAYOUT_2POINT1 \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 3,  .u = { .mask = AV_CH_LAYOUT_2POINT1 }}
#define AV_CHANNEL_LAYOUT_2_1 \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 3,  .u = { .mask = AV_CH_LAYOUT_2_1 }}
#define AV_CHANNEL_LAYOUT_SURROUND \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 3,  .u = { .mask = AV_CH_LAYOUT_SURROUND }}
#define AV_CHANNEL_LAYOUT_3POINT1 \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 4,  .u = { .mask = AV_CH_LAYOUT_3POINT1 }}
#define AV_CHANNEL_LAYOUT_4POINT0 \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 4,  .u = { .mask = AV_CH_LAYOUT_4POINT0 }}
#define AV_CHANNEL_LAYOUT_4POINT1 \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 5,  .u = { .mask = AV_CH_LAYOUT_4POINT1 }}
#define AV_CHANNEL_LAYOUT_2_2 \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 4,  .u = { .mask = AV_CH_LAYOUT_2_2 }}
#define AV_CHANNEL_LAYOUT_QUAD \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 4,  .u = { .mask = AV_CH_LAYOUT_QUAD }}
#define AV_CHANNEL_LAYOUT_5POINT0 \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 5,  .u = { .mask = AV_CH_LAYOUT_5POINT0 }}
#define AV_CHANNEL_LAYOUT_5POINT1 \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 6,  .u = { .mask = AV_CH_LAYOUT_5POINT1 }}
#define AV_CHANNEL_LAYOUT_5POINT0_BACK \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 5,  .u = { .mask = AV_CH_LAYOUT_5POINT0_BACK }}
#define AV_CHANNEL_LAYOUT_5POINT1_BACK \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 6,  .u = { .mask = AV_CH_LAYOUT_5POINT1_BACK }}
#define AV_CHANNEL_LAYOUT_6POINT0 \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 6,  .u = { .mask = AV_CH_LAYOUT_6POINT0 }}
#define AV_CHANNEL_LAYOUT_6POINT0_FRONT \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 6,  .u = { .mask = AV_CH_LAYOUT_6POINT0_FRONT }}
#define AV_CHANNEL_LAYOUT_HEXAGONAL \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 6,  .u = { .mask = AV_CH_LAYOUT_HEXAGONAL }}
#define AV_CHANNEL_LAYOUT_6POINT1 \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 7,  .u = { .mask = AV_CH_LAYOUT_6POINT1 }}
#define AV_CHANNEL_LAYOUT_6POINT1_BACK \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 7,  .u = { .mask = AV_CH_LAYOUT_6POINT1_BACK }}
#define AV_CHANNEL_LAYOUT_6POINT1_FRONT \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 7,  .u = { .mask = AV_CH_LAYOUT_6POINT1_FRONT }}
#define AV_CHANNEL_LAYOUT_7POINT0 \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 7,  .u = { .mask = AV_CH_LAYOUT_7POINT0 }}
#define AV_CHANNEL_LAYOUT_7POINT0_FRONT \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 7,  .u = { .mask = AV_CH_LAYOUT_7POINT0_FRONT }}
#define AV_CHANNEL_LAYOUT_7POINT1 \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 8,  .u = { .mask = AV_CH_LAYOUT_7POINT1 }}
#define AV_CHANNEL_LAYOUT_7POINT1_WIDE \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 8,  .u = { .mask = AV_CH_LAYOUT_7POINT1_WIDE }}
#define AV_CHANNEL_LAYOUT_7POINT1_WIDE_BACK \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 8,  .u = { .mask = AV_CH_LAYOUT_7POINT1_WIDE_BACK }}
#define AV_CHANNEL_LAYOUT_OCTAGONAL \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 8,  .u = { .mask = AV_CH_LAYOUT_OCTAGONAL }}
#define AV_CHANNEL_LAYOUT_HEXADECAGONAL \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 16, .u = { .mask = AV_CH_LAYOUT_HEXAGONAL }}
#define AV_CHANNEL_LAYOUT_STEREO_DOWNMIX \
    { .order = AV_CHANNEL_ORDER_NATIVE, .nb_channels = 2,  .u = { .mask = AV_CH_LAYOUT_STEREO_DOWNMIX }}

#if FF_API_OLD_CHANNEL_LAYOUT
/**
 * Return a channel layout id that matches name, or 0 if no match is found.
 *
 * name can be one or several of the following notations,
 * separated by '+' or '|':
 * - the name of an usual channel layout (mono, stereo, 4.0, quad, 5.0,
 *   5.0(side), 5.1, 5.1(side), 7.1, 7.1(wide), downmix);
 * - the name of a single channel (FL, FR, FC, LFE, BL, BR, FLC, FRC, BC,
 *   SL, SR, TC, TFL, TFC, TFR, TBL, TBC, TBR, DL, DR);
 * - a number of channels, in decimal, optionally followed by 'c', yielding
 *   the default channel layout for that number of channels (@see
 *   av_get_default_channel_layout);
 * - a channel layout mask, in hexadecimal starting with "0x" (see the
 *   AV_CH_* macros).
 *
 * Example: "stereo+FC" = "2+FC" = "2c+1c" = "0x7"
 *
 * @deprecated use av_channel_layout_from_string()
 */
attribute_deprecated
uint64_t av_get_channel_layout(const char *name);

/**
 * Return a description of a channel layout.
 * If nb_channels is <= 0, it is guessed from the channel_layout.
 *
 * @param buf put here the string containing the channel layout
 * @param buf_size size in bytes of the buffer
 * @deprecated use av_channel_layout_describe()
 */
attribute_deprecated
void av_get_channel_layout_string(char *buf, int buf_size, int nb_channels, uint64_t channel_layout);

/**
 * Return the number of channels in the channel layout.
 * @deprecated use AVChannelLayout.nb_channels
 */
attribute_deprecated
int av_get_channel_layout_nb_channels(uint64_t channel_layout);

/**
 * Return default channel layout for a given number of channels.
 *
 * @deprecated use av_channel_layout_default()
 */
attribute_deprecated
uint64_t av_get_default_channel_layout(int nb_channels);

/**
 * Get the index of a channel in channel_layout.
 *
 * @param channel a channel layout describing exactly one channel which must be
 *                present in channel_layout.
 *
 * @return index of channel in channel_layout on success, a negative AVERROR
 *         on error.
 *
 * @deprecated use av_channel_layout_channel_index()
 */
attribute_deprecated
int av_get_channel_layout_channel_index(uint64_t channel_layout,
                                        uint64_t channel);

/**
 * Get the channel with the given index in channel_layout.
 * @deprecated use av_channel_layout_get_channel()
 */
attribute_deprecated
uint64_t av_channel_layout_extract_channel(uint64_t channel_layout, int index);

/**
 * Get the name of a given channel.
 *
 * @return channel name on success, NULL on error.
 *
 * @deprecated use av_channel_name()
 */
attribute_deprecated
const char *av_get_channel_name(uint64_t channel);
#endif

/**
 * @return a string describing a given channel
 */
const char *av_channel_name(enum AVChannel channel);

/**
 * Initialize a native channel layout from a bitmask indicating which channels
 * are present.
 */
void av_channel_layout_from_mask(AVChannelLayout *channel_layout, uint64_t mask);

/**
 * Initialize a channel layout from a given string description.
 * TODO: specify description, same as av_channel_layout_describe
 */
int av_channel_layout_from_string(AVChannelLayout *channel_layout,
                                  const char *str);

/**
 * Get the default channel layout for a given number of channels.
 */
void av_channel_layout_default(AVChannelLayout *ch_layout, int nb_channels);

/**
 * Free any allocated data in the channel layout and reset the channel
 * count to 0.
 */
void av_channel_layout_uninit(AVChannelLayout *channel_layout);

/**
 * Make a copy of a channel layout. This differs from just assigning src to dst
 * in that it allocates and copies the map for AV_CHANNEL_ORDER_CUSTOM.
 *
 * @param dst destination channel layout
 * @param src source channel layout
 * @return 0 on success, a negative AVERROR on error.
 */
int av_channel_layout_copy(AVChannelLayout *dst, const AVChannelLayout *src);

/**
 * @return a string describing channel_layout or NULL on failure in the same
 *         format that is accepted by av_channel_layout_from_string(). The
 *         returned string allocated with av_malloc() and must be freed by the
 *         caller with av_free().
 */
char *av_channel_layout_describe(AVChannelLayout *channel_layout);

/**
 * Get the channel with the given index in a channel layout.
 *
 * @return channel with the index idx in channel_layout on success or a negative
 *                 AVERROR on failure (if idx is not valid or the channel order
 *                 is unspecified)
 */
enum AVChannel av_channel_layout_get_channel(const AVChannelLayout *channel_layout,
                                             int idx);

/**
 * Get the index of a given channel in a channel layout.
 *
 * @return index of channel in channel_layout on success or a negative number if
 *         channel is not present in channel_layout.
 */
int av_channel_layout_channel_index(const AVChannelLayout *channel_layout,
                                    enum AVChannel channel);

/**
 * Find out what channels from a given set are present in a channel layout,
 * without regard for their positions.
 *
 * @param mask a combination of AV_CH_* representing a set of channels
 * @return a bitfield representing all the channels from mask that are present
 *         in channel_layout
 */
uint64_t av_channel_layout_subset(const AVChannelLayout *channel_layout,
                                  uint64_t mask);

/**
 * Check whether a channel layout is valid, i.e. can possibly describe audio
 * data.
 *
 * @return 1 if channel_layout is valid, 0 otherwise.
 */
int av_channel_layout_check(const AVChannelLayout *channel_layout);

/**
 * Check whether two channel layouts are semantically the same, i.e. the same
 * channels are present on the same positions in both.
 *
 * If one of the channel layouts is AV_CHANNEL_ORDER_UNSPEC, while the other is
 * not, they are considered to be unequal. If both are AV_CHANNEL_ORDER_UNSPEC,
 * they are considered equal iff the channel counts are the same in both.
 *
 * @return 0 if chl and chl1 are equal, 1 if they are not equal. A negative
 * AVERROR code if one or both are invalid.
 */
int av_channel_layout_compare(const AVChannelLayout *chl, const AVChannelLayout *chl1);

/**
 * @}
 */

#endif /* AVUTIL_CHANNEL_LAYOUT_H */
