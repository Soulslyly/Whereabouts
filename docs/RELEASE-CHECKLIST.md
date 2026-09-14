# 1.1.0 Release Checklist

1. Confirm the target Skyrim runtime, matching SKSE, Address Library, and loaded SKSE Menu Framework DLL fixed version.
2. Perform a clean Release configure and build with the locked vcpkg dependencies.
3. Compile all four current Papyrus sources with the official compiler and exact import order for the 1.5.97, 1.6.1170, and 1.7.104 setups. Ship only fresh PEX files proven to match the current PSC inputs. Require canonical zero timestamp and `Whereabouts` user/computer metadata with `tools/normalize-pex-metadata.ps1 -ValidateOnly`; do not require byte-identical payloads across independent compiler runs because Bethesda's compiler renumbers internal temporary symbols.
4. Generate and validate `Whereabouts.esp` independently with the typed plugin builder; do not supply an original/template plugin.
5. Run Debug and Release CTest presets with `WHEREABOUTS_SMF_DLL` pointed at the exact installed 3.14 DLL; confirm the derived export validator passes and the exact older installed DLL is rejected.
6. Run the full in-game matrix in `TESTING.md` on Skyrim 1.5.97, 1.6.1170, and 1.7.104, including all Search content/order choices, configurable live/full limits, fresh-save location indexing, mixed and dedicated location search, compact/detailed Location rows, every Location Copy choice, interior/exterior/custom-cell travel, save/reload, marker visibility, commands, whole-row interaction, table sorting/resizing/reordering, EditorID search, typo suggestions, frame-paced More Informative Console repainting, translated death notifications, and prepared/unprepared removal cases.
7. Run `tools/stage-release.ps1 -CreateArchives`.
8. Run `tools/validate-release.ps1 -RequireArchives -MenuFrameworkDll "path\to\SKSEMenuFramework.dll"` with the exact installed supported SMF binary.
9. Run `tools/stage-github-source.ps1` and `tools/validate-github-source.ps1` separately. Confirm the public source manifest includes the API documentation, worksheet generator, and translator worksheet while rejecting internal project material and generated output. Confirm Main matches its explicit deployable allowlist and the Translation archive contains only the eight optional language overwrite files.
10. Run the Skyrim ship gate against the final main archive.
11. Record final archive hashes and label the output a test candidate until all advertised runtime matrices pass.
12. Test the same universal DLL on the exact 1.5.97/SKSE 2.0.20/legacy Address Library/BEES, 1.6.1170/SKSE 2.2.6/Address Library v12, and 1.7.104/SKSE 2.3.1/Address Library v13 environments before advertising those runtimes.
13. Publish the Main and optional Translation archives only after the applicable runtime gates pass. Publish source through the GitHub repository and version tag; never create or attach a Source ZIP.
