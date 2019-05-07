/* Copyright (c) 2018 - present, VE Software Inc. All rights reserved
 *
 * This source code is licensed under Apache 2.0 License
 *  (found in the LICENSE.Apache file in the root directory)
 */

#include "meta/processors/DropSpaceProcessor.h"

namespace nebula {
namespace meta {

void DropSpaceProcessor::process(const cpp2::DropSpaceReq& req) {
    folly::SharedMutex::WriteHolder wHolder(LockUtils::spaceLock());
    auto spaceRet = getSpaceId(req.get_space_name());

    if (!spaceRet.ok()) {
        resp_.set_code(to(std::move(spaceRet.status())));
        onFinished();
        return;;
    }

    auto spaceId = spaceRet.value();
    VLOG(3) << "Drop space " << req.get_space_name() << ", id " << spaceId;
    resp_.set_code(cpp2::ErrorCode::SUCCEEDED);
    std::vector<std::string> deleteKeys;

    auto prefix = MetaServiceUtils::partPrefix(spaceId);
    std::unique_ptr<kvstore::KVIterator> iter;
    auto ret = kvstore_->prefix(kDefaultSpaceId_, kDefaultPartId_, prefix, &iter);
    if (ret != kvstore::ResultCode::SUCCEEDED) {
        resp_.set_code(to(ret));
        onFinished();
        return;
    }

    while (iter->valid()) {
        deleteKeys.emplace_back(iter->key());
        iter->next();
    }

    deleteKeys.emplace_back(MetaServiceUtils::indexKey(EntryType::SPACE, req.get_space_name()));
    deleteKeys.emplace_back(MetaServiceUtils::spaceKey(spaceId));

    // TODO(YT) delete Tag/Edge under the space
    doMultiRemove(std::move(deleteKeys));
    // TODO(YT) delete part files of the space
}

}  // namespace meta
}  // namespace nebula