# Copyright 2021 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

ARG IMAGE
FROM ${IMAGE}
ARG PYTHON_VERSION
ARG NUMPY_VERSION

COPY update_sources.sh /
RUN /update_sources.sh

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC \
    apt-get install -y \
      build-essential \
      software-properties-common \
      zlib1g-dev  \
      curl \
      unzip \
      git \
      xxd && \
    apt-get clean

# Install Python packages.
RUN dpkg --add-architecture armhf
RUN dpkg --add-architecture arm64
RUN yes | add-apt-repository ppa:deadsnakes/ppa
RUN apt-get update && \
    apt-get install -y \
      python$PYTHON_VERSION \
      python$PYTHON_VERSION-dev \
      python$PYTHON_VERSION-venv \
      python$PYTHON_VERSION-distutils \
      libpython$PYTHON_VERSION-dev \
      libpython$PYTHON_VERSION-dev:armhf \
      libpython$PYTHON_VERSION-dev:arm64 \
      libusb-1.0-0-dev \
      libusb-1.0-0-dev:armhf \
      libusb-1.0-0-dev:arm64

RUN ln -sf /usr/bin/python$PYTHON_VERSION /usr/bin/python3
RUN curl -OL https://bootstrap.pypa.io/get-pip.py
RUN python3 get-pip.py
RUN rm get-pip.py
RUN pip3 install --upgrade pip
RUN pip3 install numpy~=$NUMPY_VERSION setuptools pybind11
RUN ln -sf /usr/include/python$PYTHON_VERSION /usr/include/python3
RUN ln -sf /usr/local/lib/python$PYTHON_VERSION/dist-packages/numpy/core/include/numpy /usr/include/python3/numpy
RUN ln -sf /usr/bin/python3 /usr/bin/python

ENV CI_BUILD_PYTHON=python$PYTHON_VERSION
ENV CROSSTOOL_PYTHON_INCLUDE_PATH=/usr/include/python$PYTHON_VERSION

COPY install_bazel.sh /
RUN /install_bazel.sh

COPY with_the_same_user /
