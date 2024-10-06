#pragma once

class KeyValue;

struct AttributeInfo
{
	bool storedAsInteger;
};

class ItemSchema
{
public:
	ItemSchema();

	bool AttributeStoredAsInteger(uint32_t defIndex) const
	{
		auto it = m_attributeInfo.find(defIndex);
		if (it != m_attributeInfo.end())
		{
			return it->second.storedAsInteger;
		}

		return false;
	}

	// mikkotodo parse
	uint32_t SealedGraffitiDefIndex() const { return 1348; }
	uint32_t GraffitiDefIndex() const { return 1349; }
	uint32_t GraffitiSlot() const { return 56; }

private:
	void ParseAttributes(const KeyValue *attributesKey);

	std::unordered_map<uint32_t, AttributeInfo> m_attributeInfo;
};
