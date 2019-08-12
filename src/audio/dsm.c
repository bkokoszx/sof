// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>

#include <sof/audio/dsm.h>
#include <sof/audio/component.h>
#include <sof/trace/trace.h>
#include <sof/drivers/ipc.h>
#include <sof/ut.h>

#define trace_dsm(__e, ...) \
	trace_event(TRACE_CLASS_DSM, __e, ##__VA_ARGS__)

#define trace_dsm_with_ids(comp_ptr, format, ...)	\
	trace_event_with_ids(TRACE_CLASS_DSM,		\
			     comp_ptr->comp.pipeline_id,\
			     comp_ptr->comp.id,		\
			     format, ##__VA_ARGS__)

#define tracev_dsm(__e, ...) \
	tracev_event(TRACE_CLASS_DSM, __e, ##__VA_ARGS__)

#define tracev_dsm_with_ids(comp_ptr, format, ...)	\
	tracev_event_with_ids(TRACE_CLASS_DSM,		\
			     comp_ptr->comp.pipeline_id,\
			     comp_ptr->comp.id,		\
			     format, ##__VA_ARGS__)

#define trace_dsm_error(__e, ...) \
	trace_error(TRACE_CLASS_DSM, __e, ##__VA_ARGS__)

#define trace_dsm_error_with_ids(comp_ptr, format, ...)	\
	trace_error_with_ids(TRACE_CLASS_DSM,		\
			     comp_ptr->comp.pipeline_id,\
			     comp_ptr->comp.id,		\
			     format, ##__VA_ARGS__)

struct dsm_data {
	struct comp_buffer *source_buf; /**< stream source buffer */
	struct comp_buffer *feedback_buf; /**< feedback source buffer */
	struct comp_buffer *sink_buf; /**< sink buffer */
};

static struct comp_dev *dsm_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_dsm *dsm;
	struct sof_ipc_comp_dsm *ipc_dsm =
		(struct sof_ipc_comp_dsm *)comp;
	struct dsm_data *dd;

	trace_dsm("dsm_new()");

	if (IPC_IS_SIZE_INVALID(ipc_dsm->config)) {
		IPC_SIZE_ERROR_TRACE(TRACE_CLASS_DSM, ipc_dsm->config);
		return NULL;
	}

	dev = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
		      COMP_SIZE(struct sof_ipc_comp_dsm));
	if (!dev)
		return NULL;

	dsm = (struct sof_ipc_comp_dsm *)&dev->comp;

	assert(!memcpy_s(dsm, sizeof(*dsm), ipc_dsm,
	       sizeof(struct sof_ipc_comp_dsm)));

	dd = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*dd));

	if (!dd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, dd);
	dev->state = COMP_STATE_READY;

	return dev;
}

static void dsm_free(struct comp_dev *dev)
{
	struct dsm_data *dd = comp_get_drvdata(dev);

	trace_dsm("dsm_free()");

	rfree(dd);
	rfree(dev);
}

static int dsm_params(struct comp_dev *dev)
{
	trace_dsm("dsm_params()");

	return 0;
}

static int dsm_demux_params(struct comp_dev *dev)
{
	trace_dsm("dsm_demux_params()");

	return 0;
}

static int dsm_trigger(struct comp_dev *dev, int cmd)
{
	struct dsm_data *dd = comp_get_drvdata(dev);
	int ret = 0;

	trace_dsm("dsm_trigger(), command = %u", cmd);

	ret = comp_set_state(dev, cmd);

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		ret = PPL_STATUS_PATH_STOP;

	switch (cmd) {
	case COMP_TRIGGER_START:
	case COMP_TRIGGER_RELEASE:
		buffer_zero(dd->feedback_buf);
		break;
	case COMP_TRIGGER_PAUSE:
	case COMP_TRIGGER_STOP:
		break;
	default:
		break;
	}

	return ret;
}

static int dsm_demux_trigger(struct comp_dev *dev, int cmd)
{
	int ret = 0;

	trace_dsm("dsm_demux_trigger(), command = %u", cmd);

	ret = comp_set_state(dev, cmd);

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		ret = PPL_STATUS_PATH_STOP;

	return ret;
}

static int dsm_process_s16(struct comp_dev *dev, struct comp_buffer *source,
			   struct comp_buffer *sink, uint32_t frames)
{
	int16_t *src;
	int16_t *dest;
	uint32_t channel;
	uint32_t buff_frag;
	int i;

	trace_dsm_with_ids(dev, "dsm_process_s16()");

	buff_frag = 0;
	for (i = 0; i < frames; i++) {
		for (channel = 0; channel < dev->params.channels; channel++) {
			src = buffer_read_frag_s16(source, buff_frag);
			dest = buffer_write_frag_s16(sink, buff_frag);

			*dest = *src;
			buff_frag++;
		}
	}

	return 0;
}

static int dsm_process_s32(struct comp_dev *dev, struct comp_buffer *source,
			   struct comp_buffer *sink, uint32_t frames)
{
	int32_t *src;
	int32_t *dest;
	uint32_t channel;
	uint32_t buff_frag;
	int i;

	trace_dsm_with_ids(dev, "dsm_process_s32()");

	buff_frag = 0;
	for (i = 0; i < frames; i++) {
		for (channel = 0; channel < dev->params.channels; channel++) {
			src = buffer_read_frag_s32(source, buff_frag);
			dest = buffer_write_frag_s32(sink, buff_frag);

			*dest = *src;
			buff_frag++;
		}
	}

	return 0;
}

static int dsm_process(struct comp_dev *dev, uint32_t avail_frames,
		       struct comp_buffer *source,
		       struct comp_buffer *sink)
{
	int ret = 0;

	switch (dev->params.frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		ret = dsm_process_s16(dev, source, sink, avail_frames);
		break;
	case SOF_IPC_FRAME_S32_LE:
		ret = dsm_process_s32(dev, source, sink, avail_frames);

		break;
	default:
		trace_dsm_error_with_ids(dev, "dsm_process() error: "
			"not supported frame format");
		return -EINVAL;
	}

	return ret;
}

static int dsm_process_feedback_data(struct comp_buffer *buf,
				     uint32_t avail_bytes)
{
	(void)buf;
	(void)avail_bytes;

	/* here it is possible to process avail_bytes from feedback buf */

	return 0;
}

static int dsm_copy(struct comp_dev *dev)
{
	struct dsm_data *dd = comp_get_drvdata(dev);
	uint32_t avail_frames;
	uint32_t copy_bytes;
	int ret = 0;

	trace_dsm_with_ids(dev, "dsm_copy()");

	avail_frames = comp_avail_frames(dd->source_buf, dd->sink_buf);
	copy_bytes = avail_frames * comp_frame_bytes(dev);

	/* process data */
	dsm_process(dev, avail_frames, dd->source_buf, dd->sink_buf);

	/* sink and source buffer pointers update */
	comp_update_buffer_produce(dd->sink_buf, copy_bytes);
	comp_update_buffer_consume(dd->source_buf, copy_bytes);

	/* from feedback buffer we should consume as much data as we consume
	 * from source buffer.
	 */
	if (dd->feedback_buf->avail < copy_bytes) {
		trace_dsm_with_ids(dev, "dsm_copy(): not enough data in "
				   "feedback buffer");

		return ret;
	}

	trace_dsm_with_ids(dev, "dsm_copy(): processing %d feedback bytes",
			   copy_bytes);
	dsm_process_feedback_data(dd->feedback_buf, copy_bytes);
	comp_update_buffer_consume(dd->feedback_buf, copy_bytes);

	return ret;
}

static int dsm_demux_copy(struct comp_dev *dev)
{
	struct dsm_data *dd = comp_get_drvdata(dev);
	uint32_t avail_frames;
	uint32_t copy_bytes;
	int ret = 0;

	trace_dsm_with_ids(dev, "dsm_demux_copy()");

	avail_frames = comp_avail_frames(dd->source_buf, dd->sink_buf);
	copy_bytes = avail_frames * comp_frame_bytes(dev);

	trace_dsm_with_ids(dev, "dsm_demux_copy(): copy from source_buf to "
			   "sink_buf");

	dsm_process(dev, avail_frames, dd->source_buf, dd->sink_buf);

	trace_dsm_with_ids(dev, "dsm_demux_copy(): copy from source_buf to "
			   "feedback_buf");

	dsm_process(dev, avail_frames, dd->source_buf, dd->feedback_buf);

	/* update buffer pointers */
	comp_update_buffer_produce(dd->sink_buf, copy_bytes);
	comp_update_buffer_produce(dd->feedback_buf, copy_bytes);
	comp_update_buffer_consume(dd->source_buf, copy_bytes);

	return ret;
}

static int dsm_reset(struct comp_dev *dev)
{
	trace_dsm("dsm_reset()");

	comp_set_state(dev, COMP_TRIGGER_RESET);

	return 0;
}

static int dsm_prepare(struct comp_dev *dev)
{
	struct sof_ipc_comp_config *config = COMP_GET_CONFIG(dev);
	struct sof_ipc_comp_dsm *ipc_dsm =
		(struct sof_ipc_comp_dsm *)&dev->comp;
	struct dsm_data *dd = comp_get_drvdata(dev);
	struct comp_buffer *source_buffer;
	struct list_item *blist;
	uint32_t period_bytes;
	int ret;

	(void)ipc_dsm;

	trace_dsm("dsm_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* calculate period size based on config */
	period_bytes = dev->frames * comp_frame_bytes(dev);
	if (period_bytes == 0) {
		trace_dsm_error("dsm_prepare() error: period_bytes = 0");
		return -EINVAL;
	}

	/* set downstream buffer size */
	ret = comp_set_sink_buffer(dev, period_bytes, config->periods_sink);
	if (ret < 0) {
		trace_dsm_error("dsm_prepare() error: "
				"comp_set_sink_buffer() failed");
		return ret;
	}

	/* searching for stream and feedback source buffers */
	list_for_item(blist, &dev->bsource_list) {
		source_buffer = container_of(blist, struct comp_buffer,
					     sink_list);

		if (source_buffer->ipc_buffer.comp.id ==
		    ipc_dsm->feedback_buf_id)
			dd->feedback_buf = source_buffer;
		else
			dd->source_buf = source_buffer;
	}

	dd->sink_buf = list_first_item(&dev->bsink_list, struct comp_buffer,
				       source_list);

	return 0;
}

static int dsm_demux_prepare(struct comp_dev *dev)
{
	struct sof_ipc_comp_dsm *ipc_dsm =
		(struct sof_ipc_comp_dsm *)&dev->comp;
	struct dsm_data *dd = comp_get_drvdata(dev);
	struct comp_buffer *sink_buffer;
	struct list_item *blist;
	uint32_t period_bytes;
	int ret;

	(void)ipc_dsm;

	trace_dsm("dsm_demux_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* calculate period size based on config */
	period_bytes = dev->frames * comp_frame_bytes(dev);
	if (period_bytes == 0) {
		trace_dsm_error("dsm_prepare() error: period_bytes = 0");
		return -EINVAL;
	}

	/* searching for stream and feedback source buffers */
	list_for_item(blist, &dev->bsink_list) {
		sink_buffer = container_of(blist, struct comp_buffer,
					   source_list);

		if (sink_buffer->ipc_buffer.comp.id ==
			ipc_dsm->feedback_buf_id)
			dd->feedback_buf = sink_buffer;
		else
			dd->sink_buf = sink_buffer;
	}

	dd->source_buf = list_first_item(&dev->bsource_list, struct comp_buffer,
					 sink_list);

	return 0;
}

struct comp_driver comp_dsm = {
	.type = SOF_COMP_DSM,
	.ops = {
		.new = dsm_new,
		.free = dsm_free,
		.params = dsm_params,
		.prepare = dsm_prepare,
		.trigger = dsm_trigger,
		.copy = dsm_copy,
		.reset = dsm_reset,
	},
};

struct comp_driver comp_dsm_demux = {
	.type = SOF_COMP_DSM_DEMUX,
	.ops = {
		.new = dsm_new,
		.free = dsm_free,
		.params = dsm_demux_params,
		.prepare = dsm_demux_prepare,
		.trigger = dsm_demux_trigger,
		.copy = dsm_demux_copy,
		.reset = dsm_reset,
	},
};

UT_STATIC void sys_comp_dsm_init(void)
{
	comp_register(&comp_dsm);
	comp_register(&comp_dsm_demux);
}

DECLARE_MODULE(sys_comp_dsm_init);
