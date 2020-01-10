FROM ubuntu:18.04 as build-py2

## Prepare Environment
RUN apt-get update -qq  \
    && apt-get install -y -qq --no-install-recommends  \
      curl  \
      gnupg  \
      locales  \
      software-properties-common \
  && curl -fsSL http://mirror2.openio.io/pub/repo/openio/APT-GPG-KEY-OPENIO-0 | apt-key add - \
  && apt-add-repository "deb http://mirror.openio.io/pub/repo/openio/sds/18.04/ubuntu/ bionic/" \
  ## Install Dev Packages
  && apt-get update \
  && apt-get install -y --no-install-recommends \
    apache2 \
    apache2-dev \
    asn1c \
    attr \
    beanstalkd \
    bison \
    cmake \
    curl \
    flex \
    gdb \
    git \
    golang \
    lcov \
    libapache2-mod-wsgi \
    libapreq2-dev \
    libattr1-dev \
    libcurl4-gnutls-dev \
    liberasurecode-dev \
    libglib2.0-dev \
    libjson-c-dev \
    libleveldb-dev \
    liblzo2-dev \
    libsqlite3-dev \
    libzmq3-dev \
    libzookeeper-mt-dev \
    make \
    openio-gridinit \
    python-all-dev \
    python-dev \
    python-pbr \
    python-pip \
    python-setuptools \
    python-virtualenv \
    python3 \
    python3-coverage \
    python3-dev \
    redis-server \
    redis-tools \
    sqlite3 \
    zookeeper \
    zookeeper-bin \
    zookeeperd \
  ## Build Go environment
  && go get gopkg.in/ini.v1 golang.org/x/sys/unix

WORKDIR /root/oio

## Build Python Environment
RUN pip install --upgrade pip setuptools virtualenv tox zkpython \
  && virtualenv /root/venv && . /root/venv/bin/activate
COPY ./all-requirements.txt ./test-requirements.txt /root/oio/
RUN pip install --upgrade -r all-requirements.txt -r test-requirements.txt

VOLUME ["/root/venv"]

ENV G_DEBUG=fatal_warnings \
  ZK=127.0.0.1:2181 \
  G_DEBUG_LEVEL=W \
  LD_LIBRARY_PATH=/tmp/oio/lib \
  PKG_CONFIG_PATH=/tmp/oio/lib/pkgconfig \
  REPO_PATH=/root/oio \
  CMAKE_OPTS="-DENABLE_CODECOVERAGE=on -DCMAKE_INSTALL_PREFIX=/tmp/oio -DLD_LIBDIR=lib -DZK_LIBDIR=/usr/lib -DZK_INCDIR=/usr/include/zookeeper"

COPY . "${REPO_PATH}"
RUN bash -c "echo '/tmp/core.%p.%E' > /proc/sys/kernel/core_pattern" || true
RUN bash ./tools/oio-build-sdk.sh "${REPO_PATH}"
RUN bash ./tools/oio-build-release.sh "${REPO_PATH}"
RUN bash ./tools/oio-check-copyright.sh "${REPO_PATH}"
RUN python /root/oio/setup.py develop
RUN tox -e variables
RUN tox -e py3_variables
