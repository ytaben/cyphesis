
// This file may be redistributed and modified only under the terms of
// the GNU General Public License (See COPYING for details).
// Copyright (C) 2000,2001 Alistair Riddoch

#ifndef SERVER_COMM_LISTENER_H
#define SERVER_COMM_LISTENER_H

#include "CommSocket.h"

#include <skstream/skserver.h>

class CommListener : public CommSocket {
  private:
    tcp_socket_server m_listener;

    bool accept();

  public:
    CommListener(CommServer & svr) : CommSocket(svr) { }
    virtual ~CommListener();

    bool setup(int port);

    int getFd() const;
    bool eof();
    bool isOpen() const;
    bool read();
    void dispatch();
};

#endif // SERVER_COMM_LISTENER_H
