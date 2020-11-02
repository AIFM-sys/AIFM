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

// Workaround for ICC 11.0 not finding __sync_fetch_and_add_4 on some of the Linux platforms.
#if __linux__ && defined(__INTEL_COMPILER)
#define __sync_fetch_and_add(ptr,addend) _InterlockedExchangeAdd(const_cast<void*>(reinterpret_cast<volatile void*>(ptr)), addend)
#endif
#include <string>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include "tbb/concurrent_hash_map.h"
#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"
#include "tbb/tick_count.h"
#include "tbb/task_scheduler_init.h"
#include "tbb/tbb_allocator.h"


//! String type with scalable allocator.
/** On platforms with non-scalable default memory allocators, the example scales 
    better if the string allocator is changed to tbb::tbb_allocator<char>. */
typedef std::basic_string<char,std::char_traits<char>,tbb::tbb_allocator<char> > MyString;

using namespace tbb;
using namespace std;

//! Set to true to counts.
static bool Verbose = false;

//! Working threads count
static int NThread = 1;

//! Problem size
long N = 1000000;
const int size_factor = 2;

//! Indicates if the number of threads wasn't set explicitly
static bool is_number_of_threads_set = false;
//! A concurrent hash table that maps strings to ints.
typedef concurrent_hash_map<MyString,int> StringTable;

//! Function object for counting occurrences of strings.
struct Tally {
    StringTable& table;
    Tally( StringTable& table_ ) : table(table_) {}
    void operator()( const blocked_range<MyString*> range ) const {
        for( MyString* p=range.begin(); p!=range.end(); ++p ) {
            StringTable::accessor a;
            table.insert( a, *p );
            a->second += 1;
        }
    }
};

static MyString* Data;

static void CountOccurrences(int nthreads) {
    StringTable table;

    tick_count t0 = tick_count::now();
    parallel_for( blocked_range<MyString*>( Data, Data+N, 1000 ), Tally(table) );
    tick_count t1 = tick_count::now();

    int n = 0;
    for( StringTable::iterator i=table.begin(); i!=table.end(); ++i ) {
        if( Verbose && nthreads )
            printf("%s %d\n",i->first.c_str(),i->second);
        n += i->second;
    }

    if (is_number_of_threads_set) {
        printf("threads = %d  total = %d  unique = %u  time = %g\n", nthreads, n, unsigned(table.size()), (t1-t0).seconds());
    } else {
        if ( nthreads == 1 ) {
            printf("serial run   total = %d  unique = %u  time = %g\n", n, unsigned(table.size()), (t1-t0).seconds());
        } else {
            printf("parallel run total = %d  unique = %u  time = %g\n", n, unsigned(table.size()), (t1-t0).seconds());
        }
    }
}

/// Generator of random words

struct Sound {
    const char *chars;
    int rates[3];// begining, middle, ending
};
Sound Vowels[] = {
    {"e", {445,6220,1762}}, {"a", {704,5262,514}}, {"i", {402,5224,162}}, {"o", {248,3726,191}},
    {"u", {155,1669,23}}, {"y", {4,400,989}}, {"io", {5,512,18}}, {"ia", {1,329,111}},
    {"ea", {21,370,16}}, {"ou", {32,298,4}}, {"ie", {0,177,140}}, {"ee", {2,183,57}},
    {"ai", {17,206,7}}, {"oo", {1,215,7}}, {"au", {40,111,2}}, {"ua", {0,102,4}},
    {"ui", {0,104,1}}, {"ei", {6,94,3}}, {"ue", {0,67,28}}, {"ay", {1,42,52}},
    {"ey", {1,14,80}}, {"oa", {5,84,3}}, {"oi", {2,81,1}}, {"eo", {1,71,5}},
    {"iou", {0,61,0}}, {"oe", {2,46,9}}, {"eu", {12,43,0}}, {"iu", {0,45,0}},
    {"ya", {12,19,5}}, {"ae", {7,18,10}}, {"oy", {0,10,13}}, {"ye", {8,7,7}},
    {"ion", {0,0,20}}, {"ing", {0,0,20}}, {"ium", {0,0,10}}, {"er", {0,0,20}}
};
Sound Consonants[] = {
    {"r", {483,1414,1110}}, {"n", {312,1548,1114}}, {"t", {363,1653,251}}, {"l", {424,1341,489}},
    {"c", {734,735,260}}, {"m", {732,785,161}}, {"d", {558,612,389}}, {"s", {574,570,405}},
    {"p", {519,361,98}}, {"b", {528,356,30}}, {"v", {197,598,16}}, {"ss", {3,191,567}},
    {"g", {285,430,42}}, {"st", {142,323,180}}, {"h", {470,89,30}}, {"nt", {0,350,231}},
    {"ng", {0,117,442}}, {"f", {319,194,19}}, {"ll", {1,414,83}}, {"w", {249,131,64}},
    {"k", {154,179,47}}, {"nd", {0,279,92}}, {"bl", {62,235,0}}, {"z", {35,223,16}},
    {"sh", {112,69,79}}, {"ch", {139,95,25}}, {"th", {70,143,39}}, {"tt", {0,219,19}},
    {"tr", {131,104,0}}, {"pr", {186,41,0}}, {"nc", {0,223,2}}, {"j", {184,32,1}},
    {"nn", {0,188,20}}, {"rt", {0,148,51}}, {"ct", {0,160,29}}, {"rr", {0,182,3}},
    {"gr", {98,87,0}}, {"ck", {0,92,86}}, {"rd", {0,81,88}}, {"x", {8,102,48}},
    {"ph", {47,101,10}}, {"br", {115,43,0}}, {"cr", {92,60,0}}, {"rm", {0,131,18}},
    {"ns", {0,124,18}}, {"sp", {81,55,4}}, {"sm", {25,29,85}}, {"sc", {53,83,1}},
    {"rn", {0,100,30}}, {"cl", {78,42,0}}, {"mm", {0,116,0}}, {"pp", {0,114,2}},
    {"mp", {0,99,14}}, {"rs", {0,96,16}}, /*{"q", {52,57,1}},*/ {"rl", {0,97,7}},
    {"rg", {0,81,15}}, {"pl", {56,39,0}}, {"sn", {32,62,1}}, {"str", {38,56,0}},
    {"dr", {47,44,0}}, {"fl", {77,13,1}}, {"fr", {77,11,0}}, {"ld", {0,47,38}},
    {"ff", {0,62,20}}, {"lt", {0,61,19}}, {"rb", {0,75,4}}, {"mb", {0,72,7}},
    {"rc", {0,76,1}}, {"gg", {0,74,1}}, {"pt", {1,56,10}}, {"bb", {0,64,1}},
    {"sl", {48,17,0}}, {"dd", {0,59,2}}, {"gn", {3,50,4}}, {"rk", {0,30,28}},
    {"nk", {0,35,20}}, {"gl", {40,14,0}}, {"wh", {45,6,0}}, {"ntr", {0,50,0}},
    {"rv", {0,47,1}}, {"ght", {0,19,29}}, {"sk", {23,17,5}}, {"nf", {0,46,0}},
    {"cc", {0,45,0}}, {"ln", {0,41,0}}, {"sw", {36,4,0}}, {"rp", {0,36,4}},
    {"dn", {0,38,0}}, {"ps", {14,19,5}}, {"nv", {0,38,0}}, {"tch", {0,21,16}},
    {"nch", {0,26,11}}, {"lv", {0,35,0}}, {"wn", {0,14,21}}, {"rf", {0,32,3}},
    {"lm", {0,30,5}}, {"dg", {0,34,0}}, {"ft", {0,18,15}}, {"scr", {23,10,0}},
    {"rch", {0,24,6}}, {"rth", {0,23,7}}, {"rh", {13,15,0}}, {"mpl", {0,29,0}},
    {"cs", {0,1,27}}, {"gh", {4,10,13}}, {"ls", {0,23,3}}, {"ndr", {0,25,0}},
    {"tl", {0,23,1}}, {"ngl", {0,25,0}}, {"lk", {0,15,9}}, {"rw", {0,23,0}},
    {"lb", {0,23,1}}, {"tw", {15,8,0}}, /*{"sq", {15,8,0}},*/ {"chr", {18,4,0}},
    {"dl", {0,23,0}}, {"ctr", {0,22,0}}, {"nst", {0,21,0}}, {"lc", {0,22,0}},
    {"sch", {16,4,0}}, {"ths", {0,1,20}}, {"nl", {0,21,0}}, {"lf", {0,15,6}},
    {"ssn", {0,20,0}}, {"xt", {0,18,1}}, {"xp", {0,20,0}}, {"rst", {0,15,5}},
    {"nh", {0,19,0}}, {"wr", {14,5,0}}
};
const int VowelsNumber = sizeof(Vowels)/sizeof(Sound);
const int ConsonantsNumber = sizeof(Consonants)/sizeof(Sound);
int VowelsRatesSum[3] = {0,0,0}, ConsonantsRatesSum[3] = {0,0,0};

int CountRateSum(Sound sounds[], const int num, const int part)
{
    int sum = 0;
    for(int i = 0; i < num; i++)
        sum += sounds[i].rates[part];
    return sum;
}

const char *GetLetters(int type, const int part)
{
    Sound *sounds; int rate, i = 0;
    if(type & 1)
        sounds = Vowels, rate = rand() % VowelsRatesSum[part];
    else
        sounds = Consonants, rate = rand() % ConsonantsRatesSum[part];
    do {
        rate -= sounds[i++].rates[part];
    } while(rate > 0);
    return sounds[--i].chars;
}

static void CreateData() {
    for(int i = 0; i < 3; i++) {
        ConsonantsRatesSum[i] = CountRateSum(Consonants, ConsonantsNumber, i);
        VowelsRatesSum[i] = CountRateSum(Vowels, VowelsNumber, i);
    }
    for( int i=0; i<N; ++i ) {
        int type = rand();
        Data[i] = GetLetters(type++, 0);
        for( int j = 0; j < type%size_factor; ++j )
            Data[i] += GetLetters(type++, 1);
        Data[i] += GetLetters(type, 2);
    }
    MyString planet = Data[12]; planet[0] = toupper(planet[0]);
    MyString helloworld = Data[0]; helloworld[0] = toupper(helloworld[0]);
    helloworld += ", "+Data[1]+" "+Data[2]+" "+Data[3]+" "+Data[4]+" "+Data[5];
    printf("Message from planet '%s': %s!\nAnalyzing whole text...\n", planet.c_str(), helloworld.c_str());
}

/// Main Driver 

static void ParseCommandLine( int argc, char* argv[] ) {
    int i = 1;
    if( i<argc && strcmp( argv[i], "verbose" )==0 ) {
        Verbose = true;
        ++i;
    }
    if( i<argc )
        if( !isdigit(argv[i][0]) ) {
            fprintf(stderr,"Usage: %s [verbose] [number-of-strings] [number-of-threads]\n",argv[0]);
            exit(1);
        } else {
            N = strtol(argv[i++],0,0);
        }
    if( i<argc )
        if( !isdigit(argv[i][0]) ) {
            fprintf(stderr,"Usage: %s [verbose] [number-of-strings] [number-of-threads]\n",argv[0]);
            exit(1);
        } else {
            NThread = strtol(argv[i++],0,0);
            is_number_of_threads_set = true;
        }
}

int main( int argc, char* argv[] ) {
    srand(2);
    ParseCommandLine( argc, argv );
    Data = new MyString[N];
    CreateData();
    if (is_number_of_threads_set) {
        task_scheduler_init init(NThread);
        CountOccurrences(NThread);
    } else { // Number of threads wasn't set explicitly. Run serial and parallel version
        { // serial run
            task_scheduler_init init_serial(1);
            CountOccurrences(1);
        }
        { // parallel run (number of threads is selected automatically)
            task_scheduler_init init_parallel;
            CountOccurrences(0);
        }
    }
    delete[] Data;
}
