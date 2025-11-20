FROM ubuntu:noble AS base
RUN apt-get update && apt-get install -y liblzfse1

FROM base AS build
RUN apt-get install -y g++ cmake make libbz2-dev libz-dev liblzma-dev liblzfse-dev
RUN mkdir -p /source
COPY CMakeLists.txt /source
COPY common /source/common
COPY includes /source/includes
COPY hdutil /source/hdutil
COPY dmg /source/dmg
COPY hfs /source/hfs
COPY cmake /source/cmake
RUN cmake -B /build -DCMAKE_BUILD_TYPE=Release /source
RUN make -C /build -j$(nproc)

FROM base AS test
RUN apt-get install -y pipx perl xxd
# We never want to actually make anything
RUN ln -s /bin/true /bin/make
RUN pipx install cram
ENV PATH="/usr/bin:/root/.local/bin"
RUN mkdir -p /test/build/dmg /test/build/hfs
COPY --from=build /build/dmg/dmg /test/build/dmg/dmg
COPY --from=build /build/hfs/hfsplus /test/build/hfs/hfsplus
COPY test /test/test
WORKDIR /test
CMD ["/bin/sh", "-c", "cram test/*.t"]
