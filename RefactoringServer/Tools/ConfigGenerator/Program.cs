using System.Text;
using YamlDotNet.Serialization;
using YamlDotNet.Serialization.NamingConventions;

internal static class Program
{
    public static int Main(string[] args)
    {
        try
        {
            string solutionRoot = FindSolutionRoot();
            string schemaRoot = Path.Combine(solutionRoot, "ConfigSchema");
            string outputRoot = Path.Combine(solutionRoot, "Generated", "Config");

            ParseArguments(args, ref schemaRoot, ref outputRoot);

            if (!Directory.Exists(schemaRoot))
            {
                throw new InvalidOperationException($"Schema root not found: {schemaRoot}");
            }

            var deserializer = new DeserializerBuilder()
                .WithNamingConvention(HyphenatedNamingConvention.Instance)
                .IgnoreUnmatchedProperties()
                .Build();

            string[] schemaFiles = Directory.GetFiles(schemaRoot, "*.schema.yaml", SearchOption.AllDirectories);
            if (schemaFiles.Length == 0)
            {
                throw new InvalidOperationException($"No config schema files found in: {schemaRoot}");
            }

            var documents = new List<ConfigSchemaDocument>();
            foreach (string schemaFile in schemaFiles)
            {
                string yamlText = File.ReadAllText(schemaFile);
                Dictionary<string, Dictionary<string, ConfigSchemaField>> rawSections =
                    deserializer.Deserialize<Dictionary<string, Dictionary<string, ConfigSchemaField>>>(yamlText)
                    ?? throw new InvalidOperationException($"Failed to deserialize config schema: {schemaFile}");

                ConfigSchemaDocument document = ConfigSchemaNormalizer.Normalize(rawSections, schemaFile);
                ConfigSchemaValidator.Validate(document, schemaFile);
                documents.Add(document);
            }

            ConfigSchemaSetValidator.Validate(documents);

            foreach (ConfigSchemaDocument document in documents)
            {
                string targetOutputDirectory = Path.Combine(outputRoot, document.Target);
                Directory.CreateDirectory(targetOutputDirectory);

                string headerPath = Path.Combine(targetOutputDirectory, $"{document.Target}Config.h");
                string cppPath = Path.Combine(targetOutputDirectory, $"{document.Target}Config.cpp");

                File.WriteAllText(headerPath, CppConfigGenerator.GenerateHeader(document), new UTF8Encoding(false));
                File.WriteAllText(cppPath, CppConfigGenerator.GenerateCpp(document), new UTF8Encoding(false));

                Console.WriteLine($"Generated: {headerPath}");
                Console.WriteLine($"Generated: {cppPath}");
            }

            return 0;
        }
        catch (Exception exception)
        {
            Console.Error.WriteLine($"ConfigGenerator failed: {exception.Message}");
            return 1;
        }
    }

    private static void ParseArguments(string[] args, ref string schemaRoot, ref string outputRoot)
    {
        for (int index = 0; index < args.Length; ++index)
        {
            string argument = args[index];
            if (argument == "--schema-root" && index + 1 < args.Length)
            {
                schemaRoot = Path.GetFullPath(args[++index]);
            }
            else if (argument == "--output-root" && index + 1 < args.Length)
            {
                outputRoot = Path.GetFullPath(args[++index]);
            }
            else
            {
                throw new InvalidOperationException($"Unknown argument: {argument}");
            }
        }
    }

    private static string FindSolutionRoot()
    {
        DirectoryInfo? directory = new DirectoryInfo(AppContext.BaseDirectory);
        while (directory != null)
        {
            if (File.Exists(Path.Combine(directory.FullName, "RefactoringServer.sln")))
            {
                return directory.FullName;
            }

            directory = directory.Parent;
        }

        return Directory.GetCurrentDirectory();
    }
}

internal static class ConfigSchemaSetValidator
{
    public static void Validate(IReadOnlyList<ConfigSchemaDocument> documents)
    {
        var usedTargets = new HashSet<string>(StringComparer.Ordinal);
        foreach (ConfigSchemaDocument document in documents)
        {
            if (!usedTargets.Add(document.Target))
            {
                throw new InvalidOperationException($"Duplicate config target: {document.Target}");
            }
        }
    }
}

internal sealed class ConfigSchemaDocument
{
    public string Target { get; init; } = string.Empty;

    public List<ConfigSchemaSection> Sections { get; init; } = [];

    public string RootClassName => ConfigSchemaNaming.BuildRootClassName(Target);
}

internal sealed class ConfigSchemaSection
{
    public string Name { get; init; } = string.Empty;

    public string ClassName { get; init; } = string.Empty;

    public List<ConfigSchemaField> Fields { get; init; } = [];
}

internal sealed class ConfigSchemaField
{
    public string Name { get; set; } = string.Empty;

    public string Type { get; set; } = string.Empty;

    public string? Default { get; set; }

    public bool Required { get; set; }

    public string? Description { get; set; }
}

internal static class ConfigSchemaNormalizer
{
    public static ConfigSchemaDocument Normalize(Dictionary<string, Dictionary<string, ConfigSchemaField>> rawSections, string schemaPath)
    {
        string target = ExtractTargetFromSchemaPath(schemaPath);
        if (rawSections.Count == 0)
        {
            throw new InvalidOperationException($"Config schema has no sections: {schemaPath}");
        }

        var sections = new List<ConfigSchemaSection>(rawSections.Count);
        foreach ((string sectionName, Dictionary<string, ConfigSchemaField> rawFields) in rawSections)
        {
            if (string.IsNullOrWhiteSpace(sectionName))
            {
                throw new InvalidOperationException($"Config schema section name is empty: {schemaPath}");
            }

            if (rawFields.Count == 0)
            {
                throw new InvalidOperationException($"Config schema section '{sectionName}' has no fields: {schemaPath}");
            }

            var fields = new List<ConfigSchemaField>(rawFields.Count);
            foreach ((string fieldName, ConfigSchemaField rawField) in rawFields)
            {
                if (rawField == null)
                {
                    throw new InvalidOperationException($"Config schema field '{sectionName}.{fieldName}' is null: {schemaPath}");
                }

                fields.Add(new ConfigSchemaField
                {
                    Name = fieldName,
                    Type = rawField.Type,
                    Default = rawField.Default,
                    Required = rawField.Required,
                    Description = rawField.Description
                });
            }

            sections.Add(new ConfigSchemaSection
            {
                Name = sectionName,
                ClassName = ConfigSchemaNaming.BuildSectionClassName(target, sectionName),
                Fields = fields
            });
        }

        return new ConfigSchemaDocument
        {
            Target = target,
            Sections = sections
        };
    }

    private static string ExtractTargetFromSchemaPath(string schemaPath)
    {
        string fileName = Path.GetFileName(schemaPath);
        const string kSchemaSuffix = ".schema.yaml";

        if (!fileName.EndsWith(kSchemaSuffix, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException($"Config schema file name must end with '{kSchemaSuffix}': {schemaPath}");
        }

        string target = fileName[..^kSchemaSuffix.Length];
        if (string.IsNullOrWhiteSpace(target))
        {
            throw new InvalidOperationException($"Config schema target could not be inferred from file name: {schemaPath}");
        }

        return target;
    }
}

internal static class ConfigSchemaNaming
{
    public static string BuildRootClassName(string target)
    {
        return $"F{NormalizeIdentifier(target)}ConfigDocument";
    }

    public static string BuildSectionClassName(string target, string sectionName)
    {
        string targetIdentifier = NormalizeIdentifier(target);
        string sectionIdentifier = NormalizeIdentifier(sectionName);

        if (string.Equals(targetIdentifier, sectionIdentifier, StringComparison.Ordinal))
        {
            return $"S{targetIdentifier}Config";
        }

        return $"S{targetIdentifier}{sectionIdentifier}Config";
    }

    private static string NormalizeIdentifier(string text)
    {
        var builder = new StringBuilder(text.Length);
        bool capitalizeNext = true;
        foreach (char character in text)
        {
            if (char.IsLetterOrDigit(character))
            {
                builder.Append(capitalizeNext ? char.ToUpperInvariant(character) : character);
                capitalizeNext = false;
            }
            else
            {
                capitalizeNext = true;
            }
        }

        if (builder.Length == 0)
        {
            throw new InvalidOperationException($"Invalid identifier text: {text}");
        }

        return builder.ToString();
    }
}

internal static class ConfigSchemaValidator
{
    public static void Validate(ConfigSchemaDocument document, string schemaPath)
    {
        if (string.IsNullOrWhiteSpace(document.Target))
        {
            throw new InvalidOperationException($"Config schema target could not be inferred: {schemaPath}");
        }

        if (document.Sections.Count == 0)
        {
            throw new InvalidOperationException($"Config schema has no sections: {schemaPath}");
        }

        var usedSectionNames = new HashSet<string>(StringComparer.Ordinal);
        foreach (ConfigSchemaSection section in document.Sections)
        {
            if (string.IsNullOrWhiteSpace(section.Name))
            {
                throw new InvalidOperationException($"Config schema section name is empty: {schemaPath}");
            }

            if (string.IsNullOrWhiteSpace(section.ClassName))
            {
                throw new InvalidOperationException($"Config schema section class is empty: {schemaPath}");
            }

            if (!usedSectionNames.Add(section.Name))
            {
                throw new InvalidOperationException($"Duplicate config section '{section.Name}': {schemaPath}");
            }

            var usedFieldNames = new HashSet<string>(StringComparer.Ordinal);
            foreach (ConfigSchemaField field in section.Fields)
            {
                if (string.IsNullOrWhiteSpace(field.Name))
                {
                    throw new InvalidOperationException($"Config schema field name is empty: {schemaPath}");
                }

                if (string.IsNullOrWhiteSpace(field.Type))
                {
                    throw new InvalidOperationException($"Config schema field type is empty: {schemaPath}");
                }

                if (!usedFieldNames.Add(field.Name))
                {
                    throw new InvalidOperationException($"Duplicate config field '{section.Name}.{field.Name}': {schemaPath}");
                }

                _ = ConfigTypeMapping.RenderCppType(field.Type);
                _ = ConfigTypeMapping.GetReaderFunctionName(field.Type, field.Required);
                _ = ConfigTypeMapping.RenderMemberInitializer(field.Type, field.Default);
            }
        }
    }
}

internal static class ConfigTypeMapping
{
    public static string RenderCppType(string schemaType)
    {
        return schemaType switch
        {
            "bool" => "bool",
            "int32" => "std::int32_t",
            "uint16" => "std::uint16_t",
            "uint32" => "std::uint32_t",
            "int64" => "std::int64_t",
            "uint64" => "std::uint64_t",
            "float" => "float",
            "double" => "double",
            "string" => "std::string",
            _ => throw new InvalidOperationException($"Unsupported config field type: {schemaType}")
        };
    }

    public static string GetReaderFunctionName(string schemaType, bool required)
    {
        string prefix = required ? "ReadRequired" : "ReadOptional";
        return schemaType switch
        {
            "bool" => prefix + "Bool",
            "int32" => prefix + "Int32",
            "uint16" => prefix + "UInt16",
            "uint32" => prefix + "UInt32",
            "int64" => prefix + "Int64",
            "uint64" => prefix + "UInt64",
            "float" => prefix + "Float",
            "double" => prefix + "Double",
            "string" => prefix + "String",
            _ => throw new InvalidOperationException($"Unsupported config field type: {schemaType}")
        };
    }

    public static string? RenderMemberInitializer(string schemaType, string? defaultText)
    {
        if (defaultText == null)
        {
            return null;
        }

        return schemaType switch
        {
            "bool" => RenderBoolLiteral(defaultText),
            "int32" => $"static_cast<std::int32_t>({defaultText})",
            "uint16" => $"static_cast<std::uint16_t>({defaultText})",
            "uint32" => $"static_cast<std::uint32_t>({defaultText})",
            "int64" => $"static_cast<std::int64_t>({defaultText})",
            "uint64" => $"static_cast<std::uint64_t>({defaultText})",
            "float" => defaultText,
            "double" => defaultText,
            "string" => $"\"{EscapeCppString(defaultText)}\"",
            _ => throw new InvalidOperationException($"Unsupported config field type: {schemaType}")
        };
    }

    private static string RenderBoolLiteral(string text)
    {
        if (bool.TryParse(text, out bool parsedValue))
        {
            return parsedValue ? "true" : "false";
        }

        string lowerText = text.Trim().ToLowerInvariant();
        return lowerText switch
        {
            "1" or "yes" or "on" => "true",
            "0" or "no" or "off" => "false",
            _ => throw new InvalidOperationException($"Invalid bool default: {text}")
        };
    }

    private static string EscapeCppString(string text)
    {
        return text
            .Replace("\\", "\\\\")
            .Replace("\"", "\\\"")
            .Replace("\r", "\\r")
            .Replace("\n", "\\n")
            .Replace("\t", "\\t");
    }
}

internal static class CppConfigGenerator
{
    public static string GenerateHeader(ConfigSchemaDocument document)
    {
        var builder = new StringBuilder();
        builder.AppendLine("#pragma once");
        builder.AppendLine();
        builder.AppendLine("#include <cstdint>");
        builder.AppendLine("#include <filesystem>");
        builder.AppendLine("#include <string>");
        builder.AppendLine();
        builder.AppendLine($"namespace Generated::Config::{document.Target}");
        builder.AppendLine("{");

        foreach (ConfigSchemaSection section in document.Sections)
        {
            builder.AppendLine($"\tstruct {section.ClassName}");
            builder.AppendLine("\t{");
            foreach (ConfigSchemaField field in section.Fields)
            {
                string cppType = ConfigTypeMapping.RenderCppType(field.Type);
                string? initializer = ConfigTypeMapping.RenderMemberInitializer(field.Type, field.Default);
                if (!string.IsNullOrWhiteSpace(field.Description))
                {
                    builder.AppendLine($"\t\t// {field.Description}");
                }

                if (initializer != null)
                {
                    builder.AppendLine($"\t\t{cppType} {field.Name} = {initializer};");
                }
                else
                {
                    builder.AppendLine($"\t\t{cppType} {field.Name}{{}};");
                }
            }

            builder.AppendLine("\t};");
            builder.AppendLine();
        }

        builder.AppendLine($"\tstruct {document.RootClassName}");
        builder.AppendLine("\t{");
        foreach (ConfigSchemaSection section in document.Sections)
        {
            builder.AppendLine($"\t\t{section.ClassName} {section.Name};");
        }
        builder.AppendLine("\t};");
        builder.AppendLine();
        builder.AppendLine($"\tclass F{document.Target}ConfigLoader");
        builder.AppendLine("\t{");
        builder.AppendLine("\tpublic:");
        builder.AppendLine($"\t\tstatic bool LoadFromFile(const std::filesystem::path& filePath, {document.RootClassName}& outConfig, std::string& outError);");
        builder.AppendLine("\t};");
        builder.AppendLine("}");
        return builder.ToString();
    }

    public static string GenerateCpp(ConfigSchemaDocument document)
    {
        var builder = new StringBuilder();
        builder.AppendLine("#include \"Pch.h\"");
        builder.AppendLine();
        builder.AppendLine($"#include \"Generated/Config/{document.Target}/{document.Target}Config.h\"");
        builder.AppendLine("#include \"Foundation/Config/FConfigFileLoader.h\"");
        builder.AppendLine("#include \"Foundation/Config/FConfigValueReader.h\"");
        builder.AppendLine();
        builder.AppendLine("#include <array>");
        builder.AppendLine("#include <string_view>");
        builder.AppendLine();
        builder.AppendLine($"namespace Generated::Config::{document.Target}");
        builder.AppendLine("{");
        builder.AppendLine($"\tbool F{document.Target}ConfigLoader::LoadFromFile(const std::filesystem::path& filePath, {document.RootClassName}& outConfig, std::string& outError)");
        builder.AppendLine("\t{");
        builder.AppendLine("\t\tFoundation::Config::SConfigDocument document{};");
        builder.AppendLine("\t\tif (!Foundation::Config::FConfigFileLoader::LoadYamlFile(filePath, document, outError))");
        builder.AppendLine("\t\t{");
        builder.AppendLine("\t\t\treturn false;");
        builder.AppendLine("\t\t}");
        builder.AppendLine();
        builder.AppendLine("\t\tFoundation::Config::FConfigValueReader reader(document);");
        builder.AppendLine();
        builder.AppendLine($"\t\tconstexpr std::array<std::string_view, {document.Sections.Count}> kKnownSections =");
        builder.AppendLine("\t\t{");
        for (int sectionIndex = 0; sectionIndex < document.Sections.Count; ++sectionIndex)
        {
            string suffix = sectionIndex + 1 == document.Sections.Count ? string.Empty : ",";
            builder.AppendLine($"\t\t\t\"{document.Sections[sectionIndex].Name}\"{suffix}");
        }
        builder.AppendLine("\t\t};");
        builder.AppendLine();
        builder.AppendLine("\t\tif (!reader.ValidateKnownSections(kKnownSections, outError))");
        builder.AppendLine("\t\t{");
        builder.AppendLine("\t\t\treturn false;");
        builder.AppendLine("\t\t}");
        builder.AppendLine();

        foreach (ConfigSchemaSection section in document.Sections)
        {
            builder.AppendLine($"\t\tconstexpr std::array<std::string_view, {section.Fields.Count}> k{section.Name}KnownKeys =");
            builder.AppendLine("\t\t{");
            for (int fieldIndex = 0; fieldIndex < section.Fields.Count; ++fieldIndex)
            {
                string suffix = fieldIndex + 1 == section.Fields.Count ? string.Empty : ",";
                builder.AppendLine($"\t\t\t\"{section.Fields[fieldIndex].Name}\"{suffix}");
            }
            builder.AppendLine("\t\t};");
            builder.AppendLine();
            builder.AppendLine($"\t\tif (!reader.ValidateKnownKeys(\"{section.Name}\", k{section.Name}KnownKeys, outError))");
            builder.AppendLine("\t\t{");
            builder.AppendLine("\t\t\treturn false;");
            builder.AppendLine("\t\t}");
            builder.AppendLine();
        }

        foreach (ConfigSchemaSection section in document.Sections)
        {
            foreach (ConfigSchemaField field in section.Fields)
            {
                string readerFunctionName = ConfigTypeMapping.GetReaderFunctionName(field.Type, field.Required);
                builder.AppendLine($"\t\tif (!reader.{readerFunctionName}(\"{section.Name}\", \"{field.Name}\", outConfig.{section.Name}.{field.Name}, outError))");
                builder.AppendLine("\t\t{");
                builder.AppendLine("\t\t\treturn false;");
                builder.AppendLine("\t\t}");
                builder.AppendLine();
            }
        }

        builder.AppendLine("\t\treturn true;");
        builder.AppendLine("\t}");
        builder.AppendLine("}");
        return builder.ToString();
    }
}
