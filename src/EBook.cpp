#include "bookmgmt/EBook.h"

#include <ostream>
#include <stdexcept>

#include "bookmgmt/Book.h" // For joinAuthors

namespace bookmgmt {

// Answer to Q2 Question:
// An e-book is also a kind of book.
// Duplicated code between Book and EBook:
// 1) The fields `authors_` (std::vector<std::string>) and `isbn_` (std::string) and their getters `authors()` and `isbn()`.
// 2) Formatting/printing authors using `joinAuthors(authors_)` and ISBN.
// 
// In C++, duplication can be avoided by:
// 1. Multiple inheritance or a mixin/interface hierarchy: e.g. having a `BibliographicBook` or `HasBookMetadata` class/interface
//    that contains `authors_` and `isbn_`, from which both `Book` (combining `Resource` + `BibliographicBook`) and
//    `EBook` (combining `ElectronicResource` + `BibliographicBook` or via virtual inheritance) inherit.
// 2. Composition (preferred OOP design principle "composition over inheritance"): creating a `BookMetadata` or `BookInfo` struct
//    holding `authors`, `isbn`, edition, etc., and having both `Book` and `EBook` contain a `BookMetadata` instance,
//    delegating getters and printing to it.

const char* eBookFormatName(EBookFormat format) {
    switch (format) {
        case EBookFormat::PDF: return "PDF";
        case EBookFormat::EPUB: return "EPUB";
        case EBookFormat::HTML: return "HTML";
    }
    return "Unknown";
}

EBook::EBook(std::string id, std::string title,
             std::vector<std::string> authors,
             std::string isbn,
             EBookFormat format,
             bool drmProtected,
             std::string publisher,
             int year,
             Money pricePerSeat,
             std::string accessUrl,
             LicenseModel license,
             Money platformFee)
    : ElectronicResource(std::move(id), std::move(title),
                         std::move(publisher), year,
                         pricePerSeat, std::move(accessUrl),
                         license, platformFee),
      authors_(std::move(authors)),
      isbn_(std::move(isbn)),
      format_(format),
      drmProtected_(drmProtected) {
}
void EBook::printDetails(std::ostream& os) const {
    ElectronicResource::printDetails(os);

    os << "  authors: " << joinAuthors(authors_) << "\n"
       << "  isbn: " << isbn_ << "\n"
       << "  format: " << eBookFormatName(format_) << "\n"
       << "  drm protected: " << (drmProtected_ ? "yes" : "no") << "\n";
}
}  // namespace bookmgmt