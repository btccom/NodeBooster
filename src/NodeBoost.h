

#include "bitcoin/base58.h"
#include "bitcoin/core.h"
#include "bitcoin/util.h"

#include "zmq.hpp"

#include "Common.h"
#include "Utils.h"


#define MSG_PUB_THIN_BLOCK "BLOCK_THIN"


class NodePeer;
class TxRepo;
class NodeBoost;


/////////////////////////////////// NodePeer ///////////////////////////////////
class NodePeer {
  atomic<bool> running_;
  zmq::socket_t  *zmqSub_;  // sub-pub
  zmq::socket_t  *zmqReq_;  // req-rep

  string subAddr_;
  string reqAddr_;

  NodeBoost *nodeBoost_;

public:
  NodePeer(const string &subAddr, const string &reqAddr,
           zmq::context_t *zmqContext, NodeBoost *nodeBoost);
  ~NodePeer();

  bool isFinish();
  void stop();
  void run();
};


//////////////////////////////////// TxRepo ////////////////////////////////////
class TxRepo {
  mutex lock_;
  std::map<uint256, CTransaction> txsPool_;

public:
  TxRepo() {}

  bool getTx(const uint256 &hash, CTransaction &tx);
  void AddTx(const CTransaction &tx);
};


/////////////////////////////////// NodeBoost //////////////////////////////////
class NodeBoost {
  atomic<bool> running_;

  zmq::context_t zmqContext_;
  zmq::socket_t *zmqRep_;  // response
  zmq::socket_t *zmqPub_;  // publish

  string zmqPubAddr_;
  string zmqRepAddr_;

  TxRepo *txRepo_;

  std::set<NodePeer *> peers_;

  void peerConnect(const string &subAddr, const string &reqAddr);
  void peerCloseAll();

  void threadZmqPublish();
  void threadZmqResponse();

public:
  NodeBoost(const string &zmqPubAddr, const string &zmqRepAddr, TxRepo *txRepo);
  ~NodeBoost();

  void run();
  void stop();
  void submitBlock(const string &block);
};

