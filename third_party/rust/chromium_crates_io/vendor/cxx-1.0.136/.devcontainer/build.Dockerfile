FROM mcr.microsoft.com/vscode/devcontainers/rust:1

RUN apt-get update \
    && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends openjdk-11-jdk lld \
    && rustup default nightly 2>&1 \
    && rustup component add rust-analyzer-preview rustfmt clippy 2>&1 \
    && wget -q -O bin/install-bazel https://github.com/bazelbuild/bazel/releases/download/4.0.0/bazel-4.0.0-installer-linux-x86_64.sh \
    && wget -q -O bin/buck https://jitpack.io/com/github/facebook/buck/a5f0342ae3/buck-a5f0342ae3-java11.pex \
    && wget -q -O bin/buildifier https://github.com/bazelbuild/buildtools/releases/latest/download/buildifier \
    && wget -q -O tmp/watchman.zip https://github.com/facebook/watchman/releases/download/v2020.09.21.00/watchman-v2020.09.21.00-linux.zip \
    && chmod +x bin/install-bazel bin/buck bin/buildifier \
    && bin/install-bazel \
    && unzip tmp/watchman.zip -d tmp \
    && mv tmp/watchman-v2020.09.21.00-linux/bin/watchman bin \
    && mv tmp/watchman-v2020.09.21.00-linux/lib/* /usr/local/lib \
    && mkdir -p /usr/local/var/run/watchman \
    && rm tmp/watchman.zip
