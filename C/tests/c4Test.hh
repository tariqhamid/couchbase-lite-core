//
//  c4Test.hh
//  Couchbase Lite Core
//
//  Created by Jens Alfke on 9/16/15.
//  Copyright (c) 2015-2016 Couchbase. All rights reserved.
//

#pragma once

#include "FleeceCpp.hh"

using namespace fleeceapi;

#include "c4.h"

#include "CatchHelper.hh"
#include "PlatformCompat.hh"
#include <functional>


#ifdef NDEBUG
    // Catch's assertion macros are pretty slow, and affect benchmark times.
    // So replace them with quick-n-dirty alternatives in an optimized build.
    #undef REQUIRE
    #define REQUIRE(X) do {if (!(X)) abort();} while (0)
    #undef CHECK
    #define CHECK(X) do {if (!(X)) abort();} while (0)
    #undef INFO
    #define INFO(X)
#endif


#ifdef _MSC_VER
    #define kPathSeparator "\\"
#else
    #define kPathSeparator "/"
#endif


#define TEMPDIR(PATH) c4str((TempDir() + PATH).c_str())

std::string TempDir();


std::ostream& operator<< (std::ostream& o, C4Slice s);
std::ostream& operator<< (std::ostream &out, C4Error error);



// Dumps a C4Key to a C++ string
static inline std::string toString(C4Slice s)   {return std::string((char*)s.buf, s.size);}


// Converts JSON5 to JSON; helps make JSON test input more readable!
std::string json5(std::string);


// This helper is necessary because it ends an open transaction if an assertion fails.
// If the transaction isn't ended, the c4db_delete call in tearDown will deadlock.
class TransactionHelper {
    public:
    explicit TransactionHelper(C4Database* db) {
        C4Error error;
        REQUIRE(c4db_beginTransaction(db, &error));
        _db = db;
    }

    ~TransactionHelper() {
        if (_db) {
            C4Error error;
            REQUIRE(c4db_endTransaction(_db, true, &error));
        }
    }

    private:
    C4Database* _db {nullptr};
};


// Handy base class that creates a new empty C4Database in its setUp method,
// and closes & deletes it in tearDown.
class C4Test {
public:
    static const int numberOfOptions = 2;       // rev-trees or version vectors

    static std::string sFixturesDir;            // directory where test files live
    
    C4Test(int testOption);
    ~C4Test();

    C4Slice databasePath() const                {return c4str(_dbPath.c_str());}

    C4Database *db;

    const C4StorageEngine storageType() const   {return _storage;}
    bool isSQLite() const                       {return storageType() == kC4SQLiteStorageEngine;}
    C4DocumentVersioning versioning() const     {return _versioning;}
    bool isRevTrees() const                     {return _versioning == kC4RevisionTrees;}
    bool isVersionVectors() const               {return _versioning == kC4VersionVectors;}

    void reopenDB();

    // Creates a new document revision with the given revID as a child of the current rev
    void createRev(C4Slice docID, C4Slice revID, C4Slice body, C4RevisionFlags flags =0);
    static void createRev(C4Database *db, C4Slice docID, C4Slice revID, C4Slice body, C4RevisionFlags flags =0);

    void createNumberedDocs(unsigned numberOfDocs);

    std::string listSharedKeys(std::string delimiter =", ");

    FLSlice readFile(std::string path); // caller must free buf when done
    unsigned importJSONFile(std::string path,
                            std::string idPrefix ="",
                            double timeout =15.0,
                            bool verbose =false);
    bool readFileByLines(std::string path, std::function<bool(FLSlice)>);
    unsigned importJSONLines(std::string path, double timeout =15.0, bool verbose =false);
    
    // Some handy constants to use
    static const C4Slice kDocID;    // "mydoc"
    C4Slice kRevID;    // "1-abcdef"
    C4Slice kRev2ID;   // "2-d00d3333"
    C4Slice kRev3ID;
    static const C4Slice kBody;     // "{\"name\":007}"
    static C4Slice kFleeceBody;

private:
    const C4StorageEngine _storage;
    const C4DocumentVersioning _versioning;
    const bool _bundled;
    std::string _dbPath;
    int objectCount;
};
