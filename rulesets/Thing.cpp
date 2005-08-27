// This file may be redistributed and modified only under the terms of
// the GNU General Public License (See COPYING for details).
// Copyright (C) 2000-2005 Alistair Riddoch

#include "Thing.h"

#include "Script.h"
#include "Motion.h"

#include "common/log.h"
#include "common/const.h"
#include "common/debug.h"

#include "common/Tick.h"
#include "common/Nourish.h"
#include "common/Update.h"

#include <wfmath/atlasconv.h>

#include <Atlas/Objects/Operation.h>
#include <Atlas/Objects/Anonymous.h>

using Atlas::Objects::smart_dynamic_cast;

using Atlas::Message::Element;
using Atlas::Message::MapType;
using Atlas::Message::ListType;
using Atlas::Objects::Root;
using Atlas::Objects::Operation::Set;
using Atlas::Objects::Operation::Tick;
using Atlas::Objects::Operation::Sight;
using Atlas::Objects::Operation::Delete;
using Atlas::Objects::Operation::Update;
using Atlas::Objects::Operation::Nourish;
using Atlas::Objects::Operation::Appearance;
using Atlas::Objects::Operation::Disappearance;
using Atlas::Objects::Entity::Anonymous;
using Atlas::Objects::Entity::RootEntity;

static const bool debug_flag = false;

/// \brief Constructor for physical or tangiable entities.
Thing::Thing(const std::string & id) : Entity(id)
{
    subscribe("setup", OP_SETUP);
    subscribe("action", OP_ACTION);
    subscribe("delete", OP_DELETE);
    subscribe("burn", OP_BURN);
    subscribe("move", OP_MOVE);
    subscribe("set", OP_SET);
    subscribe("look", OP_LOOK);
    subscribe("update", OP_UPDATE);

    m_motion = new Motion(*this);
}

Thing::~Thing()
{
    assert(m_motion != 0);
    delete m_motion;
}

void Thing::SetupOperation(const Operation & op, OpVector & res)
{
    Appearance app;
    Anonymous arg;
    arg->setId(getId());
    app->setArgs1(arg);

    res.push_back(app);

    if (m_script->Operation("setup", op, res) != 0) {
        return;
    }

    Operation tick;
    tick->setTo(getId());

    res.push_back(tick);
}

void Thing::ActionOperation(const Operation & op, OpVector & res)
{
    if (m_script->Operation("action", op, res) != 0) {
        return;
    }
    Operation s;
    s->setArgs1(op);
    res.push_back(s);
}

void Thing::DeleteOperation(const Operation & op, OpVector & res)
{
    if (m_script->Operation("delete", op, res) != 0) {
        return;
    }
    // The actual destruction and removal of this entity will be handled
    // by the WorldRouter
    Operation s;
    s->setArgs1(op);
    res.push_back(s);
}

void Thing::BurnOperation(const Operation & op, OpVector & res)
{
    if (m_script->Operation("burn", op, res) != 0) {
        return;
    }
    if (op->getArgs().empty()) {
        error(op, "Fire op has no argument", res, getId());
        return;
    }
    MapType::const_iterator I = m_attributes.find("burn_speed");
    if ((I == m_attributes.end()) || !I->second.isNum()) {
        return;
    }
    double bspeed = I->second.asNum();
    const Root & fire_ent = op->getArgs().front();
    double consumed = bspeed * fire_ent->getAttr("status").asNum();

    const std::string & to = fire_ent->getId();
    Anonymous nour_ent;
    nour_ent->setId(to);
    nour_ent->setAttr("mass", consumed);

    Set s;
    s->setTo(getId());

    Anonymous self_ent;
    self_ent->setId(getId());
    self_ent->setAttr("status", m_status - (consumed / m_mass));
    s->setArgs1(self_ent);

    res.push_back(s);

    Nourish n;
    n->setTo(to);
    n->setArgs1(nour_ent);

    res.push_back(n);
}

void Thing::MoveOperation(const Operation & op, OpVector & res)
{
    debug( std::cout << "Thing::move_operation" << std::endl << std::flush;);
    m_seq++;
    if (m_script->Operation("move", op, res) != 0) {
        return;
    }
    const std::vector<Root> & args = op->getArgs();
    if (args.empty()) {
        error(op, "Move has no argument", res, getId());
        return;
    }
    const Root & ent = args.front();
    // FIXME ent should be an entity, probably anonymous. If so, we can get
    // direct access to pos, loc and velocity without a copy.
    if (getId() != ent->getId()) {
        error(op, "Move op does not have correct id in argument", res, getId());
    }
    Element attr_loc;
    if (ent->getAttr("loc", attr_loc) != 0 || !attr_loc.isString()) {
        error(op, "Move op has no loc", res, getId());
        return;
    }
    const std::string & new_loc_id = attr_loc.asString();
    EntityDict::const_iterator J = m_world->getEntities().find(new_loc_id);
    if (J == m_world->getEntities().end()) {
        error(op, "Move op loc invalid", res, getId());
        return;
    }
    debug(std::cout << "{" << new_loc_id << "}" << std::endl << std::flush;);
    Entity * new_loc = J->second;
    Entity * test_loc = new_loc;
    for (; test_loc != 0; test_loc = test_loc->m_location.m_loc) {
        if (test_loc == this) {
            error(op, "Attempt by entity to move into itself", res, getId());
            return;
        }
    }
    Element attr_pos;
    if (ent->getAttr("pos", attr_pos) != 0 || !attr_pos.isList()) {
        error(op, "Move op has no pos", res, getId());
        return;
    }
    const ListType & pos_list = attr_pos.asList();

    // Up until this point nothing should have changed, but the changes
    // have all now been checked for validity.

    if (m_location.m_loc != new_loc) {
        // Update loc
        m_location.m_loc->m_contains.erase(this);
        if (m_location.m_loc->m_contains.empty()) {
            m_location.m_loc->m_update_flags |= a_cont;
            m_location.m_loc->updated.emit();
        }
        bool was_empty = new_loc->m_contains.empty();
        new_loc->m_contains.insert(this);
        if (was_empty) {
            new_loc->m_update_flags |= a_cont;
            new_loc->updated.emit();
        }
        m_location.m_loc->decRef();
        m_location.m_loc = new_loc;
        m_location.m_loc->incRef();
        m_update_flags |= a_loc;
    }

    std::string mode;

    if (has("mode")) {
        Element mode_attr;
        assert(get("mode", mode_attr));
        if (mode_attr.isString()) {
            mode = mode_attr.asString();
        } else {
            log(ERROR, "Non string mode on entity in Thing::MoveOperation");
            std::cout << "Mode is of type " << mode_attr.getType()
                      << std::endl << std::flush;
        }
    }

    // Move ops often include a mode change, so we handle it here, even
    // though it is not a special attribute for efficiency. Otherwise
    // an additional Set op would be required.
    Element attr_mode;
    if (ent->getAttr("mode", attr_mode) == 0) {
        // FIXME
        if (!attr_mode.isString()) {
            log(ERROR, "Non string mode set in Thing::MoveOperation");
        } else {
            // Update the mode
            set("mode", attr_mode);
            m_motion->setMode(attr_mode.asString());
            mode = attr_mode.asString();
        }
    }

    Point3D oldpos = m_location.m_pos;

    // Update pos
    m_location.m_pos.fromAtlas(pos_list);
    // FIXME Quick height hack
    m_location.m_pos.z() = m_world->constrainHeight(m_location.m_loc,
                                                    m_location.m_pos,
                                                    mode);
    // m_location.update(m_world->getTime());
    m_update_flags |= a_pos;

    Element attr_velocity;
    if (ent->getAttr("velocity", attr_velocity) == 0) {
        // Update velocity
        m_location.m_velocity.fromAtlas(attr_velocity.asList());
        // Velocity is not persistent so has no flag
    }

    Element attr_orientation;
    if (ent->getAttr("orientation", attr_orientation) == 0) {
        // Update orientation
        m_location.m_orientation.fromAtlas(attr_orientation.asList());
        m_update_flags |= a_orient;
    }

    // At this point the Location data for this entity has been updated.

    // Take into account terrain following etc.
    // Take into account mode also.
    // m_motion->adjustNewPostion();



    Operation m(op->copy());
    RootEntity marg = smart_dynamic_cast<RootEntity>(m->getArgs().front());
    assert(marg.isValid());
    m_location.addToEntity(marg);

    Sight s;
    s->setArgs1(m);

    res.push_back(s);

    if (m_location.m_velocity.isValid() &&
        m_location.m_velocity.sqrMag() > WFMATH_EPSILON) {
        // m_motion->genUpdateOperation(); ??
        Update u;
        u->setFutureSeconds(consts::basic_tick);
        u->setTo(getId());

        res.push_back(u);
    }

    // I think it might be wise to send a set indicating we have changed
    // modes, but this would probably be wasteful

    // This code handles sending Appearance and Disappearance operations
    // to this entity and others to indicate if one has gained or lost
    // sight of the other because of this movement
    if (consts::enable_ranges && isPerceptive()) {
        debug(std::cout << "testing range" << std::endl;);
        float fromSquSize = boxSquareSize(m_location.m_bBox);
        std::vector<Root> appear, disappear;

        Anonymous this_ent;
        this_ent->setId(getId());
        this_ent->setStamp(m_seq);

        EntitySet::const_iterator I = m_location.m_loc->m_contains.begin();
        EntitySet::const_iterator Iend = m_location.m_loc->m_contains.end();
        for(; I != Iend; ++I) {
            float oldDist = squareDistance((*I)->m_location.m_pos, oldpos),
                  newDist = squareDistance((*I)->m_location.m_pos, m_location.m_pos),
                  oSquSize = boxSquareSize((*I)->m_location.m_bBox);

            // Build appear and disappear lists, and send operations
            // Also so operations to (dis)appearing perceptive
            // entities saying that we are (dis)appearing
            if ((*I)->isPerceptive()) {
                bool wasInRange = ((fromSquSize / oldDist) > consts::square_sight_factor),
                     isInRange = ((fromSquSize / newDist) > consts::square_sight_factor);
                if (wasInRange ^ isInRange) {
                    if (wasInRange) {
                        // Send operation to the entity in question so it
                        // knows it is losing sight of us.
                        Disappearance d;
                        d->setArgs1(this_ent);
                        d->setTo((*I)->getId());
                        res.push_back(d);
                    } else /*if (isInRange)*/ {
                        // Send operation to the entity in question so it
                        // knows it is gaining sight of us.
                        // FIXME We don't need to do this, cos its about
                        // to get our Sight(Move)
                        Appearance a;
                        a->setArgs1(this_ent);
                        a->setTo((*I)->getId());
                        res.push_back(a);
                    }
                }
            }
            
            bool couldSee = ((oSquSize / oldDist) > consts::square_sight_factor),
                 canSee = ((oSquSize / newDist) > consts::square_sight_factor);
            if (couldSee ^ canSee) {
                Anonymous that_ent;
                that_ent->setId((*I)->getId());
                that_ent->setStamp((*I)->getSeq());
                if (couldSee) {
                    // We are losing sight of that object
                    disappear.push_back(that_ent);
                    debug(std::cout << getId() << ": losing site of "
                                    << (*I)->getId() << std::endl;);
                } else /*if (canSee)*/ {
                    // We are gaining sight of that object
                    appear.push_back(that_ent);
                    debug(std::cout << getId() << ": gaining site of "
                                    << (*I)->getId() << std::endl;);
                }
            }
        }
        if (!appear.empty()) {
            // Send an operation to ourselves with a list of entities
            // we are losing sight of
            Appearance a;
            a->setArgs(appear);
            a->setTo(getId());
            res.push_back(a);
        }
        if (!disappear.empty()) {
            // Send an operation to ourselves with a list of entities
            // we are gaining sight of
            Disappearance d;
            d->setArgs(disappear);
            d->setTo(getId());
            res.push_back(d);
        }
    }
    updated.emit();
}

void Thing::SetOperation(const Operation & op, OpVector & res)
{
    m_seq++;
    if (m_script->Operation("set", op, res) != 0) {
        return;
    }
    const std::vector<Root> & args = op->getArgs();
    if (args.empty()) {
        error(op, "Set has no argument", res, getId());
        return;
    }
    const Root & ent = args.front();
    merge(ent->asMessage());
    Sight s;
    s->setArgs1(op);
    res.push_back(s);
    if (m_status < 0) {
        Delete d;
        Anonymous darg;
        darg->setId(getId());
        d->setArgs1(darg);
        d->setTo(getId());
        res.push_back(d);
    }
    if (m_update_flags != 0) {
        updated.emit();
    }
}

void Thing::UpdateOperation(const Operation & op, OpVector & res)
{
    debug(std::cout << "Update" << std::endl << std::flush;);
    // This is where we will handle movement simulation from now on, rather
    // than in the mind interface. The details will be sorted by a new type
    // of object which will handle the specifics.
}
