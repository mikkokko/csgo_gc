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

static bool IsSpace(char c)
{
    return (c == ' ') || ((unsigned int)c - '\t' < 5);
}

class KeyValueParser
{
public:
    KeyValueParser(char *ptr, char *end)
        : m_ptr{ ptr }
        , m_end{ end }
    {
    }

    // skips whitespace, returns false on eof
    [[nodiscard]] bool NextToken()
    {
    start:
        while (true)
        {
            if (m_ptr == m_end)
            {
                return false;
            }

            if (!IsSpace(*m_ptr))
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
            if (m_ptr == m_end)
            {
                return false;
            }

            if (*m_ptr == '\n' || *m_ptr == '\r')
            {
                break;
            }

            m_ptr++;
        }

        goto start;
    }

    std::string_view ParseString()
    {
        // unquoted
        if (*m_ptr != '"')
        {
            char *start = m_ptr;

            while (m_ptr != m_end && !IsSpace(*m_ptr) && *m_ptr != '{' && *m_ptr != '}')
            {
                m_ptr++;
            }

            return { start, static_cast<size_t>(m_ptr - start) };
        }

        m_ptr++; // skip the start quote

        char *start = m_ptr;
        char *end = start;

        // escape it in place
        while (m_ptr != m_end && *m_ptr != '"')
        {
            if (*m_ptr == '\\' && m_end - m_ptr >= 2 && (m_ptr[1] == '"' || m_ptr[1] == '\\'))
            {
                m_ptr++;
            }

            *end++ = *m_ptr++;
        }

        if (m_ptr != m_end)
        {
            m_ptr++;
        }

        return { start, static_cast<size_t>(end - start) };
    }

    char PeekCharacter() const
    {
        assert(m_ptr < m_end);
        return m_ptr[0];
    }

    void SkipCharacter()
    {
        assert(m_ptr < m_end);
        m_ptr++;
    }

private:
    char *m_ptr;
    char *m_end;
};

KeyValueRead::KeyValueRead(const char *path)
{
    std::vector<size_t> order;
    std::vector<bool> removed;
    m_good = m_root.ParseFromFile(path, m_storage) && m_root.Dedupe(order, removed);
}

KeyValue::KeyValue(std::string_view name)
    : m_name{ name }
{
}

static char *LoadFile(const char *path, KeyValueStorage &storage, size_t &size)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        return nullptr;
    }

    fseek(f, 0, SEEK_END);
    size = static_cast<size_t>(ftell(f));
    fseek(f, 0, SEEK_SET);

    char *buffer = storage.AllocateString(size);
    size_t bytesRead = fread(buffer, 1, size, f);

    fclose(f);

    if (bytesRead != size)
    {
        return nullptr;
    }

    return buffer;
}

bool KeyValue::ParseFromFile(const char *path, KeyValueStorage &storage)
{
    size_t fileSize;
    char *fileData = LoadFile(path, storage, fileSize);
    if (!fileData)
    {
        Platform::Print("Could not load {} (or empty)\n", path);
        return false;
    }

    KeyValueParser parser{ fileData, fileData + fileSize };
    if (!Parse(parser, path, storage))
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

void KeyValue::BinaryWriteToString(std::string &buffer) const
{
    for (const KeyValue &subkey : Subkeys())
    {
        if (subkey.String().size())
        {
            BinaryWriteCommand(buffer, BinaryCommand::String);
            BinaryWriteString(buffer, subkey.m_name);
            BinaryWriteString(buffer, subkey.String());
        }
        else if (subkey.Subkeys().size())
        {
            BinaryWriteCommand(buffer, BinaryCommand::Subkey);
            BinaryWriteString(buffer, subkey.m_name);
            subkey.BinaryWriteToString(buffer);
        }
    }

    BinaryWriteCommand(buffer, BinaryCommand::Terminate);
}

static std::string BuildIncludeFilePath(std::string_view currentPath, std::string_view filename)
{
    std::string result{ filename };

    size_t separator = currentPath.find_last_of("/\\");
    if (separator != std::string_view::npos)
    {
        result.insert(0, currentPath.substr(0, separator + 1));
    }

    return result;
}

bool KeyValue::Parse(KeyValueParser &parser, std::string_view path, KeyValueStorage &storage)
{
    if (!std::holds_alternative<std::vector<KeyValue>>(m_value))
    {
        m_value.emplace<std::vector<KeyValue>>();
    }

    auto &subkeys = std::get<std::vector<KeyValue>>(m_value);
    subkeys.reserve(SubkeyReserveCount);

    while (true)
    {
        if (!parser.NextToken())
        {
            return true;
        }

        if (parser.PeekCharacter() == '#')
        {
            if (parser.ParseString() != "#base")
            {
                return false;
            }

            if (!parser.NextToken() || parser.PeekCharacter() != '"')
            {
                return false;
            }

            std::string includedPath = BuildIncludeFilePath(path, parser.ParseString());
            if (!ParseFromFile(includedPath.c_str(), storage))
            {
                return false;
            }

            continue;
        }

        KeyValue *current;

        switch (parser.PeekCharacter())
        {
        case '"':
            current = &subkeys.emplace_back(parser.ParseString());
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
            current->m_value = parser.ParseString();
            break;

        case '{':
            parser.SkipCharacter();
            if (!current->Parse(parser, path, storage))
            {
                return false;
            }
            break;

        default:
            return false;
        }
    }
}

bool KeyValue::Dedupe(std::vector<size_t> &order, std::vector<bool> &removed)
{
    auto *children = std::get_if<std::vector<KeyValue>>(&m_value);
    if (!children)
    {
        return true;
    }

    auto &subkeys = *children;

    order.resize(subkeys.size());
    for (size_t i = 0; i < subkeys.size(); i++)
    {
        order[i] = i;
    }

    auto predicate = [subkeys](size_t a, size_t b)
    {
        auto compare = subkeys[a].m_name.compare(subkeys[b].m_name);
        return compare ? (compare < 0) : (a < b);
    };

    std::sort(order.begin(), order.end(), predicate);

    removed.clear();
    removed.resize(subkeys.size(), false);
    size_t groupStart = 0;

    for (size_t i = 1; i < order.size(); i++)
    {
        KeyValue &current = subkeys[order[i]];
        KeyValue &existing = subkeys[order[groupStart]];

        if (existing.m_name != current.m_name)
        {
            groupStart = i;
            continue;
        }

        if (existing.m_value.index() != current.m_value.index())
        {
            Platform::Print("Key named {} present with multiple types, can't merge\n", existing.m_name);
            return false;
        }

        auto *dst = std::get_if<std::vector<KeyValue>>(&existing.m_value);
        if (dst)
        {
            auto &src = std::get<std::vector<KeyValue>>(current.m_value);
            dst->insert(dst->end(),
                std::make_move_iterator(src.begin()),
                std::make_move_iterator(src.end()));
        }
        else
        {
            existing.m_value = std::move(current.m_value);
        }

        removed[order[i]] = true;
    }

    size_t write = 0;
    for (size_t read = 0; read < subkeys.size(); read++)
    {
        if (removed[read])
        {
            continue;
        }

        if (write != read)
        {
            subkeys[write] = std::move(subkeys[read]);
        }

        write++;
    }
    subkeys.resize(write);

    for (KeyValue &subkey : subkeys)
    {
        if (!subkey.Dedupe(order, removed))
        {
            return false;
        }
    }

    return true;
}

const KeyValue *KeyValue::GetSubkey(std::string_view name) const
{
    for (const KeyValue &subkey : Subkeys())
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

    return subkey->String();
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
