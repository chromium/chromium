/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * Copyright (C) 2002-2022 Németh László
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * Hunspell is based on MySpell which is Copyright (C) 2002 Kevin Hendricks.
 *
 * Contributor(s): David Einstein, Davide Prina, Giuseppe Modugno,
 * Gianluca Turconi, Simon Brouwer, Noll János, Bíró Árpád,
 * Goldman Eleonóra, Sarlós Tamás, Bencsáth Boldizsár, Halácsy Péter,
 * Dvornik László, Gefferth András, Nagy Viktor, Varga Dániel, Chris Halls,
 * Rene Engelhard, Bram Moolenaar, Dafydd Jones, Harri Pitkänen
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/*
 * Copyright 2002 Kevin B. Hendricks, Stratford, Ontario, Canada
 * And Contributors.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. All modifications to the source code must be clearly marked as
 *    such.  Binary redistributions based on modified source code
 *    must be clearly marked as modified versions in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY KEVIN B. HENDRICKS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * KEVIN B. HENDRICKS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* trace sink - collects the records printed by the --trace mode */
#ifndef HUNSPELLTRACE_HXX_
#define HUNSPELLTRACE_HXX_

#include <string>

#include "hunspell.hxx"

// Let the compiler check the trace format string against its arguments.
#if defined(_MSC_VER)
#  include <sal.h>
#  define TRACE_FORMAT _Printf_format_string_
#  define TRACE_FORMAT_CHECK
#elif defined(__GNUC__)
#  define TRACE_FORMAT
#  define TRACE_FORMAT_CHECK __attribute__((format(printf, 2, 3)))
#else
#  define TRACE_FORMAT
#  define TRACE_FORMAT_CHECK
#endif

class AffEntry;
class AffixMgr;
class PfxEntry;
class SfxEntry;

// Receives one trace record at a time. A record is a lowercase verb from a
// closed vocabulary followed by key=value fields. The depth is the nesting
// level of the record and carries no indentation of its own.
class TraceCtx {
 public:
  TraceCtx(HunspellTraceCallback callback, void* userdata)
      : m_callback(callback), m_userdata(userdata) {}

  void set(HunspellTraceCallback callback, void* userdata) {
    m_callback = callback;
    m_userdata = userdata;
  }

  bool on() const { return m_callback && m_suppressed == 0; }

  void emit(const std::string& line) const {
    m_callback(m_userdata, m_depth, line.c_str());
  }

 private:
  friend class TraceScope;
  friend class TraceSuppress;

  int m_depth = 0;
  int m_suppressed = 0;
  HunspellTraceCallback m_callback;
  void* m_userdata;
};

// Holds the nesting level one deeper for as long as it is in scope.
class TraceScope {
 public:
  explicit TraceScope(TraceCtx* context) : m_context(context) {
    if (m_context)
      ++m_context->m_depth;
  }
  ~TraceScope() {
    if (m_context)
      --m_context->m_depth;
  }
  TraceScope(const TraceScope&) = delete;
  TraceScope& operator=(const TraceScope&) = delete;

 private:
  TraceCtx* m_context;
};

class TraceSuppress {
 public:
  explicit TraceSuppress(TraceCtx* context) : m_context(context) {
    if (m_context)
      ++m_context->m_suppressed;
  }
  ~TraceSuppress() {
    if (m_context)
      --m_context->m_suppressed;
  }
  TraceSuppress(const TraceSuppress&) = delete;
  TraceSuppress& operator=(const TraceSuppress&) = delete;

 private:
  TraceCtx* m_context;
};

// Returns the sink while something is listening to it, and null while nothing
// is.
inline TraceCtx* trace_on(TraceCtx* context) {
  return (context && context->on()) ? context : nullptr;
}

// Formats one record and hands it to the sink.
void trace(const TraceCtx& context, TRACE_FORMAT const char* format, ...)
    TRACE_FORMAT_CHECK;

// Formats one flag in the dictionary's own flag syntax. The flag hunspell
// keeps for itself, the one on the hidden capitalised form of a word, prints
// as (onlyupcase).
std::string trace_flag(const AffixMgr* pAMgr, unsigned short flag);

// Formats a flag list in the dictionary's own flag syntax. An empty list
// prints as (none).
std::string trace_flags(const AffixMgr* pAMgr,
                        const unsigned short* astr,
                        int alen);

// Opens an affix rule with the verb given, pfx or sfx. The fields of the rule
// line run in the order that line writes them and end at the line number of the
// rule, then come the fields of the affix class header and its own line number.
// A condition that hunspell dropped as already forced by the stripping is
// marked with redundant=Y, which follows the condition it belongs to.
void trace_affix(const TraceCtx& context,
                 const char* verb,
                 const AffixMgr* pAMgr,
                 const AffEntry& entry);

// Reports one flag test: the flag looked for, the place it was looked for, the
// flags that place actually holds, and how the test came out. The outcome is
// pass or fail, and may carry a comma and a few words on what it means here.
void trace_test(const TraceCtx& context,
                const char* name,
                const AffixMgr* pAMgr,
                unsigned short flag,
                const char* where,
                const unsigned short* astr,
                int alen,
                const char* outcome);

// Reports the circumfix test, which reads two places at once. The record names
// the flag, then the continuation flags of the prefix and of the suffix, so a
// reader sees both halves of the circumfix and which of them carries the flag.
// A missing prefix prints its side as (none), the same as a prefix with no
// continuation flags, because the test treats the two alike.
void trace_circumfix(const TraceCtx& context,
                     const AffixMgr* pAMgr,
                     unsigned short flag,
                     PfxEntry* pfx,
                     SfxEntry* sfx,
                     bool in_prefix,
                     bool in_suffix);

// Sums up an accepted analysis in one line, as the stem with the affixes that
// were applied to it.
void trace_form(const TraceCtx& context,
                const AffixMgr* pAMgr,
                const std::string& surface,
                const char* stem,
                PfxEntry* pfx,
                SfxEntry* sfx);

#endif
