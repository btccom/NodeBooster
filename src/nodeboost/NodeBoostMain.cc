#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cstdio>

#include <glog/logging.h>
#include <libconfig.h++>

#include "zmq.hpp"

#include "Utils.h"
#include "NodeBoost.h"

using namespace std;
using namespace libconfig;

//  Receive 0MQ string from socket and convert into string
static std::string
s_recv (zmq::socket_t & socket) {

  zmq::message_t message;
  socket.recv(&message);

  return std::string(static_cast<char*>(message.data()), message.size());
}

int main(int argc, char **argv) {
  // Initialize Google's logging library.
  google::InitGoogleLogging(argv[0]);
  FLAGS_log_dir = "./log";

  LOG(INFO) << "NodeBoost";

  Config cfg;
  // Read the file. If there is an error, report it and exit.
  try
  {
    cfg.readFile("example.cfg");
  }
  catch(const FileIOException &fioex)
  {
    std::cerr << "I/O error while reading file." << std::endl;
    return(EXIT_FAILURE);
  }
  catch(const ParseException &pex)
  {
    std::cerr << "Parse error at " << pex.getFile() << ":" << pex.getLine()
    << " - " << pex.getError() << std::endl;
    return(EXIT_FAILURE);
  }

  //  Prepare our context and subscriber
  zmq::context_t context(1);
  zmq::socket_t subscriber(context, ZMQ_SUB);
  subscriber.connect("tcp://10.45.15.220:10000");
  subscriber.setsockopt(ZMQ_SUBSCRIBE, "rawblock", 8);
  subscriber.setsockopt(ZMQ_SUBSCRIBE, "rawtx",    5);

  while (1) {
    //  Read message contents
    std::string type    = s_recv(subscriber);
    std::string content = s_recv(subscriber);

    std::cout << "[" << type << "]: " << content.length() << std::endl;

    if (type == "rawtx") {
      CTransaction tx;
      DecodeHexTx(tx, (const unsigned char *)content.data(), content.length());
      cout << "tx: " << tx.GetHash().ToString() << endl << EncodeHexTx(tx) << endl;
    }
//    std::cout << "[" << type << "]: " << content << std::endl;
  }

  return 0;
}
