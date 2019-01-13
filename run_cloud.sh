TRACE_DIR=/your/trace/directory/
binary=${1}
n_warm=${2}
n_sim=${3}
trace=${4}
option=${5}

trace1=${trace}_core_0
trace2=${trace}_core_1
trace3=${trace}_core_2
trace4=${trace}_core_3

mkdir -p results_4core_cloud
(./bin/${binary} -warmup_instructions ${n_warm}000000 -simulation_instructions ${n_sim}000000 ${option} -traces ${TRACE_DIR}/${trace1}.trace.gz ${TRACE_DIR}/${trace2}.trace.gz ${TRACE_DIR}/${trace3}.trace.gz ${TRACE_DIR}/${trace4}.trace.gz) &> results_4core_cloud/${trace}-${binary}.txt
