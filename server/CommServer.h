// This file may be redistributed and modified only under the terms of
// the GNU General Public License (See COPYING for details).
// Copyright (C) 2000,2001 Alistair Riddoch

#ifndef SERVER_COMM_SERVER_H
#define SERVER_COMM_SERVER_H

#include <set>
#include <string>

class CommSocket;
class CommIdleSocket;
class CommMetaClient;
class ServerRouting;

typedef std::set<CommSocket *> comm_set_t;
typedef std::set<CommIdleSocket *> commi_set_t;

class CommServer {
  private:
    comm_set_t sockets;
    commi_set_t idleSockets;

    void idle();

  public:
    const std::string identity;
    ServerRouting & server;

    CommServer(ServerRouting & srv, const std::string & ident);
    ~CommServer();

    void loop();
    void removeSocket(CommSocket * client, char * msg);
    void removeSocket(CommSocket * client);

    void add(CommSocket * cs) {
        sockets.insert(cs);
    }

    // There is current no mechanism for removing things from the
    // idle set. If one is needed in future, it must be implemented.
    void addIdle(CommIdleSocket * cs) {
        idleSockets.insert(cs);
    }
};

#endif // SERVER_COMM_SERVER_H
