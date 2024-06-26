#!/usr/bin/env python3

import argparse
import pandas as pd
import numpy as np
from parse_trace_file import *


def generate_data_file(system, in_file, key, out_file, time):
    df = pd.read_csv(in_file, index_col=0)
    nrows, ncols = df.shape

    for i, j in np.ndindex(df.shape):
        file_name = df.iloc[i, j]
        kernel, _ = file_name.split('/')
        file_name = "results/" + time + "/" + file_name
        func_name = f'parse_{system}_trace_file'
        val = eval(func_name + '(file_name, key)')
        df.iloc[i, j] = val

    df.to_csv("csv/" + time + "/" + out_file)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Datafile generator")
    parser.add_argument('system', help="Target system")
    parser.add_argument('in_layout_file', help="Input CSV layout")
    parser.add_argument('metric', help="Target metric",
                        choices=['time', 'io_bw', 'total_accessed_edges', 'total_io_bytes', 'io_amp', 'io_skew', 'compute_skew'])
    parser.add_argument('out_file', help="Output CSV file name")
    parser.add_argument('time', help="test time of script")
    args = parser.parse_args()

    generate_data_file(args.system, args.in_layout_file, args.metric, args.out_file, args.time)
