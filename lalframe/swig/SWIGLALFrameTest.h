//
// Copyright (C) 2011--2014 Karl Wette
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with with program; see the file COPYING. If not, write to the
// Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
// MA  02111-1307  USA
//

// Code for SWIG tests of the LALFrame bindings.
// Author: Karl Wette

#include <lal/SWIGLALTest.h>

#ifndef _SWIGLALFRAMETEST_H
#define _SWIGLALFRAMETEST_H

#ifdef  __cplusplus
extern "C" {
#endif

// Test object parent tracking between modules.
typedef struct tagswig_lalframe_test_parent_map_struct {
  swig_lal_test_struct s;
} swig_lalframe_test_parent_map_struct;
extern swig_lalframe_test_parent_map_struct swig_lalframe_test_parent_map;

#ifdef __cplusplus
}
#endif

#endif // _SWIGLALFRAMETEST_H
