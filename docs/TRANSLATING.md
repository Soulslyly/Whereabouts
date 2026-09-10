# Translating Whereabouts

Use `Interface/Translations/Whereabouts_ENGLISH.txt` as the source template.

The 1.0.1 template contains the menu, status, maintenance, warning, search-recovery, tracking-recovery and tracked-death notification text. NPC, cell and location names come from the game and their source mods. `Whereabouts` is a proper name; tracked quest objectives use each NPC's alias name. The restored journal description is embedded in `Whereabouts.esp`, not in this menu translation file; translating that description requires a translated plugin. Low-level third-party error details may remain in English.

Each row has this exact structure:

```text
$Whereabouts_InternalKey<TAB>English text shown in the menu
```

Only translate the text on the right side of the tab. Do not translate, rename, reorder, or remove the `$Whereabouts_*` key on the left. The wide gap visible in some editors is one tab character required by Skyrim, not accidental spacing.

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

The Main file intentionally contains English fallbacks for all nine supported language filenames. The optional Translations file contains translated overwrites. Regenerating resources preserves existing right-side translations and adds new keys with English fallback values. Review newly added keys before publishing a translation. Chinese and Japanese also require a menu-framework font that contains the relevant glyphs; question marks or empty glyphs are not repaired by changing the text encoding alone.
