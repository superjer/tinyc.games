#define ____ OPEN

struct room {
        int doors[4];
        int enemies[NR_ENEMIES];
        int treasure;
        int tiles[INNERTILESH * INNERTILESW];
} rooms[DUNH * DUNW] = {{
        {       WALL,
         WALL,          HOLE, // doors for room 0,0
                DOOR},
        {SCREW, BOARD, PIG}, // enemies
        0, // treasure
        {
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, BLOK, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, BLOK, SAND, BLOK, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, BLOK, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
        },
},{
        {       WALL,
         HOLE,          DOOR, // doors for room 0,1
                DOOR},
        {TOOLBOX}, // enemies
        0, // treasure
        {
                SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND,
                SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND,
                SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
        },
},{
        {       WALL,
         DOOR,          WALL, // doors for room 0,2
                SHUTTER},
        {PIG, PIG, PIG, PIG, PIG}, // enemies
        0, // treasure
        {
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
        },
},{
        {       DOOR,
         WALL,          DOOR, // doors for room 1,0
                LOCKED},
        {SCREW}, // enemies
        0, // treasure
        {
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
        },
},{
        {       DOOR,
         DOOR,          DOOR, // doors for room 1,1
                DOOR},
        {BOARD, BOARD, BOARD, BOARD}, // enemies
        0, // treasure
        {
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, L|U,  PIT|U,PIT|U,PIT|U,PIT|U,PIT|U,PIT|U,PIT|U,U|R,  ____,
                ____, PIT|L,PIT,  PIT,  PIT,  PIT,  PIT,  PIT,  PIT,  PIT|R,____,
                ____, PIT|L,PIT,  FACE, PIT,  PIT,  PIT,  FACE, PIT,  PIT|R,____,
                ____, PIT|L,PIT,  PIT|U,PIT,  PIT,  PIT,  PIT|U,PIT,  PIT|R,____,
                ____, L|D,  PIT|D,PIT|D,PIT|D,PIT|D,PIT|D,PIT|D,PIT|D,D|R,  ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
        },
},{
        {       SHUTTER,
         DOOR,          WALL, // doors for room 1,2
                HOLE},
        {SCREW}, // enemies
        0, // treasure
        {
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
        },
},{
        {       LOCKED,
         WALL,          DOOR, // doors for room 2,0
                WALL},
        {SCREW, SCREW}, // enemies
        0, // treasure
        {
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, BLOK, BLOK, BLOK, BLOK, BLOK, ____, ____, ____,
                ____, ____, ____, BLOK, BLOK, BLOK, BLOK, BLOK, ____, ____, ____,
                ____, ____, ____, BLOK, BLOK, BLOK, BLOK, BLOK, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
        },
},{
        {       DOOR,
         DOOR,          DOOR, // doors for room 2,1
                ENTRY},
        {}, // enemies
        0, // treasure
        {
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, FACE, ____, ____, ____, ____, ____, ____, ____, FACE, ____,
                ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
                ____, ____, ____, ____, SAND, SAND, SAND, ____, ____, ____, ____,
                ____, ____, ____, SAND, SAND, SAND, SAND, SAND, ____, ____, ____,
                BLOK, FACE, ____, SAND, ____, ____, ____, SAND, ____, FACE, BLOK,
                BLOK, BLOK, ____, SAND, ____, ____, ____, SAND, ____, BLOK, BLOK,
        },
},{
        {       HOLE,
         DOOR,          WALL, // doors for room 2,2
                WALL},
        {PIG, PIG, PIG}, // enemies
        0, // treasure
        {
                SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND,
                SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND,
                SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND,
                SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND,
                SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND,
                SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND,
                SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND,
        },
}};
