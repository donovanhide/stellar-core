// Copyright 2014 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "transactions/MergeOpFrame.h"
#include "database/Database.h"
#include "ledger/TrustFrame.h"

#include "medida/meter.h"
#include "medida/metrics_registry.h"

using namespace soci;

namespace stellar
{
using xdr::operator==;

MergeOpFrame::MergeOpFrame(Operation const& op, OperationResult& res,
                           TransactionFrame& parentTx)
    : OperationFrame(op, res, parentTx)
{
}

int32_t
MergeOpFrame::getNeededThreshold() const
{
    return mSourceAccount->getHighThreshold();
}

// make sure the deleted Account hasn't issued credit
// make sure we aren't holding any credit
// make sure the we delete all the offers
// make sure the we delete all the trustlines
// move the XLM to the new account
bool
MergeOpFrame::doApply(medida::MetricsRegistry& metrics, LedgerDelta& delta,
                      LedgerManager& ledgerManager)
{
    AccountFrame::pointer otherAccount;
    Database& db = ledgerManager.getDatabase();

    otherAccount = AccountFrame::loadAccount(mOperation.body.destination(), db);

    if (!otherAccount)
    {
        metrics.NewMeter({"op-merge", "failure", "no-account"}, "operation")
            .Mark();
        innerResult().code(ACCOUNT_MERGE_NO_ACCOUNT);
        return false;
    }

    if (TrustFrame::hasIssued(getSourceID(), db))
    {
        metrics.NewMeter({"op-merge", "failure", "credit-held"}, "operation")
            .Mark();
        innerResult().code(ACCOUNT_MERGE_CREDIT_HELD);
        return false;
    }

    std::vector<TrustFrame::pointer> lines;
    TrustFrame::loadLines(getSourceID(), lines, db);
    for (auto& l : lines)
    {
        if (l->getBalance() > 0)
        {
            metrics.NewMeter({"op-merge", "failure", "has-credit"}, "operation")
                .Mark();
            innerResult().code(ACCOUNT_MERGE_HAS_CREDIT);
            return false;
        }
    }

    // delete offers
    std::vector<OfferFrame::pointer> offers;
    OfferFrame::loadOffers(getSourceID(), offers, db);
    for (auto& offer : offers)
    {
        offer->storeDelete(delta, db);
    }

    // delete trust lines
    for (auto& l : lines)
    {
        l->storeDelete(delta, db);
    }

    int64 sourceBalance = mSourceAccount->getAccount().balance;
    otherAccount->getAccount().balance += sourceBalance;
    otherAccount->storeChange(delta, db);
    mSourceAccount->storeDelete(delta, db);

    metrics.NewMeter({"op-merge", "success", "apply"}, "operation").Mark();
    innerResult().code(ACCOUNT_MERGE_SUCCESS);
    innerResult().sourceAccountBalance() = sourceBalance;
    return true;
}

bool
MergeOpFrame::doCheckValid(medida::MetricsRegistry& metrics)
{
    // makes sure not merging into self
    if (getSourceID() == mOperation.body.destination())
    {
        metrics.NewMeter({"op-merge", "invalid", "malformed-self-merge"},
                         "operation").Mark();
        innerResult().code(ACCOUNT_MERGE_MALFORMED);
        return false;
    }
    return true;
}
}
