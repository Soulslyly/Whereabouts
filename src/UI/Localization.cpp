#include "PCH.h"

#include "UI/Localization.h"
#include "UI/LocalizationResource.h"

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace whereabouts::ui
{
    namespace
    {
        std::array<std::string, kTranslationCatalog.size()> values;
        const auto indexes = [] {
            std::unordered_map<std::string_view, std::size_t> result;
            result.reserve(kTranslationCatalog.size());
            for (std::size_t index = 0; index < kTranslationCatalog.size(); ++index) {
                result.emplace(kTranslationCatalog[index].english, index);
            }
            return result;
        }();
        std::mutex loadMutex;
        std::atomic_bool initialized{false};
        std::string loadedLanguage{"ENGLISH"};

        std::string CurrentGameLanguage()
        {
            auto* collection = RE::INISettingCollection::GetSingleton();
            const auto* setting = collection ? collection->GetSetting("sLanguage:General") : nullptr;
            return NormalizeTranslationLanguage(
                setting && setting->GetType() == RE::Setting::Type::kString && setting->data.s ?
                    setting->data.s : "ENGLISH");
        }

        std::expected<std::vector<std::byte>, std::string> ReadTranslationBytes(
            std::string_view language)
        {
            constexpr std::size_t kMaximumTranslationBytes = 2 * 1024 * 1024;
            const auto path = std::format(
                "Interface\\Translations\\Whereabouts_{}.txt", language);
            RE::BSResourceNiBinaryStream stream{path};
            if (!stream.good()) return std::unexpected(std::format("{} is unavailable", path));

            std::vector<std::byte> bytes;
            bytes.reserve(64 * 1024);
            std::byte value{};
            while (bytes.size() < kMaximumTranslationBytes && stream.get(value)) {
                bytes.push_back(value);
            }
            if (bytes.size() == kMaximumTranslationBytes && stream.get(value)) {
                return std::unexpected(std::format("{} exceeds the size limit", path));
            }
            return bytes;
        }
    }

    std::string ResolveConfiguredTranslationLanguage(TranslationLanguage language) noexcept
    {
        try {
            return ResolveTranslationLanguage(
                TranslationLanguageSettingText(language), CurrentGameLanguage());
        } catch (...) {
            return "ENGLISH";
        }
    }

    bool InitializeLocalization(TranslationLanguage languageChoice) noexcept
    {
        if (initialized.load(std::memory_order_acquire)) return true;
        try {
            std::scoped_lock lock(loadMutex);
            if (initialized.load(std::memory_order_relaxed)) return true;
            const auto language = ResolveConfiguredTranslationLanguage(languageChoice);
            const auto bytes = ReadTranslationBytes(language);
            if (!bytes) {
                logger::warn(
                    "Whereabouts translation is unavailable; using English for this process: {}",
                    bytes.error());
                return false;
            }
            const auto translations = ParseTranslationResource(*bytes);
            if (!translations) {
                logger::error(
                    "Whereabouts translation load failed for {}; using English for this process: {}",
                    language,
                    translations.error());
                return false;
            }
            std::array<std::string, kTranslationCatalog.size()> loaded;
            for (std::size_t index = 0; index < kTranslationCatalog.size(); ++index) {
                const auto& entry = kTranslationCatalog[index];
                const auto translated = translations->find(std::string{entry.key});
                loaded[index] = translated != translations->end() && !translated->second.empty() ?
                    translated->second : std::string{entry.english};
            }
            values = std::move(loaded);
            loadedLanguage = language;
            initialized.store(true, std::memory_order_release);
            logger::info(
                "Whereabouts translations loaded for {}: {} catalog entries, {} resource rows",
                language,
                kTranslationCatalog.size(),
                translations->size());
            return true;
        } catch (...) {
            try { logger::error("Whereabouts translations failed to initialize; using English"); }
            catch (...) {}
            return false;
        }
    }

    std::string LoadedTranslationLanguage() noexcept
    {
        try {
            return initialized.load(std::memory_order_acquire) ? loadedLanguage : "ENGLISH";
        } catch (...) {
            return "ENGLISH";
        }
    }

    const char* TranslateText(const char* english) noexcept
    {
        if (!english || !initialized.load(std::memory_order_acquire)) return english;
        const auto found = indexes.find(english);
        return found == indexes.end() ? english : values[found->second].c_str();
    }

    std::string TranslateOwned(std::string_view english) noexcept
    {
        try {
            if (!initialized.load(std::memory_order_acquire)) return std::string{english};
            const auto found = indexes.find(english);
            return found == indexes.end() ? std::string{english} : values[found->second];
        } catch (...) {
            return std::string{english};
        }
    }

    std::string TranslateFormatArgs(const char* englishFormat, std::format_args args) noexcept
    {
        if (!englishFormat) return {};
        const auto translated = TranslateOwned(englishFormat);
        return FormatLocalizedTextArgs(translated, englishFormat, args);
    }
}
