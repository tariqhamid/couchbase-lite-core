package com.couchbase.litecore;

import com.couchbase.litecore.fleece.FLSliceResult;

import java.io.File;

/**
 * Blob Store API
 */
public class C4BlobStore {
    //-------------------------------------------------------------------------
    // Member Variables
    //-------------------------------------------------------------------------
    private long handle = 0L; // hold pointer to C4BlobKey

    //-------------------------------------------------------------------------
    // public methods
    //-------------------------------------------------------------------------

    /**
     * Opens a BlobStore in a directory. If the flags allow creating, the directory will be
     * created if necessary.
     * NOTE: Call free() when finished using the BlobStore.
     *
     * @param dirPath The filesystem path of the directory holding the attachments.
     * @param flags   Specifies options like create, read-only
     *                //@param encryptionKey  Optional encryption algorithm & key
     * @return The BlobStore reference
     * @throws LiteCoreException for any error
     */
    public static C4BlobStore open(String dirPath, long flags) throws LiteCoreException {
        if (!dirPath.endsWith(File.separator))
            dirPath = dirPath + File.separator;
        return new C4BlobStore(openStore(dirPath, flags));
    }

    /**
     * Deletes the BlobStore's blobs and directory, and (if successful) frees the object.
     */
    public void delete() throws LiteCoreException {
        deleteStore(handle);
        // NOTE: deleteStore() native method release memory.
        this.handle = 0L;
    }

    /**
     * Closes/frees a BlobStore.
     */
    public void free() {
        if (handle != 0L) {
            freeStore(handle);
            handle = 0L;
        }
    }

    /**
     * Gets the content size of a blob given its key. Returns -1 if it doesn't exist.
     * WARNING: If the blob is encrypted, the return value is a conservative estimate that may
     * be up to 16 bytes larger than the actual size.
     */
    public long getSize(C4BlobKey blobKey) {
        return getSize(handle, blobKey.getHandle());
    }

    /**
     * Reads the entire contents of a blob into memory. Caller is responsible for freeing it.
     */
    public FLSliceResult getContents(C4BlobKey blobKey) throws LiteCoreException {
        return new FLSliceResult(getContents(handle, blobKey.getHandle()));
    }

    /**
     * Returns the path of the file that stores the blob, if possible. This call may fail with
     * error kC4ErrorWrongFormat if the blob is encrypted (in which case the file would be
     * unreadable by the caller) or with kC4ErrorUnsupported if for some implementation reason
     * the blob isn't stored as a standalone file.
     * Thus, the caller MUST use this function only as an optimization, and fall back to reading
     * he contents via the API if it fails.
     * Also, it goes without saying that the caller MUST not modify the file!
     */
    public String getFilePath(C4BlobKey blobKey) throws LiteCoreException {
        return getFilePath(handle, blobKey.getHandle());
    }

    /**
     * Stores a blob. The associated key will be written to `outKey`.
     */
    public C4BlobKey create(byte[] contents) throws LiteCoreException {
        return new C4BlobKey(create(handle, contents));
    }

    /**
     * Deletes a blob from the store given its key.
     */
    public void delete(C4BlobKey blobKey) throws LiteCoreException {
        delete(handle, blobKey.getHandle());
    }

    /**
     * Opens a blob for reading, as a random-access byte stream.
     */
    public C4BlobReadStream openReadStream(C4BlobKey blobKey) throws LiteCoreException {
        return new C4BlobReadStream(openReadStream(handle, blobKey.getHandle()));
    }

    /**
     * Opens a write stream for creating a new blob. You should then call c4stream_write to
     * write the data, ending with c4stream_install to compute the blob's key and add it to
     * the store, and then c4stream_closeWriter.
     */
    public C4BlobWriteStream openWriteStream() throws LiteCoreException {
        return new C4BlobWriteStream(openWriteStream(handle));
    }

    //-------------------------------------------------------------------------
    // protected methods
    //-------------------------------------------------------------------------
    @Override
    protected void finalize() throws Throwable {
        free();
        super.finalize();
    }

    //-------------------------------------------------------------------------
    // package methods
    //-------------------------------------------------------------------------
    C4BlobStore(long handle) {
        if (handle == 0)
            throw new IllegalArgumentException("handle is 0");
        this.handle = handle;
    }

    //-------------------------------------------------------------------------
    // native methods
    //-------------------------------------------------------------------------

    private native static long openStore(String dirPath, long flags) throws LiteCoreException;

    private native static void deleteStore(long blobStore) throws LiteCoreException;

    private native static void freeStore(long blobStore);

    private native static long getSize(long blobStore, long blobKey);

    private native static long getContents(long blobStore, long blobKey) throws LiteCoreException;

    private native static String getFilePath(long blobStore, long blobKey) throws LiteCoreException;

    private native static long create(long blobStore, byte[] contents) throws LiteCoreException;

    private native static void delete(long blobStore, long blobKey) throws LiteCoreException;

    private native static long openReadStream(long blobStore, long blobKey) throws LiteCoreException;

    private native static long openWriteStream(long blobStore) throws LiteCoreException;
}
