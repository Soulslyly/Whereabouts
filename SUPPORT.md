# Whereabouts Support

You can report a problem on Nexus or through [GitHub Issues](https://github.com/Soulslyly/Whereabouts/issues). GitHub is optional.

## Before Reporting

- Confirm `Whereabouts.esp` is enabled.
- Confirm SKSE, Address Library, and SKSE Menu Framework match your Skyrim runtime.
- On Skyrim 1.5.97, confirm BEES is installed and active.
- Check [Known Issues](KNOWN_ISSUES.md).
- Reproduce the problem once more if it is safe to do so.

## If an NPC Is Missing from Search

1. Submit the search with **Search** or Enter; the live preview may show fewer rows.
2. If Whereabouts says matching NPCs are hidden, choose **Clear Filters and Search Again**.
3. Try the NPC's runtime FormID, with or without leading zeroes. Surrounding spaces are ignored.
4. If Whereabouts says the NPC is absent from the current index, choose **Refresh Index and Search Again**.
5. If the NPC is still missing, attach `Whereabouts.log`. Version 1.0.1 records one concise diagnostic for a submitted search with no visible results.

Do not clear filters automatically when reporting a filter problem; include the active-filter count shown in the collapsed **Filters and sorting** heading.

## What to Include

- Skyrim runtime version, such as `1.5.97`, `1.6.1170`, or `1.7.104`
- SKSE and SKSE Menu Framework versions
- Mod manager and whether the issue also happens on a new test save
- What you clicked, what you expected, and what actually happened
- NPC name, plugin, and FormID or stable ID when one NPC is involved
- Whether the NPC was loaded, disabled, dead, or in another worldspace
- `Whereabouts.log`
- A crash log if Skyrim crashed

Whereabouts writes its log to the SKSE log folder. On a standard Steam installation this is normally:

```text
Documents\My Games\Skyrim Special Edition\SKSE\Whereabouts.log
```

Other Skyrim editions and profile setups may use a corresponding game-specific Documents folder.

## Please Do Not Post

- Nexus or GitHub API keys
- Account passwords or authentication tokens
- Entire save files unless specifically requested
- Large mod lists with no reproduction steps

Use the [bug-report template](https://github.com/Soulslyly/Whereabouts/issues/new?template=bug_report.md) when possible. Feature ideas and translation corrections have their own templates.
