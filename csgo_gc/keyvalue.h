#pragma once

class KeyValueParser;

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

class KeyValue
{
public:
    explicit KeyValue(std::string_view name);

    bool ParseFromFile(const char *path);
    bool WriteToFile(const char *path);

    void BinaryWriteToString(std::string &buffer);

    std::string_view Name() const { return m_name; }
    size_t SubkeyCount() const { return m_subkeys.size(); }
    std::string_view String() const { return m_string; }

    // range based for loops for subkeys
    const KeyValue *begin() const { return m_subkeys.data(); }
    const KeyValue *end() const { return m_subkeys.data() + m_subkeys.size(); }

    // parsing helpers
    const KeyValue *GetSubkey(std::string_view name) const;
    std::string_view GetString(std::string_view name, std::string_view fallback = {}) const;

    // writing helpers
    KeyValue &AddSubkey(std::string_view name);
    void AddString(std::string_view name, std::string_view value);

    // template helpers for parsing/writing integers/floats

    template<typename T>
    T GetNumber(std::string_view name, T fallback = 0) const
    {
        const KeyValue *subkey = GetSubkey(name);
        if (!subkey)
        {
            return fallback;
        }

        return FromString<T>(subkey->m_string);
    }

    template<typename T>
    void AddNumber(std::string_view name, T value)
    {
        KeyValue &subkey = AddSubkey(name);
        subkey.m_string = std::to_string(value);
    }

private:
    bool Parse(KeyValueParser &parser);
    KeyValue *FindOrCreateSubkey(std::string_view name);
    void WriteToFile(FILE *f, int indent);

    std::string m_name;

    // we could stick these into a variant and save a couple
    // of bytes but don't to keep things simple
    std::vector<KeyValue> m_subkeys;
    std::string m_string;
};
