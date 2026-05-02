FROM gcc:11.3 as build

RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
      libboost-log-dev \
      libboost-thread-dev \
      libboost-system-dev && \
    pip3 install conan==1.*

COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing \
    -s build_type=Release \
    -s compiler.libcxx=libstdc++11

COPY ./src /app/src
COPY ./tests /app/tests
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF .. && \
    cmake --build .

FROM ubuntu:22.04 as run

RUN apt-get update && apt-get install -y --no-install-recommends libpq5 \
    && rm -rf /var/lib/apt/lists/*

RUN groupadd -r www && useradd -r -g www www

WORKDIR /app

COPY --from=build /app/build/game_server /app/
COPY ./data /app/data
COPY static /app/static

USER www

ENTRYPOINT ["/app/game_server", "-c", "/app/data/config.json", "-w", "/app/static"]
