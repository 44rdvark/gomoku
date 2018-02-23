#ifndef GOMOKU_CONSTS_H
#define GOMOKU_CONSTS_H

#define SZ 225
#define SZ2 256
#define DEPTH 2
#define EDGE 15
#define EDGE2 225
#define EMPTY '.'
#define INF 2147483646
#define ILLEGAL_MOVE 2147483646
#define WINNING_MOVE 2147483645

#define PREFER_FIRST 3

#define N_OUTS 7
#define NOTHING 0
#define POSITIVE 1
#define WINNEXTCOND 2
#define WINCOND 3
#define WININTWO 4
#define WINNEXT 5
#define WIN 6


/*
 NOTHING -> nothing -> 0
 POSITIVE -> 4 with a gap blocked from side / 2 in a row / 3 blocked from side -> 10
 WINNEXTCOND -> 3 in a row / 4 with a gap -> 1e2
 WINCOND -> 4 blocked from side / 5 with a gap -> 1e3
 WININTWO -> ... -> 1e4
 WINNEXT -> 4 in a row -> 1e5
 WIN -> 5 in a row -> 1e6
*/

#endif //GOMOKU_CONSTS_H

