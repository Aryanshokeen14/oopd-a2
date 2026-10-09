#include "bookmgmt/Budget.h"

#include <iomanip>
#include <ostream>
#include <stdexcept>
#include <iostream>

#include "bookmgmt/Exceptions.h"

namespace bookmgmt {

namespace {
// Every category, in the order Budget::print() lists them.
const ResourceCategory kAllCategories[] = {ResourceCategory::Book,ResourceCategory::Journal,
                                           ResourceCategory::ElectronicResource, 
                                           ResourceCategory::EBook,ResourceCategory::Thesis};
}

Budget::Budget(Money total) : total_(total) {
    if (total_.isNegative()) throw std::invalid_argument("budget must not be negative");
}

void Budget::setQuota(ResourceCategory c, Quota q) {
    if (q.maxUnits < 0 || q.maxSpend.isNegative())
        throw std::invalid_argument("quota limits must not be negative");
    quotas_[c] = q;
}

void Budget::removeQuota(ResourceCategory c) { quotas_.erase(c); }

void Budget::setTitleQuota(const std::string& title, int maxUnits) {
    if (maxUnits < 0)
        throw std::invalid_argument("title quota must not be negative");

    titleQuotas_[title] = maxUnits;
}

void Budget::removeTitleQuota(const std::string& title) {
    titleQuotas_.erase(title);
}

std::optional<int> Budget::titleQuotaFor(const std::string& title) const {
    auto it = titleQuotas_.find(title);
    if (it == titleQuotas_.end())
        return std::nullopt;

    return it->second;
}

int Budget::titleUsage(const std::string& title) const {
    auto it = titleUsage_.find(title);
    if (it == titleUsage_.end())
        return 0;

    return it->second;
}

std::string Budget::check(const std::string& title,
                          ResourceCategory c,
                          int units,
                          Money cost) const {
    if (auto limit = titleQuotaFor(title)) {
        const int used = titleUsage(title);

        if (used + units > *limit) {
            return "title quota exceeded for \"" + title +
                   "\": requested " + std::to_string(units) +
                   ", " + std::to_string(*limit - used) + " remaining";
        }
    }

    return check(c, units, cost);
}

void Budget::commit(const std::string& title,
                    ResourceCategory c,
                    int units,
                    Money cost) {
    if (auto limit = titleQuotaFor(title)) {
        const int used = titleUsage(title);

        if (used + units > *limit) {
            throw QuotaExceededError(
                "title quota exceeded for \"" + title +
                "\": requested " + std::to_string(units) +
                ", " + std::to_string(*limit - used) +
                " remaining"
            );
        }
    }

    commit(c, units, cost);
    titleUsage_[title] += units;
}

std::optional<Quota> Budget::quotaFor(ResourceCategory c) const {
    auto it = quotas_.find(c);
    if (it == quotas_.end()) return std::nullopt;
    return it->second;
}

Usage Budget::usageFor(ResourceCategory c) const {
    auto it = usage_.find(c);
    return it == usage_.end() ? Usage{} : it->second;
}

std::optional<int> Budget::unitsRemaining(ResourceCategory c) const {
    auto q = quotaFor(c);
    if (!q) return std::nullopt;
    return q->maxUnits - usageFor(c).units;
}

std::optional<Money> Budget::spendRemaining(ResourceCategory c) const {
    auto q = quotaFor(c);
    if (!q) return std::nullopt;
    return q->maxSpend - usageFor(c).spent;
}

Budget::Failure Budget::evaluate(ResourceCategory c, int units, Money cost,
                                 std::string& why) const {
    if (units <= 0) {
        why = "quantity must be positive";
        return Failure::BadInput;
    }
    if (cost.isNegative()) {
        why = "cost must not be negative";
        return Failure::BadInput;
    }
    if (auto left = unitsRemaining(c); left && units > *left) {
        why = std::string(categoryName(c)) + " unit quota exceeded: requested " +
              std::to_string(units) + ", " + std::to_string(*left) + " remaining";
        return Failure::Quota;
    }
    if (auto left = spendRemaining(c); left && cost > *left) {
        why = std::string(categoryName(c)) + " spend quota exceeded: cost " +
              cost.toString() + ", " + left->toString() + " remaining";
        return Failure::Quota;
    }
    if (cost > remaining()) {
        why = "overall budget exceeded: cost " + cost.toString() + ", " +
              remaining().toString() + " remaining";
        return Failure::Overall;
    }
    why.clear();
    return Failure::None;
}

std::string Budget::check(ResourceCategory c, int units, Money cost) const {
    std::string why;
    evaluate(c, units, cost, why);
    return why;
}

void Budget::commit(ResourceCategory c, int units, Money cost) {
    std::string why;
    switch (evaluate(c, units, cost, why)) {
        case Failure::None: break;
        case Failure::BadInput: throw std::invalid_argument(why);
        case Failure::Quota: throw QuotaExceededError(why);
        case Failure::Overall: throw BudgetExceededError(why);
    }
    Usage& u = usage_[c];
    u.units += units;
    u.spent += cost;
    spent_ += cost;
}

void Budget::refund(ResourceCategory c, int units, Money cost) {
    if (units <= 0)
        throw std::invalid_argument("refund units must be positive");

    if (cost.isNegative())
        throw std::invalid_argument("refund cost must not be negative");

    Usage& u = usage_[c];

    if (units > u.units || cost > u.spent || cost > spent_)
        throw std::invalid_argument("refund exceeds recorded usage");

    u.units -= units;
    u.spent -= cost;
    spent_ -= cost;
}

void Budget::refund(const std::string& title,
                    ResourceCategory c,
                    int units,
                    Money cost) {
    if (units <= 0)
        throw std::invalid_argument("refund units must be positive");

    const int used = titleUsage(title);
    
    if (units > used)
        throw std::invalid_argument("refund exceeds title usage");

    refund(c, units, cost);
    titleUsage_[title] -= units;
}

void Budget::print(std::ostream& os) const {
    os << "Budget: total " << total_ << ", spent " << spent_ << ", remaining "
       << remaining() << "\n";
    os << std::left << std::setw(22) << "  Category" << std::setw(18) << "Units used/max"
       << "Spend used/max\n";
    for (ResourceCategory c : kAllCategories) {
        const Usage u = usageFor(c);
        const auto q = quotaFor(c);
        const std::string units =
            std::to_string(u.units) + "/" + (q ? std::to_string(q->maxUnits) : "-");
        const std::string spend =
            u.spent.toString() + "/" + (q ? q->maxSpend.toString() : "-");
        os << "  " << std::setw(20) << categoryName(c) << std::setw(18) << units << spend
           << "\n";
    }
}

}  // namespace bookmgmt
