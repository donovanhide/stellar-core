#pragma once

// Copyright 2014 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include <memory>
#include "ledger/LedgerManager.h"
#include "ledger/AccountFrame.h"
#include "overlay/StellarXDR.h"
#include "util/types.h"

namespace soci
{
class session;
}

/*
A transaction in its exploded form.
We can get it in from the DB or from the wire
*/
namespace stellar
{
class Application;
class OperationFrame;
class LedgerDelta;
class SecretKey;
class XDROutputFileStream;
class SHA256;

class TransactionFrame;
typedef std::shared_ptr<TransactionFrame> TransactionFramePtr;

class TransactionFrame
{
  protected:
    TransactionEnvelope mEnvelope;
    TransactionResult mResult;

    AccountFrame::pointer mSigningAccount;
    std::vector<bool> mUsedSignatures;

    void clearCached();
    Hash const& mNetworkID;     // used to change the way we compute signatures
    mutable Hash mContentsHash; // the hash of the contents
    mutable Hash mFullHash;     // the hash of the contents and the sig.

    std::vector<std::shared_ptr<OperationFrame>> mOperations;

    bool loadAccount(Database& app);
    bool commonValid(Application& app, bool applying, SequenceNumber current);

    void resetSignatureTracker();
    void resetResults();
    bool checkAllSignaturesUsed();
    void markResultFailed();

  public:
    TransactionFrame(Hash const& networkID,
                     TransactionEnvelope const& envelope);
    TransactionFrame(TransactionFrame const&) = delete;
    TransactionFrame() = delete;

    static TransactionFramePtr
    makeTransactionFromWire(Hash const& networkID,
                            TransactionEnvelope const& msg);

    Hash const& getFullHash() const;
    Hash const& getContentsHash() const;

    AccountFrame::pointer
    getSourceAccountPtr() const
    {
        return mSigningAccount;
    }

    void setSourceAccountPtr(AccountFrame::pointer signingAccount);

    std::vector<std::shared_ptr<OperationFrame>> const&
    getOperations() const
    {
        return mOperations;
    }

    TransactionResult const&
    getResult() const
    {
        return mResult;
    }

    TransactionResult&
    getResult()
    {
        return mResult;
    }

    TransactionResultCode
    getResultCode() const
    {
        return getResult().result.code();
    }

    TransactionResultPair getResultPair() const;
    TransactionEnvelope const& getEnvelope() const;
    TransactionEnvelope& getEnvelope();

    SequenceNumber
    getSeqNum() const
    {
        return mEnvelope.tx.seqNum;
    }

    AccountFrame const&
    getSourceAccount() const
    {
        assert(mSigningAccount);
        return *mSigningAccount;
    }

    AccountID const&
    getSourceID() const
    {
        return mEnvelope.tx.sourceAccount;
    }

    int64_t getFee() const;

    int64_t getMinFee(Application& app) const;

    float getFeeRatio(Application& app) const;

    void addSignature(SecretKey const& secretKey);

    bool checkSignature(AccountFrame& account, int32_t neededWeight);

    bool checkValid(Application& app, SequenceNumber current);

    // collect fee, consume sequence number
    void processFeeSeqNum(LedgerDelta& delta, LedgerManager& ledgerManager);

    // apply this transaction to the current ledger
    // returns true if successfully applied
    bool apply(LedgerDelta& delta, TransactionMeta& meta, Application& app);

    // version without meta
    bool apply(LedgerDelta& delta, Application& app);

    StellarMessage toStellarMessage() const;

    AccountFrame::pointer loadAccount(Database& app,
                                      AccountID const& accountID);

    // transaction history
    void storeTransaction(LedgerManager& ledgerManager, TransactionMeta& tm,
                          int txindex, TransactionResultSet& resultSet) const;

    // fee history
    void storeTransactionFee(LedgerManager& ledgerManager,
                             LedgerEntryChanges const& changes,
                             int txindex) const;

    // access to history tables
    static TransactionResultSet getTransactionHistoryMeta(Database& db,
                                                          uint32 ledgerSeq);
    static std::vector<LedgerEntryChanges>
    getTransactionFeeMeta(Database& db, uint32 ledgerSeq);

    /*
    txOut: stream of TransactionHistoryEntry
    txResultOut: stream of TransactionHistoryResultEntry
    */
    static size_t copyTransactionsToStream(Hash const& networkID, Database& db,
                                           soci::session& sess,
                                           uint32_t ledgerSeq,
                                           uint32_t ledgerCount,
                                           XDROutputFileStream& txOut,
                                           XDROutputFileStream& txResultOut);
    static void dropAll(Database& db);

    static void deleteOldEntries(Database& db, uint32_t ledgerSeq);
};
}
