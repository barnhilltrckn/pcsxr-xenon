/***************************************************************************
*   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.           *
***************************************************************************/

/*
* PSX assembly interpreter.
*/

#include "psxcommon.h"
#include "r3000a.h"
#include "gte.h"
#include "psxhle.h"

#define Read_ICache(x,y) (u32 *)PSXM(x)

//#define TEST_LOAD_DELAY

static int branch = 0;
static int branch2 = 0;
static u32 branchPC;

// These macros are used to assemble the repassembler functions

#ifdef PSXCPU_LOG
#define debugI() PSXCPU_LOG("%s\n", disR3000AF(psxRegs.code, psxRegs.pc)); 
#else
#define debugI()
#endif

static inline void execI();

// Subsets
void (*psxBSC[64])();
void (*psxSPC[64])();
void (*psxREG[32])();
void (*psxCP0[32])();
void (*psxCP2[64])();
void (*psxCP2BSC[32])();

static inline void branchBSC(char opcode);
static inline void branchREG(u32 rt);

static void delayRead(int reg, u32 bpc) {
    u32 rold, rnew;

    rold = psxRegs.GPR.r[reg];
    branchBSC(_Op_); // branch delay load
    rnew = psxRegs.GPR.r[reg];

    psxRegs.pc = bpc;

    branch = 0;

    psxRegs.GPR.r[reg] = rold;	
    execI(); // first branch opcode
    psxRegs.GPR.r[reg] = rnew;

    psxBranchTest();
}

static void delayWrite(int reg, u32 bpc) {
    branchBSC(_Op_);

    branch = 0;
    psxRegs.pc = bpc;

    psxBranchTest();
}

static void delayReadWrite(int reg, u32 bpc) {
    branch = 0;
    psxRegs.pc = bpc;

    psxBranchTest();
}

// this defines shall be used with the tmp 
// of the next func (instead of _Funct_...)
#define _tFunct_  ((tmp      ) & 0x3F)  // The funct part of the instruction register 
#define _tRd_     ((tmp >> 11) & 0x1F)  // The rd part of the instruction register 
#define _tRt_     ((tmp >> 16) & 0x1F)  // The rt part of the instruction register 
#define _tRs_     ((tmp >> 21) & 0x1F)  // The rs part of the instruction register 
#define _tSa_     ((tmp >>  6) & 0x1F)  // The sa part of the instruction register

int psxTestLoadDelay(int reg, u32 tmp) {
    if (tmp == 0) return 0; // NOP
    switch (tmp >> 26) {
        case 0x00: // SPECIAL
            switch (_tFunct_) {
                case 0x00: // SLL
                case 0x02: case 0x03: // SRL/SRA
                    if (_tRd_ == reg && _tRt_ == reg) return 1; else
                    if (_tRt_ == reg) return 2; else
                    if (_tRd_ == reg) return 3;
                    break;

                case 0x08: // JR
                    if (_tRs_ == reg) return 2;
                    break;
                case 0x09: // JALR
                    if (_tRd_ == reg && _tRs_ == reg) return 1; else
                    if (_tRs_ == reg) return 2; else
                    if (_tRd_ == reg) return 3;
                    break;

                // SYSCALL/BREAK just a break;

                case 0x20: case 0x21: case 0x22: case 0x23:
                case 0x24: case 0x25: case 0x26: case 0x27: 
                case 0x2a: case 0x2b: // ADD/ADDU...
                case 0x04: case 0x06: case 0x07: // SLLV...
                    if (_tRd_ == reg && (_tRt_ == reg || _tRs_ == reg)) return 1; else
                    if (_tRt_ == reg || _tRs_ == reg) return 2; else
                    if (_tRd_ == reg) return 3;
                    break;

                case 0x10: case 0x12: // MFHI/MFLO
                    if (_tRd_ == reg) return 3;
                    break;
                case 0x11: case 0x13: // MTHI/MTLO
                    if (_tRs_ == reg) return 2;
                    break;

                case 0x18: case 0x19:
                case 0x1a: case 0x1b: // MULT/DIV...
                    if (_tRt_ == reg || _tRs_ == reg) return 2;
                    break;
            }
            break;

        case 0x01: // REGIMM
            switch (_tRt_) {
                case 0x00: case 0x01:
                case 0x10: case 0x11: // BLTZ/BGEZ...
                    // Xenogears - lbu v0 / beq v0
                    // - no load delay (fixes battle loading)
                    break;

                    if (_tRs_ == reg) return 2;
                    break;
            }
            break;

        // J would be just a break;
        case 0x03: // JAL
            if (31 == reg) return 3;
            break;

        case 0x04: case 0x05: // BEQ/BNE
            // Xenogears - lbu v0 / beq v0
            // - no load delay (fixes battle loading)
            break;

            if (_tRs_ == reg || _tRt_ == reg) return 2;
            break;

        case 0x06: case 0x07: // BLEZ/BGTZ
            // Xenogears - lbu v0 / beq v0
            // - no load delay (fixes battle loading)
            break;

            if (_tRs_ == reg) return 2;
            break;

        case 0x08: case 0x09: case 0x0a: case 0x0b:
        case 0x0c: case 0x0d: case 0x0e: // ADDI/ADDIU...
            if (_tRt_ == reg && _tRs_ == reg) return 1; else
            if (_tRs_ == reg) return 2; else
            if (_tRt_ == reg) return 3;
            break;

        case 0x0f: // LUI
            if (_tRt_ == reg) return 3;
            break;

        case 0x10: // COP0
            switch (_tFunct_) {
                case 0x00: // MFC0
                    if (_tRt_ == reg) return 3;
                    break;
                case 0x02: // CFC0
                    if (_tRt_ == reg) return 3;
                    break;
                case 0x04: // MTC0
                    if (_tRt_ == reg) return 2;
                    break;
                case 0x06: // CTC0
                    if (_tRt_ == reg) return 2;
                    break;
                // RFE just a break;
            }
            break;

        case 0x12: // COP2
            switch (_tFunct_) {
                case 0x00: 
                    switch (_tRs_) {
                        case 0x00: // MFC2
                            if (_tRt_ == reg) return 3;
                            break;
                        case 0x02: // CFC2
                            if (_tRt_ == reg) return 3;
                            break;
                        case 0x04: // MTC2
                            if (_tRt_ == reg) return 2;
                            break;
                        case 0x06: // CTC2
                            if (_tRt_ == reg) return 2;
                            break;
                    }
                    break;
                // RTPS... break;
            }
            break;

        case 0x22: case 0x26: // LWL/LWR
            if (_tRt_ == reg) return 3; else
            if (_tRs_ == reg) return 2;
            break;

        case 0x20: case 0x21: case 0x23:
        case 0x24: case 0x25: // LB/LH/LW/LBU/LHU
            if (_tRt_ == reg && _tRs_ == reg) return 1; else
            if (_tRs_ == reg) return 2; else
            if (_tRt_ == reg) return 3;
            break;

        case 0x28: case 0x29: case 0x2a:
        case 0x2b: case 0x2e: // SB/SH/SWL/SW/SWR
            if (_tRt_ == reg || _tRs_ == reg) return 2;
            break;

        case 0x32: case 0x3a: // LWC2/SWC2
            if (_tRs_ == reg) return 2;
            break;
    }

    return 0;
}

void psxDelayTest(int reg, u32 bpc) {
    u32 tmp;

    // Don't execute yet - just peek
	tmp = __loadwordbytereverse((void*)PSXM(bpc));

    branch = 1;

    switch (psxTestLoadDelay(reg, tmp)) {
        case 1:
            delayReadWrite(reg, bpc); return;
        case 2:
            delayRead(reg, bpc); return;
        case 3:
            delayWrite(reg, bpc); return;
    }
    branchBSC(_Op_);

    branch = 0;
    psxRegs.pc = bpc;

    psxBranchTest();
}

//#ifdef TEST_LOAD_DELAY
#if 1

static u32 psxBranchNoDelay(void) {
    u32 temp;

	psxRegs.code = __loadwordbytereverse((void*)PSXM(psxRegs.pc));
    switch (_Op_) {
        case 0x00: // SPECIAL
            switch (_Funct_) {
                case 0x08: // JR
                    return _u32(_rRs_);
                case 0x09: // JALR
                    temp = _u32(_rRs_);
                    if (_Rd_) { _SetLink(_Rd_); }
                    return temp;
            }
            break;
        case 0x01: // REGIMM
            switch (_Rt_) {
                case 0x00: // BLTZ
                    if (_i32(_rRs_) < 0)
                        return _BranchTarget_;
                    break;
                case 0x01: // BGEZ
                    if (_i32(_rRs_) >= 0)
                        return _BranchTarget_;
                    break;
                case 0x08: // BLTZAL
                    if (_i32(_rRs_) < 0) {
                        _SetLink(31);
                        return _BranchTarget_;
                    }
                    break;
                case 0x09: // BGEZAL
                    if (_i32(_rRs_) >= 0) {
                        _SetLink(31);
                        return _BranchTarget_;
                    }
                    break;
            }
            break;
        case 0x02: // J
            return _JumpTarget_;
        case 0x03: // JAL
            _SetLink(31);
            return _JumpTarget_;
        case 0x04: // BEQ
            if (_i32(_rRs_) == _i32(_rRt_))
                return _BranchTarget_;
            break;
        case 0x05: // BNE
            if (_i32(_rRs_) != _i32(_rRt_))
                return _BranchTarget_;
            break;
        case 0x06: // BLEZ
            if (_i32(_rRs_) <= 0)
                return _BranchTarget_;
            break;
        case 0x07: // BGTZ
            if (_i32(_rRs_) > 0)
                return _BranchTarget_;
            break;
    }

    return (u32)-1;
}

static int psxDelayBranchExec(u32 tar) {
    execI();

    branch = 0;
    psxRegs.pc = tar;
    psxRegs.cycle += BIAS;
    psxBranchTest();
    return 1;
}

static int psxDelayBranchTest(u32 tar1) {
    u32 tar2, tmp1, tmp2;

    tar2 = psxBranchNoDelay();
    if (tar2 == (u32)-1)
        return 0;

    debugI();

    /*
     * Branch in delay slot:
     * - execute 1 instruction at tar1
     * - jump to tar2 (target of branch in delay slot; this branch
     *   has no normal delay slot, instruction at tar1 was fetched instead)
     */
    psxRegs.pc = tar1;
    tmp1 = psxBranchNoDelay();
    if (tmp1 == (u32)-1) {
        return psxDelayBranchExec(tar2);
    }
    debugI();
    psxRegs.cycle += BIAS;

    /*
     * Got a branch at tar1:
     * - execute 1 instruction at tar2
     * - jump to target of that branch (tmp1)
     */
    psxRegs.pc = tar2;
    tmp2 = psxBranchNoDelay();
    if (tmp2 == (u32)-1) {
        return psxDelayBranchExec(tmp1);
    }
    debugI();
    psxRegs.cycle += BIAS;

    /*
     * Got a branch at tar2:
     * - execute 1 instruction at tmp1
     * - jump to target of that branch (tmp2)
     */
    psxRegs.pc = tmp1;
    return psxDelayBranchExec(tmp2);
}

#endif

static void doBranch(u32 tar) {
    u32 tmp;

    branch2 = branch = 1;
    branchPC = tar;

//#ifdef TEST_LOAD_DELAY
#if 1
    // notaz: check for branch in delay slot
    if (psxDelayBranchTest(tar))
        return;
#endif
    // branch delay slot
	psxRegs.code = __loadwordbytereverse((void*)PSXM(psxRegs.pc));

    debugI();

    psxRegs.pc += 4;
    psxRegs.cycle += BIAS;

    // check for load delay
    tmp = _Op_;
    switch (tmp) {
        case 0x10: // COP0
            switch (_Rs_) {
                case 0x00: // MFC0
                case 0x02: // CFC0
                    psxDelayTest(_Rt_, branchPC);
                    return;
            }
            break;
        case 0x12: // COP2
            switch (_Funct_) {
                case 0x00:
                    switch (_Rs_) {
                        case 0x00: // MFC2
                        case 0x02: // CFC2
                            psxDelayTest(_Rt_, branchPC);
                            return;
                    }
                    break;
            }
            break;
        case 0x32: // LWC2
            psxDelayTest(_Rt_, branchPC);
            return;
        default:
            if (tmp >= 0x20 && tmp <= 0x26) { // LB/LH/LWL/LW/LBU/LHU/LWR
                psxDelayTest(_Rt_, branchPC);
                return;
            }
            break;
    }

    branchBSC(_Op_);

    branch = 0;
    psxRegs.pc = branchPC;

    psxBranchTest();
}

/*********************************************************
* Arithmetic with immediate operand                      *
* Format:  OP rt, rs, immediate                          *
*********************************************************/
void psxADDI()     { if (!_Rt_) return; _rRt_ = _u32(_rRs_) + _Imm_ ; }        // Rt = Rs + Im     (Exception on Integer Overflow)
void psxADDIU() { if (!_Rt_) return; _rRt_ = _u32(_rRs_) + _Imm_ ; }        // Rt = Rs + Im
void psxANDI()     { if (!_Rt_) return; _rRt_ = _u32(_rRs_) & _ImmU_; }        // Rt = Rs And Im
void psxORI()     { if (!_Rt_) return; _rRt_ = _u32(_rRs_) | _ImmU_; }        // Rt = Rs Or  Im
void psxXORI()     { if (!_Rt_) return; _rRt_ = _u32(_rRs_) ^ _ImmU_; }        // Rt = Rs Xor Im
void psxSLTI()     { if (!_Rt_) return; _rRt_ = _i32(_rRs_) < _Imm_ ; }        // Rt = Rs < Im        (Signed)
void psxSLTIU() { if (!_Rt_) return; _rRt_ = _u32(_rRs_) < ((u32)_Imm_); }        // Rt = Rs < Im        (Unsigned)

/*********************************************************
* Register arithmetic                                    *
* Format:  OP rd, rs, rt                                 *
*********************************************************/
void psxADD()    { if (!_Rd_) return; _rRd_ = _u32(_rRs_) + _u32(_rRt_); }    // Rd = Rs + Rt        (Exception on Integer Overflow)
void psxADDU()     { if (!_Rd_) return; _rRd_ = _u32(_rRs_) + _u32(_rRt_); }    // Rd = Rs + Rt
void psxSUB()     { if (!_Rd_) return; _rRd_ = _u32(_rRs_) - _u32(_rRt_); }    // Rd = Rs - Rt        (Exception on Integer Overflow)
void psxSUBU()     { if (!_Rd_) return; _rRd_ = _u32(_rRs_) - _u32(_rRt_); }    // Rd = Rs - Rt
void psxAND()     { if (!_Rd_) return; _rRd_ = _u32(_rRs_) & _u32(_rRt_); }    // Rd = Rs And Rt
void psxOR()     { if (!_Rd_) return; _rRd_ = _u32(_rRs_) | _u32(_rRt_); }    // Rd = Rs Or  Rt
void psxXOR()     { if (!_Rd_) return; _rRd_ = _u32(_rRs_) ^ _u32(_rRt_); }    // Rd = Rs Xor Rt
void psxNOR()     { if (!_Rd_) return; _rRd_ =~(_u32(_rRs_) | _u32(_rRt_)); }// Rd = Rs Nor Rt
void psxSLT()     { if (!_Rd_) return; _rRd_ = _i32(_rRs_) < _i32(_rRt_); }    // Rd = Rs < Rt        (Signed)
void psxSLTU()     { if (!_Rd_) return; _rRd_ = _u32(_rRs_) < _u32(_rRt_); }    // Rd = Rs < Rt        (Unsigned)

/*********************************************************
* Register mult/div & Register trap logic                *
* Format:  OP rs, rt                                     *
*********************************************************/
void psxDIV() {
	const s32 Rt = _i32(_rRt_);
    const s32 Rs = _i32(_rRs_);

    if( Rt == 0 )
    {
            _i32(_rHi_) = Rs;
            _i32(_rLo_) = (Rs >= 0) ? -1 : 1;
            return;
    }
    if( Rs == 0x80000000 && Rt == 0xffffffff )
    {
            _i32(_rHi_) = 0;
            _i32(_rLo_) = Rs;
            return;
    }

    _i32(_rHi_) = Rs % Rt;
    _i32(_rLo_) = Rs / Rt;
}

void psxDIVU() {
    if( _rRt_ == 0 )
    {
            _rHi_ = _rRs_;
            _rLo_ = 0xffffffff;
            return;
    }

    _rHi_ = _rRs_ % _rRt_;
    _rLo_ = _rRs_ / _rRt_;
}

void psxMULT() {
    u64 res = (s64)((s64)_i32(_rRs_) * (s64)_i32(_rRt_));

    psxRegs.GPR.n.lo = (u32)(res & 0xffffffff);
    psxRegs.GPR.n.hi = (u32)((res >> 32) & 0xffffffff);
}

void psxMULTU() {
    u64 res = (u64)((u64)_u32(_rRs_) * (u64)_u32(_rRt_));

    psxRegs.GPR.n.lo = (u32)(res & 0xffffffff);
    psxRegs.GPR.n.hi = (u32)((res >> 32) & 0xffffffff);
}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, offset                                 *
*********************************************************/
#define RepZBranchi32(op)      if(_i32(_rRs_) op 0) doBranch(_BranchTarget_);
#define RepZBranchLinki32(op)  if(_i32(_rRs_) op 0) { _SetLink(31); doBranch(_BranchTarget_); }

void psxBGEZ()   { RepZBranchi32(>=) }      // Branch if Rs >= 0
void psxBGEZAL() { RepZBranchLinki32(>=) }  // Branch if Rs >= 0 and link
void psxBGTZ()   { RepZBranchi32(>) }       // Branch if Rs >  0
void psxBLEZ()   { RepZBranchi32(<=) }      // Branch if Rs <= 0
void psxBLTZ()   { RepZBranchi32(<) }       // Branch if Rs <  0
void psxBLTZAL() { RepZBranchLinki32(<) }   // Branch if Rs <  0 and link

/*********************************************************
* Shift arithmetic with constant shift                   *
* Format:  OP rd, rt, sa                                 *
*********************************************************/
void psxSLL() { if (!_Rd_) return; _u32(_rRd_) = _u32(_rRt_) << _Sa_; } // Rd = Rt << sa
void psxSRA() { if (!_Rd_) return; _i32(_rRd_) = _i32(_rRt_) >> _Sa_; } // Rd = Rt >> sa (arithmetic)
void psxSRL() { if (!_Rd_) return; _u32(_rRd_) = _u32(_rRt_) >> _Sa_; } // Rd = Rt >> sa (logical)

/*********************************************************

* Shift arithmetic with variant register shift           *

* Format:  OP rd, rt, rs                                 *

*********************************************************/

__inline u32 Shamt() {
	int shamt = (_u32(_rRs_) & 0x1f);
	if(shamt >= 0 && shamt < 32) return shamt;
	return 0;
}

void psxSLLV() { if (!_Rd_) return; _u32(_rRd_)  =  _u32(_rRt_)  << Shamt(); } // Rd = Rt << rs
void psxSRAV() { if (!_Rd_) return; _i32(_rRd_)  =  _i32(_rRt_)  >> Shamt(); } // Rd = Rt >> rs (arithmetic)
void psxSRLV() { if (!_Rd_) return; _u32(_rRd_)  =  _u32(_rRt_)  >> Shamt(); } // Rd = Rt >> rs (logical)


/*********************************************************
* Load higher 16 bits of the first word in GPR with imm  *
* Format:  OP rt, immediate                              *
*********************************************************/
void psxLUI() { if (!_Rt_) return; _u32(_rRt_) = psxRegs.code << 16; } // Upper halfword of Rt = Im

/*********************************************************
* Move from HI/LO to GPR                                 *
* Format:  OP rd                                         *
*********************************************************/
void psxMFHI() { if (!_Rd_) return; _rRd_ = _rHi_; } // Rd = Hi
void psxMFLO() { if (!_Rd_) return; _rRd_ = _rLo_; } // Rd = Lo

/*********************************************************
* Move to GPR to HI/LO & Register jump                   *
* Format:  OP rs                                         *
*********************************************************/
void psxMTHI() { _rHi_ = _rRs_; } // Hi = Rs
void psxMTLO() { _rLo_ = _rRs_; } // Lo = Rs

/*********************************************************
* Special purpose instructions                           *
* Format:  OP                                            *
*********************************************************/
void psxBREAK() {
    // Break exception - psx rom doens't handles this
}

void psxSYSCALL() {
    psxRegs.pc -= 4;
    psxException(0x20, branch);
}

void psxRFE() {
//    SysPrintf("psxRFE\n");
    psxRegs.CP0.n.Status = (psxRegs.CP0.n.Status & 0xfffffff0) |
                          ((psxRegs.CP0.n.Status & 0x3c) >> 2);
}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, rt, offset                             *
*********************************************************/
#define RepBranchi32(op)      if(_i32(_rRs_) op _i32(_rRt_)) doBranch(_BranchTarget_);

void psxBEQ() {    RepBranchi32(==) }  // Branch if Rs == Rt
void psxBNE() {    RepBranchi32(!=) }  // Branch if Rs != Rt

/*********************************************************
* Jump to target                                         *
* Format:  OP target                                     *
*********************************************************/
void psxJ()   {               doBranch(_JumpTarget_); }
void psxJAL() {    _SetLink(31); doBranch(_JumpTarget_); }

/*********************************************************
* Register jump                                          *
* Format:  OP rs, rd                                     *
*********************************************************/
void psxJR()   {
    doBranch(_u32(_rRs_));
    psxJumpTest();
}

void psxJALR() {
    u32 temp = _u32(_rRs_);
    if (_Rd_) { _SetLink(_Rd_); }
    doBranch(temp);
}

/*********************************************************
* Load and store for GPR                                 *
* Format:  OP rt, offset(base)                           *
*********************************************************/

#define _oB_ (_u32(_rRs_) + _Imm_)
inline void psxLB() {
#ifdef TEST_LOAD_DELAY
    // load delay = 1 latency
    if( branch == 0 )
    {
        // simulate: beq r0,r0,lw+4 / lw / (delay slot)
        psxRegs.pc -= 4;
        doBranch( psxRegs.pc + 4 );

        return;
    }
#endif

    if (_Rt_) {
        _i32(_rRt_) = (signed char)psxMemRead8(_oB_); 
    } else {
        psxMemRead8(_oB_); 
    }
}

inline void psxLBU() {
#ifdef TEST_LOAD_DELAY
    // load delay = 1 latency
    if( branch == 0 )
    {
        // simulate: beq r0,r0,lw+4 / lw / (delay slot)
        psxRegs.pc -= 4;
        doBranch( psxRegs.pc + 4 );

        return;
    }
#endif

    if (_Rt_) {
        _u32(_rRt_) = psxMemRead8(_oB_);
    } else {
        psxMemRead8(_oB_); 
    }
}

inline void psxLH() {
#ifdef TEST_LOAD_DELAY
    // load delay = 1 latency
    if( branch == 0 )
    {
        // simulate: beq r0,r0,lw+4 / lw / (delay slot)
        psxRegs.pc -= 4;
        doBranch( psxRegs.pc + 4 );

        return;
    }
#endif

    if (_Rt_) {
        _i32(_rRt_) = (short)psxMemRead16(_oB_);
    } else {
        psxMemRead16(_oB_);
    }
}

inline void psxLHU() {
#ifdef TEST_LOAD_DELAY
    // load delay = 1 latency
    if( branch == 0 )
    {
        // simulate: beq r0,r0,lw+4 / lw / (delay slot)
        psxRegs.pc -= 4;
        doBranch( psxRegs.pc + 4 );

        return;
    }
#endif

    if (_Rt_) {
        _u32(_rRt_) = psxMemRead16(_oB_);
    } else {
        psxMemRead16(_oB_);
    }
}

inline void psxLW() {
#ifdef TEST_LOAD_DELAY
    // load delay = 1 latency
    if( branch == 0 )
    {
        // simulate: beq r0,r0,lw+4 / lw / (delay slot)
        psxRegs.pc -= 4;
        doBranch( psxRegs.pc + 4 );

        return;
    }
#endif

    if (_Rt_) {
        _u32(_rRt_) = psxMemRead32(_oB_);
    } else {
        psxMemRead32(_oB_);
    }
}

inline void psxLWL() {
	const u32 addr = _oB_;
	const u32 shift = (addr & 3) << 3;
	const u32 mem = psxMemRead32( addr & 0xfffffffc );

#ifdef TEST_LOAD_DELAY
    // load delay = 1 latency
    if( branch == 0 )
    {
        // simulate: beq r0,r0,lw+4 / lw / (delay slot)
        psxRegs.pc -= 4;
        doBranch( psxRegs.pc + 4 );

        return;
    }
#endif

    if (!_Rt_) return;

	_u32(_rRt_) = (_u32(_rRt_) & (u32)(0x00ffffff >> shift)) | (mem << (24 - shift));
}

inline void psxLWR() {
	const u32 addr = _oB_;
	const u32 shift = (addr & 3) << 3;
	const u32 mem = psxMemRead32( addr & 0xfffffffc );


#ifdef TEST_LOAD_DELAY    
    // load delay = 1 latency
    if( branch == 0 )
    {
        // simulate: beq r0,r0,lw+4 / lw / (delay slot)
        psxRegs.pc -= 4;
        doBranch( psxRegs.pc + 4 );

        return;
    }
#endif

    if (!_Rt_) return;

	// new func
	 _u32(_rRt_) = (_u32(_rRt_) & (u32)(0xffffff00 << (24 - shift))) | (mem >> shift);

}

void psxSB() { psxMemWrite8 (_oB_, _u8 (_rRt_)); }
void psxSH() { psxMemWrite16(_oB_, _u16(_rRt_)); }
void psxSW() { psxMemWrite32(_oB_, _u32(_rRt_)); }

inline void psxSWL() {
	const u32 addr = _oB_;
    const u32 shift = (addr & 3) << 3;
    const u32 mem = psxMemRead32( addr & 0xfffffffc );

	// new func
	psxMemWrite32(addr & ~3,  (_u32(_rRt_) >> (24 - shift)) |
                 (  mem & (u32)(0xffffff00 << shift ) ) );

}

inline void psxSWR() {
	const u32 addr = _oB_;
	const u32 shift = (addr & 3) << 3;
	const u32 mem = psxMemRead32( addr & 0xfffffffc );

	psxMemWrite32(addr & ~3,  (_u32(_rRt_) << shift) |
                 (  mem & (u32)(0x00ffffff >> (24 -shift)) ) );
}

/*********************************************************
* Moves between GPR and COPx                             *
* Format:  OP rt, fs                                     *
*********************************************************/
void psxMFC0()
{
#ifdef TEST_LOAD_DELAY
    // load delay = 1 latency
    if( branch == 0 )
    {
        // simulate: beq r0,r0,lw+4 / lw / (delay slot)
        psxRegs.pc -= 4;
        doBranch( psxRegs.pc + 4 );

        return;
    }
#endif

    if (!_Rt_) return;
    
    _i32(_rRt_) = (int)_rFs_;
}

void psxCFC0()
{
#ifdef TEST_LOAD_DELAY
    // load delay = 1 latency
    if( branch == 0 )
    {
        // simulate: beq r0,r0,lw+4 / lw / (delay slot)
        psxRegs.pc -= 4;
        doBranch( psxRegs.pc + 4 );

        return;
    }
#endif

    if (!_Rt_) return;
    
    _i32(_rRt_) = (int)_rFs_;
}

void psxTestSWInts() {
	if (psxRegs.CP0.n.Cause & psxRegs.CP0.n.Status & 0x0300 &&
	   psxRegs.CP0.n.Status & 0x1) {
		psxRegs.CP0.n.Cause &= ~0x7c;
		psxException(psxRegs.CP0.n.Cause, branch);
	}
}

void MTC0(int reg, u32 val) {
//	SysPrintf("MTC0 %d: %x\n", reg, val);
	switch (reg) {
		case 12: // Status
			psxRegs.CP0.r[12] = val;
			psxTestSWInts();
			break;

		case 13: // Cause
			psxRegs.CP0.n.Cause &= ~0x0300;
			psxRegs.CP0.n.Cause |= val & 0x0300;
			psxTestSWInts();
			break;

		default:
			psxRegs.CP0.r[reg] = val;
			break;
	}
}

void psxMTC0() { MTC0(_Rd_, _u32(_rRt_)); }
void psxCTC0() { MTC0(_Rd_, _u32(_rRt_)); }

/*********************************************************
* Unknow instruction (would generate an exception)       *
* Format:  ?                                             *
*********************************************************/
void psxNULL() { 
#ifdef PSXCPU_LOG
    PSXCPU_LOG("psx: Unimplemented op %x\n", psxRegs.code);
#endif
}

void psxSPECIAL() {
    psxSPC[_Funct_]();
}

void psxREGIMM() {
   // psxREG[_Rt_]();
	branchREG(_Rt_);
}

void psxCOP0() {
    psxCP0[_Rs_]();
}

void psxCOP2() {
    if ((psxRegs.CP0.n.Status & 0x40000000) == 0 )
        return;

    psxCP2[_Funct_]();
}

void psxBASIC() {
    psxCP2BSC[_Rs_]();
}

void psxHLE() {
//    psxHLEt[psxRegs.code & 0xffff]();
    psxHLEt[psxRegs.code & 0x07]();        // HDHOSHY experimental patch
}

void (*psxBSC[64])() = {
    psxSPECIAL, psxREGIMM, psxJ   , psxJAL  , psxBEQ , psxBNE , psxBLEZ, psxBGTZ,
    psxADDI   , psxADDIU , psxSLTI, psxSLTIU, psxANDI, psxORI , psxXORI, psxLUI ,
    psxCOP0   , psxNULL  , psxCOP2, psxNULL , psxNULL, psxNULL, psxNULL, psxNULL,
    psxNULL   , psxNULL  , psxNULL, psxNULL , psxNULL, psxNULL, psxNULL, psxNULL,
    psxLB     , psxLH    , psxLWL , psxLW   , psxLBU , psxLHU , psxLWR , psxNULL,
    psxSB     , psxSH    , psxSWL , psxSW   , psxNULL, psxNULL, psxSWR , psxNULL, 
    psxNULL   , psxNULL  , gteLWC2, psxNULL , psxNULL, psxNULL, psxNULL, psxNULL,
    psxNULL   , psxNULL  , gteSWC2, psxHLE  , psxNULL, psxNULL, psxNULL, psxNULL 
};


void (*psxSPC[64])() = {
    psxSLL , psxNULL , psxSRL , psxSRA , psxSLLV   , psxNULL , psxSRLV, psxSRAV,
    psxJR  , psxJALR , psxNULL, psxNULL, psxSYSCALL, psxBREAK, psxNULL, psxNULL,
    psxMFHI, psxMTHI , psxMFLO, psxMTLO, psxNULL   , psxNULL , psxNULL, psxNULL,
    psxMULT, psxMULTU, psxDIV , psxDIVU, psxNULL   , psxNULL , psxNULL, psxNULL,
    psxADD , psxADDU , psxSUB , psxSUBU, psxAND    , psxOR   , psxXOR , psxNOR ,
    psxNULL, psxNULL , psxSLT , psxSLTU, psxNULL   , psxNULL , psxNULL, psxNULL,
    psxNULL, psxNULL , psxNULL, psxNULL, psxNULL   , psxNULL , psxNULL, psxNULL,
    psxNULL, psxNULL , psxNULL, psxNULL, psxNULL   , psxNULL , psxNULL, psxNULL
};

void (*psxREG[32])() = {
    psxBLTZ  , psxBGEZ  , psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
    psxNULL  , psxNULL  , psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
    psxBLTZAL, psxBGEZAL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
    psxNULL  , psxNULL  , psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL
};

void (*psxCP0[32])() = {
    psxMFC0, psxNULL, psxCFC0, psxNULL, psxMTC0, psxNULL, psxCTC0, psxNULL,
    psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
    psxRFE , psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
    psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL
};

void (*psxCP2[64])() = {
    psxBASIC, gteRTPS , psxNULL , psxNULL, psxNULL, psxNULL , gteNCLIP, psxNULL, // 00
    psxNULL , psxNULL , psxNULL , psxNULL, gteOP  , psxNULL , psxNULL , psxNULL, // 08
    gteDPCS , gteINTPL, gteMVMVA, gteNCDS, gteCDP , psxNULL , gteNCDT , psxNULL, // 10
    psxNULL , psxNULL , psxNULL , gteNCCS, gteCC  , psxNULL , gteNCS  , psxNULL, // 18
    gteNCT  , psxNULL , psxNULL , psxNULL, psxNULL, psxNULL , psxNULL , psxNULL, // 20
    gteSQR  , gteDCPL , gteDPCT , psxNULL, psxNULL, gteAVSZ3, gteAVSZ4, psxNULL, // 28 
    gteRTPT , psxNULL , psxNULL , psxNULL, psxNULL, psxNULL , psxNULL , psxNULL, // 30
    psxNULL , psxNULL , psxNULL , psxNULL, psxNULL, gteGPF  , gteGPL  , gteNCCT  // 38
};

void (*psxCP2BSC[32])() = {
    gteMFC2, psxNULL, gteCFC2, psxNULL, gteMTC2, psxNULL, gteCTC2, psxNULL,
    psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
    psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
    psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL
};


///////////////////////////////////////////

static int intInit() {
    return 0;
}

static void intReset() {
    psxRegs.ICache_valid = FALSE;
}

static void intExecute() {
    do {
        execI();
	} while(1);
}

static void intExecuteBlock() {
    branch2 = 0;
    while (!branch2) execI();
}

static void intClear(u32 Addr, u32 Size) {
}

static void intShutdown() {
}

// interpreter execution
static inline void execI() {
	psxRegs.code = __loadwordbytereverse((void*)PSXM(psxRegs.pc));

    debugI();

    if (Config.Debug) ProcessDebug();

    psxRegs.pc += 4;
    psxRegs.cycle += BIAS;

    branchBSC(_Op_);
}

R3000Acpu psxInt = {
    intInit,
    intReset,
    intExecute,
    intExecuteBlock,
    intClear,
    intShutdown
};

static inline void branchREG(u32 rt){
#if 0
	psxREG[rt]();
#else
	/** 
	psxBLTZ  , psxBGEZ  , psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
    psxNULL  , psxNULL  , psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
    psxBLTZAL, psxBGEZAL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
    psxNULL  , psxNULL  , psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL
	**/
	switch(rt) {
	case 0:
		psxBLTZ();
		return;
	case 1:
		psxBGEZ();
		return;
	case 16:
		psxBLTZAL();
		return;
	case 17:
		psxBGEZAL();
		return;
	}
#endif
}

static inline void branchBSC(char opcode){
#if 1
	 psxBSC[opcode]();
#else
	/** 
	0			1			2		3			4		5			6		7
	psxSPECIAL, psxREGIMM, psxJ   , psxJAL  , psxBEQ , psxBNE , psxBLEZ, psxBGTZ,
	8
    psxADDI   , psxADDIU , psxSLTI, psxSLTIU, psxANDI, psxORI , psxXORI, psxLUI ,
	16
    psxCOP0   , psxNULL  , psxCOP2, psxNULL , psxNULL, psxNULL, psxNULL, psxNULL,
	24
    psxNULL   , psxNULL  , psxNULL, psxNULL , psxNULL, psxNULL, psxNULL, psxNULL,
	32
    psxLB     , psxLH    , psxLWL , psxLW   , psxLBU , psxLHU , psxLWR , psxNULL,
	40
    psxSB     , psxSH    , psxSWL , psxSW   , psxNULL, psxNULL, psxSWR , psxNULL, 
	48
    psxNULL   , psxNULL  , gteLWC2, psxNULL , psxNULL, psxNULL, psxNULL, psxNULL,
	56
    psxNULL   , psxNULL  , gteSWC2, psxHLE  , psxNULL, psxNULL, psxNULL, psxNULL 
	64
	**/

	switch(opcode & 0xF0) {
	case 0:
		switch( opcode ) {
			case 0:
				psxSPECIAL();
				return;
			case 1:
				psxREGIMM();
				return;
			case 2:
				psxJ();
				return;
			case 3:
				psxJAL();
				return;
			case 4:
				psxBEQ();
				return;
			case 5:
				psxBNE();
				return;
			case 6:
				psxBLEZ();
				return;
			case 7:
				psxBGTZ();
				return;
			case 8:
				psxADDI();
				return;
			case 9:
				psxADDIU();
				return;
			case 0xA:
				psxSLTI();
				return;
			case 0xB:
				psxSLTIU();
				return;
			case 0xC:
				psxANDI();
				return;
			case 0xD:
				psxORI();
				return;
			case 0xE:
				psxXORI();
				return;
			case 0xF:
				psxLUI();
				return;
		}
		//
	case 0x10:
		switch( opcode ) {
			case 0x10:
				psxCOP0();
				return;
			case 0x12:
				psxCOP2();
				return;
		}
		//
	case 0x20:
		switch( opcode ) {
			case 0x20:
				psxLB();
				return;
			case 0x21:
				psxLH();
				return;		
			case 0x22:
				psxLWL();
				return;		
			case 0x23:
				psxLW();
				return;		
			case 0x24:
				psxLBU();
				return;	
			case 0x25:
				psxLHU();
				return;	
			case 0x26:
				psxLWR();
				return;
			case 0x28:
				psxSB();
				return;
			case 0x29:
				psxSH();
				return;
			case 0x2A:
				psxSWL();
				return;
			case 0x2B:
				psxSW();
				return;
			case 0x2E:
				psxSWR();
				return;
		}
		//
	case 0x30:
		switch( opcode ) {
			case 0x32:
				gteLWC2();
				return;
			case 0x3A:
				gteSWC2();
				return;
			case 0x3B:
				psxHLE();
				return;
		}
	}
#endif
}
