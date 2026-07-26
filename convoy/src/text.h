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

// Title menu. The difficulty blurbs name the failure each setting leans on,
// so the choice is informed before the first run rather than after the third.
#define T_M_DIFF       "DIFFICULTY"
#define T_M_MODE       "MAP"
#define T_M_ARROWS     "ARROWS CHOOSE"
#define T_DIFF_EASY    "FORGIVING"
#define T_DIFF_NORMAL  "THE ROAD"
#define T_DIFF_HARD    "UNFORGIVING"
#define T_DIFF_EASY_D  "TOWNS ARE CLOSE AND STORMS ARE RARE. FUEL WILL STILL KILL YOU."
#define T_DIFF_NORM_D  "THIRST, FUEL AND RAIDERS ALL GET A TURN."
#define T_DIFF_HARD_D  "LONG GAPS BETWEEN TOWNS, HARD WEATHER. ARRIVING WHOLE IS THE PROBLEM."
#define T_MODE_STD     "RANDOM"
#define T_MODE_DAILY   "TODAY'S RUN"
#define T_MODE_STD_D   "A NEW MAP EVERY TIME."
#define T_MODE_DAILY_D "ONE MAP A DAY. EVERYONE GETS THE SAME ONE. COMPARE SCORES."
#define T_SCORE        "SCORE"
#define T_SEED         "SEED"

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
#define T_TOLL         "A CHAIN ACROSS THE ROAD"
#define T_CACHE        "SOMETHING BURIED OUT HERE"
#define T_BRIDGE       "THE BRIDGE IS DOWN"
#define T_RIVAL        "ANOTHER CONVOY, HEADING WEST"
#define T_PLAGUE       "FEVER IN THE LAST TOWN"
#define T_CHECKPOINT   "A CHECKPOINT, OF SORTS"
#define T_LEAK         "THE TANK IS WEEPING"
#define T_REFUGEE      "WALKERS ON THE ROAD"
#define T_SIGNAL       "A VOICE ON THE RADIO"

#define T_TOLL_A       "PAY THE TOLL"
#define T_TOLL_B       "DRIVE THROUGH IT"
#define T_CACHE_A      "DIG IT OUT"
#define T_CACHE_B      "LEAVE IT BURIED"
#define T_BRIDGE_A     "TAKE THE LONG WAY"
#define T_BRIDGE_B     "WAIT FOR THE WATER TO DROP"
#define T_RIVAL_A      "MAKE THE SWAP"
#define T_RIVAL_B      "KEEP DRIVING"
#define T_PLAGUE_A     "DOSE THE CREW"
#define T_PLAGUE_B     "RIDE IT OUT"
#define T_CHECK_A      "GREASE THEIR PALMS"
#define T_CHECK_B      "LET THEM SEARCH THE HOLD"
#define T_LEAK_A       "PATCH IT NOW"
#define T_LEAK_B       "RUN IT AND HOPE"
#define T_REFUGEE_A    "SPARE THEM WATER"
#define T_REFUGEE_B    "DRIVE PAST"
#define T_SIGNAL_A     "GO AND LOOK"
#define T_SIGNAL_B     "IGNORE IT"
#define T_CANNOT       "YOU CANNOT PAY THIS"
// world_can_accept refuses for two unrelated reasons and said the same thing
// for both, so a full hold read as an empty purse.
#define T_NO_ROOM      "NO ROOM IN THE HOLD"
#define T_FREE         "FREE"

// The opening. Short: a judge wants to play, not read.
#define T_OPEN_1A      "THE LAST GREENHOUSE IN THE WEST BURNED IN THE SPRING."
#define T_OPEN_1B      "WHAT SURVIVED FITS IN SIX CRATES."
#define T_OPEN_2A      "THE GREEN ZONE IS FOURTEEN SECTORS EAST."
#define T_OPEN_2B      "THEY HAVE SOIL AND WATER. THEY HAVE NOTHING TO PLANT."
#define T_OPEN_3A      "YOU CANNOT SELL THE SEED. NOT FOR FUEL,"
#define T_OPEN_3B      "NOT FOR WATER, NOT TO SAVE YOUR OWN LIFE."
#define T_OPEN_3C      "EVERYTHING ELSE IN THE HOLD IS NEGOTIABLE."
// ESC quits the game; it has never skipped anything. The prompt told players
// to press the one key that ends the run.
#define T_OPEN_SKIP    "ANY KEY TO CONTINUE"

// The people you meet more than once. Names are short so they fit beside a
// portrait, and each has three lines: a first meeting, a warm return and a
// cold one. Which you get depends on how you treated them last time.
#define T_WHO_CHIEF    "VULTURE"
#define T_WHO_CAPTAIN  "MARLOW"
#define T_WHO_TRADER   "OKONJO"
#define T_WHO_DOC      "SISTER RAE"
#define T_WHO_DRIFTER  "THE WALKER"

#define T_CHIEF_1      "NEW WHEELS ON MY ROAD. LET US SEE WHAT YOU CARRY."
#define T_CHIEF_WARM   "YOU PAY WITHOUT FUSS. I LIKE THAT IN A DRIVER."
#define T_CHIEF_COLD   "YOU AGAIN. YOU STILL OWE ME FOR LAST TIME."

#define T_CAPTAIN_1    "HEADING EAST? THERE IS NOTHING EAST. I WOULD KNOW."
#define T_CAPTAIN_WARM "GOOD TRADE LAST TIME. LET US DO IT AGAIN."
#define T_CAPTAIN_COLD "YOU DROVE PAST ME ONCE. I REMEMBER FACES."

#define T_TRADER_1     "EVERYONE OUT HERE NEEDS SOMETHING. WHAT IS YOURS?"
#define T_TRADER_WARM  "MY FAVOURITE CUSTOMER. I SAVED YOU A PRICE."
#define T_TRADER_COLD  "LAST TIME YOU WASTED MY WATER AND MY DAY."

#define T_DOC_1        "THAT COUGH IS NOT DUST. I HAVE SEEN IT BEFORE."
#define T_DOC_WARM     "YOU LISTENED LAST TIME. YOUR CREW ARE STILL BREATHING."
#define T_DOC_COLD     "YOU LET IT RUN LAST TIME. IT DOES NOT GET BETTER."

#define T_DRIFTER_1    "WALKED FROM THE COAST. THERE IS NOTHING BEHIND ME."
#define T_DRIFTER_WARM "YOU SPARED WATER ONCE. WORD GETS AROUND."
#define T_DRIFTER_COLD "YOU DROVE PAST. I WALKED. HERE WE BOTH ARE."

#define T_MET_BEFORE   "MET BEFORE"
#define T_REGARD_GOOD  "OWES YOU A FAVOUR"
#define T_REGARD_BAD   "HOLDS A GRUDGE"
#define T_REGARD_NEUT  "NO OPINION EITHER WAY"
#define T_TAB_JOURNAL  "PEOPLE"
#define T_NOBODY_YET   "NOBODY WORTH REMEMBERING YET"
#define T_TIMES        "TIMES"
#define T_TIME         "TIME"

// Endings, one per outcome.
#define T_END_DEAD_A   "THE ROAD KEEPS WHAT IT TAKES."
#define T_END_DEAD_B   "SOMEONE WILL FIND THE CRATES EVENTUALLY."
#define T_END_EMPTY_A  "YOU REACHED THE GREEN ZONE WITH AN EMPTY HOLD."
#define T_END_EMPTY_B  "THEY FEED YOU. THEY DO NOT ASK WHAT HAPPENED."
#define T_END_EMPTY_C  "THE SOIL STAYS BARE."
#define T_END_PART_A   "SOME OF IT MADE IT. NOT ALL."
#define T_END_PART_B   "THEY TAKE WHAT YOU HAVE AND START COUNTING ROWS."
#define T_END_PART_C   "IT WILL BE A THIN FIRST YEAR."
#define T_END_INTACT_A "ALL SIX CRATES, FOURTEEN SECTORS, ONE PIECE."
#define T_END_INTACT_B "THEY PLANT BEFORE YOU HAVE FINISHED UNLOADING."
#define T_END_EXEMP_A  "ALL SIX CRATES, A CREW STILL BREATHING,"
#define T_END_EXEMP_B  "AND ENOUGH LEFT OVER TO GO BACK FOR MORE."
#define T_END_EXEMP_C  "THEY NAME THE FIRST FIELD AFTER THE CONVOY."

// Vignettes: a line or two on the road, at moments worth marking.
#define T_VIG_FIRST_A  "THE WEST IS BEHIND YOU NOW."
#define T_VIG_FIRST_B  "THE CRATES RATTLE ON EVERY RUT."
#define T_VIG_HALF_A   "HALFWAY. THE SEED IS STILL ABOARD."
#define T_VIG_HALF_B   "SO ARE YOU."
#define T_VIG_STORM_A  "THE HORIZON GOES BROWN AND KEEPS COMING."
#define T_VIG_LAST_A   "ONE MORE HOP. YOU CAN SEE THE GREEN FROM HERE."
#define T_VIG_LOSS_A   "THEY TOOK THE LAST OF THE SEED."
#define T_VIG_LOSS_B   "YOU KEEP DRIVING EAST. THERE IS NOTHING ELSE TO DO."
#define T_PAYLOAD      "SEED"
#define T_PAYLOAD_SAFE "CRATES ABOARD"

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
// T_MARKET and T_END_AGAIN were removed: exact duplicates of T_TAB_MARKET
// and T_AGAIN that nothing referenced.
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
#define T_JOB_DECLINE   "TURN IT DOWN"

// The third branch. One line per kind would be better writing and fourteen
// more strings; a single verb per role reads well enough and keeps the table
// to five entries.
#define T_ALT_MECHANIC "LET THEM PATCH IT"
#define T_ALT_GUARD    "LET THEM HANDLE IT"
#define T_ALT_MEDIC    "LET THEM SEE TO IT"
#define T_ALT_SCOUT    "LET THEM FIND A WAY"
#define T_ALT_TRADER   "LET THEM TALK"
#define T_ALT_ODDS     "CHANCE"
#define T_ALT_RISK     "IF IT GOES WRONG IT COSTS MORE THAN WALKING AWAY"
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
