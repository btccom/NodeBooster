#
# Dockerfile
#
# @author zhibiao.pan@bitmain.com
# @copyright BTC.COM
# @since 2016-06-17
#
FROM phusion/baseimage:0.9.18
MAINTAINER PanZhibiao <zhibiao.pan@bitmain.com>

ENV HOME /root
ENV TERM xterm
CMD ["/sbin/my_init"]

RUN apt-get update
RUN apt-get install -y zsh git cmake build-essential wget
RUN apt-get install -y openssl libssl-dev libcurl4-openssl-dev libzmq3-dev libconfig++-dev

# oh my zsh
ADD oh-my-zsh.tar.gz /root/oh-my-zsh.tar.gz
RUN cp -r /root/oh-my-zsh.tar.gz/.oh-my-zsh ~/.oh-my-zsh \
  && cp ~/.oh-my-zsh/templates/zshrc.zsh-template ~/.zshrc \
  && chsh -s /bin/zsh \
  && mkdir /root/source

# install glog
RUN cd /root/source \
  && wget https://github.com/google/glog/archive/v0.3.4.tar.gz \
  && tar zxf v0.3.4.tar.gz \
  && cd glog-0.3.4 \
  && ./configure && make && make install

RUN apt-get install -y libboost-all-dev

# build
RUN cd /root \
  && git clone https://github.com/btccom/NodeBooster.git \
  && cd NodeBooster \
  && git checkout develop \
  && mkdir build \
  && cd build \
  && cmake ..\
  && make

