﻿// 
//  C4BlobStore.cs
// 
//  Author:
//  Jim Borden  <jim.borden@couchbase.com>
// 
//  Copyright (c) 2017 Couchbase, Inc All rights reserved.
// 
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
// 
//  http://www.apache.org/licenses/LICENSE-2.0
// 
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//  
namespace LiteCore.Interop
{
#if LITECORE_PACKAGED
    internal
#else
    public
#endif
         unsafe partial struct C4BlobKey
    {
        public static readonly int Size = _Size;
        // ReSharper disable InconsistentNaming
        private const int _Size = 20;
        // ReSharper restore InconsistentNaming
        
        public override int GetHashCode()
        {
            int hash = 17;
            unchecked {
                fixed(byte* b = bytes) {
                    for(int i = 0; i < _Size; i++) {
                        hash = hash * 23 + b[i];
                    }
                }
            }

            return hash;
        }

        public override bool Equals(object obj)
        {
            if(!(obj is C4BlobKey)) {
                return false;
            }

            var other = (C4BlobKey)obj;
            fixed(byte* b = bytes) {
                for(int i = 0; i < _Size; i++) {
                    if(b[i] != other.bytes[i]) {
                        return false;
                    }
                }
            }

            return true;
        }
    }

#if LITECORE_PACKAGED
    internal
#else
    public
#endif
         static unsafe partial class Native
    {
        public static ulong c4stream_read(C4ReadStream *stream, byte[] buffer, C4Error *outError)
        {
            return c4stream_read(stream, buffer, buffer.Length, outError);
        }
        
        public static bool c4stream_write(C4WriteStream* stream, byte[] bytes, C4Error* outError)
        {
            return c4stream_write(stream, bytes, (ulong)bytes.Length, outError);
        }
    }
}
