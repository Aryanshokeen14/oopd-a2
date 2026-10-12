#pragma once
// Catalog: owns every Resource, keyed by its id, and tracks copies/seats held.

#include <functional>
#include <iosfwd>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <utility>

#include "bookmgmt/Resource.h"

namespace bookmgmt {

class Catalog {
public:
    // Takes ownership. Throws DuplicateIdError if the id is already present.
    Resource& add(std::unique_ptr<Resource> r);

    // Convenience: catalog.emplace<Book>(ctor args...)
    template <typename T, typename... Args>
    T& emplace(Args&&... args) {
        auto p = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *p;
        add(std::move(p));
        return ref;
    }

    bool contains(const std::string& id) const;
    Resource* find(const std::string& id);              // nullptr if absent
    const Resource* find(const std::string& id) const;  // nullptr if absent
    Resource& get(const std::string& id);               // throws NotFoundError
    const Resource& get(const std::string& id) const;   // throws NotFoundError
    void remove(const std::string& id);                 // throws NotFoundError

    std::size_t size() const { return items_.size(); }
    bool empty() const { return items_.empty(); }

    // Holdings: number of copies (print) or seats (electronic) owned.
    int holdings(const std::string& id) const;
    void addHoldings(const std::string& id, int units);

    // Q14: Lending and electronic access sessions.
    void borrowCopy(const std::string& id, const std::string& patron);
    void returnCopy(const std::string& id, const std::string& patron);

    void openSession(const std::string& id, const std::string& patron);
    void closeSession(const std::string& id, const std::string& patron);

    int borrowedCopies(const std::string& id) const;
    int activeSessions(const std::string& id) const;


    // Queries. Results are in id order and point into the catalog.
    std::vector<const Resource*> all() const;
    std::vector<const Resource*> byCategory(ResourceCategory c) const;
    std::vector<const Resource*> searchTitle(const std::string& text) const;  // case-insensitive

    // Additional catalogue searches for Q13.
    std::vector<const Resource*> searchAuthor(const std::string& author) const;
    std::vector<const Resource*> searchIdentifier(
        const std::string& identifier) const;  // ISBN or ISSN
    std::vector<const Resource*> searchYearRange(int firstYear, int lastYear) const;

    std::vector<const Resource*> where(
        const std::function<bool(const Resource&)>& pred) const;

    // Q13: Save one typed resource record per line and reconstruct the
    // corresponding derived resource class when loading.
    void save(std::ostream& output) const;
    static Catalog load(std::istream& input);

private:
    struct Entry {
        std::unique_ptr<Resource> resource;
        int holdings = 0;

        // Q14: Active loans and electronic sessions, tracked by patron.
        std::map<std::string, int> borrowedByPatron;
        std::map<std::string, int> sessionsByPatron;
    };

    std::map<std::string, Entry> items_;
};

}  // namespace bookmgmt
