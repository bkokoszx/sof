divert(-1)

dnl Define macro for dsm(dynamic speaker management) widget

dnl DSM name)
define(`N_DSM', `DSM'PIPELINE_ID`.'$1)

dnl W_DSM(name, format, periods_sink, periods_source, kcontrols_list)
define(`W_DSM',
`SectionVendorTuples."'N_DSM($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`	}'
`}'
`SectionData."'N_DSM($1)`_data_w" {'
`	tuples "'N_DSM($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_DSM($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionVendorTuples."'N_DSM($1)`_process_tuples_str" {'
`	tokens "sof_process_tokens"'
`	tuples."string" {'
`		SOF_TKN_PROCESS_TYPE'	"DSM"
`	}'
`}'
`SectionData."'N_DSM($1)`_data_str" {'
`	tuples "'N_DSM($1)`_tuples_str"'
`	tuples "'N_DSM($1)`_process_tuples_str"'
`}'
`SectionWidget."'N_DSM($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "effect"'
`	no_pm "true"'
`	data ['
`		"'N_DSM($1)`_data_w"'
`		"'N_DSM($1)`_data_str"'
`	]'
`	bytes ['
		$5
`	]'
`}')

divert(0)dnl
