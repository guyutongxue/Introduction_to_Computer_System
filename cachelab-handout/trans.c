/**
 * @file trans.c
 * @brief Matrix transpose B = A^T, part of CacheLab, Part B
 * @author AutoLab, Guyutongxue (1900012983@pku.edu.cn)
 * @copyright Copyright (c) 2020 Guyutongxue
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 *
 */

// This file is distributed under WTFPL
// <https://www.wtfpl.net> WITHOUT ANY WARRANTY.

// This file follows Google C++ Style Guide
//     <https://google.github.io/styleguide/cppguide.html>

#include <stdio.h>

#include "./cachelab.h"
#include "./contracts.h"

/**
 * @brief
 * This helper function checks if B is the transpose of
 * A. You can check the correctness of your transpose by calling
 * it before returning from the transpose function.
 * @param M Row num
 * @param N Col num
 * @param A Matrix 1
 * @param B Matrix 2
 * @return Is B = A^T
 *
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/*
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started.
 */

char trans_desc[] = "Simple row-wise scan transpose";
/**
 * @brief A simple baseline transpose function, not optimized for the cache.
 *
 * @param M Row num of output
 * @param N Col num of output
 * @param A Source matrix
 * @param B Destination matrix
 */
void trans(int M, int N, int A[N][M], int B[M][N]) {
  int i, j, tmp;

  REQUIRES(M > 0);
  REQUIRES(N > 0);

  for (i = 0; i < N; i++) {
    for (j = 0; j < M; j++) {
      tmp = A[i][j];
      B[j][i] = tmp;
    }
  }

  ENSURES(is_transpose(M, N, A, B));
}

char transpose_submit_desc[] = "Transpose submission";
/**
 * @brief
 * This is the solution transpose function that you
 * will be graded on for Part B of the assignment. Do not change
 * the description string "Transpose submission", as the driver
 * searches for that string to identify the transpose function to
 * be graded. The REQUIRES and ENSURES from 15-122 are included
 * for your convenience. They can be removed if you like.
 * @param M Row num of output
 * @param N Col num of output
 * @param A Source matrix (Type: int(*)[<VLA>])
 * @param B Destination matrix (Type: int(*)[<VLA>])
 */
void transpose_submit(int M, int N, int A[N][M], int B[M][N]) {
  REQUIRES(M > 0);
  REQUIRES(N > 0);
  int i, j, k;
  (void)k;
  int v0, v1, v2, v3, v4, v5, v6, v7, v8;
  (void)v8;
  switch (M) {
    case 32:
      for (j = 0; j < M; j += 8) {
        for (i = 0; i < N; i++) {
          v0 = A[i][j + 0];
          v1 = A[i][j + 1];
          v2 = A[i][j + 2];
          v3 = A[i][j + 3];
          v4 = A[i][j + 4];
          v5 = A[i][j + 5];
          v6 = A[i][j + 6];
          v7 = A[i][j + 7];
          B[j + 0][i] = v0;
          B[j + 1][i] = v1;
          B[j + 2][i] = v2;
          B[j + 3][i] = v3;
          B[j + 4][i] = v4;
          B[j + 5][i] = v5;
          B[j + 6][i] = v6;
          B[j + 7][i] = v7;
        }
      }
      break;
    case 64:
      for (j = 0; j < M; j += 8) {
        for (i = 0; i < N; i += 8) {
          for (k = i; k < i + 4; k++) {
            v0 = A[k][j + 0];
            v1 = A[k][j + 1];
            v2 = A[k][j + 2];
            v3 = A[k][j + 3];
            v4 = A[k][j + 4];
            v5 = A[k][j + 5];
            v6 = A[k][j + 6];
            v7 = A[k][j + 7];
            B[j + 0][k] = v0;
            B[j + 1][k] = v1;
            B[j + 2][k] = v2;
            B[j + 3][k] = v3;
            B[j + 0][k + 4] = v4;
            B[j + 1][k + 4] = v5;
            B[j + 2][k + 4] = v6;
            B[j + 3][k + 4] = v7;
          }
          for (k = j; k < j + 4; k++) {
            v0 = A[i + 4][k];
            v1 = A[i + 5][k];
            v2 = A[i + 6][k];
            v3 = A[i + 7][k];
            v4 = B[k][i + 4];
            v5 = B[k][i + 5];
            v6 = B[k][i + 6];
            v7 = B[k][i + 7];
            B[k][i + 4] = v0;
            B[k][i + 5] = v1;
            B[k][i + 6] = v2;
            B[k][i + 7] = v3;
            B[k + 4][i + 0] = v4;
            B[k + 4][i + 1] = v5;
            B[k + 4][i + 2] = v6;
            B[k + 4][i + 3] = v7;
          }
          for (k = i + 4; k < i + 8; k++) {
            v0 = A[k][j + 4];
            v1 = A[k][j + 5];
            v2 = A[k][j + 6];
            v3 = A[k][j + 7];
            B[j + 4][k] = v0;
            B[j + 5][k] = v1;
            B[j + 6][k] = v2;
            B[j + 7][k] = v3;
          }
        }
      }
      break;
    case 60:
      for (j = 0; j < M; j += 4) {
        for (i = 0; i < N; i++) {
          v0 = A[i][j + 0];
          v1 = A[i][j + 1];
          v2 = A[i][j + 2];
          v3 = A[i][j + 3];
          B[j + 0][i] = v0;
          B[j + 1][i] = v1;
          B[j + 2][i] = v2;
          B[j + 3][i] = v3;
        }
      }
      break;
  }

  ENSURES(is_transpose(M, N, A, B));
}

/**
 * @brief
 * This function registers your transpose
 * functions with the driver.  At runtime, the driver will
 * evaluate each of the registered functions and summarize their
 * performance. This is a handy way to experiment with different
 * transpose strategies.
 *
 */
void registerFunctions() {
  /* Register your solution function */
  registerTransFunction(transpose_submit, transpose_submit_desc);

  /* Register any additional transpose functions */
  // registerTransFunction(trans, trans_desc);
}

int is_transpose(int M, int N, int A[N][M], int B[M][N]) {
  int i, j;

  for (i = 0; i < N; i++) {
    for (j = 0; j < M; ++j) {
      if (A[i][j] != B[j][i]) {
        return 0;
      }
    }
  }
  return 1;
}
