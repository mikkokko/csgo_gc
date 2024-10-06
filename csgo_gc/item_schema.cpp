#include "stdafx.h"
#include "item_schema.h"
#include "keyvalue.h"

ItemSchema::ItemSchema()
{
	KeyValue itemSchema{ "root" };
	if (!itemSchema.ParseFromFile("csgo/scripts/items/items_game.txt"))
	{
		assert(false);
		return;
	}

	const KeyValue *itemsGame = itemSchema.GetSubkey("items_game");
	if (!itemsGame)
	{
		assert(false);
		return;
	}

	const KeyValue *attributesKey = itemsGame->GetSubkey("attributes");
	if (attributesKey)
	{
		ParseAttributes(attributesKey);
	}
}

void ItemSchema::ParseAttributes(const KeyValue *attributesKey)
{
	for (const KeyValue &attributeKey : *attributesKey)
	{
		uint32_t defIndex = FromString<uint32_t>(attributeKey.Name());
		assert(defIndex);

		AttributeInfo info{};
		info.storedAsInteger = attributeKey.GetNumber<int>("stored_as_integer") ? true : false;

		m_attributeInfo.insert(std::make_pair(defIndex, info));
	}
}
