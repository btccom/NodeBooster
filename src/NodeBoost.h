
#include <glog/logging.h>

#include "bitcoin/base58.h"
#include "bitcoin/core.h"
#include "bitcoin/util.h"

#include "Common.h"
#include "Utils.h"


class NodeBoost {
private:
  std::map<uint256, CTransaction> txsPool_;

public:
  NodeBoost();

  bool getTx(const uint256 &hash, CTransaction &tx);
  void AddTx(const CTransaction &tx);
};
