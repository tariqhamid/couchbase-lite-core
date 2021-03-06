using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;

using FluentAssertions;
using LiteCore.Interop;
using LiteCore.Tests.Util;
using LiteCore.Util;
using Xunit.Abstractions;

namespace LiteCore.Tests
{
    public unsafe class Test : TestBase
    {
#if __ANDROID__
        public static readonly string TestDir = System.Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
#elif WINDOWS_UWP
        public static readonly string TestDir = Windows.Storage.ApplicationData.Current.LocalFolder.Path;
#elif __IOS__
        public static readonly string TestDir = "/tmp/";
#else
        public static readonly string TestDir = RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ?
            "C:\\tmp\\" : "/tmp/";
#endif

        internal static readonly C4Slice Body = C4Slice.Constant("{\"name\":007}");

        internal static readonly C4Slice FleeceBody;
        
        protected override int NumberOfOptions
        {
            get {
                return 2;
            }
        }
        
        private bool _bundled = true;
        private int _objectCount = 0;

        internal C4Database* Db { get; private set; }
        internal C4DocumentVersioning Versioning { get; private set; }
        protected string Storage { get; private set; }

        internal C4Slice DocID
        {
            get {
                return C4Slice.Constant("mydoc");
            }
        }

        internal C4Slice RevID
        {
            get {
                return IsRevTrees() ? C4Slice.Constant("1-abcd") : C4Slice.Constant("1@*");
            }
        }

        internal C4Slice Rev2ID
        {
            get {
                return IsRevTrees() ? C4Slice.Constant("2-c001d00d") : C4Slice.Constant("2@*");
            }
        }

        internal C4Slice Rev3ID
        {
            get {
                return IsRevTrees() ? C4Slice.Constant("3-deadbeef") : C4Slice.Constant("3@*");
            }
        }

        static Test()
        {
            var enc = Native.FLEncoder_New();
            Native.FLEncoder_BeginDict(enc, 1);
            Native.FLEncoder_WriteKey(enc, "answer");
            Native.FLEncoder_WriteInt(enc, 42);
            Native.FLEncoder_EndDict(enc);
            var result = NativeRaw.FLEncoder_Finish(enc, null);
            FleeceBody = result;
        }

        public Test(ITestOutputHelper output) : base(output)
        {

        }

        protected bool IsRevTrees()
        {
            return Versioning == C4DocumentVersioning.RevisionTrees;
        }

        protected override void SetupVariant(int option)
        {
            _objectCount = Native.c4_getObjectCount();
            Versioning = (option & 1) != 0 ? C4DocumentVersioning.VersionVectors : C4DocumentVersioning.RevisionTrees;
            Native.c4_shutdown(null);

            var config = new C4DatabaseConfig();
            config.flags = C4DatabaseFlags.Create;
            config.versioning = Versioning;

            if(_bundled) {
                config.flags |= C4DatabaseFlags.Bundled;
            }

            WriteLine($"Opening SQLite database using {Versioning}");

            C4Error err;
            config.storageEngine = C4StorageEngine.SQLite;
            Native.c4db_deleteAtPath(DatabasePath(), &config, null);
            Db = Native.c4db_open(DatabasePath(), &config, &err);
            ((long)Db).Should().NotBe(0, "because otherwise the database failed to open");
        }

        protected override void TeardownVariant(int option)
        {
            var config = C4DatabaseConfig.Get(Native.c4db_getConfig(Db));
            config.Dispose();
            LiteCoreBridge.Check(err => Native.c4db_delete(Db, err));
            Native.c4db_free(Db);
            Db = null;
            if(CurrentException == null) {
                Native.c4_getObjectCount().Should().Be(_objectCount, "because otherwise an object was leaked");
            }
        }

        internal void CreateRev(C4Database *db, string docID, C4Slice revID, C4Slice body, C4RevisionFlags flags = (C4RevisionFlags)0)
        {
            LiteCoreBridge.Check(err => Native.c4db_beginTransaction(db, err));
            try {
                var curDoc = (C4Document *)LiteCoreBridge.Check(err => Native.c4doc_get(db, docID, 
                    false, err));
                var history = new[] { revID, curDoc->revID };
                fixed(C4Slice* h = history) {
                    var rq = new C4DocPutRequest {
                        existingRevision = true,
                        docID = curDoc->docID,
                        history = h,
                        historyCount = curDoc->revID.buf != null ? 2UL : 1UL,
                        body = body,
                        revFlags = flags,
                        save = true
                    };

                    var doc = (C4Document *)LiteCoreBridge.Check(err => {
                        var localRq = rq;
                        return Native.c4doc_put(db, &localRq, null, err);
                    });
                    Native.c4doc_free(doc);
                    Native.c4doc_free(curDoc);
                }
            } finally {
                LiteCoreBridge.Check(err => Native.c4db_endTransaction(db, true, err));
            }
        }

        internal void CreateRev(string docID, C4Slice revID, C4Slice body, C4RevisionFlags flags = (C4RevisionFlags)0)
        {
            CreateRev(Db, docID, revID, body, flags);
        }

        protected void CreateNumberedDocs(int numberOfDocs)
        {
            for (int i = 1; i < 100; i++) {
                var docID = $"doc-{i:D3}";
                CreateRev(docID, RevID, Body);
            }
        }

        protected string DatabasePath()
        {
            if(_bundled) {
                return Path.Combine(TestDir, "cbl_core_test");
            } else if(Storage == C4StorageEngine.SQLite) {
                return Path.Combine(TestDir, "cbl_core_test.sqlite3");
            } else {
                return Path.Combine(TestDir, "cbl_core_test.forestdb");
            }
        }

        private void Log(C4LogLevel level, C4Slice s)
        {
            WriteLine($"[{level}] {s.CreateString()}");
        }

        internal bool ReadFileByLines(string path, Func<FLSlice, bool> callback)
        {
#if WINDOWS_UWP
            var url = $"ms-appx:///Assets/{path}";
            var file = Windows.Storage.StorageFile.GetFileFromApplicationUriAsync(new Uri(url))
                .AsTask()
                .ConfigureAwait(false)
                .GetAwaiter()
                .GetResult();

            var lines = Windows.Storage.FileIO.ReadLinesAsync(file).AsTask().ConfigureAwait(false).GetAwaiter().GetResult();
            foreach(var line in lines) {
#elif __ANDROID__
            var ctx = global::Couchbase.Lite.Tests.Android.MainActivity.ActivityContext;
            using (var tr = new StreamReader(ctx.Assets.Open(path))) {
                string line;
                while((line = tr.ReadLine()) != null) { 
#else
            using(var tr = new StreamReader(File.Open(path, FileMode.Open))) {
                string line;
                while((line = tr.ReadLine()) != null) {
#endif
            using(var c4 = new C4String(line)) {
                        if(!callback((FLSlice)c4.AsC4Slice())) {
                            return false;
                        }
                    }
                }
#if !WINDOWS_UWP
        }
#endif

            return true;
        } 

        protected uint ImportJSONLines(string path)
        {
            return ImportJSONLines(path, TimeSpan.FromSeconds(15), false);
        }

        // Read a file that contains a JSON document per line. Every line becomes a document.
        protected uint ImportJSONLines(string path, TimeSpan timeout, bool verbose)
        {
            if(verbose) {
                WriteLine($"Reading {path}...");
            }

            var st = Stopwatch.StartNew();
            uint numDocs = 0;
            LiteCoreBridge.Check(err => Native.c4db_beginTransaction(Db, err));
            try {
                var encoder = Native.FLEncoder_New();
                ReadFileByLines(path, line => {
                    FLError error;
                    NativeRaw.FLEncoder_ConvertJSON(encoder, line);
                    var body = NativeRaw.FLEncoder_Finish(encoder, &error);
                    ((long)body.buf).Should().NotBe(0, "because otherwise the encode failed");
                    Native.FLEncoder_Reset(encoder);

                    var docID = (numDocs + 1).ToString("D7");

                    // Save document:
                    using(var docID_ = new C4String(docID)) {
                        var rq = new C4DocPutRequest {
                            docID = docID_.AsC4Slice(),
                            body = (C4Slice)body,
                            save = true
                        };
                        var doc = (C4Document *)LiteCoreBridge.Check(err => {
                            var localRq = rq;
                            return Native.c4doc_put(Db, &localRq, null, err);
                        });
                        Native.c4doc_free(doc);
                    }

                    Native.FLSliceResult_Free(body);
                    ++numDocs;
                    if(numDocs % 1000 == 0 && st.Elapsed >= timeout) {
                        Console.Write($"Stopping JSON import after {st.Elapsed.TotalSeconds:F3} sec ");
                        return false;
                    }

                    if(verbose && numDocs % 10000 == 0) {
                        Console.Write($"{numDocs} ");
                    }

                    return true;
                });

                if(verbose) {
                    WriteLine("Committing...");
                }
            } finally {
                LiteCoreBridge.Check(err => Native.c4db_endTransaction(Db, true, err));
            }

            if(verbose) {
                st.PrintReport("Importing", numDocs, "doc", _output);
            }

            return numDocs;
        }

        protected void ReopenDB()
        {
            var config = C4DatabaseConfig.Get(Native.c4db_getConfig(Db));
            LiteCoreBridge.Check(err => Native.c4db_close(Db, err));
            Native.c4db_free(Db);
            Db = (C4Database *)LiteCoreBridge.Check(err => {
                var localConfig = config;
                return Native.c4db_open(DatabasePath(), &localConfig, err);
            });
        }
    }
}
