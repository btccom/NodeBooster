#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cstdio>

#include <libconfig.h++>
#include <zmq.hpp>

using namespace std;
using namespace libconfig;

int main(int argc, char **argv) {
  //  Prepare our context and socket
    zmq::context_t context (1);
    zmq::socket_t socket (context, ZMQ_REP);
    socket.bind ("tcp://*:5555");

  printf("hello\n");

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

  return 0;
}

