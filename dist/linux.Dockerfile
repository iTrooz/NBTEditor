FROM ubuntu:22.04

RUN apt update
RUN apt install -y qt6-base-dev
RUN apt install -y cmake make gcc g++
RUN apt install -y libgl-dev libvulkan-dev # idk why, but without this, QtWidgets is not found

RUN apt install zlib1g-dev

COPY . .

RUN cmake -B build
RUN cmake --build build -j 4


FROM scratch
COPY --from=0 build/nbteditor .
