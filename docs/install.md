# Installation

## Requirements

Install the packages

* gdb: package libdb++-dev in Debian

* yaml-cpp: package libyaml-cpp-dev in Debian

* [rocksdb](https://github.com/facebook/rocksdb):
    ```
    git clone https://github.com/facebook/rocksdb.git
    cd rocksdb
    make static_lib
    sudo make install-static
    ```

* [croaring](https://github.com/RoaringBitmap/CRoaring):
    ```
    git clone https://github.com/RoaringBitmap/CRoaring.git
    cd CRoaring
    mkdir -p build
    cd build
    cmake ..
    sudo make install
    ```

## Build

### Install from source on Linux

```
git clone https://github.com/dlutsky/grove.git
cd grove
make
```