# ChampSim configuration

if [ "$#" -ne 5 ]; then
    echo "Illegal number of parameters"
    echo "Usage: ./build_champsim.sh [branch_pred] [l1d_pref] [llc_pref] [llc_repl] [num_core]"
    exit 1
fi

# This function gets the path (relative to this
# script) of a component (e.g., a prefetcher) and
# returns the name of the component. It's designed to
# enjoy bash's auto-completion.
_extract_component_name() {
    # $1: path, e.g., prefetcher/next_line.l1d_pref
    # $2: directory, e.g., prefetcher
    # $3: suffix, e.g., l1d_pref
    # output: component name, e.g., next_line
    tmp=$1
    tmp=${tmp#$2/}
    tmp=${tmp%.$3}
    echo $tmp
}

BRANCH=$(_extract_component_name $1 branch bpred)                   # branch/*.bpred
L1D_PREFETCHER=$(_extract_component_name $2 prefetcher l1d_pref)    # prefetcher/*.l1d_pref
LLC_PREFETCHER=$(_extract_component_name $3 prefetcher llc_pref)    # prefetcher/*.llc_pref
LLC_REPLACEMENT=$(_extract_component_name $4 replacement llc_repl)  # replacement/*.llc_repl
NUM_CORE=$5         # tested up to 8-core system

############## Some useful macros ###############
BOLD=$(tput bold)
NORMAL=$(tput sgr0)

embed_newline()
{
   local p="$1"
   shift
   for i in "$@"
   do
      p="$p\n$i"         # Append
   done
   echo -e "$p"          # Use -e
}
#################################################

# Sanity check
if [ ! -f ./branch/${BRANCH}.bpred ] || [ ! -f ./prefetcher/${L1D_PREFETCHER}.l1d_pref ] || [ ! -f ./prefetcher/${LLC_PREFETCHER}.llc_pref ] || [ ! -f ./replacement/${LLC_REPLACEMENT}.llc_repl ]; then
	echo "${BOLD}Possible Branch Predictor: ${NORMAL}"
	LIST=$(ls branch/*.bpred | cut -d '/' -f2 | cut -d '.' -f1)
	p=$( embed_newline $LIST )
	echo "$p"

	echo "${BOLD}Possible L1D Prefetcher: ${NORMAL}"
	LIST=$(ls prefetcher/*.l1d_pref | cut -d '/' -f2 | cut -d '.' -f1)
	p=$( embed_newline $LIST )
	echo "$p"

	echo
	echo "${BOLD}Possible LLC Prefetcher: ${NORMAL}"
	LIST=$(ls prefetcher/*.llc_pref | cut -d '/' -f2 | cut -d '.' -f1)
	p=$( embed_newline $LIST )
	echo "$p"

	echo
	echo "${BOLD}Possible LLC Replacement: ${NORMAL}"
	LIST=$(ls replacement/*.llc_repl | cut -d '/' -f2 | cut -d '.' -f1)
	p=$( embed_newline $LIST )
	echo "$p"
	exit
fi

# Check for multi-core
if [ "$NUM_CORE" != "1" ]
then
    echo "${BOLD}Building multi-core ChampSim...${NORMAL}"
    sed -i.bak 's/\<NUM_CPUS 1\>/NUM_CPUS '${NUM_CORE}'/g' inc/champsim.h
	sed -i.bak 's/\<DRAM_CHANNELS 1\>/DRAM_CHANNELS 1/g' inc/champsim.h	#Kasraa: Set number of dram channels
	sed -i.bak 's/\<DRAM_CHANNELS_LOG2 0\>/DRAM_CHANNELS_LOG2 0/g' inc/champsim.h #Kasraa: Change accordingly
else
    echo "${BOLD}Building single-core ChampSim...${NORMAL}"
fi
echo

# Change prefetchers and replacement policy
cp branch/${BRANCH}.bpred branch/branch_predictor.cc
cp prefetcher/${L1D_PREFETCHER}.l1d_pref prefetcher/l1d_prefetcher.cc
cp prefetcher/${LLC_PREFETCHER}.llc_pref prefetcher/llc_prefetcher.cc
cp replacement/${LLC_REPLACEMENT}.llc_repl replacement/llc_replacement.cc

# Build
mkdir -p bin
rm -f bin/champsim
make clean
make -j #16

# Sanity check
echo ""
if [ ! -f bin/champsim ]; then
    echo "${BOLD}ChampSim build FAILED!${NORMAL}"
    echo ""
    exit
fi

echo "${BOLD}ChampSim is successfully built"
echo "Branch Predictor: ${BRANCH}"
echo "L1D Prefetcher: ${L1D_PREFETCHER}"
echo "LLC Prefetcher: ${LLC_PREFETCHER}"
echo "LLC Replacement: ${LLC_REPLACEMENT}"
echo "Cores: ${NUM_CORE}"
BINARY_NAME="${BRANCH}-${L1D_PREFETCHER}-${LLC_PREFETCHER}-${LLC_REPLACEMENT}-${NUM_CORE}core"
echo "Binary: bin/${BINARY_NAME}${NORMAL}"
echo ""
mv bin/champsim bin/${BINARY_NAME}


# Restore to the default configuration
sed -i.bak 's/\<NUM_CPUS '${NUM_CORE}'\>/NUM_CPUS 1/g' inc/champsim.h
sed -i.bak 's/\<DRAM_CHANNELS 2\>/DRAM_CHANNELS 1/g' inc/champsim.h
sed -i.bak 's/\<DRAM_CHANNELS_LOG2 1\>/DRAM_CHANNELS_LOG2 0/g' inc/champsim.h

cp branch/bimodal.bpred branch/branch_predictor.cc
cp prefetcher/no.l1d_pref prefetcher/l1d_prefetcher.cc
cp prefetcher/no.llc_pref prefetcher/llc_prefetcher.cc
cp replacement/lru.llc_repl replacement/llc_replacement.cc
