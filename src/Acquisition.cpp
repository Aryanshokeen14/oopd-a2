#include "bookmgmt/Acquisition.h"

#include <iomanip>
#include <ostream>
#include <stdexcept>
#include <cstdint>
#include <utility>

#include "bookmgmt/Exceptions.h"

namespace bookmgmt {

AcquisitionManager::AcquisitionManager(Catalog& catalog, Budget& budget)
    : catalog_(catalog), budget_(budget) {}

void AcquisitionManager::addDepartment(const std::string& dept,
                                       Budget budget) {
    if (dept.empty())
        throw std::invalid_argument("department name must not be empty");

    departments_.insert_or_assign(dept, std::move(budget));
}

Budget* AcquisitionManager::departmentBudget(const std::string& dept) {
    auto it = departments_.find(dept);
    return it == departments_.end() ? nullptr : &it->second;
}

const Budget* AcquisitionManager::departmentBudget(
    const std::string& dept) const {
    auto it = departments_.find(dept);
    return it == departments_.end() ? nullptr : &it->second;
}

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
            why = budget_.check(r->title(), r->category(), quantity, postTax);
        }
    } else {
        why = "resource not found: " + id;
    }
    if (reason) *reason = why;
    return why.empty();
}

bool AcquisitionManager::canPurchase(const std::string& dept,
                                     const std::string& id,
                                     int quantity,
                                     std::string* reason) const {
    const Budget* deptBudget = departmentBudget(dept);

    if (!deptBudget) {
        if (reason)
            *reason = "department not found: " + dept;
        return false;
    }

    const Resource* r = catalog_.find(id);
    if (!r) {
        if (reason)
            *reason = "resource not found: " + id;
        return false;
    }

    try {
        const Money preTax = r->costFor(quantity);
        const Money postTax = postTaxCost(*r, preTax);
        const std::string why =
            deptBudget->check(r->title(), r->category(), quantity, postTax);

        if (reason)
            *reason = why;

        return why.empty();
    } catch (const std::exception& e) {
        if (reason)
            *reason = e.what();
        return false;
    }
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
    std::string reason,
    std::string department) {

    history_.push_back(PurchaseRecord{
        nextOrderNo_++,
        id,
        r ? r->title() : std::string("(unknown)"),
        r ? r->category() : ResourceCategory::Book,
        qty,
        preTaxCost,
        postTaxCost,
        approved,
        std::move(reason),
        false,
        0,
        department
    });

    return history_.back();
}

const PurchaseRecord& AcquisitionManager::purchase(const std::string& id, int quantity) {
    const Resource& r = catalog_.get(id);        // may throw NotFoundError
    const Money preTax = r.costFor(quantity);
    const Money postTax = postTaxCost(r, preTax);

    budget_.commit(r.title(), r.category(), quantity, postTax);
    catalog_.addHoldings(id, quantity);

return record(&r, id, quantity, preTax, postTax, true, {});
}

const PurchaseRecord& AcquisitionManager::purchase(
    const std::string& dept,
    const std::string& id,
    int quantity) {

    Budget* deptBudget = departmentBudget(dept);
    if (!deptBudget)
        throw std::invalid_argument("department not found: " + dept);

    const Resource& r = catalog_.get(id);
    const Money preTax = r.costFor(quantity);
    const Money postTax = postTaxCost(r, preTax);

    deptBudget->commit(r.title(), r.category(), quantity, postTax);
    catalog_.addHoldings(id, quantity);

    return record(&r, id, quantity, preTax, postTax, true, {}, dept);
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
            why = budget_.check(r->title(), r->category(), req.quantity, postTax);
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

std::vector<PurchaseRecord> AcquisitionManager::processBatch(
    const std::string& dept,
    const std::vector<PurchaseRequest>& reqs) {

    std::vector<PurchaseRecord> results;

    for (const auto& req : reqs) {
        const std::string& chargedDept =
            req.department == "Default" ? dept : req.department;

        std::string reason;
        if (!canPurchase(chargedDept, req.resourceId, req.quantity, &reason)) {
            const Resource* r = catalog_.find(req.resourceId);
            const Money preTax = r ? r->costFor(req.quantity) : Money{};
            const Money postTax = r ? postTaxCost(*r, preTax) : Money{};

            results.push_back(record(r, req.resourceId, req.quantity,
                                     preTax, postTax, false, reason,
                                     chargedDept));
            continue;
        }

        results.push_back(purchase(chargedDept, req.resourceId, req.quantity));
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
           << (rec.cancellation ? "CANCELLED" :
                (rec.approved ? "APPROVED" : "REJECTED")) << "  "
           << std::setw(6) << rec.resourceId
           << " x" << std::setw(3) << rec.quantity
           << "  pre-tax: " << std::setw(10) << std::right
           << rec.preTaxCost.toString()
           << "  post-tax: " << std::setw(10)
           << rec.postTaxCost.toString()
           << std::left << "  "
           << rec.title
           << "  Department: " << rec.department;

        if (rec.cancellation)
            os << "\n        cancellation of order #" << rec.originalOrderNo;
        else if (!rec.approved)
            os << "\n        reason: " << rec.reason;

        os << "\n";
    }

    os << "Total spent: " << totalSpent() << "\n";
}


bool AcquisitionManager::cancel(int orderNo) {
    // Check if this order was already cancelled
    for (const auto& rec : history_) {
        if (rec.cancellation && rec.originalOrderNo == orderNo) {
            return false;
        }
    }

    PurchaseRecord order;
    bool found = false;

    for (const auto& rec : history_) {
        if (rec.orderNo == orderNo) {
            order = rec;
            found = true;
            break;
        }
    }

    if (!found || !order.approved || order.cancellation)
        return false;

    const Resource* r = catalog_.find(order.resourceId);
    if (!r)
        return false;

    if (order.department == kDefaultDepartment) {
        budget_.refund(order.title, order.category,
                   order.quantity, order.postTaxCost);
    } else {
        Budget* deptBudget = departmentBudget(order.department);
        if (!deptBudget)
            return false;

    deptBudget->refund(order.title, order.category,
                       order.quantity, order.postTaxCost);
    }

    catalog_.addHoldings(order.resourceId, -order.quantity);

    history_.push_back(PurchaseRecord{
        nextOrderNo_++,
        order.resourceId,
        order.title,
        order.category,
        order.quantity,
        Money{} - order.preTaxCost,
        Money{} - order.postTaxCost,
        true,
        "Cancellation of order #" + std::to_string(order.orderNo),
        true,
        order.orderNo,
        order.department
    });

    return true;
}

}  // namespace bookmgmt
