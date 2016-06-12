#include <string>
#include <sstream>
#include <vector>

#include "bitcoin/base58.h"
#include "bitcoin/core.h"
#include "bitcoin/util.h"

#include "zmq.hpp"

#include "Common.h"

string EncodeHexTx(const CTransaction &tx);
string EncodeHexBlock(const CBlock &block);


bool DecodeHexTx(CTransaction& tx, const std::string& strHexTx);
bool DecodeBinTx(CTransaction& tx, const unsigned char *data, size_t len);

bool DecodeHexBlk(CBlock& block, const std::string& strHexBlk);
bool DecodeBinBlk(CBlock& block, const unsigned char *data, size_t len);

std::string s_recv(zmq::socket_t & socket);
bool s_send(zmq::socket_t & socket, const std::string & string);
bool s_sendmore (zmq::socket_t & socket, const std::string & string);
