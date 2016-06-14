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

#include <boost/thread.hpp>
#include <glog/logging.h>

#include "NodeBoost.h"

#include "bitcoin/util.h"

uint256 getHashFromThinBlock(const string &thinBlock) {
  CBlockHeader header;
  memcpy((uint8_t *)&header, thinBlock.data(), sizeof(CBlockHeader));
  return header.GetHash();
}

/////////////////////////////////// NodePeer ///////////////////////////////////
NodePeer::NodePeer(const string &subAddr, const string &reqAddr,
                   zmq::context_t *zmqContext, NodeBoost *nodeBoost,
                   TxRepo *txRepo)
:running_(true), subAddr_(subAddr), reqAddr_(reqAddr), nodeBoost_(nodeBoost), txRepo_(txRepo)
{
  zmqSub_ = new zmq::socket_t(*zmqContext, ZMQ_SUB);
  zmqSub_->connect(subAddr_);
  zmqSub_->setsockopt(ZMQ_SUBSCRIBE, MSG_PUB_THIN_BLOCK, strlen(MSG_PUB_THIN_BLOCK));
  zmqSub_->setsockopt(ZMQ_SUBSCRIBE, MSG_PUB_HEARTBEAT,  strlen(MSG_PUB_HEARTBEAT));

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

void NodePeer::tellPeerToConnectMyServer(const string &zmqPubAddr,
                                         const string &zmqRepAddr) {
  // 1. send my zmq addr info
  assert(sizeof(uint16_t) == 2);
  zmq::message_t zmsg;
  zmsg.rebuild(MSG_CMD_LEN + 2 + zmqPubAddr.length() + 2 + zmqRepAddr.length());
  uint8_t *p = (uint8_t *)zmsg.data();

  // cmd
  memset((char *)p, 0, MSG_CMD_LEN);
  sprintf((char *)p, "%s", MSG_CMD_CONNECT_PEER);
  p += MSG_CMD_LEN;

  // pub addr
  *(uint16_t *)p = (uint16_t)zmqPubAddr.length();
  p += 2;
  memcpy((char *)p, zmqPubAddr.c_str(), zmqPubAddr.length());
  p += zmqPubAddr.length();

  // req addr
  *(uint16_t *)p = (uint16_t)zmqRepAddr.length();
  p += 2;
  memcpy((char *)p, zmqRepAddr.c_str(), zmqRepAddr.length());
  p += zmqRepAddr.length();
  assert((p - (uint8_t *)zmsg.data()) == zmsg.size());

  zmqReq_->send(zmsg);

  // 2. recv response. req-rep model need to call recv().
  string smsg = s_recv(*zmqReq_);
  DLOG(INFO) << smsg;
}

bool NodePeer::isFinish() {
  if (running_ == false && zmqSub_ == nullptr && zmqReq_ == nullptr) {
    return true;
  }
  return false;
}

void NodePeer::sendMissingTxs(const vector<uint256> &missingTxs) {
  zmq::message_t zmsg;
  zmsg.rebuild(MSG_CMD_LEN + missingTxs.size() * 32);

  memset((char *)zmsg.data(), 0, MSG_CMD_LEN);
  sprintf((char *)zmsg.data(), "%s", MSG_CMD_GET_TXS);

  memcpy((unsigned char *)zmsg.data() + MSG_CMD_LEN,
         missingTxs.data(), missingTxs.size() * 32);
  zmqReq_->send(zmsg);
}

void NodePeer::recvMissingTxs() {
  zmq::message_t zmsg;
  zmqReq_->recv(&zmsg);

  unsigned char *p = (unsigned char *)zmsg.data();
  unsigned char *e = (unsigned char *)zmsg.data() + zmsg.size();
  while (p < e) {
    // size
    const int32_t txlen = *(int32_t *)p;
    p += sizeof(int32_t);

    // tx content
    CTransaction tx;
    DecodeBinTx(tx, p, txlen);
    p += txlen;

    txRepo_->AddTx(tx);
  }
}

bool NodePeer::buildBlock(const string &thinBlock, CBlock &block) {
  const size_t txCnt = (thinBlock.size() - sizeof(CBlockHeader)) / 32;
  unsigned char *p = (unsigned char *)thinBlock.data() + sizeof(CBlockHeader);

  CBlockHeader header;
  memcpy((uint8_t *)&header, thinBlock.data(), sizeof(CBlockHeader));

  block.SetNull();
  block = header;

  // txs
  for (size_t i = 0; i < txCnt; i++) {
    std::vector<unsigned char> vch(p, p + 32);
    p += 32;

    uint256 hash(vch);
    CTransaction tx;
    if (!txRepo_->getTx(hash, tx)) {
      LOG(FATAL) << "missing tx when build block, tx: " << hash.ToString();
      return false;
    }
    block.vtx.push_back(tx);
  }
  return true;
}

void NodePeer::run() {
  while (running_) {

    // sub
    string type    = s_recv(*zmqSub_);
    string content = s_recv(*zmqSub_);

    // MSG_PUB_THIN_BLOCK
    if (type == MSG_PUB_THIN_BLOCK)
    {
      const uint256 blkhash = getHashFromThinBlock(content);
      LOG(INFO) << "received thin block, size: " << content.size()
      << ", hash: " << blkhash.ToString();

      vector<uint256> missingTxs;
      nodeBoost_->findMissingTxs(content, missingTxs);

      if (missingTxs.size() > 0) {
        LOG(INFO) << "request 'get_txs', missing tx count: " << missingTxs.size();
        // send cmd: "get_txs"
        sendMissingTxs(missingTxs);
        recvMissingTxs();
      }

      // submit block
      CBlock block;
      if (buildBlock(content, block)) {
        assert(blkhash == block.GetHash());
        nodeBoost_->foundNewBlock(block);
      } else {
        string hex;
        Bin2Hex((uint8 *)content.data(), content.size(), hex);
        LOG(ERROR) << "build block failure, hex: " << hex;
      }
    }
    else if (type == MSG_PUB_HEARTBEAT)
    {
      LOG(INFO) << "received heartbeat from: " << subAddr_ << ", content: " << content;
    }
    else
    {
      LOG(ERROR) << "unknown message type: " << type;
    }

  } /* /while */
}


//////////////////////////////////// TxRepo ////////////////////////////////////
TxRepo::TxRepo(): lock_() {
}
TxRepo::~TxRepo() {
}

bool TxRepo::isExist(const uint256 &hash) {
  ScopeLock sl(lock_);
  if (txsPool_.count(hash)) {
    return true;
  }
  return false;
}

void TxRepo::AddTx(const CTransaction &tx) {
  ScopeLock sl(lock_);

  const uint256 hash = tx.GetHash();
  if (txsPool_.count(hash)) {
    return;
  }
  txsPool_.insert(std::make_pair(hash, tx));
//  LOG(INFO) << "tx repo add tx: " << hash.ToString();
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
                     const string &zmqBitcoind,
                     const string &bitcoindRpcAddr, const string &bitcoindRpcUserpass,
                     TxRepo *txRepo)
: zmqContext_(2/*i/o threads*/),
txRepo_(txRepo), zmqPubAddr_(zmqPubAddr), zmqRepAddr_(zmqRepAddr), zmqBitcoind_(zmqBitcoind),
bitcoindRpcAddr_(bitcoindRpcAddr), bitcoindRpcUserpass_(bitcoindRpcUserpass)
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

void NodeBoost::findMissingTxs(const string &thinBlock,
                               vector<uint256> &missingTxs) {
  const size_t kHeaderSize = 80;
  assert(kHeaderSize == sizeof(CBlockHeader));
  assert(thinBlock.size() > kHeaderSize);

  uint8_t *p = (uint8_t *)thinBlock.data() + kHeaderSize;
  uint8_t *e = (uint8_t *)thinBlock.data() + thinBlock.size();
  assert((e - p) % 32 == 0);

  while (p < e) {
    std::vector<unsigned char> vch(p, p + 32);
    p += 32;

    uint256 hash(vch);
    LOG(INFO) << "check tx: " << hash.ToString();

    if (!txRepo_->isExist(hash)) {
      missingTxs.push_back(hash);
    }
  }
}

void NodeBoost::handleGetTxs(const zmq::message_t &zin, zmq::message_t &zout) {
  uint8_t *p = (uint8_t *)zin.data() + MSG_CMD_LEN;
  uint8_t *e = (uint8_t *)zin.data() + zin.size();
  assert((e - p) % 32 == 0);

  zmq::message_t ztmp(4*1024*1024);  // 4MB, TODO: resize in the future
  uint8_t *data = (uint8_t *)ztmp.data();

  while (p < e) {
    std::vector<unsigned char> vch(p, p + 32);
    p += 32;

    uint256 hash(vch);
    CTransaction tx;
    if (!txRepo_->getTx(hash, tx)) {
      LOG(INFO) << "missing tx: " << hash.ToString();
    }

    CDataStream ssTx(SER_NETWORK, BITCOIN_PROTOCOL_VERSION);
    ssTx << tx;
    const size_t txSize = ssTx.end() - ssTx.begin();

    // size
    *(int32_t *)data = (int32_t)txSize;
    data += sizeof(int32_t);

    // tx
    memcpy(data, &(*ssTx.begin()), txSize);
    data += txSize;
  }

  zout.rebuild(ztmp.data(), data - (uint8_t *)ztmp.data());
}

void NodeBoost::handleConnPeer(const zmq::message_t &zin) {
  uint8_t *p = (uint8_t *)zin.data() + MSG_CMD_LEN;
  string zmqPubAddr, zmqRepAddr;
  uint16_t lenp, lenr;

  lenp = *(uint16_t *)p;
  zmqPubAddr.assign((char *)(p + 2), (size_t)lenp);
  p += (2 + lenp);

  lenr = *(uint16_t *)p;
  zmqRepAddr.assign((char *)(p + 2), (size_t)lenr);
  p += (2 + lenr);

  assert((p - (uint8_t *)zin.data()) == zin.size());

  // connect peer
  peerConnect(zmqPubAddr, zmqRepAddr);
}

void NodeBoost::threadZmqResponse() {
  while (running_) {
    zmq::message_t zmsg;
    zmqRep_->recv(&zmsg);
    assert(zmsg.size() >= MSG_CMD_LEN);

    // cmd
    char cmd[MSG_CMD_LEN];
    snprintf(cmd, sizeof(cmd), "%s", (char *)zmsg.data());
    LOG(INFO) << "receive cmd: " << cmd;

    // handle message
    if (strncmp(cmd, MSG_CMD_GET_TXS, MSG_CMD_LEN) == 0)
    {
      zmq::message_t zout;
      handleGetTxs(zmsg, zout);
      zmqRep_->send(zout);
    }
    else if (strncmp(cmd, MSG_CMD_CONNECT_PEER, MSG_CMD_LEN) == 0)
    {
      handleConnPeer(zmsg);
      s_send(*zmqRep_, "ACK");  // send anything
    }
    else
    {
      s_send(*zmqRep_, "unknown");
      LOG(ERROR) << "unknown cmd: " << cmd;
    }
  }
}

static void _buildMsgThinBlock(const CBlock &block, zmq::message_t &zmsg) {
  const size_t kHeaderSize = 80;
  assert(kHeaderSize == sizeof(CBlockHeader));

  zmsg.rebuild(kHeaderSize + block.vtx.size() * 32);
  unsigned char *p = (unsigned char *)zmsg.data();

  CBlockHeader header = block.GetBlockHeader();
  memcpy(p, (uint8 *)&header, sizeof(CBlockHeader));
  p += kHeaderSize;

  for (size_t i = 0; i < block.vtx.size(); i++) {
    uint256 hash = block.vtx[i].GetHash();
    memcpy(p, hash.begin(), 32);
    p += 32;
  }
}

void NodeBoost::threadListenBitcoind() {
  zmq::socket_t subscriber(zmqContext_, ZMQ_SUB);
  subscriber.connect(zmqBitcoind_);
  subscriber.setsockopt(ZMQ_SUBSCRIBE, "rawblock", 8);
  subscriber.setsockopt(ZMQ_SUBSCRIBE, "rawtx",    5);

  while (running_) {
    std::string type    = s_recv(subscriber);
    std::string content = s_recv(subscriber);

    if (type == "rawtx") {
      CTransaction tx;
      DecodeBinTx(tx, (const unsigned char *)content.data(), content.length());
      txRepo_->AddTx(tx);
    }
    else if (type == "rawblock")
    {
      CBlock block;
      DecodeBinBlk(block, (const unsigned char *)content.data(), content.length());
      LOG(INFO) << "found block: " << block.GetHash().ToString()
      << ", tx count: " << block.vtx.size() << ", size: " << content.length();

      {
        // don't submit block to the bitcoind self
        ScopeLock sl(historyLock_);
        bitcoindBlockHistory_.insert(block.GetHash());
      }
      foundNewBlock(block);
    }
    else
    {
      LOG(ERROR) << "unknown message type from bitcoind: " << type;
    }
  } /* /while */
  subscriber.close();
}

void NodeBoost::foundNewBlock(const CBlock &block) {
  submitBlock(block);  // using thread, none-block
  broadcastBlock(block);
}

void NodeBoost::zmqPubMessage(const string &type, zmq::message_t &zmsg) {
  ScopeLock sl(zmqPubLock_);

  zmq::message_t ztype(type.size());
  memcpy(ztype.data(), type.data(), type.size());

  // type
  zmqPub_->send(ztype, ZMQ_SNDMORE);
  // content
  zmqPub_->send(zmsg);
}

void NodeBoost::broadcastBlock(const CBlock &block) {
  {
    // broadcast only once
    ScopeLock sl(historyLock_);
    if (zmqBroadcastHistory_.count(block.GetHash()) != 0) {
      LOG(INFO) << "block has already broadcasted: " << block.GetHash().ToString();
      return;
    }
    zmqBroadcastHistory_.insert(block.GetHash());
  }

  // add to txs repo
  for (auto tx : block.vtx) {
    txRepo_->AddTx(tx);
  }

  // broadcast thin block
  LOG(INFO) << "broadcast thin block: " << block.GetHash().ToString();
  zmq::message_t zmsg;
  _buildMsgThinBlock(block, zmsg);
  zmqPubMessage(MSG_PUB_THIN_BLOCK, zmsg);
}

void NodeBoost::broadcastHeartBeat() {
  zmq::message_t zmsg;
  string now = date("%F %T");

  zmsg.rebuild(now.size());
  memcpy(zmsg.data(), now.data(), now.size());
  zmqPubMessage(MSG_PUB_HEARTBEAT, zmsg);
}

void NodeBoost::run() {
  boost::thread t1(boost::bind(&NodeBoost::threadZmqResponse, this));
  boost::thread t2(boost::bind(&NodeBoost::threadListenBitcoind, this));

  time_t lastHeartbeatTime = 0;
  while (running_) {
    if (time(nullptr) > lastHeartbeatTime + 60) {
      broadcastHeartBeat();
      lastHeartbeatTime = time(nullptr);
    }

    sleep(1);
  }

  // wait for all peers closed
  while (peers_.size()) {
    LOG(INFO) << "wait for all peers to close, curr conns: " << peers_.size();
    sleep(1);
  }
}

void NodeBoost::peerConnect(const string &subAddr, const string &reqAddr) {
  boost::thread t(boost::bind(&NodeBoost::threadPeerConnect,  this, subAddr, reqAddr));
}

void NodeBoost::threadPeerConnect(const string subAddr, const string reqAddr) {
  if (running_ == false) {
    LOG(WARNING) << "server has stopped, ignore to connect peer: " << subAddr << ", " << reqAddr;
    return;
  }

  const string pKey = subAddr + "|" + reqAddr;
  if (peers_.count(pKey) != 0) {
    LOG(WARNING) << "already connect to this peer: " << pKey;
    return;
  }

  NodePeer *peer = new NodePeer(subAddr, reqAddr, &zmqContext_, this, txRepo_);
  peer->tellPeerToConnectMyServer(zmqPubAddr_, zmqRepAddr_);
  peers_.insert(make_pair(pKey, peer));

  LOG(INFO) << "connect peer: " << subAddr << ", " << reqAddr;
  peer->run();
}

void NodeBoost::peerCloseAll() {
  for (auto it : peers_) {
    it.second->stop();
  }

  while (peers_.size()) {
    auto it = peers_.begin();
    NodePeer *p = it->second;
    if (p->isFinish() == false) {
      sleep(1);
      continue;
    }
    peers_.erase(it);
  }
}

void NodeBoost::threadSubmitBlock2Bitcoind(const string bitcoindRpcAddr,
                                           const string bitcoindRpcUserpass,
                                           const string blockHex) {
  string req;
  req += "{\"jsonrpc\":\"1.0\",\"id\":\"1\",\"method\":\"submitblock\",\"params\":[\"";
  req += blockHex;
  req += "\"]}";

  string rep;
  bool res = bitcoindRpcCall(bitcoindRpcAddr.c_str(), bitcoindRpcUserpass.c_str(),
                             req.c_str(), rep);
  LOG(INFO) << "bitcoind rpc call, rep: " << rep;

  if (!res) {
    LOG(ERROR) << "bitcoind rpc failure";
  }
}

void NodeBoost::submitBlock(const CBlock &block) {
  {
    // submit only once
    ScopeLock sl(historyLock_);
    if (bitcoindBlockHistory_.count(block.GetHash()) != 0) {
      LOG(INFO) << "block has already submitted to bitcoind: " << block.GetHash().ToString();
      return;
    }
    bitcoindBlockHistory_.insert(block.GetHash());
  }

  // use thread to submit block, none-block
  LOG(INFO) << "submit block to bitcoind, blkhash: " << block.GetHash().ToString();
  const string hex = EncodeHexBlock(block);
  boost::thread t(boost::bind(&NodeBoost::threadSubmitBlock2Bitcoind, this,
                              bitcoindRpcAddr_, bitcoindRpcUserpass_, hex));
}

