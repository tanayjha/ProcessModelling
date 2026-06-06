#pragma once
#include <cstdio>
#include <cmath>

static int g_failures = 0;

#define CHECK(cond) do{ if(!(cond)){ ++g_failures; \
  std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } }while(0)

#define CHECK_NEAR(a,b,tol) do{ double _d = std::fabs((double)(a)-(double)(b)); \
  if(_d > (tol)){ ++g_failures; \
  std::printf("FAIL %s:%d: |%g-%g|=%g > %g\n", __FILE__, __LINE__, \
  (double)(a), (double)(b), _d, (double)(tol)); } }while(0)

#define REPORT() (g_failures==0 ? (std::printf("OK\n"),0) : \
  (std::printf("%d failures\n", g_failures), 1))
