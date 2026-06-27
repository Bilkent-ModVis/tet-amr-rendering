#!/usr/bin/env bash

cd -- "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
./build.sh
git lfs pull --include="data/mri.zip"
unzip data/mri.zip -d data
cd benchmark
python3 mri_tf_opacity_steps_benchmarks.py
