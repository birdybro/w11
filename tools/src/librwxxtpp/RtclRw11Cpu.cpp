// $Id: RtclRw11Cpu.cpp 1280 2022-08-15 09:12:03Z mueller $
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2013-2022 by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
// 
// Revision History: 
// Date         Rev Version  Comment
// 2022-08-11  1276   1.2.35 ssr->mmr rename
// 2022-07-07  1249   1.2.34 BUGFIX: quit before mem write if asm-11 error seen
// 2019-06-29  1175   1.2.33 M_ldabs(): add missing OptValid() call
// 2019-06-07  1160   1.2.32 use RtclStats::Exec()
// 2019-04-30  1143   1.2.31 add HasM9312() getter
// 2019-04-12  1131   1.2.30 BUGFIX: M_wtcpu(): check cpu attn in no-server case
//                           add MemSize() getter; loadabs: add -trace and start
// 2019-03-10  1121   1.2.28 M_cp(): tranfer BlockDone values after rblk
// 2019-03-09  1120   1.2.27 add -brf,-bwf; add range checks for -wa
// 2019-02-23  1114   1.2.26 use std::bind instead of lambda
// 2019-02-15  1112   1.2.25 add HasIbtst() getter
// 2018-12-23  1091   1.2.24 use AddWbibr(move),AddWblk(move)
// 2018-12-18  1089   1.2.23 use c++ style casts
// 2018-12-17  1085   1.2.22 use std::lock_guard instead of boost
// 2018-12-15  1082   1.2.21 use lambda instead of boost::bind
// 2018-12-07  1077   1.2.20 use SetLastExpectBlock move semantics
// 2018-11-16  1070   1.2.19 use auto; use emplace_back; use range loop
// 2018-09-23  1050   1.2.18 add HasPcnt()
// 2018-09-21  1048   1.2.18 coverity fixup (uninitialized scalar)
// 2017-04-22   883   1.2.17 M_(imap|rmap): -testname optional addr check
// 2017-04-15   876   1.2.16 add ControllerCommands()
// 2017-04-15   875   1.2.15 M_default: add attached units summary
// 2017-04-07   868   1.2.14 M_dump: use GetArgsDump and Dump detail
// 2017-02-26   857   1.2.13 use kCPAH_M_UBM22 for cp -wa -ubm
// 2017-02-19   853   1.2.12 use Rtime
// 2017-02-17   851   1.2.11 probe/setup auxilliary devices: kw11l,kw11p,iist
// 2017-02-04   848   1.2.10 M_default: add 'probe ena on' output
// 2016-12-30   834   1.2.9  use 'ssr' instead of 'mmr' for MMU registers
// 2015-12-26   718   1.2.8  use BlockSizeMax() for 'cp -b[rw]m' and 'ldasm' 
// 2015-07-12   700   1.2.4  use ..CpuAct instead ..CpuGo (new active based lam);
//                           add probe and map setup for optional cpu components
// 2015-06-27   695   1.2.3  M_get: add ibase getter
// 2015-05-04   674   1.2.2  w11a start/stop/suspend overhaul
// 2015-04-25   668   1.2.1  M_cp: add -rbibr, wbibr; GetRAddr: drop odd check
// 2015-04-03   661   1.2    expect logic: drop estatdef, use LastExpect..
// 2015-03-28   660   1.1.4  M_cp: add -estat(err|nak|tout)
// 2015-03-21   659   1.1.3  rename M_amap->M_imap; add M_rmap; add GetRAddr()
//                           add -rreg,...,-init and -[rw]ma
// 2014-12-29   623   1.1.2  add M_amap; M_cp: add -print and -dump
// 2014-12-20   616   1.1.1  M_cp: add -edone for BlockDone checking
// 2014-11-30   607   1.1    new rlink v4 iface
// 2014-08-22   584   1.0.5  use nullptr
// 2014-08-02   576   1.0.4  BUGFIX: redo estatdef logic; avoid LastExpect()
// 2014-03-02   552   1.0.3  M_cp: add -ral and -rah options (addr reg readback)
// 2013-05-19   521   1.0.2  M_cp: merge -wibrb|-wibrbbe again; add -wa
// 2013-04-26   511   1.0.1  add M_show
// 2013-04-02   502   1.0    Initial version
// 2013-02-02   480   0.1    First draft
// ---------------------------------------------------------------------------

/*!
  \brief   Implemenation of RtclRw11Cpu.
*/

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

#include <vector>
#include <memory>
#include <sstream>
#include <functional>

#include "librtools/RerrMsg.hpp"
#include "librtools/RlogMsg.hpp"
#include "librtools/RosPrintf.hpp"
#include "librtools/RosPrintBvi.hpp"
#include "librtcltools/Rtcl.hpp"
#include "librtcltools/RtclStats.hpp"
#include "librtcltools/RtclOPtr.hpp"
#include "librtcltools/RtclNameSet.hpp"
#include "librlink/RlinkCommandList.hpp"

#include "librw11/Rw11Unit.hpp"

#include "RtclRw11.hpp"

#include "RtclRw11CntlFactory.hpp"
#include "librw11/Rw11Cntl.hpp"

#include "RtclRw11Cpu.hpp"

using namespace std;
using namespace std::placeholders;

/*!
  \class Retro::RtclRw11Cpu
  \brief FIXME_docs
*/

// all method definitions in namespace Retro
namespace Retro {

//------------------------------------------+-----------------------------------
//! Default constructor

RtclRw11Cpu::RtclRw11Cpu(const std::string& type)
  : RtclProxyBase(type),
    fGets(),
    fSets()
{
  AddMeth("add",      bind(&RtclRw11Cpu::M_add,     this, _1));
  AddMeth("imap",     bind(&RtclRw11Cpu::M_imap,    this, _1));
  AddMeth("rmap",     bind(&RtclRw11Cpu::M_rmap,    this, _1));
  AddMeth("cp",       bind(&RtclRw11Cpu::M_cp,      this, _1));
  AddMeth("wtcpu",    bind(&RtclRw11Cpu::M_wtcpu,   this, _1));
  AddMeth("deposit",  bind(&RtclRw11Cpu::M_deposit, this, _1));
  AddMeth("examine",  bind(&RtclRw11Cpu::M_examine, this, _1));
  AddMeth("lsmem",    bind(&RtclRw11Cpu::M_lsmem,   this, _1));
  AddMeth("ldabs",    bind(&RtclRw11Cpu::M_ldabs,   this, _1));
  AddMeth("ldasm",    bind(&RtclRw11Cpu::M_ldasm,   this, _1));
  AddMeth("boot",     bind(&RtclRw11Cpu::M_boot,    this, _1));
  AddMeth("get",      bind(&RtclRw11Cpu::M_get,     this, _1));
  AddMeth("set",      bind(&RtclRw11Cpu::M_set,     this, _1));
  AddMeth("stats",    bind(&RtclRw11Cpu::M_stats,   this, _1));
  AddMeth("show",     bind(&RtclRw11Cpu::M_show,    this, _1));
  AddMeth("dump",     bind(&RtclRw11Cpu::M_dump,    this, _1));
  AddMeth("$default", bind(&RtclRw11Cpu::M_default, this, _1));
  
  fGets.Add<Tcl_Obj*> ("cntls", bind(&RtclRw11Cpu::ControllerCommands, this));
}

//------------------------------------------+-----------------------------------
//! Destructor

RtclRw11Cpu::~RtclRw11Cpu()
{}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int RtclRw11Cpu::M_add(RtclArgs& args)
{
  return RtclRw11CntlFactory(args, *this);
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int RtclRw11Cpu::M_imap(RtclArgs& args)
{
  static RtclNameSet optset("-name|-testname|-testaddr|-insert|-erase|-print");

  const RlinkAddrMap& addrmap = Obj().IAddrMap();

  string opt;
  string name;
  uint16_t addr=0;

  if (args.NextOpt(opt, optset)) {
    if        (opt == "-name") {            // imap -name addr
      if (!args.GetArg("addr", addr)) return kERR;
      if (!args.AllDone()) return kERR;
      string   tstname;
      if(addrmap.Find(addr, tstname)) {
        args.SetResult(tstname);
      } else {
        return args.Quit(string("-E: address '") + args.PeekArgString(-1) +
                         "' not mapped");
      }

    } else if (opt == "-testname") {        // imap -testname name ?addr
      if (!args.GetArg("name", name)) return kERR;
      if (!args.GetArg("?addr", addr)) return kERR;
      if (!args.AllDone()) return kERR;
      uint16_t tstaddr=0;
      bool found = addrmap.Find(name, tstaddr);
      if (found && args.NOptMiss()==0) {      // if specified addr
        if (tstaddr != addr) found = false;   // verify addr
      }
      args.SetResult(int(found));

    } else if (opt == "-testaddr") {        // imap -testaddr addr
      if (!args.GetArg("addr", addr)) return kERR;
      if (!args.AllDone()) return kERR;
      string   tstname;
      args.SetResult(int(addrmap.Find(addr, tstname)));

    } else if (opt == "-insert") {          // imap -insert name addr
      uint16_t tstaddr=0;
      string   tstname;
      int      tstint=0;
      if (!args.GetArg("name", name)) return kERR;
      // enforce that the name is not a valid representation of an int
      if (Tcl_GetIntFromObj(nullptr, args[args.NDone()-1], &tstint) == kOK) 
        return args.Quit(string("-E: name should not look like an int but '")+
                         name + "' does");
      if (!args.GetArg("addr", addr)) return kERR;
      if (!args.AllDone()) return kERR;
      if (addrmap.Find(name, tstaddr)) 
        return args.Quit(string("-E: mapping already defined for '")+name+"'");
      if (addrmap.Find(addr, tstname)) 
        return args.Quit(string("-E: mapping already defined for address '") +
                         args.PeekArgString(-1) + "'");
      Obj().IAddrMapInsert(name, addr);

    } else if (opt == "-erase") {           // imap -erase name
      if (!args.GetArg("name", name)) return kERR;
      if (!args.AllDone()) return kERR;
      if (!Obj().IAddrMapErase(name)) 
        return args.Quit(string("-E: no mapping defined for '") + name + "'");

    } else if (opt == "-print") {           // imap -print
      if (!args.AllDone()) return kERR;
      ostringstream sos;
      addrmap.Print(sos);
      args.AppendResultLines(sos);
    }
    
  } else {
    if (!args.OptValid()) return kERR;
    if (!args.GetArg("?name", name)) return kERR;
    if (args.NOptMiss()==0) {               // imap name
      uint16_t tstaddr=0;
      if(addrmap.Find(name, tstaddr)) {
        args.SetResult(int(tstaddr));
      } else {
        return args.Quit(string("-E: no mapping defined for '") + name + "'");
      }

    } else {                                // imap
      RtclOPtr plist(Tcl_NewListObj(0, nullptr));
      const auto amap = addrmap.Amap();
      for (auto& o: amap) {
        Tcl_Obj* tpair[2];
        tpair[0] = Tcl_NewIntObj(o.first);
        tpair[1] = Tcl_NewStringObj(o.second.c_str(),o.second.length());
        Tcl_ListObjAppendElement(nullptr, plist, Tcl_NewListObj(2, tpair));
      }
      args.SetResult(plist);
    }
  }
  
  return kOK;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int RtclRw11Cpu::M_rmap(RtclArgs& args)
{
  static RtclNameSet optset("-name|-testname|-testaddr|-insert|-erase|-print");

  const RlinkAddrMap& lmap = Obj().RAddrMap();    //  local map
  const RlinkAddrMap& cmap = Connect().AddrMap(); // common map

  string opt;
  string name;
  uint16_t addr=0;

  if (args.NextOpt(opt, optset)) {
    if        (opt == "-name") {            // rmap -name addr
      if (!args.GetArg("addr", addr)) return kERR;
      if (!args.AllDone()) return kERR;
      string   tstname;
      if(lmap.Find(addr, tstname)) {
        args.SetResult(tstname);
      } else if(cmap.Find(addr, tstname)) {
        args.SetResult(tstname);
      } else {
        return args.Quit(string("-E: address '") + args.PeekArgString(-1) +
                         "' not mapped");
      }

    } else if (opt == "-testname") {        // rmap -testname name ?addr
      if (!args.GetArg("name", name)) return kERR;
      if (!args.GetArg("?addr", addr)) return kERR;
      if (!args.AllDone()) return kERR;
      uint16_t tstaddr=0;
      bool found = lmap.Find(name, tstaddr);
      if (found && args.NOptMiss()==0) {      // if specified addr
        if (tstaddr != addr) found = false;   // verify addr
      }
      args.SetResult(int(found));

    } else if (opt == "-testaddr") {        // rmap -testaddr addr
      if (!args.GetArg("addr", addr)) return kERR;
      if (!args.AllDone()) return kERR;
      string   tstname;
      args.SetResult(int(lmap.Find(addr, tstname)));

    } else if (opt == "-insert") {          // rmap -insert name addr
      uint16_t tstaddr=0;
      string   tstname;
      int      tstint=0;
      if (!args.GetArg("name", name)) return kERR;
      // enforce that the name is not a valid representation of an int
      if (Tcl_GetIntFromObj(nullptr, args[args.NDone()-1], &tstint) == kOK) 
        return args.Quit(string("-E: name should not look like an int but '")+
                         name + "' does");
      if (!args.GetArg("addr", addr)) return kERR;
      if (!args.AllDone()) return kERR;
      if (lmap.Find(name, tstaddr)) 
        return args.Quit(string("-E: mapping already defined for '")+name+"'");
      if (lmap.Find(addr, tstname)) 
        return args.Quit(string("-E: mapping already defined for address '") +
                         args.PeekArgString(-1) + "'");
      Obj().RAddrMapInsert(name, addr);

    } else if (opt == "-erase") {           // rmap -erase name
      if (!args.GetArg("name", name)) return kERR;
      if (!args.AllDone()) return kERR;
      if (!Obj().RAddrMapErase(name)) 
        return args.Quit(string("-E: no mapping defined for '") + name + "'");

    } else if (opt == "-print") {           // rmap -print
      if (!args.AllDone()) return kERR;
      ostringstream sos;
      lmap.Print(sos);
      args.AppendResultLines(sos);
    }
    
  } else {
    if (!args.OptValid()) return kERR;
    if (!args.GetArg("?name", name)) return kERR;
    if (args.NOptMiss()==0) {               // rmap name
      uint16_t tstaddr=0;
      if(lmap.Find(name, tstaddr)) {
        args.SetResult(int(tstaddr));
      } else if(cmap.Find(name, tstaddr)) {
        args.SetResult(int(tstaddr));
      } else {
        return args.Quit(string("-E: no mapping defined for '") + name + "'");
      }

    } else {                                // rmap
      RtclOPtr plist(Tcl_NewListObj(0, nullptr));
      const auto amap = lmap.Amap();
      for (auto& o: amap) {
        Tcl_Obj* tpair[2];
        tpair[0] = Tcl_NewIntObj(o.first);
        tpair[1] = Tcl_NewStringObj(o.second.c_str(),o.second.length());
        Tcl_ListObjAppendElement(nullptr, plist, Tcl_NewListObj(2, tpair));
      }
      args.SetResult(plist);
    }
  }
  
  return kOK;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int RtclRw11Cpu::M_cp(RtclArgs& args)
{
  static RtclNameSet optset("-rreg|-rblk|-wreg|-wblk|-labo|-attn|-init|"
                            "-rr|-rr0|-rr1|-rr2|-rr3|-rr4|-rr5|-rr6|-rr7|"
                            "-wr|-wr0|-wr1|-wr2|-wr3|-wr4|-wr5|-wr6|-wr7|"
                            "-rsp|-rpc|-wsp|-wpc|"
                            "-rps|-wps|"
                            "-ral|-rah|-wal|-wah|-wa|"
                            "-rm|-rmi|-rma|-wm|-wmi|-wma|-brm|-bwm|-brf|-bwf|"
                            "-start|-stop|-step|-creset|-breset|"
                            "-suspend|-resume|"
                            "-stapc|"
                            "-rmembe|-wmembe|-ribr|-rbibr|-wibr|-wbibr|"
                            "-rconf|-rstat|"
                            "-edata|-edone|-estat|"
                            "-estaterr|-estatnak|-estattout|"
                            "-print|-dump");

  Tcl_Interp* interp = args.Interp();

  RlinkCommandList clist;
  string opt;
  uint16_t base  = Obj().Base();

  vector<string> vardata;
  vector<string> varstat;
  string varprint;
  string vardump;

  bool setcpuact = false;

  while (args.NextOpt(opt, optset)) {
    size_t lsize = clist.Size();
    
    // map register read/write
    if (opt == "-rsp") opt = "-rr6";
    if (opt == "-rpc") opt = "-rr7";
    if (opt == "-wsp") opt = "-wr6";
    if (opt == "-wpc") opt = "-wr7";
    
    int regnum = 0;
    if (opt.substr(0,3) == "-rr" || opt.substr(0,3) == "-wr" ) {
      if (opt.length() == 3) {              // -rr n or -wr n option
        if (!args.GetArg("regnum", regnum, 0, 7)) return kERR;
      } else if (opt.length() == 4 && opt[3] >= '0' && opt[3] <= '7') {
        regnum = opt[3] - '0';
        opt = opt.substr(0,3);
      }
    }    

    if        (opt == "-rreg") {            // -rreg addr ?varData ?varStat ---
      uint16_t addr=0;
      if (!GetRAddr(args, addr)) return kERR;
      if (!GetVarName(args, "??varData", lsize, vardata)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddRreg(addr);

    } else if (opt == "-rblk") {            // -rblk addr size ?varData ?varStat
      uint16_t addr=0;
      int32_t bsize=0;
      if (!GetRAddr(args, addr)) return kERR;
      if (!args.GetArg("bsize", bsize, 1, Connect().BlockSizeMax())) return kERR;
      if (!GetVarName(args, "??varData", lsize, vardata)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddRblk(addr, size_t(bsize));

    } else if (opt == "-wreg") {            // -wreg addr data ?varStat -------
      uint16_t addr=0;
      uint16_t data=0;
      if (!GetRAddr(args, addr)) return kERR;
      if (!args.GetArg("data", data)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddWreg(addr, data);

    } else if (opt == "-wblk") {            // -wblk addr block ?varStat ------
      uint16_t addr=0;
      vector<uint16_t> block;
      if (!GetRAddr(args, addr)) return kERR;
      if (!args.GetArg("data", block, 1, Connect().BlockSizeMax())) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddWblk(addr, move(block));

    } else if (opt == "-labo") {            // -labo varData ?varStat ---------
      if (!GetVarName(args, "??varData", lsize, vardata)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddLabo();

    } else if (opt == "-attn") {            // -attn varData ?varStat ---------
      if (!GetVarName(args, "??varData", lsize, vardata)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddAttn();

    } else if (opt == "-init") {            // -init addr data ?varStat -------
      uint16_t addr=0;
      uint16_t data=0;
      if (!GetRAddr(args, addr)) return kERR;
      if (!args.GetArg("data", data)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddInit(addr, data);

    } else if (opt == "-rr") {              // -rr* ?varData ?varStat --------
      if (!GetVarName(args, "??varData", lsize, vardata)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddRreg(base + Rw11Cpu::kCPR0 + regnum);

    } else if (opt == "-wr") {              // -wr* data ?varStat ------------
      uint16_t data=0;
      if (!args.GetArg("data", data)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddWreg(base + Rw11Cpu::kCPR0 + regnum, data);

    } else if (opt == "-rps") {             // -rps ?varData ?varStat --------
      if (!GetVarName(args, "??varData", lsize, vardata)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddRreg(base + Rw11Cpu::kCPPSW);

    } else if (opt == "-wps") {             // -wps data ?varStat ------------
      uint16_t data=0;
      if (!args.GetArg("data", data)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddWreg(base + Rw11Cpu::kCPPSW, data);

    } else if (opt == "-ral") {             // -ral ?varData ?varStat --------
      if (!GetVarName(args, "??varData", lsize, vardata)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddRreg(base + Rw11Cpu::kCPAL);

    } else if (opt == "-rah") {             // -rah ?varData ?varStat --------
      if (!GetVarName(args, "??varData", lsize, vardata)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddRreg(base + Rw11Cpu::kCPAH);

    } else if (opt == "-wal") {             // -wal data ?varStat ------------
      uint16_t ibaddr=0;
      if (!GetIAddr(args, ibaddr)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddWreg(base + Rw11Cpu::kCPAL, ibaddr);

    } else if (opt == "-wah") {             // -wah data ?varStat ------------
      uint16_t data=0;
      if (!args.GetArg("ah", data)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddWreg(base + Rw11Cpu::kCPAH, data);

    // Note: -wa without -p22 or -ubm options is equivalent to -wal
    } else if (opt == "-wa") {              // -wa addr ?varStat [-p22 -ubm]--
      uint32_t addr=0;
      if (!args.GetArg("addr", addr, 017777776)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      uint16_t al = addr;
      uint16_t ah = (addr>>16);
      static RtclNameSet suboptset("-p22|-ubm");
      string subopt;
      while (args.NextSubOpt(subopt, suboptset)>=0) { // loop for sub-options
        if (!args.OptValid()) return kERR;
        if (subopt == "-p22") {             // -p22 
          ah |= Rw11Cpu::kCPAH_M_22BIT;
        } else if (subopt == "-ubm") {      // -ubm 
          ah |= Rw11Cpu::kCPAH_M_UBM22;
          if (addr > 0777776) {
            ostringstream sos;
            sos << "-E: value '" << addr
                << "' for 'addr' out of range 0...0777776";
            args.AppendResult(sos);
            return kERR;
          }
        }
      }
      if ((ah & (Rw11Cpu::kCPAH_M_22BIT|Rw11Cpu::kCPAH_M_UBM22)) == 0 &&
          addr > 0177776) {
        ostringstream sos;
        sos << "-E: value '" << addr
            << "' for 'addr' out of range 0...0177776";
        args.AppendResult(sos);
        return kERR;
      }
      
      clist.AddWreg(base + Rw11Cpu::kCPAL, al);
      if (ah!=0) clist.AddWreg(base + Rw11Cpu::kCPAH, ah);

    } else if (opt == "-rm" ||              // -rm(i) ?varData ?varStat ------
               opt == "-rmi") {
      uint16_t addr = opt=="-rm" ? Rw11Cpu::kCPMEM : 
                                   Rw11Cpu::kCPMEMI;
      if (!GetVarName(args, "??varData", lsize, vardata)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddRreg(base + addr);

    } else if (opt == "-rma") {             // -rma addr ?varData ?varStat ---
      uint16_t ibaddr=0;
      if (!GetIAddr(args, ibaddr)) return kERR;
      // bind expects to memi access, which is second command
      if (!GetVarName(args, "??varData", lsize+1, vardata)) return kERR;
      if (!GetVarName(args, "??varStat", lsize+1, varstat)) return kERR;
      clist.AddWreg(base + Rw11Cpu::kCPAL, ibaddr);
      clist.AddRreg(base + Rw11Cpu::kCPMEMI);

    } else if (opt == "-wm" ||              // -wm(i) data ?varStat -
               opt == "-wmi") {
      uint16_t addr = opt=="-wm" ? Rw11Cpu::kCPMEM : 
                                   Rw11Cpu::kCPMEMI;
      uint16_t data=0;
      if (!args.GetArg("data", data)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddWreg(base + addr, data);

    } else if (opt == "-wma") {             // -wma addr data ?varStat -------
      uint16_t ibaddr=0;
      uint16_t data=0;
      if (!GetIAddr(args, ibaddr)) return kERR;
      if (!args.GetArg("data", data)) return kERR;
      // bind expects to memi access, which is second command
      if (!GetVarName(args, "??varStat", lsize+1, varstat)) return kERR;
      clist.AddWreg(base + Rw11Cpu::kCPAL, ibaddr);
      clist.AddWreg(base + Rw11Cpu::kCPMEMI, data);

    } else if (opt == "-brm") {             // -brm size ?varData ?varStat ---
      int32_t bsize=0;
      if (!args.GetArg("bsize", bsize, 1, Connect().BlockSizeMax())) return kERR;
      if (!GetVarName(args, "??varData", lsize, vardata)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddRblk(base + Rw11Cpu::kCPMEMI, size_t(bsize));

    } else if (opt == "-bwm") {             // -bwm block ?varStat -----------
      vector<uint16_t> block;
      if (!args.GetArg("data", block, 1, Connect().BlockSizeMax())) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddWblk(base + Rw11Cpu::kCPMEMI, move(block));
    
    } else if (opt == "-brf") {             // -brf size ?varData ?varStat ---
      int32_t bsize=0;
      if (!args.GetArg("bsize", bsize, 1, Connect().BlockSizeMax())) return kERR;
      if (!GetVarName(args, "??varData", lsize, vardata)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddRblk(base + Rw11Cpu::kCPMEM, size_t(bsize));

    } else if (opt == "-bwf") {             // -bwf block ?varStat -----------
      vector<uint16_t> block;
      if (!args.GetArg("data", block, 1, Connect().BlockSizeMax())) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddWblk(base + Rw11Cpu::kCPMEM, move(block));
    
    } else if (opt == "-start") {           // -start ?varStat ---------------
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddWreg(base + Rw11Cpu::kCPCNTL, Rw11Cpu::kCPFUNC_START);
      setcpuact = true;

    } else if (opt == "-stop") {            // -stop ?varStat ----------------
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddWreg(base + Rw11Cpu::kCPCNTL, Rw11Cpu::kCPFUNC_STOP);

    } else if (opt == "-step") {            // -step ?varStat ----------------
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddWreg(base + Rw11Cpu::kCPCNTL, Rw11Cpu::kCPFUNC_STEP);

    } else if (opt == "-creset") {          // -creset ?varStat --------------
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddWreg(base + Rw11Cpu::kCPCNTL, Rw11Cpu::kCPFUNC_CRESET);

    } else if (opt == "-breset") {         // -breset ?varStat --------------
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddWreg(base + Rw11Cpu::kCPCNTL, Rw11Cpu::kCPFUNC_BRESET);

    } else if (opt == "-suspend") {         // -suspend ?varStat -------------
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddWreg(base + Rw11Cpu::kCPCNTL, Rw11Cpu::kCPFUNC_SUSPEND);

    } else if (opt == "-resume") {          // -resume ?varStat --------------
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddWreg(base + Rw11Cpu::kCPCNTL, Rw11Cpu::kCPFUNC_RESUME);
      setcpuact = true;

    } else if (opt == "-stapc") {           // -stapc addr ?varStat ----------
      uint16_t data=0;
      if (!args.GetArg("data", data)) return kERR;
      if (!GetVarName(args, "??varStat", lsize+1, varstat)) return kERR;  
      clist.AddWreg(base + Rw11Cpu::kCPCNTL, Rw11Cpu::kCPFUNC_STOP);
      clist.AddWreg(base + Rw11Cpu::kCPCNTL, Rw11Cpu::kCPFUNC_CRESET);
      clist.AddWreg(base + Rw11Cpu::kCPPC, data);
      clist.AddWreg(base + Rw11Cpu::kCPCNTL, Rw11Cpu::kCPFUNC_START);
      setcpuact = true;

    } else if (opt == "-rmembe") {          // -rmembe ?varData ?varStat ------
      if (!GetVarName(args, "??varData", lsize, vardata)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddRreg(base + Rw11Cpu::kCPMEMBE);

    } else if (opt == "-wmembe") {          // -wmembe be ?varStat [-stick] -
      uint16_t be=0;
      bool     stick = false;
      if (!args.GetArg("be", be, 3)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;

      static RtclNameSet suboptset("-stick");
      string subopt;
      while (args.NextSubOpt(subopt, suboptset)>=0) { // loop for sub-options
        if (!args.OptValid()) return kERR;
        if (subopt == "-stick") {            // -stick
          stick = true;
        }
      }
      Obj().AddMembe(clist, be, stick);

    } else if (opt == "-ribr") {           // -ribr iba ?varData ?varStat ----
      uint16_t ibaddr=0;
      if (!GetIAddr(args, ibaddr)) return kERR;
      if (!GetVarName(args, "??varData", lsize, vardata)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      Obj().AddRibr(clist, ibaddr);

    } else if (opt == "-rbibr") {          // -rbibr iba size ?varData ?varStat
      uint16_t ibaddr=0;
      int32_t bsize=0;
      if (!GetIAddr(args, ibaddr)) return kERR;
      if (!args.GetArg("bsize", bsize, 1, Connect().BlockSizeMax())) return kERR;
      if (!GetVarName(args, "??varData", lsize, vardata)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      Obj().AddRbibr(clist, ibaddr, size_t(bsize));

    } else if (opt == "-wibr") {           // -wibr iba data ?varStat --------
      uint16_t ibaddr=0;
      uint16_t data=0;
      if (!GetIAddr(args, ibaddr)) return kERR;
      if (!args.GetArg("data",   data)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      Obj().AddWibr(clist, ibaddr, data);

    } else if (opt == "-wbibr") {          // -wbibr iba block data ?varStat -
      uint16_t ibaddr=0;
      vector<uint16_t> block;
      if (!GetIAddr(args, ibaddr)) return kERR;
      if (!args.GetArg("data", block, 1, Connect().BlockSizeMax())) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      Obj().AddWbibr(clist, ibaddr, move(block));

    } else if (opt == "-rconf") {           // -rconf ?varData ?varStat ------
      if (!GetVarName(args, "??varData", lsize, vardata)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddRreg(base + Rw11Cpu::kCPCONF);

    } else if (opt == "-rstat") {           // -rstat ?varData ?varStat ------
      if (!GetVarName(args, "??varData", lsize, vardata)) return kERR;
      if (!GetVarName(args, "??varStat", lsize, varstat)) return kERR;
      clist.AddRreg(base + Rw11Cpu::kCPSTAT);

    } else if (opt == "-edata") {           // -edata data ?mask --------------
      if (!ClistNonEmpty(args, clist)) return kERR;
      if (clist[lsize-1].Command() == RlinkCommand::kCmdRblk) {
        vector<uint16_t> data;
        vector<uint16_t> mask;
        size_t bsize = clist[lsize-1].BlockSize();
        if (!args.GetArg("data", data, 0, bsize)) return kERR;
        if (!args.GetArg("??mask", mask, 0, bsize)) return kERR;
        clist.SetLastExpectBlock(move(data), move(mask));
      } else {
        uint16_t data=0;
        uint16_t mask=0xffff;
        if (!args.GetArg("data", data)) return kERR;
        if (!args.GetArg("??mask", mask)) return kERR;
        clist.SetLastExpectData(data, mask);
      }

    } else if (opt == "-edone") {           // -edone done --------------------
      if (!ClistNonEmpty(args, clist)) return kERR;
      uint16_t done=0;
      if (!args.GetArg("done", done)) return kERR;
      uint8_t cmd = clist[lsize-1].Command();
      if (cmd == RlinkCommand::kCmdRblk ||
          cmd == RlinkCommand::kCmdWblk) {
        clist.SetLastExpectDone(done);
      } else {
        return args.Quit("-E: -edone allowed only after -rblk,-wblk");
      }

    } else if (opt == "-estat") {           // -estat stat ?mask --------------
      if (!ClistNonEmpty(args, clist)) return kERR;
      uint8_t stat=0;
      uint8_t mask=0xff;
      if (!args.GetArg("stat", stat)) return kERR;
      if (!args.GetArg("??mask", mask)) return kERR;
      clist.SetLastExpectStatus(stat, mask);

    } else if (opt == "-estaterr"  ||       // -estaterr ----------------------
               opt == "-estatnak"  ||       // -estatnak ----------------------
               opt == "-estattout" ||       // -estattout ---------------------
               opt == "-estatmerr") {       // -estatmerr ---------------------
      if (!ClistNonEmpty(args, clist)) return kERR;
      uint8_t val = 0;
      uint8_t msk = RlinkCommand::kStat_M_RbTout |
                    RlinkCommand::kStat_M_RbNak  |
                    RlinkCommand::kStat_M_RbErr;
      if (opt == "-estaterr")  val = RlinkCommand::kStat_M_RbErr;
      if (opt == "-estatnak")  val = RlinkCommand::kStat_M_RbNak;
      if (opt == "-estattout") val = RlinkCommand::kStat_M_RbTout;
      if (opt == "-estatmerr") {
        val  = Rw11Cpu::kStat_M_CmdMErr;
        msk |= Rw11Cpu::kStat_M_CmdMErr |
               Rw11Cpu::kStat_M_CmdErr;
      }
      clist.SetLastExpectStatus(val, msk);
      
    } else if (opt == "-print") {           // -print ?varRes -----------------
      varprint = "-";
      if (!args.GetArg("??varRes", varprint)) return kERR;
    } else if (opt == "-dump") {            // -dump ?varRes ------------------
      vardump = "-";
      if (!args.GetArg("??varRes", vardump)) return kERR;
    }

  } // while (args.NextOpt(opt, optset))

  int nact = 0;
  if (varprint == "-") nact += 1;
  if (vardump  == "-") nact += 1;
  if (nact > 1) 
    return args.Quit(
      "-E: more that one of -print,-dump without target variable found");

  if (!args.AllDone()) return kERR;
  if (clist.Size() == 0) return kOK;

  // signal cpugo up before clist executed to prevent races
  if (setcpuact) Obj().SetCpuActUp();

  RerrMsg emsg;
  // this one intentionally on Connect() to allow mixing of rlc + w11 commands
  // FIXME_code: is this a good idea ??
  if (!Connect().Exec(clist, emsg)) return args.Quit(emsg);

  // FIXME: this code is a 1-to-1 copy from RtclRlinkConnect ! put into a method
  for (size_t icmd=0; icmd<clist.Size(); icmd++) {
    RlinkCommand& cmd = clist[icmd];
    
    if (icmd<vardata.size() && !vardata[icmd].empty()) {
      RtclOPtr pres;
      vector<uint16_t> retstat;
      RtclOPtr pele;
      switch (cmd.Command()) {
        case RlinkCommand::kCmdRreg:
        case RlinkCommand::kCmdAttn:
        case RlinkCommand::kCmdLabo:
          pres = Tcl_NewIntObj(int(cmd.Data()));
          break;

        case RlinkCommand::kCmdRblk:
          pres = Rtcl::NewListIntObj(cmd.Block().data(), cmd.BlockDone());
          break;
      }
      if(!Rtcl::SetVar(interp, vardata[icmd], pres)) return kERR;
    }

    if (icmd<varstat.size() && !varstat[icmd].empty()) {
      RtclOPtr pres(Tcl_NewIntObj(int(cmd.Status())));
      if (!Rtcl::SetVar(interp, varstat[icmd], pres)) return kERR;
    }
  }

  if (!varprint.empty()) {
    ostringstream sos;
    clist.Print(sos, &Connect().AddrMap(), 
                Connect().LogBaseAddr(), Connect().LogBaseData(), 
                Connect().LogBaseStat());
    RtclOPtr pobj(Rtcl::NewLinesObj(sos));
    if (!Rtcl::SetVarOrResult(args.Interp(), varprint, pobj)) return kERR;
  }

  if (!vardump.empty()) {
    ostringstream sos;
    clist.Dump(sos, 0);
    RtclOPtr pobj(Rtcl::NewLinesObj(sos));
    if (!Rtcl::SetVarOrResult(args.Interp(), vardump, pobj)) return kERR;
  }

  return kOK;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int RtclRw11Cpu::M_wtcpu(RtclArgs& args)
{
  static RtclNameSet optset("-reset");

  string opt;
  bool reset = false;
  double dtout = 0.;

  while (args.NextOpt(opt, optset)) {
    if (opt == "-reset") reset = true;
  }
  if (!args.OptValid()) return kERR;
  if (!args.GetArg("tout", dtout, 0.001)) return kERR;
  if (!args.AllDone()) return kERR;

  Rtime tout(dtout);
  Rtime twait;
  int irc = -1;

  if (!Server().IsActive()) {               // server is not active
    Rtime twait1;
    RerrMsg emsg;
    uint16_t apat;
    uint16_t apatcpu = uint16_t(1)<<Rw11::kLam;
    // FIXME_code: make apat accessible in  tcl
    while (true) {
      irc = Connect().WaitAttn(tout, twait1, apat, emsg);
      twait += twait1;
      tout  -= twait1;
      if (irc == -2) return args.Quit(emsg); // if fail, quit
      if (irc < 0) break;                    // if timeout, quit loop 

      RlinkCommandList clist;               // get and discard attn pattern 
      clist.AddAttn();
      if (!Connect().Exec(clist, emsg)) return args.Quit(emsg);
      apat = clist[0].Data();               // get attn pattern
      if (apat & apatcpu) break;            // cpu seen, quit loop
    }

  } else {                                  // server is active
    irc = Obj().WaitCpuActDown(tout, twait);
  } 

  if (irc < 0) {                            // timeout
    if (Connect().PrintLevel() >= 1) {
      RlogMsg lmsg(Connect().LogFile());
      lmsg << "-- wtcpu to=" << RosPrintf(dtout, "f", 0,3) << " FAIL timeout";
    }
    Connect().Context().IncErrorCount();
    if (reset) {                            // reset requested 
      uint16_t base = Obj().Base();
      RlinkCommandList clist;
      clist.AddWreg(base + Rw11Cpu::kCPCNTL, Rw11Cpu::kCPFUNC_STOP);
      RerrMsg emsg;
      if (!Connect().Exec(clist, emsg)) return args.Quit(emsg);
    }
  } else {                                  // no timeout
    if (Connect().PrintLevel() >= 3) {
      RlogMsg lmsg(Connect().LogFile());
      lmsg << "-- wtcpu to=" << RosPrintf(dtout, "f", 0,3)
           << "  T=" << RosPrintf(double(twait), "f", 0,3)
           << "  OK";
    }
  }
  
  args.SetResult(irc >= 0 ? double(twait) : double(irc));
  return kOK;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int RtclRw11Cpu::M_deposit(RtclArgs& args)
{
  uint16_t  addr=0;
  vector<uint16_t> data;
  if (!args.GetArg("addr", addr)) return kERR;
  if (!args.GetArg("data", data, 1)) return kERR;
  if (!args.AllDone()) return kERR;

  RerrMsg emsg;
  // FIXME_code: handle memory read/write error
  if (!Obj().MemWrite(addr, data, emsg)) return args.Quit(emsg);

  return kOK;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int RtclRw11Cpu::M_examine(RtclArgs& args)
{
  uint16_t  addr=0;
  if (!args.GetArg("addr", addr)) return kERR;
  if (!args.AllDone()) return kERR;

  RerrMsg emsg;
  vector<uint16_t> data;
  // FIXME_code: handle memory read/write error
  if (!Obj().MemRead(addr, data, 1, emsg)) return args.Quit(emsg);

  args.SetResult(Rtcl::NewListIntObj(data));

  return kOK;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int RtclRw11Cpu::M_lsmem(RtclArgs& args)
{
  uint16_t  abeg=0;
  if (!args.GetArg("abeg", abeg)) return kERR;
  uint16_t  aend = abeg;
  if (!args.GetArg("?aend", aend, 0xffff, abeg)) return kERR;
  if (!args.AllDone()) return kERR;

  RerrMsg emsg;
  vector<uint16_t> data;
  size_t nword = 1+(aend-abeg)/2;
  // FIXME_code: handle memory read/write error
  if (!Obj().MemRead(abeg, data, nword, emsg)) return args.Quit(emsg);

  ostringstream sos;
  for (size_t i=0; i<nword; i++) {
    sos << RosPrintBvi(uint16_t(abeg+i*2), 8) 
        << " : " <<  RosPrintBvi(data[i], 8) << endl;
  }
  
  args.SetResult(sos);

  return kOK;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int RtclRw11Cpu::M_ldabs(RtclArgs& args)
{
  static RtclNameSet optset("-trace");
  
  string opt;
  bool trace = false;
  while (args.NextOpt(opt, optset)) {
    if (opt == "-trace") trace = true;
  }
  if (!args.OptValid()) return kERR;

  string file;
  string varstart;
  if (!args.GetArg("file", file)) return kERR;
  if (!args.GetArg("??start", varstart)) return kERR;
  if (!args.AllDone()) return kERR;
  RerrMsg emsg;
  uint16_t start;
  // FIXME_code: handle memory read/write error
  if (!Obj().LoadAbs(file, emsg, start, trace)) return args.Quit(emsg);
  if (varstart.length()) {
    Tcl_Interp* interp = args.Interp();
    RtclOPtr pres(Tcl_NewIntObj(int(start)));
    if (!Rtcl::SetVar(interp, varstart, pres)) return kERR;
  }
  return kOK;
}
  
//------------------------------------------+-----------------------------------
//! FIXME_docs

int RtclRw11Cpu::M_ldasm(RtclArgs& args)
{
  static RtclNameSet optset("-lst|-sym|-opt|-file");
  Tcl_Interp* interp = args.Interp();

  string varlst;
  string varsym;
  string asmopt;
  string file;
  string code;

  string opt;
  while (args.NextOpt(opt, optset)) {
    if (opt == "-lst") {
      if (!args.GetArg("??varLst", varlst)) return kERR;
    } else if (opt == "-sym") {
      if (!args.GetArg("??varSym", varsym)) return kERR;
    } else if (opt == "-opt") {
      // don't use ?? because the argument will look like an option...
      if (!args.GetArg("opts", asmopt)) return kERR;
    } else if (opt == "-file") {
      if (!args.GetArg("??file", file)) return kERR;
    }
  }
  if (!args.OptValid()) return kERR;

  if (file.length() == 0) {
    if (!args.GetArg("code", code)) return kERR;
  }
  if (!args.AllDone()) return kERR;

  // delete sym array, otherwise old entries are preserved
  if (varsym.length()) 
    Tcl_UnsetVar(interp, varsym.c_str(), 0);

  int pipe_tcl2asm[2];                      // [0] read, [1] write end
  int pipe_asm2tcl[2];
  
  if (::pipe(pipe_tcl2asm) < 0) 
    return args.Quit(RerrMsg("RtclRw11Cpu::M_ldasm" , 
                             "1st pipe() failed: ", errno));
  if (::pipe(pipe_asm2tcl) < 0) 
    return args.Quit(RerrMsg("RtclRw11Cpu::M_ldasm" , 
                             "2nd pipe() failed: ", errno));

  pid_t pid = ::fork();
  if (pid == pid_t(0)) {                    // in child here
    vector<const char*> argv;
    vector<string>      opts;

    argv.push_back("asm-11");
    if (varlst.length()>0) argv.push_back("--olst=-");
    argv.push_back("--ocof=-");
    if (asmopt.length()) {
      istringstream optstream(asmopt);
      string tok;
      while (optstream >> tok) {
        opts.push_back(tok);
        argv.push_back(opts[opts.size()-1].c_str());
      }
    }
    if (file.length()) {
      argv.push_back(file.c_str());
    } else {
      argv.push_back("-");
    }
    argv.push_back(nullptr);
    
    ::dup2(pipe_tcl2asm[0], STDIN_FILENO);
    ::dup2(pipe_asm2tcl[1], STDOUT_FILENO);
    ::dup2(STDOUT_FILENO, STDERR_FILENO);
    ::close(pipe_tcl2asm[1]);
    ::close(pipe_asm2tcl[0]);
    ::execvp("asm-11", const_cast<char* const*>(argv.data()));
    ::perror("execvp() for asm-11 failed");
    ::exit(EXIT_FAILURE);
    
  } else {                                  // in parent here
    ::close(pipe_tcl2asm[0]);
    ::close(pipe_asm2tcl[1]);
    if (pid < pid_t(0)) 
      return args.Quit(RerrMsg("RtclRw11Cpu::M_ldasm" , 
                               "fork() failed: ", errno));
  }

  // if first line empty, drop it (created often by using {)
  if (code.length() && code[0] == '\n') code = code.substr(1);

  istringstream ostream(code);
  string oline;
  while (std::getline(ostream, oline)) {
    oline += '\n';
    //cout << "+++1:" << oline;
    if (::write(pipe_tcl2asm[1], oline.data(), oline.length()) < 0) break;
  }
  ::close(pipe_tcl2asm[1]);
  
  FILE* fp = ::fdopen(pipe_asm2tcl[0], "r");
  if (fp == nullptr) {
    ::close(pipe_asm2tcl[0]);
    return args.Quit(RerrMsg("RtclRw11Cpu::M_ldasm" , 
                             "fdopen() failed: ", errno));
  }

  vector<string> ilines;
  while(true) {
    char* pline = nullptr;
    size_t nchar;
    if (::getline(&pline, &nchar, fp) < 0) break;
    //cout << "+++2:" << pline;
    string line(pline);
    if (line.length() && line[line.length()-1] =='\n')
      line.resize(line.length()-1);
    ilines.push_back(line);
    ::free(pline);
  }
  ::fclose(fp);
  ::close(pipe_asm2tcl[0]);

  int wstat;
  int wexit = -1;
  waitpid(pid, &wstat, 0);
  if (WIFEXITED(wstat)) wexit = WEXITSTATUS(wstat);

  bool insym = false;
  bool indat = false;
  char dtyp = ' ';
  
  ostringstream los;                        // list stream
  ostringstream eos;                        // error stream
  bool lstbodyseen = false;

  typedef map<uint16_t, uint16_t>  cmap_t;

  cmap_t   cmap;
  uint16_t dot = 0;

  for (size_t i=0; i<ilines.size(); i++) {
    string& line = ilines[i];
    if (line == "sym {") {
      insym = true;
      continue;
    } else if (line == "dat {") {
      indat = true;
      continue;
    } else if (dtyp == ' ' && line == "}") {
      insym = false;
      indat = false;
      continue;
    }

    // handle symbol table
    if (insym) {
      if (varsym.length() == 0) continue;
      size_t dpos = line.find(" => ");
      if (dpos != std::string::npos) {
        string key = line.substr(0,dpos);
        string val= line.substr(dpos+4);
        if (!Tcl_SetVar2Ex(interp, varsym.c_str(), key.c_str(),
                           Tcl_NewIntObj(int(::strtol(val.c_str(),nullptr,8))),
                           TCL_LEAVE_ERR_MSG)) return kERR;
      } else {
        return args.Quit(string("bad sym spec: ") + line);
      }

    // handle data part
    } else if (indat) {
      if (dtyp == ' ') {
        if (line.length() != 10) 
          return args.Quit(string("bad dat spec: ") + line);
        dtyp = line[0];
        dot  = uint16_t(::strtol(line.c_str()+2,nullptr,8));
      } else if (line[0] == '}') {
        dtyp = ' ';
      } else {
        istringstream datstream(line);
        string dat;
        while (datstream >> dat) {
          //cout << "+++1 " << dtyp << ":" << dat << endl;
          uint16_t val = uint16_t(::strtol(dat.c_str(),nullptr,8));
          if (dtyp == 'w') {
            cmap[dot] = val;
            dot += 2;
          } else {
            uint16_t tmp = cmap[dot&0xfffe];
            if (dot & 01) {
              tmp = (val&0xff)<<8 | (tmp&0xff); // odd (high) byte
            } else {
              tmp = (tmp&0xff00)  | (val&0xff); // even (low) byte
            }
            cmap[dot&0xfffe] = tmp;
            dot += 1;
          }
        }
      }

    // handle listing part (everything not sym{} or dat{}
    } else {
      los << line << endl;
      // put lines into error stream if
      //  1. before 'Input file list:' and not starting with '--'
      //  2. after  'Input file list:' and starting with uppercase letter
      if (line == "; Input file list:") lstbodyseen = true;
      bool etake = false;
      if (lstbodyseen) {
        if (line.length() && (line[0]>'A' && line[0]<'Z')) etake = true;
      } else {
        if (line.substr(0,2) != "--") etake = true;
      }
      if (line.substr(0,6) == "asm-11") etake = true;
      if (etake) eos << line << endl;
    }
  }

  if (varlst.length()) {
    if (!Rtcl::SetVar(interp, varlst, Rtcl::NewLinesObj(los))) return kERR;
  }

  // in case of asm-11 error quit (after lst and sym processing)
  if (wexit != 0) {
    args.AppendResultLines("asm-11 compilation failed with:");
    args.AppendResultLines(eos);
    return kERR;
  }

  // now, finally, iterate over cmap and write code to memory
  vector<uint16_t> block;
  uint16_t base = 0;
  dot = 0;
  RerrMsg emsg;
  
  for (auto& o: cmap) {
    if (dot != o.first || block.size() >= Connect().BlockSizeMax()) {
      if (block.size()) {
        if (!Obj().MemWrite(base, block, emsg)) return args.Quit(emsg);
        block.clear();
      }
      base = dot = o.first;
    }
    block.push_back(o.second);
    dot += 2;
  }

  if (block.size()) {
    if (!Obj().MemWrite(base, block, emsg)) return args.Quit(emsg);
    block.clear();
  }

  return kOK;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int RtclRw11Cpu::M_boot(RtclArgs& args)
{
  string uname;
  if (!args.GetArg("uname", uname)) return kERR;
  if (!args.AllDone()) return kERR;
  RerrMsg emsg;
  if (!Obj().Boot(uname, emsg)) return args.Quit(emsg);
  return kOK;
}
  
//------------------------------------------+-----------------------------------
//! FIXME_docs

int RtclRw11Cpu::M_get(RtclArgs& args)
{
  // synchronize with server thread
  lock_guard<RlinkConnect> lock(Obj().Connect());
  return fGets.M_get(args);
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int RtclRw11Cpu::M_set(RtclArgs& args)
{
  // synchronize with server thread
  lock_guard<RlinkConnect> lock(Obj().Connect());
  return fSets.M_set(args);
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int RtclRw11Cpu::M_show(RtclArgs& args)
{
  static RtclNameSet optset("-pcps|-r0ps|-mmu|-ubmap"
                            );  

  string opt;
  uint16_t base = Obj().Base();
  ostringstream sos;
  RerrMsg emsg;

  const char* mode[4]  = {"k","s","?","u"};
  const char* rust[16] = {"init",     "HALTed",   "reset",   "stopped",
                          "stepped",  "suspend",  "hbpt",    "..run..",
                          "F:vecfet", "F:redstk", "1010",    "1011",
                          "F:seq",    "F:vmbox" , "1101",    "1111"};

  while (args.NextOpt(opt, optset)) {
    if (opt == "-pcps" || opt == "-r0ps") {
      RlinkCommandList clist;
      size_t i_pc   = clist.AddRreg(base + Rw11Cpu::kCPPC);
      size_t i_psw  = clist.AddRreg(base + Rw11Cpu::kCPPSW);
      size_t i_stat = clist.AddRreg(base + Rw11Cpu::kCPSTAT);
      if (!Server().Exec(clist, emsg)) return args.Quit(emsg);
      uint16_t psw  = clist[i_psw].Data();
      uint16_t stat = clist[i_stat].Data();
      uint16_t psw_cm    = (psw>>14) & 003;
      uint16_t psw_pm    = (psw>>12) & 003;
      uint16_t psw_set   = (psw>>11) & 001;
      uint16_t psw_pri   = (psw>>5)  & 007;
      uint16_t psw_tbit  = (psw>>4)  & 001;
      uint16_t psw_nzvc  = (psw)     & 017;
      uint16_t stat_rust = (stat>>4) & 017;
      uint16_t regs[8];
      regs[7] = clist[i_pc].Data();
      bool r0ps = opt == "-r0ps";

      if (r0ps) {
        clist.Clear();
        for (size_t i=0; i<7; i++) clist.AddRreg(base + Rw11Cpu::kCPR0+i);
        if (!Server().Exec(clist, emsg)) return args.Quit(emsg);
        for (size_t i=0; i<7; i++) regs[i] = clist[i].Data();
      }

      if (r0ps)  sos << "Processor registers and status:" << endl;
      if (!r0ps) sos << "  PC: " << RosPrintBvi(regs[7],8);
      sos << "  PS: " << RosPrintBvi(psw,8)
          << " cm,pm=" << mode[psw_cm] << "," << mode[psw_pm]
          << " s,p,t=" << psw_set << "," << psw_pri << "," << psw_tbit
          << " NZVC=" << RosPrintBvi(psw_nzvc,2,4)
          << "  rust: " << RosPrintBvi(stat_rust,8,4) << " " << rust[stat_rust]
          << endl;

      if (r0ps) {
        sos << "  R0: " << RosPrintBvi(regs[0],8)
            << "  R1: " << RosPrintBvi(regs[1],8)
            << "  R2: " << RosPrintBvi(regs[2],8)
            << "  R3: " << RosPrintBvi(regs[3],8) << endl;
        sos << "  R4: " << RosPrintBvi(regs[4],8)
            << "  R5: " << RosPrintBvi(regs[5],8)
            << "  SP: " << RosPrintBvi(regs[6],8)
            << "  PC: " << RosPrintBvi(regs[7],8) << endl;
      }

    } else if (opt == "-r0r5") {
      RlinkCommandList clist;
      for (size_t i=0; i<6; i++) clist.AddRreg(base + Rw11Cpu::kCPR0+i);
      if (!Server().Exec(clist, emsg)) return args.Quit(emsg);
      sos << "R0-R5:";
      for (size_t i=0; i<6; i++) sos << "  " << RosPrintBvi(clist[i].Data(),8);
      sos << endl;

    } else if (opt == "-mmu") {
      uint16_t mmr[4];
      uint16_t asr[3][32];
      const char* pmode[3] = {"km","sm","um"};
      const char* acf[8] = {"nres ",
                            "r -r ",
                            "r    ",
                            "011  ",
                            "rw-rw",
                            "rw- w",
                            "rw   ",
                            "111  "};
      {
        lock_guard<RlinkConnect> lock(Connect());
        RlinkCommandList clist;
        clist.AddWreg(base + Rw11Cpu::kCPAL, Rw11Cpu::kMMUMMR0);
        clist.AddRblk(base + Rw11Cpu::kCPMEMI, mmr, 3);
        clist.AddWreg(base + Rw11Cpu::kCPAL, Rw11Cpu::kMMUMMR3);
        clist.AddRblk(base + Rw11Cpu::kCPMEMI, mmr+3, 1);
        if (!Server().Exec(clist, emsg)) return args.Quit(emsg);
        clist.Clear();
        clist.AddWreg(base + Rw11Cpu::kCPAL, Rw11Cpu::kMMUPDRK);
        clist.AddRblk(base + Rw11Cpu::kCPMEMI, asr[0], 32);
        clist.AddWreg(base + Rw11Cpu::kCPAL, Rw11Cpu::kMMUPDRS);
        clist.AddRblk(base + Rw11Cpu::kCPMEMI, asr[1], 32);
        clist.AddWreg(base + Rw11Cpu::kCPAL, Rw11Cpu::kMMUPDRU);
        clist.AddRblk(base + Rw11Cpu::kCPMEMI, asr[2], 32);
        if (!Server().Exec(clist, emsg)) return args.Quit(emsg);
      }
      uint16_t mmr1_0_reg = (mmr[1]    ) & 07;
       int16_t mmr1_0_val = (mmr[1]>> 3) & 37;
      uint16_t mmr1_1_reg = (mmr[1]>> 8) & 07;
       int16_t mmr1_1_val = (mmr[1]>>11) & 37;
      uint16_t mmr3_ubmap = (mmr[3]>> 5) & 01;
      uint16_t mmr3_22bit = (mmr[3]>> 4) & 01;
      uint16_t mmr3_d_km  = (mmr[3]>> 2) & 01;
      uint16_t mmr3_d_sm  = (mmr[3]>> 1) & 01;
      uint16_t mmr3_d_um  = (mmr[3]    ) & 01;
      sos << "mmu:" << endl;
      sos << "mmr0=" << RosPrintBvi(mmr[0],8) << endl;
      if (mmr1_0_val & 020) mmr1_0_val |= 0177740;
      if (mmr1_1_val & 020) mmr1_1_val |= 0177740;
      sos << "mmr1=" << RosPrintBvi(mmr[1],8);
      if (mmr1_0_val) sos << "  r" << mmr1_0_reg
                          << ":" << RosPrintf(mmr1_0_val,"d",3);
      if (mmr1_1_val) sos << "  r" << mmr1_1_reg
                          << ":" << RosPrintf(mmr1_1_val,"d",3);
      sos << endl;
      sos << "mmr2=" << RosPrintBvi(mmr[2],8) << endl;
      sos << "mmr3=" << RosPrintBvi(mmr[3],8)
          << "  ubmap=" << mmr3_ubmap
          << "  22bit=" << mmr3_22bit
          << "  d-space k,s,u=" << mmr3_d_km
          << "," << mmr3_d_sm << "," << mmr3_d_um << endl;
      for (size_t m=0; m<3; m++) {
        sos << pmode[m] << "   "
            << " I pdr plf aw d acf     I par"
            << "    "
            << " D pdr plf aw d acf     D par" << endl;
        for (size_t i=0; i<=7; i++) {
          sos << "   " << i << " ";
          for (size_t s=0; s<=1; s++) {
            if (s!=0) sos << "    ";
            uint16_t pdr = asr[m][i   +8*s];
            uint16_t par = asr[m][i+16+8*s];
            uint16_t pdr_plf = (pdr>>8) & 0177;
            uint16_t pdr_a   = (pdr>>7) & 01;
            uint16_t pdr_w   = (pdr>>6) & 01;
            uint16_t pdr_e   = (pdr>>3) & 01;
            uint16_t pdr_acf = (pdr)    & 07;
            sos<< RosPrintBvi(pdr,8)
               << " " << RosPrintf(pdr_plf,"d",3)
               << " " << pdr_a << pdr_w
               << " " << (pdr_e ? "d" : "u")
               << " " << acf[pdr_acf]
               << "  " << RosPrintBvi(par,8);
          }
          sos << endl;
        }
      }

    } else if (opt == "-ubmap") {
      uint16_t ubmap[64];
      RlinkCommandList clist;
      clist.AddWreg(base + Rw11Cpu::kCPAL, 0170200);
      clist.AddRblk(base + Rw11Cpu::kCPMEMI, ubmap, 64);
      if (!Server().Exec(clist, emsg)) return args.Quit(emsg);
      sos << "unibus map:" << endl;
      for (size_t i = 0; i<=7; i++) {
        for (size_t j = 0; j <= 030; j+=010) {
          size_t k = 2*(i+j);
          uint32_t data = uint32_t(ubmap[k]) | (uint32_t(ubmap[k+1]))<<16;
          if (j!=0) sos << "  ";
          sos << RosPrintBvi(uint32_t(j+i),8,5) << " "
              << RosPrintBvi(data,8,22);
        }
        sos << endl;
      }
    }
  }

  if (!args.AllDone()) return kERR;
  args.SetResult(sos);

  return kOK;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int RtclRw11Cpu::M_stats(RtclArgs& args)
{
  RtclStats::Context cntx;
  if (!RtclStats::GetArgs(args, cntx)) return kERR;
  if (!RtclStats::Exec(args, cntx, Obj().Stats())) return kERR;
  return kOK;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int RtclRw11Cpu::M_dump(RtclArgs& args)
{
  int detail=0;
  if (!GetArgsDump(args, detail)) return kERR;
  if (!args.AllDone()) return kERR;

  ostringstream sos;
  Obj().Dump(sos, 0, "", detail);
  args.SetResult(sos);
  return kOK;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int RtclRw11Cpu::M_default(RtclArgs& args)
{
  if (!args.AllDone()) return kERR;
  ostringstream sos;

  vector<string> cntlnames;
  Obj().ListCntl(cntlnames);

  sos << "name type  ibbase lam  probe ena on" << endl;

  for (auto& cname : cntlnames) {
    Rw11Cntl& cntl(Obj().Cntl(cname));
    sos << RosPrintf(cntl.Name().c_str(),"-s",4)
        << " " << RosPrintf(cntl.Type().c_str(),"-s",5)
        << " " << RosPrintf(cntl.Base(),"o",6)
        << " " << RosPrintf(cntl.Lam(),"d",3)
        << "  " << cntl.ProbeStatus().Found()
        << " " << cntl.ProbeStatus().IndicatorInt()
        << " " << cntl.ProbeStatus().IndicatorRem()
        << "  " << cntl.Enable()
        << "  " << cntl.IsStarted()
        << endl;
  }

  sos << endl;
  sos << "name  attachurl" << endl;
  
  for (auto& cname : cntlnames) {
    Rw11Cntl& cntl(Obj().Cntl(cname));
    for (size_t j=0; j<cntl.NUnit(); j++) {
      Rw11Unit& unit(cntl.UnitBase(j));
      if (unit.IsAttached()) {
        sos << RosPrintf(unit.Name().c_str(),"-s",5)
            << " " << unit.AttachUrl() << endl;
      }
    }
  }
  
  args.AppendResultLines(sos);
  return kOK;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void RtclRw11Cpu::SetupGetSet()
{
  Rw11Cpu* pobj = &Obj();
  fGets.Add<const string&>("type",     bind(&Rw11Cpu::Type,     pobj));
  fGets.Add<size_t>       ("index",    bind(&Rw11Cpu::Index,    pobj));
  fGets.Add<uint16_t>     ("base",     bind(&Rw11Cpu::Base,     pobj));
  fGets.Add<uint16_t>     ("ibase",    bind(&Rw11Cpu::IBase,    pobj));
  fGets.Add<uint32_t>     ("memsize",  bind(&Rw11Cpu::MemSize,  pobj));
  fGets.Add<bool>         ("hasscnt",  bind(&Rw11Cpu::HasScnt,  pobj));
  fGets.Add<bool>         ("haspcnt",  bind(&Rw11Cpu::HasPcnt,  pobj));
  fGets.Add<bool>         ("hascmon",  bind(&Rw11Cpu::HasCmon,  pobj));
  fGets.Add<uint16_t>     ("hashbpt",  bind(&Rw11Cpu::HasHbpt,  pobj));
  fGets.Add<bool>         ("hasibmon", bind(&Rw11Cpu::HasIbmon, pobj));
  fGets.Add<bool>         ("hasibtst", bind(&Rw11Cpu::HasIbtst, pobj));
  fGets.Add<bool>         ("hasm9312", bind(&Rw11Cpu::HasM9312, pobj));
  fGets.Add<bool>         ("haskw11l", bind(&Rw11Cpu::HasKw11l, pobj));
  fGets.Add<bool>         ("haskw11p", bind(&Rw11Cpu::HasKw11p, pobj));
  fGets.Add<bool>         ("hasiist",  bind(&Rw11Cpu::HasIist,  pobj));
  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

bool RtclRw11Cpu::GetIAddr(RtclArgs& args, uint16_t& ibaddr)
{
  Tcl_Obj* pobj=0;
  if (!args.GetArg("ibaddr", pobj)) return kERR;

  int tstint;
  // if a number is given..
  if (Tcl_GetIntFromObj(nullptr, pobj, &tstint) == kOK) {
    if (tstint >= 0 && tstint <= 0xffff && (tstint & 0x1) == 0) {
      ibaddr = uint16_t(tstint);
    } else {
      args.AppendResult("-E: value '", Tcl_GetString(pobj), 
                        "' for 'addr' odd number or out of range 0...0xffff", 
                        nullptr);
      return false;
    }
  // if a name is given 
  } else {
    string name(Tcl_GetString(pobj));
    uint16_t tstaddr;
    if (Obj().IAddrMap().Find(name, tstaddr)) {
      ibaddr = tstaddr;
    } else {
      args.AppendResult("-E: no address mapping known for '", 
                        Tcl_GetString(pobj), "'", nullptr);
      return false;
    }
  }

  return true;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

bool RtclRw11Cpu::GetRAddr(RtclArgs& args, uint16_t& rbaddr)
{
  Tcl_Obj* pobj=0;
  if (!args.GetArg("rbaddr", pobj)) return kERR;

  int tstint;
  // if a number is given..
  if (Tcl_GetIntFromObj(nullptr, pobj, &tstint) == kOK) {
    if (tstint >= 0 && tstint <= 0xffff) {
      rbaddr = uint16_t(tstint);
    } else {
      args.AppendResult("-E: value '", Tcl_GetString(pobj), 
                        "' for 'addr' out of range 0...0xffff", 
                        nullptr);
      return false;
    }
  // if a name is given 
  } else {
    string name(Tcl_GetString(pobj));
    uint16_t tstaddr;
    if (Obj().RAddrMap().Find(name, tstaddr)) {
      rbaddr = tstaddr;
    } else if (Connect().AddrMap().Find(name, tstaddr)) {
      rbaddr = tstaddr;
    } else {
      args.AppendResult("-E: no address mapping known for '", 
                        Tcl_GetString(pobj), "'", nullptr);
      return false;
    }
  }

  return true;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

bool RtclRw11Cpu::GetVarName(RtclArgs& args, const char* argname, 
                             size_t nind, 
                             std::vector<std::string>& varname)
{
  while (varname.size() < nind+1) varname.emplace_back(string());
  string name;
  if (!args.GetArg(argname, name)) return false;
  if (name.length()) {                      // if variable defined
    char c = name[0];
    if (isdigit(c) || c=='+' || c=='-' ) {  // check for mistaken number
      args.AppendResult("-E: invalid variable name '", name.c_str(), 
                        "': looks like a number", nullptr);
      return false;
    }
  }
  
  varname[nind] = name;
  return true;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

bool RtclRw11Cpu::ClistNonEmpty(RtclArgs& args, const RlinkCommandList& clist)
{
  if (clist.Size() == 0) {
    args.AppendResult("-E: -edata, -edone, or -estat "
                      "not allowed on empty command list", nullptr);
    return false;
  }
  return true;
}
  
//------------------------------------------+-----------------------------------
//! FIXME_docs

Tcl_Obj* RtclRw11Cpu::ControllerCommands()
{
  vector<string> cntlnames;
  Obj().ListCntl(cntlnames);

  Tcl_Obj* rlist = Tcl_NewListObj(0,nullptr);
  for (auto& cname : cntlnames) {
    string ccmd = CommandName() + cname;
    RtclOPtr pele(Tcl_NewStringObj(ccmd.data(), ccmd.length()));
    Tcl_ListObjAppendElement(nullptr, rlist, pele);
  }
  return rlist;
}

} // end namespace Retro
