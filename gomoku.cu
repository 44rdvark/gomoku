#include <cstdio>
#include "constants.h"

extern "C" {

__shared__ char board[EDGE2];
__shared__ int sum;

__device__
int cur(int x, int y) { return x + EDGE * y; }

// VERTICAL
__device__
bool h1(int x, int y, int k) { return y - k >= 0; }
__device__
int m1(int x, int y, int k) { return (x + EDGE * (y - k)); }
__device__
bool h2(int x, int y, int k)  { return y < EDGE - k; }
__device__
int m2(int x, int y, int k) { return (x + EDGE * (y + k)); }

// HORIZONTAL
__device__
bool h3(int x, int y, int k) { return x < EDGE - k; }
__device__
int m3(int x, int y, int k) { return (x + EDGE * y + k); }

__device__
bool h4(int x, int y, int k)  { return x - k >= 0; }
__device__
int m4(int x, int y, int k) { return x + EDGE * y - k; }

// DIAGONAL

__device__
bool h5(int x, int y, int k) { return y - k >= 0 && x - k >= 0; }
__device__
int m5(int x, int y, int k) { return x - k + EDGE * (y - k); }

__device__
bool h6(int x, int y, int k) { return x < EDGE - k && y < EDGE - k; }
__device__
int m6(int x, int y, int k) { return x + k + EDGE * (y + k); }

__device__
bool h7(int x, int y, int k)  { return y < EDGE - k && x - k >= 0; }
__device__
int m7(int x, int y, int k) { return x - k + EDGE * (y + k); }

__device__
bool h8(int x, int y, int k)  { return y - k >= 0 && x < EDGE - k; }
__device__
int m8(int x, int y, int k) { return x + k + EDGE * (y - k); }


__device__
bool isLegal(int x, int y) {
    return board[cur(x, y)] == EMPTY;
}

__device__ char next(char player) {
    return player == 'O' ? 'X' : 'O';
}


__device__
void evaluateHalfline(int x, int y, char player, bool (*h)(int x, int y, int k),
                      int (*m)(int x, int y, int k), int& lin, int& lin1, bool& gap, bool& blocked) {

    for (int i = 1; i < 5; i++) {
        if (h(x, y, i)) {
            if(board[m(x, y, i)] == player) {
                if(!gap)
                    lin++;
                else
                    lin1++;
            }
            else if(board[m(x, y, i)] == EMPTY) {
                if(gap)
                    break;
                else {
                    gap = true;
                }
            }

            else {
                if(!gap || lin1 != 0) {
                    blocked = true;
                }
                else {
                    gap = false;
                }
                break;
            }


        }
        else {
            if(!gap || lin1 != 0) {
                blocked = true;
            }
            else {
                gap = false;
            }
            break;
        }
    }

}


__device__ int payoffs[] = {0, 1, 10, 100, 10000, 1000000, 100000000};

__device__
int getOuts(int lin, int lin1, bool gap, int blocked1, int blocked2) {
    // 5 in a row
    if (lin >= 5)
        return WIN;
    // 4 in a row
    if(lin == 4 && !blocked1 && !blocked2)
        return WINNEXT;
    // 4 blocked from side / 5 with a gap
    if(lin == 4 || lin + lin1 == 4)
        return WINCOND;
    // 3 in a row / 4 with a gap
    if(lin + lin1 == 3 && !blocked1 && !blocked2)
        return WINNEXTCOND;
    // 4 with a gap blocked from side / 2 in a row / 3 blocked from side
    if(lin + lin1 == 3 || (lin == 2 && !blocked1 && !blocked2))
        return POSITIVE;
    // nothing
    return NOTHING;
}

__device__
int reverse(int k) {
    int l = 0;
    for (int i = 0; i < DEPTH; i++) {
        l *= EDGE2;
        l += k % EDGE2;
        k /= EDGE2;
    }
    return l;
}


__device__
bool isFiveInLine(int x, int y, bool (*h1)(int x, int y, int k), bool (*h2)(int x, int y, int k),
                  int (*m1)(int x, int y, int k), int (*m2)(int x, int y, int k)) {
    if(board[cur(x, y)] != EMPTY) {
        char player = board[cur(x, y)];
        int len = 1;
        for (int i = 1; h1(x, y, i) && len < 5; i++) {
            if(board[m1(x, y, i)] == player)
                len++;
            else
                break;
        }
        for (int i = 1; h2(x, y, i) && len < 5; i++) {
            if(board[m2(x, y, i)] == player)
                len++;
            else
                break;
        }
        return len == 5;

    }
    return false;
}


__device__
bool isFive(int x, int y) {
    return isFiveInLine(x, y, h1, h2, m1, m2) ||
           isFiveInLine(x, y, h3, h4, m3, m4) ||
           isFiveInLine(x, y, h5, h6, m5, m6) ||
           isFiveInLine(x, y, h7, h8, m7, m8);
}


__device__
bool spaceLeft(int x, int y, char opponent, bool (*h1)(int x, int y, int k), bool (*h2)(int x, int y, int k), int (*m1)(int x, int y, int k), int (*m2)(int x, int y, int k)) {
    int len = 1;
    for (int i = 1; h1(x, y, i) && len < 5; i++) {
        if(board[m1(x, y, i)] != opponent)
            len++;
        else
            break;
    }
    for (int i = 1; h2(x, y, i) && len < 5; i++) {
        if(board[m2(x, y, i)] != opponent)
            len++;
        else
            break;
    }
    return len == 5;
}

__device__
void evaluateLine(int* outs, int x, int y, char player, bool (*h1)(int x, int y, int k), bool (*h2)(int x, int y, int k),
                  int (*m1)(int x, int y, int k), int (*m2)(int x, int y, int k)) {

    bool gap1 = false, gap2, blocked1 = false, blocked2 = false;
    int lin = 1, lin1 = 0, lin2 = 0;
    if (!spaceLeft(x, y, next(player), h1, h2, m1, m2))
        return;

    evaluateHalfline(x, y, player, h1, m1, lin, lin1, gap1, blocked1);
    evaluateHalfline(x, y, player, h2, m2, lin, lin2, gap2, blocked2);
    if (gap1 && gap2) {
        outs[getOuts(lin, lin1, gap1, blocked1, false)]++;
        outs[getOuts(lin, lin2, gap2, blocked2, false)]++;
    }
    else if (gap1)
        outs[getOuts(lin, lin1, gap1, blocked1, blocked2)]++;
    else if (gap2)
        outs[getOuts(lin, lin2, gap2, blocked2, blocked1)]++;
    else
        outs[getOuts(lin, 0, false, blocked1, blocked2)]++;
}

__device__
int evaluateMove(int x, int y, char player) {
    int outs[N_OUTS];
    for(int i = 0; i < N_OUTS; i++)
        outs[i] = 0;
    evaluateLine(outs, x, y, player, h1, h2, m1, m2);
    evaluateLine(outs, x, y, player, h3, h4, m3, m4);
    evaluateLine(outs, x, y, player, h5, h6, m5, m6);
    evaluateLine(outs, x, y, player, h7, h8, m7, m8);
    outs[WININTWO] += outs[WINNEXTCOND] / 2;
    outs[WINNEXTCOND] %= 2;

    if(outs[WINNEXTCOND] > 0 && outs[WINCOND] > 0) {
        outs[WININTWO]++;
        outs[WINNEXTCOND]--;
        outs[WINCOND]--;
    }

    outs[WININTWO] += outs[WINCOND] / 2;
    outs[WINCOND] %= 2;

    int payoff = 0;
    for (int i = 0; i < N_OUTS; i++)
        payoff += outs[i] * payoffs[i];
    return payoff;
}

__global__
void cuMax(int* vals, int* max) {
    __shared__ int t[SZ2];
    int thid = threadIdx.x, pos = thid + SZ * blockIdx.x;

    if (thid >= SZ) {
        t[thid] = -ILLEGAL_MOVE;
        return;
    }
    else
        t[thid] = vals[pos];

    __syncthreads();
    for (int s = SZ2 / 2; s > 0; s /= 2) {
        if (thid < s) {
            if((t[thid + s] > t[thid] && t[thid + s] != ILLEGAL_MOVE) || t[thid] == ILLEGAL_MOVE)
                t[thid] = t[thid + s];
        }
        else
            return;
        __syncthreads();
    }
    if (thid == 0)
        max[blockIdx.x] = t[0];
}

__global__
void cuMin(int* vals, int* min) {
    __shared__ int t[SZ2];
    int thid = threadIdx.x, pos = thid + SZ * blockIdx.x;

    if (thid >= SZ) {
        t[thid] = ILLEGAL_MOVE;
        return;
    }
    else
        t[thid] = vals[pos];

    __syncthreads();
    for (int s = SZ2 / 2; s > 0; s /= 2) {
        if (thid < s) {
            if((t[thid + s] < t[thid] && t[thid + s] != -ILLEGAL_MOVE) || t[thid] == -ILLEGAL_MOVE)
                t[thid] = t[thid + s];
        }
        else
            return;
        __syncthreads();
    }
    if (thid == 0)
        min[blockIdx.x] = t[0];
}

__global__
void evaluate(char* t, int* vals, char startingPlayer, int length, int firstMove) {
    int x, y, mul = 1, rem = blockIdx.x * blockDim.x + firstMove;
    char player;
    board[threadIdx.x] = t[threadIdx.x];
    __syncthreads();

    if (threadIdx.x == 0) {
        int tmp = reverse(rem);
        player = startingPlayer;
        sum = 0;
        int k = 0;
        for (int i = 0; i < DEPTH; i++) {
            x = (tmp % EDGE2) % EDGE, y = (tmp % EDGE2) / EDGE;
            if (isLegal(x, y)) {
                board[tmp % EDGE2] = player;
                if (isFive(x, y)) {
                    vals[rem] = mul * (WINNING_MOVE - k);
                    sum = -1;
                    break;
                }

            } else {
                vals[rem] = ILLEGAL_MOVE;
                sum = -1;
                break;
            }
            player = next(player);
            tmp /= EDGE2;
            mul *= -1;
            k++;
        }
    }
    __syncthreads();
    if(sum == -1) return;

    player = (DEPTH % 2 == 0) ? startingPlayer : next(startingPlayer);

    x = threadIdx.x % EDGE, y = threadIdx.x / EDGE;
    if(isLegal(x, y)) {
        atomicAdd(&sum, PREFER_FIRST * evaluateMove(x, y, player));
        atomicAdd(&sum, -evaluateMove(x, y, next(player)));
    }
    __syncthreads();

    if(threadIdx.x == 0) {
        vals[rem] = sum * mul;
    }
}

}

