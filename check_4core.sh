cd results_4core

all=$(ls -q * | wc -l)
good=$(grep "ChampSim completed all CPUs" * | wc -l)

if [ $all -eq $good ]; then
    echo "OK"
else
    echo "Not OK"
fi

