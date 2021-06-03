# SoL

SoL (for _Speed of Light_, or sun in Spanish) is a small rendering library written in C++20.
Its goal is to strike a good balance between performance and usability,
and allow easy experimentation for rendering researchers.

## Building

SoL is provided as a library that can be embedded in other projects.
In order to build it, you will need Git, and CMake.
First, make sure you have downloaded all the submodules, by running:

    git submodule update --init --recursive

Once all submodules have been downloaded, just type:

    mkdir build
    cd build
    cmake ..
    cmake --build .

> It is recommended to install TBB, if you want to use all the cores during rendering.
> On Linux distributions, this can be achieved via the package manager.
> On other systems, please refer to the documentation available on the [TBB webpage](https://github.com/oneapi-src/oneTBB).
