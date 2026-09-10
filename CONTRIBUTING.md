# Contributing to Whereabouts

Bug reports, feature ideas, translation corrections, API use cases, and code improvements are welcome. You do not need to contribute code to help.

## Bug Reports

Read [Support](SUPPORT.md) and [Known Issues](KNOWN_ISSUES.md), then use the [bug-report template](https://github.com/Soulslyly/Whereabouts/issues/new?template=bug_report.md). Clear reproduction steps are more useful than a long load order by itself.

## Feature Ideas

Check the [Roadmap](ROADMAP.md) before opening a request. Describe the problem or use case first; an implementation idea is welcome, but it should not depend on invented Skyrim, SKSE, CommonLib, or SKSE Menu Framework behavior.

## Translations

Follow [Translating Whereabouts](docs/TRANSLATING.md). Translate the text on the right side of each tab, preserve the keys on the left, retain the documented encoding and line format, and state whether the result was checked in game by a native speaker.

## Papyrus API and Code

The public API is intentionally read-only. Start with the [Papyrus API documentation](docs/PAPYRUS_API.md) and explain the real compatibility use case before proposing a new function.

Build instructions and dependency pins are in [README.md](README.md) and [docs/DEPENDENCIES.md](docs/DEPENDENCIES.md). Runtime changes should include focused regression coverage and should not change the save format, quest layout, or supported runtimes without an explicit compatibility plan.

## Ownership and Licensing

Whereabouts is open-permissions under [LICENSE](LICENSE). Third-party components retain their own licenses under [LICENSES](LICENSES). Do not submit code, assets, or translated text copied from another mod unless its license permits redistribution and the required credit is included.

By contributing, you confirm that you created the contribution or have the right to submit it under compatible terms.
