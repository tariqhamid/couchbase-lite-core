//
//  SQLiteDatabase.cc
//  CBNano
//
//  Created by Jens Alfke on 7/21/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#include "SQLiteDatabase.hh"
#include "Document.hh"
#include "DocEnumerator.hh"
#include "Error.hh"
#include "SQLiteCpp/SQLiteCpp.h"
#include <sstream>
#include <unistd.h>

using namespace std;

namespace cbforest {

    SQLiteDatabase::SQLiteDatabase(const string &path, const Options *options)
    :Database(path, options)
    {
        reopen();
    }


    SQLiteDatabase::~SQLiteDatabase() {
        close();
    }


    void SQLiteDatabase::reopen() {
        int sqlFlags = options().writeable ? SQLite::OPEN_READWRITE : SQLite::OPEN_READONLY;
        if (options().create)
            sqlFlags |= SQLite::OPEN_CREATE;
        _sqlDb.reset(new SQLite::Database(filename().c_str(), sqlFlags));
        _sqlDb->exec("PRAGMA mmap_size=50000000");
        _sqlDb->exec("PRAGMA journal_mode=WAL");
        _sqlDb->exec("CREATE TABLE IF NOT EXISTS kvmeta (name TEXT PRIMARY KEY, lastSeq INTEGER DEFAULT 0) WITHOUT ROWID");
    }


    bool SQLiteDatabase::isOpen() const {
        return _sqlDb != nullptr;
    }


    void SQLiteDatabase::close() {
        Database::close(); // closes all the KeyStores
        _getLastSeqStmt.reset();
        _setLastSeqStmt.reset();
        _sqlDb.reset();
    }


    KeyStore* SQLiteDatabase::newKeyStore(const string &name, KeyStore::Options options) {
        return new SQLiteKeyStore(*this, name, options);
    }

    void SQLiteDatabase::deleteKeyStore(const string &name) {
        _sqlDb->exec(string("DROP TABLE IF EXISTS kv_") + name);
    }


    void SQLiteDatabase::_beginTransaction(Transaction*) {
        CBFAssert(_transaction == nullptr);
        _transaction.reset( new SQLite::Transaction(*_sqlDb) );
    }


    void SQLiteDatabase::_endTransaction(Transaction *t) {
        if (t->state() >= Transaction::kCommit)
            _transaction->commit();
        _transaction.reset(); // destructs the SQLite::Transaction, which does the actual commit
    }


    SQLite::Statement& SQLiteDatabase::compile(const unique_ptr<SQLite::Statement>& ref,
                                               string sql) const
    {
        if (ref == nullptr)
            const_cast<unique_ptr<SQLite::Statement>&>(ref).reset(new SQLite::Statement(*_sqlDb, sql));
        ref->reset();  // prepare statement to be run again
        return *ref.get();
    }


    sequence SQLiteDatabase::lastSequence(const string& keyStoreName) const {
        compile(_getLastSeqStmt, string("SELECT lastSeq FROM kvmeta WHERE name=?"));
        _getLastSeqStmt->bindNoCopy(1, keyStoreName);
        if (_getLastSeqStmt->executeStep()) {
            auto seq = (int64_t)_getLastSeqStmt->getColumn(0);
            _getLastSeqStmt->reset();
            return seq;
        }
        return 0;
    }

    void SQLiteDatabase::setLastSequence(SQLiteKeyStore &store, sequence seq) {
        compile(_setLastSeqStmt,
                string("INSERT OR REPLACE INTO kvmeta (name, lastSeq) VALUES (?, ?)"));
        _setLastSeqStmt->bindNoCopy(1, store.name());
        _setLastSeqStmt->bind(2, (int64_t)seq);
        _setLastSeqStmt->exec();
    }


    void SQLiteDatabase::deleteDatabase() {
        close();
        deleteDatabase(filename());
    }

    void SQLiteDatabase::deleteDatabase(const string &path) {
        ::unlink(path.c_str());
        ::unlink((path + ".shm").c_str());
        ::unlink((path + ".wal").c_str());
    }


    void SQLiteDatabase::compact() {
        for (auto& name : allKeyStoreNames())
            _sqlDb->exec(string("DELETE FROM kv_")+name+" WHERE deleted=1");
        _sqlDb->exec(string("VACUUM"));
        updatePurgeCount();
    }


#pragma mark - KEY-STORE:


    vector<string> SQLiteDatabase::allKeyStoreNames() {
        vector<string> names;
        SQLite::Statement allStores(*_sqlDb, string("SELECT substr(name,4) FROM sqlite_master WHERE type='table' AND name GLOB 'kv_*'"));
        while (allStores.executeStep()) {
            names.push_back(allStores.getColumn(0));
        }
        return names;
    }


    SQLiteKeyStore::SQLiteKeyStore(SQLiteDatabase &db, const string &name, KeyStore::Options options)
    :KeyStore(db, name, options)
    {
        // Create the sequence and deleted columns regardless of options, otherwise it's too
        // complicated to customize all the SQL queries to conditionally use them...
        db._sqlDb->exec(string("CREATE TABLE IF NOT EXISTS kv_"+name+" (key BLOB PRIMARY KEY, meta BLOB, body BLOB, sequence INTEGER, deleted INTEGER DEFAULT 0)"));
    }


    uint64_t SQLiteKeyStore::documentCount() const {
        stringstream sql;
        sql << "SELECT count(*) FROM kv_" << _name;
        if (_options.softDeletes)
            sql << " WHERE deleted!=1";
        db().compile(_docCountStmt, sql.str());
        if (_docCountStmt->executeStep()) {
            auto count = (int64_t)_docCountStmt->getColumn(0);
            _docCountStmt->reset();
            return count;
        }
        return 0;
    }


    static slice columnAsSlice(const SQLite::Column &col) {
        return slice(col.getBlob(), col.getBytes());
    }


    bool SQLiteKeyStore::read(Document &doc, ContentOptions options) const {
        auto &stmt = (options & kMetaOnly)
            ? db().compile(_getMetaByKeyStmt,
                          string("SELECT sequence, meta, deleted, FROM kv_")+name()+" WHERE key=?")
            : db().compile(_getByKeyStmt,
                          string("SELECT sequence, meta, deleted, body FROM kv_")+name()+" WHERE key=?");
        stmt.bindNoCopy(1, doc.key().buf, (int)doc.key().size);
        if (!stmt.executeStep())
            return false;

        if (_options.sequences)
            updateDoc(doc, (int64_t)stmt.getColumn(0), (int)stmt.getColumn(2));
        doc.setMeta(columnAsSlice(stmt.getColumn(1)));
        if (options == kDefaultContent)
            doc.setBody(columnAsSlice(stmt.getColumn(3)));
        stmt.reset();
        return true;
    }


    void SQLiteKeyStore::get(slice key, ContentOptions options, function<void(const Document&)> fn) {
        auto &stmt = (options & kMetaOnly)
        ? db().compile(_getMetaByKeyStmt,
                       string("SELECT sequence, meta, deleted FROM kv_")+name()+" WHERE key=?")
        : db().compile(_getByKeyStmt,
                       string("SELECT sequence, meta, deleted, body FROM kv_")+name()+" WHERE key=?");
        stmt.bindNoCopy(1, key.buf, (int)key.size);

        // OPT: Would be nice to avoid copying key/meta/body here; this would require Document to
        // know that the pointers are ephemeral, and create copies if they're accessed as
        // alloc_slice (not just slice).
        Document doc;
        doc.setKey(key);
        if (stmt.executeStep()) {
            if (_options.sequences)
                updateDoc(doc, (int64_t)stmt.getColumn(0), (int)stmt.getColumn(2));
            doc.setMeta(columnAsSlice(stmt.getColumn(1)));
            if (options == kDefaultContent)
                doc.setBody(columnAsSlice(stmt.getColumn(3)));
        }
        fn(doc);
        stmt.reset();   // invalidates the memory pointed to by doc
    }

    
    Document SQLiteKeyStore::get(sequence seq, ContentOptions options) const {
        if (!_options.sequences)
            error::_throw(error::NoSequences);
        Document doc;
        auto &stmt = (options & kMetaOnly)
            ? db().compile(_getMetaBySeqStmt,
                          string("SELECT key, meta, deleted FROM kv_")+name()+" WHERE sequence=?")
            : db().compile(_getBySeqStmt,
                          string("SELECT key, meta, deleted, body FROM kv_")+name()+" WHERE sequence=?");
        stmt.bind(1, (int64_t)seq);
        if (stmt.executeStep()) {
            updateDoc(doc, seq);
            doc.setKey(columnAsSlice(stmt.getColumn(0)));
            doc.setMeta(columnAsSlice(stmt.getColumn(1)));
            doc.setDeleted((int)stmt.getColumn(2));
            if (options == kDefaultContent)
                doc.setBody(columnAsSlice(stmt.getColumn(3)));
            stmt.reset();
        }
        return doc;
    }


    sequence SQLiteKeyStore::set(slice key, slice meta, slice body, Transaction&) {
        db().compile(_setStmt,
                    string("INSERT OR REPLACE INTO kv_")+name()
                        +" (key, meta, body, sequence, deleted) VALUES (?, ?, ?, ?, 0)");
        _setStmt->bindNoCopy(1, key.buf, (int)key.size);
        _setStmt->bindNoCopy(2, meta.buf, (int)meta.size);
        _setStmt->bindNoCopy(3, body.buf, (int)body.size);

        sequence seq = 0;
        if (_options.sequences) {
            seq = lastSequence() + 1;
            _setStmt->bind(4, (int64_t)seq);
        } else {
            _setStmt->bind(4);
        }
        _setStmt->exec();
        db().setLastSequence(*this, seq);
        return seq;
    }


    bool SQLiteKeyStore::_del(slice key, sequence delSeq, Transaction&) {
        auto& stmt = delSeq ? _delBySeqStmt : _delByKeyStmt;
        if (!stmt) {
            stringstream sql;
            if (_options.softDeletes) {
                sql << "UPDATE kv_" << name() << " SET deleted=1, meta=null, body=null";
                if (_options.sequences)
                    sql << ", sequence=? ";
            } else {
                sql << "DELETE FROM kv_" << name();
            }
            sql << (delSeq ? " WHERE sequence=?" : " WHERE key=?");
            db().compile(stmt, sql.str());
        }

        sequence newSeq = 0;
        int param = 1;
        if (_options.softDeletes && _options.sequences) {
            newSeq = lastSequence() + 1;
            stmt->bind(param++, (int64_t)newSeq);
        }
        if (delSeq)
            stmt->bind(param++, (int64_t)delSeq);
        else
            stmt->bindNoCopy(param++, key.buf, (int)key.size);

        bool ok = stmt->exec() > 0;
        if (ok && newSeq > 0)
            db().setLastSequence(*this, newSeq);
        return ok;
    }


    void SQLiteKeyStore::erase() {
        db()._sqlDb->exec(string("DELETE FROM kv_"+name()));
        db().setLastSequence(*this, 0);
    }


#pragma mark - ITERATOR:


    class SQLiteIterator : public DocEnumerator::Impl {
    public:
        SQLiteIterator(SQLite::Statement *stmt, ContentOptions content)
        :_stmt(stmt),
         _content(content)
        { }

        virtual bool next() override {
            return _stmt->executeStep();
        }

        virtual bool seek(slice key) override {
            error::_throw(error::Unimplemented); //FIX
        }

        virtual bool read(Document &doc) override {
            updateDoc(doc, (int64_t)_stmt->getColumn(0), 0, (int)_stmt->getColumn(3));
            doc.setKey(columnAsSlice(_stmt->getColumn(1)));
            doc.setMeta(columnAsSlice(_stmt->getColumn(2)));
            if (_content == kDefaultContent)
                doc.setBody(columnAsSlice(_stmt->getColumn(4)));
            return true;
        }

    private:
        unique_ptr<SQLite::Statement> _stmt;
        ContentOptions _content;
    };


    stringstream SQLiteKeyStore::selectFrom(const DocEnumerator::Options &options) {
        stringstream sql;
        sql << "SELECT sequence, key, meta, deleted";
        if (options.contentOptions == kDefaultContent)
            sql << ", body";
        sql << " FROM kv_" << name();
        return sql;
    }

    void SQLiteKeyStore::writeSQLOptions(stringstream &sql, DocEnumerator::Options &options) {
        if (options.descending)
            sql << " DESC";
        if (options.limit < UINT_MAX) {
            sql << " LIMIT " << options.limit;
        }
        if (options.skip > 0) {
            if (options.limit == UINT_MAX)
                sql << " LIMIT -1";             // OFFSET has to have a LIMIT before it
            sql << " OFFSET " << options.skip;
            options.skip = 0;                   // tells DocEnumerator not to do skip on its own
        }
        options.limit = UINT_MAX;               // ditto for limit
    }


    // iterate by key:
    DocEnumerator::Impl* SQLiteKeyStore::newEnumeratorImpl(slice minKey, slice maxKey,
                                                           DocEnumerator::Options &options)
    {
        db()._sqlDb->exec(string("CREATE INDEX IF NOT EXISTS kv_"+name()+"_keys ON kv_"+name()+" (key)"));

        stringstream sql = selectFrom(options);
        if (minKey.buf) {
            sql << (options.inclusiveMin() ? " WHERE key >= ?" : " WHERE key > ?");
            if (maxKey.buf)
                sql << " AND ";
        }
        if (maxKey.buf) {
            sql << (options.inclusiveMax() ? " key <= ?" : " key < ?");
        }
        if (_options.softDeletes && !options.includeDeleted)
            sql << " AND deleted!=1";
        sql << " ORDER BY key";
        writeSQLOptions(sql, options);

        auto st = new SQLite::Statement(db(), sql.str());    //TODO: Cache a statement
        int param = 1;
        if (minKey.buf)
            st->bind(param++, minKey.buf, (int)minKey.size);
        if (maxKey.buf)
            st->bind(param++, maxKey.buf, (int)maxKey.size);
        return new SQLiteIterator(st, options.contentOptions);
    }

    // iterate by sequence:
    DocEnumerator::Impl* SQLiteKeyStore::newEnumeratorImpl(sequence min, sequence max,
                                                           DocEnumerator::Options &options)
    {
        if (!_options.sequences)
            error::_throw(error::NoSequences);
        db()._sqlDb->exec(string("CREATE UNIQUE INDEX IF NOT EXISTS kv_"+name()+"_seqs ON kv_"+name()+" (sequence)"));

        stringstream sql = selectFrom(options);
        sql << (options.inclusiveMin() ? " WHERE sequence >= ?" : " WHERE sequence > ?");
        if (max < INT64_MAX)
            sql << (options.inclusiveMax() ? " AND sequence <= ?"   : " AND sequence < ?");
        if (_options.softDeletes && !options.includeDeleted)
            sql << " AND deleted!=1";
        sql << " ORDER BY sequence";
        writeSQLOptions(sql, options);

        auto st = new SQLite::Statement(db(), sql.str());        // TODO: Cache a statement
        st->bind(1, (int64_t)min);
        if (max < INT64_MAX)
            st->bind(2, (int64_t)max);
        return new SQLiteIterator(st, options.contentOptions);
    }

}
