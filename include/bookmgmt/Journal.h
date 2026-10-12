// ARYAN SHOKEEN
// MT26116

#pragma once

#include <string>

#include "bookmgmt/Resource.h"

namespace bookmgmt {

class Journal : public Resource {
public:
    Journal(std::string id, std::string title, std::string issn,
            int issuesPerYear, std::string publisher, int year,
            Money annualPrice, int subscriptionYears = 1);

    const std::string& issn() const { return issn_; }
    int issuesPerYear() const { return issuesPerYear_; }
    int subscriptionYears() const { return subscriptionYears_; }

    ResourceCategory category() const override {
        return ResourceCategory::Journal;
    }

    Money costFor(int copies) const override;

protected:
    void printDetails(std::ostream& os) const override;

private:
    std::string issn_;
    int issuesPerYear_;
    int subscriptionYears_;
};

}  // namespace bookmgmt