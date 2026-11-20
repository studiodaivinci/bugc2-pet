// Your robot's names. Change them here and rebuild; nothing else in the firmware needs editing.
// The fonts carry all of printable ASCII, so any name renders. About 8 letters fit the largest captions
// ("Happy Birthday <name>!") and 10 the golf menus.
#pragma once

// The kid the robot belongs to. It hatches and asks "Are you... <name>?", says "Hi <name>!", sings Happy
// Birthday to them, and the golf game is "<name>'s Pop-Up Golf".
#ifndef PET_KID_NAME
#define PET_KID_NAME "Friend"
#endif

// Whoever builds and gives the robot. In golf, the Maker's scores before the egg hatches become the records
// the kid tries to beat ("You beat Maker!"); after hatching the Maker plays like anyone else.
#ifndef PET_MAKER_NAME
#define PET_MAKER_NAME "Maker"
#endif

// The other golf players, in menu order: the kid, PLAYER_2, the Maker, PLAYER_4, PLAYER_5, then Guest.
#ifndef PET_PLAYER_2
#define PET_PLAYER_2 "Player 2"
#endif
#ifndef PET_PLAYER_4
#define PET_PLAYER_4 "Player 4"
#endif
#ifndef PET_PLAYER_5
#define PET_PLAYER_5 "Player 5"
#endif
