#include "bookmgmt/Catalog.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <istream>
#include <ostream>
#include <cctype>
#include <stdexcept>
#include "bookmgmt/Book.h"
#include "bookmgmt/AudioBook.h"
#include "bookmgmt/Thesis.h"
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

void writeCommon(std::ostream& out, const Resource& resource) {
    const Money price = resource.unitPrice();
    out << std::quoted(resource.id()) << ' '
        << std::quoted(resource.title()) << ' '
        << std::quoted(resource.publisher()) << ' '
        << resource.year() << ' '
        << price.minorUnits() << ' '
        << std::quoted(price.currency()) << ' ';
}

bool readCommon(std::istream& in, std::string& id, std::string& title,
                std::string& publisher, int& year, Money& price) {
    std::int64_t minor = 0;
    std::string currency;
    if (!(in >> std::quoted(id) >> std::quoted(title)
             >> std::quoted(publisher) >> year >> minor
             >> std::quoted(currency))) {
        return false;
    }
    price = Money::fromMinor(minor, currency);
    return true;
}

void writeAuthors(std::ostream& out, const std::vector<std::string>& authors) {
    out << authors.size() << ' ';
    for (const auto& author : authors) {
        out << std::quoted(author) << ' ';
    }
}

bool readAuthors(std::istream& in, std::vector<std::string>& authors) {
    int count = 0;
    if (!(in >> count) || count < 0 || count > 100000) {
        return false;
    }
    authors.clear();
    authors.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        std::string author;
        if (!(in >> std::quoted(author))) {
            return false;
        }
        authors.push_back(std::move(author));
    }
    return true;
}

void writeBookFields(std::ostream& out, const Book& book) {
    writeAuthors(out, book.authors());
    out << std::quoted(book.isbn()) << ' '
        << book.edition() << ' '
        << static_cast<int>(book.binding()) << ' ';
}

bool readBookFields(std::istream& in, std::vector<std::string>& authors,
                    std::string& isbn, int& edition, Binding& binding) {
    int bindingValue = 0;
    if (!readAuthors(in, authors) ||
        !(in >> std::quoted(isbn) >> edition >> bindingValue) ||
        edition < 1 || (bindingValue != static_cast<int>(Binding::Paperback) &&
                        bindingValue != static_cast<int>(Binding::Hardcover))) {
        return false;
    }
    binding = static_cast<Binding>(bindingValue);
    return true;
}

void writeElectronicFields(std::ostream& out,
                           const ElectronicResource& resource) {
    const Money fee = resource.platformFee();
    out << std::quoted(resource.accessUrl()) << ' '
        << static_cast<int>(resource.license()) << ' '
        << fee.minorUnits() << ' '
        << std::quoted(fee.currency()) << ' ';
}

bool readElectronicFields(std::istream& in, std::string& url,
                          LicenseModel& license, Money& fee) {
    int licenseValue = 0;
    std::int64_t feeMinor = 0;
    std::string feeCurrency;
    if (!(in >> std::quoted(url) >> licenseValue >> feeMinor
             >> std::quoted(feeCurrency)) ||
        (licenseValue != static_cast<int>(LicenseModel::Perpetual) &&
         licenseValue != static_cast<int>(LicenseModel::AnnualSubscription))) {
        return false;
    }
    license = static_cast<LicenseModel>(licenseValue);
    fee = Money::fromMinor(feeMinor, feeCurrency);
    return true;
}

bool recordHasNoExtraFields(std::istream& in) {
    std::string extra;
    return !(in >> extra);
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

void Catalog::save(std::ostream& output) const {
    for (const auto& [id, entry] : items_) {
        const Resource& resource = *entry.resource;
        const int held = entry.holdings;

        if (const auto* audio = dynamic_cast<const AudioBook*>(&resource)) {
            output << "AUDIOBOOK ";
            writeCommon(output, *audio);
            writeBookFields(output, *audio);
            output << std::quoted(audio->narrator()) << ' '
                   << audio->durationMinutes() << ' ' << held;
        } else if (const auto* ebook = dynamic_cast<const EBook*>(&resource)) {
            output << "EBOOK ";
            writeCommon(output, *ebook);
            writeAuthors(output, ebook->authors());
            output << std::quoted(ebook->isbn()) << ' '
                   << static_cast<int>(ebook->format()) << ' '
                   << (ebook->drmProtected() ? 1 : 0) << ' ';
            writeElectronicFields(output, *ebook);
            output << held;
        } else if (const auto* book = dynamic_cast<const Book*>(&resource)) {
            output << "BOOK ";
            writeCommon(output, *book);
            writeBookFields(output, *book);
            output << held;
        } else if (const auto* journal = dynamic_cast<const Journal*>(&resource)) {
            output << "JOURNAL ";
            writeCommon(output, *journal);
            output << std::quoted(journal->issn()) << ' '
                   << journal->issuesPerYear() << ' '
                   << journal->subscriptionYears() << ' ' << held;
        } else if (const auto* thesis = dynamic_cast<const Thesis*>(&resource)) {
            output << "THESIS ";
            writeCommon(output, *thesis);
            output << std::quoted(thesis->university()) << ' '
                   << std::quoted(thesis->degree()) << ' '
                   << std::quoted(thesis->supervisor()) << ' ' << held;
        } else if (const auto* electronic =
                       dynamic_cast<const ElectronicResource*>(&resource)) {
            output << "ELECTRONIC ";
            writeCommon(output, *electronic);
            writeElectronicFields(output, *electronic);
            output << held;
        } else {
            throw std::runtime_error(
                "cannot save unsupported resource type: " + resource.id());
        }

        output << '\n';
        if (!output) {
            throw std::runtime_error("failed while saving catalogue");
        }
    }
}

Catalog Catalog::load(std::istream& input) {
    Catalog catalog;
    std::string line;
    std::size_t lineNumber = 0;

    while (std::getline(input, line)) {
        ++lineNumber;
        if (line.find_first_not_of(" \t\r") == std::string::npos) {
            continue;
        }

        std::istringstream record(line);
        std::string type;
        std::string id;
        std::string title;
        std::string publisher;
        int year = 0;
        Money price;
        if (!(record >> type) ||
            !readCommon(record, id, title, publisher, year, price)) {
            throw std::runtime_error(
                "invalid catalogue record on line " + std::to_string(lineNumber));
        }

        std::unique_ptr<Resource> resource;
        int held = 0;

        if (type == "AUDIOBOOK" || type == "BOOK") {
            std::vector<std::string> authors;
            std::string isbn;
            int edition = 1;
            Binding binding = Binding::Paperback;
            if (!readBookFields(record, authors, isbn, edition, binding)) {
                throw std::runtime_error(
                    "invalid book fields on catalogue line " +
                    std::to_string(lineNumber));
            }

            if (type == "AUDIOBOOK") {
                std::string narrator;
                int duration = 0;
                if (!(record >> std::quoted(narrator) >> duration >> held) ||
                    duration <= 0) {
                    throw std::runtime_error(
                        "invalid audiobook fields on catalogue line " +
                        std::to_string(lineNumber));
                }
                resource = std::make_unique<AudioBook>(
                    id, title, std::move(authors), isbn, publisher, year,
                    price, narrator, duration, edition, binding);
            } else {
                if (!(record >> held)) {
                    throw std::runtime_error(
                        "invalid book holdings on catalogue line " +
                        std::to_string(lineNumber));
                }
                resource = std::make_unique<Book>(
                    id, title, std::move(authors), isbn, publisher, year,
                    price, edition, binding);
            }
        } else if (type == "EBOOK") {
            std::vector<std::string> authors;
            std::string isbn;
            int formatValue = 0;
            int drm = 0;
            std::string url;
            LicenseModel license = LicenseModel::AnnualSubscription;
            Money fee;
            if (!readAuthors(record, authors) ||
                !(record >> std::quoted(isbn) >> formatValue >> drm) ||
                formatValue < static_cast<int>(EBookFormat::PDF) ||
                formatValue > static_cast<int>(EBookFormat::HTML) ||
                (drm != 0 && drm != 1) ||
                !readElectronicFields(record, url, license, fee) ||
                !(record >> held)) {
                throw std::runtime_error(
                    "invalid ebook fields on catalogue line " +
                    std::to_string(lineNumber));
            }
            resource = std::make_unique<EBook>(
                id, title, std::move(authors), isbn,
                static_cast<EBookFormat>(formatValue), drm != 0,
                publisher, year, price, url, license, fee);
        } else if (type == "ELECTRONIC") {
            std::string url;
            LicenseModel license = LicenseModel::AnnualSubscription;
            Money fee;
            if (!readElectronicFields(record, url, license, fee) ||
                !(record >> held)) {
                throw std::runtime_error(
                    "invalid electronic-resource fields on catalogue line " +
                    std::to_string(lineNumber));
            }
            resource = std::make_unique<ElectronicResource>(
                id, title, publisher, year, price, url, license, fee);
        } else if (type == "JOURNAL") {
            std::string issn;
            int issues = 0;
            int subscriptionYears = 0;
            if (!(record >> std::quoted(issn) >> issues >>
                  subscriptionYears >> held)) {
                throw std::runtime_error(
                    "invalid journal fields on catalogue line " +
                    std::to_string(lineNumber));
            }
            resource = std::make_unique<Journal>(
                id, title, issn, issues, publisher, year, price,
                subscriptionYears);
        } else if (type == "THESIS") {
            std::string university;
            std::string degree;
            std::string supervisor;
            if (!(record >> std::quoted(university) >> std::quoted(degree) >>
                  std::quoted(supervisor) >> held)) {
                throw std::runtime_error(
                    "invalid thesis fields on catalogue line " +
                    std::to_string(lineNumber));
            }
            resource = std::make_unique<Thesis>(
                id, title, university, degree, supervisor,
                publisher, year, price);
        } else {
            throw std::runtime_error(
                "unknown resource type on catalogue line " +
                std::to_string(lineNumber) + ": " + type);
        }

        if (held < 0 || !recordHasNoExtraFields(record)) {
            throw std::runtime_error(
                "invalid trailing fields or holdings on catalogue line " +
                std::to_string(lineNumber));
        }

        catalog.add(std::move(resource));
        if (held > 0) {
            catalog.addHoldings(id, held);
        }
    }

    if (input.bad()) {
        throw std::runtime_error("failed while reading catalogue");
    }
    return catalog;
}

}  // namespace bookmgmt
