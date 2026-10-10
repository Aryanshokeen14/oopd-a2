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