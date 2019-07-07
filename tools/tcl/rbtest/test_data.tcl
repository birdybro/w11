# $Id: test_data.tcl 1177 2019-06-30 12:34:07Z mueller $
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2011-2016 by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
#
#  Revision History:
# Date         Rev Version  Comment
# 2016-06-19   777   2.1.1  add dinc tests
# 2015-04-03   661   2.1    drop estatdef, use estattout
# 2014-12-21   617   2.0.1  use rbtout stat bit for timeout
# 2014-11-09   603   2.0    use rlink v4 address layout and iface
# 2011-03-27   374   1.0    Initial version
# 2011-03-13   369   0.1    First Draft
#

package provide rbtest 1.0

package require rutiltpp
package require rutil
package require rlink

namespace eval rbtest {
  #
  # Basic tests with cntl, stat, data and dinc registers.
  # All tests depend only on rbd_tester logic alone and not on how the
  # rbd_tester is embedded in the design (e.g. stat and attn connections)
  #
  proc test_data {} {
    #
    set errcnt 0
    rlc errcnt -clear
    #
    rlc log "rbtest::test_data - init: clear cntl, data, and fifo"
    # Note: fifo clear via init is tested later, used here 'speculatively'
    rlc exec -init te.cntl [regbld rbtest::INIT fifo data cntl]
    #
    #-------------------------------------------------------------------------
    rlc log "  test 1a: cntl, stat and data are write- and read-able"
    foreach {addr valw valr} [list te.cntl 0xffff 0x83ff \
                                   te.cntl 0x0000 0x0000 \
                                   te.stat 0xffff 0x000f \
                                   te.stat 0x0000 0x0000 \
                                   te.data 0xffff 0xffff \
                                   te.data 0x0000 0x0000 ] {
      rlc exec -wreg $addr $valw
      rlc exec -rreg $addr -edata $valr
    }    
    #
    #
    rlc log "  test 1b: as test 1a, use clists, check cntl,stat,data distinct"
    foreach {valc vals vald} [list 0x1 0x2 0x3 0x0 0x0 0x0] {
      rlc exec \
        -wreg te.cntl $valc \
        -wreg te.stat $vals \
        -wreg te.data $vald \
        -rreg te.cntl -edata $valc \
        -rreg te.stat -edata $vals \
        -rreg te.data -edata $vald
    }
    #
    #-------------------------------------------------------------------------
    rlc log "  test 2: verify that large nbusy causes timeout"
    rlc exec \
      -wreg te.data 0xdead \
      -rreg te.data -edata 0xdead \
      -wreg te.cntl [regbld rbtest::CNTL {nbusy 0x3ff}] \
      -wreg te.data 0xbeaf -estattout \
      -rreg te.data        -estattout \
      -wreg te.cntl 0x0000 \
      -rreg te.data -edata 0xdead
    #
    # -------------------------------------------------------------------------
    rlc log "  test 3a: verify that init 001 clears cntl,stat and not data"
    set valc [regbld rbtest::CNTL {nbusy 1}]
    rlc exec \
      -wreg te.cntl $valc \
      -wreg te.stat 0x0002 \
      -wreg te.data 0x1234 \
      -init te.cntl [regbld rbtest::INIT cntl] \
      -rreg te.cntl -edata 0x0 \
      -rreg te.stat -edata 0x0 \
      -wreg te.data 0x1234
    rlc log "  test 3b: verify that init 010 clears data and not cntl,stat"
    set valc [regbld rbtest::CNTL {nbusy 2}]
    rlc exec \
      -wreg te.cntl $valc \
      -wreg te.stat 0x0003 \
      -wreg te.data 0x4321 \
      -init te.cntl [regbld rbtest::INIT data] \
      -rreg te.cntl -edata $valc \
      -rreg te.stat -edata 0x0003 \
      -wreg te.data 0x0
    rlc log "  test 3c: verify that init 011 clears data and cntl,stat"
    rlc exec \
      -wreg te.cntl [regbld rbtest::CNTL {nbusy 3}] \
      -wreg te.stat 0x0004 \
      -wreg te.data 0xabcd \
      -init te.cntl [regbld rbtest::INIT data cntl] \
      -rreg te.cntl -edata 0x0 \
      -rreg te.stat -edata 0x0 \
      -wreg te.data 0x0
    #
    # -------------------------------------------------------------------------
    rlc log "  test 4: test that te.ncyc returns # of cycles for te.data w&r"
    foreach nbusy {0x03 0x07 0x0f 0x1f 0x00} {
      set valc [regbld rbtest::CNTL [list nbusy $nbusy]]
      rlc exec \
        -wreg te.cntl $valc \
        -wreg te.data [expr {$nbusy | ( $nbusy << 8 ) }] \
        -rreg te.ncyc -edata [expr {$nbusy + 1 }] \
        -rreg te.data -edata [expr {$nbusy | ( $nbusy << 8 ) }] \
        -rreg te.ncyc -edata [expr {$nbusy + 1 }] 
    }
    #
    # -------------------------------------------------------------------------
    rlc log "  test 5a: test dinc: post-increment on read"
      rlc exec \
        -wreg te.data 0x1100 \
        -rreg te.dinc -edata 0x1100 \
        -rreg te.dinc -edata 0x1101 \
        -rreg te.dinc -edata 0x1102 \
        -rreg te.data -edata 0x1103
    #
    # -------------------------------------------------------------------------
    rlc log "  test 5b: test dinc: write-check and post-increment on write"
      # clear wchk, do proper writes
      rlc exec \
        -wreg te.cntl [regbld rbtest::CNTL {wchk 0} {nbusy 0}] \
        -wreg te.data 0x1200 \
        -wreg te.dinc 0x1200 \
        -wreg te.dinc 0x1201 \
        -wreg te.dinc 0x1202 \
        -rreg te.data -edata 0x1203 \
        -rreg te.cntl -edata [regbld rbtest::CNTL {wchk 0}]
      # wchk still clear; bad write (ff03, expected 1203); check wchk;
      #   good write; check wchk (must stick); check that data write clears wchk
      rlc exec \
        -wreg te.dinc 0xff03 \
        -rreg te.cntl -edata [regbld rbtest::CNTL {wchk 1}] \
        -wreg te.dinc 0x1204 \
        -rreg te.cntl -edata [regbld rbtest::CNTL {wchk 1}] \
        -rreg te.dinc -edata 0x1205 \
        -wreg te.data 0x1300 \
        -rreg te.cntl -edata [regbld rbtest::CNTL {wchk 0}]
    #
    #-------------------------------------------------------------------------
    rlc log "rbtest::test_data - cleanup: clear cntl and data"
    rlc exec -init te.cntl [regbld rbtest::INIT data cntl]
    #
    incr errcnt [rlc errcnt -clear]
    return $errcnt
  }
}
