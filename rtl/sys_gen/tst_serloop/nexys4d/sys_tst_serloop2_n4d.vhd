-- $Id: sys_tst_serloop2_n4d.vhd 984 2018-01-02 20:56:27Z mueller $
--
-- Copyright 2017- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
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
-- Module Name:    sys_tst_serloop1_n4d - syn
-- Description:    Tester serial link for nexys4d (serport_1clock case)
--
-- Dependencies:   vlib/xlib/s7_cmt_sfs
--                 vlib/genlib/clkdivce
--                 bpgen/bp_rs232_4line_iob
--                 bpgen/sn_humanio
--                 tst_serloop_hiomap
--                 vlib/serport/serport_2clock2
--                 tst_serloop
--
-- Test bench:     -
--
-- Target Devices: generic
-- Tool versions:  viv 2016.2; ghdl 0.33
--
-- Synthesized:
-- Date         Rev  viv    Target       flop  lutl  lutm  bram  slic
-- 2017-xx-xx   xxx 2016.2  xc7a100t-1    xxx  xxxx    xx     0   xxx  
--
-- Revision History: 
-- Date         Rev Version  Comment
-- 2017-01-04   838   1.0    Initial version (derived from sys_tst_serloop2_n4)
------------------------------------------------------------------------------
--

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

use work.slvtypes.all;
use work.xlib.all;
use work.genlib.all;
use work.bpgenlib.all;
use work.tst_serlooplib.all;
use work.serportlib.all;
use work.sys_conf.all;

-- ----------------------------------------------------------------------------

entity sys_tst_serloop2_n4d is          -- top level
                                        -- implements nexys4d_aif
  port (
    I_CLK100 : in slbit;                -- 100 MHz clock
    I_RXD : in slbit;                   -- receive data (board view)
    O_TXD : out slbit;                  -- transmit data (board view)
    O_RTS_N : out slbit;                -- rx rts (board view; act.low)
    I_CTS_N : in slbit;                 -- tx cts (board view; act.low)
    I_SWI : in slv16;                   -- n4d switches
    I_BTN : in slv5;                    -- n4d buttons
    I_BTNRST_N : in slbit;              -- n4d reset button
    O_LED : out slv16;                  -- n4d leds
    O_RGBLED0 : out slv3;               -- n4d rgb-led 0
    O_RGBLED1 : out slv3;               -- n4d rgb-led 1
    O_ANO_N : out slv8;                 -- 7 segment disp: anodes   (act.low)
    O_SEG_N : out slv8                  -- 7 segment disp: segments (act.low)
  );
end sys_tst_serloop2_n4d;

architecture syn of sys_tst_serloop2_n4d is

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

  GEN_CLKSYS : s7_cmt_sfs               -- clock generator system ------------
    generic map (
      VCO_DIVIDE     => sys_conf_clksys_vcodivide,
      VCO_MULTIPLY   => sys_conf_clksys_vcomultiply,
      OUT_DIVIDE     => sys_conf_clksys_outdivide,
      CLKIN_PERIOD   => 10.0,
      CLKIN_JITTER   => 0.01,
      STARTUP_WAIT   => false,
      GEN_TYPE       => sys_conf_clksys_gentype)
    port map (
      CLKIN   => I_CLK100,
      CLKFX   => CLK,
      LOCKED  => open
    );

  CLKDIV_CLK : clkdivce
    generic map (
      CDUWIDTH => 8,
      USECDIV  => sys_conf_clksys_mhz,
      MSECDIV  => sys_conf_clksys_msecdiv)
    port map (
      CLK     => CLK,
      CE_USEC => open,
      CE_MSEC => CE_MSEC
    );

  GEN_CLKSER : s7_cmt_sfs               -- clock generator serport -----------
    generic map (
      VCO_DIVIDE     => sys_conf_clkser_vcodivide,
      VCO_MULTIPLY   => sys_conf_clkser_vcomultiply,
      OUT_DIVIDE     => sys_conf_clkser_outdivide,
      CLKIN_PERIOD   => 10.0,
      CLKIN_JITTER   => 0.01,
      STARTUP_WAIT   => false,
      GEN_TYPE       => sys_conf_clkser_gentype)
    port map (
      CLKIN   => I_CLK100,
      CLKFX   => CLKS,
      LOCKED  => open
    );

  CLKDIV_CLKS : clkdivce
    generic map (
      CDUWIDTH => 7,                    -- good up to 127 MHz
      USECDIV  => sys_conf_clkser_mhz,
      MSECDIV  => sys_conf_clkser_msecdiv)
    port map (
      CLK     => CLKS,
      CE_USEC => open,
      CE_MSEC => CES_MSEC
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

  -- setup unused outputs in nexys4d
  O_RGBLED0 <= (others=>'0');
  O_RGBLED1 <= (others=>not I_BTNRST_N);

end syn;
