//-----------------------------------
// Copyright Pierric Gimmig 2013-2017
//-----------------------------------

#include "OrbitSession.h"

//-----------------------------------------------------------------------------
Session::Session() {}

//-----------------------------------------------------------------------------
Session::~Session() {}

//-----------------------------------------------------------------------------
ORBIT_SERIALIZE(Session, 1) {
  ORBIT_NVP_VAL(0, m_ProcessFullPath);
  ORBIT_NVP_VAL(0, m_Modules);
  ORBIT_NVP_VAL(1, m_WorkingDirectory);
  ORBIT_NVP_VAL(1, m_Arguments);
}

//-----------------------------------------------------------------------------
ORBIT_SERIALIZE(SessionModule, 0) {
  ORBIT_NVP_VAL(0, m_Name);
  ORBIT_NVP_VAL(0, m_FunctionHashes);
}
