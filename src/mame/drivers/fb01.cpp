// license:BSD-3-Clause
// copyright-holders:Wilbert Pol
/***************************************************************************

  Yamaha FB-01

***************************************************************************/

#include "emu.h"

#include "bus/midi/midi.h"
#include "cpu/z80/z80.h"
#include "machine/clock.h"
#include "machine/i8251.h"
#include "machine/nvram.h"
#include "sound/ym2151.h"
#include "video/hd44780.h"

#include "emupal.h"
#include "rendlay.h"
#include "screen.h"
#include "speaker.h"

#include "fb01.lh"


class fb01_state : public driver_device
{
public:
	fb01_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_upd71051(*this, "upd71051")
		, m_midi_thru(*this, "mdthru")
		, m_ym2164_irq(CLEAR_LINE)
		, m_upd71051_txrdy(CLEAR_LINE)
		, m_upd71051_rxrdy(CLEAR_LINE)
	{
	}

	void fb01(machine_config &config);

private:
	DECLARE_WRITE_LINE_MEMBER(write_usart_clock);
	DECLARE_WRITE_LINE_MEMBER(midi_in);
	DECLARE_WRITE_LINE_MEMBER(ym2164_irq_w);
	DECLARE_WRITE_LINE_MEMBER(upd71051_txrdy_w);
	DECLARE_WRITE_LINE_MEMBER(upd71051_rxrdy_w);

	virtual void machine_start() override;
	virtual void machine_reset() override;

	DECLARE_PALETTE_INIT(fb01);
	HD44780_PIXEL_UPDATE(fb01_pixel_update);

	void fb01_io(address_map &map);
	void fb01_mem(address_map &map);

	required_device<z80_device> m_maincpu;
	required_device<i8251_device> m_upd71051;
	required_device<midi_port_device> m_midi_thru;
	int m_ym2164_irq;
	int m_upd71051_txrdy;
	int m_upd71051_rxrdy;

	void update_int();
};


void fb01_state::fb01_mem(address_map &map)
{
	map(0x0000, 0x7fff).rom();
	map(0x8000, 0xbfff).ram().share("nvram");  // 2 * 8KB S-RAM
}


void fb01_state::fb01_io(address_map &map)
{
	map.unmap_value_high();
	map.global_mask(0xff);
	// 00-01  YM2164
	map(0x00, 0x00).w("ym2164", FUNC(ym2151_device::register_w));
	map(0x01, 0x01).rw("ym2164", FUNC(ym2151_device::status_r), FUNC(ym2151_device::data_w));

	// 10-11  USART uPD71051C  4MHz & 4MHz / 8
	map(0x10, 0x10).rw(m_upd71051, FUNC(i8251_device::data_r), FUNC(i8251_device::data_w));
	map(0x11, 0x11).rw(m_upd71051, FUNC(i8251_device::status_r), FUNC(i8251_device::control_w));

	// 20     PANEL SWITCH
	map(0x20, 0x20).portr("PANEL");

	// 30-31  HD44780A
	map(0x30, 0x30).rw("hd44780", FUNC(hd44780_device::control_read), FUNC(hd44780_device::control_write));
	map(0x31, 0x31).rw("hd44780", FUNC(hd44780_device::data_read), FUNC(hd44780_device::data_write));
}


static INPUT_PORTS_START( fb01 )
	PORT_START("PANEL")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_OTHER) PORT_NAME("System Set Up")
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_OTHER) PORT_NAME("Inst Select")
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_OTHER) PORT_NAME("Inst Assign")
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_OTHER) PORT_NAME("Inst Function")
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_OTHER) PORT_NAME("Voice Function")
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_OTHER) PORT_NAME("Voice Select")
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_OTHER) PORT_NAME("-1/No")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_OTHER) PORT_NAME("+1/Yes")
INPUT_PORTS_END


void fb01_state::machine_start()
{
	save_item(NAME(m_ym2164_irq));
	save_item(NAME(m_upd71051_txrdy));
	save_item(NAME(m_upd71051_rxrdy));
}


void fb01_state::machine_reset()
{
	m_upd71051->write_cts(0);
	m_upd71051->write_rxd(ASSERT_LINE);
}


WRITE_LINE_MEMBER(fb01_state::write_usart_clock)
{
	m_upd71051->write_txc(state);
	m_upd71051->write_rxc(state);
}


WRITE_LINE_MEMBER(fb01_state::midi_in)
{
	m_midi_thru->write_txd(state);
	m_upd71051->write_rxd(state);
}


WRITE_LINE_MEMBER(fb01_state::ym2164_irq_w)
{
	m_ym2164_irq = state;
	update_int();
}


WRITE_LINE_MEMBER(fb01_state::upd71051_txrdy_w)
{
	m_upd71051_txrdy = state;
	update_int();
}


WRITE_LINE_MEMBER(fb01_state::upd71051_rxrdy_w)
{
	m_upd71051_rxrdy = state;
	update_int();
}


void fb01_state::update_int()
{
	m_maincpu->set_input_line(0, (m_ym2164_irq || m_upd71051_txrdy || m_upd71051_rxrdy) ? ASSERT_LINE : CLEAR_LINE);
}


HD44780_PIXEL_UPDATE(fb01_state::fb01_pixel_update)
{
	if ( pos < 8 && line < 2 )
	{
		bitmap.pix16(y, line*6*8 + pos*6 + x) = state;
	}
}


PALETTE_INIT_MEMBER(fb01_state, fb01)
{
	palette.set_pen_color(0, rgb_t(30, 0, 0));
	palette.set_pen_color(1, rgb_t(150, 0, 0));
}


MACHINE_CONFIG_START(fb01_state::fb01)
	/* basic machine hardware */
	MCFG_DEVICE_ADD("maincpu", Z80, XTAL(12'000'000)/2)
	MCFG_DEVICE_PROGRAM_MAP(fb01_mem)
	MCFG_DEVICE_IO_MAP(fb01_io)

	/* video hardware */
	MCFG_SCREEN_ADD("screen", LCD)
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(2500)) /* not accurate */
	MCFG_SCREEN_SIZE(6*16, 9)
	MCFG_SCREEN_VISIBLE_AREA(0, 6*16-1, 0, 9-1)
	MCFG_DEFAULT_LAYOUT(layout_lcd)
	MCFG_SCREEN_UPDATE_DEVICE("hd44780", hd44780_device, screen_update)
	MCFG_SCREEN_PALETTE("palette")

	MCFG_DEFAULT_LAYOUT( layout_fb01 )

	MCFG_PALETTE_ADD("palette", 2)
	MCFG_PALETTE_INIT_OWNER(fb01_state, fb01)

	MCFG_HD44780_ADD("hd44780")
	MCFG_HD44780_LCD_SIZE(2, 8)   // 2x8 displayed as 1x16
	MCFG_HD44780_PIXEL_UPDATE_CB(fb01_state,fb01_pixel_update)

	MCFG_DEVICE_ADD("upd71051", I8251, XTAL(4'000'000))
	MCFG_I8251_RXRDY_HANDLER(WRITELINE(*this, fb01_state, upd71051_rxrdy_w))
	MCFG_I8251_TXRDY_HANDLER(WRITELINE(*this, fb01_state, upd71051_txrdy_w))
	MCFG_I8251_TXD_HANDLER(WRITELINE("mdout", midi_port_device, write_txd))

	MCFG_DEVICE_ADD("usart_clock", CLOCK, XTAL(4'000'000) / 8) // 500KHz
	MCFG_CLOCK_SIGNAL_HANDLER(WRITELINE(*this, fb01_state, write_usart_clock))

	MCFG_MIDI_PORT_ADD("mdin", midiin_slot, "midiin")
	MCFG_MIDI_RX_HANDLER(WRITELINE(*this, fb01_state, midi_in))

	MCFG_MIDI_PORT_ADD("mdout", midiout_slot, "midiout")

	MCFG_MIDI_PORT_ADD("mdthru", midiout_slot, "midiout")

	SPEAKER(config, "lspeaker").front_left();
	SPEAKER(config, "rspeaker").front_right();
	MCFG_DEVICE_ADD("ym2164", YM2151, XTAL(4'000'000))
	MCFG_YM2151_IRQ_HANDLER(WRITELINE(*this, fb01_state, ym2164_irq_w))
	MCFG_SOUND_ROUTE(0, "lspeaker", 1.00)
	MCFG_SOUND_ROUTE(1, "rspeaker", 1.00)

	MCFG_NVRAM_ADD_0FILL("nvram")
MACHINE_CONFIG_END


/* ROM definition */
ROM_START( fb01 )
	ROM_REGION( 0x8000, "maincpu", 0 )
	ROM_LOAD("nec__-011_xb712c0__8709ex700__d27c256c-15.ic11", 0, 0x8000, CRC(7357e9a4) SHA1(049c482d6c91b7e2846757dd0f5138e0d8b687f0)) // OTP 27c256 windowless eprom?
ROM_END


/* Driver */

//    YEAR  NAME  PARENT  COMPAT  MACHINE  INPUT  STATE       INIT        COMPANY   FULLNAME  FLAGS
CONS( 1986, fb01, 0,      0,      fb01,    fb01,  fb01_state, empty_init, "Yamaha", "FB-01",  MACHINE_SUPPORTS_SAVE )
