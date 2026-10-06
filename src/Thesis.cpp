#include "bookmgmt/Thesis.h"

#include <ostream>

namespace bookmgmt {

Thesis::Thesis(std::string id, std::string title,
               std::string university,
               std::string degree,
               std::string supervisor,
               std::string publisher,
               int year,
               Money unitPrice)
    : Resource(std::move(id), std::move(title),
               std::move(publisher), year, unitPrice),
      university_(std::move(university)),
      degree_(std::move(degree)),
      supervisor_(std::move(supervisor)) {
}

void Thesis::printDetails(std::ostream& os) const {
    os << "  university: " << university_ << "\n"
       << "  degree: " << degree_ << "\n"
       << "  supervisor: " << supervisor_ << "\n";
}

}  // namespace bookmgmt