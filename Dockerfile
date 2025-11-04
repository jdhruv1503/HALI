# HALI Research Project - Docker Environment
# Ubuntu 22.04 LTS with GCC 11.4.0+ for reproducible benchmarking

FROM ubuntu:22.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install essential build tools and dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc-11 \
    g++-11 \
    cmake \
    git \
    wget \
    curl \
    unzip \
    python3 \
    python3-pip \
    linux-tools-generic \
    linux-tools-common \
    libboost-all-dev \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 100 \
    && rm -rf /var/lib/apt/lists/*

# Install Python packages for data generation and visualization
RUN pip3 install numpy scipy matplotlib pandas

# Set working directory
WORKDIR /workspace

# Copy project files
COPY . /workspace/

# Expose the workspace
VOLUME ["/workspace"]

# Default command
CMD ["/bin/bash"]
