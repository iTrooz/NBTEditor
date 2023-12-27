FROM stateoftheartio/qt6:6.6-wasm-aqt

WORKDIR /home/user/project

# Install vcpkg
USER root
RUN apt update && apt install curl zip unzip tar pkg-config -y
USER user
RUN git clone --depth 1 https://github.com/microsoft/vcpkg /home/user/vcpkg && /home/user/vcpkg/bootstrap-vcpkg.sh
ENV PATH=$PATH:/home/user/vcpkg

# Setup ccache
USER root
RUN apt install -y ccache
USER user
ENV CCACHE_DIR /home/user/ccache

# Install zlib dep
RUN CC=emcc CXX=em++ vcpkg install --triplet=wasm32-emscripten zlib

COPY . .
RUN --mount=type=cache,target=/home/user/cache <<EOF
# Build NBTEditor
set -xe
ccache -zs
cmake \
    -DCMAKE_TOOLCHAIN_FILE=/home/user/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$QT_WASM/lib/cmake/Qt6/qt.toolchain.cmake \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DQT_HOST_PATH=$(dirname "$QT_WASM")/gcc_64 \
    -B build -G Ninja
cmake --build build -j 4
ccache -s
EOF


FROM scratch
COPY --from=0 [ \
    "/home/user/project/build/nbteditor.js", \
    "/home/user/project/build/nbteditor.html", \
    "/home/user/project/build/nbteditor.wasm", \
    "/home/user/project/build/qtloader.js", \
    "." \
]
