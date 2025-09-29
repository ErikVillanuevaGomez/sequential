#!/bin/bash
#SBATCH --job-name=Parallel
#SBATCH --partition=Centaurus
#SBATCH --time=00:10:00
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=1
#SBATCH --cpus-per-task=16

cd $HOME/sequential

make clean
make

rm -f results.txt

echo "--- Running Sequential Client: level_client ---" >> results.txt
echo "Running Tom Hanks (Depth 3)..." >> results.txt
{ /usr/bin/time -p ./level_client "Tom Hanks" 3; } >> results.txt 2>&1
echo "---------------------------------" >> results.txt

echo "Running Kevin Bacon (Depth 2)..." >> results.txt
{ /usr/bin/time -p ./level_client "Kevin Bacon" 2; } >> results.txt 2>&1
echo "---------------------------------" >> results.txt

echo "" >> results.txt
echo "--- Running Parallel Client: parallel_level_client ---" >> results.txt
echo "Running Tom Hanks (Depth 3) with 8 Threads..." >> results.txt
{ /usr/bin/time -p ./parallel_level_client "Tom Hanks" 3 8; } >> results.txt 2>&1
echo "---------------------------------" >> results.txt

echo "Running Tom Hanks (Depth 3) with 16 Threads..." >> results.txt
{ /usr/bin/time -p ./parallel_level_client "Tom Hanks" 3 16; } >> results.txt 2>&1
echo "---------------------------------" >> results.txt

echo "Running Kevin Bacon (Depth 2) with 8 Threads..." >> results.txt
{ /usr/bin/time -p ./parallel_level_client "Kevin Bacon" 2 8; } >> results.txt 2>&1
echo "---------------------------------" >> results.txt
