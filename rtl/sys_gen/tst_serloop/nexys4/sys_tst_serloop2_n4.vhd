-- $Id: sys_tst_serloop2_n4.vhd 1086 2018-12-16 18:29:55Z mueller $
--
-- Copyright 2016-2018 by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
--
-- This program is free software; you may redistribute and/or modify it under
-- the terms of the GNU General Public License as published by the Free
-- Software Foundation, either version 3, or (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful, but
-- WITHOUT ANY WARRANTY, without even the implied warranty of MERCHANTABILITY
-- or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
-- for complete details.
--
------------------------------------------------------------------------------
-- Module Name:    sys_tst_serloop2_n4 - syn
-- Description:    Tester serial link for nexys4 (serport_2clock case)
--
-- Dependencies:   bplib/bpgen/s7_cmt_1ce1ce
--                 bpgen/bp_rs232_4line_iob
--                 bpgen/sn_humanio
--                 tst_serloop_hiomap
--                 vlib/serport/serport_2clock2
--                 tst_serloop
--
-- Test bench:     -
--
-- Target Devices: generic
-- Tool versions:  viv 2014.4-2018.2; ghdl 0.31-0.34
--
-- Synthesized:
-- Date         Rev  viv    Target       flop  lutl  lutm  bram  slic
-- 2016-03-25   751 2015.4  xc7a100t-1    415  402x    32     0   185  
--
-- Revision History: 
-- Date         Rev Version  Comment
-- 2018-12-16  1086   1.1    use s7_cmt_1ce1ce
-- 2016-06-05   722   1.0.1  use CDUWIDTH=7 for CLKS, 120 MHz is natural choice
-- 2015-02-01   641   1.0    Initial version (derived from sys_tst_serloop1_n4)
------------------------------------------------------------------------------
--

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

use work.slvtypes.all;
use work.genlib.all;
use work.bpgenlib.all;
use work.tst_serlooplib.all;
use work.serportlib.all;
use work.sys_conf.all;

-- ----------------------------------------------------------------------------

entity sys_tst_serloop2_n4 is           -- top level
                                        -- implements nexys4_aif
  port (
    I_CLK100 : in slbit;                -- 100 MHz clock
    I_RXD : in slbit;                   -- receive data (board view)
    O_TXD : out slbit;                  -- transmit data (board view)
    O_RTS_N : out slbit;                -- rx rts (board view; act.low)
    I_CTS_N : in slbit;                 -- tx cts (board view; act.low)
    I_SWI : in slv16;                   -- n4 switches
    I_BTN : in slv5;                    -- n4 buttons
    I_BTNRST_N : in slbit;              -- n4 reset button
    O_LED : out slv16;                  -- n4 leds
    O_RGBLED0 : out slv3;               -- n4 rgb-led 0
    O_RGBLED1 : out slv3;               -- n4 rgb-led 1
    O_ANO_N : out slv8;                 -- 7 segment disp: anodes   (act.low)
    O_SEG_N : out slv8                  -- 7 segment disp: segments (act.low)
  );
end sys_tst_serloop2_n4;

architecture syn of sys_tst_serloop2_n4 is

  signal CLK :   slbit := '0';
  signal RESET : slbit := '0';

  signal CE_USEC : slbit := '0';
  signal CE_MSEC : slbit := '0';

  signal CLKS :  slbit := '0';
  signal CES_MSEC : slbit := '0';

  signal RXD :   slbit := '0';
  signal TXD :   slbit := '0';
  signal CTS_N : slbit := '0';
  signal RTS_N : slbit := '0';
  
  signal SWI     : slv16 := (others=>'0');
  signal BTN     : slv5  := (others=>'0');
  signal LED     : slv16 := (others=>'0');  
  signal DSP_DAT : slv32 := (others=>'0');
  signal DSP_DP  : slv8  := (others=>'0');

  signal HIO_CNTL : hio_cntl_type := hio_cntl_init;
  signal HIO_STAT : hio_stat_type := hio_stat_init;
  
  signal RXDATA : slv8  := (others=>'0');
  signal RXVAL :  slbit := '0';
  signal RXHOLD : slbit := '0';
  signal TXDATA : slv8  := (others=>'0');
  signal TXENA :  slbit := '0';
  signal TXBUSY : slbit := '0';
  
  signal SER_MONI : serport_moni_type  := serport_moni_init;

begin

  GEN_CLKALL : s7_cmt_1ce1ce            -- clock generator system ------------
    generic map (
      CLKIN_PERIOD   => 10.0,
      CLKIN_JITTER   => 0.01,
      STARTUP_WAIT   => false,
      CLK0_VCODIV    => sys_conf_clksys_vcodivide,
      CLK0_VCOMUL    => sys_conf_clksys_vcomultiply,
      CLK0_OUTDIV    => sys_conf_clksys_outdivide,
      CLK0_GENTYPE   => sys_conf_clksys_gentype,
      CLK0_CDUWIDTH  => 8,
      CLK0_USECDIV   => sys_conf_clksys_mhz,
      CLK0_MSECDIV   => sys_conf_clksys_msecdiv,      
      CLK1_VCODIV    => sys_conf_clkser_vcodivide,
      CLK1_VCOMUL    => sys_conf_clkser_vcomultiply,
      CLK1_OUTDIV    => sys_conf_clkser_outdivide,
      CLK1_GENTYPE   => sys_conf_clkser_gentype,
      CLK1_CDUWIDTH  => 7,
      CLK1_USECDIV   => sys_conf_clkser_mhz,
      CLK1_MSECDIV   => sys_conf_clkser_msecdiv)
    port map (
      CLKIN     => I_CLK100,
      CLK0      => CLK,
      CE0_USEC  => CE_USEC,
      CE0_MSEC  => CE_MSEC,
      CLK1      => CLKS,
      CE1_USEC  => open,
      CE1_MSEC  => CES_MSEC,
      LOCKED    => open
    );

  HIO : sn_humanio
    generic map (
      SWIDTH   => 16,
      BWIDTH   =>  5,
      LWIDTH   => 16,
      DCWIDTH  =>  3,
      DEBOUNCE => sys_conf_hio_debounce)
    port map (
      CLK     => CLK,
      RESET   => '0',
      CE_MSEC => CE_MSEC,
      SWI     => SWI,
      BTN     => BTN,
      LED     => LED,
      DSP_DAT => DSP_DAT,
      DSP_DP  => DSP_DP,
      I_SWI   => I_SWI,
      I_BTN   => I_BTN,
      O_LED   => O_LED,
      O_ANO_N => O_ANO_N,
      O_SEG_N => O_SEG_N
    );

  RESET <= BTN(0);                      -- BTN(0) will reset tester !!
  
  HIOMAP : tst_serloop_hiomap
    port map (
      CLK      => CLK,
      RESET    => RESET,
      HIO_CNTL => HIO_CNTL,
      HIO_STAT => HIO_STAT,
      SER_MONI => SER_MONI,
      SWI      => SWI(7 downto 0),
      BTN      => BTN(3 downto 0),
      LED      => LED(7 downto 0),
      DSP_DAT  => DSP_DAT(15 downto 0),
      DSP_DP   => DSP_DP(3 downto 0)
    );

  IOB_RS232 : bp_rs232_4line_iob
    port map (
      CLK     => CLKS,
      RXD     => RXD,
      TXD     => TXD,
      CTS_N   => CTS_N,
      RTS_N   => RTS_N,
      I_RXD   => I_RXD,
      O_TXD   => O_TXD,
      I_CTS_N => I_CTS_N,
      O_RTS_N => O_RTS_N
    );
  
  SERPORT : serport_2clock2
    generic map (
      CDWIDTH   => 12,
      CDINIT    => sys_conf_uart_cdinit,
      RXFAWIDTH => 5,
      TXFAWIDTH => 5)
    port map (
      CLKU     => CLK,
      RESET    => RESET,
      CLKS     => CLKS,
      CES_MSEC => CES_MSEC,
      ENAXON   => HIO_CNTL.enaxon,
      ENAESC   => HIO_CNTL.enaesc,
      RXDATA   => RXDATA,
      RXVAL    => RXVAL,
      RXHOLD   => RXHOLD,
      TXDATA   => TXDATA,
      TXENA    => TXENA,
      TXBUSY   => TXBUSY,
      MONI     => SER_MONI,
      RXSD     => RXD,
      TXSD     => TXD,
      RXRTS_N  => RTS_N,
      TXCTS_N  => CTS_N
    );

  TESTER : tst_serloop
    port map (
      CLK      => CLK,
      RESET    => RESET,
      CE_MSEC  => CE_MSEC,
      HIO_CNTL => HIO_CNTL,
      HIO_STAT => HIO_STAT,
      SER_MONI => SER_MONI,
      RXDATA   => RXDATA,
      RXVAL    => RXVAL,
      RXHOLD   => RXHOLD,
      TXDATA   => TXDATA,
      TXENA    => TXENA,
      TXBUSY   => TXBUSY
    );

  -- show autobauder clock divisor on msb of display
  DSP_DAT(31 downto 20) <= SER_MONI.abclkdiv(11 downto 0);
  DSP_DAT(19)           <= '0';
  DSP_DAT(18 downto 16) <= SER_MONI.abclkdiv_f;
  DSP_DP(7 downto 4) <= "0010";

  -- setup unused outputs in nexys4
  O_RGBLED0 <= (others=>'0');
  O_RGBLED1 <= (others=>not I_BTNRST_N);

end syn;
