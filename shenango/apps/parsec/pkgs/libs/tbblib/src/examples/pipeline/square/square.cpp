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

//
// Example program that reads a file of decimal integers in text format
// and changes each to its square.
// 
#include "tbb/pipeline.h"
#include "tbb/tick_count.h"
#include "tbb/task_scheduler_init.h"
#include "tbb/tbb_allocator.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cctype>

using namespace std;

//! Holds a slice of text.
/** Instances *must* be allocated/freed using methods herein, because the C++ declaration
    represents only the header of a much larger object in memory. */
class TextSlice {
    //! Pointer to one past last character in sequence
    char* logical_end;
    //! Pointer to one past last available byte in sequence.
    char* physical_end;
public:
    //! Allocate a TextSlice object that can hold up to max_size characters.
    static TextSlice* allocate( size_t max_size ) {
        // +1 leaves room for a terminating null character.
        TextSlice* t = (TextSlice*)tbb::tbb_allocator<char>().allocate( sizeof(TextSlice)+max_size+1 );
        t->logical_end = t->begin();
        t->physical_end = t->begin()+max_size;
        return t;
    }
    //! Free a TextSlice object 
    void free() {
        tbb::tbb_allocator<char>().deallocate((char*)this,sizeof(TextSlice)+(physical_end-begin())+1);
    } 
    //! Pointer to beginning of sequence
    char* begin() {return (char*)(this+1);}
    //! Pointer to one past last character in sequence
    char* end() {return logical_end;}
    //! Length of sequence
    size_t size() const {return logical_end-(char*)(this+1);}
    //! Maximum number of characters that can be appended to sequence
    size_t avail() const {return physical_end-logical_end;}
    //! Append sequence [first,last) to this sequence.
    void append( char* first, char* last ) {
        memcpy( logical_end, first, last-first );
        logical_end += last-first;
    }
    //! Set end() to given value.
    void set_end( char* p ) {logical_end=p;}
};

const size_t MAX_CHAR_PER_INPUT_SLICE = 4000;
static const char* InputFileName = "input.txt";
static const char* OutputFileName = "output.txt";

class MyInputFilter: public tbb::filter {
public:
    MyInputFilter( FILE* input_file_ );
    ~MyInputFilter();
private:
    FILE* input_file;
    TextSlice* next_slice;
    /*override*/ void* operator()(void*);
};

MyInputFilter::MyInputFilter( FILE* input_file_ ) : 
    filter(serial_in_order),
    input_file(input_file_),
    next_slice( TextSlice::allocate( MAX_CHAR_PER_INPUT_SLICE ) )
{ 
}

MyInputFilter::~MyInputFilter() {
    next_slice->free();
}
 
void* MyInputFilter::operator()(void*) {
    // Read characters into space that is available in the next slice.
    size_t m = next_slice->avail();
    size_t n = fread( next_slice->end(), 1, m, input_file );
    if( !n && next_slice->size()==0 ) {
        // No more characters to process
        return NULL;
    } else {
        // Have more characters to process.
        TextSlice& t = *next_slice;
        next_slice = TextSlice::allocate( MAX_CHAR_PER_INPUT_SLICE );
        char* p = t.end()+n;
        if( n==m ) {
            // Might have read partial number.  If so, transfer characters of partial number to next slice.
            while( p>t.begin() && isdigit(p[-1]) ) 
                --p;
            next_slice->append( p, t.end()+n );
        }
        t.set_end(p);
        return &t;
    }
}
    
//! Filter that changes each decimal number to its square.
class MyTransformFilter: public tbb::filter {
public:
    MyTransformFilter();
    /*override*/void* operator()( void* item );
};

MyTransformFilter::MyTransformFilter() : 
    tbb::filter(parallel) 
{}  

/*override*/void* MyTransformFilter::operator()( void* item ) {
    TextSlice& input = *static_cast<TextSlice*>(item);
    // Add terminating null so that strtol works right even if number is at end of the input.
    *input.end() = '\0';
    char* p = input.begin();
    TextSlice& out = *TextSlice::allocate( 2*MAX_CHAR_PER_INPUT_SLICE );
    char* q = out.begin();
    for(;;) {
        while( p<input.end() && !isdigit(*p) ) 
            *q++ = *p++; 
        if( p==input.end() ) 
            break;
        long x = strtol( p, &p, 10 );
        // Note: no overflow checking is needed here, as we have twice the 
        // input string length, but the square of a non-negative integer n 
        // cannot have more than twice as many digits as n.
        long y = x*x; 
        sprintf(q,"%ld",y);
        q = strchr(q,0);
    }
    out.set_end(q);
    input.free();
    return &out;
}
         
//! Filter that writes each buffer to a file.
class MyOutputFilter: public tbb::filter {
    FILE* my_output_file;
public:
    MyOutputFilter( FILE* output_file );
    /*override*/void* operator()( void* item );
};

MyOutputFilter::MyOutputFilter( FILE* output_file ) : 
    tbb::filter(serial_in_order),
    my_output_file(output_file)
{
}

void* MyOutputFilter::operator()( void* item ) {
    TextSlice& out = *static_cast<TextSlice*>(item);
    size_t n = fwrite( out.begin(), 1, out.size(), my_output_file );
    if( n!=out.size() ) {
        fprintf(stderr,"Can't write into file '%s'\n", OutputFileName);
        exit(1);
    }
    out.free();
    return NULL;
}

static int NThread = tbb::task_scheduler_init::automatic;
static bool is_number_of_threads_set = false;

void Usage()
{
    fprintf( stderr, "Usage:\tsquare [input-file [output-file [nthread]]]\n");
}

int ParseCommandLine(  int argc, char* argv[] ) {
    // Parse command line
    if( argc> 4 ){
        Usage();
        return 0;
    }
    if( argc>=2 ) InputFileName = argv[1];
    if( argc>=3 ) OutputFileName = argv[2];
    if( argc>=4 ) {
        NThread = strtol(argv[3],0,0);
        if( NThread<1 ) {
            fprintf(stderr,"nthread set to %d, but must be at least 1\n",NThread);
            return 0;
        }
        is_number_of_threads_set = true; //Number of threads is set explicitly
    }
    return 1;
}

int run_pipeline( int nthreads )
{
    FILE* input_file = fopen(InputFileName,"r");
    if( !input_file ) {
        perror( InputFileName );
        Usage();
        return 0;
    }
    FILE* output_file = fopen(OutputFileName,"w");
    if( !output_file ) {
        perror( OutputFileName );
        return 0;
    }

    // Create the pipeline
    tbb::pipeline pipeline;

    // Create file-reading writing stage and add it to the pipeline
    MyInputFilter input_filter( input_file );
    pipeline.add_filter( input_filter );

    // Create capitalization stage and add it to the pipeline
    MyTransformFilter transform_filter; 
    pipeline.add_filter( transform_filter );

    // Create file-writing stage and add it to the pipeline
    MyOutputFilter output_filter( output_file );
    pipeline.add_filter( output_filter );

    // Run the pipeline
    tbb::tick_count t0 = tbb::tick_count::now();
    // Need more than one token in flight per thread to keep all threads 
    // busy; 2-4 works
    pipeline.run( nthreads*4 );
    tbb::tick_count t1 = tbb::tick_count::now();

    fclose( output_file );
    fclose( input_file );

    if (is_number_of_threads_set) {
        printf("threads = %d time = %g\n", nthreads, (t1-t0).seconds());
    } else {
        if ( nthreads == 1 ){
            printf("serial run   time = %g\n", (t1-t0).seconds());
        } else {
            printf("parallel run time = %g\n", (t1-t0).seconds());
        }
    }
    return 1;
}

int main( int argc, char* argv[] ) {
    if(!ParseCommandLine( argc, argv ))
        return 1;
    if (is_number_of_threads_set) {
        // Start task scheduler
        tbb::task_scheduler_init init( NThread );
        if(!run_pipeline (NThread))
            return 1;
    } else { // Number of threads wasn't set explicitly. Run serial and parallel version
        { // serial run
            tbb::task_scheduler_init init_serial(1);
            if(!run_pipeline (1))
                return 1;
        }
        { // parallel run (number of threads is selected automatically)
            tbb::task_scheduler_init init_parallel;
            if(!run_pipeline (init_parallel.default_num_threads()))
                return 1;
        }
    }
    return 0;
}
