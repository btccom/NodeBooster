#include <string>
#include <sstream>
#include <vector>

#include "bitcoin/base58.h"
#include "bitcoin/core.h"
#include "bitcoin/util.h"

#include "Common.h"

string EncodeHexTx(const CTransaction &tx);
string EncodeHexBlock(const CBlock &block);

bool DecodeHexTx(CTransaction& tx, const unsigned char *data, size_t len);
bool DecodeHexTx(CTransaction& tx, const std::string& strHexTx);
bool DecodeHexBlk(CBlock& block, const std::string& strHexBlk);

