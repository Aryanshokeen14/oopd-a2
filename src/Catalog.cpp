#include "bookmgmt/Catalog.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include "bookmgmt/Book.h"
#include "bookmgmt/EBook.h"
#include "bookmgmt/Journal.h"
#include "bookmgmt/ElectronicResource.h"
#include <limits>

#include "bookmgmt/Exceptions.h"

namespace bookmgmt {

namespace {
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return s;
}
}  // namespace

Resource& Catalog::add(std::unique_ptr<Resource> r) {
    if (!r) throw std::invalid_argument("cannot add a null resource");
    const std::string id = r->id();
    if (items_.count(id)) throw DuplicateIdError(id);
    Entry& e = items_[id];
    e.resource = std::move(r);
    return *e.resource;
}

bool Catalog::contains(const std::string& id) const { return items_.count(id) > 0; }

Resource* Catalog::find(const std::string& id) {
    auto it = items_.find(id);
    return it == items_.end() ? nullptr : it->second.resource.get();
}

const Resource* Catalog::find(const std::string& id) const {
    auto it = items_.find(id);
    return it == items_.end() ? nullptr : it->second.resource.get();
}

Resource& Catalog::get(const std::string& id) {
    if (Resource* r = find(id)) return *r;
    throw NotFoundError(id);
}

const Resource& Catalog::get(const std::string& id) const {
    if (const Resource* r = find(id)) return *r;
    throw NotFoundError(id);
}

void Catalog::remove(const std::string& id) {
    if (items_.erase(id) == 0) throw NotFoundError(id);
}

int Catalog::holdings(const std::string& id) const {
    auto it = items_.find(id);
    if (it == items_.end()) throw NotFoundError(id);
    return it->second.holdings;
}


void Catalog::addHoldings(const std::string& id, int units) {
    auto it = items_.find(id);
    if (it == items_.end()) {
        throw NotFoundError(id);
    }

    Entry& entry = it->second;

    if (units < 0 && units < -entry.holdings) {
        throw std::invalid_argument("holdings cannot become negative");
    }

    const int borrowed = [&entry]() {
        int total = 0;
        for (const auto& [patron, count] : entry.borrowedByPatron) {
            (void)patron;
            total += count;
        }
        return total;
    }();

    const int sessions = [&entry]() {
        int total = 0;
        for (const auto& [patron, count] : entry.sessionsByPatron) {
            (void)patron;
            total += count;
        }
        return total;
    }();

    const int newHoldings = entry.holdings + units;

    if (newHoldings < borrowed || newHoldings < sessions) {
        throw std::invalid_argument(
            "holdings cannot be reduced below active loans or sessions");
    }

    entry.holdings = newHoldings;
}

// Q14: Lending print copies.
void Catalog::borrowCopy(const std::string& id,
                         const std::string& patron) {
    auto it = items_.find(id);
    if (it == items_.end()) {
        throw NotFoundError(id);
    }

    Entry& entry = it->second;

    if (entry.resource->isDigital()) {
        throw std::invalid_argument(
            "cannot borrow a print copy of an electronic resource");
    }

    if (patron.empty()) {
        throw std::invalid_argument("patron cannot be empty");
    }

    int borrowed = 0;
    for (const auto& [name, count] : entry.borrowedByPatron) {
        (void)name;
        borrowed += count;
    }

    if (borrowed >= entry.holdings) {
        throw std::invalid_argument("no print copies available");
    }

    ++entry.borrowedByPatron[patron];
}

void Catalog::returnCopy(const std::string& id,
                         const std::string& patron) {
    auto it = items_.find(id);
    if (it == items_.end()) {
        throw NotFoundError(id);
    }

    Entry& entry = it->second;
    auto loan = entry.borrowedByPatron.find(patron);

    if (loan == entry.borrowedByPatron.end() || loan->second <= 0) {
        throw std::invalid_argument(
            "patron has no borrowed copy of this resource");
    }

    --loan->second;
    if (loan->second == 0) {
        entry.borrowedByPatron.erase(loan);
    }
}

// Q14: Electronic access sessions.
void Catalog::openSession(const std::string& id,
                          const std::string& patron) {
    auto it = items_.find(id);
    if (it == items_.end()) {
        throw NotFoundError(id);
    }

    Entry& entry = it->second;

    if (!entry.resource->isDigital()) {
        throw std::invalid_argument(
            "cannot open an electronic session for a print resource");
    }

    if (patron.empty()) {
        throw std::invalid_argument("patron cannot be empty");
    }

    int active = 0;
    for (const auto& [name, count] : entry.sessionsByPatron) {
        (void)name;
        active += count;
    }

    if (active >= entry.holdings) {
        throw std::invalid_argument("no electronic seats available");
    }

    ++entry.sessionsByPatron[patron];
}

void Catalog::closeSession(const std::string& id,
                           const std::string& patron) {
    auto it = items_.find(id);
    if (it == items_.end()) {
        throw NotFoundError(id);
    }

    Entry& entry = it->second;
    auto session = entry.sessionsByPatron.find(patron);

    if (session == entry.sessionsByPatron.end() ||
        session->second <= 0) {
        throw std::invalid_argument(
            "patron has no active session for this resource");
    }

    --session->second;
    if (session->second == 0) {
        entry.sessionsByPatron.erase(session);
    }
}

int Catalog::borrowedCopies(const std::string& id) const {
    auto it = items_.find(id);
    if (it == items_.end()) {
        throw NotFoundError(id);
    }

    int total = 0;
    for (const auto& [patron, count] : it->second.borrowedByPatron) {
        (void)patron;
        total += count;
    }
    return total;
}

int Catalog::activeSessions(const std::string& id) const {
    auto it = items_.find(id);
    if (it == items_.end()) {
        throw NotFoundError(id);
    }

    int total = 0;
    for (const auto& [patron, count] : it->second.sessionsByPatron) {
        (void)patron;
        total += count;
    }
    return total;
}


std::vector<const Resource*> Catalog::where(
    const std::function<bool(const Resource&)>& pred) const {
    std::vector<const Resource*> out;
    for (const auto& [id, entry] : items_)
        if (pred(*entry.resource)) out.push_back(entry.resource.get());
    return out;
}

std::vector<const Resource*> Catalog::all() const {
    return where([](const Resource&) { return true; });
}

std::vector<const Resource*> Catalog::byCategory(ResourceCategory c) const {
    return where([c](const Resource& r) { return r.category() == c; });
}

std::vector<const Resource*> Catalog::searchTitle(const std::string& text) const {
    const std::string needle = lower(text);
    return where([&needle](const Resource& r) {
        return lower(r.title()).find(needle) != std::string::npos;
    });
}

std::vector<const Resource*> Catalog::searchAuthor(
    const std::string& author) const {
    const std::string needle = lower(author);

    if (needle.empty()) {
        return {};
    }

    return where([&needle](const Resource& resource) {
        // Book also covers AudioBook if AudioBook derives from Book.
        if (const auto* book = dynamic_cast<const Book*>(&resource)) {
            for (const std::string& name : book->authors()) {
                if (lower(name).find(needle) != std::string::npos) {
                    return true;
                }
            }
        }

        if (const auto* ebook = dynamic_cast<const EBook*>(&resource)) {
            for (const std::string& name : ebook->authors()) {
                if (lower(name).find(needle) != std::string::npos) {
                    return true;
                }
            }
        }

        return false;
    });
}

std::vector<const Resource*> Catalog::searchIdentifier(
    const std::string& identifier) const {
    const std::string needle = lower(identifier);

    if (needle.empty()) {
        return {};
    }

    return where([&needle](const Resource& resource) {
        if (const auto* book = dynamic_cast<const Book*>(&resource)) {
            return lower(book->isbn()) == needle;
        }

        if (const auto* ebook = dynamic_cast<const EBook*>(&resource)) {
            return lower(ebook->isbn()) == needle;
        }

        if (const auto* journal = dynamic_cast<const Journal*>(&resource)) {
            return lower(journal->issn()) == needle;
        }

        return false;
    });
}

std::vector<const Resource*> Catalog::searchYearRange(
    int firstYear, int lastYear) const {
    if (firstYear > lastYear) {
        throw std::invalid_argument(
            "first publication year cannot exceed last publication year");
    }

    return where([firstYear, lastYear](const Resource& resource) {
        return resource.year() >= firstYear &&
               resource.year() <= lastYear;
    });
}

}  // namespace bookmgmt
