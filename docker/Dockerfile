FROM debian:buster

RUN : &&\
    apt-get update &&\
    apt-get install -y libaio-dev ninja-build ragel libhwloc-dev libnuma-dev libpciaccess-dev libcrypto++-dev libboost-all-dev libxen-dev libxml2-dev xfslibs-dev libgnutls28-dev liblz4-dev libsctp-dev gcc make libprotobuf-dev protobuf-compiler python3 libunwind8-dev systemtap-sdt-dev libtool g++ cmake vim less gdb pkg-config &&\
    apt-get clean &&\
    :

RUN : &&\
    adduser --uid=1001 --disabled-password dev &&\
    :

USER dev
WORKDIR /home/dev/seabrute
 
