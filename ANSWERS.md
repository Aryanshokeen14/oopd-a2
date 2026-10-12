# ANSWERS.md

## Q1 — Journal

Implemented a `Journal` class derived from `Resource`. It stores the ISSN, number of issues published per year, and subscription duration in years.

The subscription duration defaults to one year and must be at least one; otherwise, an `std::invalid_argument` exception is thrown. The `costFor()` method calculates the total subscription cost using the unit price, number of copies, and subscription duration.

Added `Journal` to `ResourceCategory`, category naming, and budget quota reporting. Overrode `printDetails()` to display journal-specific information.

## Q2 — EBook

Implemented an `EBook` class derived from `ElectronicResource`. It stores authors, ISBN, file format (PDF, EPUB, or HTML), and DRM protection status.

The class inherits pricing behavior from `ElectronicResource`, including the platform fee and per-seat pricing. A separate `ResourceCategory::EBook` allows e-books to have independent purchase quotas.

Overrode `printDetails()` to display inherited electronic-resource details followed by e-book-specific information.

**Design question:** The `Book` and `EBook` classes duplicate author and ISBN fields, their accessors, and related display logic. This duplication could be reduced by introducing a shared abstract bibliographic class containing common metadata, with both classes inheriting from it.

## Q3 — AudioBook and Thesis

Added an `AudioBook` class with narrator and duration fields, and a `Thesis` class with university, degree, and supervisor fields.

`AudioBook` derives from `Book` because it retains book-related metadata, including authors and ISBN, while adding audio-specific information.

`Thesis` derives directly from `Resource` because it represents an academic work with its own metadata and does not necessarily require the pricing behavior or fields of a print book or electronic resource.

The thesis can be assigned a zero unit price when it is available free of cost.

## Q4 — Hardcover Pricing

Updated the pricing behavior for hardcover books so that hardcover copies cost 20% more than their listed unit price.

The existing pricing behavior for paperback books remains unchanged. The calculation is implemented through the overridden `costFor()` method.

## Q5 — Bulk Discounts

Implemented bulk discounts for print and electronic resources.

- **Print resources:** A 10% discount applies when at least 10 copies are purchased in one transaction.
- **Electronic resources:** Seats numbered 1 through 50 are charged at the regular rate; each additional seat is charged at half the regular rate.

The discount logic is implemented in the resource pricing methods while preserving the existing pricing behavior for smaller purchases.

## Q6 — Taxes

Implemented configurable tax rates for print and electronic resources.

Purchase calculations record both pre-tax and post-tax costs. Budget and quota validation uses the post-tax amount so that taxes are included when determining whether a purchase can be approved.

The purchase report displays the pre-tax cost, tax-inclusive cost, and relevant purchase information.

## Q7 — Different-Title Quota

Extended the budget and quota functionality to limit the number of distinct resource titles purchased in each category.

The system tracks distinct purchased titles separately from the total number of units. Purchasing additional copies of an already-purchased title does not count as a new distinct title.

A purchase that exceeds the configured distinct-title limit is rejected without changing the budget or quota usage.

## Q8 — Order Cancellation

Implemented cancellation of approved purchases.

When an order is cancelled, the system refunds the corresponding budget expenditure, restores the applicable quota usage, and reduces the catalogue holdings by the number of units originally purchased.

The original purchase record remains in the order history, and a separate cancellation record is added for traceability. This preserves the transaction history while reflecting the reversal in the current budget, quota, and holdings totals.

## Q9 — Department-Specific Budgets and Quotas

**Implementation**

- Added support for multiple named departments, each with its own budget, category quotas, and spending usage.
- Extended purchase requests and purchase records to track the department associated with each transaction.
- Added department-aware purchase validation, purchasing, and batch processing.
- Unknown departments are rejected during purchase validation.
- Updated order reports to display the department charged for each order.
- Updated cancellation logic so refunds return to the budget of the department that originally paid.

**Design decisions**

- Stored department budgets in a map keyed by department name.
- Preserved the existing single-budget API for backward compatibility.
- Recorded the department in purchase history so cancellations can identify the correct budget.

**Testing**

- Tested independent spending across Computer Science and Physics budgets.
- Tested rejection of purchases for unknown departments.
- Tested cancellation and refunding to the original department.
- All 155 checks passed.


## Q10 — Year-End Budget Rollover and Quota Warnings

**Implementation**
- Added `Budget::rollover(int carryForwardPercent) const`. The next year's total is the configured percentage of the current year's unspent balance.
- Category and title quota limits carry forward, while spending and usage reset for the new year. The original budget remains unchanged.
- Percentages outside 0–100 are rejected with `std::invalid_argument`.
- Added `Budget::quotaWarnings()`, which reports category unit or spending quotas only when usage is strictly above 80% of the corresponding positive limit.

**Design decision**
- Quota warnings are returned as strings, allowing the demo or caller to display them without coupling `Budget` to console output.

**Testing**
- Tested 0%, 50%, and 100% rollover, quota preservation, reset usage, invalid percentages, and unchanged original budget.
- Tested that exactly 80% does not warn, usage above 80% triggers the appropriate warning, and categories without quotas do not warn.

## Q11 — All-or-Nothing Batch Processing

Added an optional `allOrNothing` parameter to both `processBatch`
overloads. The default remains `false`, preserving the existing
independent-request behavior.

When enabled, the entire batch is validated against trial copies of
the relevant budget or department budgets before purchases are made.
If any request is rejected, no purchases are committed and catalog
holdings and spending remain unchanged. Rejected outcomes are recorded
in order history. If every request passes validation, the batch is
processed normally.

Tests cover both an aborted batch and a successful batch.

## Q12 — Vendors

Added vendor offers associated with resource titles. Each offer stores
a vendor name and unit price. When multiple offers exist, the
AcquisitionManager selects the lowest-priced offer for quotes and
purchases.

The selected vendor is recorded in each purchase record and displayed
in the acquisition report. Registering an existing vendor again
updates that vendor's price.

Resources without registered vendor offers retain their catalog
pricing and use "Catalog" as the vendor label. Existing tax, budget,
quota, department, cancellation, and batch-processing behavior is
preserved.

## Q13 — Catalogue Searches and Save/Load

Extended the catalogue with searches by author, ISBN/ISSN, and publication-year range.

- **Author search:** Performs a case-insensitive partial match against author names in books and e-books.
- **Identifier search:** Matches ISBN values for books and e-books, and ISSN values for journals.
- **Publication-year search:** Returns resources whose publication years fall within the specified inclusive range.
- **Save/load:** `Catalog::save()` writes one record per line with an explicit resource-type tag. `Catalog::load()` uses that tag to reconstruct the correct derived class and restores catalogue holdings.

The searches reuse the existing `where()` filtering method and return results in catalogue ID order. Invalid year ranges are rejected, and empty author or identifier queries return no results. The save format uses quoted strings to preserve spaces and punctuation, and stores monetary values in minor units with their currency codes.

**Design decision:** Serialization uses stream-based methods so callers can save/load through files or other C++ streams. Unknown resource types and malformed records are rejected with an exception.

**Testing:** Added round-trip coverage for `Book`, `AudioBook`, `EBook`, `ElectronicResource`, `Journal`, and `Thesis`, including derived-type restoration, subtype fields, currencies, and holdings.


### Q14 — Lending

Implemented lending and electronic access-session management in `Catalog`.

- **Print resources:** Patrons can borrow and return copies. The total number of borrowed copies cannot exceed the resource's holdings.
- **Electronic resources:** Patrons can open and close access sessions. The number of active sessions cannot exceed the number of licensed seats.
- **Availability tracking:** Active loans and sessions are tracked separately for each patron and resource.
- **Validation:** Rejects attempts to exceed availability, return a copy that the patron has not borrowed, close a session that is not active, or use a print-lending operation on an electronic resource and vice versa.
- **Holdings integrity:** Holdings cannot be reduced below the number of active loans or sessions.

**Design decision:** Lending and session tracking are implemented in `Catalog`, which already owns resources and maintains their holdings. This keeps availability management alongside catalogue inventory management.

**Testing:** Added tests for borrowing limits, returns, session limits, closing sessions, invalid operations, and missing resource IDs. All 231 checks passed after adding the Q14 tests.


### Q15 — Currency Support in Money

Extended the `Money` class to store a currency code alongside the amount, while retaining integer-based fixed-point storage in minor units.

- **Currency code:** Added a `currency()` accessor and optional currency-code parameters to `Money::of()` and `Money::fromMinor()`. The default currency is `INR` for backward compatibility.
- **Arithmetic validation:** Addition and subtraction throw `std::invalid_argument` when the operands use different currencies.
- **Comparison validation:** All six comparison operators (`==`, `!=`, `<`, `<=`, `>`, and `>=`) throw `std::invalid_argument` when the currencies differ.
- **Input validation:** An empty currency code is rejected by the `Money::of()` factory.
- **Compatibility:** Existing calls such as `Money::of(10)` and `Money::fromMinor(1050)` continue to work using INR by default.

**Design decision:** Currency codes are stored as strings. Arithmetic and comparisons validate currency compatibility before operating on the integer minor-unit values. No implicit currency conversion is performed.

**Testing:** Added tests for currency storage, default currency behavior, mixed-currency arithmetic, all comparison operators, and empty currency codes. All 243 checks passed.


### Q16 — Replace Test Macros with `assert`

Replaced the custom `CHECK` and `CHECK_THROWS` test macros with standard C++ `assert` and a templated `assertThrows` helper.

- **Ordinary checks:** Replaced `CHECK(condition)` with `assert(condition)`.
- **Exception checks:** Replaced `CHECK_THROWS(expression, ExceptionType)` with `assertThrows<ExceptionType>(...)`.
- **Exception validation:** The helper passes only when the expected exception type is caught; a different exception type does not satisfy the test.
- **Test coverage:** Preserved the existing test cases while removing the custom check counters and failure-summary output.

**Design decision:** Standard `assert` is used for Boolean conditions, while `assertThrows` provides equivalent expected-exception checks.

**Testing:** The project builds successfully after the test framework conversion. Assertions must remain enabled during test execution.
