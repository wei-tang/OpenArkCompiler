/*
 * Copyright (C) [2021] Futurewei Technologies, Inc. All rights reverved.
 *
 * Licensed under the Mulan Permissive Software License v2.
 * You can use this software according to the terms and conditions of the MulanPSL - 2.0.
 * You may obtain a copy of MulanPSL - 2.0 at:
 *
 *   https://opensource.org/licenses/MulanPSL-2.0
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR
 * FIT FOR A PARTICULAR PURPOSE.
 * See the MulanPSL - 2.0 for more details.
 */
#include "arm_neon.h"
#include <stdio.h>
#include <stdlib.h>

uint16x8_t foo(uint16x8_t p1, uint16x8_t p2)
{
   return vceqq_u16(p1, p2);
}

int main()
{
  unsigned short s1[] = { 10, 20, 30, 40, 1, 2, 3, 4 };
  unsigned short s2[] = { 10, 20, 30, 41, 1, 2, 3, 3 };

  uint16x8_t a = vld1q_u16((void*)&s1);
  uint16x8_t b = vld1q_u16((void*)&s2);
  uint16x8_t x = foo(a, b);
  unsigned s = vgetq_lane_u16(x, 3) + vgetq_lane_u16(x, 7);
  // printf("s = 0x%x\n", s);
  if (s != 0 )
    abort();

  s = vgetq_lane_u16(x, 0);
  // printf("s = 0x%x\n", s);
  if (s != 0xffff)
    abort();

  int16x8_t c = { 1, 1, 1, 1, 2, 2, 2, 2 };
  if (vgetq_lane_u16( vshlq_u16(a, c), 2) != 60)
    abort();

  if (vgetq_lane_u16( vshlq_u16(a, c), 7) != 16)
    abort();
}
