using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Plugins.Records;
using Mutagen.Bethesda.Skyrim;
using Mutagen.Bethesda.Strings;

internal static class PluginCommands
{
    private const string PluginName = "Whereabouts.esp";
    private const string QuestEditorId = "WhereaboutsTrackingQuest";
    private const string QuestScript = "WhereaboutsQuest";
    private const string AliasScript = "WhereaboutsTrackedAlias";
    private const int TrackedCount = 100;

    public static int Build(string outputPath)
    {
        var outputModKey = ModKey.FromFileName(PluginName);
        var output = new SkyrimMod(outputModKey, SkyrimRelease.SkyrimSE, 1.71F, false)
        {
            IsSmallMaster = true
        };
        output.ModHeader.Author = "Whereabouts";
        output.ModHeader.Description = "Tracking quest for Whereabouts - Search, Locate, and Track NPCs";
        output.ModHeader.MasterReferences.Add(new MasterReference
        {
            Master = ModKey.FromFileName("Skyrim.esm")
        });

        var quest = new Quest(new FormKey(outputModKey, 0x801), SkyrimRelease.SkyrimSE);
        quest.EditorID = QuestEditorId;
        quest.Name = new TranslatedString(Language.English, "Whereabouts");
        quest.Description = new TranslatedString(Language.English, "Find your tracked NPCs using their map markers. Manage tracking in the Whereabouts menu.");
        quest.FormVersion = 44;
        quest.Flags = Quest.Flag.StartGameEnabled | (Quest.Flag)0x10;
        quest.Priority = 0;
        quest.QuestFormVersion = byte.MaxValue;
        quest.Type = Quest.TypeEnum.None;
        quest.NextAliasID = TrackedCount + 2;

        quest.Aliases.Add(new QuestAlias
        {
            ID = 1,
            Type = QuestAlias.TypeEnum.Reference,
            Name = "PlayerRef",
            Flags = 0,
            ForcedReference = new FormLinkNullable<IPlacedGetter>(
                new FormKey(ModKey.FromFileName("Skyrim.esm"), 0x14))
        });

        var stage = new QuestStage
        {
            Index = 0,
            Flags = QuestStage.Flag.StartUpStage
        };
        stage.LogEntries.Add(new QuestLogEntry
        {
            Entry = new TranslatedString(Language.English, "Find your tracked NPCs using their map markers. Manage tracking in the Whereabouts menu.")
        });
        quest.Stages.Add(stage);

        for (var index = 0; index < TrackedCount; ++index)
        {
            var alias = new QuestAlias
            {
                ID = checked((uint)(index + 2)),
                Name = $"WhereaboutsTracked{index:00}",
                Type = QuestAlias.TypeEnum.Reference,
                Flags = QuestAlias.Flag.Optional |
                        QuestAlias.Flag.AllowDead |
                        QuestAlias.Flag.AllowDisabled |
                        QuestAlias.Flag.StoresText
            };
            quest.Aliases.Add(alias);

            var objective = new QuestObjective
            {
                Index = checked((ushort)index),
                DisplayText = new TranslatedString(Language.English, $"<Alias={alias.Name}>")
            };
            objective.Targets.Add(new QuestObjectiveTarget { AliasID = checked((int)alias.ID) });
            quest.Objectives.Add(objective);
        }

        quest.VirtualMachineAdapter = BuildVmad(quest);
        output.Quests.Add(quest);

        var outputDirectory = Path.GetDirectoryName(outputPath);
        if (string.IsNullOrWhiteSpace(outputDirectory))
        {
            throw new InvalidOperationException("Output path has no parent directory.");
        }

        Directory.CreateDirectory(outputDirectory);
        var temporaryDirectory = Path.Combine(outputDirectory, ".staging");
        Directory.CreateDirectory(temporaryDirectory);
        var temporaryPath = Path.Combine(temporaryDirectory, PluginName);
        if (File.Exists(temporaryPath))
        {
            File.Delete(temporaryPath);
        }

        output.BeginWrite
            .ToPath(temporaryPath)
            .WithLoadOrder(ModKey.FromFileName("Skyrim.esm"))
            .WithKnownMasters(new KeyedMasterStyle(ModKey.FromFileName("Skyrim.esm"), MasterStyle.Full))
            .Write();
        File.Move(temporaryPath, outputPath, true);
        Directory.Delete(temporaryDirectory);
        Console.WriteLine($"Built {outputPath}");
        return Validate(outputPath);
    }

    public static int Validate(string pluginPath)
    {
        var errors = new List<string>();
        if (!File.Exists(pluginPath))
        {
            Console.Error.WriteLine($"Plugin not found: {pluginPath}");
            return 2;
        }

        using var mod = SkyrimMod.CreateFromBinaryOverlay(pluginPath, SkyrimRelease.SkyrimSE);
        Check(string.Equals(mod.ModKey.FileName.String, PluginName, StringComparison.OrdinalIgnoreCase),
            $"plugin name must be {PluginName}", errors);
        Check(mod.IsSmallMaster, "plugin must carry the ESL/light flag", errors);
        Check(Math.Abs(mod.ModHeader.Stats.Version - 1.71F) < 0.0001F,
            "plugin header version must be 1.71", errors);
        Check(mod.ModHeader.MasterReferences.Count == 1 &&
              string.Equals(mod.ModHeader.MasterReferences[0].Master.FileName.String, "Skyrim.esm", StringComparison.OrdinalIgnoreCase),
            "Skyrim.esm must be the only and first master", errors);
        Check(mod.Quests.Count == 1, "plugin must contain exactly one quest", errors);

        var quest = mod.Quests.SingleOrDefault();
        if (quest is null)
        {
            return Report(errors);
        }

        Check(quest.FormKey.ID == 0x801, "quest local FormID must be 0x801", errors);
        Check(string.Equals(quest.EditorID, QuestEditorId, StringComparison.Ordinal),
            $"quest EditorID must be {QuestEditorId}", errors);
        Check(quest.Name?.String == "Whereabouts", "quest journal title must be Whereabouts", errors);
        Check(!string.IsNullOrWhiteSpace(quest.Description?.String), "quest description must be present", errors);
        Check(quest.FormVersion == 44, "quest form version must be 44", errors);
        Check((quest.Flags & Quest.Flag.StartGameEnabled) != 0,
            "quest must preserve the original Start Game Enabled marker lifecycle", errors);
        Check((quest.Flags & Quest.Flag.RunOnce) == 0,
            "quest must remain restartable when upgrading a beta save whose quest was stopped", errors);
        Check(quest.Aliases.Count == TrackedCount + 1, "quest must contain 101 aliases", errors);
        Check(quest.Objectives.Count == TrackedCount, "quest must contain 100 objectives", errors);
        Check(quest.QuestFormVersion == byte.MaxValue, "quest data version must be 255", errors);
        Check(quest.Type == Quest.TypeEnum.None, "quest type must remain None", errors);
        Check(quest.Stages.Count == 1 && quest.Stages[0].Index == 0 &&
              (quest.Stages[0].Flags & QuestStage.Flag.StartUpStage) != 0 &&
              quest.Stages[0].LogEntries.Count == 1 &&
              !string.IsNullOrWhiteSpace(quest.Stages[0].LogEntries[0].Entry?.String),
            "quest startup stage must retain its journal description", errors);

        if (quest.Aliases.Count == TrackedCount + 1)
        {
            var player = quest.Aliases[0];
            Check(string.Equals(player.Name, "PlayerRef", StringComparison.Ordinal), "alias 0 must be PlayerRef", errors);
            Check(player.ForcedReference.FormKey == new FormKey(ModKey.FromFileName("Skyrim.esm"), 0x14),
                "PlayerRef must force-fill PlayerRef 0x14 from Skyrim.esm", errors);

            for (var index = 0; index < TrackedCount; ++index)
            {
                var alias = quest.Aliases[index + 1];
                var objective = quest.Objectives[index];
                var requiredFlags = QuestAlias.Flag.Optional |
                                    QuestAlias.Flag.AllowDead |
                                    QuestAlias.Flag.AllowDisabled |
                                    QuestAlias.Flag.StoresText;
                Check(alias.Type == QuestAlias.TypeEnum.Reference, $"tracked alias {index} must be a reference alias", errors);
                Check((alias.Flags.GetValueOrDefault() & requiredFlags) == requiredFlags,
                    $"tracked alias {index} is missing required flags", errors);
                Check(objective.Index == index, $"objective {index} has the wrong index", errors);
                Check(objective.Targets.Count == 1 && objective.Targets[0].AliasID == alias.ID,
                    $"objective {index} must point to its matching alias", errors);
            }
        }

        ValidateVmad(quest, errors);
        return Report(errors);
    }

    private static void ValidateVmad(IQuestGetter quest, List<string> errors)
    {
        var adapter = quest.VirtualMachineAdapter;
        if (adapter is null)
        {
            errors.Add("quest VMAD is missing");
            return;
        }

        Check(adapter.Scripts.Count == 1 && string.Equals(adapter.Scripts[0].Name, QuestScript, StringComparison.Ordinal),
            $"quest VMAD must contain only {QuestScript}", errors);
        Check(adapter.Fragments.Count == 0, "quest must not contain compiled stage fragments", errors);
        Check(adapter.Aliases.Count == TrackedCount, "quest VMAD must contain 100 tracked alias bindings", errors);

        if (adapter.Aliases.Count != TrackedCount || quest.Aliases.Count != TrackedCount + 1)
        {
            return;
        }

        for (var index = 0; index < TrackedCount; ++index)
        {
            var alias = quest.Aliases[index + 1];
            var fragment = adapter.Aliases[index];
            Check(fragment.Property.Object.FormKey == quest.FormKey && fragment.Property.Alias == alias.ID,
                $"VMAD alias binding {index} points to the wrong quest alias", errors);
            Check(fragment.Scripts.Count == 1 && string.Equals(fragment.Scripts[0].Name, AliasScript, StringComparison.Ordinal),
                $"VMAD alias binding {index} must contain only {AliasScript}", errors);

            if (fragment.Scripts.Count == 1)
            {
                var properties = fragment.Scripts[0].Properties;
                Check(properties.Count == 1 &&
                      properties[0] is IScriptIntPropertyGetter objectiveIndex &&
                      string.Equals(objectiveIndex.Name, "ObjectiveIndex", StringComparison.Ordinal) &&
                      objectiveIndex.Data == index,
                    $"VMAD alias binding {index} has the wrong ObjectiveIndex property", errors);
            }
        }
    }

    private static QuestAdapter BuildVmad(Quest quest)
    {
        var adapter = new QuestAdapter
        {
            Version = 5,
            ObjectFormat = 2,
            ExtraBindDataVersion = 2,
            FileName = string.Empty
        };
        adapter.Scripts.Add(new ScriptEntry
        {
            Name = QuestScript,
            Flags = ScriptEntry.Flag.Local
        });

        for (var index = 0; index < TrackedCount; ++index)
        {
            var alias = quest.Aliases[index + 1];
            var script = new ScriptEntry
            {
                Name = AliasScript,
                Flags = ScriptEntry.Flag.Local
            };
            script.Properties.Add(new ScriptIntProperty
            {
                Name = "ObjectiveIndex",
                Flags = ScriptProperty.Flag.Edited,
                Data = index
            });

            var fragment = new QuestFragmentAlias
            {
                Version = 5,
                ObjectFormat = 2,
                Property = new ScriptObjectProperty
                {
                    Name = string.Empty,
                    Flags = ScriptProperty.Flag.Edited,
                    Object = new FormLink<ISkyrimMajorRecordGetter>(quest.FormKey),
                    Alias = checked((short)alias.ID),
                    Unused = 0
                }
            };
            fragment.Scripts.Add(script);
            adapter.Aliases.Add(fragment);
        }

        return adapter;
    }

    private static void Check(bool condition, string message, List<string> errors)
    {
        if (!condition)
        {
            errors.Add(message);
        }
    }

    private static int Report(IReadOnlyList<string> errors)
    {
        if (errors.Count == 0)
        {
            Console.WriteLine("PASS: Whereabouts.esp structure is valid.");
            return 0;
        }

        foreach (var error in errors)
        {
            Console.Error.WriteLine($"FAIL: {error}");
        }
        return 1;
    }
}
