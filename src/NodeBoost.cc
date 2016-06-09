#include "NodeBoost.h"

NodeBoost::NodeBoost() {
}

void NodeBoost::AddTx(const CTransaction &tx) {
  const uint256 hash = tx.GetHash();
  if (txsPool_.count(hash)) {
    return;
  }
  txsPool_.insert(std::make_pair(hash, tx));
}

bool NodeBoost::getTx(const uint256 &hash, CTransaction &tx) {
  auto it = txsPool_.find(hash);
  if (it == txsPool_.end()) {
    return false;
  }
  tx = it->second;
  return true;
}