// Minimal self-contained test runner (no external framework needed).

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "bookmgmt/bookmgmt.h"

using namespace bookmgmt;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #cond \
                      << "\n";                                                   \
        }                                                                        \
    } while (0)

#define CHECK_THROWS(expr, ExType)            \
    do {                                      \
        bool thrown_ = false;                 \
        try {                                 \
            (void)(expr);                     \
        } catch (const ExType&) {             \
            thrown_ = true;                   \
        } catch (...) {                       \
        }                                     \
        CHECK(thrown_ && "expected " #ExType); \
    } while (0)

static void testMoney() {
    CHECK(Money::of(12, 5).toString() == "12.05");
    CHECK(Money::of(-3, 50).toString() == "-3.50");
    CHECK(Money::fromMinor(7).toString() == "0.07");
    CHECK(Money::of(10) + Money::of(0, 50) == Money::fromMinor(1050));
    CHECK(Money::of(3) * 4 == Money::of(12));
    CHECK(Money::of(1) < Money::of(2));
    CHECK_THROWS(Money::of(1, 100), std::invalid_argument);
}

static void testResourcesAndCost() {
    Book b("B1", "T", {"A", "B", "C"}, "isbn", "P", 2020, Money::of(100));
    CHECK(b.category() == ResourceCategory::Book);
    CHECK(!b.isDigital());
    CHECK(b.costFor(3) == Money::of(300));
    CHECK_THROWS(b.costFor(0), std::invalid_argument);
    CHECK(joinAuthors(b.authors()) == "A, B and C");

    ElectronicResource e("R1", "DB", "P", 2026, Money::of(10), "url",
                         LicenseModel::AnnualSubscription, Money::of(100));
    CHECK(e.isDigital());
    CHECK(e.costFor(5) == Money::of(150));

    CHECK(e.category() == ResourceCategory::ElectronicResource);

    // Polymorphism through a base-class reference
    const Resource& r = e;
    CHECK(r.costFor(1) == Money::of(110));
    std::ostringstream os;
    os << r;
    CHECK(os.str().find("platform fee: 100.00") != std::string::npos);

    CHECK_THROWS(Book("", "T", {}, "", "", 2000, Money::of(1)), std::invalid_argument);
    CHECK_THROWS(Book("B", "T", {}, "", "", 2000, Money::fromMinor(-1)),
                 std::invalid_argument);
}

static void testCatalog() {
    Catalog c;
    c.emplace<Book>("B1", "Clean Code", std::vector<std::string>{"M"}, "i", "P", 2008,
                    Money::of(1));
    c.emplace<Book>("B2", "Clean Architecture", std::vector<std::string>{"M"}, "i", "P",
                    2017, Money::of(1));
    c.emplace<ElectronicResource>("R1", "ACM Digital Library", "ACM", 2026, Money::of(1),
                                  "url");

    CHECK(c.size() == 3);
    CHECK(c.contains("B1"));
    CHECK(c.find("nope") == nullptr);
    CHECK_THROWS(c.get("nope"), NotFoundError);
    CHECK_THROWS(c.emplace<Book>("B1", "dup", std::vector<std::string>{}, "", "", 1,
                                 Money::of(1)),
                 DuplicateIdError);

    CHECK(c.searchTitle("clean").size() == 2);
    CHECK(c.byCategory(ResourceCategory::ElectronicResource).size() == 1);
    CHECK(c.where([](const Resource& r) { return r.isDigital(); }).size() == 1);

    CHECK(c.holdings("B1") == 0);
    c.addHoldings("B1", 3);
    CHECK(c.holdings("B1") == 3);
    CHECK_THROWS(c.addHoldings("B1", -5), std::invalid_argument);

    c.remove("R1");
    CHECK(c.size() == 2);
    CHECK_THROWS(c.remove("R1"), NotFoundError);
}

static void testBudget() {
    Budget b(Money::of(1000));
    b.setQuota(ResourceCategory::Book, {5, Money::of(400)});

    CHECK(b.check(ResourceCategory::Book, 2, Money::of(200)).empty());
    CHECK(!b.check(ResourceCategory::Book, 6, Money::of(10)).empty());   // units
    CHECK(!b.check(ResourceCategory::Book, 1, Money::of(401)).empty());  // spend
    CHECK(!b.check(ResourceCategory::ElectronicResource, 1, Money::of(1001)).empty());  // overall
    CHECK(b.check(ResourceCategory::ElectronicResource, 1, Money::of(900)).empty());    // no quota

    b.commit(ResourceCategory::Book, 4, Money::of(300));
    CHECK(b.spent() == Money::of(300));
    CHECK(*b.unitsRemaining(ResourceCategory::Book) == 1);
    CHECK(*b.spendRemaining(ResourceCategory::Book) == Money::of(100));
    CHECK(!b.unitsRemaining(ResourceCategory::ElectronicResource).has_value());

    CHECK_THROWS(b.commit(ResourceCategory::Book, 2, Money::of(10)), QuotaExceededError);
    CHECK_THROWS(b.commit(ResourceCategory::ElectronicResource, 1, Money::of(800)),
                 BudgetExceededError);
    CHECK_THROWS(b.commit(ResourceCategory::ElectronicResource, 0, Money::of(1)),
                 std::invalid_argument);
    CHECK(b.spent() == Money::of(300));  // failed commits changed nothing
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

    CHECK(acq.quote("R1", 5) == Money::of(100));
    std::string why;
    CHECK(acq.canPurchase("B1", 3, &why) && why.empty());
    CHECK(!acq.canPurchase("B1", 4, &why) && !why.empty());
    CHECK(!acq.canPurchase("nope", 1, &why));

    const auto& rec = acq.purchase("B1", 2);
    CHECK(rec.approved &&
      rec.preTaxCost == Money::of(200) &&
      rec.postTaxCost == Money::of(200) &&
      rec.orderNo == 1);
    CHECK(c.holdings("B1") == 2);

    CHECK_THROWS(acq.purchase("B1", 2), QuotaExceededError);
    CHECK_THROWS(acq.purchase("nope", 1), NotFoundError);
    CHECK(acq.history().size() == 1);  // exceptions don't record

    auto res = acq.processBatch({{"R1", 10}, {"R1", 100}, {"B1", 1}, {"zzz", 1}, {"B1", 0}});
    CHECK(res.size() == 5);
    CHECK(res[0].approved &&
      res[0].preTaxCost == Money::of(150) &&
      res[0].postTaxCost == Money::of(150));
    CHECK(!res[1].approved);  // 1050 > remaining 150
    CHECK(res[2].approved);
    CHECK(!res[3].approved && res[3].reason.find("not found") != std::string::npos);
    CHECK(!res[4].approved);
    CHECK(acq.totalSpent() == Money::of(450));
    CHECK(b.spent() == acq.totalSpent());
    CHECK(c.holdings("R1") == 10 && c.holdings("B1") == 3);
    CHECK(acq.history().size() == 6);
}

static void testJournal() {
    Journal j("J1", "IEEE Spectrum", "1939-1170", 12,
              "IEEE", 2026, Money::of(500), 2);

    CHECK(j.category() == ResourceCategory::Journal);
    CHECK(!j.isDigital());

    CHECK(j.issn() == "1939-1170");
    CHECK(j.issuesPerYear() == 12);
    CHECK(j.subscriptionYears() == 2);

    // 500 per year × 3 copies × 2 years = 3000
    CHECK(j.costFor(3) == Money::of(3000));

    CHECK_THROWS(j.costFor(0), std::invalid_argument);

    CHECK_THROWS(
        Journal("J2", "Nature", "1234-5678", 12,
                "Publisher", 2026, Money::of(400), 0),
        std::invalid_argument
    );

    std::ostringstream os;
    os << j;

    CHECK(os.str().find("Journal") != std::string::npos);
    CHECK(os.str().find("1939-1170") != std::string::npos);
    CHECK(os.str().find("issues per year: 12") != std::string::npos);
    CHECK(os.str().find("subscription years: 2") != std::string::npos);
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

    CHECK(ebook.category() == ResourceCategory::EBook);
    CHECK(ebook.isDigital());

    CHECK(ebook.authors().size() == 1);
    CHECK(ebook.authors()[0] == "Robert C. Martin");
    CHECK(ebook.isbn() == "978-0132350884");
    CHECK(ebook.format() == EBookFormat::PDF);
    CHECK(ebook.drmProtected());

    // Pricing is inherited from ElectronicResource:
    // platform fee + price per seat × seats
    CHECK(ebook.costFor(5) == Money::of(550));

    std::ostringstream os;
    os << ebook;

    CHECK(os.str().find("EBook E001") != std::string::npos);
    CHECK(os.str().find("platform fee: 50.00") != std::string::npos);
    CHECK(os.str().find("authors: Robert C. Martin") != std::string::npos);
    CHECK(os.str().find("isbn: 978-0132350884") != std::string::npos);
    CHECK(os.str().find("format: PDF") != std::string::npos);
    CHECK(os.str().find("drm protected: yes") != std::string::npos);
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

    CHECK(audiobook.category() == ResourceCategory::Book);
    CHECK(!audiobook.isDigital());

    CHECK(audiobook.narrator() == "John Doe");
    CHECK(audiobook.durationMinutes() == 600);
    CHECK(audiobook.authors().size() == 1);
    CHECK(audiobook.isbn() == "978-0132350884");

    std::ostringstream audioDetails;
    audioDetails << audiobook;

    CHECK(audioDetails.str().find("authors: Robert C. Martin")
          != std::string::npos);
    CHECK(audioDetails.str().find("narrator: John Doe")
          != std::string::npos);
    CHECK(audioDetails.str().find("duration minutes: 600")
          != std::string::npos);

    CHECK_THROWS(
        AudioBook(
            "A002",
            "Test",
            {"Author"},
            "123",
            "Publisher",
            2026,
            Money::of(100),
            "",
            100
        ),
        std::invalid_argument
    );

    CHECK_THROWS(
        AudioBook(
            "A003",
            "Test",
            {"Author"},
            "123",
            "Publisher",
            2026,
            Money::of(100),
            "Narrator",
            0
        ),
        std::invalid_argument
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

    CHECK(thesis.category() == ResourceCategory::Thesis);
    CHECK(!thesis.isDigital());

    CHECK(thesis.university() == "IIIT Delhi");
    CHECK(thesis.degree() == "M.Tech");
    CHECK(thesis.supervisor() == "Dr. Professor");

    CHECK(thesis.costFor(1) == Money::of(0));

    std::ostringstream thesisDetails;
    thesisDetails << thesis;

    CHECK(thesisDetails.str().find("university: IIIT Delhi")
          != std::string::npos);
    CHECK(thesisDetails.str().find("degree: M.Tech")
          != std::string::npos);
    CHECK(thesisDetails.str().find("supervisor: Dr. Professor")
          != std::string::npos);
}

static void testBulkPricing() {

    // Q5: print resources get 10% off at 10+ copies
    {
        Book paperback(
            "Q5-B1", "Test Book", {"Author"}, "ISBN-Q5",
            "Publisher", 2026, Money::of(100),
            1, Binding::Paperback);

        CHECK(paperback.costFor(9) == Money::of(900));
        CHECK(paperback.costFor(10) == Money::of(900));
    }

    // Q5: electronic seats beyond 50 cost half price
    {
        ElectronicResource ebook(
            "Q5-E1", "Test Electronic Resource", "Publisher", 2026,
            Money::of(100), "https://example.com",
            LicenseModel::AnnualSubscription, Money::of(50));

        CHECK(ebook.costFor(50) == Money::of(5050));
        CHECK(ebook.costFor(51) == Money::of(5100));
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

    CHECK(acq.printTaxRate() == 10.0);
    CHECK(acq.electronicTaxRate() == 20.0);

    // Book: 100 pre-tax -> 110 post-tax
    CHECK(acq.quote("TAX-B1", 1) == Money::of(100));

        // Quota is 105. Pre-tax is 100, but 10% tax makes it 110.
    // Therefore the purchase must be rejected.
    Budget tightBudget(Money::of(1000));
    tightBudget.setQuota(ResourceCategory::Book, {10, Money::of(105)});

    AcquisitionManager tightAcq(c, tightBudget);
    tightAcq.setPrintTaxRate(10.0);

    std::string why;
    CHECK(!tightAcq.canPurchase("TAX-B1", 1, &why));

    // std::string why;
    CHECK(acq.canPurchase("TAX-B1", 1, &why));
    CHECK(why.empty());

    const auto& bookRecord = acq.purchase("TAX-B1", 1);

    CHECK(bookRecord.preTaxCost == Money::of(100));
    CHECK(bookRecord.postTaxCost == Money::of(110));

    // Electronic: 100 pre-tax -> 120 post-tax
    CHECK(acq.canPurchase("TAX-E1", 1, &why));
    CHECK(why.empty());

    const auto& ebookRecord = acq.purchase("TAX-E1", 1);

    CHECK(ebookRecord.preTaxCost == Money::of(100));
    CHECK(ebookRecord.postTaxCost == Money::of(120));

    // Budget spent must use post-tax amounts.
    CHECK(b.spent() == Money::of(230));
    CHECK(acq.totalSpent() == Money::of(230));
}

static void testTitleQuota() {
    Budget budget(Money::of(1000));

    budget.setTitleQuota("Algorithms", 5);

    CHECK(budget.titleQuotaFor("Algorithms").has_value());
    CHECK(*budget.titleQuotaFor("Algorithms") == 5);
    CHECK(budget.titleUsage("Algorithms") == 0);

    budget.commit("Algorithms",
                  ResourceCategory::Book,
                  3,
                  Money::of(300));

    CHECK(budget.titleUsage("Algorithms") == 3);

    CHECK(budget.check("Algorithms",
                       ResourceCategory::Book,
                       2,
                       Money::of(200)).empty());

    CHECK(!budget.check("Algorithms",
                        ResourceCategory::Book,
                        3,
                        Money::of(300)).empty());

    CHECK_THROWS(
        budget.commit("Algorithms",
                      ResourceCategory::Book,
                      3,
                      Money::of(300)),
        QuotaExceededError
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

    CHECK(order.approved);
    CHECK(budget.spent() == Money::of(300));
    CHECK(budget.titleUsage("Cancellation Book") == 3);

    CHECK(acq.cancel(order.orderNo));

    CHECK(budget.spent() == Money{});
    CHECK(budget.titleUsage("Cancellation Book") == 0);
    CHECK(acq.totalSpent() == Money{});
    CHECK(acq.history().size() == 2);
    CHECK(acq.history()[0].cancellation == false);
    CHECK(acq.history()[1].cancellation == true);
    CHECK(acq.history()[1].originalOrderNo == order.orderNo);

    CHECK(!acq.cancel(order.orderNo));
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
    
    std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
