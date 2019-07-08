// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>

#include <sof/ipc.h>
#include <sof/list.h>
#include <sof/stream.h>
#include <stddef.h>
#include <stdint.h>

/* tracing */
#define trace_proc_module(format, ...) \
	trace_event(TRACE_CLASS_PROCESSING_MODULE, format, ##__VA_ARGS__)
#define trace_proc_module_error(format, ...) \
	trace_error(TRACE_CLASS_PROCESSING_MODULE, format, ##__VA_ARGS__)
#define tracev_proc_module(format, ...)	\
	tracev_event(TRACE_CLASS_PROCESSING_MODULE, format, ##__VA_ARGS__)

#define PROC_MODULE_DEBUG_TRACES 1

#define PROC_MODULE_DEFAULT_PERIODS 2 /* in order to achieve ping-pong */

struct comp_data {
	uint32_t preload;
};

static struct comp_dev *proc_module_new(struct sof_ipc_comp *comp)
{
	struct sof_ipc_comp_process *ipc_proc_module =
		(struct sof_ipc_comp_process *)comp;
	struct comp_dev *dev;
	struct comp_data *cd;

	trace_proc_module("proc_module_new()");

	if (IPC_IS_SIZE_INVALID(ipc_proc_module->config)) {
		IPC_SIZE_ERROR_TRACE(TRACE_CLASS_PROCESSING_MODULE,
				     ipc_proc_module->config);
		return NULL;
	}

	dev = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
		      COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev)
		return NULL;

	assert(!memcpy_s(&dev->comp, sizeof(struct sof_ipc_comp_process),
	       comp, sizeof(struct sof_ipc_comp_process)));

	cd = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	dev->state = COMP_STATE_READY;

	return dev;
}

static void proc_module_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_proc_module("proc_module_free()");

	rfree(cd);
	rfree(dev);
}

static int proc_module_params(struct comp_dev *dev)
{
	trace_proc_module("proc_module_params()");

	return 0;
}

static int proc_module_trigger(struct comp_dev *dev, int cmd)
{
	trace_proc_module("proc_module_trigger()");

	return comp_set_state(dev, cmd);
}

static int proc_module_cmd(struct comp_dev *dev, int cmd, void *data,
			   int max_data_size)
{
	trace_proc_module("proc_module_cmd()");

	return 0;
}

static void proc_module_process_s32(struct comp_buffer *source,
				    struct comp_buffer *sink,
				    uint32_t samples)
{
	int32_t *src;
	int32_t *dest;
	uint32_t i;

	for (i = 0; i < samples; i++) {
		src = buffer_read_frag_s32(source, i);
		dest = buffer_write_frag_s32(sink, i);
		*dest = *src;
	}
}

static void proc_module_process_s16(struct comp_buffer *source,
				    struct comp_buffer *sink,
				    uint32_t samples)
{
	int16_t *src;
	int16_t *dest;
	uint32_t i;

	for (i = 0; i < samples; i++) {
		src = buffer_read_frag_s16(source, i);
		dest = buffer_write_frag_s16(sink, i);
		*dest = *src;
	}
}

static int proc_module_copy(struct comp_dev *dev)
{
	struct sof_ipc_comp_process *process =
		COMP_GET_IPC(dev, sof_ipc_comp_process);
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source;
	struct comp_buffer *sink;
	uint32_t input_samples;
	uint32_t output_samples;
	uint32_t copy_samples;
	uint32_t copy_bytes;

	tracev_proc_module("proc_module_copy()");

	/* fetching source and sink buffers */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);

	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);

	/* input available samples */
	input_samples = source->avail / comp_sample_bytes(source->source);

	if (cd->preload) {
		/* if source buffer is fulfilled */
		if (source->avail >= process->ibs *
		    PROC_MODULE_DEFAULT_PERIODS) {
			/* end of preload state */
			cd->preload = 0;
			trace_proc_module("exit proc module preload");
		} else {
			trace_proc_module("proc module preload");
			return pipeline_is_preload(dev->pipeline) ?
				PPL_STATUS_PATH_STOP : 0;
		}
	}

	if (input_samples < process->ibs) {
		tracev_proc_module("proc_module_copy(), not enough input "
				   "samples: %u", input_samples);
		return pipeline_is_preload(dev->pipeline) ?
			PPL_STATUS_PATH_STOP : 0;
	}

	/* output available samples */
	output_samples = sink->free / comp_sample_bytes(sink->sink);

	if (output_samples < process->obs) {
		tracev_proc_module("proc_module_copy(), not enough output "
				   "samples: %u", output_samples);
		return 0;
	}

	/* copy as much as possible */
	copy_samples = process->ibs * MIN(input_samples / process->ibs,
					  output_samples / process->obs);

#if PROC_MODULE_DEBUG_TRACES
	trace_proc_module("proc_module_copy(), dev->params.frame_fmt: %u",
			  dev->params.frame_fmt);
	trace_proc_module("proc_module_copy(), input samples available: %u",
			  input_samples);
	trace_proc_module("proc_module_copy(), output samples available: %u",
			  output_samples);
	trace_proc_module("proc_module_copy(), copy_samples: %u",
			  copy_samples);
#endif

	copy_bytes = copy_samples * comp_sample_bytes(dev);

	switch (process->config.frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		proc_module_process_s16(source, sink, copy_samples);
		break;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		proc_module_process_s32(source, sink, copy_samples);
		break;
	default:
		trace_proc_module("proc_module_copy(): unsupported format");
		return -EINVAL;
	}

#if PROC_MODULE_DEBUG_TRACES
	trace_proc_module("proc_module_copy(), produce_bytes: %u",
			  copy_bytes);
#endif
	comp_update_buffer_produce(sink, copy_bytes);

#if PROC_MODULE_DEBUG_TRACES
	trace_proc_module("proc_module_copy(), consume_bytes: %u",
			  copy_bytes);
#endif
	comp_update_buffer_consume(source, copy_bytes);

	return 0;
}

static int proc_module_reset(struct comp_dev *dev)
{
	trace_proc_module("proc_module_reset()");

	return comp_set_state(dev, COMP_TRIGGER_RESET);
}

static int proc_module_prepare(struct comp_dev *dev)
{
	struct sof_ipc_comp_process *process =
		COMP_GET_IPC(dev, sof_ipc_comp_process);
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_comp_config *config = COMP_GET_CONFIG(dev);
	struct comp_buffer *source;
	struct comp_buffer *sink;
	uint32_t out_sample_bytes;
	uint32_t in_sample_bytes;
	int ret;

	trace_proc_module("proc_module_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);

	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);

	in_sample_bytes = process->ibs * comp_sample_bytes(dev);
	out_sample_bytes = process->obs * comp_sample_bytes(dev);

#if PROC_MODULE_DEBUG_TRACES
	trace_proc_module("proc_module_prepare(): process->bs: %u",
			  process->bs);
	trace_proc_module("proc_module_prepare(): process->ibs: %u",
			  process->ibs);
	trace_proc_module("proc_module_prepare(): process->obs: %u",
			  process->obs);
	trace_proc_module("proc_module_prepare(): out_sample_bytes: %u",
			  out_sample_bytes);
	trace_proc_module("proc_module_prepare(): source->size: %u",
			  source->size);
	trace_proc_module("proc_module_prepare(): sink->size: %u",
			  sink->size);
	trace_proc_module("proc_module_prepare(): config->periods_source: %u",
			  config->periods_source);
	trace_proc_module("proc_module_prepare(): config->periods_sink: %u",
			  config->periods_sink);
#endif
	cd->preload = 1;

	if (in_sample_bytes * PROC_MODULE_DEFAULT_PERIODS > source->size) {
		trace_proc_module("proc_module_prepare(): in_sample_bytes * "
				  "PROC_MODULE_DEFAULT_PERIODS > "
				  "source->size");
		comp_set_state(dev, COMP_TRIGGER_RESET);
		return -EINVAL;
	}

	if (out_sample_bytes * PROC_MODULE_DEFAULT_PERIODS > sink->size) {
		trace_proc_module("proc_module_prepare(): out_sample_bytes * "
				  "PROC_MODULE_DEFAULT_PERIODS > sink->size");
		comp_set_state(dev, COMP_TRIGGER_RESET);
		return -EINVAL;
	}

#if PROC_MODULE_DEBUG_TRACES
	trace_proc_module("sink->size: %u", sink->size);
#endif

	return 0;
}

struct comp_driver comp_proc_module = {
	.type = SOF_COMP_PROCESSING_MODULE,
	.ops = {
		.new =		proc_module_new,
		.free =		proc_module_free,
		.params =	proc_module_params,
		.cmd =		proc_module_cmd,
		.trigger =	proc_module_trigger,
		.copy =		proc_module_copy,
		.prepare =	proc_module_prepare,
		.reset =	proc_module_reset,
},
};

static void sys_comp_proc_module_init(void)
{
	comp_register(&comp_proc_module);
}

DECLARE_MODULE(sys_comp_proc_module_init);
