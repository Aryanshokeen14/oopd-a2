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


## Q10 — Year-End Budget Rollover

**Implementation**
- Added `Budget::rollover(int carryForwardPercent) const`.
- The next year's total budget is the configured percentage of the current year's unspent balance.
- Category quotas and title quotas carry forward, while spending and usage reset for the new year.
- The original budget remains unchanged.
- Percentages outside the inclusive range 0–100 are rejected with `std::invalid_argument`.

**Testing**
- Tested 0%, 50%, and 100% rollover.
- Tested quota preservation and resetting of category and title usage.
- Tested invalid percentages and confirmed that the original budget remains unchanged.

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