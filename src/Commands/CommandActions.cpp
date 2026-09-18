#include "Commands/CommandActions.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <format>
#include <sstream>

namespace whereabouts
{
    namespace
    {
        constexpr std::size_t kMaximumCustomLabelLength = 64;
        constexpr std::size_t kMaximumCustomCommandLength = 512;
        constexpr std::size_t kMaximumExpandedCommandLength = 1024;

        std::string HexFormID(std::uint32_t value)
        {
            return std::format("{:08X}", value);
        }

        bool HasControlCharacters(std::string_view value)
        {
            return std::ranges::any_of(value, [](unsigned char character) {
                return std::iscntrl(character) != 0;
            });
        }

        bool HasCommandChaining(std::string_view value)
        {
            return value.find(';') != std::string_view::npos;
        }

        std::expected<std::string, std::string> Expand(
            std::string_view command,
            std::uint32_t referenceRuntimeFormID,
            std::uint32_t baseRuntimeFormID)
        {
            std::string expanded;
            expanded.reserve(command.size() + 16);
            for (std::size_t index = 0; index < command.size();) {
                if (command[index] != '{') {
                    if (command[index] == '}') {
                        return std::unexpected("Custom command contains an unmatched '}'");
                    }
                    expanded.push_back(command[index++]);
                    continue;
                }
                const auto close = command.find('}', index + 1);
                if (close == std::string_view::npos) {
                    return std::unexpected("Custom command contains an unmatched '{'");
                }
                const auto placeholder = command.substr(index, close - index + 1);
                if (placeholder == "{refid}") {
                    expanded += HexFormID(referenceRuntimeFormID);
                } else if (placeholder == "{baseid}") {
                    expanded += HexFormID(baseRuntimeFormID);
                } else {
                    return std::unexpected(std::format(
                        "Unknown custom-command placeholder: {}", placeholder));
                }
                index = close + 1;
            }
            if (expanded.size() > kMaximumExpandedCommandLength) {
                return std::unexpected("Expanded custom command is too long");
            }
            return expanded;
        }
    }

    std::optional<std::size_t> CustomSlotForActionID(
        std::string_view actionID) noexcept
    {
        constexpr std::string_view prefix = "custom.";
        if (!actionID.starts_with(prefix)) return std::nullopt;
        std::size_t oneBased = 0;
        const auto number = actionID.substr(prefix.size());
        const auto parsed = std::from_chars(
            number.data(), number.data() + number.size(), oneBased);
        if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size() ||
            oneBased == 0 || oneBased > kCustomCommandSlotCount) {
            return std::nullopt;
        }
        return oneBased - 1;
    }

    std::expected<void, std::string> ApplyVerifiedActorFlagMutation(
        ActorFlagPair before,
        ActorFlagPair desired,
        const std::function<void(ActorFlagPair)>& write,
        const std::function<ActorFlagPair()>& read)
    {
        write(desired);
        if (read() == desired) return {};
        write(before);
        if (read() != before) {
            return std::unexpected(
                "Skyrim did not apply both base NPC flags and rollback could not be verified");
        }
        return std::unexpected(
            "Skyrim did not apply both base NPC flags; the change was rolled back");
    }

    std::string CustomActionID(std::size_t slot)
    {
        return slot < kCustomCommandSlotCount ?
            std::format("custom.{}", slot + 1) : std::string{kDisabledActionID};
    }

    void OriginalActorFlagStore::BeginSession(OperationEpochToken token) noexcept
    {
        try {
            std::scoped_lock lock(mutex_);
            if (session_ == token) return;
            session_ = token;
            originals_.clear();
        } catch (...) {}
    }

    void OriginalActorFlagStore::Clear() noexcept
    {
        try {
            std::scoped_lock lock(mutex_);
            session_ = {};
            originals_.clear();
        } catch (...) {}
    }

    void OriginalActorFlagStore::RememberAfterSuccessfulChange(
        std::uint32_t ownerRuntimeFormID,
        ActorFlagPair original,
        OperationEpochToken token) noexcept
    {
        if (ownerRuntimeFormID == 0 || !token) return;
        try {
            std::scoped_lock lock(mutex_);
            if (session_ != token) return;
            originals_.try_emplace(ownerRuntimeFormID, original);
        } catch (...) {}
    }

    std::optional<ActorFlagPair> OriginalActorFlagStore::Original(
        std::uint32_t ownerRuntimeFormID,
        OperationEpochToken token) const noexcept
    {
        if (ownerRuntimeFormID == 0 || !token) return std::nullopt;
        try {
            std::scoped_lock lock(mutex_);
            if (session_ != token) return std::nullopt;
            const auto found = originals_.find(ownerRuntimeFormID);
            return found == originals_.end() ? std::nullopt : std::optional{found->second};
        } catch (...) {
            return std::nullopt;
        }
    }

    std::expected<void, std::string> ValidateCustomCommand(
        std::string_view label,
        std::string_view command)
    {
        if (label.empty()) return std::unexpected("Custom command label is required");
        if (command.empty()) return std::unexpected("Custom command text is required");
        if (label.size() > kMaximumCustomLabelLength) {
            return std::unexpected("Custom command label is too long");
        }
        if (command.size() > kMaximumCustomCommandLength) {
            return std::unexpected("Custom command text is too long");
        }
        if (HasControlCharacters(label) || HasControlCharacters(command)) {
            return std::unexpected("Custom commands cannot contain control characters or new lines");
        }
        if (HasCommandChaining(command)) {
            return std::unexpected("Custom commands must contain exactly one console command");
        }
        const auto expanded = Expand(command, 0, 0);
        if (!expanded) return std::unexpected(expanded.error());
        return {};
    }

    std::expected<std::string, std::string> ExpandCustomCommand(
        std::string_view command,
        std::uint32_t referenceRuntimeFormID,
        std::uint32_t baseRuntimeFormID)
    {
        if (command.empty()) return std::unexpected("Custom command text is required");
        if (command.size() > kMaximumCustomCommandLength || HasControlCharacters(command) ||
            HasCommandChaining(command)) {
            return std::unexpected("Custom command text is invalid");
        }
        return Expand(command, referenceRuntimeFormID, baseRuntimeFormID);
    }

    std::string SharedBaseWarning(std::size_t indexedReferenceCount)
    {
        if (indexedReferenceCount <= 1) return {};
        return std::format(
            "This changes the effective base NPC used by {} currently indexed references.",
            indexedReferenceCount);
    }

    std::string FormatNpcReport(const NpcSnapshot& npc)
    {
        std::ostringstream report;
        report << npc.displayName << '\n'
               << "Reference FormID: " << std::format("{:08X}", npc.ReferenceRuntimeID()) << '\n'
               << "Base FormID: " << std::format("{:08X}", npc.BaseRuntimeID()) << '\n'
               << "Plugin: " << (npc.SourcePlugin().empty() ? "Dynamic / unavailable" : npc.SourcePlugin()) << '\n'
               << "Location: " << (npc.spatial.location.empty() ?
                    (npc.spatial.cell.empty() ? "Unknown" : npc.spatial.cell) : npc.spatial.location) << '\n'
               << "Essential: " << (npc.actorFlagsKnown ? (npc.essential ? "Yes" : "No") : "Unknown") << '\n'
               << "Protected: " << (npc.actorFlagsKnown ? (npc.protectedActor ? "Yes" : "No") : "Unknown") << '\n'
               << "Indexed references sharing base: " << npc.indexedReferencesSharingBase << '\n';
        const auto* projection = npc.recordProjection.get();
        if (!projection) return report.str();
        report << "Indexed references sharing effective Base Data owner: "
               << npc.indexedReferencesSharingBaseDataOwner << '\n'
               << "Original plugin: "
               << (projection->provenance.known ? projection->provenance.originalPlugin : "Unknown") << '\n'
               << "Winning plugin: "
               << (projection->provenance.known ? projection->provenance.winningPlugin : "Unknown") << '\n'
               << "Plugin record count: "
               << (projection->provenance.known ?
                    std::to_string(projection->provenance.PluginRecordCount()) : "Unknown") << '\n'
               << "Class: " << (projection->classKnown ? projection->npcClass.label : "Unknown") << '\n'
               << "Voice type: " << (projection->voiceTypeKnown ? projection->voiceType.label : "Unknown") << '\n'
               << "Combat style: " << (projection->combatStyleKnown ? projection->combatStyle.label : "Unknown") << '\n';
        if (projection->provenance.known) {
            report << "Touching plugins (load order):";
            for (const auto& plugin : projection->provenance.touchingPlugins) {
                report << "\n- " << plugin;
            }
            report << '\n';
        }
        if (!projection->levelScaling.known) {
            report << "Level scaling: Unknown\n";
        } else if (projection->levelScaling.playerLevelMult) {
            report << "Level scaling: PC level x "
                   << std::format("{:.3f}", static_cast<float>(projection->levelScaling.value) / 1000.0F)
                   << " (min " << projection->levelScaling.minimum
                   << ", max " << projection->levelScaling.maximum << ")\n";
        } else {
            report << "Level scaling: Fixed level " << projection->levelScaling.value << '\n';
        }
        return report.str();
    }
}
