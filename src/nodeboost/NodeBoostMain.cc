/*
 The MIT License (MIT)

 Copyright (c) [2016] [BTC.COM]

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
*/
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>

#include <iostream>

#include <glog/logging.h>
#include <libconfig.h++>

#include "zmq.hpp"

#include "Utils.h"
#include "NodeBoost.h"

using namespace std;
using namespace libconfig;

NodeBoost *gNodeBoost = nullptr;

void handler(int sig) {
  if (gNodeBoost) {
    gNodeBoost->stop();
  }
}

void usage() {
  fprintf(stderr, "Usage:\n\tnodeboost -c \"nodeboost.cfg\" -l \"log_dir\"\n");
}

int main(int argc, char **argv) {
  char *optLogDir = NULL;
  char *optConf   = NULL;
  int c;

  if (argc <= 1) {
    usage();
    return 1;
  }
  while ((c = getopt(argc, argv, "c:l:h")) != -1) {
    switch (c) {
      case 'c':
        optConf = optarg;
        break;
      case 'l':
        optLogDir = optarg;
        break;
      case 'h': default:
        usage();
        exit(0);
    }
  }

  // Initialize Google's logging library.
  google::InitGoogleLogging(argv[0]);
  FLAGS_log_dir = string(optLogDir);
  FLAGS_logbuflevel = -1;

  // Read the file. If there is an error, report it and exit.
  Config cfg;
  try
  {
    cfg.readFile(optConf);
  } catch(const FileIOException &fioex) {
    std::cerr << "I/O error while reading file." << std::endl;
    return(EXIT_FAILURE);
  } catch(const ParseException &pex) {
    std::cerr << "Parse error at " << pex.getFile() << ":" << pex.getLine()
    << " - " << pex.getError() << std::endl;
    return(EXIT_FAILURE);
  }

  signal(SIGTERM, handler);
  signal(SIGINT,  handler);

  TxRepo txrepo;
  gNodeBoost = new NodeBoost(cfg.lookup("server.publish_addr"),
                             cfg.lookup("server.response_addr"),
                             cfg.lookup("bitcoind.sub_addr"),
                             cfg.lookup("bitcoind.rpc_addr"),
                             cfg.lookup("bitcoind.rpc_userpwd"),
                             &txrepo);

  try {
    // connect peers in config file
    {
      const Setting &peers = cfg.lookup("peers");
      for (int i = 0; i < peers.getLength(); i++) {
        string subAddr, reqAddr;
        peers[i].lookupValue("publish_addr",  subAddr);
        peers[i].lookupValue("response_addr", reqAddr);
        gNodeBoost->peerConnect(subAddr, reqAddr);
      }
    }

    gNodeBoost->run();

    gNodeBoost->stop();
    delete gNodeBoost;
  } catch (std::exception & e) {
    LOG(FATAL) << "exception: " << e.what();
    return 1;
  }

  return 0;
}
