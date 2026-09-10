using System.Collections;
using System.Reflection;
using System.Text.Json;
using Mutagen.Bethesda.Skyrim;

if (args.Length == 2 && string.Equals(args[0], "build", StringComparison.OrdinalIgnoreCase))
{
    return PluginCommands.Build(Path.GetFullPath(args[1]));
}

if (args.Length == 2 && string.Equals(args[0], "validate", StringComparison.OrdinalIgnoreCase))
{
    return PluginCommands.Validate(Path.GetFullPath(args[1]));
}

if (args.Length == 1 && string.Equals(args[0], "types", StringComparison.OrdinalIgnoreCase))
{
    DumpType(typeof(Quest));
    DumpType(typeof(QuestStage));
    foreach (var entryType in typeof(QuestStage).GetProperty("LogEntries")!.PropertyType.GetGenericArguments())
    {
        DumpType(entryType);
    }
    DumpType(typeof(SkyrimMod));
    DumpType(typeof(SkyrimModHeader));
    DumpType(typeof(ModStats));
    DumpType(typeof(Mutagen.Bethesda.Strings.TranslatedString));
    DumpType(typeof(Mutagen.Bethesda.Plugins.Records.KeyedMasterStyle));
    DumpEnum(typeof(Mutagen.Bethesda.Plugins.MasterStyle));
    DumpType(typeof(QuestAlias));
    DumpType(typeof(QuestObjective));
    DumpType(typeof(QuestObjectiveTarget));
    DumpType(typeof(QuestAdapter));
    DumpType(typeof(QuestFragmentAlias));
    DumpType(typeof(ScriptEntry));
    DumpType(typeof(ScriptIntProperty));
    DumpType(typeof(ScriptObjectProperty));
    DumpEnum(typeof(Quest.Flag));
    DumpEnum(typeof(QuestAlias.Flag));
    DumpEnum(typeof(ScriptEntry.Flag));
    DumpEnum(typeof(ScriptProperty.Flag));
    return 0;
}

if (args.Length != 2 ||
    (!string.Equals(args[0], "inspect", StringComparison.OrdinalIgnoreCase) &&
     !string.Equals(args[0], "inspect-vmad", StringComparison.OrdinalIgnoreCase)))
{
    Console.Error.WriteLine("Usage: Whereabouts.PluginBuilder build <output-path> | <inspect|inspect-vmad|validate> <plugin-path>");
    return 2;
}

var pluginPath = Path.GetFullPath(args[1]);
if (!File.Exists(pluginPath))
{
    Console.Error.WriteLine($"Plugin not found: {pluginPath}");
    return 2;
}

using var mod = SkyrimMod.CreateFromBinaryOverlay(pluginPath, SkyrimRelease.SkyrimSE);
if (string.Equals(args[0], "inspect-vmad", StringComparison.OrdinalIgnoreCase))
{
    var sourceQuest = mod.Quests.Single();
    var adapter = sourceQuest.VirtualMachineAdapter ?? throw new InvalidDataException("Quest has no VMAD adapter.");
    Console.WriteLine(JsonSerializer.Serialize(new
    {
        QuestScripts = adapter.Scripts.Select(script => DescribeProperties(script, 4)).ToArray(),
        AliasFragments = adapter.Aliases.Take(2).Select(alias => DescribeProperties(alias, 4)).ToArray()
    }, new JsonSerializerOptions { WriteIndented = true }));
    return 0;
}

var quests = mod.Quests.Select(quest => new
{
    FormKey = quest.FormKey.ToString(),
    quest.EditorID,
    RecordFlags = quest.MajorRecordFlagsRaw,
    Properties = DescribeProperties(quest, 2)
}).ToArray();

Console.WriteLine(JsonSerializer.Serialize(new
{
    Plugin = pluginPath,
    ModKey = mod.ModKey.ToString(),
    Quests = quests
}, new JsonSerializerOptions { WriteIndented = true }));

return 0;

static void DumpType(Type type)
{
    Console.WriteLine($"TYPE {type.FullName}");
    foreach (var constructor in type.GetConstructors())
    {
        Console.WriteLine($"  CTOR {constructor}");
        foreach (var parameter in constructor.GetParameters())
        {
            Console.WriteLine($"    ARG {parameter.ParameterType.FullName} {parameter.Name} default={parameter.DefaultValue}");
        }
    }
    foreach (var property in type.GetProperties(BindingFlags.Instance | BindingFlags.Public))
    {
        Console.WriteLine($"  PROP {(property.CanWrite ? "RW" : "RO")} {property.PropertyType.FullName} {property.Name}");
    }
}

static void DumpEnum(Type type)
{
    Console.WriteLine($"ENUM {type.FullName}");
    foreach (var value in Enum.GetValues(type))
    {
        Console.WriteLine($"  {value}={Convert.ToUInt64(value)}");
    }
}

static SortedDictionary<string, object?> DescribeProperties(object value, int depth)
{
    var output = new SortedDictionary<string, object?>(StringComparer.Ordinal);
    foreach (var property in value.GetType().GetProperties(BindingFlags.Instance | BindingFlags.Public))
    {
        if (property.GetIndexParameters().Length != 0 || !property.CanRead)
        {
            continue;
        }

        object? propertyValue;
        try
        {
            propertyValue = property.GetValue(value);
        }
        catch (Exception exception)
        {
            output[property.Name] = $"<{exception.GetType().Name}>";
            continue;
        }

        output[property.Name] = DescribeValue(propertyValue, depth);
    }
    return output;
}

static object? DescribeValue(object? value, int depth)
{
    if (value is null)
    {
        return null;
    }

    var type = value.GetType();
    if (value is string || type.IsPrimitive || type.IsEnum || value is decimal)
    {
        return value.ToString();
    }

    if (depth <= 0)
    {
        return value.ToString();
    }

    if (value is IEnumerable items)
    {
        var output = new List<object?>();
        foreach (var item in items)
        {
            output.Add(item is null ? null : new
            {
                Type = item.GetType().FullName,
                Value = DescribeProperties(item, depth - 1)
            });
        }
        return output;
    }

    return new
    {
        Type = type.FullName,
        Value = DescribeProperties(value, depth - 1)
    };
}
