/*
 * Copyright (c) 2012-2014 ARM Limited
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
 * Authors: Andrew Bardsley
 */

#include "arch/utility.hh"
#include "cpu/minor/cpu.hh"
#include "cpu/minor/dyn_inst.hh"
#include "cpu/minor/fetch1.hh"
#include "cpu/minor/pipeline.hh"
#include "debug/Drain.hh"
#include "debug/MinorCPU.hh"
#include "debug/Quiesce.hh"
#include "debug/ShsTemp.hh"
#include "debug/FI.hh"

MinorCPU::MinorCPU(MinorCPUParams *params) :
    BaseCPU(params),
    threadPolicy(params->threadPolicy),
    injectFaultReg(params->injectFaultReg), //HwiSoo: 0-> No injection, 1-> Fault injection
    injectFaultRegHard(params->injectFaultRegHard), //Yohan: 0-> No injection, 1-> Fault injection
    injectTime(params->injectTime),         //HwiSoo: Injection time
    injectLoc(params->injectLoc),           //HwiSoo: Injection location
    injectReg(false),
    injectRegHard(false),
    checkReg(false),
    //traceReg(false),
    checkFaultReg(params->checkFaultReg),    //HwiSoo: 0->No check, 1-> Check
    //ybkim
    injectFaultToFu(params->injectFaultFu),
    isFaultInjectedToFu(false),
    //YOHAN
    correctTime(params->correctTime),
    correctRf(params->correctRf),
        injectFaultLSQ(params->injectFaultLSQ),
    checkFaultLSQ(params->checkFaultLSQ),
        LSQEntrySize(128),
    injectLSQ(false),
    checkLSQ(false)
{
    //YOHAN
    Callback *cb = new MakeCallback<MinorCPU, &MinorCPU::exitCallback>(this);
    registerExitCallback(cb);
    
    /* This is only written for one thread at the moment */
    Minor::MinorThread *thread;

    for (ThreadID i = 0; i < numThreads; i++) {
        if (FullSystem) {
            thread = new Minor::MinorThread(this, i, params->system,
                    params->itb, params->dtb, params->isa[i]);
            thread->setStatus(ThreadContext::Halted);
        } else {
            thread = new Minor::MinorThread(this, i, params->system,
                    params->workload[i], params->itb, params->dtb,
                    params->isa[i]);
        }

        threads.push_back(thread);
        ThreadContext *tc = thread->getTC();
        threadContexts.push_back(tc);
    }


    if (params->checker) {
        fatal("The Minor model doesn't support checking (yet)\n");
    }

    Minor::MinorDynInst::init();

    pipeline = new Minor::Pipeline(*this, *params);
    activityRecorder = pipeline->getActivityRecorder();
}

MinorCPU::~MinorCPU()
{
    delete pipeline;

    for (ThreadID thread_id = 0; thread_id < threads.size(); thread_id++) {
        delete threads[thread_id];
    }
}

void
MinorCPU::init()
{
    BaseCPU::init();

    if (!params()->switched_out &&
        system->getMemoryMode() != Enums::timing)
    {
        fatal("The Minor CPU requires the memory system to be in "
            "'timing' mode.\n");
    }

    /* Initialise the ThreadContext's memory proxies */
    for (ThreadID thread_id = 0; thread_id < threads.size(); thread_id++) {
        ThreadContext *tc = getContext(thread_id);

        tc->initMemProxies(tc);
    }

    /* Initialise CPUs (== threads in the ISA) */
    if (FullSystem && !params()->switched_out) {
        for (ThreadID thread_id = 0; thread_id < threads.size(); thread_id++)
        {
            ThreadContext *tc = getContext(thread_id);

            /* Initialize CPU, including PC */
            TheISA::initCPU(tc, cpuId());
        }
    }
}

/** Stats interface from SimObject (by way of BaseCPU) */
void
MinorCPU::regStats()
{
    BaseCPU::regStats();
    stats.regStats(name(), *this);
    pipeline->regStats();
}

void
MinorCPU::serializeThread(CheckpointOut &cp, ThreadID thread_id) const
{
    threads[thread_id]->serialize(cp);
}

void
MinorCPU::unserializeThread(CheckpointIn &cp, ThreadID thread_id)
{
    threads[thread_id]->unserialize(cp);
}

void
MinorCPU::serialize(CheckpointOut &cp) const
{
    pipeline->serialize(cp);
    BaseCPU::serialize(cp);
}

void
MinorCPU::unserialize(CheckpointIn &cp)
{
    pipeline->unserialize(cp);
    BaseCPU::unserialize(cp);
}

Addr
MinorCPU::dbg_vtophys(Addr addr)
{
    /* Note that this gives you the translation for thread 0 */
    panic("No implementation for vtophy\n");

    return 0;
}

void
MinorCPU::wakeup(ThreadID tid)
{
    DPRINTF(Drain, "[tid:%d] MinorCPU wakeup\n", tid);
    assert(tid < numThreads);

    if (threads[tid]->status() == ThreadContext::Suspended) {
        threads[tid]->activate();
    }
}

void
MinorCPU::startup()
{
    DPRINTF(MinorCPU, "MinorCPU startup\n");

    BaseCPU::startup();

    for (auto i = threads.begin(); i != threads.end(); i ++)
        (*i)->startup();

    for (ThreadID tid = 0; tid < numThreads; tid++) {
        threads[tid]->startup();
        pipeline->wakeupFetch(tid);
    }
}

DrainState
MinorCPU::drain()
{
    if (switchedOut()) {
        DPRINTF(Drain, "Minor CPU switched out, draining not needed.\n");
        return DrainState::Drained;
    }

    DPRINTF(Drain, "MinorCPU drain\n");

    /* Need to suspend all threads and wait for Execute to idle.
     * Tell Fetch1 not to fetch */
    if (pipeline->drain()) {
        DPRINTF(Drain, "MinorCPU drained\n");
        return DrainState::Drained;
    } else {
        DPRINTF(Drain, "MinorCPU not finished draining\n");
        return DrainState::Draining;
    }
}

void
MinorCPU::signalDrainDone()
{
    DPRINTF(Drain, "MinorCPU drain done\n");
    Drainable::signalDrainDone();
}

void
MinorCPU::drainResume()
{
    /* When taking over from another cpu make sure lastStopped
     * is reset since it might have not been defined previously
     * and might lead to a stats corruption */
    pipeline->resetLastStopped();

    if (switchedOut()) {
        DPRINTF(Drain, "drainResume while switched out.  Ignoring\n");
        return;
    }

    DPRINTF(Drain, "MinorCPU drainResume\n");

    if (!system->isTimingMode()) {
        fatal("The Minor CPU requires the memory system to be in "
            "'timing' mode.\n");
    }

    for (ThreadID tid = 0; tid < numThreads; tid++)
        wakeup(tid);

    pipeline->drainResume();
}

void
MinorCPU::memWriteback()
{
    DPRINTF(Drain, "MinorCPU memWriteback\n");
}

void
MinorCPU::switchOut()
{
    DPRINTF(MinorCPU, "MinorCPU switchOut\n");

    assert(!switchedOut());
    BaseCPU::switchOut();

    /* Check that the CPU is drained? */
    activityRecorder->reset();
}

void
MinorCPU::takeOverFrom(BaseCPU *old_cpu)
{
    DPRINTF(MinorCPU, "MinorCPU takeOverFrom\n");

    BaseCPU::takeOverFrom(old_cpu);
}

void
MinorCPU::activateContext(ThreadID thread_id)
{
    DPRINTF(MinorCPU, "ActivateContext thread: %d\n", thread_id);

    /* Do some cycle accounting.  lastStopped is reset to stop the
     *  wakeup call on the pipeline from adding the quiesce period
     *  to BaseCPU::numCycles */
    stats.quiesceCycles += pipeline->cyclesSinceLastStopped();
    pipeline->resetLastStopped();

    /* Wake up the thread, wakeup the pipeline tick */
    threads[thread_id]->activate();
    wakeupOnEvent(Minor::Pipeline::CPUStageId);
    pipeline->wakeupFetch(thread_id);

    BaseCPU::activateContext(thread_id);
}

void
MinorCPU::suspendContext(ThreadID thread_id)
{
    DPRINTF(MinorCPU, "SuspendContext %d\n", thread_id);

    threads[thread_id]->suspend();

    BaseCPU::suspendContext(thread_id);
}

void
MinorCPU::wakeupOnEvent(unsigned int stage_id)
{
    DPRINTF(Quiesce, "Event wakeup from stage %d\n", stage_id);

    /* Mark that some activity has taken place and start the pipeline */
    activityRecorder->activateStage(stage_id);
    pipeline->start();
}

MinorCPU *
MinorCPUParams::create()
{
    return new MinorCPU(this);
}

MasterPort &MinorCPU::getInstPort()
{
    return pipeline->getInstPort();
}

MasterPort &MinorCPU::getDataPort()
{
    return pipeline->getDataPort();
}

Counter
MinorCPU::totalInsts() const
{
    Counter ret = 0;

    for (auto i = threads.begin(); i != threads.end(); i ++)
        ret += (*i)->numInst;

    return ret;
}

Counter
MinorCPU::totalOps() const
{
    Counter ret = 0;

    for (auto i = threads.begin(); i != threads.end(); i ++)
        ret += (*i)->numOp;

    return ret;
}

//HwiSoo: Fault injection into register file
void
MinorCPU::injectFaultRegFunc()
{
    if((checkFaultReg==1 || injectFaultReg==1)&&curTick()>=injectTime) {        
        if(injectFaultReg == 1) {
            injectReg = threads[0]->flipRegFile(injectLoc, &originalRegData);
            traceReg = true;
        }
        else {
            checkReg = true;
            traceReg = true;
        }

        if(injectReg)
        {
            injectFaultReg = 0;
            //add register information to faultyRegs
            RCDBP[injectLoc/32] = originalRegData;
        }
        else {
            if(injectLoc >= threads[0]->totalNumPhysRegs()*32)
                injectLoc = injectLoc - threads[0]->totalNumPhysRegs()*32;
        }
    }
}

//YOHAN: Fault injection (hard error) into register file
void
MinorCPU::injectFaultRegFuncHard()
{
    if((checkFaultRegHard ==1 || injectFaultRegHard ==1)&&curTick()>=injectTime) {        
        if(injectFaultRegHard == 1) {
            injectRegHard = threads[0]->flipRegFileHard(injectLoc, &originalRegData);
        }
        else {
            checkRegHard = true;
        }

        if(injectRegHard)
        {
            injectFaultRegHard = 0;
        }
        else {
            if(injectLoc >= threads[0]->totalNumPhysRegs()*32)
                injectLoc = injectLoc - threads[0]->totalNumPhysRegs()*32;
        }
    }
}

//YOHAN: Exit when corrupted reg is not used
void
MinorCPU::exitCallback()
{
    std::map<int, uint64_t>::iterator itMap;
    
    if(traceReg && injectReg)
        DPRINTF(FI, "Corrupted reg %d is unused\n", injectLoc/32);

    for(itMap = RCDBP.begin(); itMap != RCDBP.end(); itMap++) {
        DPRINTF(FI, "Reg %d is faulty\n", itMap->first);
    }
    
    for(itMap = RCDAP.begin(); itMap != RCDAP.end(); itMap++) {
        DPRINTF(FI, "Reg %d is faulty\n", itMap->first);
    }
}