
Library

* openssl
* libconfig++

zmq 4.1.4

```
git clone https://github.com/zeromq/libzmq
./autogen.sh && ./configure && make -j 4
make check && make install && ldconfig
```

google glog
```
wget https://github.com/google/glog/archive/v0.3.4.tar.gz
tar zxvf v0.3.4.tar.gz
cd glog-0.3.4
./configure && make && make install
```
