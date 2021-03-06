//
//  RawRevTree.cc
//  Couchbase Lite Core
//
//  Created by Jens Alfke on 5/25/16.
//  Copyright (c) 2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "RawRevTree.hh"
#include "RevTree.hh"
#include "Error.hh"
#include "varint.hh"
#ifdef _MSC_VER
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#endif

using namespace fleece;


namespace litecore {

    std::vector<Rev> RawRevision::decodeTree(slice raw_tree, RevTree* owner, sequence curSeq) {
        const RawRevision *rawRev = (const RawRevision*)raw_tree.buf;
        unsigned count = rawRev->count();
        if (count > UINT16_MAX)
            error::_throw(error::CorruptRevisionData);
        std::vector<Rev> revs(count);
        auto rev = revs.begin();
        for (; rawRev->isValid(); rawRev = rawRev->next()) {
            rawRev->copyTo(*rev);
            if (rev->sequence == 0)
                rev->sequence = curSeq;
            rev->owner = owner;
            rev++;
        }
        if ((uint8_t*)rawRev != (uint8_t*)raw_tree.end() - sizeof(uint32_t)) {
            error::_throw(error::CorruptRevisionData);
        }
        return revs;
    }


    slice RawRevision::getCurrentRevBody(slice raw_tree) noexcept {
        const RawRevision *rawRev = (const RawRevision*)raw_tree.buf;
        return rawRev->body();
    }


    alloc_slice RawRevision::encodeTree(const std::vector<Rev> &revs) {
        // Allocate output buffer:
        size_t totalSize = sizeof(uint32_t);  // start with space for trailing 0 size
        for (auto rev = revs.begin(); rev != revs.end(); ++rev)
            totalSize += sizeToWrite(*rev);

        alloc_slice result(totalSize);

        // Write the raw revs:
        RawRevision *dst = (RawRevision*)result.buf;
        for (auto src = revs.begin(); src != revs.end(); ++src) {
            dst = dst->copyFrom(*src);
        }
        dst->size = _enc32(0);   // write trailing 0 size marker
        Assert((&dst->size + 1) == result.end());
        return result;
    }


    size_t RawRevision::sizeToWrite(const Rev &rev) {
        return offsetof(RawRevision, revID)
             + rev.revID.size
             + SizeOfVarInt(rev.sequence)
             + rev._body.size;
    }

    RawRevision* RawRevision::copyFrom(const Rev &rev) {
        size_t revSize = sizeToWrite(rev);
        this->size = _enc32((uint32_t)revSize);
        this->revIDLen = (uint8_t)rev.revID.size;
        memcpy(this->revID, rev.revID.buf, rev.revID.size);
        this->parentIndex = htons(rev._parentIndex);

        uint8_t dstFlags = rev.flags & RawRevision::kPublicPersistentFlags;
        if (rev._body.size > 0)
            dstFlags |= RawRevision::kHasData;
        this->flags = (Rev::Flags)dstFlags;

        void *dstData = offsetby(&this->revID[0], rev.revID.size);
        dstData = offsetby(dstData, PutUVarInt(dstData, rev.sequence));
        memcpy(dstData, rev._body.buf, rev._body.size);

        return (RawRevision*)offsetby(this, revSize);
    }

    void RawRevision::copyTo(Rev &dst) const {
        const void* end = this->next();
        dst.revID = {this->revID, this->revIDLen};
        dst.flags = (Rev::Flags)(this->flags & RawRevision::kPublicPersistentFlags);
        dst._parentIndex = ntohs(this->parentIndex);
        const void *data = offsetby(&this->revID, this->revIDLen);
        ptrdiff_t len = (uint8_t*)end-(uint8_t*)data;
        data = offsetby(data, GetUVarInt(slice(data, len), &dst.sequence));
        if (this->flags & RawRevision::kHasData)
            dst._body = slice(data, end);
        else
            dst._body = nullslice;
    }


    slice RawRevision::body() const {
        if (this->flags & RawRevision::kHasData) {
            const void* end = this->next();
            const void *data = offsetby(&this->revID, this->revIDLen);
            uint64_t sequence;
            data = offsetby(data, GetUVarInt(slice(data, end), &sequence));
            return slice(data, end);
        } else {
            return nullslice;
        }
    }

}
