#!/bin/bash

echo 'Input: [preprocess_path]/text2csr.sh [dataset_path] [dataset_name]'

if [ ! $# -eq 2 ];
then
    echo Input wrong
    exit
fi

echo $@

dataset_path=$1
dataset_name=$2

base=$PWD

file_count=200


if [ ! -f $dataset_path/txt/${dataset_name}.txt ];
then
    echo File **$dataset_path/txt/${dataset_name}.txt** does not exist
    exit
fi

echo "Converting text to csr format"
echo "Generating multiple files for the dataset"
mkdir -p $dataset_path/multi
# format: [preprocess_path]/split.sh [input_txt] [output_dir] [num_split_files] [name]
bash preprocess/split.sh $dataset_path/txt/${dataset_name}.txt $dataset_path/multi/ $file_count ${dataset_name}

# convert to binary format
cd ${base}/preprocess/text2bin && make
./text2bin.bin ${base}/$dataset_path/multi/${dataset_name} $(($file_count+1)) 16 1

# convert to csr format
cd ${base}/preprocess/bin2csr && make
./bin2csr.bin ${base}/$dataset_path/multi/${dataset_name} $(($file_count+1)) 1 1 16

# remove the binary files and rename the csr files
cd ${base} && mkdir -p $dataset_path/csr_bin
mv $dataset_path/multi/${dataset_name}_beg.0_0_of_1x1.bin $dataset_path/csr_bin/${dataset_name}.idx
mv $dataset_path/multi/${dataset_name}_csr.0_0_of_1x1.bin $dataset_path/csr_bin/${dataset_name}.adj
mv $dataset_path/multi/${dataset_name}.config $dataset_path/csr_bin/${dataset_name}.config
rm -rf $dataset_path/multi

# generate in-csr
cd ${base}/preprocess/genincsr && make
./main ${base}/$dataset_path/csr_bin/ ${dataset_name}
