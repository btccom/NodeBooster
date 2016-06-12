
#include "NodeBoost.h"

#include <boost/thread.hpp>
#include <glog/logging.h>


//  Receive 0MQ string from socket and convert into string
static std::string
s_recv (zmq::socket_t & socket) {
  zmq::message_t message;
  socket.recv(&message);

  return std::string(static_cast<char*>(message.data()), message.size());
}

//  Convert string to 0MQ string and send to socket
static bool
s_send(zmq::socket_t & socket, const std::string & string) {
  zmq::message_t message(string.size());
  memcpy (message.data(), string.data(), string.size());

  bool rc = socket.send (message);
  return (rc);
}

//  Sends string as 0MQ string, as multipart non-terminal
static bool
s_sendmore (zmq::socket_t & socket, const std::string & string) {
  zmq::message_t message(string.size());
  memcpy (message.data(), string.data(), string.size());

  bool rc = socket.send (message, ZMQ_SNDMORE);
  return (rc);
}




/////////////////////////////////// NodePeer ///////////////////////////////////
NodePeer::NodePeer(const string &subAddr, const string &reqAddr,
                   zmq::context_t *zmqContext, NodeBoost *nodeBoost)
:running_(true), subAddr_(subAddr), reqAddr_(reqAddr), nodeBoost_(nodeBoost)
{
  zmqSub_ = new zmq::socket_t(*zmqContext, ZMQ_SUB);
  zmqSub_->connect(subAddr_);
  zmqSub_->setsockopt(ZMQ_SUBSCRIBE, MSG_PUB_THIN_BLOCK, strlen(MSG_PUB_THIN_BLOCK));

  zmqReq_ = new zmq::socket_t(*zmqContext, ZMQ_REQ);
  zmqReq_->connect(reqAddr_);

  int zmqLinger = 5 * 1000;
  zmqSub_->setsockopt(ZMQ_LINGER, &zmqLinger/*ms*/, sizeof(zmqLinger));
  zmqReq_->setsockopt(ZMQ_LINGER, &zmqLinger/*ms*/, sizeof(zmqLinger));
}

NodePeer::~NodePeer() {
  LOG(INFO) << "close peer: " << subAddr_ << ", " << reqAddr_;
}

void NodePeer::stop() {
  if (!running_) {
    return;
  }
  running_ = false;

  LOG(INFO) << "stop peer: " << subAddr_ << ", " << reqAddr_;
  zmqSub_->close();
  zmqReq_->close();
  zmqSub_ = nullptr;
  zmqReq_ = nullptr;
}

bool NodePeer::isFinish() {
  if (running_ == false && zmqSub_ == nullptr && zmqReq_ == nullptr) {
    return true;
  }
  return false;
}

void NodePeer::run() {
  while (running_) {
    string smsg;
    // sub
    {
      smsg = s_recv(*zmqSub_);
      LOG(INFO) << "[sub, " << subAddr_ << "] recv: " << smsg;

      smsg = s_recv(*zmqSub_);
      LOG(INFO) << "[sub, " << subAddr_ << "] recv: " << smsg;
    }

    // TODO: handle message

    // req
    s_send(*zmqReq_, "hello");
    smsg = s_recv(*zmqReq_);
    LOG(INFO) << "[req, " << reqAddr_ << "] recv: " << smsg;

    string block(smsg);
    nodeBoost_->submitBlock(block);
  }
}


//////////////////////////////////// TxRepo ////////////////////////////////////
void TxRepo::AddTx(const CTransaction &tx) {
  ScopeLock sl(lock_);

  const uint256 hash = tx.GetHash();
  if (txsPool_.count(hash)) {
    return;
  }
  txsPool_.insert(std::make_pair(hash, tx));
}

bool TxRepo::getTx(const uint256 &hash, CTransaction &tx) {
  ScopeLock sl(lock_);

  auto it = txsPool_.find(hash);
  if (it == txsPool_.end()) {
    return false;
  }
  tx = it->second;
  return true;
}


//////////////////////////////////// NodeBoost ////////////////////////////////////
NodeBoost::NodeBoost(const string &zmqPubAddr, const string &zmqRepAddr,
                     TxRepo *txRepo)
: zmqContext_(2/*i/o threads*/),
txRepo_(txRepo), zmqPubAddr_(zmqPubAddr), zmqRepAddr_(zmqRepAddr)
{
  // publisher
  zmqPub_ = new zmq::socket_t(zmqContext_, ZMQ_PUB);
  zmqPub_->bind(zmqPubAddr_);

  // response
  zmqRep_ = new zmq::socket_t(zmqContext_, ZMQ_REP);
  zmqRep_->bind(zmqRepAddr_);
}

NodeBoost::~NodeBoost() {
  zmqRep_->close();
  zmqPub_->close();
  zmqRep_ = nullptr;
  zmqPub_ = nullptr;
}

void NodeBoost::stop() {
  if (!running_) {
    return;
  }
  running_ = false;

  peerCloseAll();
}

void NodeBoost::threadZmqPublish() {
  while (running_) {
    s_sendmore(*zmqPub_, MSG_PUB_THIN_BLOCK);
    s_send(*zmqPub_, "We do want to see this");

    sleep(3);
  }
}

void NodeBoost::threadZmqResponse() {
  while (running_) {
    string smsg;
    smsg = s_recv(*zmqRep_);
    LOG(INFO) << "[NodeBoost::threadZmqResponse] recv: " << smsg;

    //  Send reply back to client
    s_send(*zmqRep_, "world");
  }
}

void NodeBoost::run() {
  boost::thread t1(boost::bind(&NodeBoost::threadZmqPublish,  this));
  boost::thread t2(boost::bind(&NodeBoost::threadZmqResponse, this));

  while (running_) {
    sleep(1);
  }
}

void NodeBoost::peerConnect(const string &subAddr, const string &reqAddr) {
  if (running_ == false) {
    LOG(WARNING) << "server has stopped, ignore to connect peer: " << subAddr << ", " << reqAddr;
    return;
  }
  NodePeer *peer = new NodePeer(subAddr, reqAddr, &zmqContext_, this);
  peers_.insert(peer);

  LOG(INFO) << "connect peer: " << subAddr << ", " << reqAddr;
  peer->run();
}

void NodeBoost::peerCloseAll() {
  for (auto it : peers_) {
    it->stop();
  }

  while (peers_.size()) {
    auto it = peers_.begin();
    if (!(*it)->isFinish()) {
      sleep(1);
      continue;
    }
    peers_.erase(it);
  }
}

void NodeBoost::submitBlock(const string &block) {
  LOG(INFO) << "NodeBoost::submitBlock, " << block;
}

