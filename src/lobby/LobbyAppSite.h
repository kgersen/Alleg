#pragma once

#ifndef __LobbyAppSite_h__
#define __LobbyAppSite_h__

/////////////////////////////////////////////////////////////////////////////
// LobbyAppSite.h
//

class ATL_NO_VTABLE ILobbyAppSite
{
public:
  virtual int LogEvent(WORD wType, int id, ...) = 0;
};


/////////////////////////////////////////////////////////////////////////////

#endif // !__LobbyAppSite_h__
