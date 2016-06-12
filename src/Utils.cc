
#include "Utils.h"

#include "bitcoin/util.h"

string EncodeHexTx(const CTransaction& tx) {
  CDataStream ssTx(SER_NETWORK, BITCOIN_PROTOCOL_VERSION);
  ssTx << tx;
  return HexStr(ssTx.begin(), ssTx.end());
}

string EncodeHexBlock(const CBlock &block) {
  CDataStream ssBlock(SER_NETWORK, BITCOIN_PROTOCOL_VERSION);
  ssBlock << block;
  return HexStr(ssBlock.begin(), ssBlock.end());
}


static bool _DecodeBinTx(CTransaction& tx, vector<unsigned char> &data) {
  CDataStream ssData(data, SER_NETWORK, BITCOIN_PROTOCOL_VERSION);
  try {
    ssData >> tx;
  }
  catch (const std::exception &) {
    return false;
  }
  return true;
}

bool DecodeBinTx(CTransaction& tx, const unsigned char *data, size_t len) {
  vector<unsigned char> txData(data, data + len);
  return _DecodeBinTx(tx, txData);
}

bool DecodeHexTx(CTransaction& tx, const std::string& strHexTx)
{
  if (!IsHex(strHexTx))
    return false;

  vector<unsigned char> txData(ParseHex(strHexTx));
  return _DecodeBinTx(tx, txData);
}


static bool _DecodeBinBlk(CBlock& block, vector<unsigned char> &data) {
  CDataStream ssBlock(data, SER_NETWORK, BITCOIN_PROTOCOL_VERSION);
  try {
    ssBlock >> block;
  }
  catch (const std::exception &) {
    return false;
  }
  return true;
}

bool DecodeBinBlk(CBlock& block, const unsigned char *data, size_t len) {
  vector<unsigned char> blkData(data, data + len);
  return _DecodeBinBlk(block, blkData);
}

bool DecodeHexBlk(CBlock& block, const std::string& strHexBlk)
{
  if (!IsHex(strHexBlk))
    return false;

  std::vector<unsigned char> blkData(ParseHex(strHexBlk));
  return _DecodeBinBlk(block, blkData);
}

//  Receive 0MQ string from socket and convert into string
std::string s_recv (zmq::socket_t & socket) {
  zmq::message_t message;
  socket.recv(&message);

  return std::string(static_cast<char*>(message.data()), message.size());
}

//  Convert string to 0MQ string and send to socket
bool s_send(zmq::socket_t & socket, const std::string & string) {
  zmq::message_t message(string.size());
  memcpy(message.data(), string.data(), string.size());

  bool rc = socket.send(message);
  return (rc);
}

//  Sends string as 0MQ string, as multipart non-terminal
bool s_sendmore (zmq::socket_t & socket, const std::string & string) {
  zmq::message_t message(string.size());
  memcpy(message.data(), string.data(), string.size());

  bool rc = socket.send(message, ZMQ_SNDMORE);
  return (rc);
}
