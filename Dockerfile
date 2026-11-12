# Building the server:
#   x64:   $ docker build -t imagename .
#   arm64: $ docker build --platform linux/arm64 -t imagename .
#   multi-platform (build and push):
#     $ docker buildx create --name st-server-multiarch --use
#     $ docker buildx build --platform linux/amd64,linux/arm64 -t imagename:tag --push .
#
# Build layout (fast rebuilds):
#   toolchain  compiler, git and xmake
#   manifest   copies ONLY the files xmake reads when configuring (*.lua, *.in), so the
#              next stage is cached until a build file changes (not on every commit)
#   deps       configures a throwaway project from those files, which installs every
#              xmake package (the slow part) into /root/.xmake and keeps it in this layer
#   builder    copies the real source and compiles only the project itself
# Do NOT add `RUN --mount=type=cache` over /root/.xmake in the builder stage: a cache mount
# hides the packages that the deps layer installed there.

FROM debian:12 AS toolchain

ENV DEBIAN_FRONTEND=noninteractive
ENV XMAKE_ROOT=y
SHELL ["/bin/bash", "-c"]

# Get packages and xmake

RUN apt-get update && apt-get install -y --no-install-recommends build-essential curl pkg-config git ca-certificates unzip libssl-dev && \
    rm -rf /var/lib/apt/lists/* && \
    curl -fsSL https://xmake.io/shget.text | bash

# Only the build description files, with their directory layout

FROM debian:12-slim AS manifest

# The build context is mounted read-only instead of copied, so its (large) contents, including the
# full git history, do not become a layer that would be exported to the layer cache on every push.
RUN --mount=type=bind,target=/ctx \
    mkdir /manifest && cd /ctx && \
    find . -path ./.git -prune -o -type f \( -name '*.lua' -o -name '*.in' \) -exec cp --parents {} /manifest \;

# Install every xmake package once. The project root here is a throwaway directory, so nothing
# of the real source tree (or its .git) is needed, and this layer only changes when a build
# file changes. A tiny git repo is created so that xmake's version variables can be resolved.

FROM toolchain AS deps

# Passed from the workflow; needed so that xmake always picks github.com as host
ARG GITHUB_ACTIONS

WORKDIR /deps
COPY --from=manifest /manifest/ /deps/
RUN git init -q . && \
    git -c user.email=ci@example.invalid -c user.name=ci commit -q --allow-empty -m deps && \
    git tag v0.0.0 && \
    source ~/.xmake/profile && \
    (xmake config -y -m release || echo "deps stage: xmake config failed; the builder stage will install the packages itself")

# Copy source and build

FROM deps AS builder

ARG GITHUB_ACTIONS

WORKDIR /src
COPY . /src

# Optional: make `git describe --tags` (used for the build/version string) report an exact tag, so a
# patched server still matches stock clients of that version (the client refuses mismatched versions).
ARG VERSION_OVERRIDE
RUN if [ -n "$VERSION_OVERRIDE" ]; then git -C /src tag -f "$VERSION_OVERRIDE"; fi

RUN source ~/.xmake/profile && \
    xmake config -y -m release && \
    xmake -y && \
    xmake install -y -o package

# Actual server runtime image; (todo: maybe reconsider 'distroless' in the future)

FROM debian:12-slim AS runtime

WORKDIR /st-server

COPY --from=builder \
    /src/package/lib/libSTServer.so \
    /src/package/bin/SkyrimTogetherServer \
    /st-server/

ENTRYPOINT ["./SkyrimTogetherServer"]
EXPOSE 10578/udp
