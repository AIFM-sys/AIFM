// Hossein Moein
// September 9, 2020
/*
Copyright (c) 2020-2022, Hossein Moein
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
* Neither the name of Hossein Moein and/or the DataFrame nor the
  names of its contributors may be used to endorse or promote products
  derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL Hossein Moein BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include <DataFrame/DataFrameStatsVisitors.h>

// ----------------------------------------------------------------------------

namespace hmdf
{

struct  GroupbySum  {

    template<typename T, typename I>
    static inline SumVisitor<T, I>
    get_aggregator()  { return (SumVisitor<T, I>()); }
};

// ----------------------------------------------------------------------------

struct  GroupbyMean  {

    template<typename T, typename I>
    static inline MeanVisitor<T, I>
    get_aggregator()  { return (MeanVisitor<T, I>()); }
};

// ----------------------------------------------------------------------------

struct  GroupbyMax  {

    template<typename T, typename I>
    static inline MaxVisitor<T, I>
    get_aggregator()  { return (MaxVisitor<T, I>()); }
};

// ----------------------------------------------------------------------------

struct  GroupbyMin  {

    template<typename T, typename I>
    static inline MinVisitor<T, I>
    get_aggregator()  { return (MinVisitor<T, I>()); }
};

// ----------------------------------------------------------------------------

struct  GroupbyVar  {

    template<typename T, typename I>
    static inline VarVisitor<T, I>
    get_aggregator()  { return (VarVisitor<T, I>()); }
};

// ----------------------------------------------------------------------------

struct  GroupbyStd  {

    template<typename T, typename I>
    static inline StdVisitor<T, I>
    get_aggregator()  { return (StdVisitor<T, I>()); }
};

// ----------------------------------------------------------------------------

struct  GroupbySEM  {

    template<typename T, typename I>
    static inline SEMVisitor<T, I>
    get_aggregator()  { return (SEMVisitor<T, I>()); }
};

// ----------------------------------------------------------------------------

struct  GroupbyMedian  {

    template<typename T, typename I>
    static inline MedianVisitor<T, I>
    get_aggregator()  { return (MedianVisitor<T, I>()); }
};

} // namespace hmdf

// ----------------------------------------------------------------------------

// Local Variables:
// mode:C++
// tab-width:4
// c-basic-offset:4
// End:
