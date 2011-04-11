#!/usr/bin/env sh

cd c_src/snappy && [ ! -f libsnappy.la ] && ./configure --with-pic && make
cd "$pwd"
