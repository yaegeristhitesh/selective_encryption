FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Use a faster Indian mirror
RUN sed -i 's|http://archive.ubuntu.com|http://mirror.cse.iitk.ac.in/ubuntu|g' /etc/apt/sources.list

# Install required packages
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libopencv-dev \
    libssl-dev \
    libeigen3-dev \
    ffmpeg \
    libavcodec-dev \
    libavformat-dev \
    libswscale-dev \
    libavutil-dev \
    python3.10 \
    python3-pip \
    && apt-get clean

# Set Linux-friendly include and lib paths
ENV OPENSSL_CFLAGS="-I/usr/include/openssl"
ENV OPENSSL_LIBS="-lssl -lcrypto"
ENV EIGEN_CFLAGS="-I/usr/include/eigen3"
ENV CXXFLAGS="-std=c++17 -fPIC -Icodec -I/usr/include/opencv4 ${OPENSSL_CFLAGS} ${EIGEN_CFLAGS} $(pkg-config --cflags libavformat libavcodec libswscale libavutil opencv4)"
ENV LDFLAGS="$(pkg-config --libs libavformat libavcodec libswscale libavutil)"
ENV OPENCV_LIBS="$(pkg-config --libs opencv4)"

# Set working directory
WORKDIR /app

# Copy entire project
COPY . .

# Install pycryptodomex and your Python tool
RUN pip3 install --no-cache-dir pycryptodomex
RUN cd encryption_schemes/scheme2 && pip3 install .

# Build C++ binaries
RUN make clean && make

# Set library path and default run command
ENV LD_LIBRARY_PATH="/app/build/lib"
CMD ["./run"]
