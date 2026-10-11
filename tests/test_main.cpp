// Minimal self-contained test runner (no external framework needed).

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <cassert>

#include "bookmgmt/bookmgmt.h"

using namespace bookmgmt;

template <typename Exception, typename Callable>
void assertThrows(Callable&& action) {
    bool thrown = false;

    try {
        action();
    } catch (const Exception&) {
        thrown = true;
    } catch (...) {
        // A different exception type does not satisfy the test.
    }

    assert(thrown && "Expected exception was not thrown");
}

static void testMoney() {
    assert(Money::of(12, 5).toString() == "12.05");
    assert(Money::of(-3, 50).toString() == "-3.50");
    assert(Money::fromMinor(7).toString() == "0.07");
    assert(Money::of(10) + Money::of(0, 50) == Money::fromMinor(1050));
    assert(Money::of(3) * 4 == Money::of(12));
    assert(Money::of(1) < Money::of(2));
    assertThrows<std::invalid_argument>(
    [&] { (void)Money::of(1, 100); }
);

    const Money rupees = Money::of(10, 50, "INR");
    const Money dollars = Money::of(10, 50, "USD");

    assert(rupees.currency() == "INR");
    assert(dollars.currency() == "USD");
    assert(rupees.toString() == "10.50");

    assertThrows<std::invalid_argument>(
    [&] { (void)(rupees + dollars); }
);
    assertThrows<std::invalid_argument>(
    [&] { (void)(rupees - dollars); }
);
    assertThrows<std::invalid_argument>(
    [&] { (void)(rupees == dollars); }
);
    assertThrows<std::invalid_argument>(
    [&] { (void)(rupees != dollars); }
);
    assertThrows<std::invalid_argument>(
    [&] { (void)(rupees < dollars); }
);
    assertThrows<std::invalid_argument>(
    [&] { (void)(rupees <= dollars); }
);
    assertThrows<std::invalid_argument>(
    [&] { (void)(rupees > dollars); }
);
    assertThrows<std::invalid_argument>(
    [&] { (void)(rupees >= dollars); }
);
    assertThrows<std::invalid_argument>(
    [&] { (void)Money::of(1, 0, ""); }
);
}

static void testResourcesAndCost() {
    Book b("B1", "T", {"A", "B", "C"}, "isbn", "P", 2020, Money::of(100));
    assert(b.category() == ResourceCategory::Book);
    assert(!b.isDigital());
    assert(b.costFor(3) == Money::of(300));
    assertThrows<std::invalid_argument>(
    [&] { (void)b.costFor(0); }
);
    assert(joinAuthors(b.authors()) == "A, B and C");

    ElectronicResource e("R1", "DB", "P", 2026, Money::of(10), "url",
                         LicenseModel::AnnualSubscription, Money::of(100));
    assert(e.isDigital());
    assert(e.costFor(5) == Money::of(150));

    assert(e.category() == ResourceCategory::ElectronicResource);

    // Polymorphism through a base-class reference
    const Resource& r = e;
    assert(r.costFor(1) == Money::of(110));
    std::ostringstream os;
    os << r;
    assert(os.str().find("platform fee: 100.00") != std::string::npos);

    assertThrows<std::invalid_argument>(
    [&] {
        (void)Book("", "T", {}, "", "", 2000, Money::of(1));
    }
);

assertThrows<std::invalid_argument>(
    [&] {
        (void)Book("B", "T", {}, "", "", 2000, Money::fromMinor(-1));
    }
);
}

static void testCatalog() {
    Catalog c;
    c.emplace<Book>("B1", "Clean Code", std::vector<std::string>{"M"}, "i", "P", 2008,
                    Money::of(1));
    c.emplace<Book>("B2", "Clean Architecture", std::vector<std::string>{"M"}, "i", "P",
                    2017, Money::of(1));
    c.emplace<ElectronicResource>("R1", "ACM Digital Library", "ACM", 2026, Money::of(1),
                                  "url");

    assert(c.size() == 3);
    assert(c.contains("B1"));
    assert(c.find("nope") == nullptr);
    assertThrows<NotFoundError>(
    [&] {
        (void)c.get("nope");
    }
);

assertThrows<DuplicateIdError>(
    [&] {
        (void)c.emplace<Book>(
            "B1", "dup", std::vector<std::string>{}, "", "", 1,
            Money::of(1)
        );
    }
);

    assert(c.searchTitle("clean").size() == 2);
    assert(c.byCategory(ResourceCategory::ElectronicResource).size() == 1);
    assert(c.where([](const Resource& r) { return r.isDigital(); }).size() == 1);

    assert(c.holdings("B1") == 0);
    c.addHoldings("B1", 3);
    assert(c.holdings("B1") == 3);
    assertThrows<std::invalid_argument>(
    [&] {
        c.addHoldings("B1", -5);
    }
);

    c.remove("R1");
    assert(c.size() == 2);
    assertThrows<NotFoundError>(
    [&] {
        c.remove("R1");
    }
);
}

static void testBudget() {
    Budget b(Money::of(1000));
    b.setQuota(ResourceCategory::Book, {5, Money::of(400)});

    assert(b.check(ResourceCategory::Book, 2, Money::of(200)).empty());
assert(!b.check(ResourceCategory::Book, 6, Money::of(10)).empty());   // units
assert(!b.check(ResourceCategory::Book, 1, Money::of(401)).empty());  // spend
assert(!b.check(ResourceCategory::ElectronicResource, 1, Money::of(1001)).empty());  // overall
assert(b.check(ResourceCategory::ElectronicResource, 1, Money::of(900)).empty());    // no quota

    b.commit(ResourceCategory::Book, 4, Money::of(300));
    assert(b.spent() == Money::of(300));
    assert(*b.unitsRemaining(ResourceCategory::Book) == 1);
    assert(*b.spendRemaining(ResourceCategory::Book) == Money::of(100));
    assert(!b.unitsRemaining(ResourceCategory::ElectronicResource).has_value());

    assertThrows<QuotaExceededError>(
    [&] {
        b.commit(ResourceCategory::Book, 2, Money::of(10));
    }
);

assertThrows<BudgetExceededError>(
    [&] {
        b.commit(ResourceCategory::ElectronicResource, 1, Money::of(800));
    }
);

assertThrows<std::invalid_argument>(
    [&] {
        b.commit(ResourceCategory::ElectronicResource, 0, Money::of(1));
    }
);
    assert(b.spent() == Money::of(300));  // failed commits changed nothing
}

static void testAcquisition() {
    Catalog c;
    c.emplace<Book>("B1", "Book", std::vector<std::string>{"A"}, "i", "P", 2020,
                    Money::of(100));
    c.emplace<ElectronicResource>("R1", "DB", "P", 2026, Money::of(10), "url",
                                  LicenseModel::AnnualSubscription, Money::of(50));
    Budget b(Money::of(500));
    b.setQuota(ResourceCategory::Book, {3, Money::of(1000)});
    AcquisitionManager acq(c, b);

    assert(acq.quote("R1", 5) == Money::of(100));
    std::string why;
    assert(acq.canPurchase("B1", 3, &why) && why.empty());
    assert(!acq.canPurchase("B1", 4, &why) && !why.empty());
    assert(!acq.canPurchase("nope", 1, &why));

    const auto& rec = acq.purchase("B1", 2);
    assert(rec.approved &&
      rec.preTaxCost == Money::of(200) &&
      rec.postTaxCost == Money::of(200) &&
      rec.orderNo == 1);
    assert(c.holdings("B1") == 2);

    assertThrows<QuotaExceededError>(
    [&] {
        (void)acq.purchase("B1", 2);
    }
);

assertThrows<NotFoundError>(
    [&] {
        (void)acq.purchase("nope", 1);
    }
);
    assert(acq.history().size() == 1);  // exceptions don't record

    auto res = acq.processBatch({{"R1", 10}, {"R1", 100}, {"B1", 1}, {"zzz", 1}, {"B1", 0}});
    assert(res.size() == 5);
    assert(res[0].approved &&
      res[0].preTaxCost == Money::of(150) &&
      res[0].postTaxCost == Money::of(150));
    assert(!res[1].approved);  // 1050 > remaining 150
    assert(res[2].approved);
    assert(!res[3].approved && res[3].reason.find("not found") != std::string::npos);
    assert(!res[4].approved);
    assert(acq.totalSpent() == Money::of(450));
    assert(b.spent() == acq.totalSpent());
    assert(c.holdings("R1") == 10 && c.holdings("B1") == 3);
    assert(acq.history().size() == 6);
}

static void testJournal() {
    Journal j("J1", "IEEE Spectrum", "1939-1170", 12,
              "IEEE", 2026, Money::of(500), 2);

    assert(j.category() == ResourceCategory::Journal);
    assert(!j.isDigital());

    assert(j.issn() == "1939-1170");
    assert(j.issuesPerYear() == 12);
    assert(j.subscriptionYears() == 2);

    // 500 per year × 3 copies × 2 years = 3000
    assert(j.costFor(3) == Money::of(3000));

    assertThrows<std::invalid_argument>(
    [&] {
        (void)j.costFor(0);
    }
);

assertThrows<std::invalid_argument>(
    [&] {
        (void)Journal(
            "J2", "Nature", "1234-5678", 12,
            "Publisher", 2026, Money::of(400), 0
        );
    }
);

    std::ostringstream os;
    os << j;

    assert(os.str().find("Journal") != std::string::npos);
    assert(os.str().find("1939-1170") != std::string::npos);
    assert(os.str().find("issues per year: 12") != std::string::npos);
    assert(os.str().find("subscription years: 2") != std::string::npos);
}

static void testEBook() {
    EBook ebook(
        "E001",
        "Clean Code",
        {"Robert C. Martin"},
        "978-0132350884",
        EBookFormat::PDF,
        true,
        "Prentice Hall",
        2026,
        Money::of(100),
        "https://example.com/ebook",
        LicenseModel::AnnualSubscription,
        Money::of(50)
    );

    assert(ebook.category() == ResourceCategory::EBook);
    assert(ebook.isDigital());

    assert(ebook.authors().size() == 1);
    assert(ebook.authors()[0] == "Robert C. Martin");
    assert(ebook.isbn() == "978-0132350884");
    assert(ebook.format() == EBookFormat::PDF);
    assert(ebook.drmProtected());

    // Pricing is inherited from ElectronicResource:
    // platform fee + price per seat × seats
    assert(ebook.costFor(5) == Money::of(550));

    std::ostringstream os;
    os << ebook;

    assert(os.str().find("EBook E001") != std::string::npos);
    assert(os.str().find("platform fee: 50.00") != std::string::npos);
    assert(os.str().find("authors: Robert C. Martin") != std::string::npos);
    assert(os.str().find("isbn: 978-0132350884") != std::string::npos);
    assert(os.str().find("format: PDF") != std::string::npos);
    assert(os.str().find("drm protected: yes") != std::string::npos);
}

static void testAudioBookAndThesis() {
    AudioBook audiobook(
        "A001",
        "Clean Code",
        {"Robert C. Martin"},
        "978-0132350884",
        "Prentice Hall",
        2026,
        Money::of(500),
        "John Doe",
        600
    );

    assert(audiobook.category() == ResourceCategory::Book);
    assert(!audiobook.isDigital());

    assert(audiobook.narrator() == "John Doe");
    assert(audiobook.durationMinutes() == 600);
    assert(audiobook.authors().size() == 1);
    assert(audiobook.isbn() == "978-0132350884");

    std::ostringstream audioDetails;
    audioDetails << audiobook;

    assert(audioDetails.str().find("authors: Robert C. Martin")
          != std::string::npos);
    assert(audioDetails.str().find("narrator: John Doe")
          != std::string::npos);
    assert(audioDetails.str().find("duration minutes: 600")
          != std::string::npos);

    assertThrows<std::invalid_argument>(
    [&] {
        (void)AudioBook(
            "A002",
            "Test",
            {"Author"},
            "123",
            "Publisher",
            2026,
            Money::of(100),
            "",
            100
        );
    }
);

assertThrows<std::invalid_argument>(
    [&] {
        (void)AudioBook(
            "A003",
            "Test",
            {"Author"},
            "123",
            "Publisher",
            2026,
            Money::of(100),
            "Narrator",
            0
        );
    }
);

    Thesis thesis(
        "T001",
        "AI in Education",
        "IIIT Delhi",
        "M.Tech",
        "Dr. Professor",
        "IIIT Delhi",
        2026,
        Money::of(0)
    );

    assert(thesis.category() == ResourceCategory::Thesis);
    assert(!thesis.isDigital());

    assert(thesis.university() == "IIIT Delhi");
    assert(thesis.degree() == "M.Tech");
    assert(thesis.supervisor() == "Dr. Professor");

    assert(thesis.costFor(1) == Money::of(0));

    std::ostringstream thesisDetails;
    thesisDetails << thesis;

    assert(thesisDetails.str().find("university: IIIT Delhi")
          != std::string::npos);
    assert(thesisDetails.str().find("degree: M.Tech")
          != std::string::npos);
    assert(thesisDetails.str().find("supervisor: Dr. Professor")
          != std::string::npos);
}

static void testBulkPricing() {

    // Q5: print resources get 10% off at 10+ copies
    {
        Book paperback(
            "Q5-B1", "Test Book", {"Author"}, "ISBN-Q5",
            "Publisher", 2026, Money::of(100),
            1, Binding::Paperback);

        assert(paperback.costFor(9) == Money::of(900));
        assert(paperback.costFor(10) == Money::of(900));
    }

    // Q5: electronic seats beyond 50 cost half price
    {
        ElectronicResource ebook(
            "Q5-E1", "Test Electronic Resource", "Publisher", 2026,
            Money::of(100), "https://example.com",
            LicenseModel::AnnualSubscription, Money::of(50));

        assert(ebook.costFor(50) == Money::of(5050));
        assert(ebook.costFor(51) == Money::of(5100));
    }
}

static void testTaxes() {

    Catalog c;

    c.emplace<Book>(
        "TAX-B1", "Tax Book",
        std::vector<std::string>{"Author"},
        "ISBN-TAX", "Publisher", 2026,
        Money::of(100)
    );

    c.emplace<ElectronicResource>(
        "TAX-E1", "Tax Electronic",
        "Publisher", 2026,
        Money::of(100),
        "https://example.com"
    );

    Budget b(Money::of(1000));
    b.setQuota(ResourceCategory::Book, {10, Money::of(1000)});
    b.setQuota(ResourceCategory::ElectronicResource, {10, Money::of(1000)});

    AcquisitionManager acq(c, b);

    // 10% tax on print resources
    acq.setPrintTaxRate(10.0);

    // 20% tax on electronic resources
    acq.setElectronicTaxRate(20.0);

    assert(acq.printTaxRate() == 10.0);
    assert(acq.electronicTaxRate() == 20.0);

    // Book: 100 pre-tax -> 110 post-tax
    assert(acq.quote("TAX-B1", 1) == Money::of(100));

        // Quota is 105. Pre-tax is 100, but 10% tax makes it 110.
    // Therefore the purchase must be rejected.
    Budget tightBudget(Money::of(1000));
    tightBudget.setQuota(ResourceCategory::Book, {10, Money::of(105)});

    AcquisitionManager tightAcq(c, tightBudget);
    tightAcq.setPrintTaxRate(10.0);

    std::string why;
    assert(!tightAcq.canPurchase("TAX-B1", 1, &why));

    // std::string why;
    assert(acq.canPurchase("TAX-B1", 1, &why));
    assert(why.empty());

    const auto& bookRecord = acq.purchase("TAX-B1", 1);

    assert(bookRecord.preTaxCost == Money::of(100));
    assert(bookRecord.postTaxCost == Money::of(110));

    // Electronic: 100 pre-tax -> 120 post-tax
    assert(acq.canPurchase("TAX-E1", 1, &why));
    assert(why.empty());

    const auto& ebookRecord = acq.purchase("TAX-E1", 1);

    assert(ebookRecord.preTaxCost == Money::of(100));
    assert(ebookRecord.postTaxCost == Money::of(120));

    // Budget spent must use post-tax amounts.
    assert(b.spent() == Money::of(230));
    assert(acq.totalSpent() == Money::of(230));
}

static void testTitleQuota() {
    Budget budget(Money::of(1000));

    budget.setTitleQuota("Algorithms", 5);

    assert(budget.titleQuotaFor("Algorithms").has_value());
    assert(*budget.titleQuotaFor("Algorithms") == 5);
    assert(budget.titleUsage("Algorithms") == 0);

    budget.commit("Algorithms",
                  ResourceCategory::Book,
                  3,
                  Money::of(300));

    assert(budget.titleUsage("Algorithms") == 3);

    assert(budget.check("Algorithms",
                       ResourceCategory::Book,
                       2,
                       Money::of(200)).empty());

    assert(!budget.check("Algorithms",
                        ResourceCategory::Book,
                        3,
                        Money::of(300)).empty());

    assertThrows<QuotaExceededError>(
    [&] {
        budget.commit(
            "Algorithms",
            ResourceCategory::Book,
            3,
            Money::of(300)
        );
    }
);
}

static void testCancellation() {
    Catalog catalog;

    catalog.emplace<Book>(
        "CANCEL-B1",
        "Cancellation Book",
        std::vector<std::string>{"Author"},
        "ISBN-CANCEL",
        "Publisher",
        2026,
        Money::of(100)
    );

    Budget budget(Money::of(1000));
    budget.setQuota(ResourceCategory::Book, Quota{10, Money::of(1000)});
    budget.setTitleQuota("Cancellation Book", 10);

    AcquisitionManager acq(catalog, budget);

    const auto order = acq.purchase("CANCEL-B1", 3);

    assert(order.approved);
    assert(budget.spent() == Money::of(300));
    assert(budget.titleUsage("Cancellation Book") == 3);

    assert(acq.cancel(order.orderNo));

    assert(budget.spent() == Money{});
    assert(budget.titleUsage("Cancellation Book") == 0);
    assert(acq.totalSpent() == Money{});
    assert(acq.history().size() == 2);
    assert(acq.history()[0].cancellation == false);
    assert(acq.history()[1].cancellation == true);
    assert(acq.history()[1].originalOrderNo == order.orderNo);

    assert(!acq.cancel(order.orderNo));
}

static void testDepartmentBudgets() {
    Catalog catalog;
    catalog.emplace<Book>(
        "DEPT-B1", "Department Book",
        std::vector<std::string>{"Author"},
        "ISBN-DEPT", "Publisher", 2026, Money::of(100)
    );

    Budget defaultBudget(Money::of(1000));
    AcquisitionManager acq(catalog, defaultBudget);

    Budget csBudget(Money::of(300));
    csBudget.setQuota(ResourceCategory::Book,
                      Quota{5, Money::of(300)});
    acq.addDepartment("Computer Science", std::move(csBudget));

    Budget physicsBudget(Money::of(500));
    physicsBudget.setQuota(ResourceCategory::Book,
                           Quota{5, Money::of(500)});
    acq.addDepartment("Physics", std::move(physicsBudget));

    const auto& order = acq.purchase("Computer Science", "DEPT-B1", 2);

    assert(order.approved);
    assert(order.department == "Computer Science");
    assert(acq.departmentBudget("Computer Science")->spent()
          == Money::of(200));
    assert(acq.departmentBudget("Physics")->spent() == Money{});

    assert(!acq.canPurchase("Unknown", "DEPT-B1", 1));

    assert(acq.cancel(order.orderNo));
    assert(acq.departmentBudget("Computer Science")->spent() == Money{});
    assert(acq.departmentBudget("Physics")->spent() == Money{});
}

static void testBudgetRollover() {
    Budget currentYear(Money::of(1000));

    currentYear.setQuota(
        ResourceCategory::Book,
        Quota{10, Money::of(800)}
    );
    currentYear.setTitleQuota("Rollover Book", 5);

    currentYear.commit(
        "Rollover Book",
        ResourceCategory::Book,
        3,
        Money::of(300)
    );

    // Current balance: 1000 - 300 = 700.
    Budget nextYear = currentYear.rollover(50);

    assert(nextYear.total() == Money::of(350));
    assert(nextYear.spent() == Money{});
    assert(nextYear.remaining() == Money::of(350));

    // Category quota carries forward, but category usage resets.
    const auto bookQuota = nextYear.quotaFor(ResourceCategory::Book);
    assert(bookQuota.has_value());

    if (bookQuota) {
        assert(bookQuota->maxUnits == 10);
        assert(bookQuota->maxSpend == Money::of(800));
    }

    assert(nextYear.usageFor(ResourceCategory::Book).units == 0);
    assert(nextYear.usageFor(ResourceCategory::Book).spent == Money{});

    // Title quota carries forward, but title usage resets.
    assert(nextYear.titleQuotaFor("Rollover Book").has_value());
    assert(nextYear.titleQuotaFor("Rollover Book").value() == 5);
    assert(nextYear.titleUsage("Rollover Book") == 0);

    // The original budget remains unchanged.
    assert(currentYear.spent() == Money::of(300));
    assert(currentYear.titleUsage("Rollover Book") == 3);

    assert(currentYear.rollover(0).total() == Money{});
    assert(currentYear.rollover(100).total() == Money::of(700));

    bool negativeRejected = false;
    try {
        (void)currentYear.rollover(-1);
    } catch (const std::invalid_argument&) {
        negativeRejected = true;
    }
    assert(negativeRejected);

    bool excessiveRejected = false;
    try {
        (void)currentYear.rollover(101);
    } catch (const std::invalid_argument&) {
        excessiveRejected = true;
    }
    assert(excessiveRejected);
}


static void testAllOrNothingBatch() {
    Catalog c;

    c.emplace<Book>(
        "B1", "Book",
        std::vector<std::string>{"A"},
        "ISBN1", "Publisher", 2020,
        Money::of(100));

    Budget b(Money::of(1000));
    b.setQuota(ResourceCategory::Book, {10, Money::of(1000)});

    AcquisitionManager acq(c, b);

    // CASE 1: One valid request followed by an invalid request.
    // The entire batch must be rejected without purchasing anything.
    auto failed = acq.processBatch(
        {{"B1", 2}, {"MISSING", 1}},
        true);

    assert(failed.size() == 2);
    assert(!failed[0].approved);
    assert(!failed[1].approved);

    assert(failed[1].reason.find("not found") != std::string::npos);

    // No purchase must have occurred.
    assert(c.holdings("B1") == 0);
    assert(b.spent() == Money::of(0));
    assert(acq.totalSpent() == Money::of(0));

    // Rejected outcomes are still recorded in history.
    assert(acq.history().size() == 2);

    // CASE 2: Every request is valid, so the entire batch is purchased.
    auto successful = acq.processBatch(
        {{"B1", 2}, {"B1", 1}},
        true);

    assert(successful.size() == 2);
    assert(successful[0].approved);
    assert(successful[1].approved);

    assert(c.holdings("B1") == 3);
    assert(b.spent() == Money::of(300));
    assert(acq.totalSpent() == Money::of(300));
}


static void testVendors() {
    Catalog c;

    c.emplace<Book>(
        "B1", "Vendor Test Book",
        std::vector<std::string>{"Author"},
        "ISBN-V1", "Publisher", 2025,
        Money::of(100));

    Budget b(Money::of(1000));
    b.setQuota(ResourceCategory::Book, {10, Money::of(1000)});

    AcquisitionManager acq(c, b);

    // Three vendors offer the same title at different unit prices.
    acq.addVendor("B1", "Vendor A", Money::of(120));
    acq.addVendor("B1", "Vendor B", Money::of(80));
    acq.addVendor("B1", "Vendor C", Money::of(100));

    // The cheapest vendor's price is used in the quote.
    assert(acq.quote("B1", 2) == Money::of(160));

    // The purchase uses Vendor B and charges 2 * 80 = 160.
    const auto& purchase = acq.purchase("B1", 2);

    assert(purchase.approved);
    assert(purchase.vendor == "Vendor B");
    assert(purchase.preTaxCost == Money::of(160));
    assert(purchase.postTaxCost == Money::of(160));

    assert(c.holdings("B1") == 2);
    assert(b.spent() == Money::of(160));

    // Re-registering an existing vendor updates its price.
    acq.addVendor("B1", "Vendor B", Money::of(150));

    // Vendor C is now cheapest at 100 per unit.
    assert(acq.quote("B1", 1) == Money::of(100));

    const auto& secondPurchase = acq.purchase("B1", 1);

    assert(secondPurchase.approved);
    assert(secondPurchase.vendor == "Vendor C");
    assert(b.spent() == Money::of(260));

    // The chosen vendor is visible in the order history.
    assert(acq.history().size() == 2);
    assert(acq.history()[0].vendor == "Vendor B");
    assert(acq.history()[1].vendor == "Vendor C");

    // Resources without registered vendors use catalog pricing.
    c.emplace<Book>(
        "B2", "Catalog Price Book",
        std::vector<std::string>{"Another Author"},
        "ISBN-V2", "Publisher", 2025,
        Money::of(50));

    assert(acq.quote("B2", 2) == Money::of(100));

    const auto& catalogPurchase = acq.purchase("B2", 2);

    assert(catalogPurchase.approved);
    assert(catalogPurchase.vendor == "Catalog");
    assert(catalogPurchase.preTaxCost == Money::of(100));
    assert(catalogPurchase.postTaxCost == Money::of(100));
}

static void testCatalogSearches() {
    Catalog catalog;

    catalog.emplace<Book>(
        "B1", "Clean Code",
        std::vector<std::string>{"Robert Martin", "Alice Smith"},
        "ISBN-111", "Prentice Hall", 2008, Money::of(450));

    catalog.emplace<EBook>(
        "E1", "Modern C++",
        std::vector<std::string>{"Alice Smith"},
        "ISBN-222", EBookFormat::PDF, false,
        "Tech Publisher", 2022, Money::of(200),
        "https://example.com/modern-cpp");

    catalog.emplace<Journal>(
        "J1", "Computer Science Review",
        "ISSN-333", 4, "Academic Press", 2024,
        Money::of(500));

    // Search by author: case-insensitive partial match.
    const auto authorResults = catalog.searchAuthor("alice");
    assert(authorResults.size() == 2);

    // Search by ISBN or ISSN.
    const auto bookResults = catalog.searchIdentifier("ISBN-111");
    assert(bookResults.size() == 1);
    assert(bookResults[0]->id() == "B1");

    const auto ebookResults = catalog.searchIdentifier("ISBN-222");
    assert(ebookResults.size() == 1);
    assert(ebookResults[0]->id() == "E1");

    const auto journalResults = catalog.searchIdentifier("ISSN-333");
    assert(journalResults.size() == 1);
    assert(journalResults[0]->id() == "J1");

    // Inclusive publication-year range: 2022 through 2024.
    const auto yearResults = catalog.searchYearRange(2022, 2024);
    assert(yearResults.size() == 2);

    // A year range with no matches.
    assert(catalog.searchYearRange(1990, 1999).empty());

    // Invalid year range.
    bool threw = false;
    try {
        catalog.searchYearRange(2025, 2020);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

static void testLending() {
    Catalog catalog;

    // Two print copies.
    catalog.emplace<Book>(
        "LEND-B1", "Lending Book",
        std::vector<std::string>{"Test Author"},
        "ISBN-L1", "Publisher", 2024, Money::of(100));
    catalog.addHoldings("LEND-B1", 2);

    // Only two copies can be borrowed at once.
    catalog.borrowCopy("LEND-B1", "Alice");
    catalog.borrowCopy("LEND-B1", "Bob");

    assert(catalog.borrowedCopies("LEND-B1") == 2);
    assertThrows<std::invalid_argument>(
    [&] {
        catalog.borrowCopy("LEND-B1", "Charlie");
    }
);

// Cannot reduce holdings below the number of borrowed copies.
assertThrows<std::invalid_argument>(
    [&] {
        catalog.addHoldings("LEND-B1", -1);
    }
);

    // Returning a copy frees capacity.
    catalog.returnCopy("LEND-B1", "Alice");
    assert(catalog.borrowedCopies("LEND-B1") == 1);

    catalog.borrowCopy("LEND-B1", "Charlie");
    assert(catalog.borrowedCopies("LEND-B1") == 2);

    assertThrows<std::invalid_argument>(
    [&] {
        catalog.returnCopy("LEND-B1", "Unknown");
    }
);

// Print resources cannot open electronic sessions.
assertThrows<std::invalid_argument>(
    [&] {
        catalog.openSession("LEND-B1", "Alice");
    }
);

    // Electronic resource with two licensed seats.
    catalog.emplace<ElectronicResource>(
        "LEND-E1", "Online Database", "Publisher", 2024,
        Money::of(10), "https://example.com",
        LicenseModel::AnnualSubscription, Money{});
    catalog.addHoldings("LEND-E1", 2);

    catalog.openSession("LEND-E1", "Alice");
    catalog.openSession("LEND-E1", "Bob");

    assert(catalog.activeSessions("LEND-E1") == 2);

    assertThrows<std::invalid_argument>(
    [&] {
        catalog.openSession("LEND-E1", "Charlie");
    }
);

assertThrows<std::invalid_argument>(
    [&] {
        catalog.addHoldings("LEND-E1", -1);
    }
);

    // Closing a session frees a licensed seat.
    catalog.closeSession("LEND-E1", "Alice");
    assert(catalog.activeSessions("LEND-E1") == 1);

    catalog.openSession("LEND-E1", "Charlie");
    assert(catalog.activeSessions("LEND-E1") == 2);

    assertThrows<std::invalid_argument>(
    [&] {
        catalog.closeSession("LEND-E1", "Unknown");
    }
);

// Electronic resources cannot be borrowed as print copies.
assertThrows<std::invalid_argument>(
    [&] {
        catalog.borrowCopy("LEND-E1", "Alice");
    }
);

// Unknown resource IDs must be rejected.
assertThrows<NotFoundError>(
    [&] {
        catalog.borrowCopy("MISSING", "Alice");
    }
);

assertThrows<NotFoundError>(
    [&] {
        catalog.openSession("MISSING", "Alice");
    }
);
}

int main() {
    testMoney();
    testResourcesAndCost();
    testCatalog();
    testBudget();
    testAcquisition();
    testJournal();
    testEBook();
    testAudioBookAndThesis();
    testBulkPricing();
    testTaxes();
    testTitleQuota();
    testCancellation();
    testDepartmentBudgets();
    testBudgetRollover();
    testAllOrNothingBatch();
    testVendors();
    testCatalogSearches();
    testLending();
    
    return 0;
}
