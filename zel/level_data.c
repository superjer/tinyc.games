#define ____ OPEN

#define INNEROFFSET ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, \
                    ____, ____, ____, ____, \
                    ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, \
                    ____, ____, ____, ____,
#define I ____, ____,

struct room {
        int doors[4];
        int enemies[NR_ENEMIES];
        int treasure;
        int tiles[TILESH * TILESW];
} rooms[DUNH * DUNW] = {{
        {       WALL,
         WALL,          HOLE, // doors for room 0,0
                DOOR},
        {SCREW, BOARD, PIG}, // enemies
        0, // treasure
        {       INNEROFFSET
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, BLOK, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, BLOK, SAND, BLOK, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, BLOK, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
        },
},{
        {       WALL,
         HOLE,          DOOR, // doors for room 0,1
                DOOR},
        {TOOLBOX}, // enemies
        0, // treasure
        {       INNEROFFSET
                I SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, I
                I SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, I
                I SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
        },
},{
        {       WALL,
         DOOR,          WALL, // doors for room 0,2
                SHUTTER},
        {PIG, PIG, PIG, PIG, PIG}, // enemies
        0, // treasure
        {       INNEROFFSET
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
        },
},{
        {       DOOR,
         WALL,          DOOR, // doors for room 1,0
                LOCKED},
        {SCREW}, // enemies
        0, // treasure
        {       INNEROFFSET
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
        },
},{
        {       DOOR,
         DOOR,          DOOR, // doors for room 1,1
                DOOR},
        {BOARD, BOARD, BOARD, BOARD}, // enemies
        0, // treasure
        {       INNEROFFSET
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I L|U,  PIT|U,PIT|U,PIT|U,PIT|U,PIT|U,PIT|U,PIT|U,PIT|U,PIT|U,U|R,  I
                I ____, ____, ____, ____, ____, ____, PIT,  PIT,  PIT|R,____, ____, I
                I ____, ____, L|U,  FACE, PIT|U,____, PIT|D,FACE, D|R,  ____, ____, I
                I ____, ____, PIT|L,PIT|U,PIT,  ____, ____, ____, ____, ____, ____, I
                I L|D,  PIT|D,PIT|D,PIT|D,PIT|D,PIT|D,PIT|D,PIT|D,PIT|D,PIT|D,D|R,  I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
        },
},{
        {       SHUTTER,
         DOOR,          WALL, // doors for room 1,2
                HOLE},
        {SCREW}, // enemies
        0, // treasure
        {       INNEROFFSET
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
        },
},{
        {       LOCKED,
         WALL,          DOOR, // doors for room 2,0
                WALL},
        {SCREW, SCREW}, // enemies
        0, // treasure
        {       INNEROFFSET
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, BLOK, BLOK, BLOK, BLOK, BLOK, ____, ____, ____, I
                I ____, ____, ____, BLOK, BLOK, BLOK, BLOK, BLOK, ____, ____, ____, I
                I ____, ____, ____, BLOK, BLOK, BLOK, BLOK, BLOK, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
        },
},{
        {       DOOR,
         DOOR,          DOOR, // doors for room 2,1
                ENTRY},
        {}, // enemies
        0, // treasure
        {       INNEROFFSET
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, FACE, ____, ____, ____, ____, ____, ____, ____, FACE, ____, I
                I ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, ____, I
                I ____, ____, ____, ____, SAND, SAND, SAND, ____, ____, ____, ____, I
                I ____, ____, ____, SAND, SAND, SAND, SAND, SAND, ____, ____, ____, I
                I BLOK, FACE, ____, SAND, ____, ____, ____, SAND, ____, FACE, BLOK, I
                I BLOK, BLOK, ____, SAND, ____, ____, ____, SAND, ____, BLOK, BLOK, I
        },
},{
        {       HOLE,
         DOOR,          WALL, // doors for room 2,2
                WALL},
        {PIG, PIG, PIG}, // enemies
        0, // treasure
        {       INNEROFFSET
                I SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, I
                I SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, I
                I SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, I
                I SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, I
                I SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, I
                I SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, I
                I SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, SAND, I
        },
}};

#undef I
