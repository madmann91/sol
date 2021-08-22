# SoL

SoL (for _Speed of Light_, or sun in Spanish) is a small rendering library written in C++20.
Its goal is to strike a good balance between performance and usability,
and allow easy experimentation for rendering researchers.
SoL is provided as a library that can be embedded in other projects.

## Building

The recommended way to build SoL is by cloning the meta-repository [Solar](https://github.com/madmann91/solar),
which downloads and installs all dependencies automatically.

If you prefer to do this manually, you will need Git, CMake,
[proto](https://github.com/madmann91/proto), [par](https://github.com/madmann91/par),
and [bvh](https://github.com/madmann91/bvh) (`v2` branch).
Additionally, SoL can use libpng, libjpeg, and libtiff when those are present on the system.
These libraries are not required to build a working version of SoL, but without them, it can only load and save EXR image files.

Before building, make sure you have downloaded all the submodules, by running:

    git submodule update --init --recursive

Once all submodules have been downloaded and the dependencies have been downloaded and installed, type:

    mkdir build
    cd build
    cmake \
        -Dproto_DIR=</path/to/proto> \
        -Dpar_DIR=</path/to/par> \
        -Dbvh_DIR=</path/to/bvh> \
        -DSOL_MULTITHREADING_FRAMEWORK=<None|OpenMP|TBB> \
        -DCMAKE_BUILD_TYPE=<Debug|Release|RelWithDebInfo>
    cmake --build .
