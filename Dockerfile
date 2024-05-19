FROM ubuntu:22.04

RUN apt update && apt install apt-transport-https curl gnupg -y && \
    curl -fsSL https://bazel.build/bazel-release.pub.gpg | gpg --dearmor >bazel-archive-keyring.gpg &&\
    mv bazel-archive-keyring.gpg /usr/share/keyrings && \
    echo "deb [arch=amd64 signed-by=/usr/share/keyrings/bazel-archive-keyring.gpg] https://storage.googleapis.com/bazel-apt stable jdk1.8" | tee /etc/apt/sources.list.d/bazel.list && \
    apt update &&\
    apt install net-tools iproute2 can-utils bazel clang-15 -y

ADD docker-entrypoint.sh .

ENTRYPOINT ["sh", "docker-entrypoint.sh"]