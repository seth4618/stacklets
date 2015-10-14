# List of papers

Here there is a list of the papers we should look at along with some
descriptive info.  Or, there are links to the papers if not
downloaded.

## Patents

- [flexible ULI](https://www.google.com/patents/US8285904)
- [ULI on SMT](https://www.google.com/patents/US7996593)
- [delivering ULI to another process](https://www.google.com/patents/US8356130)

## [Case for User-Level Interrupts](http://dl.acm.org/citation.cfm?id=571675), 2002

Why user level interrupts are good for I/O

## [Concurrent Event Handling through Multithreading](http://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=795220&tag=1), 1999

Exceptions have traditionally been used to handle infrequently
occurring and unpredictable events during normal program
execution. Current trends in microprocessor and operating systems
design continue to increase the cost of event handling. Because of the
deep pipelines and wide out-of-order superscalar architectures of
contemporary microprocessors, an event may need to nullify a large
number of in-flight instructions. Large register files require
existing software systems to save and restore a substantial amount of
process state before executing an exception handler. At the same time,
processors are executing in environments that supply higher event
frequencies and demand higher performance. We have developed an
alternative architecture, Concurrent Event Handling, that incorporates
multithreading into event handling architectures. Instead of handling
the event in the faulting thread's architectural and pipeline
registers, the fault handler is forked into its own thread slot and
executes concurrently with the faulting thread.  Microbenchmark
programs show a factor of 3 speedup for concurrent event handling over
a traditional architecture on code that takes frequent exceptions. We
also demonstrate substantial speedups on two event-based
applications. Concurrent Event Handling is implemented in the MIT
Multi-ALU Processor (MAP) chip.

## [SMTp: An Architecture for Next-generation Scalable Multi-threading](http://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=1310769), 2004 

We introduce the SMTp architecture—an SMT processor
augmented with a coherence protocol thread context,
that together with a standard integrated memory controller
can enable the design of (among other possibilities) scalable
cache-coherent hardware distributed shared memory
(DSM) machines from commodity nodes. We describe the
minor changes needed to a conventional out-of-order multithreaded
core to realize SMTp, discussing issues related to
both deadlock avoidance and performance. We then compare
SMTp performance to that of various conventional
DSM machines with normal SMT processors both with and
without integrated memory controllers. On configurations
from 1 to 32 nodes, with 1 to 4 application threads per
node, we find that SMTp delivers performance comparable
to, and sometimes better than, machines with more complex
integrated DSM-specific memory controllers. Our results
also show that the protocol thread has extremely low
pipeline overhead. Given the simplicity and the flexibility of
the SMTp mechanism, we argue that next-generation multithreaded
processors with integrated memory controllers
should adopt this mechanism as a way of building less complex
high-performance DSM multiprocessors.

# [In-line Interrupt Handling and Lockup Free TLBs](http://www.jaleels.org/ajaleel/publications/ajaleel-MS-thesis.pdf)

approaches to ULI using a TLB miss as an example. Master's thesis (see below)

# [In-line interrupt handling and lock-up free translation lookaside buffers (TLBs)](http://ieeexplore.ieee.org/xpls/abs_all.jsp?arnumber=1613837&tag=1), 2006

[Also the related](http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.133.6650&rep=rep1&type=pdf)

The effects of the general-purpose precise interrupt mechanisms in use for the past few decades have received very little attention. When modern out-of-order processors handle interrupts precisely, they typically begin by flushing the pipeline to make the CPU available to execute handler instructions. In doing so, the CPU ends up flushing many instructions that have been brought in to the reorder buffer. In particular, these instructions may have reached a very deep stage in the pipeline - representing significant work that is wasted. In addition, an overhead of several cycles and wastage of energy (per exception detected) can be expected in refetching and reexecuting the instructions flushed. This paper concentrates on improving the performance of precisely handling software managed translation look-aside buffer (TLB) interrupts, one of the most frequently occurring interrupts. The paper presents a novel method of in-lining the interrupt handler within the reorder buffer. Since the first level interrupt-handlers of TLBs are usually small, they could potentially fit in the reorder buffer along with the user-level code already there. In doing so, the instructions that would otherwise be flushed from the pipe need not be refetched and reexecuted. Additionally, it allows for instructions independent of the exceptional instruction to continue to execute in parallel with the handler code. By in-lining the TLB interrupt handler, this provides lock-up free TLBs. This paper proposes the prepend and append schemes of in-lining the interrupt handler into the available reorder buffer space. The two schemes are implemented on a performance model of the Alpha 21264 processor built by Alpha designers at the Palo Alto Design Center (PADC), California. We compare the overhead and performance impact of handling TLB interrupts by the traditional scheme, the append in-lined scheme, and the prepend in-lined scheme. For small, medium, and large memory footprints, the overhead is quantified by comparing the number and pipeline state of instructions flushed, the energy savings, and the performance improvements. We find that lock-up free TLBs reduce the overhead of refetching and reexecuting the instructions flushed by 30-95 percent, reduce the execution time by 5-25 percent, and al- so reduce the energy wasted by 30-90 percent.

# [User-level Device Drivers: Achieved Performance](https://ssrg.nicta.com.au/publications/papers/Leslie_CFGGMPSEH_05-tr.pdf), 2005

Implementing user level device drivers
