#pragma once

#include "bookmgmt/Book.h"

namespace bookmgmt {

/*
 * Q3 design decision:
 *
 * AudioBook derives from Book because an audiobook represents a book
 * and can reuse the existing book information such as authors, ISBN,
 * edition, binding, publisher, year, and unit price. It only adds
 * audiobook-specific information: narrator and duration.
 */
class AudioBook : public Book {
public:
    AudioBook(std::string id, std::string title,
              std::vector<std::string> authors,
              std::string isbn, std::string publisher, int year,
              Money unitPrice, std::string narrator,
              int durationMinutes,
              int edition = 1,
              Binding binding = Binding::Paperback);

    const std::string& narrator() const { return narrator_; }
    int durationMinutes() const { return durationMinutes_; }

protected:
    void printDetails(std::ostream& os) const override;

private:
    std::string narrator_;
    int durationMinutes_;
};

}  // namespace bookmgmt