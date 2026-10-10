// Demo: builds a small catalog, sets a budget with per-category quotas,
// and runs a batch of purchase requests through the acquisition manager.

#include <iostream>

#include "bookmgmt/bookmgmt.h"

using namespace bookmgmt;

int main()
{
    Catalog catalog;

    catalog.emplace<Book>("B001", "Clean Code", std::vector<std::string>{"Robert C. Martin"},
                          "978-0132350884", "Prentice Hall", 2008, Money::of(450));
    catalog.emplace<Book>("B002", "The C++ Programming Language",
                          std::vector<std::string>{"Bjarne Stroustrup"}, "978-0321563842",
                          "Addison-Wesley", 2013, Money::of(1200), 4, Binding::Hardcover);
    catalog.emplace<ElectronicResource>("R001", "IEEE Xplore Digital Library", "IEEE", 2026,
                                        Money::of(150), "https://ieeexplore.example",
                                        LicenseModel::AnnualSubscription, Money::of(2000));
    catalog.emplace<ElectronicResource>("R002", "MATLAB Campus Licence", "MathWorks", 2026,
                                        Money::of(400), "https://licensing.example/matlab",
                                        LicenseModel::Perpetual);
    catalog.emplace<Journal>("J001", "IEEE Spectrum", "1939-1170",
                             12, "IEEE", 2026, Money::of(500), 2);
    catalog.emplace<EBook>("E001", "Clean Code", std::vector<std::string>{"Robert C. Martin"},
                           "978-0132350884", EBookFormat::PDF, true, "Prentice Hall",
                           2026, Money::of(100), "https://example.com/ebook",
                           LicenseModel::AnnualSubscription, Money::of(50));
    catalog.emplace<AudioBook>(
        "A001",
        "Clean Code Audiobook",
        std::vector<std::string>{"Robert C. Martin"},
        "978-0132350884",
        "Prentice Hall",
        2026,
        Money::of(500),
        "John Doe",
        600);
    catalog.emplace<Thesis>(
        "T001",
        "AI in Education",
        "IIIT Delhi",
        "M.Tech",
        "Dr. Professor",
        "IIIT Delhi",
        2026,
        Money::of(0));

    std::cout << "=== Catalog ===\n";
    for (const Resource *r : catalog.all())
        std::cout << r->summary() << "\n";

    std::cout << "\n=== Details of R001 ===\n"
              << catalog.get("R001");

    std::cout << "\n=== Details of J001 ===\n"
              << catalog.get("J001");

    std::cout << "\n=== Details of E001 ===\n"
              << catalog.get("E001");
    std::cout << "\n=== Details of A001 ===\n"
              << catalog.get("A001");

    std::cout << "\n=== Details of T001 ===\n"
              << catalog.get("T001");

    std::cout << "\n=== Q5 Bulk Pricing ===\n";
    Book bulkBook(
        "Q5-B1", "Bulk Test Book", {"Author"}, "ISBN-Q5",
        "Publisher", 2026, Money::of(100),
        1, Binding::Paperback);

    std::cout << "9 copies:  " << bulkBook.costFor(9) << "\n";
    std::cout << "10 copies: " << bulkBook.costFor(10) << "\n";

    ElectronicResource bulkElectronic(
        "Q5-E1", "Bulk Electronic Resource", "Publisher", 2026,
        Money::of(100), "https://example.com",
        LicenseModel::AnnualSubscription, Money::of(50));

    std::cout << "50 seats: " << bulkElectronic.costFor(50) << "\n";
    std::cout << "51 seats: " << bulkElectronic.costFor(51) << "\n";

    std::cout << "\n=== Q6 Tax Pricing ===\n";
    Catalog taxCatalog;
    taxCatalog.emplace<Book>(
        "TAX-B1",
        "Tax Book",
        std::vector<std::string>{"Author"},
        "ISBN-TAX",
        "Publisher",
        2026,
        Money::of(100),
        1,
        Binding::Paperback);
    taxCatalog.emplace<ElectronicResource>(
        "TAX-E1",
        "Tax Electronic",
        "Publisher",
        2026,
        Money::of(100),
        "https://example.com");

    Budget taxBudget(Money::of(5000));

    AcquisitionManager taxAcq(taxCatalog, taxBudget);

    taxAcq.setPrintTaxRate(10.0);
    taxAcq.setElectronicTaxRate(20.0);

    const auto printOrder = taxAcq.purchase("TAX-B1", 1);
    const auto electronicOrder = taxAcq.purchase("TAX-E1", 1);

    std::cout << "\n=== Q7 Title Quota ===\n";

    Budget titleBudget(Money::of(1000));
    titleBudget.setTitleQuota("Algorithms", 5);

    titleBudget.commit("Algorithms",
                       ResourceCategory::Book,
                       3,
                       Money::of(300));

    std::cout << "Algorithms usage: "
              << titleBudget.titleUsage("Algorithms") << "/"
              << *titleBudget.titleQuotaFor("Algorithms") << "\n";

    std::cout << "Request 2 more: "
              << (titleBudget.check("Algorithms",
                                    ResourceCategory::Book,
                                    2,
                                    Money::of(200))
                          .empty()
                      ? "approved"
                      : "rejected")
              << "\n";

    std::cout << "Request 3 more: "
              << (titleBudget.check("Algorithms",
                                    ResourceCategory::Book,
                                    3,
                                    Money::of(300))
                          .empty()
                      ? "approved"
                      : "rejected")
              << "\n";

    std::cout << "Print resource:\n"
              << "  pre-tax:  " << printOrder.preTaxCost << "\n"
              << "  post-tax: " << printOrder.postTaxCost << "\n";

    std::cout << "Electronic resource:\n"
              << "  pre-tax:  " << electronicOrder.preTaxCost << "\n"
              << "  post-tax: " << electronicOrder.postTaxCost << "\n";

    taxAcq.printReport(std::cout);

    Budget budget(Money::of(20000));
    budget.setQuota(ResourceCategory::Book, {10, Money::of(8000)});
    budget.setQuota(ResourceCategory::ElectronicResource, {40, Money::of(12000)});
    budget.setQuota(ResourceCategory::Journal, {5, Money::of(5000)});
    budget.setQuota(ResourceCategory::EBook, {10, Money::of(3000)});
    budget.setQuota(ResourceCategory::Thesis, {5, Money::of(1000)});

    AcquisitionManager acq(catalog, budget);

    std::cout << "\n=== Quotes ===\n";
    std::cout << "5 copies of B002  = " << acq.quote("B002", 5) << "\n";
    std::cout << "20 seats of R001  = " << acq.quote("R001", 20) << "  (incl. platform fee)\n";
    std::cout << "\n=== Journal Quote ===\n";
    std::cout << "2 copies of J001 = "
              << acq.quote("J001", 2) << "\n";

    acq.processBatch({
        {"B001", 4},  // 1800  ok
        {"B002", 5},  // 6000  ok  -> book spend 7800
        {"B001", 1},  // 450   rejected: book spend quota (200 left)
        {"R001", 20}, // 5000  ok
        {"R002", 25}, // 10000 rejected: e-resource unit quota (20 seats left)
        {"R002", 15}, // 6000  ok  -> e-resource spend 11000
        {"R002", 5},  // 2000  rejected: e-resource spend quota (1000 left)
        {"X999", 1},  // rejected: unknown id
        {"J001", 2},
    });

    std::cout << "\n=== Acquisition report ===\n";
    acq.printReport(std::cout);

    std::cout << "\n=== Budget ===\n";
    budget.print(std::cout);

    std::cout << "\n=== Holdings ===\n";
    for (const Resource *r : catalog.all())
        std::cout << "  " << r->id() << ": " << catalog.holdings(r->id())
                  << (r->isDigital() ? " seats" : " copies") << "\n";

    // Direct purchase: errors are reported with exceptions
    std::cout << "\n=== Direct purchase that breaks a quota ===\n";
    try
    {
        acq.purchase("B002", 1);
    }
    catch (const QuotaExceededError &e)
    {
        std::cout << "QuotaExceededError: " << e.what() << "\n";
    }

    std::cout << "\n=== Q8 Cancellation ===\n";

Catalog cancelCatalog;
cancelCatalog.emplace<Book>(
    "CANCEL-B1",
    "Cancellation Book",
    std::vector<std::string>{"Author"},
    "ISBN-CANCEL",
    "Publisher",
    2026,
    Money::of(100)
);

Budget cancelBudget(Money::of(1000));
cancelBudget.setQuota(
    ResourceCategory::Book,
    Quota{10, Money::of(1000)}
);
cancelBudget.setTitleQuota("Cancellation Book", 10);

AcquisitionManager cancelAcq(cancelCatalog, cancelBudget);

const auto cancelOrder = cancelAcq.purchase("CANCEL-B1", 3);

std::cout << "Before cancellation: spent = "
          << cancelBudget.spent()
          << ", holdings = "
          << cancelCatalog.holdings("CANCEL-B1") << "\n";

const bool cancelled = cancelAcq.cancel(cancelOrder.orderNo);

std::cout << "Cancellation successful: "
          << (cancelled ? "yes" : "no") << "\n";

std::cout << "After cancellation: spent = "
          << cancelBudget.spent()
          << ", holdings = "
          << cancelCatalog.holdings("CANCEL-B1") << "\n";

cancelAcq.printReport(std::cout);


    Budget csBudget(Money::of(1000));
    Budget physicsBudget(Money::of(2000));

    acq.addDepartment("Computer Science", csBudget);
    acq.addDepartment("Physics", physicsBudget);

    const auto& order =
        acq.purchase("Computer Science", "B001", 2);

    std::cout << "Charged department: " << order.department << '\n';
    std::cout << "CS spent: "
              << acq.departmentBudget("Computer Science")->spent() << '\n';
    std::cout << "Physics spent: "
              << acq.departmentBudget("Physics")->spent() << '\n';
    
    return 0;
}
