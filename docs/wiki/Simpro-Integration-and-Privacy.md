# Simpro integration: approval and privacy

**Hard requirement:** no live connection to the employer's Simpro tenant until explicitly approved by ILS/authorized admin and IT/security. Having Simpro mobile-app access does not authorize a separate API consumer or credentials. Never scrape the app or bypass the normal administrative path.

## Intended scope
The user's own service schedule: appointment times, minimal site/customer label, service call/job reference, status, and perhaps assigned tasks. All updates remain inside the official mobile app.

## Questions for the approval request
- Is a personally owned entryway display permitted to show customer/site details?
- Is API access allowed? Who provisions, rotates, audits, and revokes credentials?
- Are API credentials tenant-wide rather than permission-scoped? What mitigations are required?
- Which endpoints/fields are approved, and how are technician assignments mapped?
- What are rate limits, retention rules, logging/auditing requirements and privacy restrictions?
- Where may a bridge run, and may it cache company data on a personal network?

## Proposed boundary
The authenticated bridge keeps company credentials off the MCU, sends only an approved allowlist of fields, and has bounded cache TTL, backoff and redacted logs. Display is read-only; no mutation endpoints or job commands. Restrict local clients and use encrypted transport where appropriate. Last-sync timestamp must reflect the real underlying data freshness.

## Demo-first
Use exclusively fictional data in GitHub, automated tests, screenshots and public demos. An unavailable, stale or unauthorized Simpro source must never masquerade as a current schedule. If approval is denied, LaunchPad remains usable with local/personal data.
