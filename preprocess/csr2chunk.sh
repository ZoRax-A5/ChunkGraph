#!/bin/bash

echo 'Input: [preprocess_path]/csr2chunk.sh [dataset_path] [dataset_name] [out_path]'

if [ ! $# -eq 3 ];
then
    echo Input wrong
    exit
fi

echo $@

base=$PWD
dataset_path=$1
dataset_name=$2
out_path=$3

sblk_size=256
threshold=90
nverts=$(cat ${dataset_path}/${dataset_name}.config)

echo $nverts

cd CSRGraph && make
./bin/main -f ${base}/${dataset_path} --prefix ${dataset_name} --ssd ${base}/${out_path} --sblk_pool_size ${sblk_size} --global_threshold ${threshold} -t 1 -q 0 -j 6 -v ${nverts}