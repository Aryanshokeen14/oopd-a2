// ARYAN SHOKEEN
// MT26116

#include "bookmgmt/Book.h"

#include <ostream>
#include <stdexcept>

namespace bookmgmt {

std::string joinAuthors(const std::vector<std::string>& authors) {
    std::string out;
    for (std::size_t i = 0; i < authors.size(); ++i) {
        if (i > 0) out += (i + 1 == authors.size()) ? " and " : ", ";
        out += authors[i];
    }
    return out;
}

Book::Book(std::string id, std::string title, std::vector<std::string> authors,
           std::string isbn, std::string publisher, int year, Money unitPrice,
           int edition, Binding binding)
    : Resource(std::move(id), std::move(title), std::move(publisher), year, unitPrice),
      authors_(std::move(authors)),
      isbn_(std::move(isbn)),
      edition_(edition),
      binding_(binding) {
    if (edition_ < 1) throw std::invalid_argument("edition must be >= 1");
}

void Book::printDetails(std::ostream& os) const {
    os << "  authors: " << joinAuthors(authors_) << "\n"
       << "  isbn: " << isbn_ << "\n"
       << "  edition: " << edition_ << "\n"
       << "  binding: " << (binding_ == Binding::Hardcover ? "Hardcover" : "Paperback")
       << "\n";
}

Money Book::costFor(int copies) const {
    requirePositive(copies);

    Money total = unitPrice() * copies;

    // Q4: Hardcover costs 20% more.
    if (binding_ == Binding::Hardcover) {
        const auto minor = total.minorUnits();
        total = Money::fromMinor(minor + minor / 5);
    }

    // Q5: Print resources get 10% off for 10 or more copies.
    if (copies >= 10) {
        const auto minor = total.minorUnits();
        total = Money::fromMinor(minor - minor / 10);
    }

    return total;
}

}  // namespace bookmgmt
