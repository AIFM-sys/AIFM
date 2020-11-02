/*
    Copyright 2005-2010 Intel Corporation.  All Rights Reserved.

    This file is part of Threading Building Blocks.

    Threading Building Blocks is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    Threading Building Blocks is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Threading Building Blocks; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
*/

#include <iostream>
#include <string>
#include <algorithm>
#include "tbb/parallel_for.h"
#include "tbb/blocked_range.h"

using namespace tbb;
using namespace std;
static const size_t N = 23;

class SubStringFinder {
  const string str;
  size_t *max_array;
  size_t *pos_array;
public: 
  void operator() ( const blocked_range<size_t>& r ) const { 
    for ( size_t i = r.begin(); i != r.end(); ++i ) {
      size_t max_size = 0, max_pos = 0;
      for (size_t j = 0; j < str.size(); ++j)
      if (j != i) {
        size_t limit = str.size()-max(i,j);
        for (size_t k = 0; k < limit; ++k) {
          if (str[i + k] != str[j + k]) break;
          if (k > max_size) {
            max_size = k;
            max_pos = j;
          }
        }
      }
      max_array[i] = max_size;
      pos_array[i] = max_pos;
    }
  }
  SubStringFinder(string &s, size_t *m, size_t *p) :
    str(s), max_array(m), pos_array(p) { }
};

int main() {

  string str[N] = { string("a"), string("b") };
  for (size_t i = 2; i < N; ++i) str[i] = str[i-1]+str[i-2];
  string &to_scan = str[N-1]; 

  size_t *max = new size_t[to_scan.size()];
  size_t *pos = new size_t[to_scan.size()];

  parallel_for(blocked_range<size_t>(0, to_scan.size() ),
               SubStringFinder( to_scan, max, pos ) );

  for (size_t i = 0; i < to_scan.size(); ++i)
    cout << " " << max[i] << "(" << pos[i] << ")" << endl;
  delete[] pos;
  delete[] max;
  return 0;
}

