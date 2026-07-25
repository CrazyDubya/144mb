// convoy -- every user-visible string in one place.
//
// Kept centralised so a translation is a data swap rather than a hunt through
// the drawing code. The font is uppercase-only Latin, so another script would
// need its own glyph table too -- but the strings themselves would not move.
#ifndef TEXT_H
#define TEXT_H

#define T_TITLE        "CONVOY"
#define T_TAGLINE      "TRADE EAST OR DIE TRYING"
#define T_START        "PRESS ENTER TO START"
#define T_HELP_HINT    "PRESS H FOR HOW TO PLAY"

// Goods. Order matches the G_* enum in world.h and the ICON_* enum in render.h.
#define T_WATER        "WATER"
#define T_FUEL         "FUEL"
#define T_AMMO         "AMMO"
#define T_MEDS         "MEDS"
#define T_SCRAP        "SCRAP"

// What each good is for. This is the whole design in five lines.
#define T_USE_WATER    "THE CREW DRINK 1 EACH DAY"
#define T_USE_FUEL     "1 PER HOP. NO FUEL, NO ROAD"
#define T_USE_AMMO     "SPEND IT TO FIGHT OFF RAIDERS"
#define T_USE_MEDS     "CURES SICKNESS IN THE CREW"
#define T_USE_SCRAP    "REPAIRS. ALSO PURE TRADE GOODS"

// HUD
#define T_DAY          "DAY"
#define T_CREDITS      "CREDITS"
#define T_HOLD         "HOLD"

// Market
#define T_MARKET       "MARKET"
#define T_PRICE        "PRICE"
#define T_HELD         "HELD"
#define T_BUY          "BUY"
#define T_SELL         "SELL"
#define T_DEPART       "DEPART"

// Map
#define T_ROUTE        "CHOOSE YOUR ROAD"
#define T_SETTLEMENT   "SETTLEMENT - TRADE HERE"
#define T_ENCOUNTER    "ENCOUNTER - UNKNOWN"
#define T_STORM        "STORM - COSTS EXTRA FUEL AND WATER"
#define T_EMPTY_ROAD   "EMPTY ROAD - NOTHING HERE"
#define T_GREEN_ZONE   "THE GREEN ZONE - THE END OF THE ROAD"
#define T_COST         "COST"
#define T_TRAVEL       "TRAVEL"

// Encounters
#define T_RAIDERS      "RAIDERS!"
#define T_WRECK        "A WRECK BY THE ROAD"
#define T_SICK         "THE CREW ARE SICK"
#define T_BREAKDOWN    "THE CONVOY HAS BROKEN DOWN"
#define T_TRADER       "A TRADER FLAGS YOU DOWN"

#define T_RAIDERS_A    "FIGHT THEM OFF"
#define T_RAIDERS_B    "LET THEM TAKE WHAT THEY WANT"
#define T_WRECK_A      "DETOUR AND STRIP IT"
#define T_WRECK_B      "DRIVE ON PAST"
#define T_SICK_A       "TREAT THEM"
#define T_SICK_B       "LET THEM SWEAT IT OUT"
#define T_BREAK_A      "PATCH IT UP"
#define T_BREAK_B      "LIMP ON AND LOSE FUEL"
#define T_TRADER_A     "MAKE THE DEAL"
#define T_TRADER_B     "WAVE HIM ON"
#define T_CANNOT       "YOU CANNOT PAY THIS"

// Endings
#define T_ARRIVED      "YOU REACHED THE GREEN ZONE"
#define T_DIED_THIRST  "THE CREW DIED OF THIRST"
#define T_DIED_FUEL    "STRANDED WITH AN EMPTY TANK"
#define T_DIED_STRIP   "STRIPPED BARE AND LEFT BEHIND"
#define T_AGAIN        "PRESS ENTER TO RUN IT AGAIN"
#define T_REACHED      "REACHED"
#define T_BANKED       "BANKED"

// Help
#define T_HELP_TITLE   "HOW TO PLAY"
#define T_HELP_1       "DRIVE EAST ACROSS 14 SECTORS TO THE GREEN ZONE."
#define T_HELP_2       "EVERY HOP COSTS 1 FUEL. THE CREW DRINK 1 WATER A DAY."
#define T_HELP_3       "YOU START WITH FAR TOO LITTLE OF BOTH."
#define T_HELP_4       "SO YOU TRADE: BUY LOW, CARRY IT EAST, SELL HIGH."
#define T_HELP_5       "BUT SELLING INTO A MARKET DRIVES ITS PRICE DOWN"
#define T_HELP_6       "FOR GOOD. ROUTES BURN OUT BEHIND YOU."
#define T_HELP_7       "THERE IS NO HEALTH BAR. YOUR CARGO IS YOUR LIFE:"
#define T_HELP_8       "RAIDERS TAKE IT, STORMS EAT IT, AND AN EMPTY"
#define T_HELP_9       "HOLD IS THE END OF THE RUN. YOU DIE BROKE."
#define T_HELP_10      "EVERY SETTLEMENT MAKES SOMETHING. A WELL SELLS WATER"
#define T_HELP_11      "CHEAP AND PAYS WELL FOR FUEL. READ THE MAP BADGES."
#define T_HELP_12      "AN ARROW BY A PRICE COMPARES IT TO EVERY PRICE YOU"
#define T_HELP_13      "HAVE SEEN:  DOWN MEANS CHEAP,  UP MEANS THEY PAY WELL."
#define T_HELP_14      "A STALL PAYS LESS THAN IT CHARGES. THE SELL PRICE IS"
#define T_HELP_15      "SHOWN NEXT TO SELL, SO CHECK IT BEFORE YOU HAUL."
#define T_HELP_KEYS    "ARROWS SELECT    Z BUY / ACCEPT    X SELL / REFUSE"
#define T_HELP_KEYS2   "ENTER DEPART / CONFIRM    ESC QUIT"
#define T_BACK         "PRESS ANY KEY TO GO BACK"

// Settlement tabs
#define T_TAB_MARKET    "MARKET"
#define T_TAB_GARAGE    "GARAGE"
#define T_TAB_CREW      "CREW"
#define T_TAB_CONTRACTS "CONTRACTS"

// Contracts
#define T_NO_WORK       "NO WORK POSTED HERE"
#define T_JOB_OFFER     "DELIVERY OFFERED"
#define T_JOB_TAKEN     "CARGO PROMISED"
#define T_JOB_DELIVER   "DELIVER TO ANY SETTLEMENT AT OR PAST SECTOR"
#define T_JOB_PAYS      "PAYS"
#define T_JOB_ACCEPT    "ACCEPT THE JOB"
#define T_JOB_HOLDING   "HOLDING"
#define T_JOB_DONE      "DELIVERED"
#define T_JOB_LOCKED    "PROMISED CARGO CANNOT BE SOLD"

// Garage: one-off purchases that change the rules of the run.
#define T_UPG_HOLD      "CARGO RACKS"
#define T_UPG_ECON      "TUNED ENGINE"
#define T_UPG_ARMOUR    "PLATE ARMOUR"
#define T_UPG_TANKS     "WATER TANKS"
#define T_UPG_HOLD_D    "+10 SLOTS IN THE HOLD"
#define T_UPG_ECON_D    "EVERY SECOND HOP BURNS NO FUEL"
#define T_UPG_ARMOUR_D  "RAIDERS LEAVE WITH ALMOST NOTHING"
#define T_UPG_TANKS_D   "CONDENSERS: EVERY SECOND DAY IS DRY"
#define T_OWNED         "FITTED"
#define T_SALVAGED      "SALVAGED"
#define T_SOUND         "SOUND"
#define T_SALVAGE_WARN  "CHEAP, BUT IT MAY GIVE OUT ON THE ROAD"
#define T_SOUND_NOTE    "WILL HOLD FOR THE REST OF THE RUN"
#define T_ROAD_AHEAD    "THE ROAD EAST"
#define T_AHEAD_STORMS  "STORMS"
#define T_AHEAD_EVENTS  "ENCOUNTERS"
#define T_KIT_BROKE     "BROKE ON THE ROAD"
#define T_PAYS_BACK     "SHOULD RETURN"
#define T_BUY_UPGRADE   "FIT IT"
#define T_NO_GARAGE     "NOTHING FOR SALE HERE"

// Crew: hands that help, and mouths that drink.
#define T_CREW_MECHANIC "MECHANIC"
#define T_CREW_GUARD    "GUARD"
#define T_CREW_MEDIC    "MEDIC"
#define T_CREW_SCOUT    "SCOUT"
#define T_CREW_TRADER   "TRADER"
#define T_CREW_MECHANIC_D "BREAKDOWNS COST NO SCRAP"
#define T_CREW_GUARD_D    "RAIDS COST NO AMMO"
#define T_CREW_MEDIC_D    "SICKNESS COSTS NO MEDS"
#define T_CREW_SCOUT_D    "KNOWS THE SAFE LINE: STORMS COST NOTHING"
#define T_CREW_TRADER_D   "BETTER PRICES AT EVERY STALL"
#define T_HIRED         "ABOARD"
#define T_HIRE          "TAKE THEM ON"
#define T_NO_CREW       "NOBODY LOOKING FOR WORK"
#define T_CREW_WARN     "EVERY HAND ABOARD DRINKS A WATER A DAY"
#define T_CREW_COUNT    "CREW"

// Settlement archetypes. Each makes its own good cheap and the rest dearer, so
// the map reads as an economic landscape rather than a row of identical shops.
#define T_ARCH_WELL     "WELL"
#define T_ARCH_REFINERY "REFINERY"
#define T_ARCH_ARMOURY  "ARMOURY"
#define T_ARCH_CLINIC   "CLINIC"
#define T_ARCH_SCRAPYARD "SCRAPYARD"
#define T_ARCH_GENERAL  "TRADING POST"

#define T_ARCH_WELL_D      "DEEP WATER. THEY SELL IT CHEAP"
#define T_ARCH_REFINERY_D  "THEY CRACK FUEL HERE"
#define T_ARCH_ARMOURY_D   "GUNS AND BRASS, NOTHING ELSE"
#define T_ARCH_CLINIC_D    "MEDICINE, AND QUESTIONS ABOUT IT"
#define T_ARCH_SCRAPYARD_D "PICKED-OVER METAL, PILED HIGH"
#define T_ARCH_GENERAL_D   "A LITTLE OF EVERYTHING, CHEAP AT NOTHING"

#define T_CHEAP_HERE    "CHEAP HERE"
#define T_DEAR_HERE     "THEY PAY WELL HERE"

extern const char *const GOOD_NAME[5];
extern const char *const GOOD_USE[5];
extern const char *const ARCH_NAME[6];
extern const char *const ARCH_DESC[6];

#endif
