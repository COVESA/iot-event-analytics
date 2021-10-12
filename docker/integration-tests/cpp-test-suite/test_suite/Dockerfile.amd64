FROM alpine:3.13 as base_stage

ARG HTTP_PROXY
ARG HTTPS_PROXY

ENV HTTP_PROXY=${HTTP_PROXY}
ENV HTTPS_PROXY=${HTTPS_PROXY}

# Install all build tools & certs for paho etc
RUN apk add --no-cache \
git \
cmake \
make \
g++ \
ca-certificates


# > > > > > > > > > > > > > > > > > > > > > STAGE 1: 
FROM base_stage as build_stage

# Create app directory
RUN mkdir -p /app
WORKDIR /app

# Copy C++ SDK sources
COPY /src/sdk/cpp ./src/sdk/cpp

ARG INTEGRATION_TESTS=/test/integration-tests

# Copy C++ integration-Test sources
COPY ${INTEGRATION_TESTS}/sdk-test-suites/cpp/ .${INTEGRATION_TESTS}/sdk-test-suites/cpp/
COPY ${INTEGRATION_TESTS}/config/tests/cpp/ .${INTEGRATION_TESTS}/config/tests/cpp/

# set the directory for building the cpp app's
WORKDIR /app${INTEGRATION_TESTS}/sdk-test-suites/cpp

# Compile/Build with cmake tools
RUN cmake -B build -S .
RUN cmake --build build --parallel --target testsuite_sdk


# > > > > > > > > > > > > > > > > > > > > > STAGE 2: 
FROM alpine:3.13 as runtime_stage

# Create app directory
RUN mkdir -p /app
WORKDIR /app

# we only want the bare minimum libraries for it to work in runtime
RUN apk add --no-cache libstdc++ libgcc

ARG INTEGRATION_TESTS=/test/integration-tests

# Copy in pre-built cpp exe and config from build stage
COPY --from=build_stage /app${INTEGRATION_TESTS}/sdk-test-suites/cpp/build .${INTEGRATION_TESTS}/sdk-test-suites/cpp/build
COPY --from=build_stage /app${INTEGRATION_TESTS}/config/tests/cpp/ .${INTEGRATION_TESTS}/config/tests/cpp/

WORKDIR /app${INTEGRATION_TESTS}/sdk-test-suites/cpp/build

CMD ./testsuite_sdk