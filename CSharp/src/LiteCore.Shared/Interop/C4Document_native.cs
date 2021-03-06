//
// Document_native.cs
//
// Author:
// 	Jim Borden  <jim.borden@couchbase.com>
//
// Copyright (c) 2017 Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

using System;
using System.Linq;
using System.Runtime.InteropServices;

using LiteCore.Util;

namespace LiteCore.Interop
{
#if LITECORE_PACKAGED
    internal
#else
    public
#endif 
    unsafe static partial class Native
    {
        public static C4Document* c4doc_get(C4Database* database, string docID, bool mustExist, C4Error* outError)
        {
            using(var docID_ = new C4String(docID)) {
                return NativeRaw.c4doc_get(database, docID_.AsC4Slice(), mustExist, outError);
            }
        }

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern C4Document* c4doc_getBySequence(C4Database* database, ulong sequence, C4Error* outError);

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool c4doc_save(C4Document* doc, uint maxRevTreeDepth, C4Error* outError);

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void c4doc_free(C4Document* doc);

        public static bool c4doc_selectRevision(C4Document* doc, string revID, bool withBody, C4Error* outError)
        {
            using(var revID_ = new C4String(revID)) {
                return NativeRaw.c4doc_selectRevision(doc, revID_.AsC4Slice(), withBody, outError);
            }
        }

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool c4doc_selectCurrentRevision(C4Document* doc);

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool c4doc_loadRevisionBody(C4Document* doc, C4Error* outError);

        public static string c4doc_detachRevisionBody(C4Document* doc)
        {
            using(var retVal = NativeRaw.c4doc_detachRevisionBody(doc)) {
                return ((C4Slice)retVal).CreateString();
            }
        }

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool c4doc_hasRevisionBody(C4Document* doc);

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool c4doc_selectParentRevision(C4Document* doc);

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool c4doc_selectNextRevision(C4Document* doc);

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool c4doc_selectNextLeafRevision(C4Document* doc, [MarshalAs(UnmanagedType.U1)]bool includeDeleted, [MarshalAs(UnmanagedType.U1)]bool withBody, C4Error* outError);

        public static bool c4doc_selectFirstPossibleAncestorOf(C4Document* doc, string revID)
        {
            using(var revID_ = new C4String(revID)) {
                return NativeRaw.c4doc_selectFirstPossibleAncestorOf(doc, revID_.AsC4Slice());
            }
        }

        public static bool c4doc_selectNextPossibleAncestorOf(C4Document* doc, string revID)
        {
            using(var revID_ = new C4String(revID)) {
                return NativeRaw.c4doc_selectNextPossibleAncestorOf(doc, revID_.AsC4Slice());
            }
        }

        public static uint c4rev_getGeneration(string revID)
        {
            using(var revID_ = new C4String(revID)) {
                return NativeRaw.c4rev_getGeneration(revID_.AsC4Slice());
            }
        }

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool c4doc_removeRevisionBody(C4Document* doc);

        public static int c4doc_purgeRevision(C4Document* doc, string revID, C4Error* outError)
        {
            using(var revID_ = new C4String(revID)) {
                return NativeRaw.c4doc_purgeRevision(doc, revID_.AsC4Slice(), outError);
            }
        }

        public static bool c4db_purgeDoc(C4Database* database, string docID, C4Error* outError)
        {
            using(var docID_ = new C4String(docID)) {
                return NativeRaw.c4db_purgeDoc(database, docID_.AsC4Slice(), outError);
            }
        }

        public static bool c4doc_setExpiration(C4Database* db, string docId, ulong timestamp, C4Error* outError)
        {
            using(var docId_ = new C4String(docId)) {
                return NativeRaw.c4doc_setExpiration(db, docId_.AsC4Slice(), timestamp, outError);
            }
        }

        public static ulong c4doc_getExpiration(C4Database* db, string docId)
        {
            using(var docId_ = new C4String(docId)) {
                return NativeRaw.c4doc_getExpiration(db, docId_.AsC4Slice());
            }
        }

        public static C4Document* c4doc_put(C4Database *database, 
                                            C4DocPutRequest *request, 
                                            ulong* outCommonAncestorIndex, 
                                            C4Error *outError)
        {
            var uintptr = new UIntPtr();
            var retVal = NativeRaw.c4doc_put(database, request, &uintptr, outError);
            if(outCommonAncestorIndex != null) {
                *outCommonAncestorIndex = uintptr.ToUInt64();
            }

            return retVal;
        }


        public static string c4doc_generateRevID(string body, string parentRevID, bool deletion)
        {
            using(var body_ = new C4String(body))
            using(var parentRevID_ = new C4String(parentRevID)) {
                using(var retVal = NativeRaw.c4doc_generateRevID(body_.AsC4Slice(), parentRevID_.AsC4Slice(), deletion)) {
                    return ((C4Slice)retVal).CreateString();
                }
            }
        }

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void c4doc_generateOldStyleRevID([MarshalAs(UnmanagedType.U1)]bool generateOldStyle);


    }
    
#if LITECORE_PACKAGED
    internal
#else
    public
#endif 
    unsafe static partial class NativeRaw
    {
        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern C4Document* c4doc_get(C4Database* database, C4Slice docID, [MarshalAs(UnmanagedType.U1)]bool mustExist, C4Error* outError);

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool c4doc_selectRevision(C4Document* doc, C4Slice revID, [MarshalAs(UnmanagedType.U1)]bool withBody, C4Error* outError);

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern C4SliceResult c4doc_detachRevisionBody(C4Document* doc);

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool c4doc_selectFirstPossibleAncestorOf(C4Document* doc, C4Slice revID);

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool c4doc_selectNextPossibleAncestorOf(C4Document* doc, C4Slice revID);

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint c4rev_getGeneration(C4Slice revID);

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int c4doc_purgeRevision(C4Document* doc, C4Slice revID, C4Error* outError);

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool c4db_purgeDoc(C4Database* database, C4Slice docID, C4Error* outError);

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool c4doc_setExpiration(C4Database* db, C4Slice docId, ulong timestamp, C4Error* outError);

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern ulong c4doc_getExpiration(C4Database* db, C4Slice docId);

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern C4Document* c4doc_put(C4Database* database, C4DocPutRequest* request, UIntPtr* outCommonAncestorIndex, C4Error* outError);

        [DllImport(Constants.DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern C4SliceResult c4doc_generateRevID(C4Slice body, C4Slice parentRevID, [MarshalAs(UnmanagedType.U1)]bool deletion);


    }
}
