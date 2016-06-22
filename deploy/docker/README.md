Docker for NodeBooster
======================

## Install Docker

* OS: Ubuntu 14.04 LTS

```
wget -qO- https://get.docker.com/ | sh
service docker start
service docker status
```

## build docker

```
cd /work

git clone https://github.com/btccom/NodeBooster.git
cd NodeBooster/deploy/docker

docker build -t nodebooster:0.1 .
# build without cache
# docker build --no-cache -t nodebooster:0.1 .
```

## run docker

```
# dir for config file and log
mkdir /work/NodeBooster/run_dir

# stat docker
docker run -it -v /work/NodeBooster/run_dir:/root/nodebooster_run_dir \
 --name nodebooster -p 20000:20000 -p 20001:20001 --restart always -d nodebooster:0.1
```
