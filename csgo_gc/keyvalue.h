#pragma once

class KeyValueParser;
class KeyValueStorage;

// not really the right place for this...
template<typename T>
inline T FromString(std::string_view string)
{
    T value;
    std::from_chars_result result = std::from_chars(string.data(), string.data() + string.size(), value);
    if (result.ec != std::errc{})
    {
        assert(false);
        return 0;
    }

    return value;
}

#if defined(__APPLE__)
// can't do from_chars with floats on macos...
template<>
inline float FromString(std::string_view string)
{
    std::string temp{ string }; // should fit the short string buffer
    return strtof(temp.c_str(), nullptr);
}
#endif

template<>
inline bool FromString(std::string_view string)
{
    return FromString<int>(string) ? true : false;
}

class KeyValue
{
public:
    KeyValue() = default;
    KeyValue(std::string_view name);

    void BinaryWriteToString(std::string &buffer) const;

    std::string_view Name() const { return m_name; }

    std::string_view String() const
    {
        const auto *result = std::get_if<std::string_view>(&m_value);
        return result ? *result : std::string_view{};
    }

    std::span<const KeyValue> Subkeys() const
    {
        const auto *result = std::get_if<std::vector<KeyValue>>(&m_value);
        return result ? *result : std::span<const KeyValue>{};
    }

    // parsing helpers
    const KeyValue *GetSubkey(std::string_view name) const;
    std::string_view GetString(std::string_view name, std::string_view fallback = {}) const;

    // template helpers for parsing/writing integers/floats

    template<typename T>
    T GetNumber(std::string_view name, T fallback = 0) const
    {
        const KeyValue *subkey = GetSubkey(name);
        if (!subkey)
        {
            return fallback;
        }

        if constexpr (std::is_enum<T>::value)
        {
            return static_cast<T>(FromString<typename std::underlying_type<T>::type>(subkey->String()));
        }
        else
        {
            return FromString<T>(subkey->String());
        }
    }

private:
    friend class KeyValueRead;

    bool ParseFromFile(const char *path, KeyValueStorage &storage);
    bool Parse(KeyValueParser &parser, std::string_view path, KeyValueStorage &storage);
    bool Dedupe(std::vector<size_t> &order, std::vector<bool> &removed);

    std::string_view m_name;
    std::variant<std::vector<KeyValue>, std::string_view> m_value;
};

// temp bullshit for migration
class KeyValueStorage
{
public:
    char* AllocateString(size_t length)
    {
        return m_strings.emplace_back(std::make_unique_for_overwrite<char[]>(length)).get();
    }

private:
    std::vector<std::unique_ptr<char[]>> m_strings;
};

class KeyValueRead : public NonCopyable
{
public:
    KeyValueRead(const char *path);

    bool Good() const { return m_good; }
    const KeyValue &Root() const { return m_root; }

    // hacks for the item schema!!!
    bool MergeToFromFile(const char *path);

private:
    KeyValueStorage m_storage;
    KeyValue m_root;
    bool m_good{};
};

class KeyValueWrite
{
public:
    KeyValueWrite(FILE *file);

    void BeginBlock(std::string_view name);
    void AddString(std::string_view name, std::string_view value);
    void EndBlock();

    template<typename T>
    void AddNumber(std::string_view name, T value)
    {
        AddString(name, std::to_string(value));
    }

private:
    void WriteText(std::string_view text);
    void WriteQuoted(std::string_view text);
    void WriteIndent();

    FILE *m_file{};
    int m_depth{};
};
