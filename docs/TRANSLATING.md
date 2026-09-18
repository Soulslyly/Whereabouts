# Translating Whereabouts

Use `Interface/Translations/Whereabouts_ENGLISH.txt` as the runtime-file source template. For spreadsheet work, use `translations/Whereabouts_1.1.0_Translator_Worksheet.tsv`.

The 2.0.0 English catalog contains the menu, all filter labels and help, status, maintenance, warning, search-recovery, command, Inspector, tracking-recovery and tracked-death notification text. NPC, race, faction, keyword, cell and location names come from the game and their source mods. `Whereabouts` is a proper name; tracked quest objectives use each NPC's alias name. The journal description is embedded in `Whereabouts.esp`, not in this menu translation file; translating that description requires a translated plugin. Low-level third-party error details may remain in English.

Each row has this exact structure:

```text
$Whereabouts_InternalKey<TAB>English text shown in the menu
```

Only translate the text on the right side of the tab. Do not translate, rename, reorder, or remove the `$Whereabouts_*` key on the left. The wide gap visible in some editors is one tab character required by Skyrim, not accidental spacing.

The UTF-8 worksheet has four columns: Internal key, English source, blank Translation, and required Placeholders. Fill only the Translation column. The worksheet is not installed by Skyrim; copy completed values back into the right side of a runtime translation file. Regenerate it with `tools/generate-translator-worksheet.ps1` after intentionally changing the English catalog.

Keep all `{}` placeholders exactly as they appear. They are replaced with names, counts, IDs, or status text while the menu is running. Do not add tabs or line breaks inside a value.

Save the finished file with the matching Skyrim filename:

- `Whereabouts_CHINESE.txt`
- `Whereabouts_FRENCH.txt`
- `Whereabouts_GERMAN.txt`
- `Whereabouts_ITALIAN.txt`
- `Whereabouts_JAPANESE.txt`
- `Whereabouts_POLISH.txt`
- `Whereabouts_RUSSIAN.txt`
- `Whereabouts_SPANISH.txt`

The required encoding is UTF-16 little-endian with a BOM and Windows CRLF line endings. Keep exactly one tab between each key and value. Every key must remain present, even when a value temporarily stays in English.

Install the translated file under `Interface/Translations`, select it from Whereabouts Settings, then fully restart Skyrim. `Follow Skyrim (default)` instead uses Skyrim's `sLanguage:General` setting.

The Main file contains the translated resources for all nine supported language filenames and uses English fallbacks for untranslated values. There is no separate translation download. Regenerating resources preserves existing right-side translations and adds new keys with English fallback values. Review newly added keys before publishing a translation. Chinese and Japanese also require a menu-framework font that contains the relevant glyphs; question marks or empty glyphs are not repaired by changing the text encoding alone.
