FROM ubuntu:22.04

RUN apt update
RUN apt install -y qt6-base-dev
RUN apt install -y cmake ninja-build gcc g++
RUN apt install -y libgl-dev libvulkan-dev # idk why, but without this, QtWidgets is not found

RUN apt install zlib1g-dev

COPY . .

ARG BUILD_TYPE=Debug
RUN cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -B build -G "Ninja"
RUN cmake --build build


FROM scratch
COPY --from=0 build/nbteditor .
