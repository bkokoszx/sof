/*
 * Copyright (c) 2019, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
 */

#include <stdint.h>
#include <stddef.h>
#include <sof/list.h>
#include <sof/ipc.h>
#include <sof/stream.h>
#include <sof/audio/processing_module.h>

#define PROC_MODULE_DEBUG_TRACES 1

/* Processing module private data, runtime data */
struct comp_data {
	enum sof_ipc_frame source_format;	/**< source frame format */
	enum sof_ipc_frame sink_format;		/**< sink frame format */

	uint32_t input_sample_bytes;
	uint32_t output_sample_bytes;

	uint32_t preload;

	uint32_t reserved[4];
};

static struct comp_dev *proc_module_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct comp_data *cd;
	struct sof_ipc_comp_process *ipc_proc_module =
		(struct sof_ipc_comp_process *)comp;

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

/* set component audio stream parameters */
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

/* used to pass standard and bespoke commands (with data) to component */
static int proc_module_cmd(struct comp_dev *dev, int cmd, void *data,
			   int max_data_size)
{
	trace_proc_module("proc_module_cmd()");

	/* mux will use buffer "connected" status */
	return 0;
}

static int is_proc_module_in_preload(struct comp_dev *dev)
{
	struct sof_ipc_comp_process *ipc_comp =
		COMP_GET_IPC(dev, sof_ipc_comp_process);
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sourceb;
	
	uint32_t isa; /* input samples available */

	sourceb = list_first_item(&dev->bsource_list,
				  struct comp_buffer, sink_list);

	isa = sourceb->avail / comp_sample_bytes(sourceb->source);

	/* check if input buffer is preloaded */
	if (cd->preload && isa < (ipc_comp->ibs * 2)) {
		trace_proc_module("proc_module_copy(): proc module preload, "
				  "Not enough amount of input samples.");
	} else {
		cd->preload = 0;
	}

	return cd->preload;
}

static int is_enough_free_output_memory(struct comp_dev *dev)
{
	struct sof_ipc_comp_process *ipc_comp =
		COMP_GET_IPC(dev, sof_ipc_comp_process);
	struct comp_buffer *sinkb;
	uint32_t osa;		// output samples available 

	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	osa = sinkb->free / comp_sample_bytes(sinkb->sink);

	trace_proc_module("osa: %d", osa);
	trace_proc_module("sinkb->free: %d", sinkb->free);
	trace_proc_module("sinkb->avail: %d", sinkb->avail);
	trace_proc_module("mp_sample_bytes(sinkb->sink): %d",comp_sample_bytes(sinkb->sink));
	trace_proc_module("ipc_comp->obs: %d", ipc_comp->obs);

	if (osa >= ipc_comp->obs) {
		trace_proc_module("osa >= ipc_comp->obs");
		return 1;
	} else {
		trace_proc_module("error: osa < ipc_comp->obs");
		// TODO: return error
		return 0;
	}
}

static int is_enough_input_samples(struct comp_dev *dev)
{
	struct sof_ipc_comp_process *ipc_comp =
		COMP_GET_IPC(dev, sof_ipc_comp_process);
	struct comp_buffer *sourceb;
	uint32_t isa;		// input samples available

	sourceb = list_first_item(&dev->bsource_list,
		struct comp_buffer, sink_list);

	isa = sourceb->avail / comp_sample_bytes(sourceb->source);

	if (isa >= ipc_comp->ibs)
		return 1;
	else
		// TODO: return error
		return 0;
} 

/* copy and process stream data from source to sink buffers */
static int proc_module_copy(struct comp_dev *dev)
{
	struct sof_ipc_comp_process *ipc_comp =
		COMP_GET_IPC(dev, sof_ipc_comp_process);

	struct comp_buffer *sourceb;
	struct comp_buffer *sinkb;

	int32_t *src;
	int32_t *dest;

	uint32_t bytes_avail;
	uint32_t copy_samples;
	uint32_t isa;		/* input samples available */
	uint32_t osa;		/* output samples available */

	uint32_t buff_frag;
	uint32_t i;

	trace_proc_module("proc_module_copy()");

	/* checking if component is in preload */
	if (is_proc_module_in_preload(dev))
		return 0;

	/* TODO: xrun handling */
	if (!is_enough_free_output_memory(dev))
		return 0;

	/* TODO: xrun handling */
	if (!is_enough_input_samples(dev))
		return 0;

	/* fetching source and sink buffers */
	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);

	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	/* input and output available samples calculation */
	isa = sourceb->avail / comp_sample_bytes(sourceb->source);
	osa = sinkb->free / comp_sample_bytes(sinkb->source);

	copy_samples = MIN(isa, osa);

#if PROC_MODULE_DEBUG_TRACES
	trace_proc_module("proc_module_copy(), dev->params.frame_fmt: %d",
			  dev->params.frame_fmt);
	trace_proc_module("proc_module_copy(), input samples available: %d",
			  isa);
	trace_proc_module("proc_module_copy(), output samples available: %d",
			  osa);
	trace_proc_module("proc_module_copy(), copy_samples: %d",
		osa);
#endif

	/* Copy max one period */
	copy_samples = MIN(copy_samples, ipc_comp->ibs);
	bytes_avail = copy_samples * comp_sample_bytes(dev);

	buff_frag = 0;

	/* Assumption: now only for format 32/32 */
	for (i = 0; i < copy_samples; i++) {
		src = buffer_read_frag_s32(sourceb, buff_frag);
		dest = buffer_write_frag_s32(sinkb, buff_frag);

		*dest = *src;

		buff_frag++;
	}

#if PROC_MODULE_DEBUG_TRACES
	trace_proc_module("proc_module_copy(), produce_bytes: %d",
			  bytes_avail);
#endif
	comp_update_buffer_produce(sinkb, bytes_avail);

#if PROC_MODULE_DEBUG_TRACES
	trace_proc_module("proc_module_copy(), consume_bytes: %d",
			  bytes_avail);
#endif
	comp_update_buffer_consume(sourceb, bytes_avail);

	return 0;
}

static int proc_module_reset(struct comp_dev *dev)
{
	trace_proc_module("proc_module_reset()");

	return 0;
}

static int proc_module_prepare(struct comp_dev *dev)
{
	struct sof_ipc_comp_process *ipc_comp =
		COMP_GET_IPC(dev, sof_ipc_comp_process);
	struct comp_data *cd = comp_get_drvdata(dev);

	struct comp_buffer *sinkb;
	struct comp_buffer *sourceb;

	uint32_t source_period_bytes;
	uint32_t sink_period_bytes;

	int ret;

	trace_proc_module("proc_module_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	cd->preload = 1;

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);

	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	/* get source data format */
	comp_set_period_bytes(sourceb->source, dev->frames, &cd->source_format,
			      &source_period_bytes);

	/* get sink data format */
	comp_set_period_bytes(sinkb->sink, dev->frames, &cd->sink_format,
			      &sink_period_bytes);

	cd->input_sample_bytes = ipc_comp->ibs * comp_sample_bytes(dev);
	cd->output_sample_bytes = ipc_comp->obs * comp_sample_bytes(dev);

#if PROC_MODULE_DEBUG_TRACES
	trace_proc_module("cd->input_sample_bytes: %d",
			  cd->input_sample_bytes);
	trace_proc_module("cd->output_sample_bytes: %d",
			  cd->output_sample_bytes);
	trace_proc_module("sink_period_bytes: %d", sink_period_bytes);
	trace_proc_module("source_period_bytes: %d", source_period_bytes);
	trace_proc_module("ipc_comp->bs: %d", ipc_comp->bs);
	trace_proc_module("ipc_comp->ibs: %d", ipc_comp->ibs);
	trace_proc_module("ipc_comp->obs: %d", ipc_comp->obs);
#endif

	/* resizing source and sink buffers */
	if (cd->input_sample_bytes > source_period_bytes) {
		/* set downstream buffer size
		 * input_sample_bytes * 2 in order to make ping-pong
		 */
		ret = buffer_resize(sourceb, cd->input_sample_bytes * 2);
		if (ret < 0) {
			trace_proc_module_error("volume_prepare() error: "
				"buffer_set_size() failed");
			goto err;
		}
	}

	if (cd->output_sample_bytes > sink_period_bytes) {
		/* set upstream buffer size
		 * output_sample_bytes * 2 in order to make ping-pong
		 */
		ret = buffer_resize(sinkb, cd->output_sample_bytes * 2);
		if (ret < 0) {
			trace_proc_module_error("volume_prepare() error: "
				"buffer_set_size() failed");
			goto err;
		}
	}

#if PROC_MODULE_DEBUG_TRACES
	trace_proc_module("sourceb->size: %d", sourceb->size);
	trace_proc_module("sinkb->size: %d", sinkb->size);
#endif

	return 0;

err:
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return ret;
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

static void sys_comp_mux_init(void)
{
	comp_register(&comp_proc_module);
}

DECLARE_MODULE(sys_comp_mux_init);
