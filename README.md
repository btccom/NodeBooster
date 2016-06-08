
Library

* openssl
* libconfig++

zmq 4.1.4

```
git clone https://github.com/zeromq/libzmq
./autogen.sh && ./configure && make -j 4
make check && make install && ldconfig
```
