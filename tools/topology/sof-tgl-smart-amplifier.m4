#
# Topology for Tigerlake with rt5682 codec.
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`ssp.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include platform specific DSP configuration
include(`platform/intel/tgl.m4')

DEBUG_START

define(`SSP_INDEX', 0)
undefine(`SSP_NAME')
define(`SSP_NAME', `NoCodec-0')

#
# Define the pipelines
#
# PCM0 ----> dsm ----> SSP(SSP_INDEX)
#             ^
#             |
#             |
# PCM0 <---- demux <----- SSP(SSP_INDEX)
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Demux pipeline 1 on PCM 0 using max 2 channels of s24le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-smart-amplifier-playback.m4,
	1, 0, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency capture pipeline 2 on PCM 0 using max 2 channels of s24le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-demux-capture.m4,
	2, 0, 4, s24le,
	1000, 0, 0,
	48000, 48000, 48000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

# playback DAI is SSP(SPP_INDEX) using 2 periods
# Buffers use s24le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SSP, SSP_INDEX, SSP_NAME,
	PIPELINE_SOURCE_1, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP(SSP_INDEX) using 2 periods
# Buffers use s24le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, SSP,SSP_INDEX, SSP_NAME,
	PIPELINE_SINK_2, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# Connect demux to dsm
SectionGraph."PIPE_DSM" {
	index "0"

	lines [
		# demux to dsm
		dapm(BUF1.2, MUXDEMUX2.0)
	]
}

# PCM Low Latency, id 0
dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)
PCM_DUPLEX_ADD(Port1, 0, PIPELINE_PCM_1, PIPELINE_PCM_2)
#PCM_CAPTURE_ADD(EchoRef, 5, PIPELINE_PCM_5)

#
# BE configurations - overrides config in ACPI if present
#

#SSP SSP_INDEX (ID: 0)
DAI_CONFIG(SSP, SSP_INDEX, 0, SSP_NAME,
	SSP_CONFIG(I2S, SSP_CLOCK(mclk, 38400000, codec_mclk_in),
		      SSP_CLOCK(bclk, 9600000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(8, 25, 15, 255),
		      SSP_CONFIG_DATA(SSP, SSP_INDEX, 24, 0, SSP_QUIRK_LBM)))

DEBUG_END
