// This file may be redistributed and modified only under the terms of
// the GNU General Public License (See COPYING for details).
// Copyright (C) 2000,2001 Alistair Riddoch

#ifndef RULESETS_CREATOR_H
#define RULESETS_CREATOR_H

#include "Character.h"

class Creator : public Character {
  public:
    Creator();

    virtual OpVector operation(const RootOperation & op);
    virtual OpVector externalOperation(const RootOperation & op);

    OpVector sendMind(const RootOperation & msg);
};

#endif // RULESETS_CREATOR_H
