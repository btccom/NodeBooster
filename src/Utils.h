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

bool Hex2Bin(const char *in, size_t size, vector<char> &out);
bool Hex2Bin(const char *in, vector<char> &out);
void Bin2Hex(const uint8 *in, size_t len, string &str);
void Bin2Hex(const vector<char> &in, string &str);
void Bin2HexR(const vector<char> &in, string &str);

bool DecodeHexTx(CTransaction& tx, const std::string& strHexTx);
bool DecodeBinTx(CTransaction& tx, const unsigned char *data, size_t len);

bool DecodeHexBlk(CBlock& block, const std::string& strHexBlk);
bool DecodeBinBlk(CBlock& block, const unsigned char *data, size_t len);

std::string s_recv(zmq::socket_t & socket);
bool s_send(zmq::socket_t & socket, const std::string & string);
bool s_sendmore (zmq::socket_t & socket, const std::string & string);
