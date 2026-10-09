#pragma once
// AcquisitionManager: turns purchase requests into orders, enforcing the
// Budget's quotas, updating Catalog holdings and keeping an order history.

#include <iosfwd>
#include <string>
#include <vector>

#include "bookmgmt/Budget.h"
#include "bookmgmt/Catalog.h"

namespace bookmgmt {

struct PurchaseRequest {
    std::string resourceId;
    int quantity;  // copies for print, seats for electronic
};

struct PurchaseRecord {
    int orderNo;
    std::string resourceId;
    std::string title;
    ResourceCategory category;
    int quantity;
    Money preTaxCost;
    Money postTaxCost;
    bool approved;
    std::string reason;  // why it was rejected; empty if approved
    bool cancellation;
    int originalOrderNo;
};

class AcquisitionManager {
public:
    AcquisitionManager(Catalog& catalog, Budget& budget);

    // Price of a request without buying anything. Throws NotFoundError.
    Money quote(const std::string& id, int quantity) const;

    void setPrintTaxRate(double percent);
    void setElectronicTaxRate(double percent);

    double printTaxRate() const { return printTaxRate_; }
    double electronicTaxRate() const { return electronicTaxRate_; }

    // True if the purchase would be approved; if not, `reason` explains why.
    bool canPurchase(const std::string& id, int quantity,
                     std::string* reason = nullptr) const;

    // Buys immediately. Throws NotFoundError, QuotaExceededError,
    // BudgetExceededError or std::invalid_argument. On success the budget
    // and holdings are updated and the record is added to history.
    const PurchaseRecord& purchase(const std::string& id, int quantity);

    bool cancel(int orderNo);

    // Processes requests in order; each is approved or rejected on its own
    // (never throws for a rejected request). Every outcome is recorded.
    // EXTENSION POINT: priority ordering, all-or-nothing batches, ...
    std::vector<PurchaseRecord> processBatch(const std::vector<PurchaseRequest>& reqs);

    const std::vector<PurchaseRecord>& history() const { return history_; }
    Money totalSpent() const;

    void printReport(std::ostream& os) const;

private:
    PurchaseRecord& record(const Resource* r, const std::string& id, int qty,
                           Money preTaxCost, Money postTaxCost, bool approved, std::string reason);

    
    Money postTaxCost(const Resource& r, Money preTax) const;

    Catalog& catalog_;
    Budget& budget_;
    std::vector<PurchaseRecord> history_;
    int nextOrderNo_ = 1;

    double printTaxRate_ = 0.0;
    double electronicTaxRate_ = 0.0;
};

}  // namespace bookmgmt
