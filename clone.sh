#!/bin/bash

# OP-TEE gits
git clone --branch 4.0.0 --depth 1 https://github.com/OP-TEE/optee_test.git optee_test

# linaro-swg gits
git clone --branch optee-4.0.0 --depth 1 https://github.com/linaro-swg/linux.git linux
git clone --branch 4.0.0 --depth 1 https://github.com/linaro-swg/optee_benchmark.git optee_benchmark
git clone --branch 4.0.0 --depth 1 https://github.com/linaro-swg/optee_examples.git optee_examples

# Misc gits
git clone --branch 2022.11.1 --depth 1 https://github.com/buildroot/buildroot.git buildroot
git clone --branch mbedtls-2.26.0 --depth 1 https://github.com/Mbed-TLS/mbedtls.git mbedtls
git clone https://github.com/apache/incubator-teaclave-trustzone-sdk.git optee_rust
cd optee_rust
git checkout 4031e7282a8f398f54faa19acb2b84fab05de877
cd ..
git clone --branch v8.0.0 --depth 1 https://github.com/qemu/qemu.git qemu
git clone --branch v2.9 --depth 1 https://git.trustedfirmware.org/TF-A/trusted-firmware-a.git trusted-firmware-a
git clone --branch v2.9 --depth 1 https://git.trustedfirmware.org/hafnium/hafnium.git hafnium
git clone --branch v2023.07.02 --depth 1 https://source.denx.de/u-boot/u-boot.git u-boot
