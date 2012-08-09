//
//  Copyright (C) 2011, 2012 Karl Wette
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with with program; see the file COPYING. If not, write to the
//  Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
//  MA  02111-1307  USA
//

// SWIG interface to GSL structures
// Author: Karl Wette

// Only in SWIG interface.
#ifdef SWIG

// Exclude from preprocessing interface.
#ifndef SWIGXML

// Include basic GSL headers.
%header %{
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
%}

// Set custom GSL error handler which raises an XLAL error (instead of aborting).
%header %{
  void swiglal_gsl_error_handler(const char *reason, const char *file, int line, int errnum) {
    XLALPrintError("GSL function failed: %s (errnum=%i)\n", reason, errnum);
    XLALError("<GSL function>", file, line, XLAL_EFAILED);
  }
%}
%init %{
  gsl_set_error_handler(swiglal_gsl_error_handler);
%}

// This macro create wrapping structs for GSL vectors and matrices.
%define %lalswig_gsl_vector_matrix(TYPE, NAME)

  // GSL vector of type NAME.
  typedef struct {
    %extend {
      gsl_vector##NAME(const size_t n) {
        return gsl_vector##NAME##_calloc(n);
      }
      gsl_vector##NAME(gsl_vector##NAME *v0) {
        gsl_vector##NAME *v = gsl_vector##NAME##_alloc(v0->size);
        gsl_vector##NAME##_memcpy(v, v0);
        return v;
      }
      ~gsl_vector##NAME() {
        %swiglal_call_dtor(gsl_vector##NAME##_free, $self);
      }
    }
    %swiglal_array_dynamic_1D(TYPE, size_t, data, size, arg1->stride);
  } gsl_vector##NAME;

  // GSL matrix of type NAME.
  typedef struct {
    %extend {
      gsl_matrix##NAME(const size_t n1, const size_t n2) {
        return gsl_matrix##NAME##_calloc(n1, n2);
      }
      gsl_matrix##NAME(gsl_matrix##NAME *m0) {
        gsl_matrix##NAME *m = gsl_matrix##NAME##_alloc(m0->size1, m0->size2);
        gsl_matrix##NAME##_memcpy(m, m0);
        return m;
      }
      ~gsl_matrix##NAME() {
        %swiglal_call_dtor(gsl_matrix##NAME##_free, $self);
      }
    }
    %swiglal_array_dynamic_2D(TYPE, size_t, data, size1, size2, arg1->tda, 1);
  } gsl_matrix##NAME;

%enddef // %lalswig_gsl_vector_matrix

// GSL integer vectors and matrices.
%lalswig_gsl_vector_matrix(short, _short);
%lalswig_gsl_vector_matrix(unsigned short, _ushort);
%lalswig_gsl_vector_matrix(int, _int);
%lalswig_gsl_vector_matrix(unsigned int, _uint);
%lalswig_gsl_vector_matrix(long, _long);
%lalswig_gsl_vector_matrix(unsigned long, _ulong);

// GSL real and complex vectors and matrices.
%lalswig_gsl_vector_matrix(float, _float);
%lalswig_gsl_vector_matrix(double, ); // GSL double vec./mat. has no typename suffix.
%lalswig_gsl_vector_matrix(gsl_complex_float, _complex_float);
%lalswig_gsl_vector_matrix(gsl_complex, _complex);

#endif // !SWIGXML

#endif // SWIG