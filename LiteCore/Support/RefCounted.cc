//
//  RefCounted.cc
//  Couchbase Lite Core
//
//  Created by Jens Alfke on 8/12/16.
//  Copyright (c) 2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "RefCounted.hh"
#include "Logging.hh"
#include <stdlib.h>

namespace litecore {

    RefCounted::~RefCounted() {
        if (_refCount != 0) {
            Warn("FATAL: RefCounted object at %p destructed while it has a refCount of %d",
                 this, (int)_refCount);
            abort();
        }
    }

}
