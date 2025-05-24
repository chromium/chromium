/* -*- coding: utf-8 -*-
 *
 * OneEuroFilter.cpp -
 *
 * Authors: 
 * Nicolas Roussel (nicolas.roussel@inria.fr)
 * Géry Casiez https://gery.casiez.net
 *
 * Copyright 2019 Inria
 * 
 * BSD License https://opensource.org/licenses/BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, this list of conditions
 * and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions
 * and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or
 * promote products derived from this software without specific prior written permission.

 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "OneEuroFilter.h"

// Math constants are not always defined.
#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

// -----------------------------------------------------------------


void LowPassFilter::setAlpha(double alpha) {
  if (alpha<=0.0 || alpha>1.0)
#ifdef __EXCEPTIONS
    throw std::range_error("alpha should be in (0.0., 1.0] and its current value is " + std::to_string(alpha)) ;
#else
    alpha = 0.5 ;
#endif
  a = alpha ;
}

LowPassFilter::LowPassFilter(double alpha, double initval) {
  y = s = initval ;
  setAlpha(alpha) ;
  initialized = false ;
}

double LowPassFilter::filter(double value) {
  double result ;
  if (initialized)
    result = a*value + (1.0-a)*s ;
  else {
    result = value ;
    initialized = true ;
  }
  y = value ;
  s = result ;
  return result ;
}

double LowPassFilter::filterWithAlpha(double value, double alpha) {
  setAlpha(alpha) ;
  return filter(value) ;
}

bool LowPassFilter::hasLastRawValue(void) {
  return initialized ;
}

double LowPassFilter::lastRawValue(void) {
  return y ;
}

double LowPassFilter::lastFilteredValue(void) {
  return s ;
}

// -----------------------------------------------------------------

double OneEuroFilter::alpha(double cutoff) {
  double te = 1.0 / freq ;
  double tau = 1.0 / (2*M_PI*cutoff) ;
  return 1.0 / (1.0 + tau/te) ;
}

void OneEuroFilter::setFrequency(double f) {
  if (f<=0)
#ifdef __EXCEPTIONS
    throw std::range_error("freq should be >0") ;
#else
    f= 120 ;  // set to 120Hz default
#endif

  freq = f ;
}

void OneEuroFilter::setMinCutoff(double mc) {
  if (mc<=0)
#ifdef __EXCEPTIONS
    throw std::range_error("mincutoff should be >0") ;
#else
    mc = 1.0 ;
#endif
  mincutoff = mc ;
}

void OneEuroFilter::setBeta(double b) {
  beta_ = b ;
}

void OneEuroFilter::setDerivateCutoff(double dc) {
  if (dc<=0)
#ifdef __EXCEPTIONS
    throw std::range_error("dcutoff should be >0") ;
#else
    dc = 1.0 ;
#endif
  dcutoff = dc ;
}

OneEuroFilter::OneEuroFilter(double freq, 
  double mincutoff, double beta_, double dcutoff) {
  setFrequency(freq) ;
  setMinCutoff(mincutoff) ;
  setBeta(beta_) ;
  setDerivateCutoff(dcutoff) ;
  x = new LowPassFilter(alpha(mincutoff)) ;
  dx = new LowPassFilter(alpha(dcutoff)) ;
  lasttime = UndefinedTime ;
}

double OneEuroFilter::filter(double value, TimeStamp timestamp) {
  // update the sampling frequency based on timestamps
  if (lasttime!=UndefinedTime && timestamp!=UndefinedTime && timestamp>lasttime)
    freq = 1.0 / (timestamp-lasttime) ;
  lasttime = timestamp ;
  // estimate the current variation per second 
  // Fixed in 08/23 to use lastFilteredValue
  double dvalue = x->hasLastRawValue() ? (value - x->lastFilteredValue())*freq : 0.0 ; // FIXME: 0.0 or value?
  double edvalue = dx->filterWithAlpha(dvalue, alpha(dcutoff)) ;
  // use it to update the cutoff frequency
  double cutoff = mincutoff + beta_*fabs(edvalue) ;
  // filter the given value
  return x->filterWithAlpha(value, alpha(cutoff)) ;
}

OneEuroFilter::~OneEuroFilter(void) {
  delete x ;
  delete dx ;
}
