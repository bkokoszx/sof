// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>

#include <ipc/topology.h>
#include <sof/list.h>
#include <ipc/stream.h>
#include <sof/trace/trace.h>
#include <sof/drivers/ipc.h>
#include <sof/lib/alloc.h>
#include <sof/audio/component.h>
#include <stddef.h>
#include <stdint.h>
#include <sof/drivers/timer.h>
#include <sof/lib/wait.h>

/* tracing */
#define trace_proc_module(format, ...) \
	trace_event(TRACE_CLASS_PROCESSING_MODULE, format, ##__VA_ARGS__)
#define trace_proc_module_error(format, ...) \
	trace_error(TRACE_CLASS_PROCESSING_MODULE, format, ##__VA_ARGS__)
#define tracev_proc_module(format, ...)	\
	tracev_event(TRACE_CLASS_PROCESSING_MODULE, format, ##__VA_ARGS__)

#define PROC_MODULE_DEBUG_TRACES 1
#define PROC_MODULE_DUMMY_MIPS	50

struct comp_data {
	void (*process_func)(struct comp_buffer *source,
			     struct comp_buffer *sink, uint32_t samples);
};

static void produce_dummy_mips(struct comp_dev *dev, uint64_t mips)
{
	uint32_t pipe_period; /* execution pipeline period in us*/

	pipe_period = dev->pipeline->ipc_pipe.period;

	trace_proc_module("produce_dummy_mips(): start");

	// long version: idelay(mips * 1000000 / 1000000 * pipe_period) i.e.
	// mips * milion / (s/us) * period (in us)
	idelay(mips * pipe_period);

	trace_proc_module("produce_dummy_mips(): end");
}

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

static int proc_module_params(struct comp_dev *dev,
			      struct sof_ipc_stream_params *params)
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
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source;
	struct comp_buffer *sink;
	uint32_t input_bytes;
	uint32_t output_bytes;
	uint32_t copy_samples;
	uint32_t copy_bytes;

	tracev_proc_module("proc_module_copy()");

	/* fetching source and sink buffers */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);

	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);

	/* input available samples */
	input_bytes = source->avail;

	if (input_bytes < dev->min_source_bytes) {
		tracev_proc_module("proc_module_copy(), not enough input "
				   "bytes: %u", input_bytes);

		return 0;
	}

	/* output available samples */
	output_bytes = sink->free;

	if (output_bytes < dev->min_sink_bytes) {
		tracev_proc_module("proc_module_copy(), not enough output "
				   "bytes: %u", output_bytes);
		return 0;
	}

	/* copy as much as possible */
	copy_bytes = dev->min_source_bytes *
		MIN(input_bytes / dev->min_source_bytes,
		    output_bytes / dev->min_sink_bytes);

#if PROC_MODULE_DEBUG_TRACES
	trace_proc_module("proc_module_copy(), source->frame_fmt: %u",
			  source->frame_fmt);
	trace_proc_module("proc_module_copy(), input bytes available: %u",
			  input_bytes);
	trace_proc_module("proc_module_copy(), output bytes available: %u",
			  output_bytes);
	trace_proc_module("proc_module_copy(), copy_bytes: %u",
			  copy_bytes);
#endif

	copy_samples = copy_bytes / buffer_sample_bytes(source);

	cd->process_func(source, sink, copy_samples);

	/* dummy mips */
	produce_dummy_mips(dev, PROC_MODULE_DUMMY_MIPS);

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

	dev->min_source_bytes = process->min_source_bytes;
	dev->min_sink_bytes = process->min_sink_bytes;

#if PROC_MODULE_DEBUG_TRACES
	trace_proc_module("proc_module_prepare(): dev->min_source_bytes: %u",
			  dev->min_source_bytes);
	trace_proc_module("proc_module_prepare(): dev->min_sink_bytes: %u",
			  dev->min_sink_bytes);
	trace_proc_module("proc_module_prepare(): source->size: %u",
			  source->size);
	trace_proc_module("proc_module_prepare(): sink->size: %u",
			  sink->size);
	trace_proc_module("proc_module_prepare(): config->periods_source: %u",
			  config->periods_source);
	trace_proc_module("proc_module_prepare(): config->periods_sink: %u",
			  config->periods_sink);
#endif

	if (dev->min_source_bytes * config->periods_source > source->size) {
		trace_proc_module("proc_module_prepare(): "
				  "dev->min_source_bytes * "
				  "config->periods_source > source->size");

		comp_set_state(dev, COMP_TRIGGER_RESET);
		return -EINVAL;
	}

	if (dev->min_sink_bytes * config->periods_sink > sink->size) {
		trace_proc_module("proc_module_prepare(): dev->min_sink_bytes "
				  "* config->periods_sink > sink->size");
		comp_set_state(dev, COMP_TRIGGER_RESET);
		return -EINVAL;
	}

	switch (process->config.frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		cd->process_func = proc_module_process_s16;
		break;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		cd->process_func = proc_module_process_s32;
		break;
	default:
		trace_proc_module("proc_module_copy(): unsupported format");
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
