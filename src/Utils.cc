
#include "Utils.h"

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


static bool _DecodeHexTx(CTransaction& tx, vector<unsigned char> &txData) {
  CDataStream ssData(txData, SER_NETWORK, BITCOIN_PROTOCOL_VERSION);
  try {
    ssData >> tx;
  }
  catch (const std::exception &) {
    return false;
  }
  return true;
}

bool DecodeHexTx(CTransaction& tx, const unsigned char *data, size_t len) {
  vector<unsigned char> txData(data, data + len);
  return _DecodeHexTx(tx, txData);
}

bool DecodeHexTx(CTransaction& tx, const std::string& strHexTx)
{
  if (!IsHex(strHexTx))
    return false;

  vector<unsigned char> txData(ParseHex(strHexTx));
  return _DecodeHexTx(tx, txData);
}

bool DecodeHexBlk(CBlock& block, const std::string& strHexBlk)
{
  if (!IsHex(strHexBlk))
    return false;

  std::vector<unsigned char> blockData(ParseHex(strHexBlk));
  CDataStream ssBlock(blockData, SER_NETWORK, BITCOIN_PROTOCOL_VERSION);
  try {
    ssBlock >> block;
  }
  catch (const std::exception &) {
    return false;
  }

  return true;
}

