#!/usr/bin/env bash

cd -- "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
git submodule update --init --recursive

mkdir build
wget -O - https://github.com/NVIDIA/optix-dev/archive/refs/tags/v7.7.0.tar.gz | tar -xvz -C build
export OptiX_INSTALL_DIR="$(realpath "build/optix-dev-7.7.0")"
wget -O - https://micro.mamba.pm/api/micromamba/$(uname)-$(uname -m)/latest | tar -xvj --strip-components=1 -C build bin/micromamba
MAMBA="$(realpath "build/micromamba")"
mkdir build/mamba_env
MAMBA_ENV="$(realpath "build/mamba_env")"
$MAMBA create -y -p $MAMBA_ENV -f environment.yml

cmake -S . -B build \
	-DCMAKE_BUILD_TYPE=Release \
	-DVTK_DIR="$MAMBA_ENV/lib/cmake/vtk-9.3" \
	-DCUDAToolkit_ROOT="$MAMBA_ENV" \
	-DCMAKE_CUDA_COMPILER="$MAMBA_ENV/bin/nvcc" \
	-DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc \
	-DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/g++ \
  	-DCMAKE_CUDA_HOST_COMPILER:FILEPATH=/usr/bin/g++
cmake --build build -j
