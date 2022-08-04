/* $Id: stktst.c 1268 2022-08-04 07:03:08Z mueller $ */
/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright 2022- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de> */

/* Revision History:                                                         */
/* Date         Rev Version  Comment                                         */
/* 2022-08-03  1268   1.0    Initial version                                 */

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <string.h>

int doconv(arg)
  char  *arg;
{
  char  *eptr = NULL;
  long  res = 0;
  int   ires = 0;
  if (arg[0] == 0) {
    fprintf(stderr, "stktst-E: empty string instead of a number\n");
    exit(1);
  }
  res = strtol(arg, &eptr, 0);
  if (eptr[0] != 0) {
    fprintf(stderr, "stktst-E: number '%s' invalid after '%s' \n", arg, eptr);
    exit(1);    
  }
  
  if (errno == ERANGE || res > 0xFFFFL || res < -0x8000L) {
    fprintf(stderr, "stktst-E: number '%s' out of range\n", arg);
    exit(1);
  }

  ires = res;
  return ires;
}

print_stack(val)
  unsigned int val;
{
  unsigned int ns = 7-(val>>13);
  unsigned int nc = 127-((val&017777)>>6);
  unsigned int no = 64-(val&077);
  fprintf(stdout," %06o (%1d,%3d,%2d);", val, ns, nc, no);
}


int main(argc, argv)
  int     argc;
  char    *argv[];
{
  int   argi = 0;
  char  cmd  = 0;
  int   rcnt = 0;
  int   cnt  = 0;
  int   idat[5];
  unsigned int   odat[3];
  int   optrwh = 0;
  int   dotst();

  if (argc < 3) {
    fprintf(stderr, "stktst-E: at least two arguments required\n");
    exit(1);
  }

  cmd  = argv[1][0];                        /* get command */
  rcnt = doconv(argv[2]);
  idat[0] = cmd;                            /* command code */
  idat[1] = 0;                              /* command repeat */
  idat[2] = 0;                              /* -c repeat */
  idat[3] = 0;                              /* -s repeat */
  idat[4] = 0;                              /* -o offset */

  /* check that command is valid */
  switch (cmd) {
  case 'r':
  case 'w':
  case 'h':
    optrwh = 1;
    break;
  case 'I':
  case 'i':
  case 'l':
  case 'f':
  case 'd':
    break;
  default:
    fprintf(stderr, "stktst-E: invalid command %c\n", cmd);
    exit(1);
  }

  /* process options */
  for (argi=3; argi<argc; argi+=2) {
    if (argi+1 >= argc) {
      fprintf(stderr, "stktst-E: no value after %s \n", argv[argi]);
      exit(1);
    }
    cnt = doconv(argv[argi+1]);
    if (strcmp(argv[argi], "-c") == 0) {
      idat[2] = cnt;
    } else if (strcmp(argv[argi], "-s") == 0) {
      idat[3] = cnt;
    } else if (strcmp(argv[argi], "-o") == 0) {
      idat[4] = cnt;
    } else {
      fprintf(stderr, "stktst-E: bad option %s \n", argv[argi]);
      exit(1);
    }  
  }

  if (optrwh) idat[1] = rcnt;
  /* call test: 1st round */
  dotst(idat, odat);
  if (optrwh) exit(0);
  printf("stktst-I: before sp");
  print_stack(odat[0]);
  print_stack(odat[1]);
  printf("\n");

  /* call test: 2nd round */
  idat[1] = rcnt;
  dotst(idat, odat);
  printf("stktst-I: after  sp");
  print_stack(odat[0]);
  print_stack(odat[1]);
  print_stack(odat[2]);
  printf("\n");

  exit(0);
}
