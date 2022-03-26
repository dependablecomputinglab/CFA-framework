/*
 * Copyright (c) 2013-2014 ARM Limited
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

/**
 * @file
 *
 *  The constructed pipeline.  Kept out of MinorCPU to keep the interface
 *  between the CPU and its grubby implementation details clean.
 */

#ifndef __CPU_MINOR_PIPELINE_HH__
#define __CPU_MINOR_PIPELINE_HH__

#include "cpu/minor/activity.hh"
#include "cpu/minor/cpu.hh"
#include "cpu/minor/decode.hh"
#include "cpu/minor/execute.hh"
#include "cpu/minor/fetch1.hh"
#include "cpu/minor/fetch2.hh"
#include "params/MinorCPU.hh"
#include "sim/ticked_object.hh"

namespace Minor
{

/**
 * @namespace Minor
 *
 * Minor contains all the definitions within the MinorCPU apart from the CPU
 * class itself
 */

/** The constructed pipeline.  Kept out of MinorCPU to keep the interface
 *  between the CPU and its grubby implementation details clean. */
class Pipeline : public Ticked
{
  // JONGHO
  public:
    enum DataFlow
    {
        INPUT,
        OUTPUT
    };

  // JONGHO
  protected:
    /* Draw pipeline datapath */
    static void drawDatapath(std::ostream& os);

    /* Draw ascii-arted picture of pipeline data flow */
    void drawDataflow(std::ostream& os, DataFlow flow) const;

    /* Printout bubbles in pipeline registers */
    void pipeRegBubble(std::ostream& os) const;

    unsigned long last_snapshot_time = 0;

  // JONGHO
  public:
    void checkDebugFlags();
    void checkAssertions();

    /* Override method regStats() of class Ticked */
    void regStats() override;

    /* Statistics */
    Stats::Scalar snapshot_count;

    /*
     * To profile how long time pipeline register is bubble
     *
     * Note that there is case that non-bubble is not vulnerable
     */
    Stats::Scalar f1ToF2_bubble_ticks;
    Stats::Formula f1ToF2_bubble_ticks_percentage;
    Stats::Scalar f2ToD_bubble_ticks;
    Stats::Formula f2ToD_bubble_ticks_percentage;
    Stats::Scalar dToE_bubble_ticks;
    Stats::Formula dToE_bubble_ticks_percentage;
    Stats::Scalar eToF1_bubble_ticks;
    Stats::Formula eToF1_bubble_ticks_percentage;
    Stats::Scalar f2ToF1_bubble_ticks;
    Stats::Formula f2ToF1_bubble_ticks_percentage;

    /**/
    Stats::Scalar f2ToF1_predT_ticks;
    Stats::Scalar eToF1_predT_T_ticks;
    Stats::Scalar eToF1_predT_WT_ticks;
    Stats::Scalar eToF1_predT_NT_ticks;
    Stats::Scalar eToF1_predNT_T_ticks;
    //Stats::Scalar eToF1_predT_dropped_ticks;

    /*
     * STATS WHICH NEVER DEPEND ON HARDWARE
     */
    enum DynBranchInstType {
        UNCOND,
        COND_TAKEN,
        COND_NOTTAKEN,
        NUM_DYN_BRANCH_INST_TYPE
    };

    Stats::Vector dyn_branch_inst_type;

    /* */
    enum BranchResult {
        BRANCH_TAKEN,
        BRANCH_NOTTAKEN,
        NUM_BRANCH_RESULT
    };
    enum BPResult {
        BP_TAKEN,
        BP_NOTTAKEN,
        BP_WRONGTAKEN,
        NUM_BP_RESULT
    };

    Stats::Vector2d eToF1_count;
    //Stats::Vector2d eToF1_ticks;

    Stats::Scalar predT_count;
    Stats::Scalar predT_T_count;
    Stats::Scalar predT_WT_count;
    Stats::Scalar predT_NT_count;
    Stats::Scalar predNT_T_count;
    //Stats::Scalar predT_dropped_count;

    /* P(T|pred-T)*/
    Stats::Formula prob_T_given_predT_percentage;

    /* P(NT|pred-T)*/
    Stats::Formula prob_NT_given_predT_percentage;

    /* P(WT|pred-T)*/
    Stats::Formula prob_WT_given_predT_percentage;

    /* Time in which data in [E->$] is vulnerable */
    Stats::Formula eToF1_vul_ticks;
    Stats::Formula eToF1_vul_ticks_percentage;

    /* Time in which data in [E->$] or [F->$] is vulnerable */
    Stats::Formula addr_vul_ticks;
    Stats::Formula addr_vul_ticks_percentage;

    /* P([E->$] Vul | Addr Vul) */
    Stats::Formula eToF1_vul_given_addr_vul_ticks_percentage;

    /* P([F->$] Vul | Addr Vul) */
    Stats::Formula f2ToF1_vul_given_addr_vul_ticks_percentage;

  protected:
    MinorCPU &cpu;

    /** Allow cycles to be skipped when the pipeline is idle */
    bool allow_idling;

    Latch<ForwardLineData> f1ToF2;
    Latch<BranchData> f2ToF1;
    Latch<ForwardInstData> f2ToD;
    Latch<ForwardInstData> dToE;
    Latch<BranchData> eToF1;

    Execute execute;
    Decode decode;
    Fetch2 fetch2;
    Fetch1 fetch1;

    /** Activity recording for the pipeline.  This is access through the CPU
     *  by the pipeline stages but belongs to the Pipeline as it is the
     *  cleanest place to initialise it */
    MinorActivityRecorder activityRecorder;

  public:
    /** Enumerated ids of the 'stages' for the activity recorder */
    enum StageId
    {
        /* A stage representing wakeup of the whole processor */
        CPUStageId = 0,
        /* Real pipeline stages */
        Fetch1StageId, Fetch2StageId, DecodeStageId, ExecuteStageId,
        Num_StageId /* Stage count */
    };

    /** True after drain is called but draining isn't complete */
    bool needToSignalDrained;

  public:
    Pipeline(MinorCPU &cpu_, MinorCPUParams &params);

  public:
    /** Wake up the Fetch unit.  This is needed on thread activation esp.
     *  after quiesce wakeup */
    void wakeupFetch(ThreadID tid);

    /** Try to drain the CPU */
    bool drain();

    void drainResume();

    /** Test to see if the CPU is drained */
    bool isDrained();

    /** A custom evaluate allows report in the right place (between
     *  stages and pipeline advance) */
    void evaluate() override;

    void countCycles(Cycles delta) override
    {
        cpu.ppCycles->notify(delta);
    }

    void minorTrace() const;

    /** Functions below here are BaseCPU operations passed on to pipeline
     *  stages */

    /** Return the IcachePort belonging to Fetch1 for the CPU */
    MinorCPU::MinorCPUPort &getInstPort();
    /** Return the DcachePort belonging to Execute for the CPU */
    MinorCPU::MinorCPUPort &getDataPort();

    /** To give the activity recorder to the CPU */
    MinorActivityRecorder *getActivityRecorder() { return &activityRecorder; }
};

}

#endif /* __CPU_MINOR_PIPELINE_HH__ */
