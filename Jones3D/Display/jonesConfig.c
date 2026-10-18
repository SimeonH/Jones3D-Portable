#include "jonesConfig.h"
#include <j3dcore/j3dhook.h>
#include <wkernel/wkernel.h>

#include <Jones3D/Display/JonesHud.h>
#include <Jones3D/Display/JonesHudConstants.h>
#include <Jones3D/Gui/JonesDialog.h>
#include <Jones3D/Gui/wx/JonesWxDialogs.h>
#include <Jones3D/Main/JonesFile.h>
#include <Jones3D/Main/JonesLevel.h>
#include <Jones3D/Main/JonesMain.h>
#include <Jones3D/Main/jonesString.h>
#include <Jones3D/Play/JonesControl.h>
#include <Jones3D/Play/jonesInventory.h>
#include <Jones3D/RTI/symbols.h>

#include <rdroid/Main/rdroid.h>
#include <rdroid/Raster/rdCache.h>

#include <sith/Devices/sithControl.h>
#include <sith/Devices/sithSoundMixer.h>
#include <sith/Dss/sithGamesave.h>
#include <sith/Engine/sithRender.h>
#include <sith/Gameplay/sithOverlayMap.h>
#include <sith/Gameplay/sithPlayer.h>
#include <sith/Main/sithString.h>
#include <sith/World/sithModel.h>
#include <sith/World/sithVoice.h>
#include <sith/World/sithWorld.h>

#include <std/General/std.h>
#include <std/General/stdBmp.h>
#include <std/General/stdFnames.h>
#include <std/General/stdMemory.h>
#include <std/General/stdUtil.h>
#include <std/Win95/std3D.h>
#include <std/Win95/stdControl.h>
#include <std/Win95/stdDisplay.h>
#include <std/Win95/stdWin95.h>

#include <sound/Sound.h>

#include <w32util/wuRegistry.h>

#include <math.h>
#include <windowsx.h>

typedef struct sJonesConfigControlAction
{
    const size_t numBinds;
    const size_t* aBindings;
} JonesConfigControlAction;




// Module vars
static int jonesConfig_bStartup = 0; // Fixed: Added init. to 0

static char jonesConfig_aKeySetsDirPath[128] = { 0 }; // Fixed: Added init. to 0

// Game play options dialog
static int jonesConfig_gamePlayOptions_bDefaultRun;

// Control options dialog
static JonesControlsConfig jonesConfig_controlOptions_curControlConfig = { 0 };

// Game statistics dialog
static int jonesConfig_gameStatistics_curLevelNum;


// Control keyset scheme serialization vars
static size_t jonesConfig_maxControlStrLen       = 0u;
static size_t jonesConfig_maxControlWords        = 0u;
static size_t jonesConfig_maxControlPaddedStrLen = 0u;
static size_t jonesConfig_maxActionStrLen        = 0u;
static size_t jonesConfig_maxActionWords         = 0u;
static size_t jonesConfig_maxActionPaddedStrLen  = 0u; // Fixed: Init to 0





// Keyboard default keysets

static const size_t aActionWalkFwdKeys[2]     = { DIK_UP   , JONESCONFIG_JOYAXISKID_UP };
static const size_t aActionWalkBackKeys[2]    = { DIK_DOWN , JONESCONFIG_JOYAXISKID_DOWN };
static const size_t aActionTurnLeftKeys[2]    = { DIK_LEFT , JONESCONFIG_JOYAXISKID_LEFT };
static const size_t aActionTurnRightKeys[2]   = { DIK_RIGHT, JONESCONFIG_JOYAXISKID_RIGHT };
static const size_t aActionStepLeftKeys[1]    = { DIK_A };
static const size_t aActionStepRightKeys[1]   = { DIK_S };
static const size_t aActionCrawlKeys[1]       = { DIK_C };
static const size_t aActionRunKeys[2]         = { DIK_LSHIFT, DIK_RSHIFT };
static const size_t aActionRollKeys[1]        = { DIK_Z };
static const size_t aActionJumpKeys[4]        = { DIK_X, DIK_LMENU, DIK_RMENU, STDCONTROL_JOYSTICK_GETBUTTON(0, 1) };
static const size_t aActionLookKeys[2]        = { DIK_INSERT, DIK_NUMPAD0 };
static const size_t aActionActivateKeys[3]    = { DIK_LCONTROL, DIK_RCONTROL, STDCONTROL_JOYSTICK_GETBUTTON(0, 0) };
static const size_t aActionToggleMapKeys[1]   = { DIK_SPACE };
static const size_t aActionPrevWeaponKeys[1]  = { DIK_COMMA };
static const size_t aActionNextWeaponKeys[1]  = { DIK_PERIOD };
static const size_t aActionToggleLightKeys[1] = { DIK_L };
static const size_t aActionFistsKeys[1]       = { DIK_0 };
static const size_t aActionWhipKeys[1]        = { DIK_1 };
static const size_t aActionMauserKeys[1]      = { DIK_4 };
static const size_t aActionPPSH41Keys[1]      = { DIK_7 };
static const size_t aActionPistolKeys[1]      = { DIK_2 };
static const size_t aActionSimonovKeys[1]     = { DIK_5 };
static const size_t aActionTokarevKeys[1]     = { DIK_3 };
static const size_t aActionToz34Keys[1]       = { DIK_8 };
static const size_t aActionBazookaKeys[1]     = { DIK_9 };
static const size_t aActionMacheteKeys[1]     = { DIK_6 };
static const size_t aActionSatchelKeys[1]     = { DIK_P };
static const size_t aActionGrenadeKeys[1]     = { DIK_G };
static const size_t aActionMapKeys[1]         = { DIK_M };
static const size_t aActionInterfaceKeys[1]   = { DIK_I };
static const size_t aActionHealthKeys[1]      = { DIK_H };
static const size_t aActionIMP1Keys[1]        = { DIK_Q };
static const size_t aActionIMP2Keys[1]        = { DIK_W };
static const size_t aActionIMP3Keys[1]        = { DIK_E };
static const size_t aActionIMP4Keys[1]        = { DIK_R };
static const size_t aActionIMP5Keys[1]        = { DIK_T };
static const size_t aActionChalk[1]           = { DIK_K };

static const JonesConfigControlAction jonesConfig_aDefaultControlSet[JONESCONTROL_ACTION_NUMACTIONS] =
{
    { STD_ARRAYLEN(aActionWalkFwdKeys)    , aActionWalkFwdKeys     },
    { STD_ARRAYLEN(aActionWalkBackKeys)   , aActionWalkBackKeys    },
    { STD_ARRAYLEN(aActionTurnLeftKeys)   , aActionTurnLeftKeys    },
    { STD_ARRAYLEN(aActionTurnRightKeys)  , aActionTurnRightKeys   },
    { STD_ARRAYLEN(aActionStepLeftKeys)   , aActionStepLeftKeys    },
    { STD_ARRAYLEN(aActionStepRightKeys)  , aActionStepRightKeys   },
    { STD_ARRAYLEN(aActionCrawlKeys)      , aActionCrawlKeys       },
    { STD_ARRAYLEN(aActionRunKeys)        , aActionRunKeys         },
    { STD_ARRAYLEN(aActionRollKeys)       , aActionRollKeys        },
    { STD_ARRAYLEN(aActionJumpKeys)       , aActionJumpKeys        },
    { STD_ARRAYLEN(aActionLookKeys)       , aActionLookKeys        },
    { STD_ARRAYLEN(aActionActivateKeys)   , aActionActivateKeys    },
    { STD_ARRAYLEN(aActionToggleMapKeys)  , aActionToggleMapKeys   },
    { STD_ARRAYLEN(aActionPrevWeaponKeys) , aActionPrevWeaponKeys  },
    { STD_ARRAYLEN(aActionNextWeaponKeys) , aActionNextWeaponKeys  },
    { STD_ARRAYLEN(aActionToggleLightKeys), aActionToggleLightKeys },
    { STD_ARRAYLEN(aActionFistsKeys)      , aActionFistsKeys       },
    { STD_ARRAYLEN(aActionWhipKeys)       , aActionWhipKeys        },
    { STD_ARRAYLEN(aActionMauserKeys)     , aActionMauserKeys      },
    { STD_ARRAYLEN(aActionPPSH41Keys)     , aActionPPSH41Keys      },
    { STD_ARRAYLEN(aActionPistolKeys)     , aActionPistolKeys      },
    { STD_ARRAYLEN(aActionSimonovKeys)    , aActionSimonovKeys     },
    { STD_ARRAYLEN(aActionTokarevKeys)    , aActionTokarevKeys     },
    { STD_ARRAYLEN(aActionToz34Keys)      , aActionToz34Keys       },
    { STD_ARRAYLEN(aActionBazookaKeys)    , aActionBazookaKeys     },
    { STD_ARRAYLEN(aActionMacheteKeys)    , aActionMacheteKeys     },
    { STD_ARRAYLEN(aActionSatchelKeys)    , aActionSatchelKeys     },
    { STD_ARRAYLEN(aActionGrenadeKeys)    , aActionGrenadeKeys     },
    { STD_ARRAYLEN(aActionMapKeys)        , aActionMapKeys         },
    { STD_ARRAYLEN(aActionInterfaceKeys)  , aActionInterfaceKeys   },
    { STD_ARRAYLEN(aActionHealthKeys)     , aActionHealthKeys      },
    { STD_ARRAYLEN(aActionIMP1Keys)       , aActionIMP1Keys        },
    { STD_ARRAYLEN(aActionIMP2Keys)       , aActionIMP2Keys        },
    { STD_ARRAYLEN(aActionIMP3Keys)       , aActionIMP3Keys        },
    { STD_ARRAYLEN(aActionIMP4Keys)       , aActionIMP4Keys        },
    { STD_ARRAYLEN(aActionIMP5Keys)       , aActionIMP5Keys        },
    { STD_ARRAYLEN(aActionChalk)          , aActionChalk           }
};

// Joystick 2 Axis 4 buttons default keysets

static const size_t aActionCrawlJoyButtons[2]     = { DIK_C, STDCONTROL_JOYSTICK_GETBUTTON(0, 2) };
static const size_t aActionInterfaceJoyButtons[2] = { DIK_I, STDCONTROL_JOYSTICK_GETBUTTON(0, 3) };

static const JonesConfigControlAction jonesConfig_aDfault2Axis4Button[JONESCONTROL_ACTION_NUMACTIONS] =
{
    { STD_ARRAYLEN(aActionWalkFwdKeys)        , aActionWalkFwdKeys },
    { STD_ARRAYLEN(aActionWalkBackKeys)       , aActionWalkBackKeys },
    { STD_ARRAYLEN(aActionTurnLeftKeys)       , aActionTurnLeftKeys },
    { STD_ARRAYLEN(aActionTurnRightKeys)      , aActionTurnRightKeys },
    { STD_ARRAYLEN(aActionStepLeftKeys)       , aActionStepLeftKeys },
    { STD_ARRAYLEN(aActionStepRightKeys)      , aActionStepRightKeys },
    { STD_ARRAYLEN(aActionCrawlJoyButtons)    , aActionCrawlJoyButtons },
    { STD_ARRAYLEN(aActionRunKeys)            , aActionRunKeys },
    { STD_ARRAYLEN(aActionRollKeys)           , aActionRollKeys },
    { STD_ARRAYLEN(aActionJumpKeys)           , aActionJumpKeys },
    { STD_ARRAYLEN(aActionLookKeys)           , aActionLookKeys },
    { STD_ARRAYLEN(aActionActivateKeys)       , aActionActivateKeys },
    { STD_ARRAYLEN(aActionToggleMapKeys)      , aActionToggleMapKeys },
    { STD_ARRAYLEN(aActionPrevWeaponKeys)     , aActionPrevWeaponKeys },
    { STD_ARRAYLEN(aActionNextWeaponKeys)     , aActionNextWeaponKeys },
    { STD_ARRAYLEN(aActionToggleLightKeys)    , aActionToggleLightKeys },
    { STD_ARRAYLEN(aActionFistsKeys)          , aActionFistsKeys },
    { STD_ARRAYLEN(aActionWhipKeys)           , aActionWhipKeys },
    { STD_ARRAYLEN(aActionMauserKeys)         , aActionMauserKeys },
    { STD_ARRAYLEN(aActionPPSH41Keys)         , aActionPPSH41Keys },
    { STD_ARRAYLEN(aActionPistolKeys)         , aActionPistolKeys },
    { STD_ARRAYLEN(aActionSimonovKeys)        , aActionSimonovKeys },
    { STD_ARRAYLEN(aActionTokarevKeys)        , aActionTokarevKeys },
    { STD_ARRAYLEN(aActionToz34Keys)          , aActionToz34Keys },
    { STD_ARRAYLEN(aActionBazookaKeys)        , aActionBazookaKeys },
    { STD_ARRAYLEN(aActionMacheteKeys)        , aActionMacheteKeys },
    { STD_ARRAYLEN(aActionSatchelKeys)        , aActionSatchelKeys },
    { STD_ARRAYLEN(aActionGrenadeKeys)        , aActionGrenadeKeys },
    { STD_ARRAYLEN(aActionMapKeys)            , aActionMapKeys },
    { STD_ARRAYLEN(aActionInterfaceJoyButtons), aActionInterfaceJoyButtons },
    { STD_ARRAYLEN(aActionHealthKeys)         , aActionHealthKeys },
    { STD_ARRAYLEN(aActionIMP1Keys)           , aActionIMP1Keys },
    { STD_ARRAYLEN(aActionIMP2Keys)           , aActionIMP2Keys },
    { STD_ARRAYLEN(aActionIMP3Keys)           , aActionIMP3Keys },
    { STD_ARRAYLEN(aActionIMP4Keys)           , aActionIMP4Keys },
    { STD_ARRAYLEN(aActionIMP5Keys)           , aActionIMP5Keys },
    { STD_ARRAYLEN(aActionChalk)              , aActionChalk }
};

// Joystick 2 Axis 8 buttons default keysets

static const size_t aActionRollJoyButtons[2]         = { DIK_CAPITAL, STDCONTROL_JOYSTICK_GETBUTTON(0, 4) };
static const size_t aActionNextWeaponJoyButtons[2]   = { DIK_PERIOD, STDCONTROL_JOYSTICK_GETBUTTON(0, 5) };
static const size_t aActionToggleWeaponJoyButtons[2] = { DIK_SPACE, STDCONTROL_JOYSTICK_GETBUTTON(0, 6) };
static const size_t aActionLookJoyButtons[3]         = { DIK_INSERT, DIK_NUMPAD0, STDCONTROL_JOYSTICK_GETBUTTON(0, 7) };

static const JonesConfigControlAction jonesConfig_aDefault2Axis8Button[JONESCONTROL_ACTION_NUMACTIONS] =
{
    { STD_ARRAYLEN(aActionWalkFwdKeys)           , aActionWalkFwdKeys            },
    { STD_ARRAYLEN(aActionWalkBackKeys)          , aActionWalkBackKeys           },
    { STD_ARRAYLEN(aActionTurnLeftKeys)          , aActionTurnLeftKeys           },
    { STD_ARRAYLEN(aActionTurnRightKeys)         , aActionTurnRightKeys          },
    { STD_ARRAYLEN(aActionStepLeftKeys)          , aActionStepLeftKeys           },
    { STD_ARRAYLEN(aActionStepRightKeys)         , aActionStepRightKeys          },
    { STD_ARRAYLEN(aActionCrawlJoyButtons)       , aActionCrawlJoyButtons        },
    { STD_ARRAYLEN(aActionRunKeys)               , aActionRunKeys                },
    { STD_ARRAYLEN(aActionRollJoyButtons)        , aActionRollJoyButtons         },
    { STD_ARRAYLEN(aActionJumpKeys)              , aActionJumpKeys               },
    { STD_ARRAYLEN(aActionLookJoyButtons)        , aActionLookJoyButtons         },
    { STD_ARRAYLEN(aActionActivateKeys)          , aActionActivateKeys           },
    { STD_ARRAYLEN(aActionToggleWeaponJoyButtons), aActionToggleWeaponJoyButtons },
    { STD_ARRAYLEN(aActionPrevWeaponKeys)        , aActionPrevWeaponKeys         },
    { STD_ARRAYLEN(aActionNextWeaponJoyButtons)  , aActionNextWeaponJoyButtons   },
    { STD_ARRAYLEN(aActionToggleLightKeys)       , aActionToggleLightKeys        },
    { STD_ARRAYLEN(aActionFistsKeys)             , aActionFistsKeys              },
    { STD_ARRAYLEN(aActionWhipKeys)              , aActionWhipKeys               },
    { STD_ARRAYLEN(aActionMauserKeys)            , aActionMauserKeys             },
    { STD_ARRAYLEN(aActionPPSH41Keys)            , aActionPPSH41Keys             },
    { STD_ARRAYLEN(aActionPistolKeys)            , aActionPistolKeys             },
    { STD_ARRAYLEN(aActionSimonovKeys)           , aActionSimonovKeys            },
    { STD_ARRAYLEN(aActionTokarevKeys)           , aActionTokarevKeys            },
    { STD_ARRAYLEN(aActionToz34Keys)             , aActionToz34Keys              },
    { STD_ARRAYLEN(aActionBazookaKeys)           , aActionBazookaKeys            },
    { STD_ARRAYLEN(aActionMacheteKeys)           , aActionMacheteKeys            },
    { STD_ARRAYLEN(aActionSatchelKeys)           , aActionSatchelKeys            },
    { STD_ARRAYLEN(aActionGrenadeKeys)           , aActionGrenadeKeys            },
    { STD_ARRAYLEN(aActionMapKeys)               , aActionMapKeys                },
    { STD_ARRAYLEN(aActionInterfaceJoyButtons)   , aActionInterfaceJoyButtons    },
    { STD_ARRAYLEN(aActionHealthKeys)            , aActionHealthKeys             },
    { STD_ARRAYLEN(aActionIMP1Keys)              , aActionIMP1Keys               },
    { STD_ARRAYLEN(aActionIMP2Keys)              , aActionIMP2Keys               },
    { STD_ARRAYLEN(aActionIMP3Keys)              , aActionIMP3Keys               },
    { STD_ARRAYLEN(aActionIMP4Keys)              , aActionIMP4Keys               },
    { STD_ARRAYLEN(aActionIMP5Keys)              , aActionIMP5Keys               },
    { STD_ARRAYLEN(aActionChalk)                 , aActionChalk                  }
};

// Side Winder Gamepad default keysets found in debug version

static const size_t aActionToggleStepLeftSideWinderGamePadButtons[2]  = { DIK_LBRACKET, STDCONTROL_JOYSTICK_GETBUTTON(0, 6) };
static const size_t aActionToggleStepRightSideWinderGamePadButtons[2] = { DIK_RBRACKET, STDCONTROL_JOYSTICK_GETBUTTON(0, 7) };
static const size_t aActionToggleCrawlSideWinderGamePadButtons[2]     = { DIK_C       , STDCONTROL_JOYSTICK_GETBUTTON(0, 2) };
static const size_t aActionToggleRunSideWinderGamePadButtons[2]       = { DIK_CAPITAL , STDCONTROL_JOYSTICK_GETBUTTON(0, 3) };
static const size_t aActionLookSideWinderGamePadButtons[3]            = { DIK_INSERT  , DIK_NUMPAD0, STDCONTROL_JOYSTICK_GETBUTTON(0, 5) };
static const size_t aActionToggleWeaponSideWinderGamePadButtons[2]    = { DIK_SPACE   , STDCONTROL_JOYSTICK_GETBUTTON(0, 9) };
static const size_t aActionNextWeaponSideWinderGamePad[2]             = { DIK_PERIOD  , STDCONTROL_JOYSTICK_GETBUTTON(0, 4) };
static const size_t aActionToggleInterfaceSideWinderGamePadButtons[2] = { DIK_I       , STDCONTROL_JOYSTICK_GETBUTTON(0, 8) };

static const JonesConfigControlAction jonesConfig_aDefaultSideWinderGamepad[JONESCONTROL_ACTION_NUMACTIONS] =
{
    { STD_ARRAYLEN(aActionWalkFwdKeys)                            , aActionWalkFwdKeys    },
    { STD_ARRAYLEN(aActionWalkBackKeys)                           , aActionWalkBackKeys  },
    { STD_ARRAYLEN(aActionTurnLeftKeys)                           , aActionTurnLeftKeys  },
    { STD_ARRAYLEN(aActionTurnRightKeys)                          , aActionTurnRightKeys },
    { STD_ARRAYLEN(aActionToggleStepLeftSideWinderGamePadButtons) , aActionToggleStepLeftSideWinderGamePadButtons},
    { STD_ARRAYLEN(aActionToggleStepRightSideWinderGamePadButtons), aActionToggleStepRightSideWinderGamePadButtons},
    { STD_ARRAYLEN(aActionToggleCrawlSideWinderGamePadButtons)    , aActionToggleCrawlSideWinderGamePadButtons},
    { STD_ARRAYLEN(aActionToggleRunSideWinderGamePadButtons)      , aActionToggleRunSideWinderGamePadButtons},
    { STD_ARRAYLEN(aActionRollKeys)                               , aActionRollKeys      },
    { STD_ARRAYLEN(aActionJumpKeys)                               , aActionJumpKeys      },
    { STD_ARRAYLEN(aActionLookSideWinderGamePadButtons)           , aActionLookSideWinderGamePadButtons},
    { STD_ARRAYLEN(aActionActivateKeys)                           , aActionActivateKeys  },
    { STD_ARRAYLEN(aActionToggleWeaponSideWinderGamePadButtons)   , aActionToggleWeaponSideWinderGamePadButtons},
    { STD_ARRAYLEN(aActionPrevWeaponKeys)                         , aActionPrevWeaponKeys},
    { STD_ARRAYLEN(aActionNextWeaponSideWinderGamePad)            , aActionNextWeaponSideWinderGamePad},
    { STD_ARRAYLEN(aActionToggleLightKeys)                        , aActionToggleLightKeys},
    { STD_ARRAYLEN(aActionFistsKeys)                              , aActionFistsKeys    },
    { STD_ARRAYLEN(aActionWhipKeys)                               , aActionWhipKeys     },
    { STD_ARRAYLEN(aActionMauserKeys)                             , aActionMauserKeys   },
    { STD_ARRAYLEN(aActionPPSH41Keys)                             , aActionPPSH41Keys   },
    { STD_ARRAYLEN(aActionPistolKeys)                             , aActionPistolKeys   },
    { STD_ARRAYLEN(aActionSimonovKeys)                            , aActionSimonovKeys  },
    { STD_ARRAYLEN(aActionTokarevKeys)                            , aActionTokarevKeys  },
    { STD_ARRAYLEN(aActionToz34Keys)                              , aActionToz34Keys    },
    { STD_ARRAYLEN(aActionBazookaKeys)                            , aActionBazookaKeys  },
    { STD_ARRAYLEN(aActionMacheteKeys)                            , aActionMacheteKeys  },
    { STD_ARRAYLEN(aActionSatchelKeys)                            , aActionSatchelKeys  },
    { STD_ARRAYLEN(aActionGrenadeKeys)                            , aActionGrenadeKeys  },
    { STD_ARRAYLEN(aActionMapKeys)                                , aActionMapKeys      },
    { STD_ARRAYLEN(aActionToggleInterfaceSideWinderGamePadButtons), aActionToggleInterfaceSideWinderGamePadButtons},
    { STD_ARRAYLEN(aActionHealthKeys)                             , aActionHealthKeys   },
    { STD_ARRAYLEN(aActionIMP1Keys)                               , aActionIMP1Keys     },
    { STD_ARRAYLEN(aActionIMP2Keys)                               , aActionIMP2Keys     },
    { STD_ARRAYLEN(aActionIMP3Keys)                               , aActionIMP3Keys     },
    { STD_ARRAYLEN(aActionIMP4Keys)                               , aActionIMP4Keys     },
    { STD_ARRAYLEN(aActionIMP5Keys)                               , aActionIMP5Keys     },
    { STD_ARRAYLEN(aActionChalk)                                  , aActionChalk        }
};

// Default key sets
static const JonesConfigControlAction* jonesConfig_aDfltKeySets[] = {
    jonesConfig_aDefaultControlSet,
    jonesConfig_aDfault2Axis4Button,
    jonesConfig_aDefault2Axis8Button
};

static_assert(STD_ARRAYLEN(jonesConfig_aDfltKeySets) == JONESCONFIG_NUMDEFAULTCONTROLSCHEMES, "STD_ARRAYLEN(jonesConfig_aDfltKeySets) == JONESCONFIG_NUMDEFAULTCONTROLSCHEMES");

static const char* jonesConfig_aDfltKeySetNames[JONESCONFIG_NUMDEFAULTCONTROLSCHEMES] = {
    "JONES_STR_DFLTKEY",
    "JONES_STR_DFLT2A4B",
    "JONES_STR_DFLT2A8B"
};

// The action string names
static const char* jonesConfig_aJonesControlActionNames[JONESCONTROL_ACTION_NUMACTIONS] =
{
    "JONES_STR_WLKFWD",
    "JONES_STR_WLKBK",
    "JONES_STR_TRNLFT",
    "JONES_STR_TRNRGHT",
    "JONES_STR_STPLFT",
    "JONES_STR_STPRGHT",
    "JONES_STR_CRAWL",
    "JONES_STR_RUN",
    "JONES_STR_ROLL",
    "JONES_STR_JUMP",
    "JONES_STR_LOOK",
    "JONES_STR_ACT",
    "JONES_STR_TGLWEAP",
    "JONES_STR_PREVWEAP",
    "JONES_STR_NEXTWEAP",
    "JONES_STR_TGLLGHT",
    "JONES_STR_FISTS",
    "JONES_STR_WHIP",
    "JONES_STR_MAUSER",
    "JONES_STR_PPSH41",
    "JONES_STR_PISTOL",
    "JONES_STR_SIMONOV",
    "JONES_STR_TOKEREV",
    "JONES_STR_TOZ34",
    "JONES_STR_BAZOOKA",
    "JONES_STR_MACHETE",
    "JONES_STR_SATCHEL",
    "JONES_STR_GRENADE",
    "JONES_STR_MAP",
    "JONES_STR_INTERFACE",
    "JONES_STR_HEALTH",
    "JONES_STR_IMP1",
    "JONES_STR_IMP2",
    "JONES_STR_IMP3",
    "JONES_STR_IMP4",
    "JONES_STR_IMP5",
    "JONES_STR_CHALK"
};

static const char* jonesConfig_aJonesCapControlActionNames[JONESCONTROL_ACTION_NUMACTIONS] =
{
    "JONES_STR_CAPS_WLKFWD",
    "JONES_STR_CAPS_WLKBK",
    "JONES_STR_CAPS_TRNLFT",
    "JONES_STR_CAPS_TRNRGHT",
    "JONES_STR_CAPS_STPLFT",
    "JONES_STR_CAPS_STPRGHT",
    "JONES_STR_CAPS_CRAWL",
    "JONES_STR_CAPS_RUN",
    "JONES_STR_CAPS_ROLL",
    "JONES_STR_CAPS_JUMP",
    "JONES_STR_CAPS_LOOK",
    "JONES_STR_CAPS_ACT",
    "JONES_STR_CAPS_TGLWEAP",
    "JONES_STR_CAPS_PREVWEAP",
    "JONES_STR_CAPS_NEXTWEAP",
    "JONES_STR_CAPS_TGLLGHT",
    "JONES_STR_CAPS_FISTS",
    "JONES_STR_CAPS_WHIP",
    "JONES_STR_CAPS_MAUSER",
    "JONES_STR_CAPS_PPSH41",
    "JONES_STR_CAPS_PISTOL",
    "JONES_STR_CAPS_SIMONOV",
    "JONES_STR_CAPS_TOKEREV",
    "JONES_STR_CAPS_TOZ34",
    "JONES_STR_CAPS_BAZOOKA",
    "JONES_STR_CAPS_MACHETE",
    "JONES_STR_CAPS_SATCHEL",
    "JONES_STR_CAPS_GRENADE",
    "JONES_STR_CAPS_MAP",
    "JONES_STR_CAPS_INTERFACE",
    "JONES_STR_CAPS_HEALTH",
    "JONES_STR_CAPS_IMP1",
    "JONES_STR_CAPS_IMP2",
    "JONES_STR_CAPS_IMP3",
    "JONES_STR_CAPS_IMP4",
    "JONES_STR_CAPS_IMP5",
    "JONES_STR_CAPS_CHALK"
};


// Key control sting names
static const char* jonesConfig_aControlKeyStrings[223] =
{
    NULL,
    "JONES_STR_ESCAPE",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    "0",
    "JONES_STR_MINUS",
    "JONES_STR_EQUAL",
    "JONES_STR_BACK",
    "JONES_STR_TAB",
    "Q",
    "W",
    "E",
    "R",
    "T",
    "Y",
    "U",
    "I",
    "O",
    "P",
    "JONES_STR_LEFT_BRCKT",
    "JONES_STR_RIGHT_BRCKT",
    "JONES_STR_ENTER",
    "JONES_STR_LEFT_CTRL",
    "A",
    "S",
    "D",
    "F",
    "G",
    "H",
    "J",
    "K",
    "L",
    ";",
    "'",
    "`",
    "JONES_STR_LEFT_SHIFT",
    "\\",
    "Z",
    "X",
    "C",
    "V",
    "B",
    "N",
    "M",
    "JONES_STR_COMMA",
    ".",
    "/",
    "JONES_STR_RIGHT_SHIFT",
    "JONES_STR_NUMPAD_MULT",
    "JONES_STR_LEFT_ALT",
    "JONES_STR_SPACE",
    "JONES_STR_CAPS",
    "JONES_STR_F1",
    "JONES_STR_F2",
    "JONES_STR_F3",
    "JONES_STR_F4",
    "JONES_STR_F5",
    "JONES_STR_F6",
    "JONES_STR_F7",
    "JONES_STR_F8",
    "JONES_STR_F9",
    "JONES_STR_F10",
    "JONES_STR_NUMLOCK",
    "JONES_STR_SCROLL",
    "JONES_STR_NUMPAD_7",
    "JONES_STR_NUMPAD_8",
    "JONES_STR_NUMPAD_9",
    "JONES_STR_NUMPAD_MINUS",
    "JONES_STR_NUMPAD_4",
    "JONES_STR_NUMPAD_5",
    "JONES_STR_NUMPAD_6",
    "JONES_STR_NUMPAD_PLUS",
    "JONES_STR_NUMPAD_1",
    "JONES_STR_NUMPAD_2",
    "JONES_STR_NUMPAD_3",
    "JONES_STR_NUMPAD_0",
    "JONES_STR_NUMPAD_DOT",
    NULL,
    NULL,
    NULL,
    "JONES_STR_F11",
    "JONES_STR_F12",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "JONES_STR_F13",
    "JONES_STR_F14",
    "JONES_STR_F15",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "JONES_STR_NUMPAD_EQL",
    NULL,
    NULL,
    NULL,
    "@",
    ":",
    "__",
    NULL,
    "STOP",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "JONES_STR_NUMPAD_ENTER",
    "JONES_STR_RIGHT_CTRL",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "JONES_STR_NUMPAD_COMMA",
    NULL,
    "JONES_STR_NUMPAD_DIV",
    NULL,
    "JONES_STR_SYSRQ",
    "JONES_STR_RIGHT_ALT",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "JONES_STR_HOME",
    "JONES_STR_UPARRW",
    "JONES_STR_PAGEUP",
    NULL,
    "JONES_STR_LEFTARRW",
    NULL,
    "JONES_STR_RGHTARRW",
    NULL,
    "JONES_STR_END",
    "JONES_STR_DNARRW",
    "JONES_STR_PGDN",
    "JONES_STR_INSERT",
    "JONES_STR_DELETE",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "JONES_STR_LEFT_WND",
    "JONES_STR_RIGHT_WND",
    "JONES_STR_APPS",
    NULL
};


int jonesConfig_InitKeySetsPath(void);
int jonesConfig_InitKeyActions(void);

void J3DAPI jonesConfig_ControlToString(size_t controlId, char* pDest);

JonesControlsScheme* jonesConfig_LoadActiveControlScheme(void);
int J3DAPI jonesConfig_LoadControlScheme(const char* pFilePath, JonesControlsScheme* pConfig);
int J3DAPI jonesConfig_ParseArgumentIndex(size_t* argNum, const StdConffileEntry* pEntry, size_t a3, const char** apStringList, size_t size);
int J3DAPI jonesConfig_GetStringListIndex(char* pValue, const char** apStringList, size_t size);
int J3DAPI jonesConfig_ParseJoystickControlId(size_t* pArgNum, const StdConffileEntry* pEntry);
int J3DAPI jonesConfig_SetDefaultControlScheme(JonesControlsScheme* pScheme, size_t num);
void J3DAPI jonesConfig_BindControls(JonesControlsScheme* pConfig);
void J3DAPI jonesConfig_BindJoystickControl(SithControlFunction functionId, size_t controlId);
void J3DAPI jonesConfig_FreeControlScheme(JonesControlsScheme* pScheme);
void J3DAPI jonesConfig_FreeControlConfigEntry(JonesControlsConfig* pConfig);







int J3DAPI jonesConfig_AssignKey_CheckBindForKey(JonesControlsScheme* pScheme, size_t keyControlID, size_t* pFunctionId, int listID, size_t* pOffset);





int J3DAPI jonesConfig_CopyControlConfig(JonesControlsConfig* pSrc, JonesControlsConfig* pDst);
JonesControlsScheme* J3DAPI jonesConfig_CloneControlSchemesList(JonesControlsScheme* aSchemes, size_t start, size_t numSchemes);

int jonesConfig_ControlOptions_InitControlsConfig(void);
int J3DAPI jonesConfig_ControlOptions_LoadAllControlSchemes(JonesControlsConfig* pConfig);
int J3DAPI jonesConfig_ControlOptions_SetAllDefaultControlSchemes(JonesControlsConfig* pConfig);
int J3DAPI jonesConfig_ControlOptions_WriteScheme(JonesControlsScheme* pScheme);
JonesControlsScheme* J3DAPI jonesConfig_GetDefaultControlScheme(JonesControlsConfig* pConfig);


int J3DAPI jonesConfig_DisplaySettings_Get3DDeviceSupportsBPP(const StdDisplayInfo* pDisplayInfo, const JonesDisplaySettings* pSettings, int bpp);


void J3DAPI jonesConfig_GetAutoSaveFilePath(char* pPath, size_t size)
{
    memset(pPath, 0, size);
    if ( sithWorld_g_pCurrentWorld )
    {
        const char* pNdsFilename = sithGetCurrentWorldSaveName();
        const char* pFilePrefix = sithGetAutoSaveFilePrefix();

        char aFilename[128] = { 0 };
        STD_FORMAT(aFilename, "%s%s", pFilePrefix, pNdsFilename);
        stdFnames_ChangeExt(aFilename, "nds");

        const char* pDirPath = sithGetSaveGamesDir();
        LPSTR pFilePart;
        SearchPath(pDirPath, aFilename, NULL, size, pPath, &pFilePart);
    }
}







void jonesConfig_InstallHooks(void)
{
    J3D_HOOKFUNC(jonesConfig_Startup);
    J3D_HOOKFUNC(jonesConfig_InitKeySetsPath);
    J3D_HOOKFUNC(jonesConfig_InitKeyActions);
    J3D_HOOKFUNC(jonesConfig_Shutdown);
    J3D_HOOKFUNC(jonesConfig_ControlToString);
    J3D_HOOKFUNC(jonesConfig_LoadActiveControlScheme);
    J3D_HOOKFUNC(jonesConfig_LoadControlScheme);
    J3D_HOOKFUNC(jonesConfig_ParseArgumentIndex);
    J3D_HOOKFUNC(jonesConfig_GetStringListIndex);
    J3D_HOOKFUNC(jonesConfig_ParseJoystickControlId);
    J3D_HOOKFUNC(jonesConfig_SetDefaultControlScheme);
    J3D_HOOKFUNC(jonesConfig_BindControls);
    J3D_HOOKFUNC(jonesConfig_BindJoystickControl);
    J3D_HOOKFUNC(jonesConfig_EnableMouseControl);
    J3D_HOOKFUNC(jonesConfig_FreeControlScheme);
    J3D_HOOKFUNC(jonesConfig_FreeControlConfigEntry);
    J3D_HOOKFUNC(jonesConfig_CopyControlConfig);
    J3D_HOOKFUNC(jonesConfig_CloneControlSchemesList);
    J3D_HOOKFUNC(jonesConfig_ControlOptions_InitControlsConfig);
    J3D_HOOKFUNC(jonesConfig_ControlOptions_LoadAllControlSchemes);
    J3D_HOOKFUNC(jonesConfig_ControlOptions_SetAllDefaultControlSchemes);
    J3D_HOOKFUNC(jonesConfig_ControlOptions_WriteScheme);
    J3D_HOOKFUNC(jonesConfig_GetDefaultControlScheme);
    J3D_HOOKFUNC(jonesConfig_AssignKey_CheckBindForKey);
    J3D_HOOKFUNC(jonesConfig_DisplaySettings_Get3DDeviceSupportsBPP);
}

int jonesConfig_Startup(void)
{
    if ( jonesConfig_bStartup )
    {
        return 1;
    }

    if ( !jonesConfig_InitKeySetsPath() )
    {
        return 0;
    }

    if ( !jonesConfig_InitKeyActions() )
    {
        return 0;
    }

    jonesConfig_gamePlayOptions_bDefaultRun = wuRegistry_GetIntEx("Default Run", 0);

    if ( jonesConfig_gamePlayOptions_bDefaultRun )
    {
        sithControl_g_controlOptions |= 2u; // run flag
    }
    else
    {
        sithControl_g_controlOptions &= ~2;
    }

    // Init cur level num
    jonesConfig_gameStatistics_curLevelNum = 0;

    JonesControlsScheme* pConfig = jonesConfig_LoadActiveControlScheme();
    if ( pConfig )
    {
        jonesConfig_BindControls(pConfig);
        jonesConfig_FreeControlScheme(pConfig);
    }

    jonesConfig_bStartup = 1;
    return 1;
}

int jonesConfig_InitKeySetsPath(void)
{
    wchar_t* pwKeySets = sithString_GetString("SITHSTRING_KEYSETS");
    if ( !pwKeySets )
    {
        return 0;
    }

    char* pKeySets = stdUtil_ToAString(pwKeySets);

    char aInstallPath[128] = { 0 };
    wuRegistry_GetStr("Install Path", aInstallPath, STD_ARRAYLEN(aInstallPath), "");

    if ( pKeySets )
    {
        size_t len = strlen(aInstallPath) + 1;
        if ( len == 1 )
        {
            STD_FORMAT(jonesConfig_aKeySetsDirPath, "..\\\\%s", pKeySets);
            stdMemory_Free(pKeySets);
            return 1;
        }
        else
        {
            if ( aInstallPath[len - 1] == '\\' )
            {
                STD_FORMAT(jonesConfig_aKeySetsDirPath, "%s%s", aInstallPath, pKeySets);
            }
            else
            {
                STD_FORMAT(jonesConfig_aKeySetsDirPath, "%s\\%s", aInstallPath, pKeySets);
            }

            stdMemory_Free(pKeySets);
            return 1;
        }
    }
    else
    {
        STDLOG_ERROR("Couldn't find string %s in lookup table.\n", 0); // TODO: [BUG] zero passed as %s
        stdMemory_Free(pwKeySets); // pwKeySets is null value but is allocated on heap in sithString_GetString
        return 0;
    }
}

int jonesConfig_InitKeyActions(void)
{
    jonesConfig_maxActionWords  = 0;
    jonesConfig_maxActionStrLen = 0;

    jonesConfig_maxControlWords  = 0; // Fixed: Assign 0
    jonesConfig_maxControlStrLen = 0; // Fixed: Assign 0

    for ( size_t i = 0; i < STD_ARRAYLEN(jonesConfig_aJonesCapControlActionNames); ++i )
    {
        const char* pActionName = jonesString_GetString(jonesConfig_aJonesCapControlActionNames[i]);
        if ( !pActionName )
        {
            return 0;
        }

        char aActionName[512] = { 0 }; // Fixed: Init. to 0
        STD_STRCPY(aActionName, pActionName);

        size_t len = 0;
        char* pCurActionWord = aActionName;
        while ( pCurActionWord )
        {
            pCurActionWord = strchr(pCurActionWord, ' ');
            if ( pCurActionWord )
            {
                ++pCurActionWord;
            }

            ++len;
        }

        if ( len > jonesConfig_maxActionWords )
        {
            jonesConfig_maxActionWords = len;
        }

        len = strlen(aActionName);
        if ( len > jonesConfig_maxActionStrLen )
        {
            jonesConfig_maxActionStrLen = len;
        }
    }

    for ( size_t i = 0; i < STDCONTROL_MAX_KEYID + JONESCONFIG_NUM_JOYAXISKIDS + 4; ++i ) // 656 =  STDCONTROL_MAX_KEYID + JONESCONFIG_NUM_JOYAXISKIDS + 4
    {
        char aControlStr[JONESCONFIG_CONTROLSTRING_MAXLEN] = { 0 };
        switch ( i )
        {
            case 644:
                jonesConfig_ControlToString(JONESCONFIG_JOYAXISKID_UP, aControlStr);
                break;

            case 645:
                jonesConfig_ControlToString(JONESCONFIG_JOYAXISKID_DOWN, aControlStr);
                break;

            case 646:
                jonesConfig_ControlToString(JONESCONFIG_JOYAXISKID_LEFT, aControlStr);
                break;

            case 647:
                jonesConfig_ControlToString(JONESCONFIG_JOYAXISKID_RIGHT, aControlStr);
                break;

            case 648:
                jonesConfig_ControlToString(JONESCONFIG_JOYAXISKID_AIR, aControlStr);
                break;

            case 649:
                jonesConfig_ControlToString(JONESCONFIG_JOYAXISKID_GROUND, aControlStr);
                break;

            case 650:
                jonesConfig_ControlToString(JONESCONFIG_JOYAXISKID_TWISTL, aControlStr);
                break;

            case 651:
                jonesConfig_ControlToString(JONESCONFIG_JOYAXISKID_TWISTR, aControlStr);
                break;

            default:
                jonesConfig_ControlToString(i, aControlStr);
                break;
        }

        size_t len = 0;
        char* pCurControlWord = aControlStr;
        while ( pCurControlWord )
        {
            pCurControlWord = strchr(pCurControlWord, ' ');
            if ( pCurControlWord )
            {
                ++pCurControlWord;
            }

            ++len;
        }

        if ( len > jonesConfig_maxControlWords )
        {
            jonesConfig_maxControlWords = len;
        }

        len = strlen(aControlStr);
        if ( len > jonesConfig_maxControlStrLen )
        {
            jonesConfig_maxControlStrLen = len;
        }
    }

    jonesConfig_maxActionPaddedStrLen  = 5 - jonesConfig_maxActionStrLen % 5 + jonesConfig_maxActionStrLen;
    jonesConfig_maxControlPaddedStrLen = 5 - jonesConfig_maxControlStrLen % 5 + jonesConfig_maxControlStrLen;
    return 1;
}

void jonesConfig_Shutdown(void)
{
    jonesConfig_FreeControlConfigEntry(&jonesConfig_controlOptions_curControlConfig);

    if ( *jonesConfig_aKeySetsDirPath )
    {
        memset(jonesConfig_aKeySetsDirPath, 0, sizeof(jonesConfig_aKeySetsDirPath));
    }
}

void J3DAPI jonesConfig_ControlToString(size_t controlId, char* pDest)
{
    if ( !pDest )
    {
        // TODO: maybe log?
        return;
    }

    memset(pDest, 0, JONESCONFIG_CONTROLSTRING_MAXLEN);

    char* pJonesString = NULL;
    if ( controlId < JONESCONFIG_JOYAXISKID_UP )
    {
        switch ( controlId )
        {
            case DIK_ESCAPE:
                pJonesString = "JONES_STR_ESCAPE";
                break;

            case DIK_1:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "1");
                break;

            case DIK_2:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "2");
                break;

            case DIK_3:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "3");
                break;

            case DIK_4:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "4");
                break;

            case DIK_5:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "5");
                break;

            case DIK_6:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "6");
                break;

            case DIK_7:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "7");
                break;

            case DIK_8:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "8");
                break;

            case DIK_9:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "9");
                break;

            case DIK_0:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "0");
                break;

            case DIK_MINUS:
                pJonesString = "JONES_STR_MINUS";
                break;

            case DIK_EQUALS:
                pJonesString = "JONES_STR_EQUAL";
                break;

            case DIK_BACK:
                pJonesString = "JONES_STR_BACK";
                break;

            case DIK_TAB:
                pJonesString = "JONES_STR_TAB";
                break;

            case DIK_Q:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "Q");
                break;

            case DIK_W:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "W");
                break;

            case DIK_E:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "E");
                break;

            case DIK_R:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "R");
                break;

            case DIK_T:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "T");
                break;

            case DIK_Y:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "Y");
                break;

            case DIK_U:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "U");
                break;

            case DIK_I:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "I");
                break;

            case DIK_O:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "O");
                break;

            case DIK_P:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "P");
                break;

            case DIK_LBRACKET:
                pJonesString = "JONES_STR_LEFT_BRCKT";
                break;

            case DIK_RBRACKET:
                pJonesString = "JONES_STR_RIGHT_BRCKT";
                break;

            case DIK_RETURN:
                pJonesString = "JONES_STR_ENTER";
                break;

            case DIK_LCONTROL:
                pJonesString = "JONES_STR_LEFT_CTRL";
                break;

            case DIK_A:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "A");
                break;

            case DIK_S:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "S");
                break;

            case DIK_D:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "D");
                break;

            case DIK_F:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "F");
                break;

            case DIK_G:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "G");
                break;

            case DIK_H:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "H");
                break;

            case DIK_J:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "J");
                break;

            case DIK_K:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "K");
                break;

            case DIK_L:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "L");
                break;

            case DIK_SEMICOLON:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, ";");
                break;

            case DIK_APOSTROPHE:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "'");
                break;

            case DIK_GRAVE:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "`");
                break;

            case DIK_LSHIFT:
                pJonesString = "JONES_STR_LEFT_SHIFT";
                break;

            case DIK_BACKSLASH:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "\\");
                break;

            case DIK_Z:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "Z");
                break;

            case DIK_X:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "X");
                break;

            case DIK_C:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "C");
                break;

            case DIK_V:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "V");
                break;

            case DIK_B:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "B");
                break;

            case DIK_N:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "N");
                break;

            case DIK_M:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "M");
                break;

            case DIK_COMMA:
                pJonesString = "JONES_STR_COMMA";
                break;

            case DIK_PERIOD:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, ".");
                break;

            case DIK_SLASH:
                stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, "/");
                break;

            case DIK_RSHIFT:
                pJonesString = "JONES_STR_RIGHT_SHIFT";
                break;

            case DIK_MULTIPLY:
                pJonesString = "JONES_STR_NUMPAD_MULT";
                break;

            case DIK_LMENU:
                pJonesString = "JONES_STR_LEFT_ALT";
                break;

            case DIK_SPACE:
                pJonesString = "JONES_STR_SPACE";
                break;

            case DIK_CAPITAL:
                pJonesString = "JONES_STR_CAPS";
                break;

            case DIK_F1:
                pJonesString = "JONES_STR_F1";
                break;

            case DIK_F2:
                pJonesString = "JONES_STR_F2";
                break;

            case DIK_F3:
                pJonesString = "JONES_STR_F3";
                break;

            case DIK_F4:
                pJonesString = "JONES_STR_F4";
                break;

            case DIK_F5:
                pJonesString = "JONES_STR_F5";
                break;

            case DIK_F6:
                pJonesString = "JONES_STR_F6";
                break;

            case DIK_F7:
                pJonesString = "JONES_STR_F7";
                break;

            case DIK_F8:
                pJonesString = "JONES_STR_F8";
                break;

            case DIK_F9:
                pJonesString = "JONES_STR_F9";
                break;

            case DIK_F10:
                pJonesString = "JONES_STR_F10";
                break;

            case DIK_NUMLOCK:
                pJonesString = "JONES_STR_NUMLOCK";
                break;

            case DIK_NUMPAD7:
                pJonesString = "JONES_STR_NUMPAD_7";
                break;

            case DIK_NUMPAD8:
                pJonesString = "JONES_STR_NUMPAD_8";
                break;

            case DIK_NUMPAD9:
                pJonesString = "JONES_STR_NUMPAD_9";
                break;

            case DIK_SUBTRACT:
                pJonesString = "JONES_STR_NUMPAD_MINUS";
                break;

            case DIK_NUMPAD4:
                pJonesString = "JONES_STR_NUMPAD_4";
                break;

            case DIK_NUMPAD5:
                pJonesString = "JONES_STR_NUMPAD_5";
                break;

            case DIK_NUMPAD6:
                pJonesString = "JONES_STR_NUMPAD_6";
                break;

            case DIK_ADD:
                pJonesString = "JONES_STR_NUMPAD_PLUS";
                break;

            case DIK_NUMPAD1:
                pJonesString = "JONES_STR_NUMPAD_1";
                break;

            case DIK_NUMPAD2:
                pJonesString = "JONES_STR_NUMPAD_2";
                break;

            case DIK_NUMPAD3:
                pJonesString = "JONES_STR_NUMPAD_3";
                break;

            case DIK_NUMPAD0:
                pJonesString = "JONES_STR_NUMPAD_0";
                break;

            case DIK_DECIMAL:
                pJonesString = "JONES_STR_NUMPAD_DOT";
                break;

            case DIK_F11:
                pJonesString = "JONES_STR_F11";
                break;

            case DIK_F12:
                pJonesString = "JONES_STR_F12";
                break;

            case DIK_F13:
                pJonesString = "JONES_STR_F13";
                break;

            case DIK_F14:
                pJonesString = "JONES_STR_F14";
                break;

            case DIK_F15:
                pJonesString = "JONES_STR_F15";
                break;

            case DIK_NUMPADEQUALS:
                pJonesString = "JONES_STR_NUMPAD_EQL";
                break;

            case DIK_NUMPADENTER:
                pJonesString = "JONES_STR_NUMPAD_ENTER";
                break;

            case DIK_RCONTROL:
                pJonesString = "JONES_STR_RIGHT_CTRL";
                break;

            case DIK_NUMPADCOMMA:
                pJonesString = "JONES_STR_NUMPAD_COMMA";
                break;

            case DIK_DIVIDE:
                pJonesString = "JONES_STR_NUMPAD_DIV";
                break;

            case DIK_RMENU:
                pJonesString = "JONES_STR_RIGHT_ALT";
                break;

            case DIK_HOME:
                pJonesString = "JONES_STR_HOME";
                break;

            case DIK_UP:
                pJonesString = "JONES_STR_UPARRW";
                break;

            case DIK_PRIOR:
                pJonesString = "JONES_STR_PAGEUP";
                break;

            case DIK_LEFT:
                pJonesString = "JONES_STR_LEFTARRW";
                break;

            case DIK_RIGHT:
                pJonesString = "JONES_STR_RGHTARRW";
                break;

            case DIK_END:
                pJonesString = "JONES_STR_END";
                break;

            case DIK_DOWN:
                pJonesString = "JONES_STR_DNARRW";
                break;

            case DIK_NEXT:
                pJonesString = "JONES_STR_PGDN";
                break;

            case DIK_INSERT:
                pJonesString = "JONES_STR_INSERT";
                break;

            case DIK_DELETE:
                pJonesString = "JONES_STR_DELETE";
                break;

            case DIK_LWIN:
                pJonesString = "JONES_STR_LEFT_WND";
                break;

            case DIK_RWIN:
                pJonesString = "JONES_STR_RIGHT_WND";
                break;

            default:
            {
            joystick_controls:
                if ( controlId < STDCONTROL_JOYSTICK_FIRTPOVCID || controlId > STDCONTROL_JOYSTICK_LASTPOVCID || STDCONTROL_JOYSTICK_GETPOVDIRECTIONINDEX(controlId) != 0 )
                {
                    if ( controlId >= STDCONTROL_JOYSTICK_FIRTPOVCID && controlId <= STDCONTROL_JOYSTICK_LASTPOVCID && STDCONTROL_JOYSTICK_GETPOVDIRECTIONINDEX(controlId) == 2 )
                    {
                        pJonesString = "JONES_STR_HATRIGHT";
                    }
                    else if ( controlId >= STDCONTROL_JOYSTICK_FIRTPOVCID && controlId <= STDCONTROL_JOYSTICK_LASTPOVCID && STDCONTROL_JOYSTICK_GETPOVDIRECTIONINDEX(controlId) == 3 )
                    {
                        pJonesString = "JONES_STR_HATDOWN";
                    }
                    else if ( controlId >= STDCONTROL_JOYSTICK_FIRTPOVCID && controlId <= STDCONTROL_JOYSTICK_LASTPOVCID && STDCONTROL_JOYSTICK_GETPOVDIRECTIONINDEX(controlId) == 1 )
                    {
                        pJonesString = "JONES_STR_HATUP";
                    }
                    else if ( controlId >= STDCONTROL_JOYSTICK_FIRSTKID && controlId <= STDCONTROL_JOYSTICK_LASTPOVCID )
                    {
                        pJonesString = "JONES_STR_JOYSTICK";
                    }
                }
                else
                {
                    // Index 0
                    pJonesString = "JONES_STR_HATLEFT";
                }
            } break;
        }
    }
    else
    {
        switch ( controlId )
        {
            case JONESCONFIG_JOYAXISKID_UP:
                pJonesString = "JONES_STR_JOYUP";
                break;

            case JONESCONFIG_JOYAXISKID_RIGHT:
                pJonesString = "JONES_STR_JOYRIGHT";
                break;

            case JONESCONFIG_JOYAXISKID_DOWN:
                pJonesString = "JONES_STR_JOYDOWN";
                break;

            case JONESCONFIG_JOYAXISKID_LEFT:
                pJonesString = "JONES_STR_JOYLEFT";
                break;

            case JONESCONFIG_JOYAXISKID_TWISTL:
                pJonesString = "JONES_STR_JOYTWISTL";
                break;

            case JONESCONFIG_JOYAXISKID_TWISTR:
                pJonesString = "JONES_STR_JOYTWISTR";
                break;

            // Note missing handling of JONESCONFIG_JOYAXISKID_AIR and JONESCONFIG_JOYAXISKID_GROUND

            default:
                goto joystick_controls;
        }
    }

    if ( pJonesString )
    {
        const char* pControlStr = jonesString_GetString(pJonesString);
        if ( pControlStr )
        {
            if ( controlId < STDCONTROL_MAX_KEYBOARD_BUTTONS
                || controlId > STDCONTROL_JOYSTICK_LASTPOVCID
                || controlId >= STDCONTROL_JOYSTICK_FIRTPOVCID && STDCONTROL_JOYSTICK_GETPOVDIRECTIONINDEX(controlId) == STDCONTROL_JOYSTICK_POVDIRLEFT
                || controlId >= STDCONTROL_JOYSTICK_FIRTPOVCID && STDCONTROL_JOYSTICK_GETPOVDIRECTIONINDEX(controlId) == STDCONTROL_JOYSTICK_POVDIRUP
                || controlId >= STDCONTROL_JOYSTICK_FIRTPOVCID && STDCONTROL_JOYSTICK_GETPOVDIRECTIONINDEX(controlId) == STDCONTROL_JOYSTICK_POVDIRRIGHT
                || controlId >= STDCONTROL_JOYSTICK_FIRTPOVCID && STDCONTROL_JOYSTICK_GETPOVDIRECTIONINDEX(controlId) == STDCONTROL_JOYSTICK_POVDIRDOWN )
            {
                if ( controlId >= STDCONTROL_JOYSTICK_FIRTPOVCID && controlId <= STDCONTROL_JOYSTICK_LASTPOVCID && STDCONTROL_JOYSTICK_GETPOVDIRECTIONINDEX(controlId) == STDCONTROL_JOYSTICK_POVDIRLEFT
                    || controlId >= STDCONTROL_JOYSTICK_FIRTPOVCID && controlId <= STDCONTROL_JOYSTICK_LASTPOVCID && STDCONTROL_JOYSTICK_GETPOVDIRECTIONINDEX(controlId) == STDCONTROL_JOYSTICK_POVDIRUP
                    || controlId >= STDCONTROL_JOYSTICK_FIRTPOVCID && controlId <= STDCONTROL_JOYSTICK_LASTPOVCID && STDCONTROL_JOYSTICK_GETPOVDIRECTIONINDEX(controlId) == STDCONTROL_JOYSTICK_POVDIRRIGHT
                    || controlId >= STDCONTROL_JOYSTICK_FIRTPOVCID && controlId <= STDCONTROL_JOYSTICK_LASTPOVCID && STDCONTROL_JOYSTICK_GETPOVDIRECTIONINDEX(controlId) == STDCONTROL_JOYSTICK_POVDIRDOWN )
                {
                    // Joystick POV 
                    stdUtil_Format(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, pControlStr, STDCONTROL_JOYSTICK_GETPOVINDEX(controlId) + 1);
                }
                else
                {
                    // Regular key or joystick stick move
                    stdUtil_StringCopy(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, pControlStr);
                }
            }
            else
            {
                // Joystick button
                stdUtil_Format(pDest, JONESCONFIG_CONTROLSTRING_MAXLEN, pControlStr, STDCONTROL_JOYSTICK_GETBUTTONINDEX(controlId) + 1);
            }
        }
    }
}

JonesControlsScheme* jonesConfig_LoadActiveControlScheme(void)
{
    JonesControlsScheme* pScheme = (JonesControlsScheme*)STDMALLOC(sizeof(JonesControlsScheme));
    if ( !pScheme )
    {
        return NULL;
    }

    memset(pScheme, 0, sizeof(JonesControlsScheme));

    char aPath[128] = { 0 };

    // Find active control scheme in system
    char aFilename[128] = { 0 };
    wuRegistry_GetStr("Configuration", aFilename, STD_ARRAYLEN(aFilename), "");
    if ( strlen(aFilename) )
    {
        // Load config scheme to new scheme
        char aFilePath[128] = { 0 };
        STD_FORMAT(aFilePath, "%s\\%s%s", jonesConfig_aKeySetsDirPath, aFilename, ".kfg");

        LPSTR pFilePart;
        SearchPath(jonesConfig_aKeySetsDirPath, aFilename, ".kfg", STD_ARRAYLEN(aPath), aPath, &pFilePart);
        if ( strlen(aPath) && jonesConfig_LoadControlScheme(aFilePath, pScheme) )
        {
            return pScheme;
        }

        // Load failed, try to make new scheme to default scheme based on config scheme name
        for ( size_t i = 0; i < STD_ARRAYLEN(jonesConfig_aDfltKeySetNames); i++ )
        {
            const char* pDfltKeyset = jonesString_GetString(jonesConfig_aDfltKeySetNames[i]);
            if ( !pDfltKeyset )
            {
                goto error;
            }

            STD_STRCPY(pScheme->aName, pDfltKeyset);

            if ( streq(aFilename, pDfltKeyset) )
            {
                if ( strlen(aPath) != 0 && jonesConfig_SetDefaultControlScheme(pScheme, i) )
                {
                    return pScheme;
                }
                else
                {
                    // Fixed: Fixed potential infinitive loop, when aPath len == 0 or jonesConfig_SetDefaultControlScheme fails
                    break;
                }
            }
        }

        // Clear keyset config 
        wuRegistry_SaveStr("Configuration", "");
        goto error;
    }

    // TODO: Hmm why is this check here? Should be already handled in above scope.
    if ( strlen(aFilename) && strlen(aPath) )
    {
        return pScheme;
    }

    // Ok we have no scheme configured, let's load default
    size_t schemeIdx = 0;
    size_t maxJoyButtons = stdControl_GetMaxJoystickButtons();
    if ( maxJoyButtons == 4 )
    {
        schemeIdx = 1;
    }
    else if ( maxJoyButtons == 8 )
    {
        schemeIdx = 2;
    }

    const char* pDfltKeyset = jonesString_GetString(jonesConfig_aDfltKeySetNames[schemeIdx]);
    if ( pDfltKeyset )
    {
        STD_STRCPY(pScheme->aName, pDfltKeyset);
        if ( jonesConfig_SetDefaultControlScheme(pScheme, schemeIdx) )
        {
            wuRegistry_SaveStr("Configuration", pDfltKeyset);
            return pScheme;
        }
    }

error:
    jonesConfig_FreeControlScheme(pScheme);
    return NULL;
}

int J3DAPI jonesConfig_LoadControlScheme(const char* pFilePath, JonesControlsScheme* pConfig)
{
     // Function loads kfg file to pData

     // Altered: Moved this check before stdConffile_Open
    if ( !pConfig || !pFilePath )
    {
        return 0;
    }

    if ( !stdConffile_Open(pFilePath) )
    {
        return 0;
    }

    memset(pConfig->aName, 0, sizeof(pConfig->aName));
    STD_STRNCPY(pConfig->aName, pFilePath, strlen(pFilePath) - 4);// -4 removes .kfg extension

    size_t bindNum = 0;

    stdConffile_ReadArgs(); // Skip first comment line
    stdConffile_ReadArgs();

    if ( stdConffile_g_entry.numArgs && strneqi(stdConffile_g_entry.aArgs[0].argName, "1", 128u) )// 1 mark scheme is enabled
    {
        stdConffile_ReadArgs(); // Skip header line

        size_t actionCount = 0;

        for ( size_t i = 0; i < STD_ARRAYLEN(pConfig->aActions); ++i )
        {
            size_t argNum = 0;
            if ( !stdConffile_ReadArgs() )
            {
                break;
            }

            int actionIdx = jonesConfig_ParseArgumentIndex(&argNum, &stdConffile_g_entry, jonesConfig_maxActionWords, jonesConfig_aJonesControlActionNames, STD_ARRAYLEN(jonesConfig_aJonesControlActionNames));
            if ( actionIdx == -1 )
            {
                break;
            }

            bindNum = 1; // Note, at index 0 the num of bindings state is stored
            pConfig->aActions[actionIdx][0] = 0; // Reset number of bindings state

            // Removed: As paActionBindings was pointer to fixed size array
            //int (*paActionBindings)[9] = &pScheme->aActions[actionIdx];
            //if ( paActionBindings )

            while ( 1 )
            {
                if ( argNum >= stdConffile_g_entry.numArgs )
                {
                    break;
                }

                uint8_t bController = 0;
                uint8_t bKeyboard = 0;

                int keyIdx = jonesConfig_ParseArgumentIndex(&argNum, &stdConffile_g_entry, jonesConfig_maxControlWords, jonesConfig_aControlKeyStrings, STD_ARRAYLEN(jonesConfig_aControlKeyStrings));
                pConfig->aActions[actionIdx][bindNum] = keyIdx;

                if ( keyIdx != -1 )
                {
                    bKeyboard = 1;
                }
                else
                {
                    // We couldn't find control key num, try searching by control mouse button num, gamepad & joystick button num
                    --argNum; // Has to be decremented as jonesConfig_ParseArgumentIndex increments it
                    int controllerKey = jonesConfig_ParseJoystickControlId(&argNum, &stdConffile_g_entry);
                    pConfig->aActions[actionIdx][bindNum] = controllerKey;

                    if ( controllerKey == -1 )
                    {
                        goto error;
                    }

                    bController = 1;
                    ++argNum;
                }

                bindNum++;
                JONESCONTROL_ACTION_UPDATENUMBINDINGS(pConfig->aActions[actionIdx], bKeyboard, bController);

                if ( JONESCONTROL_ACTION_GETNUMBINDS(pConfig->aActions[actionIdx]) > JONESCONTROL_ACTION_MAXBINDS ) // TODO: is this a bug? Shouldn't it be GT or EQ?
                {
                    static_assert(STD_ARRAYLEN(pConfig->aActions[actionIdx]) == 9, "STD_ARRAYLEN(pConfig->aActions[actionIdx]) == 9");
                    // We've exceeded the number of key mappings

                    char aErrorActionName[512] = { 0 };
                    const char* pFaultyActionName = jonesString_GetString(jonesConfig_aJonesCapControlActionNames[actionIdx]);
                    if ( pFaultyActionName )
                    {
                        STD_STRCPY(aErrorActionName, pFaultyActionName);
                    }

                    char aErrorFormat[512] = { 0 };
                    const char* pErrorText = jonesString_GetString("JONES_STR_EXCEED");
                    if ( pErrorText )
                    {
                        STD_STRCPY(aErrorFormat, pErrorText);
                    }

                    if ( aErrorFormat[0] && aErrorActionName[0] )
                    {
                        char aErrorStr[256] = { 0 };
                        STD_FORMAT(aErrorStr, aErrorFormat, aErrorActionName);
                        jonesConfig_ShowMessageDialog(stdWin95_GetWindow(), "JONES_STR_CTRL_OPTS", aErrorStr, 145);
                    }

                    goto error;
                }
            }
        }

        // Success
        stdConffile_Close();
        return 1;
    }

error:
    memset(pConfig->aName, 0, sizeof(pConfig->aName));
    for ( size_t i = 0; i < bindNum; ++i ) // TODO: Hmm is it ok using binNum here? Shouldn't it use actionCount, or just memset the whole array?
    {
        if ( pConfig->aActions[i][0] )
        {
            memset(pConfig->aActions[i], 0, STD_ARRAYLEN(pConfig->aActions[i]));
        }
    }
    // Old code
    /*if ( bindNum > 0 )
    {
        pAction = pData->aActions;
        do
        {
            if ( pAction )
            {
                (*pAction)[0] = 0;
                (*pAction)[1] = 0;
                (*pAction)[2] &= 0xFF00;
            }

            ++pAction;
            --bindNum;
        } while ( bindNum );
    }*/

    stdConffile_Close();
    return 0;
}

int J3DAPI jonesConfig_ParseArgumentIndex(size_t* argNum, const StdConffileEntry* pEntry, size_t a3, const char** apStringList, size_t size)
{
    J3D_UNUSED(a3);

    char aValue[256] = { 0 }; // Fixed: Init to 0
    STD_STRCPY(aValue, pEntry->aArgs[*argNum].argName);
    int result = jonesConfig_GetStringListIndex(aValue, apStringList, size);

    ++*argNum;
    return result;
}

int J3DAPI jonesConfig_GetStringListIndex(char* pValue, const char** aStringList, size_t size)
{
    if ( size == STD_ARRAYLEN(jonesConfig_aControlKeyStrings) ) // if the list is list of keys, TODO: kill this nonsense and make case insensitive comparison
    {
        stdUtil_ToUpper(pValue);
    }

    for ( size_t index = 0; index < size; index++ )
    {
        if ( aStringList[index] )
        {
            const char* pStr = jonesString_GetString(aStringList[index]);
            if ( pStr )
            {
                if ( strneqi(pValue, pStr, strlen(pStr)) && strlen(pStr) == strlen(pValue) )
                {
                    return index;
                }
            }
            else if ( strneqi(pValue, aStringList[index], strlen(pValue)) && strlen(pValue) == strlen(aStringList[index]) )
            {
                return index;
            }
        }
    }

    return -1;
}

int J3DAPI jonesConfig_ParseJoystickControlId(size_t* pArgNum, const StdConffileEntry* pEntry)
{
    int controlId = -1;

    const char* pButtonFormat = jonesString_GetString("JONES_STR_CAPS_BUTTON");// "BUTTON"
    if ( pButtonFormat )
    {
        size_t formatLen = strlen(pButtonFormat);
        if ( strneqi(pEntry->aArgs[*pArgNum].argName, pButtonFormat, formatLen) )
        {
            char* pArg2 = strpbrk(pEntry->aArgs[*pArgNum].argName, "0123456789");
            if ( pArg2 )
            {
                int btnNum = atoi(pArg2) - 1;
                controlId = STDCONTROL_JOYSTICK_GETBUTTON(0, btnNum);
            }
        }
    }

    if ( controlId == -1 )
    {

        char aArgText[256] = { 0 };
        STD_STRCPY(aArgText, pEntry->aArgs[*pArgNum].argName);

        size_t btnNum = strcspn(aArgText, "0123456789");
        aArgText[btnNum] = '\0';

        if ( btnNum > 0 )
        {
            btnNum = atoi(&pEntry->aArgs[*pArgNum].argName[btnNum]) - 1;
        }

        char aHatFormat[256] = { 0 };
        char* pArg2 = strchr(pEntry->aArgs[*pArgNum].argName, ' ');
        if ( pArg2 )
        {
            STD_FORMAT(aHatFormat, "%s%%i %s", aArgText, pArg2 + 1);
        }

        char aForward[512] = { 0 };
        pButtonFormat = jonesString_GetString("JONES_STR_HATUP");// "HAT%i UP"
        if ( pButtonFormat )
        {
            STD_STRCPY(aForward, pButtonFormat);
        }

        char aBackward[512] = { 0 };
        pButtonFormat = jonesString_GetString("JONES_STR_HATDOWN");// "HAT%i DOWN"
        if ( pButtonFormat )
        {
            STD_STRCPY(aBackward, pButtonFormat);
        }

        char aLeft[512] = { 0 };
        pButtonFormat = jonesString_GetString("JONES_STR_HATLEFT");// "HAT%i LEFT"
        if ( pButtonFormat )
        {
            STD_STRCPY(aLeft, pButtonFormat);
        }

        char aRight[512] = { 0 };
        pButtonFormat = jonesString_GetString("JONES_STR_HATRIGHT");// "HAT%i RIGHT"
        if ( pButtonFormat )
        {
            STD_STRCPY(aRight, pButtonFormat);
        }

        if ( aForward[0] && aBackward[0] && aLeft[0] && aRight[0] && strlen(aHatFormat) )
        {
            size_t formatLen = strlen(aForward);
            if ( strneqi(aHatFormat, aForward, formatLen) )
            {
                controlId = STDCONTROL_JOYSTICK_GETPOVUP(0, btnNum);
            }
            else
            {
                formatLen = strlen(aBackward);
                if ( strneqi(aHatFormat, aBackward, formatLen) )
                {
                    controlId = STDCONTROL_JOYSTICK_GETPOVDOWN(0, btnNum);
                }
                else
                {
                    formatLen = strlen(aLeft);
                    if ( strneqi(aHatFormat, aLeft, formatLen) )
                    {
                        controlId = STDCONTROL_JOYSTICK_GETPOVLEFT(0, btnNum);
                    }
                    else
                    {
                        formatLen = strlen(aRight);
                        if ( strneqi(aHatFormat, aRight, formatLen) )
                        {
                            controlId = STDCONTROL_JOYSTICK_GETPOVRIGHT(0, btnNum);
                        }
                    }
                }
            }
        }
    }

    if ( controlId == -1 )
    {
        char aTwistList[512] = { 0 };
        pButtonFormat = jonesString_GetString("JONES_STR_JOYTWISTL");// "TWISTL"
        if ( pButtonFormat )
        {
            STD_STRCPY(aTwistList, pButtonFormat);
        }

        char aTwistRight[512] = { 0 };
        pButtonFormat = jonesString_GetString("JONES_STR_JOYTWISTR");// "TWISTR"
        if ( pButtonFormat )
        {
            STD_STRCPY(aTwistRight, pButtonFormat);
        }

        char aArgText[256] = { 0 };
        STD_FORMAT(aArgText, "%s", pEntry->aArgs[*pArgNum].argName);

        size_t formatLen = strlen(aTwistList);
        if ( strneqi(aArgText, aTwistList, formatLen) )
        {
            controlId = JONESCONFIG_JOYAXISKID_TWISTL;
        }
        else
        {
            formatLen = strlen(aTwistRight);
            if ( strneqi(aArgText, aTwistRight, formatLen) )
            {
                controlId = JONESCONFIG_JOYAXISKID_TWISTR;
            }
        }
    }

    if ( controlId == -1 )
    {
        char aForward[512] = { 0 };
        pButtonFormat = jonesString_GetString("JONES_STR_JOYUP");// "FORWARD"
        if ( pButtonFormat )
        {
            STD_STRCPY(aForward, pButtonFormat);
        }

        char aBackward[512] = { 0 };
        pButtonFormat = jonesString_GetString("JONES_STR_JOYDOWN");// "BACKWARD"
        if ( pButtonFormat )
        {
            STD_STRCPY(aBackward, pButtonFormat);
        }

        char aLeft[512] = { 0 };
        pButtonFormat = jonesString_GetString("JONES_STR_JOYLEFT");// "LEFT"
        if ( pButtonFormat )
        {
            STD_STRCPY(aLeft, pButtonFormat);
        }

        char aRight[512] = { 0 };
        pButtonFormat = jonesString_GetString("JONES_STR_JOYRIGHT");// "RIGHT"
        if ( pButtonFormat )
        {
            STD_STRCPY(aRight, pButtonFormat);
        }

        size_t formatLen = strlen(aForward);
        if ( strneqi(pEntry->aArgs[*pArgNum].argName, aForward, formatLen) )
        {
            controlId = JONESCONFIG_JOYAXISKID_UP;
        }
        else
        {
            formatLen = strlen(aBackward);
            if ( strneqi(pEntry->aArgs[*pArgNum].argName, aBackward, formatLen) )
            {
                controlId = JONESCONFIG_JOYAXISKID_DOWN;
            }
            else
            {
                formatLen = strlen(aLeft);
                if ( strneqi(pEntry->aArgs[*pArgNum].argName, aLeft, formatLen) )
                {
                    controlId = JONESCONFIG_JOYAXISKID_LEFT;
                }
                else
                {
                    formatLen = strlen(aRight);
                    if ( strneqi(pEntry->aArgs[*pArgNum].argName, aRight, formatLen) )
                    {
                        controlId = JONESCONFIG_JOYAXISKID_RIGHT;
                    }
                }
            }
        }
    }

    if ( controlId > -1 )
    {
        ++*pArgNum;
    }

    return controlId;
}

int J3DAPI jonesConfig_SetDefaultControlScheme(JonesControlsScheme* pScheme, size_t num)
{
    const char* pDefaultKeySetName = jonesString_GetString(jonesConfig_aDfltKeySetNames[num]);
    if ( !pDefaultKeySetName )
    {
        return 0;
    }

    STD_STRCPY(pScheme->aName, pDefaultKeySetName);

    for ( size_t i = 0; i < JONESCONTROL_ACTION_NUMACTIONS; ++i )
    {
        size_t* pAction = pScheme->aActions[i];
        *pAction = 0;

        for ( size_t j = 1; j <= jonesConfig_aDfltKeySets[num][i].numBinds; ++j )
        {
            pAction[j] = jonesConfig_aDfltKeySets[num][i].aBindings[j - 1];
            if ( (pAction[j] < STDCONTROL_MAX_KEYBOARD_BUTTONS || pAction[j] > STDCONTROL_JOYSTICK_LASTPOVCID)
                && (pAction[j] < JONESCONFIG_JOYAXISKID_UP || pAction[j] > JONESCONFIG_JOYAXISKID_GROUND) )
            {
                // keyboard
                JONESCONTROL_ACTION_UPDATENUMBINDINGS_KEYBOARD(pScheme->aActions[i], +1);
            }
            else
            {
                // joystick controller
                JONESCONTROL_ACTION_UPDATENUMBINDINGS_CONTROLLER(pScheme->aActions[i], +1);
            }
        }
    }

    return 1;
}

void J3DAPI jonesConfig_BindControls(JonesControlsScheme* pConfig)
{
    JonesControlActions actionId;

    int bMouse    = wuRegistry_GetIntEx("Mouse Control", 0);
    int bJoystick = wuRegistry_GetIntEx("Joystick Control", 0);

    if ( pConfig )
    {
        // Bind toggle menu keys
        JonesMain_BindToggleMenuControlKeys(&pConfig->aActions[JONESCONTROL_ACTION_INTERFACE][1],
            JONESCONTROL_ACTION_GETNUMBINDS(pConfig->aActions[JONESCONTROL_ACTION_INTERFACE])
        );

        // Bind toggle menu keys
        JonesHud_BindActivateControlKeys(&pConfig->aActions[JONESCONTROL_ACTION_ACTIVATE][1],
            JONESCONTROL_ACTION_GETNUMBINDS(pConfig->aActions[JONESCONTROL_ACTION_ACTIVATE])
        );

        for ( SithControlFunction fctnID = SITHCONTROL_WSELECT0; fctnID <= SITHCONTROL_WSELECT16; ++fctnID )
        {
            sithControl_UnbindFunction(fctnID);
        }

        for ( actionId = JONESCONTROL_ACTION_WALKFORWARD; actionId < JONESCONTROL_ACTION_NUMACTIONS; ++actionId )
        {
            SithControlFunction functionId = -1;

            if ( actionId == JONESCONTROL_ACTION_ACTIVATE || actionId == JONESCONTROL_ACTION_TURNLEFT || actionId == JONESCONTROL_ACTION_TURNRIGHT )
            {
                size_t curBindNum = 1;
                size_t numActionBinds = 3;
                if ( actionId <= JONESCONTROL_ACTION_TURNRIGHT )
                {
                    numActionBinds = 2;
                }

                while ( curBindNum <= numActionBinds )
                {
                    if ( actionId == JONESCONTROL_ACTION_TURNLEFT )
                    {
                        if ( curBindNum == 1 )
                        {
                            functionId = SITHCONTROL_LEFT;
                        }
                        else if ( curBindNum == 2 )
                        {
                            functionId = SITHCONTROL_TURNLEFT;
                        }
                    }
                    else if ( actionId == JONESCONTROL_ACTION_TURNRIGHT )
                    {
                        if ( curBindNum == 1 )
                        {
                            functionId = SITHCONTROL_RIGHT;
                        }

                        else if ( curBindNum == 2 )
                        {
                            functionId = SITHCONTROL_TURNRIGHT;
                        }
                    }
                    else
                    {
                        switch ( curBindNum )
                        {
                            case 1:
                                functionId = SITHCONTROL_ACT2;
                                break;

                            case 2:
                                functionId = SITHCONTROL_FIRE1;
                                break;

                            case 3:
                                functionId = SITHCONTROL_ACTIVATE;
                                break;
                        }
                    }

                    if ( functionId != -1 )
                    {
                        sithControl_UnbindFunction(functionId);

                        // TODO: This must be a bug checking i is LT or EQ, most likely should be LT
                        for ( size_t i = 1; i <= JONESCONTROL_ACTION_GETNUMBINDS(pConfig->aActions[actionId]); ++i )
                        {
                            if ( pConfig->aActions[actionId][i] >= STDCONTROL_JOYSTICK_FIRSTKID && pConfig->aActions[actionId][i] <= STDCONTROL_JOYSTICK_LASTPOVCID
                                || pConfig->aActions[actionId][i] >= STDCONTROL_JOYSTICK_FIRSTKID )
                            {
                                // We have joystick key bind
                                if ( bJoystick )
                                {
                                    jonesConfig_BindJoystickControl(functionId, pConfig->aActions[actionId][i]);
                                }
                            }
                            else
                            {
                                // keyboard key bind
                                sithControl_BindControl(functionId, pConfig->aActions[actionId][i], (SithControlBindFlag)0);
                            }
                        }
                    }

                    ++curBindNum;
                }
            }

            else if ( actionId >= JONESCONTROL_ACTION_FISTS && actionId != JONESCONTROL_ACTION_MAP && actionId != JONESCONTROL_ACTION_INTERFACE )
            {
                if ( JONESCONTROL_ACTION_GETNUMBINDS(pConfig->aActions[actionId]) ) // TODO: this is kinda useless
                {
                    // TODO: This must be a bug checking i is LT or EQ, most likely should be LT
                    for ( size_t i = 1; i <= JONESCONTROL_ACTION_GETNUMBINDS(pConfig->aActions[actionId]); ++i )
                    {
                        switch ( actionId )
                        {
                            case JONESCONTROL_ACTION_FISTS:
                                functionId = SITHCONTROL_WSELECT0;
                                break;

                            case JONESCONTROL_ACTION_WHIP:
                                functionId = SITHCONTROL_WSELECT1;
                                break;

                            case JONESCONTROL_ACTION_MAUSER:
                                functionId = SITHCONTROL_WSELECT4;
                                break;

                            case JONESCONTROL_ACTION_PPSH41:
                                functionId = SITHCONTROL_WSELECT7;
                                break;

                            case JONESCONTROL_ACTION_PISTOL:
                                functionId = SITHCONTROL_WSELECT2;
                                break;

                            case JONESCONTROL_ACTION_SIMONOV:
                                functionId = SITHCONTROL_WSELECT5;
                                break;

                            case JONESCONTROL_ACTION_TOKEREV:
                                functionId = SITHCONTROL_WSELECT3;
                                break;

                            case JONESCONTROL_ACTION_TOZ34:
                                functionId = SITHCONTROL_WSELECT8;
                                break;

                            case JONESCONTROL_ACTION_BAZOOKA:
                                functionId = SITHCONTROL_WSELECT9;
                                break;

                            case JONESCONTROL_ACTION_MACHETE:
                                functionId = SITHCONTROL_WSELECT6;
                                break;

                            case JONESCONTROL_ACTION_SATCHEL:
                                functionId = SITHCONTROL_WSELECT11;
                                break;

                            case JONESCONTROL_ACTION_GRENADE:
                                functionId = SITHCONTROL_WSELECT10;
                                break;

                            case JONESCONTROL_ACTION_HEALTH:
                                functionId = SITHCONTROL_HEALTH;
                                break;

                            case JONESCONTROL_ACTION_IMP1:
                                functionId = SITHCONTROL_WSELECT12;
                                break;

                            case JONESCONTROL_ACTION_IMP2:
                                functionId = SITHCONTROL_WSELECT13;
                                break;

                            case JONESCONTROL_ACTION_IMP3:
                                functionId = SITHCONTROL_WSELECT14;
                                break;

                            case JONESCONTROL_ACTION_IMP4:
                                functionId = SITHCONTROL_WSELECT15;
                                break;

                            case JONESCONTROL_ACTION_IMP5:
                                functionId = SITHCONTROL_WSELECT16;
                                break;

                            case JONESCONTROL_ACTION_CHALK:
                                functionId = SITHCONTROL_CHALK;
                                break;

                            default:
                                break;
                        }

                        if ( functionId != -1 )
                        {
                            if ( pConfig->aActions[actionId][i] >= STDCONTROL_JOYSTICK_FIRSTKID && pConfig->aActions[actionId][i] <= STDCONTROL_JOYSTICK_LASTPOVCID
                                || pConfig->aActions[actionId][i] >= STDCONTROL_JOYSTICK_FIRSTKID )
                            {
                                // We have joystick key bind
                                if ( bJoystick )
                                {
                                    jonesConfig_BindJoystickControl(functionId, pConfig->aActions[actionId][i]);
                                }
                            }
                            else
                            {
                                // We have keyboard key bind
                                sithControl_BindControl(functionId, pConfig->aActions[actionId][i], (SithControlBindFlag)0);
                            }
                        }
                    }
                }
            }
            else
            {
                switch ( actionId )
                {
                    case JONESCONTROL_ACTION_WALKFORWARD:
                        functionId = SITHCONTROL_FORWARD;
                        break;

                    case JONESCONTROL_ACTION_WALKBACK:
                        functionId = SITHCONTROL_BACK;
                        break;

                    case JONESCONTROL_ACTION_STEPLEFT:
                        functionId = SITHCONTROL_STPLEFT;
                        break;

                    case JONESCONTROL_ACTION_STEPRIGHT:
                        functionId = SITHCONTROL_STPRIGHT;
                        break;

                    case JONESCONTROL_ACTION_CRAWL:
                        functionId = SITHCONTROL_CRAWLTOGGLE;
                        break;

                    case JONESCONTROL_ACTION_RUN:
                        functionId = SITHCONTROL_ACT1;
                        break;

                    case JONESCONTROL_ACTION_ROLL:
                        functionId = SITHCONTROL_ACT3;
                        break;

                    case JONESCONTROL_ACTION_JUMP:
                        functionId = SITHCONTROL_JUMP;
                        break;

                    case JONESCONTROL_ACTION_LOOK:
                        functionId = SITHCONTROL_LOOK;
                        break;

                    case JONESCONTROL_ACTION_TOGGLEWEAPON:
                        functionId = SITHCONTROL_WEAPONTOGGLE;
                        break;

                    case JONESCONTROL_ACTION_PREVWEAPON:
                        functionId = SITHCONTROL_PREVWEAPON;
                        break;

                    case JONESCONTROL_ACTION_NEXTWEAPON:
                        functionId = SITHCONTROL_NEXTWEAPON;
                        break;

                    case JONESCONTROL_ACTION_TOGGLELIGHT:
                        functionId = SITHCONTROL_LIGHTERTOGGLE;
                        break;

                    case JONESCONTROL_ACTION_MAP:
                        functionId = SITHCONTROL_MAP;
                        break;

                    default:
                        break;
                }

                if ( functionId != -1 )
                {
                    sithControl_UnbindFunction(functionId);
                    if ( functionId == SITHCONTROL_FORWARD )
                    {
                        sithControl_UnbindFunction(SITHCONTROL_RUNFWD);
                    }

                    // TODO: This must be a bug checking i is LT or EQ, most likely should be LT
                    for ( size_t i = 1; i <= JONESCONTROL_ACTION_GETNUMBINDS(pConfig->aActions[actionId]); ++i )
                    {
                        if ( pConfig->aActions[actionId][i] >= STDCONTROL_JOYSTICK_FIRSTKID && pConfig->aActions[actionId][i] <= STDCONTROL_JOYSTICK_LASTPOVCID
                            || pConfig->aActions[actionId][i] >= STDCONTROL_JOYSTICK_FIRSTKID )
                        {
                            if ( bJoystick )
                            {
                                jonesConfig_BindJoystickControl(functionId, pConfig->aActions[actionId][i]);
                            }
                        }
                        else
                        {
                            sithControl_BindControl(functionId, pConfig->aActions[actionId][i], (SithControlBindFlag)0);
                        }
                    }
                }
            }
        }

        jonesConfig_EnableMouseControl(bMouse);
        if ( !bJoystick )
        {
            sithControl_UnbindJoystickAxes();
        }
    }
}

void J3DAPI jonesConfig_BindJoystickControl(SithControlFunction functionId, size_t controlId)
{
    int axis = -1;
    int flags = 0; // Fixed: Init to 0

    switch ( controlId )
    {
        case JONESCONFIG_JOYAXISKID_UP:
            axis  = STDCONTROL_JOYSTICK_AXIS_Y;
            flags = STDCONTROL_AID_NEGATIVE_AXIS;
            break;

        case JONESCONFIG_JOYAXISKID_RIGHT:
            axis  = STDCONTROL_JOYSTICK_AXIS_X;
            flags = STDCONTROL_AID_POSITIVE_AXIS;
            break;

        case JONESCONFIG_JOYAXISKID_DOWN:
            axis  = STDCONTROL_JOYSTICK_AXIS_Y;
            flags = STDCONTROL_AID_POSITIVE_AXIS;
            break;

        case JONESCONFIG_JOYAXISKID_LEFT:
            axis  = STDCONTROL_JOYSTICK_AXIS_X;
            flags = STDCONTROL_AID_NEGATIVE_AXIS;
            break;

        case JONESCONFIG_JOYAXISKID_TWISTL:
            axis  = STDCONTROL_JOYSTICK_AXIS_RZ;
            flags = STDCONTROL_AID_NEGATIVE_AXIS;
            break;

        case JONESCONFIG_JOYAXISKID_TWISTR:
            axis  = STDCONTROL_JOYSTICK_AXIS_RZ;
            flags = STDCONTROL_AID_POSITIVE_AXIS;
            break;

        case JONESCONFIG_JOYAXISKID_AIR:
            axis  = STDCONTROL_JOYSTICK_AXIS_Z;
            flags = STDCONTROL_AID_NEGATIVE_AXIS;
            break;

        case JONESCONFIG_JOYAXISKID_GROUND:
            axis  = STDCONTROL_JOYSTICK_AXIS_Z;
            flags = STDCONTROL_AID_POSITIVE_AXIS;
            break;
        default:
            axis = -1;
            break;
    }

    const size_t numJoysticks = stdControl_GetNumJoysticks();
    if ( axis == -1 )
    {
        // Bind joystick button
        for ( size_t joyNum = 0; joyNum < numJoysticks; ++joyNum )
        {
            // Note, controlID has to be unpacked to button number due to binding cid to specific joystick
            sithControl_BindControl(functionId, STDCONTROL_JOYSTICK_GETBUTTON(joyNum, STDCONTROL_JOYSTICK_GETBUTTONINDEX(controlId)), (SithControlBindFlag)0);
        }
    }
    else
    {
        for ( size_t joyNum = 0; (int)joyNum < numJoysticks; ++joyNum )
        {
            sithControl_BindAxis(functionId, STDCONTROL_GET_JOYSTICK_AXIS(joyNum, axis) | flags, (SithControlBindFlag)0);
            if ( functionId == SITHCONTROL_FORWARD && !stdControl_IsGamePad(joyNum) )
            {
                flags |= STDCONTROL_AID_LOW_SENSITIVITY;
                functionId = SITHCONTROL_RUNFWD;
                sithControl_BindAxis(SITHCONTROL_RUNFWD, STDCONTROL_GET_JOYSTICK_AXIS(joyNum, axis) | flags, (SithControlBindFlag)0);
            }
        }
    }
}

void jonesConfig_UpdateCurrentLevelNum(void)
{
    jonesConfig_gameStatistics_curLevelNum = JonesMain_GetCurrentLevelNum();
}

int jonesConfig_GetCurrentLevelNum(void)
{
    return jonesConfig_gameStatistics_curLevelNum;
}

int jonesConfig_GetDefaultRun(void)
{
    return jonesConfig_gamePlayOptions_bDefaultRun;
}

void J3DAPI jonesConfig_SetDefaultRun(int bDefaultRun)
{
    jonesConfig_gamePlayOptions_bDefaultRun = bDefaultRun;
    wuRegistry_SaveIntEx("Default Run", jonesConfig_gamePlayOptions_bDefaultRun);

    if ( jonesConfig_gamePlayOptions_bDefaultRun )
    {
        sithControl_g_controlOptions |= 0x02;
    }
    else
    {
        sithControl_g_controlOptions &= ~0x02;
    }
}

void J3DAPI jonesConfig_EnableMouseControl(int bEnable)
{
    JonesDisplaySettings* pSettings = JonesMain_GetDisplaySettings();
    if ( bEnable )
    {
        sithControl_RebindMouse();
    }

    else if ( pSettings->bWindowMode )
    {
        stdControl_EnableMouse(0);
    }
    else
    {
        sithControl_UnbindMouseAxes();
        stdControl_EnableMouse(1);
    }
}

void J3DAPI jonesConfig_FreeControlScheme(JonesControlsScheme* pScheme)
{
    for ( size_t i = 0; i < STD_ARRAYLEN(pScheme->aActions); ++i )
    {
        //if ( (JonesControlsScheme *)((char *)pScheme + 36 * i) != (JonesControlsScheme *)-4 )
        {
            memset(pScheme->aActions[i], 0, STD_ARRAYLEN(pScheme->aActions[i]));
        }
    }

    stdMemory_Free(pScheme);
}

void J3DAPI jonesConfig_FreeControlConfigEntry(JonesControlsConfig* pConfig)
{
    for ( size_t i = 0; i < pConfig->numSchemes; ++i )
    {
        JonesControlsScheme* pScheme = &pConfig->aSchemes[i];
        if ( pScheme )
        {
            for ( size_t j = 0; j < STD_ARRAYLEN(pScheme->aActions); ++j )
            {
                //if ( (JonesControlsScheme*)((char*)pScheme + 36 * j) != (JonesControlsScheme*)-4 )
                {
                    memset(pScheme->aActions[j], 0, STD_ARRAYLEN(pScheme->aActions[j]));
                }
            }
        }
    }

    if ( pConfig->paDeleteSchemes )
    {
        stdMemory_Free(pConfig->paDeleteSchemes);
        pConfig->paDeleteSchemes = NULL;
    }

    if ( pConfig->aSchemes )
    {
        stdMemory_Free(pConfig->aSchemes);
        pConfig->aSchemes = NULL;
    }
}

int J3DAPI jonesConfig_CopyControlConfig(JonesControlsConfig* pSrc, JonesControlsConfig* pDst)
{
    if ( !pSrc )
    {
        return 0;
    }

    if ( !pDst )
    {
        pDst = (JonesControlsConfig*)STDMALLOC(sizeof(JonesControlsConfig));
        if ( !pDst )
        {
            return 0;
        }
    }

    pDst->numSchemes       = pSrc->numSchemes;
    pDst->selectedShemeIdx = pSrc->selectedShemeIdx;

    JonesControlsScheme* apSchemes = jonesConfig_CloneControlSchemesList(pSrc->aSchemes, 0, pSrc->numSchemes);
    if ( !apSchemes && pSrc->aSchemes )
    {
        return 0;
    }

    if ( pDst->aSchemes )
    {
        jonesConfig_FreeControlConfigEntry(pDst);
        pDst->aSchemes = NULL;
    }

    pDst->aSchemes = apSchemes;

    size_t bufSize = 10 - pSrc->numSchemes % 10;
    if ( bufSize <= 0 || bufSize >= 10 )
    {
        return 1;
    }

    pDst->aSchemes = (JonesControlsScheme*)STDREALLOC(pDst->aSchemes, sizeof(JonesControlsScheme) * (bufSize + pSrc->numSchemes));
    memset(&pDst->aSchemes[pSrc->numSchemes], 0, sizeof(JonesControlsScheme) * bufSize);
    return 1;
}

JonesControlsScheme* J3DAPI jonesConfig_CloneControlSchemesList(JonesControlsScheme* aSchemes, size_t start, size_t numSchemes)
{
    JonesControlsScheme* aNewSchemes = (JonesControlsScheme*)STDMALLOC(sizeof(JonesControlsScheme) * numSchemes);
    if ( !aNewSchemes || !aSchemes )
    {
        return 0;
    }

    memset(aNewSchemes, 0, sizeof(JonesControlsScheme) * numSchemes);

    for ( size_t i = start; i < numSchemes + start; ++i )
    {
        STD_STRCPY(aNewSchemes[i - start].aName, aSchemes[i].aName);

        for ( size_t j = 0; j < STD_ARRAYLEN(aSchemes[i].aActions); ++j )
        {
            memset(aNewSchemes[i - start].aActions[j], 0, STD_ARRAYLEN(aNewSchemes[i - start].aActions[j]));
            for ( size_t k = 0; k <= JONESCONTROL_ACTION_GETNUMBINDS(aSchemes[i].aActions[j]); ++k )
            {
                aNewSchemes[i - start].aActions[j][k] = aSchemes[i].aActions[j][k];
            }
        }
    }

    return aNewSchemes;
}

int jonesConfig_ControlOptions_InitControlsConfig(void)
{
    JonesControlsConfig config = { 0 };
    config.selectedShemeIdx = -1;

    if ( !jonesConfig_ControlOptions_SetAllDefaultControlSchemes(&config) || !jonesConfig_ControlOptions_LoadAllControlSchemes(&config) )
    {
        jonesConfig_FreeControlConfigEntry(&config);
        return 0;
    }
    else if ( !jonesConfig_CopyControlConfig(&config, &jonesConfig_controlOptions_curControlConfig) )
    {
        jonesConfig_FreeControlConfigEntry(&jonesConfig_controlOptions_curControlConfig);
        jonesConfig_FreeControlConfigEntry(&config);
        return 0;
    }
    else
    {
        jonesConfig_FreeControlConfigEntry(&config);
        return 1;
    }
}

int J3DAPI jonesConfig_ControlOptions_LoadAllControlSchemes(JonesControlsConfig* pConfig)
{
    char aCurDir[128] = { 0 };
    GetCurrentDirectory(STD_ARRAYLEN(aCurDir), aCurDir);
    if ( !SetCurrentDirectory(jonesConfig_aKeySetsDirPath) )
    {
        return 1;
    }

    WIN32_FIND_DATAA fileData;
    HANDLE hFile = FindFirstFile("*.kfg", &fileData);
    if ( hFile == INVALID_HANDLE_VALUE )
    {
        SetCurrentDirectory(aCurDir);
        return 1;
    }

    while ( 1 )
    {
        if ( (pConfig->numSchemes % 10) == 0 )
        {
            pConfig->aSchemes = (JonesControlsScheme*)STDREALLOC(pConfig->aSchemes, sizeof(JonesControlsScheme) * (pConfig->numSchemes + 10));
        }

        if ( !pConfig->aSchemes )
        {
            break;
        }

        if ( jonesConfig_LoadControlScheme(fileData.cFileName, &pConfig->aSchemes[pConfig->numSchemes]) )
        {
            ++pConfig->numSchemes;
        }

        if ( !FindNextFile(hFile, &fileData) )
        {
            FindClose(hFile);
            SetCurrentDirectory(aCurDir);
            return 1;
        }
    }

    SetCurrentDirectory(aCurDir);
    return 0;
}

int J3DAPI jonesConfig_ControlOptions_SetAllDefaultControlSchemes(JonesControlsConfig* pConfig)
{
    for ( size_t i = 0; i < JONESCONFIG_NUMDEFAULTCONTROLSCHEMES; i++ )
    {
        if ( !(pConfig->numSchemes % 10) )
        {
            pConfig->aSchemes = (JonesControlsScheme*)STDREALLOC(pConfig->aSchemes, sizeof(JonesControlsScheme) * (pConfig->numSchemes + 10));
        }

        if ( !pConfig->aSchemes )
        {
            return 0;
        }

        JonesControlsScheme* pScheme = &pConfig->aSchemes[pConfig->numSchemes];
        if ( !pScheme || !jonesConfig_SetDefaultControlScheme(pScheme, i) )
        {
            return 0;
        }

        ++pConfig->numSchemes;
    }

    return 1;
}

int J3DAPI jonesConfig_ControlOptions_WriteScheme(JonesControlsScheme* pScheme)
{
    // Change cur dir to control config dir
    char aCurDir[128] = { 0 };
    if ( !SetCurrentDirectory(jonesConfig_aKeySetsDirPath) )
    {
        SECURITY_ATTRIBUTES secattr = { 0 };
        secattr.nLength              = sizeof(SECURITY_ATTRIBUTES);
        secattr.lpSecurityDescriptor = NULL;
        secattr.bInheritHandle       = 1;
        CreateDirectory(jonesConfig_aKeySetsDirPath, &secattr);

        GetCurrentDirectory(STD_ARRAYLEN(aCurDir), aCurDir);
        SetCurrentDirectory(jonesConfig_aKeySetsDirPath);
    }

    char* pPadding;
    if ( jonesConfig_maxActionPaddedStrLen <= jonesConfig_maxControlPaddedStrLen )
    {
        pPadding = (char*)STDMALLOC(jonesConfig_maxControlPaddedStrLen);
    }
    else
    {
        pPadding = (char*)STDMALLOC(jonesConfig_maxActionPaddedStrLen);
    }

    if ( !pPadding )
    {
        return 0;
    }

    if ( jonesConfig_maxActionPaddedStrLen <= jonesConfig_maxControlPaddedStrLen )
    {
        memset(pPadding, ' ', jonesConfig_maxControlPaddedStrLen);
    }
    else
    {
        memset(pPadding, ' ', jonesConfig_maxActionPaddedStrLen);
    }

    const size_t maxColumnTextLen = jonesConfig_maxControlStrLen + jonesConfig_maxActionStrLen;
    char* pColumnTitleText = (char*)STDMALLOC(maxColumnTextLen);
    if ( !pColumnTitleText )
    {
        stdMemory_Free(pPadding);
        return 0;
    }

    memset(pColumnTitleText, 0, maxColumnTextLen);

    char aKfgParh[128] = { 0 };
    STD_FORMAT(aKfgParh, "%s%s", pScheme->aName, ".kfg");

    FILE* pFile = fopen(aKfgParh, "wt+");
    if ( !pFile )
    {
        SetCurrentDirectory(aCurDir);
        stdMemory_Free(pColumnTitleText);
        stdMemory_Free(pPadding);
        return 0;
    }

    // Write heading text
    const char* pJonesString = jonesString_GetString("JONES_STR_HEADING");
    if ( !pJonesString )
    {
        goto error;
    }

    fprintf(pFile, pJonesString);
    fprintf(pFile, "\n");

    // Write enabled line
    char aEnabledLine[12] = { 0 }; // Added_ Init to 0
    STD_FORMAT(aEnabledLine, "%s\n\n", "1");
    fprintf(pFile, aEnabledLine);

    // Write column text "Actions :"
    memset(pColumnTitleText, 0, maxColumnTextLen);

    pJonesString = jonesString_GetString("JONES_STR_ACTIONS");
    if ( !pJonesString )
    {
        goto error;
    }

    stdUtil_StringCopy(pColumnTitleText, maxColumnTextLen, pJonesString);

    size_t numPadding = jonesConfig_maxActionPaddedStrLen - strlen(pColumnTitleText);
    if ( jonesConfig_maxActionPaddedStrLen <= jonesConfig_maxControlPaddedStrLen )
    {
        memset(pPadding, 0, jonesConfig_maxControlPaddedStrLen);
    }
    else
    {
        memset(pPadding, 0, jonesConfig_maxActionPaddedStrLen);
    }

    memset(pPadding, ' ', numPadding);
    stdUtil_StringCat(pColumnTitleText, maxColumnTextLen, pPadding);

    // Write column text "Keys:"
    pJonesString = jonesString_GetString("JONES_STR_KEYS");
    if ( !pJonesString )
    {
        goto error;
    }

    // Write column header line
    stdUtil_StringCat(pColumnTitleText, maxColumnTextLen, pJonesString);
    fprintf(pFile, pColumnTitleText);
    fprintf(pFile, "\n\n");

    // Now go over actions and write the key config per action
    for ( size_t i = 0; i < STD_ARRAYLEN(pScheme->aActions); ++i )
    {
        int (*aBindings)[9] = (int (*)[9])pScheme->aActions[i];

      // Write action name
        pJonesString = jonesString_GetString(jonesConfig_aJonesControlActionNames[i]);
        if ( !aBindings || !pJonesString )
        {
            goto error;
        }

        char aActionControlLine[256] = { 0 };
        STD_FORMAT(aActionControlLine, "\"%s\"", pJonesString);
        fprintf(pFile, aActionControlLine);

        // Write padding
        numPadding = jonesConfig_maxActionPaddedStrLen - strlen(aActionControlLine);
        if ( jonesConfig_maxActionPaddedStrLen <= jonesConfig_maxControlPaddedStrLen )
        {
            memset(pPadding, 0, jonesConfig_maxControlPaddedStrLen);
        }
        else
        {
            memset(pPadding, 0, jonesConfig_maxActionPaddedStrLen);
        }

        memset(pPadding, ' ', numPadding);
        fprintf(pFile, pPadding);

        // Now go over action's bindings and write them to file
        for ( size_t j = 1; j <= (uint8_t)(*aBindings)[0]; ++j )
        {
            char aControlText[JONESCONFIG_CONTROLSTRING_MAXLEN] = { 0 };
            jonesConfig_ControlToString((*aBindings)[j], aControlText);

            STD_FORMAT(aActionControlLine, "\"%s\"", aControlText);
            fprintf(pFile, aActionControlLine);

            // Write padding
            numPadding = jonesConfig_maxControlPaddedStrLen - strlen(aControlText);
            if ( jonesConfig_maxActionPaddedStrLen <= jonesConfig_maxControlPaddedStrLen )
            {
                memset(pPadding, 0, jonesConfig_maxControlPaddedStrLen);
            }
            else
            {
                memset(pPadding, 0, jonesConfig_maxActionPaddedStrLen);
            }

            memset(pPadding, ' ', numPadding);
            fprintf(pFile, pPadding);
        }

        fprintf(pFile, "\n");
    }

    // Close file and set cur dir to previous dir
    fclose(pFile);
    SetCurrentDirectory(aCurDir);

    stdMemory_Free(pPadding);
    stdMemory_Free(pColumnTitleText);
    return 1;

error:
    if ( pFile )
    {
        fclose(pFile);
    }

    SetCurrentDirectory(aCurDir);
    stdMemory_Free(pPadding);
    stdMemory_Free(pColumnTitleText);
    return 0;
}

JonesControlsConfig* jonesConfig_GetControlOptionsConfig(void)
{
    return &jonesConfig_controlOptions_curControlConfig;
}

const char* jonesConfig_GetDefaultControlSchemeName(void)
{
    size_t numButtons = stdControl_GetMaxJoystickButtons();
    if ( numButtons == 4 )
    {
        return jonesString_GetString(jonesConfig_aDfltKeySetNames[1]);
    }
    else if ( numButtons == 8 )
    {
        return jonesString_GetString(jonesConfig_aDfltKeySetNames[2]);
    }

    // Keyboard
    return jonesString_GetString(jonesConfig_aDfltKeySetNames[0]);
}

const char* J3DAPI jonesConfig_GetControlActionName(size_t actionId, int bCapitalized)
{
    if ( actionId >= JONESCONTROL_ACTION_NUMACTIONS )
    {
        return NULL;
    }

    return jonesString_GetString(bCapitalized ? jonesConfig_aJonesCapControlActionNames[actionId] : jonesConfig_aJonesControlActionNames[actionId]);
}

const char* jonesConfig_GetKeySetsDirPath(void)
{
    return jonesConfig_aKeySetsDirPath;
}

JonesControlsScheme* J3DAPI jonesConfig_GetDefaultControlScheme(JonesControlsConfig* pConfig)
{
    const char* pSchemeName = jonesConfig_GetDefaultControlSchemeName();
    if ( pSchemeName )
    {
        for ( size_t i = 0; i < JONESCONFIG_NUMDEFAULTCONTROLSCHEMES; ++i ) // 3 default schemes
        {
            if ( streq(pConfig->aSchemes[i].aName, pSchemeName) )
            {
                return &pConfig->aSchemes[i];
            }
        }
    }

    return NULL;
}

int J3DAPI jonesConfig_AssignKey_CheckBindForKey(JonesControlsScheme* pScheme, size_t keyControlID, size_t* pFunctionId, int listID, size_t* pOffset)
{
    int index = -1;
    for ( size_t function = 0; function < STD_ARRAYLEN(pScheme->aActions); ++function )
    {
        size_t offset = 0;
        for ( size_t bindIndx = 1; bindIndx <= JONESCONTROL_ACTION_GETNUMBINDS(pScheme->aActions[function]); ++bindIndx )
        {
            if ( listID == 1006 // controller list
                && ((unsigned int)pScheme->aActions[function][bindIndx] >= STDCONTROL_JOYSTICK_FIRSTKID && (unsigned int)pScheme->aActions[function][bindIndx] <= STDCONTROL_JOYSTICK_LASTPOVCID
                    || (unsigned int)pScheme->aActions[function][bindIndx] >= JONESCONFIG_JOYAXISKID_UP && (unsigned int)pScheme->aActions[function][bindIndx] <= JONESCONFIG_JOYAXISKID_GROUND) )
            {
                ++index;
            }
            else if ( listID == 1005 // key list
                && ((unsigned int)pScheme->aActions[function][bindIndx] < STDCONTROL_MAX_KEYBOARD_BUTTONS || (unsigned int)pScheme->aActions[function][bindIndx] > STDCONTROL_JOYSTICK_LASTPOVCID)
                && ((unsigned int)pScheme->aActions[function][bindIndx] < JONESCONFIG_JOYAXISKID_UP || (unsigned int)pScheme->aActions[function][bindIndx] > JONESCONFIG_JOYAXISKID_GROUND) )
            {
                ++index;
            }

            if ( keyControlID == pScheme->aActions[function][bindIndx] )
            {
                // We found existing binding for keyID
                *pFunctionId = function;
                *pOffset = offset;
                STDLOG_STATUS("index: %i function: %i offset: %i\n", index, function, offset);
                return index;
            }

            ++offset;
        }

        if ( !JONESCONTROL_ACTION_GETNUMKEYBOARDBINDS(pScheme->aActions[function]) && listID == 1005 // key list
            || !JONESCONTROL_ACTION_GETNUMCONTROLLERBINDS(pScheme->aActions[function]) && listID == 1006 ) // controller list
        {
            ++index;
        }
    }

    return -1;
}

int J3DAPI jonesConfig_DisplaySettings_Get3DDeviceSupportsBPP(const StdDisplayInfo* pDisplayInfo, const JonesDisplaySettings* pSettings, int bpp)
{
    if ( !pDisplayInfo )
    {
        return 0;
    }

    switch ( bpp )
    {
        case 16:
            return pDisplayInfo->aDevices[pSettings->device3DNum].d3dDesc.dwDeviceRenderBitDepth & DDBD_16;

        case 24:
            return pDisplayInfo->aDevices[pSettings->device3DNum].d3dDesc.dwDeviceRenderBitDepth & DDBD_24;

        case 32:
            return pDisplayInfo->aDevices[pSettings->device3DNum].d3dDesc.dwDeviceRenderBitDepth & DDBD_32;
    }

    return 0;
}
