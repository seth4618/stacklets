// /*BEGIN_LEGAL
// Intel Open Source License

// Copyright (c) 2002-2015 Intel Corporation. All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:

// Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.  Redistributions
// in binary form must reproduce the above copyright notice, this list of
// conditions and the following disclaimer in the documentation and/or
// other materials provided with the distribution.  Neither the name of
// the Intel Corporation nor the names of its contributors may be used to
// endorse or promote products derived from this software without
// specific prior written permission.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
// ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// END_LEGAL */
#include <iostream>
#include <fstream>
#include <algorithm>
#include "math.h"
#include "pin.H"
#include "assert.h"
#include "xed-iclass-enum.h"
#include "cache_sim.h"
ofstream OutFile;

Cache::Cache() {}
Cache::~Cache() {}
Cache::Cache(unsigned int cache_size,
             unsigned int line_size,
             unsigned int miss_penalty,
             bool is_direct_mapped) :
    cache_size(cache_size), line_size(line_size), miss_penalty(miss_penalty) {
    cache = new vector< vector<cache_line*>* >();

    assoc = 1;
    if (is_direct_mapped)
    {
        set_size = cache_size / line_size;
    } else {
        assoc = 2;
        set_size = cache_size / 2 / line_size;
    }

    // initialize the cache
    for (unsigned int i = 0; i < set_size; ++i)
    {
        vector<cache_line*> *a_set = new vector<cache_line*>();
        for (unsigned int j = 0; j < assoc; ++j)
        {
            cache_line *a_line = new cache_line();
            a_line -> valid = false;
            a_line -> tag = 0;
            a_line -> recent = 0;
            a_set -> push_back(a_line);
        }
        cache -> push_back(a_set);
    }

    s = (unsigned int)log2f(set_size);
    b = (unsigned int)log2f(line_size);
    t = (unsigned int)(32 - s - b);

}

void Cache::dataRead(unsigned int addr, unsigned int mem_addr) {

    unsigned int set_index = ~(0xFFFFFFFF << s) & (mem_addr >> b);
    unsigned int tag = ~(0xFFFFFFFF << t) & (mem_addr >> (b + s));

    vector<cache_line*> cache_set = *(cache -> at(set_index));
    assert(cache_set.size() == assoc);
    for (vector<cache_line*>::iterator it = cache_set.begin(); it != cache_set.end(); ++it) {
        cache_line *line = *it;
        if (line -> valid && line -> tag == tag)
        {
            // cache hit, increment hit count
            if (cacheloadstat.count(addr) == 1) {
                stat s = cacheloadstat[addr];
                s.hit++;
                cacheloadstat[addr] = s;
            } else {
                stat s;
                s.hit = 1;
                s.miss = 0;
                cacheloadstat[addr] = s;
            }

            // increment the recent counter
            line -> recent++;
            return;
        }
    }

    // cache miss, increment miss count

    if (cacheloadstat.count(addr) == 1) {
        stat s = cacheloadstat[addr];
        s.miss++;
        cacheloadstat[addr] = s;
    } else {
        stat s;
        s.miss = 1;
        s.hit = 0;
        cacheloadstat[addr] = s;
    }

    // eviction
    for (vector<cache_line*>::iterator it = cache_set.begin(); it != cache_set.end(); ++it) {
        cache_line *line = *it;
        if (!line -> valid)
        {
            line -> valid = true;
            line -> tag = tag;
            line -> recent++;
            assert(line -> recent == 1);
            return;
        }
    }

    // otherwise, we try to evict a line
    if (assoc == 1)
    {
        cache_line* candidate = (cache_set.at(0));
        assert(candidate != NULL);
        candidate -> valid = true;
        candidate -> tag = tag;
        candidate -> recent = 1;
    } else {    // we can assume that at most 2 cache line in a set
        assert(cache_set.size() == 2);
        cache_line* candidate0 = (cache_set.at(0));
        cache_line* candidate1 = (cache_set.at(1));
        if (candidate1 -> recent >= candidate0 -> recent)
        {
            // candidate0 is the least recent
            candidate0 -> valid = true;
            candidate0 -> tag = tag;
            candidate0 -> recent = 1;
        } else {
            // candidate1 is the least recent
            candidate1 -> valid = true;
            candidate1 -> tag = tag;
            candidate1 -> recent = 1;
        }
    }
    return;

}

void Cache::dataWrite(unsigned int addr, unsigned int mem_addr) {

    unsigned int set_index = ~(0xFFFFFFFF << s) & (mem_addr >> b);
    unsigned int tag = ~(0xFFFFFFFF << t) & (mem_addr >> (b + s));
    // find the cache line
    vector<cache_line*> cache_set = *(cache -> at(set_index));
    assert(cache_set.size() == assoc);
    for (vector<cache_line*>::iterator it = cache_set.begin(); it != cache_set.end(); ++it) {
        cache_line *line = *it;
        if (line -> valid && line -> tag == tag)
        {
            // cache hit, increment hit count
            if (cachestorestat.count(addr) == 1) {
                stat s = cachestorestat[addr];
                s.hit++;
                cachestorestat[addr] = s;
            } else {
                stat s;
                s.hit = 1;
                s.miss = 0;
                cachestorestat[addr] = s;
            }

            // increment the recent counter
            line -> recent++;
            return;
        }
    }

    // cache miss, increment miss count
    if (cachestorestat.count(addr) == 1) {
        stat s = cachestorestat[addr];
        s.miss++;
        cachestorestat[addr] = s;
    } else {
        stat s;
        s.miss = 1;
        s.hit = 0;
        cachestorestat[addr] = s;
    }

    // eviction
    for (vector<cache_line*>::iterator it = cache_set.begin(); it != cache_set.end(); ++it) {
        cache_line *line = *it;
        if (!line -> valid)
        {
            line -> valid = true;
            line -> tag = tag;
            line -> recent++;
            assert(line -> recent == 1);
            return;
        }
    }

    // otherwise, we try to evict a line
    if (assoc == 1)
    {
        cache_line* candidate = (cache_set.at(0));
        assert(candidate != NULL);
        candidate -> valid = true;
        candidate -> tag = tag;
        candidate -> recent = 1;
    } else {    // we can assume that at most 2 cache line in a set
        assert(cache_set.size() == 2);
        cache_line* candidate0 = (cache_set.at(0));
        cache_line* candidate1 = (cache_set.at(1));
        if (candidate1 -> recent >= candidate0 -> recent)
        {
            // candidate0 is the least recent
            candidate0 -> valid = true;
            candidate0 -> tag = tag;
            candidate0 -> recent = 1;
        } else {
            // candidate1 is the least recent
            candidate1 -> valid = true;
            candidate1 -> tag = tag;
            candidate1 -> recent = 1;
        }
    }
    return;

}

Cache *icache = new Cache(32 * 1024, 128, 100, false);
Cache *dcache = new Cache(32 * 1024, 128, 100, false);

// this function represents the case of a memory copied to register
void MemoryRead(ADDRINT mem_r, ADDRINT inst_addr) {
    dcache -> dataRead(inst_addr, mem_r);
}

void MemoryWrite(ADDRINT mem_r, ADDRINT inst_addr) {
    dcache -> dataWrite(inst_addr, mem_r);
}

void InstructionExec(ADDRINT addr) {
    icache -> dataRead(addr, addr);
}

/*EXCEPT_HANDLING_RESULT DivideHandler(THREADID tid, EXCEPTION_INFO * pExceptInfo, 
                                        PHYSICAL_CONTEXT * pPhysCtxt, VOID *appContextArg)
{
    if(PIN_GetExceptionCode(pExceptInfo) == EXCEPTCODE_INT_DIVIDE_BY_ZERO) 
    {
#if 1
        //Temporary work-around, Remove when Mantis #1986 is resolved
        string str = PIN_ExceptionToString(pExceptInfo);
        printf("GlobalHandler: Caught divide by zero exception. %s\n", str.c_str());
        fflush(stdout);
#else
        cout << "GlobalHandler: Caught divide by zero exception. " << PIN_ExceptionToString(pExceptInfo) << endl << flush;
#endif
        CONTEXT * appCtxt = (CONTEXT *)appContextArg;
        ADDRINT faultIp = PIN_GetContextReg(appCtxt, REG_INST_PTR);
        PIN_SetExceptionAddress(pExceptInfo, faultIp);
        PIN_RaiseException(appCtxt, tid, pExceptInfo); //never returns 
    }
    return EHR_CONTINUE_SEARCH;
}*/

VOID EmulateIllegal(void)
{
/*    PIN_TryStart(tid, DivideHandler, ctxt);

    UINT64 dividend = *pGdx;
    dividend <<= 32;
    dividend += *pGax;
    *pGax = dividend / divisor;
    *pGdx = dividend % divisor;

    PIN_TryEnd(tid); */
}

// Pin calls this function every time a new instruction is encountered
VOID Instruction(INS ins)
{
    if ((INS_Opcode(ins) == XED_ICLASS_DIV))
    {
        cerr << "Inside Illegal exception" << endl;
        cerr << "Opcode" << INS_Mnemonic(ins) << endl;
       /* INS_InsertCall(ins,
                       IPOINT_BEFORE,
                       AFUNPTR(EmulateIllegal),
                       IARG_REG_REFERENCE, REG_GDX,
                       IARG_REG_REFERENCE, REG_GAX,
                       IARG_REG_VALUE, REG(INS_OperandReg(ins, 0)),
                       IARG_CONTEXT,
                       IARG_THREAD_ID, 
                       IARG_END);*/
        INS_Delete(ins);
    }
}


KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
                            "o", "problem_941.out", "specify output file name");


VOID Image(IMG img, VOID *v) {
    // hardcoded path
    if (IMG_Name(img) == "/afs/andrew.cmu.edu/usr5/preetium/public/tools/pin-2.14-71313-gcc.4.4.7-linux/a.out") {
        for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
        {

            for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
            {
                // Open the RTN.
                RTN_Open( rtn );

                // Examine each instruction in the routine.
                for ( INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins) )
                {
                    Instruction(ins);
                }
                // Close the RTN.
                RTN_Close( rtn );
            }
        }
      }
}

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{

    ofstream OutFile;
    OutFile.open(KnobOutputFile.Value().c_str());
    OutFile.setf(ios::showbase);
    unsigned long total_data_miss = 0;
    unsigned long total_instr_miss = 0;
    unsigned long total_references = 0;
    for (map<unsigned int, stat>::iterator it = (dcache -> cacheloadstat).begin();
            it != (dcache->cacheloadstat).end(); ++it)
    {

        print_stat s;
        s.pc = it->first;
        s.read = true;
        s.ref = it->second.hit + it->second.miss;
        s.miss = it->second.miss;
        s.miss_rate = ((float)s.miss) / s.ref;
        s.miss_cycle = ((dcache -> miss_penalty) * s.miss) + 1;
        s.contrib = 0.0;

        total_data_miss += s.miss_cycle;

        dcache -> print_vector.push_back(s);
    }


    for (map<unsigned int, stat>::iterator it = (dcache -> cachestorestat).begin();
            it != (dcache->cachestorestat).end(); ++it)
    {
        print_stat s;
        s.pc = it->first;
        s.read = false;
        s.ref = it->second.hit + it->second.miss;
        s.miss = it->second.miss;
        s.miss_rate = ((float)s.miss) / s.ref;
        s.miss_cycle = ((dcache -> miss_penalty) * s.miss) + 1;
        s.contrib = 0.0;
        dcache -> print_vector.push_back(s);

        total_data_miss += s.miss_cycle;

    }

    for (map<unsigned int, stat>::iterator it = (icache -> cacheloadstat).begin();
            it != (icache->cacheloadstat).end(); ++it)
    {

        print_stat s;
        s.pc = it->first;
        s.read = true;
        s.ref = it->second.hit + it->second.miss;
        s.miss = it->second.miss;
        if (dcache->cacheloadstat.count(s.pc)) {
            s.miss_cycle += ((dcache->cacheloadstat[s.pc].miss * dcache->miss_penalty) - 1);
        }
        if (dcache->cachestorestat.count(s.pc)) {
            s.miss_cycle += ((dcache->cachestorestat[s.pc].miss * dcache->miss_penalty) - 1);
        }
        s.miss_rate = ((float)s.miss) / s.ref;
        s.miss_cycle = ((dcache -> miss_penalty) * s.miss) + 1;
        s.contrib = 0.0;
        icache -> print_vector.push_back(s);

        total_instr_miss += s.miss_cycle;
        total_references += s.ref;
    }

    // iterate through the vector and add contrib.
    for (unsigned int i = 0; i < dcache -> print_vector.size(); ++i)
    {
        dcache->print_vector.at(i).contrib = ((float) dcache->print_vector.at(i).miss) / total_data_miss;
    }

    // sort vector as per misses for printing.
    sort(dcache -> print_vector.begin(), dcache -> print_vector.end(), greater<print_stat>());


    // iterate through the vector and add contrib.
    for (unsigned int i = 0; i < icache -> print_vector.size(); ++i)
    {
        icache->print_vector.at(i).contrib = ((float) icache->print_vector.at(i).miss) / total_instr_miss;
    }

    // sort vector as per misses for printing.
    sort(icache -> print_vector.begin(), icache -> print_vector.end(), greater<print_stat>());

    OutFile << endl << endl;
    OutFile << "Overall Performance Breakdown:" << endl;
    OutFile << "================================" << endl;
    char log_str[512];
    unsigned long total_execution = total_references + total_data_miss + total_instr_miss;
    sprintf(log_str, "Instruction Execution:\t %lu cycles (%f%%)", total_references, (((((float) total_references)) / total_execution) * 100));
    OutFile << log_str << endl;
    sprintf(log_str, "Data Cache Stalls:\t %lu cycles (%f%%)", total_data_miss, (((((float) total_data_miss)) / total_execution) * 100));
    OutFile << log_str << endl;
    sprintf(log_str, "Instruction Cache Stalls:\t %lu cycles (%f%%)", total_instr_miss, (((((float) total_instr_miss)) / total_execution) * 100));
    OutFile << log_str << endl;
    sprintf(log_str, "------------------------------------------------------");
    OutFile << log_str << endl;
    sprintf(log_str, "Total Execution:\t %lu cycles", total_execution);
    OutFile << log_str << endl;

    OutFile << endl << endl;
    OutFile << "ICache Instruction Wise Breakup:" << endl;
    OutFile << "================================" << endl;
    OutFile << "PC, Type, References, Misses, Miss Rate, Total Miss Cycles, Contribution to total data miss cycles" << endl;
    for (unsigned int i = 0; i < icache -> print_vector.size(); ++i)
    {
        print_stat s = icache -> print_vector.at(i);
        char log_str[512];
        sprintf(log_str, "%x, %d, %lu, %u, %f, %lu, %f", s.pc, s.read, s.ref, s.miss, s.miss_rate, s.miss_cycle, s.contrib);
        OutFile << log_str << endl;
    }

    OutFile << endl << endl;
    OutFile << "DCache Instruction Wise Breakup:" << endl;
    OutFile << "================================" << endl;
    OutFile << "PC, Type, References, Misses, Miss Rate, Total Miss Cycles, Contribution to total data miss cycles" << endl;

    for (unsigned int i = 0; i < dcache -> print_vector.size(); ++i)
    {
        print_stat s = dcache -> print_vector.at(i);
        char log_str[512];
        sprintf(log_str, "%x, %d, %lu, %u, %f, %lu, %f", s.pc, s.read, s.ref, s.miss, s.miss_rate, s.miss_cycle, s.contrib);
        OutFile << log_str << endl;
    }

    OutFile.close();
}


INT32 Usage()
{
    cerr << "This tool tests memory address translation callback assert" << endl;
    cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}


int main(int argc, char * argv[])
{
    // Initialize pin
    if (PIN_Init(argc, argv)) return Usage();
    PIN_InitSymbols();
    OutFile.open(KnobOutputFile.Value().c_str());

    // Register Instruction to be called to instrument instructions
    IMG_AddInstrumentFunction(Image, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}





