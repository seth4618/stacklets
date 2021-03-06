/*
 * Copyright (c) 2010-2012 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2011 Advanced Micro Devices, Inc.
 * Copyright (c) 2003-2006 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Nathan Binkert
 */

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <fstream>
#include <string>
#include <vector>

#include "arch/x86/stacktrace.hh"
#include "sim/global_msg.hh"
#include "arch/kernel_stats.hh"
#include "arch/utility.hh"
#include "arch/vtophys.hh"
#include "arch/pseudo_inst.hh"
#include "base/debug.hh"
#include "base/output.hh"
#include "config/the_isa.hh"
#include "cpu/base.hh"
#include "cpu/quiesce_event.hh"
#include "cpu/thread_context.hh"
#include "debug/Loader.hh"
#include "debug/PseudoInst.hh"
#include "debug/Quiesce.hh"
#include "debug/WorkItems.hh"
#include "params/BaseCPU.hh"
#include "sim/full_system.hh"
#include "sim/process.hh"
#include "sim/pseudo_inst.hh"
#include "sim/serialize.hh"
#include "sim/sim_events.hh"
#include "sim/sim_exit.hh"
#include "sim/stat_control.hh"
#include "sim/stats.hh"
#include "sim/system.hh"
#include "sim/vptr.hh"
#include "debug/Stacklet.hh"

using namespace std;

using namespace Stats;
using namespace TheISA;

namespace PseudoInst {

static inline void
panicFsOnlyPseudoInst(const char *name)
{
    panic("Pseudo inst \"%s\" is only available in Full System mode.");
}

uint64_t
pseudoInst(ThreadContext *tc, uint8_t func, uint8_t subfunc)
{
    uint64_t args[4];

    DPRINTF(PseudoInst, "PseudoInst::pseudoInst(%i, %i)\n", func, subfunc);

    // We need to do this in a slightly convoluted way since
    // getArgument() might have side-effects on arg_num. We could have
    // used the Argument class, but due to the possible side effects
    // from getArgument, it'd most likely break.
    int arg_num(0);
    for (int i = 0; i < sizeof(args) / sizeof(*args); ++i) {
        args[arg_num] = getArgument(tc, arg_num, sizeof(uint64_t), false);
        ++arg_num;
    }

    switch (func) {
      case 0x00: // arm_func
        arm(tc);
        break;

      case 0x01: // quiesce_func
        quiesce(tc);
        break;

      case 0x02: // quiescens_func
        quiesceSkip(tc);
        break;

      case 0x03: // quiescecycle_func
        quiesceNs(tc, args[0]);
        break;

      case 0x04: // quiescetime_func
        return quiesceTime(tc);

      case 0x07: // rpns_func
        return rpns(tc);

      case 0x09: // wakecpu_func
        wakeCPU(tc, args[0]);
        break;

      case 0x21: // exit_func
        m5exit(tc, args[0]);
        break;

      case 0x22:
        m5fail(tc, args[0], args[1]);
        break;

      case 0x30: // initparam_func
        return initParam(tc);

      case 0x31: // loadsymbol_func
        loadsymbol(tc);
        break;

      case 0x40: // resetstats_func
        resetstats(tc, args[0], args[1]);
        break;

      case 0x41: // dumpstats_func
        dumpstats(tc, args[0], args[1]);
        break;

      case 0x42: // dumprststats_func
        dumpresetstats(tc, args[0], args[1]);
        break;

      case 0x43: // ckpt_func
        m5checkpoint(tc, args[0], args[1]);
        break;

      case 0x4f: // writefile_func
        return writefile(tc, args[0], args[1], args[2], args[3]);

      case 0x50: // readfile_func
        return readfile(tc, args[0], args[1], args[2]);

      case 0x51: // debugbreak_func
        debugbreak(tc);
        break;

      case 0x52: // switchcpu_func
        switchcpu(tc);
        break;

      case 0x53: // addsymbol_func
        addsymbol(tc, args[0], args[1]);
        break;

      case 0x54: // panic_func
        panic("M5 panic instruction called at %s\n", tc->pcState());

      case 0x5a: // work_begin_func
        workbegin(tc, args[0], args[1]);
        break;

      case 0x5b: // work_end_func
        workend(tc, args[0], args[1]);
        break;
      
      case 0x55:
        stacklet_uli_toggle(tc, args[0], args[1]);
        break;

      case 0x56:
        //stacklet_dui(tc, args[0]);
        stacklet_setupuli(tc, args[0]);
        break;

      case 0x57:
        stacklet_sendi(tc, args[0], args[1],args[2]);
        break;

      case 0x58:
        stacklet_getcpuid(tc);
        break;

      case 0x59:
        stacklet_retuli(tc);
        break;

#if 0
      case 0x55: // annotate_func
      case 0x56: // stacklets
        DPRINTF(PseudoInst, "Trying to execute stacklet functions\n");
        switch(subfunc) {
          case 0x00:
            stacklet_eui(tc, args[0]);
            break;
          case 0x01:
            stacklet_dui(tc, args[0]);
            break;
          case 0x02:
            stacklet_sendi(tc, args[0], args[1]);
            break;
          case 0x03:
            stacklet_moviadr(tc, args[0]);
            break;
          case 0x04:
            stacklet_retuli(tc);
            break;
          case 0x05:
            stacklet_getcpuid(tc);
            break;
        }
        break;

      case 0x57: // reserved3_func
      case 0x58: // reserved4_func
      case 0x59: // reserved5_func
        warn("Unimplemented m5 op (0x%x)\n", func);
        break;
#endif
      /* SE mode functions */
      case 0x60: // syscall_func
        m5Syscall(tc);
        break;

      case 0x61: // pagefault_func
        m5PageFault(tc);
        break;

      default:
        warn("Unhandled m5 op: 0x%x\n", func);
        break;
    }

    return 0;
}

void
arm(ThreadContext *tc)
{
    DPRINTF(PseudoInst, "PseudoInst::arm()\n");
    if (!FullSystem)
        panicFsOnlyPseudoInst("arm");

    if (tc->getKernelStats())
        tc->getKernelStats()->arm();
}

void
quiesce(ThreadContext *tc)
{
    DPRINTF(PseudoInst, "PseudoInst::quiesce()\n");
    if (!FullSystem)
        panicFsOnlyPseudoInst("quiesce");

    if (!tc->getCpuPtr()->params()->do_quiesce)
        return;

    DPRINTF(Quiesce, "%s: quiesce()\n", tc->getCpuPtr()->name());

    tc->suspend();
    if (tc->getKernelStats())
        tc->getKernelStats()->quiesce();
}

void
quiesceSkip(ThreadContext *tc)
{
    DPRINTF(PseudoInst, "PseudoInst::quiesceSkip()\n");
    if (!FullSystem)
        panicFsOnlyPseudoInst("quiesceSkip");

    BaseCPU *cpu = tc->getCpuPtr();

    if (!cpu->params()->do_quiesce)
        return;

    EndQuiesceEvent *quiesceEvent = tc->getQuiesceEvent();

    Tick resume = curTick() + 1;

    cpu->reschedule(quiesceEvent, resume, true);

    DPRINTF(Quiesce, "%s: quiesceSkip() until %d\n",
            cpu->name(), resume);

    tc->suspend();
    if (tc->getKernelStats())
        tc->getKernelStats()->quiesce();
}

void
quiesceNs(ThreadContext *tc, uint64_t ns)
{
    DPRINTF(PseudoInst, "PseudoInst::quiesceNs(%i)\n", ns);
    if (!FullSystem)
        panicFsOnlyPseudoInst("quiesceNs");

    BaseCPU *cpu = tc->getCpuPtr();

    if (!cpu->params()->do_quiesce || ns == 0)
        return;

    EndQuiesceEvent *quiesceEvent = tc->getQuiesceEvent();

    Tick resume = curTick() + SimClock::Int::ns * ns;

    cpu->reschedule(quiesceEvent, resume, true);

    DPRINTF(Quiesce, "%s: quiesceNs(%d) until %d\n",
            cpu->name(), ns, resume);

    tc->suspend();
    if (tc->getKernelStats())
        tc->getKernelStats()->quiesce();
}

void
quiesceCycles(ThreadContext *tc, uint64_t cycles)
{
    DPRINTF(PseudoInst, "PseudoInst::quiesceCycles(%i)\n", cycles);
    if (!FullSystem)
        panicFsOnlyPseudoInst("quiesceCycles");

    BaseCPU *cpu = tc->getCpuPtr();

    if (!cpu->params()->do_quiesce || cycles == 0)
        return;

    EndQuiesceEvent *quiesceEvent = tc->getQuiesceEvent();

    Tick resume = cpu->clockEdge(Cycles(cycles));

    cpu->reschedule(quiesceEvent, resume, true);

    DPRINTF(Quiesce, "%s: quiesceCycles(%d) until %d\n",
            cpu->name(), cycles, resume);

    tc->suspend();
    if (tc->getKernelStats())
        tc->getKernelStats()->quiesce();
}

uint64_t
quiesceTime(ThreadContext *tc)
{
    DPRINTF(PseudoInst, "PseudoInst::quiesceTime()\n");
    if (!FullSystem) {
        panicFsOnlyPseudoInst("quiesceTime");
        return 0;
    }

    return (tc->readLastActivate() - tc->readLastSuspend()) /
        SimClock::Int::ns;
}

uint64_t
rpns(ThreadContext *tc)
{
    DPRINTF(PseudoInst, "PseudoInst::rpns()\n");
    return curTick() / SimClock::Int::ns;
}

void
wakeCPU(ThreadContext *tc, uint64_t cpuid)
{
    DPRINTF(PseudoInst, "PseudoInst::wakeCPU(%i)\n", cpuid);
    System *sys = tc->getSystemPtr();
    ThreadContext *other_tc = sys->threadContexts[cpuid];
    if (other_tc->status() == ThreadContext::Suspended)
        other_tc->activate();
}

void
m5exit(ThreadContext *tc, Tick delay)
{
    DPRINTF(PseudoInst, "PseudoInst::m5exit(%i)\n", delay);
    Tick when = curTick() + delay * SimClock::Int::ns;
    exitSimLoop("m5_exit instruction encountered", 0, when, 0, true);
}

void
m5fail(ThreadContext *tc, Tick delay, uint64_t code)
{
    DPRINTF(PseudoInst, "PseudoInst::m5fail(%i, %i)\n", delay, code);
    Tick when = curTick() + delay * SimClock::Int::ns;
    exitSimLoop("m5_fail instruction encountered", code, when, 0, true);
}

void
loadsymbol(ThreadContext *tc)
{
    DPRINTF(PseudoInst, "PseudoInst::loadsymbol()\n");
    if (!FullSystem)
        panicFsOnlyPseudoInst("loadsymbol");

    const string &filename = tc->getCpuPtr()->system->params()->symbolfile;
    if (filename.empty()) {
        return;
    }

    std::string buffer;
    ifstream file(filename.c_str());

    if (!file)
        fatal("file error: Can't open symbol table file %s\n", filename);

    while (!file.eof()) {
        getline(file, buffer);

        if (buffer.empty())
            continue;

        string::size_type idx = buffer.find(' ');
        if (idx == string::npos)
            continue;

        string address = "0x" + buffer.substr(0, idx);
        eat_white(address);
        if (address.empty())
            continue;

        // Skip over letter and space
        string symbol = buffer.substr(idx + 3);
        eat_white(symbol);
        if (symbol.empty())
            continue;

        Addr addr;
        if (!to_number(address, addr))
            continue;

        if (!tc->getSystemPtr()->kernelSymtab->insert(addr, symbol))
            continue;


        DPRINTF(Loader, "Loaded symbol: %s @ %#llx\n", symbol, addr);
    }
    file.close();
}

void
addsymbol(ThreadContext *tc, Addr addr, Addr symbolAddr)
{
    DPRINTF(PseudoInst, "PseudoInst::addsymbol(0x%x, 0x%x)\n",
            addr, symbolAddr);
    if (!FullSystem)
        panicFsOnlyPseudoInst("addSymbol");

    char symb[100];
    CopyStringOut(tc, symb, symbolAddr, 100);
    std::string symbol(symb);

    DPRINTF(Loader, "Loaded symbol: %s @ %#llx\n", symbol, addr);

    tc->getSystemPtr()->kernelSymtab->insert(addr,symbol);
    debugSymbolTable->insert(addr,symbol);
}

uint64_t
initParam(ThreadContext *tc)
{
    DPRINTF(PseudoInst, "PseudoInst::initParam()\n");
    if (!FullSystem) {
        panicFsOnlyPseudoInst("initParam");
        return 0;
    }

    return tc->getCpuPtr()->system->init_param;
}


void
resetstats(ThreadContext *tc, Tick delay, Tick period)
{
    DPRINTF(PseudoInst, "PseudoInst::resetstats(%i, %i)\n", delay, period);
    if (!tc->getCpuPtr()->params()->do_statistics_insts)
        return;


    Tick when = curTick() + delay * SimClock::Int::ns;
    Tick repeat = period * SimClock::Int::ns;

    Stats::schedStatEvent(false, true, when, repeat);
}

void
dumpstats(ThreadContext *tc, Tick delay, Tick period)
{
    DPRINTF(PseudoInst, "PseudoInst::dumpstats(%i, %i)\n", delay, period);
    if (!tc->getCpuPtr()->params()->do_statistics_insts)
        return;


    Tick when = curTick() + delay * SimClock::Int::ns;
    Tick repeat = period * SimClock::Int::ns;

    Stats::schedStatEvent(true, false, when, repeat);
}

void
dumpresetstats(ThreadContext *tc, Tick delay, Tick period)
{
    DPRINTF(PseudoInst, "PseudoInst::dumpresetstats(%i, %i)\n", delay, period);
    if (!tc->getCpuPtr()->params()->do_statistics_insts)
        return;


    Tick when = curTick() + delay * SimClock::Int::ns;
    Tick repeat = period * SimClock::Int::ns;

    Stats::schedStatEvent(true, true, when, repeat);
}

void
m5checkpoint(ThreadContext *tc, Tick delay, Tick period)
{
    DPRINTF(PseudoInst, "PseudoInst::m5checkpoint(%i, %i)\n", delay, period);
    if (!tc->getCpuPtr()->params()->do_checkpoint_insts)
        return;

    Tick when = curTick() + delay * SimClock::Int::ns;
    Tick repeat = period * SimClock::Int::ns;

    exitSimLoop("checkpoint", 0, when, repeat);
}

uint64_t
readfile(ThreadContext *tc, Addr vaddr, uint64_t len, uint64_t offset)
{
    DPRINTF(PseudoInst, "PseudoInst::readfile(0x%x, 0x%x, 0x%x)\n",
            vaddr, len, offset);
    if (!FullSystem) {
        panicFsOnlyPseudoInst("readfile");
        return 0;
    }

    const string &file = tc->getSystemPtr()->params()->readfile;
    if (file.empty()) {
        return ULL(0);
    }

    uint64_t result = 0;

    int fd = ::open(file.c_str(), O_RDONLY, 0);
    if (fd < 0)
        panic("could not open file %s\n", file);

    if (::lseek(fd, offset, SEEK_SET) < 0)
        panic("could not seek: %s", strerror(errno));

    char *buf = new char[len];
    char *p = buf;
    while (len > 0) {
        int bytes = ::read(fd, p, len);
        if (bytes <= 0)
            break;

        p += bytes;
        result += bytes;
        len -= bytes;
    }

    close(fd);
    CopyIn(tc, vaddr, buf, result);
    delete [] buf;
    return result;
}

uint64_t
writefile(ThreadContext *tc, Addr vaddr, uint64_t len, uint64_t offset,
            Addr filename_addr)
{
    DPRINTF(PseudoInst, "PseudoInst::writefile(0x%x, 0x%x, 0x%x, 0x%x)\n",
            vaddr, len, offset, filename_addr);
    ostream *os;

    // copy out target filename
    char fn[100];
    std::string filename;
    CopyStringOut(tc, fn, filename_addr, 100);
    filename = std::string(fn);

    if (offset == 0) {
        // create a new file (truncate)
        os = simout.create(filename, true);
    } else {
        // do not truncate file if offset is non-zero
        // (ios::in flag is required as well to keep the existing data
        //  intact, otherwise existing data will be zeroed out.)
        os = simout.openFile(simout.directory() + filename,
                            ios::in | ios::out | ios::binary);
    }
    if (!os)
        panic("could not open file %s\n", filename);

    // seek to offset
    os->seekp(offset);

    // copy out data and write to file
    char *buf = new char[len];
    CopyOut(tc, buf, vaddr, len);
    os->write(buf, len);
    if (os->fail() || os->bad())
        panic("Error while doing writefile!\n");

    simout.close(os);

    delete [] buf;

    return len;
}

void
debugbreak(ThreadContext *tc)
{
    DPRINTF(PseudoInst, "PseudoInst::debugbreak()\n");
    Debug::breakpoint();
}

void
switchcpu(ThreadContext *tc)
{
    DPRINTF(PseudoInst, "PseudoInst::switchcpu()\n");
    exitSimLoop("switchcpu");
}

#if 0
uint64_t
stacklet_eui(ThreadContext *tc, uint16_t mask)
{
  /*
   * In this instruction, we enable user-level interrupts, i.e. typically
   * after completing the handling of a user-level interrupt, we enable
   * user-level interrupts to be open to accepting further interrupts.
   */
  DPRINTF(Stacklet, "PseudoInst::%s called with mask %u\n", __func__, mask);
  return 0;
}

uint64_t
stacklet_dui(ThreadContext *tc, uint16_t mask)
{
  /*
   * In this instruction, we disable user-level interrupts. When we are
   * processing one user-level interrupt, we would like to disable more
   * user-level interrupts and interrupts occuring at the time will be
   * treated as packet drops.
   */
  DPRINTF(Stacklet, "PseudoInst::%s called with mask %u\n", __func__, mask);
  return 0;
}
#endif

uint64_t
stacklet_uli_toggle(ThreadContext *tc, uint32_t enable, uint16_t mask)
{
  /*
   * In this instruction we toggle between enabling and disabling user-level
   * interrupts. If enable == 1, then we enable ULI, otherwise we disable it.
   */
  if(enable) {
    //DPRINTF(Stacklet, "PseudoInst::%s called to enable ULI with mask %u\n", __func__, mask);
  } else {
    //DPRINTF(Stacklet, "PseudoInst::%s called to disable ULI with mask %u\n", __func__, mask);
  }
  return 0;
}

uint64_t
stacklet_sendi(ThreadContext *tc, Addr callback,Addr p, uint16_t dest_cpu)
{
  /*
   * This instruction sends a user-level interrupt to another core.
   */
  //System *sys = tc->getSystemPtr();
  DPRINTF(Stacklet, "PseudoInst::%s source CPU=%d, dest CPU=%d\n", __func__, tc->cpuId(), dest_cpu);
   //X86ISA::ProcessInfo* procInfo = new X86ISA::ProcessInfo(tc);
   //int pid = procInfo->pid(tc->readIntReg(StackPointerReg));
  //DPRINTF(Stacklet, "Pid when sending:%d\n",pid);
  Interrupts * interrupts = dynamic_cast<Interrupts *>(tc->getCpuPtr()->getInterruptController());
  assert(interrupts);
  DPRINTF(Stacklet, "Sendi side: CR3:%0x\n",tc->readMiscReg(MISCREG_CR3));

  /*
  System *glob_sys = tc->getSystemPtr();
  vector<System *>::iterator i = glob_sys->systemList.begin();
  vector<System *>::iterator end = glob_sys->systemList.end();
  for(; i !=end; i++)
  {
    System *sys = *i;
    DPRINTF(Stacklet, "Sysname: %s\n",sys->name());
  }


  Interrupts *dst_interrupts = dynamic_cast<Interrupts *>(glob_sys->getThreadContext(dest_cpu)->getCpuPtr()->getInterruptController());
  assert(dst_interrupts);
*/

  
  TriggerIntMessage message = 0;
  message.deliveryMode = 3; // 3 stands for ULI
  message.vector = 0; // this will be mask eventually
  //dst_interrupts->global_message_counter++;
  
  msg_counter++;
  
  message.global_message_map_key = msg_counter;//interrupts->global_message_counter; // position in the global messages queue
  stacklet_message_t stacklet_msg;
  stacklet_msg.callback = callback;
  stacklet_msg.p = p; // this is assuming 64 bit architecture
  DPRINTF(Stacklet, "PseudoInst:: callback=%0x and p=%0x\n", stacklet_msg.callback, stacklet_msg.p);
  //dst_interrupts->global_message_map[message.global_message_map_key] = stacklet_msg;
  
  msg_map[msg_counter] = stacklet_msg;
  cr3_map[tc->readMiscReg(MISCREG_CR3)] = message.global_message_map_key;
  ApicList apics;
  apics.push_back(dest_cpu);
  TheISA::IntDevice::IntMasterPort intMasterPort = dynamic_cast<TheISA::IntDevice::IntMasterPort& >(interrupts->getMasterPort("int_master"));
  intMasterPort.sendMessage(apics, message, false);
  return 0;
}

uint64_t
stacklet_moviadr(ThreadContext *tc, Addr addr)
{
  /*
   * This instruction moves the interrupt-handler address to a special
   * register (or variable, in the case of a simulator).
   */
  DPRINTF(Stacklet, "PseudoInst::%s\n", __func__);
  tc->savedULIPC = addr;
  return 0;
}

uint64_t
stacklet_retuli(ThreadContext *tc)
{
  /*
   * This instruction denotes the return from a user-level interrupt
   * handler.
   */
  DPRINTF(Stacklet, "PseudoInst::%s\n", __func__);
  DPRINTF(Stacklet,"Setting pc: %0x, RSP: %0x, RDI: %0x, RFLAGS: %0x, thread_ctx: %0x\n",(uint64_t)(tc->savedULIPC),(uint64_t)(tc->savedULISP),
                              (uint64_t)(tc->savedULIDI), (uint64_t)(tc->savedULIRFLAGS), (uint64_t)tc);

  if(tc->was_kernel_mode)
  {
    tc->setMiscReg(MISCREG_CS, 0x10);
    tc->setMiscReg(MISCREG_DS, 18);
    tc->setMiscReg(MISCREG_ES, 18);
  }
  tc->setIntReg(INTREG_RDI, tc->savedULIDI);
  tc->setIntReg(StackPointerReg, tc->savedULISP);
  tc->setMiscReg(MISCREG_RFLAGS, tc->savedULIRFLAGS);
  tc->pcState(tc->savedULIPC);
  return 0;
}

uint64_t
stacklet_getcpuid(ThreadContext *tc)
{
  /*
   * This instruction sends a user-level interrupt to another core.
   */
  //DPRINTF(Stacklet, "PseudoInst::%s\n", __func__);
  return tc->cpuId();
}

uint64_t
stacklet_setupuli(ThreadContext *tc, Addr stackaddr)
{
  /*
   * This instruction sets up a stack for handling ULIs.
   */
  DPRINTF(Stacklet, "PseudoInst::%s\n stack address = %0x\n", __func__, stackaddr);
  tc->ULISP = stackaddr;
  DPRINTF(Stacklet, "Inside SETUPULI, CR3:%0x\n",tc->readMiscReg(MISCREG_CR3));
  return 0;
}

#if 0
uint64_t
stacklet(ThreadContext *tc, uint64_t arg1, uint64_t arg2, uint8_t subfunc)
{
  DPRINTF(PseudoInst, "PseudoInst::stacklet()\n");
  return arg1 + arg2;
}
#endif
//
// This function is executed when annotated work items begin.  Depending on 
// what the user specified at the command line, the simulation may exit and/or
// take a checkpoint when a certain work item begins.
//
void
workbegin(ThreadContext *tc, uint64_t workid, uint64_t threadid)
{
    DPRINTF(PseudoInst, "PseudoInst::workbegin(%i, %i)\n", workid, threadid);
    tc->getCpuPtr()->workItemBegin();
    System *sys = tc->getSystemPtr();
    const System::Params *params = sys->params();
    sys->workItemBegin(threadid, workid);

    DPRINTF(WorkItems, "Work Begin workid: %d, threadid %d\n", workid, 
            threadid);

    //
    // If specified, determine if this is the specific work item the user
    // identified
    //
    if (params->work_item_id == -1 || params->work_item_id == workid) {

        uint64_t systemWorkBeginCount = sys->incWorkItemsBegin();
        int cpuId = tc->getCpuPtr()->cpuId();

        if (params->work_cpus_ckpt_count != 0 &&
            sys->markWorkItem(cpuId) >= params->work_cpus_ckpt_count) {
            //
            // If active cpus equals checkpoint count, create checkpoint
            //
            exitSimLoop("checkpoint");
        }

        if (systemWorkBeginCount == params->work_begin_ckpt_count) {
            //
            // Note: the string specified as the cause of the exit event must
            // exactly equal "checkpoint" inorder to create a checkpoint
            //
            exitSimLoop("checkpoint");
        }

        if (systemWorkBeginCount == params->work_begin_exit_count) {
            //
            // If a certain number of work items started, exit simulation
            //
            exitSimLoop("work started count reach");
        }

        if (cpuId == params->work_begin_cpu_id_exit) {
            //
            // If work started on the cpu id specified, exit simulation
            //
            exitSimLoop("work started on specific cpu");
        }
    }
}

//
// This function is executed when annotated work items end.  Depending on 
// what the user specified at the command line, the simulation may exit and/or
// take a checkpoint when a certain work item ends.
//
void
workend(ThreadContext *tc, uint64_t workid, uint64_t threadid)
{
    DPRINTF(PseudoInst, "PseudoInst::workend(%i, %i)\n", workid, threadid);
    tc->getCpuPtr()->workItemEnd();
    System *sys = tc->getSystemPtr();
    const System::Params *params = sys->params();
    sys->workItemEnd(threadid, workid);

    DPRINTF(WorkItems, "Work End workid: %d, threadid %d\n", workid, threadid);

    //
    // If specified, determine if this is the specific work item the user
    // identified
    //
    if (params->work_item_id == -1 || params->work_item_id == workid) {

        uint64_t systemWorkEndCount = sys->incWorkItemsEnd();
        int cpuId = tc->getCpuPtr()->cpuId();

        if (params->work_cpus_ckpt_count != 0 &&
            sys->markWorkItem(cpuId) >= params->work_cpus_ckpt_count) {
            //
            // If active cpus equals checkpoint count, create checkpoint
            //
            exitSimLoop("checkpoint");
        }

        if (params->work_end_ckpt_count != 0 &&
            systemWorkEndCount == params->work_end_ckpt_count) {
            //
            // If total work items completed equals checkpoint count, create
            // checkpoint
            //
            exitSimLoop("checkpoint");
        }

        if (params->work_end_exit_count != 0 &&
            systemWorkEndCount == params->work_end_exit_count) {
            //
            // If total work items completed equals exit count, exit simulation
            //
            exitSimLoop("work items exit count reached");
        }
    }
}

} // namespace PseudoInst
