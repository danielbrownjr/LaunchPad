# GitHub planning setup: resources still pending

The GitHub connection can create issues, branches and repository files, but does **not** expose native label, milestone or Wiki administration. This page is a configuration specification, not proof that those resources exist.

## Milestones to create (no due dates)
- M0: Foundation and board verification
- M1: v0.1 Offline Launchpad
- M2: v0.2 Connected dashboard
- M3: v0.3 Departure experience and enclosure
- M4: v1.0 Authorized Simpro integration

Descriptions and exit criteria: [Roadmap](Roadmap.md).

## Custom labels
`type:feature`, `type:task`, `type:research`, `type:docs`;
`area:display`, `area:firmware`, `area:network`, `area:bridge`, `area:simpro`, `area:hardware`, `area:security`, `area:ux`;
`priority:high`, `priority:medium`, `priority:low`;
`blocked:approval`, `blocked:hardware`.

Preserve GitHub defaults when useful. Native milestone assignments and custom label application must wait until resource creation. Initial issue titles/body specify the planned M0–M4 stage.

## Wiki publication
The Markdown files in `docs/wiki/` are the editable canonical drafts. They do **not** create GitHub's separate native Wiki. Once Wiki is enabled and an authorized method is available, publish/sync these pages and verify links.

## Follow-up checklist
Create native resources; attach each issue to corresponding milestone; apply precise custom labels; publish the Wiki; read back resource assignments and links. Avoid inventing due dates or representing demo data as live.
