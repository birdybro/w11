-- $Id: ib_sres_or_3.vhd 314 2010-07-09 17:38:41Z mueller $
--
-- Copyright 2007-2008 by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
--
-- This program is free software; you may redistribute and/or modify it under
-- the terms of the GNU General Public License as published by the Free
-- Software Foundation, either version 2, or at your option any later version.
--
-- This program is distributed in the hope that it will be useful, but
-- WITHOUT ANY WARRANTY, without even the implied warranty of MERCHANTABILITY
-- or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
-- for complete details.
--
------------------------------------------------------------------------------
-- Module Name:    ib_sres_or_3 - syn
-- Description:    ibus: result or, 3 input
--
-- Dependencies:   -
-- Test bench:     tb/tb_pdp11_core (implicit)
-- Target Devices: generic
-- Tool versions:  xst 8.1, 8.2, 9.1, 9.2; ghdl 0.18-0.25
-- Revision History: 
-- Date         Rev Version  Comment
-- 2008-08-22   161   1.0.2  renamed pdp11_ibres_ -> ib_sres_; use iblib
-- 2008-01-05   110   1.0.1  rename IB_MREQ(ena->req) SRES(sel->ack, hold->busy)
-- 2007-12-29   107   1.0    Initial version 
------------------------------------------------------------------------------

library ieee;
use ieee.std_logic_1164.all;

use work.slvtypes.all;
use work.iblib.all;

-- ----------------------------------------------------------------------------

entity ib_sres_or_3 is                  -- ibus result or, 3 input
  port (
    IB_SRES_1 :  in ib_sres_type;                 -- ib_sres input 1
    IB_SRES_2 :  in ib_sres_type := ib_sres_init; -- ib_sres input 2
    IB_SRES_3 :  in ib_sres_type := ib_sres_init; -- ib_sres input 3
    IB_SRES_OR : out ib_sres_type       -- ib_sres or'ed output
  );
end ib_sres_or_3;

architecture syn of ib_sres_or_3 is
  
begin

  proc_comb : process (IB_SRES_1, IB_SRES_2, IB_SRES_3)
  begin

    IB_SRES_OR.ack  <= IB_SRES_1.ack or
                       IB_SRES_2.ack or
                       IB_SRES_3.ack;
    IB_SRES_OR.busy <= IB_SRES_1.busy or
                       IB_SRES_2.busy or
                       IB_SRES_3.busy;
    IB_SRES_OR.dout <= IB_SRES_1.dout or
                       IB_SRES_2.dout or
                       IB_SRES_3.dout;
    
  end process proc_comb;
  
end syn;