// license:BSD-3-Clause
// copyright-holders: Robbbert
/**********************************************************************************

TRS-80 DT-1

Tandy's Data Terminal.

Skeleton driver commenced on 2017-10-25.

Core bugs noted:
- If AM_REGION used to locate the main rom in another region, validation
  complains that region ':maincpu' not found.
- If region 'maincpu' changed to 0x1000 (same size as the rom), a fatal error
  of duplicate save state occurs at start.


ToDo:
- Serial i/o
- Centronics printer
- Fix video - displays every other line on top & bottom half of the screen
- Check that attributes are correctly applied
- Connect up ports 1 and 3.
- Fix timing issue with timer interrupt and remove hack

You can get into the setup menu by pressing Ctrl+Shift+Enter.


**********************************************************************************/

#include "emu.h"
#include "cpu/mcs51/mcs51.h"
#include "video/i8275.h"
#include "machine/7474.h"
#include "machine/x2212.h"
#include "sound/beep.h"
#include "emupal.h"
#include "screen.h"
#include "speaker.h"
//#include "logmacro.h"

class trs80dt1_state : public driver_device
{
public:
	trs80dt1_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_p_videoram(*this, "videoram")
		, m_p_chargen(*this, "chargen")
		, m_maincpu(*this, "maincpu")
		, m_palette(*this, "palette")
		, m_crtc(*this, "crtc")
		, m_nvram(*this,"nvram")
		, m_keyboard(*this, "X%u", 0)
		, m_beep(*this, "beeper")
		, m_7474(*this, "7474")
		{ }

	void trs80dt1(machine_config &config);

private:
	DECLARE_READ8_MEMBER(dma_r);
	DECLARE_READ8_MEMBER(key_r);
	DECLARE_WRITE8_MEMBER(store_w);
	DECLARE_WRITE8_MEMBER(port1_w);
	DECLARE_WRITE8_MEMBER(port3_w);
	I8275_DRAW_CHARACTER_MEMBER(crtc_update_row);

	void io_map(address_map &map);
	void prg_map(address_map &map);

	bool m_bow;
	virtual void machine_reset() override;
	virtual void machine_start() override;
	required_shared_ptr<u8> m_p_videoram;
	required_region_ptr<u8> m_p_chargen;
	required_device<cpu_device> m_maincpu;
	required_device<palette_device> m_palette;
	required_device<i8275_device> m_crtc;
	required_device<x2210_device> m_nvram;
	required_ioport_array<10> m_keyboard;
	required_device<beep_device> m_beep;
	required_device<ttl7474_device> m_7474;
};

void trs80dt1_state::machine_reset()
{
	m_bow = 0;
	m_7474->preset_w(1);
	// line is active low in the real chip
	m_nvram->recall(1);
	m_nvram->recall(0);
}

READ8_MEMBER( trs80dt1_state::dma_r )
{
	m_crtc->dack_w(space, 0, m_p_videoram[offset]); // write to /BS pin
	// Temporary hack to work around a timing issue
	// timer interrupts fires too early and ends the psuedo-dma transfer after only 77 chars
	// we send the last three manually until this is fixed
	if (offset%80 == 76) {
		offset++;
		m_crtc->dack_w(space, 0, m_p_videoram[offset]);
		offset++;
		m_crtc->dack_w(space, 0, m_p_videoram[offset]);
		offset++;
		m_crtc->dack_w(space, 0, m_p_videoram[offset]);
	}
	return 0x7f;
}

READ8_MEMBER( trs80dt1_state::key_r )
{
	offset &= 15;
	if (offset < 9)
		return m_keyboard[offset]->read();
	else
		return 0xff;
}

WRITE8_MEMBER( trs80dt1_state::store_w )
{
	// line is active low in the real chip
	m_nvram->store(1);
	m_nvram->store(0);
}

/*
d0 : /PSTRB (centronics strobe)
d1 : TRPRT
d2 : /SP BUSY
d3 : /RTS
d4 : BOW (applies reverse video to entire screen)
d5 : /DTR
d6 : PP BUSY (printer busy - input)
d7 : n/c */
WRITE8_MEMBER( trs80dt1_state::port1_w )
{
	m_bow = BIT(data, 4);
}

/*
d4 : beeper
d5 : Printer enable */
WRITE8_MEMBER( trs80dt1_state::port3_w )
{
	m_beep->set_state(BIT(data, 4));
}

void trs80dt1_state::prg_map(address_map &map)
{
	map(0x0000, 0x0fff).rom();
	map(0x2000, 0x27ff).r(FUNC(trs80dt1_state::dma_r));
}

void trs80dt1_state::io_map(address_map &map)
{
	map.global_mask(0xbfff); // A14 not used
	map(0xa000, 0xa7ff).ram().share("videoram");
	map(0xa800, 0xa83f).mirror(0x3c0).rw(m_nvram, FUNC(x2210_device::read), FUNC(x2210_device::write)); // X2210
	map(0xac00, 0xafff).r(FUNC(trs80dt1_state::key_r));
	map(0xb000, 0xb3ff).portr("X9"); // also reads some RS232 inputs
	map(0xb400, 0xb7ff).w(FUNC(trs80dt1_state::store_w));
	map(0xbc00, 0xbc01).mirror(0x3fe).rw(m_crtc, FUNC(i8275_device::read), FUNC(i8275_device::write)); // i8276
}

/* Input ports */
static INPUT_PORTS_START( trs80dt1 )
	PORT_START("X0")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_1) PORT_CHAR('1') PORT_CHAR('!')
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_DEL) PORT_CHAR(0x7f)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_W) PORT_CHAR('w') PORT_CHAR('W')
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_TAB) PORT_CHAR(9)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_K) PORT_CHAR('k') PORT_CHAR('K')
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_SPACE) PORT_CHAR(32)
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_8_PAD)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_9_PAD)

	PORT_START("X1")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_2) PORT_CHAR('2') PORT_CHAR('@')
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("BS") PORT_CODE(KEYCODE_BACKSPACE) PORT_CHAR(8)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_E) PORT_CHAR('e') PORT_CHAR('E')
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_Q) PORT_CHAR('q') PORT_CHAR('Q')
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_L) PORT_CHAR('l') PORT_CHAR('L')
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_Z) PORT_CHAR('z') PORT_CHAR('Z')
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_7_PAD)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_4_PAD)

	PORT_START("X2")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_3) PORT_CHAR('3') PORT_CHAR('#')
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_MINUS) PORT_CHAR('-') PORT_CHAR('_')
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_R) PORT_CHAR('r') PORT_CHAR('R')
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_A) PORT_CHAR('a') PORT_CHAR('A')
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_COLON) PORT_CHAR(';') PORT_CHAR(':')
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_X) PORT_CHAR('x') PORT_CHAR('X')
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_5_PAD)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_6_PAD)

	PORT_START("X3")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_4) PORT_CHAR('4') PORT_CHAR('$')
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_EQUALS) PORT_CHAR('=') PORT_CHAR('+')
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_T) PORT_CHAR('t') PORT_CHAR('T')
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_S) PORT_CHAR('s') PORT_CHAR('S')
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_QUOTE) PORT_CHAR('\'') PORT_CHAR('\"')
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_C) PORT_CHAR('c') PORT_CHAR('C')
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_1_PAD) PORT_CHAR('1') PORT_CHAR('\\')
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_2_PAD)

	PORT_START("X4")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_5) PORT_CHAR('5') PORT_CHAR('%')
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_0) PORT_CHAR('0') PORT_CHAR(')')
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_Y) PORT_CHAR('y') PORT_CHAR('Y')
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_D) PORT_CHAR('d') PORT_CHAR('D')
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Enter") PORT_CODE(KEYCODE_ENTER) PORT_CHAR(13)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_V) PORT_CHAR('v') PORT_CHAR('V')
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_3_PAD) PORT_CHAR('3') PORT_CHAR('~')
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_0_PAD)

	PORT_START("X5")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_6) PORT_CHAR('6') PORT_CHAR('^')
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_END) PORT_CHAR(3)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_U) PORT_CHAR('u') PORT_CHAR('U')
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_F) PORT_CHAR('f') PORT_CHAR('F')
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_ESC) PORT_CHAR(27)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_B) PORT_CHAR('b') PORT_CHAR('B')
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_DEL_PAD)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_ENTER_PAD)

	PORT_START("X6")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_7) PORT_CHAR('7') PORT_CHAR('&')
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Clear") PORT_CODE(KEYCODE_HOME)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_I) PORT_CHAR('i') PORT_CHAR('I')
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_G) PORT_CHAR('g') PORT_CHAR('G')
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Linefeed") PORT_CODE(KEYCODE_INSERT)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_N) PORT_CHAR('n') PORT_CHAR('N')
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_UNUSED)

	PORT_START("X7")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_8) PORT_CHAR('8') PORT_CHAR('*')
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_CLOSEBRACE) PORT_CHAR('}') PORT_CHAR(']')
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_O) PORT_CHAR('o') PORT_CHAR('O')
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_H) PORT_CHAR('h') PORT_CHAR('H')
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_SLASH) PORT_CHAR('/') PORT_CHAR('?')
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_M) PORT_CHAR('m') PORT_CHAR('M')
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_UNUSED)

	PORT_START("X8")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_9) PORT_CHAR('9') PORT_CHAR('(')
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_OPENBRACE) PORT_CHAR('{') PORT_CHAR('[')
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_P) PORT_CHAR('p') PORT_CHAR('P')
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_J) PORT_CHAR('j') PORT_CHAR('J')
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_STOP) PORT_CHAR('.') PORT_CHAR('>')
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_COMMA) PORT_CHAR(',') PORT_CHAR('<')
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_UNUSED)

	PORT_START("X9")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_RSHIFT) PORT_CODE(KEYCODE_LSHIFT)
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_RCONTROL) PORT_CODE(KEYCODE_LCONTROL)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_CAPSLOCK) PORT_TOGGLE
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_UNUSED) // Jumper - LOW for 60Hz, high for 50Hz
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_UNUSED) // No Connect
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_UNUSED)
INPUT_PORTS_END

void trs80dt1_state::machine_start()
{
	m_palette->set_pen_color(0, rgb_t(0x00,0x00,0x00)); // black
	m_palette->set_pen_color(1, rgb_t(0x00,0xa0,0x00)); // normal
	m_palette->set_pen_color(2, rgb_t(0x00,0xff,0x00)); // highlight
}

const gfx_layout trs80dt1_charlayout =
{
	8, 8,             /* 8x16 characters - the last 8 lines are always blank */
	128,                /* 128 characters */
	1,              /* 1 bits per pixel */
	{0},                /* no bitplanes; 1 bit per pixel */
	{0,1,2,3,4,5,6,7},
	{0, 8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*16                /* space between characters */
};

static GFXDECODE_START( gfx_trs80dt1 )
	GFXDECODE_ENTRY( "chargen", 0x0000, trs80dt1_charlayout, 0, 1 )
GFXDECODE_END


I8275_DRAW_CHARACTER_MEMBER( trs80dt1_state::crtc_update_row )
{
	charcode &= 0x7f;
	linecount &= 15;

	const rgb_t *palette = m_palette->palette()->entry_list_raw();
	u8 gfx = (lten) ? 0xff : 0;
	if (!vsp)
		gfx = m_p_chargen[linecount | (charcode << 4)];

	if (rvv)
		gfx ^= 0xff;

	if (m_bow)
		gfx ^= 0xff;

	for(u8 i=0; i<8; i++)
		bitmap.pix32(y, x + i) = palette[BIT(gfx, 7-i) ? (hlgt ? 2 : 1) : 0];
}


MACHINE_CONFIG_START(trs80dt1_state::trs80dt1)
	/* basic machine hardware */
	MCFG_DEVICE_ADD("maincpu", I8051, 7372800)
	MCFG_DEVICE_PROGRAM_MAP(prg_map)
	MCFG_DEVICE_IO_MAP(io_map)
	MCFG_MCS51_PORT_P1_OUT_CB(WRITE8(*this, trs80dt1_state, port1_w))
	MCFG_MCS51_PORT_P3_OUT_CB(WRITE8(*this, trs80dt1_state, port3_w))

	/* video hardware */
	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_UPDATE_DEVICE("crtc", i8275_device, screen_update)
	MCFG_SCREEN_REFRESH_RATE(60)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(2500)) /* not accurate */
	MCFG_SCREEN_SIZE(40*12, 16*16)
	MCFG_SCREEN_VISIBLE_AREA(0, 40*12-1, 0, 16*16-1)
	MCFG_DEVICE_ADD("gfxdecode", GFXDECODE, "palette", gfx_trs80dt1 )

	MCFG_DEVICE_ADD("crtc", I8275, 12480000 / 8)
	MCFG_I8275_CHARACTER_WIDTH(8)
	MCFG_I8275_DRAW_CHARACTER_CALLBACK_OWNER(trs80dt1_state, crtc_update_row)
	MCFG_I8275_DRQ_CALLBACK(INPUTLINE("maincpu", MCS51_INT0_LINE)) // BRDY pin goes through inverter to /INT0, so we don't invert
	MCFG_I8275_IRQ_CALLBACK(WRITELINE("7474", ttl7474_device, clear_w)) // INT pin
	MCFG_DEVCB_CHAIN_OUTPUT(WRITELINE("7474", ttl7474_device, d_w))
	MCFG_I8275_VRTC_CALLBACK(WRITELINE("7474", ttl7474_device, clock_w))
	MCFG_VIDEO_SET_SCREEN("screen")
	MCFG_PALETTE_ADD("palette", 3)

	MCFG_X2210_ADD("nvram")

	MCFG_DEVICE_ADD("7474", TTL7474, 0)
	MCFG_7474_COMP_OUTPUT_CB(INPUTLINE("maincpu", MCS51_INT1_LINE)) MCFG_DEVCB_INVERT // /Q connects directly to /INT1, so we need to invert?

	/* sound hardware */
	SPEAKER(config, "mono").front_center();
	MCFG_DEVICE_ADD("beeper", BEEP, 2000)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.50)
MACHINE_CONFIG_END

ROM_START( trs80dt1 )

	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "trs80dt1.u12", 0x0000, 0x1000, CRC(04e8a53f) SHA1(7b5d5047319ef8f230b82684d97a918b564d466e) )

	ROM_REGION( 0x0800, "chargen", 0 )
	ROM_LOAD( "8045716.u8",   0x0000, 0x0800, CRC(e2c5e59b) SHA1(0d571888d5f9fea4e565486ea8d3af8998ca46b1) )
ROM_END

COMP( 1989, trs80dt1, 0, 0, trs80dt1, trs80dt1, trs80dt1_state, empty_init, "Radio Shack", "TRS-80 DT-1", MACHINE_NOT_WORKING )
