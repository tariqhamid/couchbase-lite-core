//
//  Database.hh
//  Couchbase Lite Core
//
//  Created by Jens Alfke on 8/12/16.
//  Copyright (c) 2016 Couchbase. All rights reserved.
//

#pragma once

#include "c4Internal.hh"
#include "c4Database.h"
#include "c4Document.h"
#include "DataFile.hh"
#include "FilePath.hh"
#include "c4Private.h"


#if C4DB_THREADSAFE
#define WITH_LOCK(db) lock_guard<mutex> _lock((db)->_mutex)
#else
#define WITH_LOCK(db) do { } while (0)  // no-op
#endif


namespace fleece {
    class Encoder;
    class SharedKeys;
}
namespace litecore {
    class CASRevisionStore;
    class SequenceTracker;
    struct DocumentMeta;
    class BlobStore;
}


namespace c4Internal {
    class Document;
    class DocumentFactory;


    /** A top-level LiteCore database. */
    class Database : public RefCounted, C4InstanceCounted {
    public:
        Database(const string &path, C4DatabaseConfig config);

        void close();
        void deleteDatabase();
        static bool deleteDatabaseAtPath(const string &dbPath, const C4DatabaseConfig*);

        DataFile* dataFile()                                {return _db.get();}
        FilePath path() const;
        uint64_t countDocuments();
        sequence_t lastSequence()       {WITH_LOCK(this); return defaultKeyStore().lastSequence();}

        uint32_t maxRevTreeDepth();
        void setMaxRevTreeDepth(uint32_t depth);

        struct UUID {
            uint8_t bytes[32];
        };
        static const slice kPublicUUIDKey;
        static const slice kPrivateUUIDKey;

        UUID getUUID(slice key);

        void rekey(const C4EncryptionKey *newKey);

        void compact();
        void setOnCompact(DataFile::OnCompactCallback callback) noexcept;

        const C4DatabaseConfig config;

        Transaction& transaction() const;

        // Transaction methods below acquire _transactionMutex. Do not call them if
        // _mutex is already locked, or deadlock may occur!
        void beginTransaction();
        void endTransaction(bool commit);

        bool inTransaction() noexcept;

        KeyStore& defaultKeyStore();
        KeyStore& getKeyStore(const string &name) const;

        bool purgeDocument(slice docID);

        Record getRawDocument(const std::string &storeName, slice key);
        void putRawDocument(const string &storeName, slice key, slice meta, slice body);

        DocumentFactory& documentFactory()                  {return *_documentFactory;}

        fleece::Encoder& sharedEncoder();

        void useDocumentKeys()                              {_db->useDocumentKeys();}
        fleece::SharedKeys* documentKeys()                  {return _db->documentKeys();}

        SequenceTracker& sequenceTracker()                  {return *_sequenceTracker;}

        BlobStore* blobStore();

#if C4DB_THREADSAFE
        // Mutex for synchronizing DataFile calls. Non-recursive!
        mutex _mutex;
#endif

    public:
        // should be private, but called from Document
        void saved(Document*);

        // these should be private, but are also used by c4View
        static DataFile* newDataFile(const FilePath &path,
                                     const C4DatabaseConfig &config,
                                     bool isMainDB);
        static void rekeyDataFile(DataFile* database, const C4EncryptionKey *newKey);

    protected:
        virtual ~Database();
        void mustBeInTransaction();
        void mustNotBeInTransaction();
        void externalTransactionCommitted(const SequenceTracker&);

    private:
        static FilePath findOrCreateBundle(const string &path, C4DatabaseConfig &config);

        unique_ptr<DataFile>        _db;                    // Underlying DataFile
        Transaction*                _transaction {nullptr}; // Current Transaction, or null
        int                         _transactionLevel {0};  // Nesting level of transaction
        unique_ptr<DocumentFactory> _documentFactory;       // Instantiates C4Documents
    #if C4DB_THREADSAFE
        // Recursive mutex for accessing _transaction and _transactionLevel.
        // Must be acquired BEFORE _mutex, or deadlock may occur!
        recursive_mutex             _transactionMutex;
    #endif
        unique_ptr<fleece::Encoder> _encoder;
        unique_ptr<SequenceTracker> _sequenceTracker;       // Doc change tracker/notifier
        unique_ptr<BlobStore>       _blobStore;
        uint32_t                    _maxRevTreeDepth {0};
    };


    static inline C4Database* external(Database *db)    {return (C4Database*)db;}


    /** Abstract interface for creating Document instances; owned by a Database. */
    class DocumentFactory {
    public:
        DocumentFactory(Database *db)       :_db(db) { }
        Database* database() const          {return _db;}

        virtual ~DocumentFactory() { }
        virtual Document* newDocumentInstance(C4Slice docID) =0;
        virtual Document* newDocumentInstance(const Record&) =0;
        virtual alloc_slice revIDFromMeta(const DocumentMeta&) =0;
        virtual DataFile::FleeceAccessor fleeceAccessor() const {return nullptr;}

    private:
        Database* const _db;
    };


    /** DocumentFactory subclass for rev-tree document schema. */
    class TreeDocumentFactory : public DocumentFactory {
    public:
        TreeDocumentFactory(Database *db)   :DocumentFactory(db) { }
        Document* newDocumentInstance(C4Slice docID) override;
        Document* newDocumentInstance(const Record&) override;
        alloc_slice revIDFromMeta(const DocumentMeta&) override;
        DataFile::FleeceAccessor fleeceAccessor() const override;
    };


    /** DocumentFactory subclass for version-vector (RevisionStore) document schema. */
    class VectorDocumentFactory : public DocumentFactory {
    public:
        VectorDocumentFactory(Database *db);
        Document* newDocumentInstance(C4Slice docID) override;
        Document* newDocumentInstance(const Record&) override;
        alloc_slice revIDFromMeta(const DocumentMeta&) override;

        CASRevisionStore& revisionStore();

    private:
        unique_ptr<CASRevisionStore> _revisionStore;
    };

}


// This is the struct that's forward-declared in the public c4Database.h
struct c4Database : public c4Internal::Database {
    c4Database(const FilePath &path, C4DatabaseConfig config)
    :Database(path, config) { }
    
    bool mustUseVersioning(C4DocumentVersioning, C4Error*) noexcept;
    bool mustBeInTransaction(C4Error *outError) noexcept;
    bool mustNotBeInTransaction(C4Error *outError) noexcept;
    static bool rekeyDataFile(DataFile*, const C4EncryptionKey*, C4Error*) noexcept;
};

