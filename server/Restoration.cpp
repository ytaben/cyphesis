// This file may be redistributed and modified only under the terms of
// the GNU General Public License (See COPYING for details).
// Copyright (C) 2000,2001 Alistair Riddoch

#include "Restoration.h"
#include "Restorer.h"
#include "ServerRouting.h"
#include "WorldRouter.h"

#include <common/const.h>
#include <common/log.h>
#include <common/debug.h>
#include <common/Database.h>

#include <rulesets/Creator.h>
#include <rulesets/Plant.h>
#include <rulesets/Food.h>
#include <rulesets/Area.h>
#include <rulesets/Line.h>
#include <rulesets/Structure.h>
#include <rulesets/Stackable.h>

#include <iostream>

static const bool debug_flag = false;

Restoration::Restoration(ServerRouting & svr) : server(svr),
                                                database(*Database::instance())
{
    m_restorers["entity"] = &Restorer<Entity>::restore;
    m_restorers["thing"] = &Restorer<Thing>::restore;
    m_restorers["character"] = &Restorer<Character>::restore;
    m_restorers["creator"] = &Restorer<Creator>::restore;
    m_restorers["plant"] = &Restorer<Plant>::restore;
    m_restorers["food"] = &Restorer<Food>::restore;
    m_restorers["line"] = &Restorer<Line>::restore;
    m_restorers["area"] = &Restorer<Area>::restore;
    m_restorers["structure"] = &Restorer<Structure>::restore;
    m_restorers["stackable"] = &Restorer<Stackable>::restore;
}

void Restoration::restore(const std::string & id,
                          const std::string & classn,
                          Entity * loc)
{
    DatabaseResult res = database.selectEntityRow(id, classn);
    if (res.error()) {
        debug(std::cout << "DEBUG: ERROR: Problem getting entity " << id
                        << " from world db" << std::endl << std::flush;);
        return;
    }
    if (res.empty()) {
        log(ERROR, "Missing id in database");
        debug(std::cout << "DEBUG: ERROR: No " << id << " in database"
                        << std::endl << std::flush;);
        return;
    }
    if (res.size() > 1) {
        log(ERROR, "Duplicate id in database");
        debug(std::cout << "DEBUG: ERROR: More than one " << id
                        << " in database" << std::endl << std::flush;);
        return;
    }

    Entity * ent = 0;
    if (loc == 0) {
        debug(std::cout << "DEBUG: No creation of world object " << id
                        << std::endl << std::flush;);
        if (id != server.world.gameWorld.getId()) {
            abort();
        }
        ent = &server.world.gameWorld;
    } else {
        RestoreDict::const_iterator I = m_restorers.find(classn);
        if (I == m_restorers.end()) {
            log(ERROR, "Could not find a restorer for class");
            debug(std::cerr << "ERROR: Class " << classn << " has no restorer."
                            << std::endl << std::flush;);
        } else {
            debug(std::cout << "DEBUG: Creating object for " << id
                            << " with loc " << loc->getId() << std::endl
                            << std::flush;);
            ent = I->second(id, res);
            ent->location.ref = loc;
            server.world.addObject(ent, false);
        }
    }
    DatabaseResult res2 = database.selectClassByLoc(id);
    if (res2.error()) {
        debug(std::cout << "DEBUG: Problem getting " << id
                        << "'s children from world db"
                        << std::endl << std::flush;);
        return;
    }
    if (res2.empty()) {
        debug(std::cout << "DEBUG: No " << id << " children in database"
                        << std::endl << std::flush;);
        return;
    }
    DatabaseResult::const_iterator I = res2.begin();
    for(; I != res2.end(); ++I) {
        std::string child_id = I.column("id");
        std::string child_class = I.column("class");
        if (child_id.empty() || child_class.empty()) {
            log(ERROR, "Malformed (id,class) record from database.");
            debug(std::cout << "DEBUG: Empty record when reading children of "
                            << id << std::endl << std::flush;);
            continue;
        }
        debug(std::cout << "DEBUG: Child is " << child_id
                        << " with class " << child_class
                        << std::endl << std::flush;);
        restore(child_id, child_class, ent);

    }
}

void Restoration::read()
{
    DatabaseResult res = database.selectClassByLoc("");
    if (res.error()) {
        log(ERROR, "Database error retrieving root world.");
        debug(std::cout << "DEBUG: Problem getting root id from world db"
                        << std::endl << std::flush;);
        return;
    }
    if (res.empty()) {
        debug(std::cout << "DEBUG: No world in database"
                        << std::endl << std::flush;);
        database.clearTable("entity_ent");
        database.createEntityRow("world", consts::rootWorldId, "class",
                                                               "'world'");
        return;
    }
    if (res.size() > 1) {
        debug(std::cout << "DEBUG: More than one root entity in database"
                        << std::endl << std::flush;);
        database.clearTable("entity_ent");
        return;
    }
    std::string rootId = res.field("id");
    std::string rootClass = res.field("class");
    if (rootId.empty() || rootClass.empty()) {
        debug(std::cout << "DEBUG: Stuff empty" << std::endl << std::flush;);
        return;
    }
    debug(std::cout << "DEBUG: World is " << rootId << " with class "
                    << rootClass << std::endl << std::flush;);
    restore(rootId, rootClass, false);
}
