// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Artur Kloniecki <arturx.kloniecki@linux.intel.com>

#include <config.h>

#if CONFIG_COMP_MUX

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/mux.h>
#include <sof/bit.h>
#include <sof/common.h>
#include <ipc/stream.h>
#include <stddef.h>
#include <stdint.h>

#if CONFIG_FORMAT_S16LE
/* \brief Demuxing 16 bit streams.
 *
 * Source stream is routed to sink with regard to routing bitmasks from
 * mux_stream_data structure. Each bitmask describes composition of single
 * output channel.
 *
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames to process.
 * \param[in] data Parameters describing channel count and routing.
 */
static void demux_s16le(struct audio_stream *sink,
			const struct audio_stream *source, uint32_t frames,
			struct mux_stream_data *data, struct mux_look_up_entry *look_up)
{
	int16_t *src;
	int16_t *dst;
	uint32_t offset;
	uint32_t dst_idx;
	uint8_t source_ch;
	uint8_t i;
	uint8_t out_ch;

	for (i = 0; i < frames; i++) {
		for (out_ch = 0; out_ch < sink->channels; out_ch++) {
			if (!look_up[out_ch].set)
				continue;

			source_ch = look_up[out_ch].in_ch;
			offset = i * source->channels;
			src = audio_stream_read_frag_s16(source,
							 offset + source_ch);
			/* saturate to 32 bits */
			dst_idx = i * sink->channels + out_ch;
			dst = audio_stream_write_frag_s16(sink, dst_idx);
			*dst = *src;
		}
	}
}

/* \brief Muxing 16 bit streams.
 *
 * Source streams are routed to sink with regard to routing bitmasks from
 * mux_stream_data structures array. Each source stream has bitmask for each
 * of it's channels describing to which channels of output stream it
 * contributes.
 *
 * \param[in,out] sink Destination buffer.
 * \param[in,out] sources Array of source buffers.
 * \param[in] frames Number of frames to process.
 * \param[in] data Array of parameters describing channel count and routing for
 *		   each stream.
 */
static void mux_s16le(struct audio_stream *sink,
		      const struct audio_stream **sources, uint32_t frames,
		      struct mux_stream_data *data, struct mux_look_up_entry *look_up)
{
	const struct audio_stream *source;
	uint8_t i;
	uint8_t source_ch;
	uint8_t out_ch;
	uint32_t offset;
	int16_t *src;
	int16_t *dst;
	uint32_t dst_idx;

	for (i = 0; i < frames; i++) {
		for (out_ch = 0; out_ch < sink->channels; out_ch++) {
			source = sources[look_up[out_ch].stream_id];

			if (!source || !look_up[out_ch].set)
				continue;

			source_ch = look_up[out_ch].in_ch;
			offset = i * source->channels;
			src = audio_stream_read_frag_s16(source,
							 offset + source_ch);
			dst_idx = i * sink->channels + out_ch;
			dst = audio_stream_write_frag_s16(sink, dst_idx);
			*dst = *src;
		}
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
/* \brief Demuxing 32 bit streams.
 *
 * Source stream is routed to sink with regard to routing bitmasks from
 * mux_stream_data structure. Each bitmask describes composition of single
 * output channel.
 *
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames to process.
 * \param[in] data Parameters describing channel count and routing.
 */
static void demux_s32le(struct audio_stream *sink,
			const struct audio_stream *source, uint32_t frames,
			struct mux_stream_data *data, struct mux_look_up_entry *look_up)
{
	int32_t *src;
	int32_t *dst;
	uint32_t offset;
	uint32_t dst_idx;
	uint8_t source_ch;
	uint8_t i;
	uint8_t out_ch;

	for (i = 0; i < frames; i++) {
		for (out_ch = 0; out_ch < sink->channels; out_ch++) {
			if (!look_up[out_ch].set)
				continue;

			source_ch = look_up[out_ch].in_ch;
			offset = i * source->channels;
			src = audio_stream_read_frag_s32(source,
							 offset + source_ch);
			/* saturate to 32 bits */
			dst_idx = i * sink->channels + out_ch;
			dst = audio_stream_write_frag_s32(sink, dst_idx);
			*dst = *src;
		}
	}
}

/* \brief Muxing 32 bit streams.
 *
 * Source streams are routed to sink with regard to routing bitmasks from
 * mux_stream_data structures array. Each source stream has bitmask for each
 * of it's channels describing to which channels of output stream it
 * contributes.
 *
 * \param[in,out] sink Destination buffer.
 * \param[in,out] sources Array of source buffers.
 * \param[in] frames Number of frames to process.
 * \param[in] data Array of parameters describing channel count and routing for
 *		   each stream.
 */
static void mux_s32le(struct audio_stream *sink,
		      const struct audio_stream **sources, uint32_t frames,
		      struct mux_stream_data *data, struct mux_look_up_entry *look_up)
{
	const struct audio_stream *source;
	uint8_t i;
	uint8_t source_ch;
	uint8_t out_ch;
	uint32_t offset;
	int32_t *src;
	int32_t *dst;
	uint32_t dst_idx;

	for (i = 0; i < frames; i++) {
		for (out_ch = 0; out_ch < sink->channels; out_ch++) {
			source = sources[look_up[out_ch].stream_id];

			if (!source || !look_up[out_ch].set)
				continue;

			source_ch = look_up[out_ch].in_ch;
			offset = i * source->channels;
			src = audio_stream_read_frag_s32(source,
							 offset + source_ch);
			dst_idx = i * sink->channels + out_ch;
			dst = audio_stream_write_frag_s32(sink, dst_idx);
			*dst = *src;
		}
	}
}

#endif /* CONFIG_FORMAT_S24LE CONFIG_FORMAT_S32LE */

const struct comp_func_map mux_func_map[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, &mux_s16le, &demux_s16le },
#endif
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, &mux_s32le, &demux_s32le },
#endif
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, &mux_s32le, &demux_s32le },
#endif
};

void mux_prepare_look_up_table(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	uint8_t i;
	uint8_t j;
	uint8_t k;

	/* Prepare look up table */
	for (i = 0; i < cd->config.num_streams; i++) {
		for (j = 0; j < PLATFORM_MAX_CHANNELS; j++) {
			for (k = 0; k < PLATFORM_MAX_CHANNELS; k++) {
				if (cd->config.streams[i].mask[j] & BIT(k)) {
					/* MUX component has only one sink */
					cd->proc_lookup[0][j].in_ch = k;
					cd->proc_lookup[0][j].stream_id = i;
					cd->proc_lookup[0][j].set = true;
				}
			}
		}
	}

	
	for (j = 0; j < PLATFORM_MAX_CHANNELS; j++) {
		comp_info(dev, "cd->proc_lookup[0][%d]: in_ch: %d, stream_id: %d", j, cd->proc_lookup[0][j].in_ch, cd->proc_lookup[0][j].stream_id);
	}
}

void demux_prepare_look_up_table(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	uint8_t i;
	uint8_t j;
	uint8_t k;

	/* Prepare look up table */
	for (i = 0; i < cd->config.num_streams; i++) {
		for (j = 0; j < PLATFORM_MAX_CHANNELS; j++) {
			for (k = 0; k < PLATFORM_MAX_CHANNELS; k++) {
				if (cd->config.streams[i].mask[j] & BIT(k)) {
					/* MUX component has only one sink */
					cd->proc_lookup[i][j].in_ch = k;
					cd->proc_lookup[i][j].stream_id = i;
					cd->proc_lookup[i][j].set = true;
				}
			}
		}
	}
}

mux_func mux_get_processing_function(struct comp_dev *dev)
{
	struct comp_buffer *sinkb;
	uint8_t i;

	if (list_is_empty(&dev->bsink_list))
		return NULL;

	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	for (i = 0; i < ARRAY_SIZE(mux_func_map); i++) {
		if (sinkb->stream.frame_fmt == mux_func_map[i].frame_format)
			return mux_func_map[i].mux_proc_func;
	}

	return NULL;
}

demux_func demux_get_processing_function(struct comp_dev *dev)
{
	struct comp_buffer *sinkb;
	uint8_t i;

	if (list_is_empty(&dev->bsink_list))
		return NULL;

	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	for (i = 0; i < ARRAY_SIZE(mux_func_map); i++) {
		if (sinkb->stream.frame_fmt == mux_func_map[i].frame_format)
			return mux_func_map[i].demux_proc_func;
	}

	return NULL;
}

#endif /* CONFIG_COMP_MUX */
