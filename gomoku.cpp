#include "cuda.h"
#include "constants.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>

typedef int (*fptr)(int, int);

int cur(int x, int y) { return x + EDGE * y; }

// VERTICAL
bool h1(int x, int y, int k) { return y - k >= 0; }
int m1(int x, int y, int k) { return (x + EDGE * (y - k)); }
bool h2(int x, int y, int k)  { return y < EDGE - k; }
int m2(int x, int y, int k) { return (x + EDGE * (y + k)); }

// HORIZONTAL
bool h3(int x, int y, int k) { return x < EDGE - k; }
int m3(int x, int y, int k) { return (x + EDGE * y + k); }

bool h4(int x, int y, int k)  { return x - k >= 0; }
int m4(int x, int y, int k) { return x + EDGE * y - k; }

// DIAGONAL

bool h5(int x, int y, int k) { return y - k >= 0 && x - k >= 0; }
int m5(int x, int y, int k) { return x - k + EDGE * (y - k); }

bool h6(int x, int y, int k) { return x < EDGE - k && y < EDGE - k; }
int m6(int x, int y, int k) { return x + k + EDGE * (y + k); }

bool h7(int x, int y, int k)  { return y < EDGE - k && x - k >= 0; }
int m7(int x, int y, int k) { return x - k + EDGE * (y + k); }

bool h8(int x, int y, int k)  { return y - k >= 0 && x < EDGE - k; }
int m8(int x, int y, int k) { return x + k + EDGE * (y - k); }


bool isLegal(int x, int y, char* board) {
    return x >= 0 && x < EDGE && y >= 0 && y < EDGE && board[cur(x, y)] == EMPTY;
}

int min(int a, int b) { return a < b ? a : b; }
int max(int a, int b) { return a > b ? a : b; }

fptr next(fptr current) {
    return current == &min ? &max : &min;
}

CUmodule cuModule = (CUmodule)0;
CUfunction evaluate;
CUfunction cuMin;
CUfunction cuMax;


bool isFiveInLine(char* board, int x, int y, bool (*h1)(int x, int y, int k), bool (*h2)(int x, int y, int k), int (*m1)(int x, int y, int k), int (*m2)(int x, int y, int k)) {
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


bool isFive(char* board, int x, int y) {
    return isFiveInLine(board, x, y, h1, h2, m1, m2) ||
           isFiveInLine(board, x, y, h3, h4, m3, m4) ||
           isFiveInLine(board, x, y, h5, h6, m5, m6) ||
           isFiveInLine(board, x, y, h7, h8, m7, m8);
}


bool isWin(char* board) {
    for (int i = 0; i < EDGE2; i++) {
        if (isFive(board, i % EDGE, i / EDGE)) {
            return true;
        }
    }
    return false;
}


void printBoard(char* board) {
    std::system("clear");
    for (int i = 0; i < EDGE; i++) {
        if(EDGE - i - 1 >= 10)
            printf("%d |", EDGE - i - 1);
        else
            printf("%d  |", EDGE - i - 1);
        for (int j = 0; j < EDGE; j++) {
            if (board[cur(j, i)] == EMPTY)
                printf(" |");
            else
                printf("%c|", board[cur(j, i)]);
        }
        printf("\n");
    }
    printf("    ");
    for(int i = 0; i < EDGE; i++)
        printf("%c ", 'A' + i);
    printf("\n");
}

int minMax(CUdeviceptr& dVals, int nThreads) {
    int length = nThreads;
    int *newVals = (int*) malloc(SZ * sizeof(int));
    CUdeviceptr dVals2;
    cuMemAlloc(&dVals2, nThreads / SZ * sizeof(int));
    int blocks_per_grid = nThreads / SZ;
    int threads_per_block = SZ2;
    void *args1[] = {&dVals, &dVals2};
    void *args2[] = {&dVals2, &dVals};

    if (DEPTH == 2) {
        cuLaunchKernel(cuMin, blocks_per_grid, 1, 1, threads_per_block, 1, 1, 0, 0, args1, 0);
        cuMemcpyDtoH(newVals, dVals2, SZ * sizeof(int));
    }
    else if (DEPTH == 3) {
        cuLaunchKernel(cuMax, blocks_per_grid, 1, 1, threads_per_block, 1, 1, 0, 0, args1, 0);
	blocks_per_grid /= SZ;
        cuLaunchKernel(cuMin, blocks_per_grid, 1, 1, threads_per_block, 1, 1, 0, 0, args2, 0);
        cuMemcpyDtoH(newVals, dVals, SZ * sizeof(int));
    }

    int m = -ILLEGAL_MOVE, p = 0;
    for (int i = 0; i < SZ; i++) {
        if (newVals[i] > m && newVals[i] != ILLEGAL_MOVE) {
            m = newVals[i];
            p = i;
        }
    }
    cuMemFree(dVals2);
    std::free(newVals);
    printf("%d\n", p);
    return p;
}

void aiTurn(char player, int turnNo, char* board, int*& vals, CUdeviceptr& dBoard, CUdeviceptr& dVals, int nThreads) {
    if (turnNo == 0)
        board[cur(EDGE / 2, EDGE / 2)] = 'O';
    else if(turnNo == 1) {
        for (int x = 0; x < EDGE; x++)
            for (int y = 0; y < EDGE; y++)
                if(board[cur(x, y)] == 'O')
                    board[cur(x > EDGE - x ? x - 1 : x + 1, y > EDGE - y ? y - 1 : y + 1)] = 'X';
    }
    else {
        cuMemcpyHtoD(dVals, vals, nThreads * sizeof(int));
        cuMemcpyHtoD(dBoard, board, EDGE2 * sizeof(char));
        int k;
        void *args[] = {&dBoard, &dVals, &player, &nThreads, &k};
        int blocks_per_grid = nThreads / EDGE2;
        int threads_per_block = EDGE2;
        for (k = 0; k < EDGE2; k++)
            cuLaunchKernel(evaluate, blocks_per_grid, 1, 1, threads_per_block, 1, 1, 0, 0, args, 0);
        cuCtxSynchronize();
        board[minMax(dVals, nThreads)] = player;
    }
    printBoard(board);

}

void playerTurn(char player, int turn, char* board) {
    char xc;
    int x, y;
    if(turn == 0)
        printBoard(board);
    while(true) {
        scanf("%c %d", &xc, &y);
        while(xc == '\n')
            scanf("%c %d", &xc, &y);
        y = EDGE - y - 1;
        if(xc - 'a' >= 0 && xc - 'a' < EDGE)
            x = xc - 'a';
        else
            x = xc - 'A';
        if(isLegal(x, y, board))
            break;
        else
            printf("Error: invalid coordinates.\n");
    }
    board[cur(x, y)] = player;
    printBoard(board);
}

void randomTurn(char player, int turnNo, char* board) {
    int i, r = rand() % (EDGE2 - turnNo);
    for (i = 0; r >= 0; i++)
        if(board[i] == EMPTY)
            r--;
    board[--i] = player;
    printBoard(board);
}

char next(char player) {
    return player == 'O' ? 'X' : 'O';
}

void aiVsPlayer(char* board, int*& vals, CUdeviceptr& dBoard, CUdeviceptr& dVals, int nThreads) {
    int turn = 0;
    while(true) {

        aiTurn('O', turn, board, vals, dBoard, dVals, nThreads);
        turn++;

        if(isWin(board)) {
            printf("AI won!\n");
            return;
        }

        playerTurn('X', turn, board);
        if(isWin(board)) {
            printf("Player won!\n");
            return;
        }
        turn++;
    }
}

void playerVsAI(char* board, int*& vals, CUdeviceptr& dBoard, CUdeviceptr& dVals, int nThreads) {
    int turn = 0;
    while(true) {
        playerTurn('O', turn, board);
        turn++;

        if(isWin(board)) {
            printf("Player won!\n");
            return;
        }
        aiTurn('X', turn, board, vals, dBoard, dVals, nThreads);
        if(isWin(board)) {
            printf("AI won!\n");
            return;
        }
        turn++;
    }
}

void aiVsRand(char* board, int*& vals, CUdeviceptr& dBoard, CUdeviceptr& dVals, int nThreads) {
    int turn = 0;
    while(true) {

        aiTurn('O', turn, board, vals, dBoard, dVals, nThreads);
        turn++;

        if(isWin(board)) {
            printf("AI won!\n");
            return;
        }

        randomTurn('X', turn, board);
        if(isWin(board)) {
            printf("Random agent won!\n");
            return;
        }
        turn++;
    }
}


void gomoku() {

    cuModuleLoad(&cuModule, "gomoku.ptx");

    cuModuleGetFunction(&evaluate, cuModule, "evaluate");

    cuModuleGetFunction(&cuMin, cuModule, "cuMin");
    cuModuleGetFunction(&cuMax, cuModule, "cuMax");

    srand(time(NULL));

    int nThreads = 1;
    for (int i = 0; i < DEPTH; i++)
        nThreads *= EDGE2;
    size_t outSize = nThreads * sizeof(int), boardSize = EDGE2 * sizeof(char);
    int *vals, *plays;
    char *board;
    CUdeviceptr dVals, dBoard;

    cuMemAlloc(&dVals, outSize);
    cuMemAlloc(&dBoard, boardSize);

    vals = (int*) malloc(outSize);
    board = (char*) malloc(boardSize);

    for (int i = 0; i < EDGE2; i++)
        board[i] = EMPTY;

    int k = 0;
    do {
        printf("Select: \n1) AI vs Player \n2) Player vs AI \n3) AI vs Random Agent\n");
        scanf("%d", &k);
    } while (k < 1 || k > 4);

    if (k == 1)
        aiVsPlayer(board, vals, dBoard, dVals, nThreads);
    else if (k == 2)
        playerVsAI(board, vals, dBoard, dVals, nThreads);
    else if (k == 3)
        aiVsRand(board, vals, dBoard, dVals, nThreads);

    std::free(vals);
    std::free(board);
    cuMemFree(dVals);
    cuMemFree(dBoard);

}


