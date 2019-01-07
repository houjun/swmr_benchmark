#!/bin/bash -l
#SBATCH -p debug
#SBATCH -N 1
#SBATCH -t 0:30:00
#SBATCH -L SCRATCH
#SBATCH -C haswell
#SBATCH -J swmr_bench
#SBATCH -A m2885
#SBATCH -o o%j.1to2G_10w_nodelay
#SBATCH -e o%j.1to2G_10w_nodelay
##SBATCH --qos=premium


WRITE_SIZE=1

REPEAT=3
COUNT=12
ROUND=10

SWMR_EXE=./swmr_benchmark.exe
NONSWMR_EXE=./nonswmr_benchmark.exe
WRITE_LOC=/global/cscratch1/sd/houhun/swmr_benchmark_test_data

export DELAY=0

echo "SWMR"
echo "====================="
echo "====================="
echo "====================="
for (( j = 0; j < $REPEAT; j++ )); do

    SIZE=$WRITE_SIZE
    for (( i = 0; i < $COUNT; i++ )); do

        echo "SWMR Write"
        cmd="srun -n 4 $SWMR_EXE $SIZE $ROUND $WRITE_LOC"
        echo $cmd
        $cmd

        let SIZE=$SIZE*2

    done

done

echo "NON-SWMR"
echo "====================="
echo "====================="
echo "====================="
for (( j = 0; j < $REPEAT; j++ )); do

    SIZE=$WRITE_SIZE
    for (( i = 0; i < $COUNT; i++ )); do

        echo "Non-SWMR Write"
        cmd="srun -n 4 $NONSWMR_EXE $SIZE $ROUND $WRITE_LOC"
        echo $cmd
        $cmd


        let SIZE=$SIZE*2

    done

done


date
echo "=========="
echo "END OF JOB"
