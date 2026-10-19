#include "stdafx.h"
#include "keyvalue.h"

constexpr auto SubkeyReserveCount = 8;

// for writing binary keyvalues
enum class BinaryCommand : uint8_t
{
    Subkey,
    String,
    Int,
    Float,
    Ptr,
    Wstring,
    Color,
    Uint64,
    CompiledIntByte,
    CompiledInt0,
    CompiledInt1,
    Terminate
};

class KeyValueParser
{
public:
    KeyValueParser(std::string_view str)
        : m_ptr{ str.data() }
        , m_end{ str.data() + str.size() }
    {
    }

    // skips whitespace, returns false on eof
    [[nodiscard]] bool NextToken()
    {
    start:
        while (true)
        {
            if (IsEndOfFile())
            {
                return false;
            }

            if (static_cast<unsigned char>(*m_ptr) > ' ')
            {
                break;
            }

            m_ptr++;
        }

        if (m_ptr + 1 >= m_end || m_ptr[0] != '/' || m_ptr[1] != '/')
        {
            return true;
        }

        m_ptr += 2;

        while (true)
        {
            if (IsEndOfFile())
            {
                return false;
            }

            if (*m_ptr == '\n')
            {
                break;
            }

            m_ptr++;
        }

        goto start;
    }

    // FIXME: unfuck this
    std::string ParseUnquotedString()
    {
        std::string value;

        while (!IsEndOfFile())
        {
            char ch = *m_ptr++;
            if (static_cast<unsigned char>(ch) <= ' ')
            {
                break;
            }

            value.push_back(ch);
        }

        return value;
    }

    std::string ParseString()
    {
        if (*m_ptr != '"')
        {
            return ParseUnquotedString();
        }

        m_ptr++; // skip the start quote

        std::string value;

        while (!IsEndOfFile())
        {
            char ch = *m_ptr++;
            if (ch == '"')
            {
                break;
            }

            if (ch == '\\' && !IsEndOfFile() && (*m_ptr == '"' || *m_ptr == '\\'))
            {
                ch = *m_ptr++;
            }

            value.push_back(ch);
        }

        return value;
    }

    char PeekCharacter() const
    {
        assert(!IsEndOfFile());
        return m_ptr[0];
    }

    void SkipCharacter()
    {
        assert(!IsEndOfFile());
        m_ptr++;
    }

private:
    bool IsEndOfFile() const { return m_ptr >= m_end; }

    const char *m_ptr;
    const char *m_end;
};

std::string LoadFile(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        return {};
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::string buffer;
    buffer.resize(size);
    long bytesRead = static_cast<long>(fread(buffer.data(), 1, size, f));

    fclose(f);

    if (bytesRead != size)
    {
        return {};
    }

    return buffer;
}

KeyValue::KeyValue(std::string_view name)
    : m_name{ name }
{
}

bool KeyValue::ParseFromFile(const char *path)
{
    std::string data = LoadFile(path);
    if (data.empty())
    {
        Platform::Print("Could not load {} (or empty)\n", path);
        return false;
    }

    KeyValueParser parser{ data };
    if (!Parse(parser, path))
    {
        Platform::Print("Could not parse {}\n", path);
        return false;
    }

    return true;
}

static void BinaryWriteCommand(std::string &buffer, BinaryCommand cmd)
{
    buffer.append(1, static_cast<char>(cmd));
}

static void BinaryWriteString(std::string &buffer, std::string_view string)
{
    buffer.append(string);
    buffer.append(1, '\0');
}

void KeyValue::BinaryWriteToString(std::string &buffer)
{
    for (KeyValue &subkey : m_subkeys)
    {
        if (subkey.m_string.empty() && subkey.m_subkeys.empty())
        {
            continue;
        }

        if (subkey.m_string.size())
        {
            BinaryWriteCommand(buffer, BinaryCommand::String);
            BinaryWriteString(buffer, subkey.m_name);
            BinaryWriteString(buffer, subkey.m_string);
        }
        else
        {
            BinaryWriteCommand(buffer, BinaryCommand::Subkey);
            BinaryWriteString(buffer, subkey.m_name);
            subkey.BinaryWriteToString(buffer);
        }
    }

    BinaryWriteCommand(buffer, BinaryCommand::Terminate);
}

static std::string BuildIncludeFilePath(std::string_view currentPath, std::string filename)
{
    size_t separator = currentPath.find_last_of("/\\");
    if (separator != std::string_view::npos)
    {
        filename.insert(0, currentPath.substr(0, separator + 1));
    }

    return filename;
}

bool KeyValue::Parse(KeyValueParser &parser, std::string_view path)
{
    m_subkeys.reserve(SubkeyReserveCount);

    while (true)
    {
        if (!parser.NextToken())
        {
            return true;
        }

        if (parser.PeekCharacter() == '#')
        {
            std::string directive = parser.ParseString();
            if (directive != "#base")
            {
                return false;
            }

            if (!parser.NextToken() || parser.PeekCharacter() != '"')
            {
                return false;
            }

            std::string includedPath = BuildIncludeFilePath(path, parser.ParseString());
            if (!ParseFromFile(includedPath.c_str()))
            {
                return false;
            }

            continue;
        }

        KeyValue *current;

        switch (parser.PeekCharacter())
        {
        case '"':
            current = FindOrCreateSubkey(parser.ParseString());
            break;

        case '}':
            parser.SkipCharacter();
            return true;

        default:
            return false;
        }

        if (!parser.NextToken())
        {
            return false;
        }

        switch (parser.PeekCharacter())
        {
        case '"':
            current->m_string = parser.ParseString();
            break;

        case '{':
            parser.SkipCharacter();
            if (!current->Parse(parser, path))
            {
                return false;
            }
            break;

        default:
            return false;
        }
    }
}

KeyValue *KeyValue::FindOrCreateSubkey(std::string_view name)
{
    for (KeyValue &subkey : m_subkeys)
    {
        if (subkey.m_name == name)
        {
            return &subkey;
        }
    }

    return &m_subkeys.emplace_back(name);
}

const KeyValue *KeyValue::GetSubkey(std::string_view name) const
{
    for (const KeyValue &subkey : m_subkeys)
    {
        if (subkey.m_name == name)
        {
            return &subkey;
        }
    }

    return nullptr;
}

std::string_view KeyValue::GetString(std::string_view name, std::string_view fallback) const
{
    const KeyValue *subkey = GetSubkey(name);
    if (!subkey)
    {
        return fallback;
    }

    return subkey->m_string;
}

KeyValueWrite::KeyValueWrite(FILE *file)
    : m_file{ file }
{
}

void KeyValueWrite::WriteText(std::string_view text)
{
    fwrite(text.data(), 1, text.size(), m_file);
}

void KeyValueWrite::WriteQuoted(std::string_view text)
{
    WriteText("\"");

    std::size_t start = 0;

    for (size_t i = text.find_first_of("\"\\"); i != std::string_view::npos; i = text.find_first_of("\"\\", i + 1))
    {
        WriteText(text.substr(start, i - start));
        WriteText("\\");
        start = i;
    }

    WriteText(text.substr(start));
    WriteText("\"");
}

void KeyValueWrite::WriteIndent()
{
    for (int i = 0; i < m_depth; i++)
    {
        WriteText("\t");
    }
}

void KeyValueWrite::BeginBlock(std::string_view name)
{
    WriteIndent();
    WriteQuoted(name);
    WriteText("\n");
    WriteIndent();
    WriteText("{\n");
    m_depth++;
}

void KeyValueWrite::AddString(std::string_view name, std::string_view value)
{
    WriteIndent();
    WriteQuoted(name);
    WriteText("\t\t");
    WriteQuoted(value);
    WriteText("\n");
}

void KeyValueWrite::EndBlock()
{
    m_depth--;
    WriteIndent();
    WriteText("}\n");
}
