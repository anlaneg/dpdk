#! /bin/bash
#require gcc numactl-devel libnuma-dev
make config T=x86_64-native-linuxapp-gcc
make 
make install DESTDIR=build/x86_64-native-linuxapp-gcc

