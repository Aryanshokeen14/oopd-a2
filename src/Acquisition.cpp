#include "bookmgmt/Acquisition.h"

#include <iomanip>
#include <ostream>
#include <stdexcept>
#include <cstdint>

#include "bookmgmt/Exceptions.h"

namespace bookmgmt {

AcquisitionManager::AcquisitionManager(Catalog& catalog, Budget& budget)
    : catalog_(catalog), budget_(budget) {}

Money AcquisitionManager::quote(const std::string& id, int quantity) const {
    return catalog_.get(id).costFor(quantity);
}

bool AcquisitionManager::canPurchase(const std::string& id, int quantity,
                                     std::string* reason) const {
    std::string why;
    if (const Resource* r = catalog_.find(id)) {
        if (quantity <= 0)
            why = "quantity must be positive";
        else {
            const Money preTax = r->costFor(quantity);
            const Money postTax = postTaxCost(*r, preTax);
            why = budget_.check(r->category(), quantity, postTax);
        }
    } else {
        why = "resource not found: " + id;
    }
    if (reason) *reason = why;
    return why.empty();
}

void AcquisitionManager::setPrintTaxRate(double percent) {
    if (percent < 0.0)
        throw std::invalid_argument("tax rate must not be negative");

    printTaxRate_ = percent;
}

void AcquisitionManager::setElectronicTaxRate(double percent) {
    if (percent < 0.0)
        throw std::invalid_argument("tax rate must not be negative");

    electronicTaxRate_ = percent;
}


Money AcquisitionManager::postTaxCost(const Resource& r, Money preTax) const {
    const double rate = r.isDigital() ? electronicTaxRate_ : printTaxRate_;

    if (rate == 0.0)
        return preTax;

    const auto taxMinor = static_cast<std::int64_t>(
        preTax.minorUnits() * rate / 100.0 + 0.5
    );

    return Money::fromMinor(preTax.minorUnits() + taxMinor);
}

PurchaseRecord& AcquisitionManager::record(
    const Resource* r,
    const std::string& id,
    int qty,
    Money preTaxCost,
    Money postTaxCost,
    bool approved,
    std::string reason) {

    history_.push_back(PurchaseRecord{
        nextOrderNo_++,
        id,
        r ? r->title() : std::string("(unknown)"),
        r ? r->category() : ResourceCategory::Book,
        qty,
        preTaxCost,
        postTaxCost,
        approved,
        std::move(reason)
    });

    return history_.back();
}

const PurchaseRecord& AcquisitionManager::purchase(const std::string& id, int quantity) {
    const Resource& r = catalog_.get(id);        // may throw NotFoundError
    const Money preTax = r.costFor(quantity);
    const Money postTax = postTaxCost(r, preTax);

    budget_.commit(r.category(), quantity, postTax);
    catalog_.addHoldings(id, quantity);

return record(&r, id, quantity, preTax, postTax, true, {});
}

std::vector<PurchaseRecord> AcquisitionManager::processBatch(
    const std::vector<PurchaseRequest>& reqs) {

    std::vector<PurchaseRecord> results;
    results.reserve(reqs.size());

    for (const auto& req : reqs) {
        const Resource* r = catalog_.find(req.resourceId);

        Money preTax;
        Money postTax;
        std::string why;

        if (!r) {
            why = "resource not found: " + req.resourceId;
        } else if (req.quantity <= 0) {
            why = "quantity must be positive";
        } else {
            preTax = r->costFor(req.quantity);
            postTax = postTaxCost(*r, preTax);

            // Q6: quota and overall budget are checked using post-tax cost.
            why = budget_.check(r->category(), req.quantity, postTax);
        }

        if (why.empty()) {
            results.push_back(purchase(req.resourceId, req.quantity));
        } else {
            results.push_back(
                record(r,
                       req.resourceId,
                       req.quantity,
                       preTax,
                       postTax,
                       false,
                       why)
            );
        }
    }

    return results;
}

Money AcquisitionManager::totalSpent() const {
    Money sum;
    for (const auto& rec : history_)
        if (rec.approved) sum += rec.postTaxCost;
    return sum;
}

void AcquisitionManager::printReport(std::ostream& os) const {
    os << "Order history (" << history_.size() << " orders)\n";

    for (const auto& rec : history_) {
        os << "  #" << std::setw(3) << std::left << rec.orderNo << " "
           << (rec.approved ? "APPROVED" : "REJECTED") << "  "
           << std::setw(6) << rec.resourceId
           << " x" << std::setw(3) << rec.quantity
           << "  pre-tax: " << std::setw(10) << std::right
           << rec.preTaxCost.toString()
           << "  post-tax: " << std::setw(10)
           << rec.postTaxCost.toString()
           << std::left << "  "
           << rec.title;

        if (!rec.approved)
            os << "\n        reason: " << rec.reason;

        os << "\n";
    }

    os << "Total spent: " << totalSpent() << "\n";
}

}  // namespace bookmgmt
