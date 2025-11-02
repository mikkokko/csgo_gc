# Tutorial: How to Give Yourself Items, Skins, Agents, and More

> **Quick thanks to Parker!**
>
> * csgo gc stuff Pastebin: [https://pastebin.com/icfAsgRw](https://pastebin.com/icfAsgRw)
> * YouTube Tutorial: [https://www.youtube.com/watch?v=6TrCRjWmBoQ](https://www.youtube.com/watch?v=6TrCRjWmBoQ)

---

## 📘 Basic Item Template

```
// Example item entry
"4"
{
    "inventory"        "3" 
    "def_index"        "60" // Weapon ID, Pin ID, etc.
    "level"            "1"
    "quality"          "5" // Quality ID
    "flags"            "0"
    "origin"           "8"
    "in_use"           "0"
    "rarity"           "1" // Rarity ID

    "attributes" // Used for skins (omit if it's a pin, sticker, etc.)
    {
        "6"  "282.000000" // Finish Catalog
        "7"  "0.000000"   // Pattern Template
        "8"  "0.100000"   // Float Value
    }
}
```

---

## 🧩 Quality IDs

| ID | Quality Name | Notes        |
| -- | ------------ | ------------ |
| 0  | Normal       |              |
| 1  | Genuine      |              |
| 2  | Vintage      |              |
| 3  | Unusual      |              |
| 4  | Unique       |              |
| 5  | Community    |              |
| 6  | Developer    | (Valve Only) |
| 7  | Selfmade     |              |
| 8  | Customized   |              |
| 9  | Strange      |              |
| 10 | Completed    |              |
| 11 | Haunted      |              |
| 12 | Tournament   | (Souvenir)   |

---

## 💎 Rarity IDs

| ID | Rarity Name      |
| -- | ---------------- |
| 0  | Default          |
| 1  | Consumer Grade   |
| 2  | Industrial Grade |
| 3  | Mil-Spec Grade   |
| 4  | Restricted       |
| 5  | Classified       |
| 6  | Covert           |
| 7  | Contraband       |
| 99 | Unusual          |

---

## 🔫 Weapon IDs

Below are common weapon IDs. Use these in `"def_index"`.

| ID | Weapon               |   | ID | Weapon              |
| -- | -------------------- | - | -- | ------------------- |
| 1  | weapon_deagle        |   | 9  | weapon_awp          |
| 7  | weapon_ak47          |   | 16 | weapon_m4a1         |
| 19 | weapon_p90           |   | 24 | weapon_ump45        |
| 25 | weapon_xm1014        |   | 27 | weapon_mag7         |
| 29 | weapon_sawedoff      |   | 30 | weapon_tec9         |
| 32 | weapon_hkp2000       |   | 35 | weapon_nova         |
| 36 | weapon_p250          |   | 38 | weapon_scar20       |
| 39 | weapon_sg556         |   | 40 | weapon_ssg08        |
| 42 | weapon_knife         |   | 43 | weapon_flashbang    |
| 44 | weapon_hegrenade     |   | 49 | weapon_c4           |
| 60 | weapon_m4a1_silencer |   | 61 | weapon_usp_silencer |
| 63 | weapon_cz75a         |   | 64 | weapon_revolver     |

**Knives**

| ID  | Knife Type          |
| --- | ------------------- |
| 500 | Bayonet             |
| 505 | Flip Knife          |
| 506 | Gut Knife           |
| 507 | Karambit            |
| 508 | M9 Bayonet          |
| 509 | Huntsman Knife      |
| 512 | Falchion Knife      |
| 515 | Butterfly Knife     |
| 516 | Shadow Daggers      |
| 523 | Widowmaker (Navaja) |
| 525 | Skeleton Knife      |

(You can find full lists of IDs online if needed.)

---

## 🧤 Glove IDs

| Glove Type        | ID   |
| ----------------- | ---- |
| Bloodhound        | 5027 |
| Broken Fang       | 4725 |
| Driver Gloves     | 5031 |
| Hand Wraps        | 5032 |
| Hydra Gloves      | 5035 |
| Moto Gloves       | 5033 |
| Specialist Gloves | 5034 |
| Sport Gloves      | 5030 |

### Example: Giving Yourself Gloves

```
// Example: Sport Gloves (Pandora’s Box)
"number"
{
    "inventory"        "number"
    "def_index"        "5030" // Sport Gloves
    "level"            "1"
    "quality"          "5"
    "flags"            "0"
    "origin"           "8"
    "in_use"           "0"
    "rarity"           "1"

    "attributes"
    {
        "6"  "282.000000"     // Finish Catalog (Pandora’s Box)
        "7"  "69420.000000"   // Pattern Template
        "8"  "0.000000666"    // Float Value
    }
}
```

🔍 You can look up glove or skin finish catalogs here:
👉 [https://csgoskins.gg](https://csgoskins.gg)

---

## 🧍 Agent IDs

### CT Agents

#### Master Agents

* Cmdr. Frank “Wet Sox” Baroud | SEAL Frogman — **4771**
* Cmdr. Mae “Dead Cold” Jamison | SWAT — **4711**
* Special Agent Ava | FBI — **5308**

#### Superior Agents

* Chem-Haz Capitaine | Gendarmerie Nationale — **4750**
* Lieutenant Rex Krikey | SEAL Frogman — **4772**
* 1st Lieutenant Farlow | SWAT — **4712**
* “Two Times” McCoy | TACP Cavalry — **5403**

#### Exceptional Agents

* “Blueberries” Buckshot | NSWC SEAL — **4619**
* Officer Jacques Beltram | Gendarmerie Nationale — **4753**
* Sergeant Bombson | SWAT — **4715**
* Markus Delrow | FBI HRT — **5306**

#### Distinguished Agents

* D Squadron Officer | NZSAS — **5602**
* Seal Team 6 Soldier | NSWC SEAL — **5401**
* Operator | FBI SWAT — **5305**

---

### T Agents

#### Master Agents

* Sir Bloody Miami Darryl | The Professionals — **4726**
* Sir Bloody Silent Darryl | The Professionals — **4733**
* The Elite Mr. Muhlik | Elite Crew — **5108**
* Vypa Sista of the Revolution | Guerrilla Warfare — **4777**

#### Superior Agents

* Number K | The Professionals — **4732**
* Safecracker Voltzmann | The Professionals — **4727**
* Blackwolf | Sabre — **5503**

#### Exceptional Agents

* Getaway Sally | The Professionals — **4730**
* Dragomir | Sabre — **5500**
* Trapper | Guerrilla Warfare — **4781**

#### Distinguished Agents

* Trapper Aggressor | Guerrilla Warfare — **4778**
* Jungle Rebel | Elite Crew — **5109**
* Street Soldier | Phoenix — **5208**

---

### Example: Giving Yourself an Agent

```
// Example: Number K (The Professionals)
"4"
{
    "inventory"        "3" 
    "def_index"        "4732" // Number K
    "level"            "1"
    "quality"          "5"
    "flags"            "0"
    "origin"           "8"
    "in_use"           "0"
    "rarity"           "1"
    // Attributes are optional unless you want patches
}
```

---

## 🎨 Adding Skins (Weapons and Knives)

You can use [CSGOSkins.gg](https://csgoskins.gg) to find any skin’s **Finish Catalog ID**.

**Example:**

* AWP Dragon Lore → Finish Catalog **344**
* AWP’s def_index → **9**

```
// Example: Souvenir AWP Dragon Lore
"number"
{
    "inventory"        "number"
    "def_index"        "9"         // AWP
    "level"            "1"
    "quality"          "5"         // Quality ID
    "flags"            "0"
    "origin"           "8"
    "in_use"           "0"
    "rarity"           "12"        // Souvenir
    "attributes"
    {
        "6"  "344.000000" // Finish Catalog (Dragon Lore)
        "7"  "0.000000"   // Pattern Template
        "8"  "0.100000"   // Float
    }
}
```

> 💡 **Tip:** Adjust the float value and pattern to customize the skin’s wear and design.

---

### ✅ Final Notes

* Always back up your configuration before editing.
* Use valid IDs for items and finishes.
* You can omit `"attributes"` for items without skins (like pins or agents).

---

*Made and organized for easy reading.*
