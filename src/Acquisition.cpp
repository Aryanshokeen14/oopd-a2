// ARYAN SHOKEEN
// MT26116

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


void AcquisitionManager::addVendor(
    const std::string& resourceId,
    const std::string& vendor,
    Money unitPrice) {

    if (vendor.empty()) {
        throw std::invalid_argument("vendor name must not be empty");
    }

    if (unitPrice.isNegative()) {
        throw std::invalid_argument("vendor price must not be negative");
    }

    const Resource& resource = catalog_.get(resourceId);
    auto& offers = vendors_[resource.title()];

    for (auto& offer : offers) {
        if (offer.name == vendor) {
            offer.unitPrice = unitPrice;
            return;
        }
    }

    offers.push_back(VendorOffer{vendor, unitPrice});
}

const AcquisitionManager::VendorOffer*
AcquisitionManager::cheapestVendor(const std::string& title) const {
    const auto it = vendors_.find(title);

    if (it == vendors_.end() || it->second.empty()) {
        return nullptr;
    }

    const VendorOffer* cheapest = &it->second.front();

    for (const auto& offer : it->second) {
        if (offer.unitPrice < cheapest->unitPrice) {
            cheapest = &offer;
        }
    }

    return cheapest;
}

std::string AcquisitionManager::vendorFor(
    const std::string& title) const {

    const VendorOffer* offer = cheapestVendor(title);
    return offer ? offer->name : "Catalog";
}

Money AcquisitionManager::costAtBestVendor(
    Resource& resource,
    int quantity) const {

    const VendorOffer* offer = cheapestVendor(resource.title());

    if (!offer) {
        return resource.costFor(quantity);
    }

    const Money originalPrice = resource.unitPrice();
    resource.setUnitPrice(offer->unitPrice);

    try {
        const Money cost = resource.costFor(quantity);
        resource.setUnitPrice(originalPrice);
        return cost;
    } catch (...) {
        resource.setUnitPrice(originalPrice);
        throw;
    }
}


Money AcquisitionManager::quote(
    const std::string& id,
    int quantity) const {

    Resource& resource = catalog_.get(id);
    return costAtBestVendor(resource, quantity);
}

bool AcquisitionManager::canPurchase(const std::string& id, int quantity,
                                     std::string* reason) const {
    std::string why;
    if (Resource* r = catalog_.find(id)) {
        if (quantity <= 0)
            why = "quantity must be positive";
        else {
            const Money preTax = costAtBestVendor(*r, quantity);
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

    Resource* r = catalog_.find(id);
    if (!r) {
        if (reason)
            *reason = "resource not found: " + id;
        return false;
    }

    try {
        const Money preTax = costAtBestVendor(*r, quantity);
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


const PurchaseRecord& AcquisitionManager::record(
    const Resource* r,
    const std::string& id,
    int qty,
    Money preTaxCost,
    Money postTaxCost,
    bool approved,
    std::string reason,
    std::string department,
    std::string vendor) {

    if (vendor.empty()) {
        vendor = r ? vendorFor(r->title()) : "Catalog";
    }

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
        std::move(department),
        std::move(vendor)
    });

    return history_.back();
}



const PurchaseRecord& AcquisitionManager::purchase(
    const std::string& id,
    int quantity) {

    Resource& r = catalog_.get(id);
    const Money preTax = costAtBestVendor(r, quantity);
    const Money postTax = postTaxCost(r, preTax);

    budget_.commit(r.title(), r.category(), quantity, postTax);
    catalog_.addHoldings(id, quantity);

    return record(
        &r, id, quantity, preTax, postTax, true, {},
        "Default", vendorFor(r.title()));
}



const PurchaseRecord& AcquisitionManager::purchase(
    const std::string& dept,
    const std::string& id,
    int quantity) {

    Budget* deptBudget = departmentBudget(dept);

    if (!deptBudget) {
        throw std::invalid_argument("department not found: " + dept);
    }

    Resource& r = catalog_.get(id);
    const Money preTax = costAtBestVendor(r, quantity);
    const Money postTax = postTaxCost(r, preTax);

    deptBudget->commit(r.title(), r.category(), quantity, postTax);
    catalog_.addHoldings(id, quantity);

    return record(
        &r, id, quantity, preTax, postTax, true, {},
        dept, vendorFor(r.title()));
}


std::vector<PurchaseRecord> AcquisitionManager::processBatch(
    const std::vector<PurchaseRequest>& reqs,
    bool allOrNothing) {

    // Q11: preflight the whole batch against a copy of the budget.
    // No real purchases occur during this validation.
    if (allOrNothing) {
        Budget trialBudget = budget_;

        std::vector<Money> preTaxCosts(reqs.size());
        std::vector<Money> postTaxCosts(reqs.size());
        std::vector<std::string> reasons(reqs.size());

        std::size_t failedIndex = reqs.size();

        for (std::size_t i = 0; i < reqs.size(); ++i) {
            const auto& req = reqs[i];
            const Resource* r = catalog_.find(req.resourceId);

            if (!r) {
                reasons[i] = "resource not found: " + req.resourceId;
            } else if (req.quantity <= 0) {
                reasons[i] = "quantity must be positive";
            } else {
                try {
                    Resource& resource = *const_cast<Resource*>(r);
                    preTaxCosts[i] = costAtBestVendor(resource, req.quantity);
                    postTaxCosts[i] = postTaxCost(*r, preTaxCosts[i]);

                    reasons[i] = trialBudget.check(
                        r->title(),
                        r->category(),
                        req.quantity,
                        postTaxCosts[i]);

                    if (reasons[i].empty()) {
                        trialBudget.commit(
                            r->title(),
                            r->category(),
                            req.quantity,
                            postTaxCosts[i]);
                    }
                } catch (const std::exception& e) {
                    reasons[i] = e.what();
                }
            }

            if (!reasons[i].empty()) {
                failedIndex = i;
                break;
            }
        }

        // If any request fails, do not execute any purchase.
        if (failedIndex != reqs.size()) {
            std::vector<PurchaseRecord> results;
            results.reserve(reqs.size());

            const std::string abortReason =
                "batch aborted because request " +
                std::to_string(failedIndex + 1) +
                " was rejected: " + reasons[failedIndex];

            for (std::size_t i = 0; i < reqs.size(); ++i) {
                const auto& req = reqs[i];
                const Resource* r = catalog_.find(req.resourceId);

                const std::string reason =
                    (i == failedIndex)
                        ? reasons[i]
                        : abortReason;

                results.push_back(record(
                    r,
                    req.resourceId,
                    req.quantity,
                    preTaxCosts[i],
                    postTaxCosts[i],
                    false,
                    reason,
                    "Default"));
            }

            return results;
        }
    }

    // Normal processing: preserved for the original non-atomic mode.
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
            Resource& resource = *const_cast<Resource*>(r);
            preTax = costAtBestVendor(resource, req.quantity);
            postTax = postTaxCost(*r, preTax);

            // Q6: quota and budget checks use post-tax cost.
            why = budget_.check(
                r->title(),
                r->category(),
                req.quantity,
                postTax);
        }

        if (why.empty()) {
            results.push_back(purchase(req.resourceId, req.quantity));
        } else {
            results.push_back(record(
                r,
                req.resourceId,
                req.quantity,
                preTax,
                postTax,
                false,
                why,
                "Default"));
        }
    }

    return results;
}

std::vector<PurchaseRecord> AcquisitionManager::processBatch(
    const std::string& dept,
    const std::vector<PurchaseRequest>& reqs,
    bool allOrNothing) {

    // Q11: validate against copies of all department budgets.
    if (allOrNothing) {
        auto trialDepartments = departments_;

        std::vector<Money> preTaxCosts(reqs.size());
        std::vector<Money> postTaxCosts(reqs.size());
        std::vector<std::string> reasons(reqs.size());
        std::vector<std::string> chargedDepartments(reqs.size());

        std::size_t failedIndex = reqs.size();

        for (std::size_t i = 0; i < reqs.size(); ++i) {
            const auto& req = reqs[i];

            const std::string chargedDept =
                req.department == "Default" ? dept : req.department;

            chargedDepartments[i] = chargedDept;

            auto budgetIt = trialDepartments.find(chargedDept);
            const Resource* r = catalog_.find(req.resourceId);

            if (budgetIt == trialDepartments.end()) {
                reasons[i] = "department not found: " + chargedDept;
            } else if (!r) {
                reasons[i] = "resource not found: " + req.resourceId;
            } else if (req.quantity <= 0) {
                reasons[i] = "quantity must be positive";
            } else {
                try {
                    Resource& resource = *const_cast<Resource*>(r);
                    preTaxCosts[i] = costAtBestVendor(resource, req.quantity);
                    postTaxCosts[i] = postTaxCost(*r, preTaxCosts[i]);

                    reasons[i] = budgetIt->second.check(
                        r->title(),
                        r->category(),
                        req.quantity,
                        postTaxCosts[i]);

                    if (reasons[i].empty()) {
                        budgetIt->second.commit(
                            r->title(),
                            r->category(),
                            req.quantity,
                            postTaxCosts[i]);
                    }
                } catch (const std::exception& e) {
                    reasons[i] = e.what();
                }
            }

            if (!reasons[i].empty()) {
                failedIndex = i;
                break;
            }
        }

        // A failed preflight means no department is charged and no
        // catalog holdings are changed.
        if (failedIndex != reqs.size()) {
            std::vector<PurchaseRecord> results;
            results.reserve(reqs.size());

            const std::string abortReason =
                "batch aborted because request " +
                std::to_string(failedIndex + 1) +
                " was rejected: " + reasons[failedIndex];

            for (std::size_t i = 0; i < reqs.size(); ++i) {
                const auto& req = reqs[i];
                const Resource* r = catalog_.find(req.resourceId);

                const std::string reason =
                    (i == failedIndex)
                        ? reasons[i]
                        : abortReason;

                results.push_back(record(
                    r,
                    req.resourceId,
                    req.quantity,
                    preTaxCosts[i],
                    postTaxCosts[i],
                    false,
                    reason,
                    chargedDepartments[i]));
            }

            return results;
        }
    }

    // Normal processing: preserved for the original non-atomic mode.
    std::vector<PurchaseRecord> results;
    results.reserve(reqs.size());

    for (const auto& req : reqs) {
        const std::string& chargedDept =
            req.department == "Default" ? dept : req.department;

        std::string reason;

        if (!canPurchase(
                chargedDept,
                req.resourceId,
                req.quantity,
                &reason)) {

            const Resource* r = catalog_.find(req.resourceId);
            Money preTax;
            Money postTax;

            if (r && req.quantity > 0) {
                try {
                    Resource& resource = *const_cast<Resource*>(r);
                    preTax = costAtBestVendor(resource, req.quantity);
                    postTax = postTaxCost(*r, preTax);
                } catch (const std::exception&) {
                    // Keep default costs when the request cannot be priced.
                }
            }

            results.push_back(record(
                r,
                req.resourceId,
                req.quantity,
                preTax,
                postTax,
                false,
                reason,
                chargedDept));

            continue;
        }

        results.push_back(
            purchase(chargedDept, req.resourceId, req.quantity));
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
           << "  Department: " << rec.department
           << "  Vendor: " << rec.vendor;

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
        order.department,
        order.vendor
    });

    return true;
}

}  // namespace bookmgmt
