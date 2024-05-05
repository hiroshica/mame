// license:BSD-3-Clause
// copyright-holders:Olivier Galibert

#include "emu.h"

#include "bus/midi/midiinport.h"
#include "bus/midi/midioutport.h"
#include "cpu/sh/sh7042.h"
#include "sound/swx00.h"
#include "video/hd44780.h"

#include "debugger.h"
#include "screen.h"
#include "speaker.h"

class psr540_state : public driver_device {
public:
	psr540_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag),
		  m_maincpu(*this, "maincpu"),
		  m_swx00(*this, "swx00"),
		  m_lcdc(*this, "ks0066"),
		  m_outputs(*this, "%02d.%x.%x", 0U, 0U, 0U),
		  m_sustain(*this, "SUSTAIN"),
		  m_pitch_bend(*this, "PITCH_BEND")
	{ }

	void psr540(machine_config &config);

private:
	required_device<sh7042_device> m_maincpu;
	required_device<swx00_sound_device> m_swx00;
	required_device<hd44780_device> m_lcdc;
	output_finder<80, 8, 5> m_outputs;
	required_ioport m_sustain;
	required_ioport m_pitch_bend;

	u16 m_pe, m_led, m_scan;

	void map(address_map &map);

	void machine_start() override;

	u16 adc_sustain_r();
	u16 adc_midisw_r();
	u16 adc_battery_r();

	u16 pe_r();
	void pe_w(u16 data);
	u8 pf_r();

	void lcd_data_w(u8 data);
	void led_data_w(offs_t, u16 data, u16 mem_mask);
	void render_w(int state);
};

void psr540_state::render_w(int state)
{
	if(!state)
		return;

	const u8 *render = m_lcdc->render();
	if(1)
	for(int yy=0; yy != 8; yy++)
		for(int x=0; x != 80; x++) {
			uint8_t v = render[16*x + yy];
			for(int xx=0; xx != 5; xx++)
				m_outputs[x][yy][xx] = (v >> xx) & 1;
		}

	if(1)
	for(int x1=0; x1 != 4; x1++)
		for(int yy=0; yy != 8; yy++) {
			std::string s;
			for(int x=0; x != 10; x++) {
				uint8_t v = render[16*(x+10*x1) + yy];
				for(int xx=0; xx != 5; xx++)
					s += ((v >> (4-xx)) & 1) ? '#' : '.';
			}
			logerror("XX %02d.%d %s\n", x1*10, yy, s);
		}
}

void psr540_state::machine_start()
{
	m_outputs.resolve();
 
	save_item(NAME(m_pe));
	save_item(NAME(m_led));
	save_item(NAME(m_scan));
	m_pe = 0;
	m_led = 0;
	m_scan = 0;
}

u16 psr540_state::adc_sustain_r()
{
	return m_sustain->read() ? 0x3ff : 0;
}

u16 psr540_state::adc_midisw_r()
{
	return 0;
}

u16 psr540_state::adc_battery_r()
{
	return 0x3ff;
}

void psr540_state::psr540(machine_config &config)
{
	SH7042A(config, m_maincpu, 7_MHz_XTAL*4); // // md=a, on-chip rom, 32-bit space, pll 4x -- XW25610 6437042F14F 9M1 A
	m_maincpu->set_addrmap(AS_PROGRAM, &psr540_state::map);
	m_maincpu->read_adc<0>().set(FUNC(psr540_state::adc_midisw_r));
	m_maincpu->read_adc<1>().set(FUNC(psr540_state::adc_sustain_r));
	m_maincpu->read_adc<2>().set(FUNC(psr540_state::adc_battery_r));
	m_maincpu->read_adc<3>().set_constant(0);
	m_maincpu->read_adc<4>().set_ioport(m_pitch_bend);
	m_maincpu->read_porte().set(FUNC(psr540_state::pe_r));
	m_maincpu->write_porte().set(FUNC(psr540_state::pe_w));
	m_maincpu->read_portf().set(FUNC(psr540_state::pf_r));

	SWX00_SOUND(config, m_swx00);
	m_swx00->add_route(0, "lspeaker", 1.0);
	m_swx00->add_route(1, "rspeaker", 1.0);

	KS0066(config, m_lcdc, 270000); // OSC = 91K resistor, TODO: actually KS0066U-10B
	m_lcdc->set_default_bios_tag("f05");
	m_lcdc->set_lcd_size(2, 40);

	/* video hardware */
	auto &screen = SCREEN(config, "screen", SCREEN_TYPE_SVG);
	screen.set_refresh_hz(60);
	screen.set_size(1080, 360);
	screen.set_visarea_full();
	screen.screen_vblank().set(FUNC(psr540_state::render_w));

	SPEAKER(config, "lspeaker").front_left();
	SPEAKER(config, "rspeaker").front_right();
}

u16 psr540_state::pe_r()
{
	return 0xffff;
}

u8 psr540_state::pf_r()
{
	return 0xff;
}

void psr540_state::pe_w(u16 data)
{
	m_pe = data;
	logerror("pe lcd_rs=%x lcd_en=%x ldcic=%d fdcic=%d\n", BIT(m_pe, 11), BIT(m_pe, 9), BIT(m_pe, 4), BIT(m_pe, 3));
	m_lcdc->rs_w(BIT(m_pe, 11));
	m_lcdc->e_w(BIT(m_pe, 9));

	if(BIT(m_pe, 4))
		m_scan = m_led & 7;
}

void psr540_state::lcd_data_w(u8 data)
{
	m_lcdc->db_w(data);
}

void psr540_state::led_data_w(offs_t, u16 data, u16 mem_mask)
{
	COMBINE_DATA(&m_led);
	if(BIT(m_pe, 4))
		m_scan = m_led & 7;
}

void psr540_state::map(address_map &map)
{
	map(0x00000000, 0x0001ffff).rom().region("kernel", 0).mirror(0x1e0000);  // Internal rom, single-cycle

	// 200000-3fffff: cs0 space, 8bits, 1 cycle between accesses, cs assert extension, 6 wait states
	// 200000 fdc
	map(0x00200000, 0x00200000).lr8(NAME([]() -> u8 { return 0x80; }));
	// 280000 sram (battery-backed)
	map(0x00280000, 0x0029ffff).ram();
	// 2c0000 leds/scanning
	map(0x002c0000, 0x002c0001).w(FUNC(psr540_state::led_data_w));
	// 300000 lcd
	map(0x00300000, 0x00300000).w(FUNC(psr540_state::lcd_data_w));

	// 400000-7fffff: cs1 space, 16 bits, 2 wait states
	map(0x00400000, 0x007fffff).rom().region("program_rom", 0);

	// c00000-ffffff: cs3 space, 8 bits, cs assert extension, 3 wait states
	map(0x00c00000, 0x00c00fff).m(m_swx00, FUNC(swx00_sound_device::map));

	// Dedicated dram space, ras precharge = 1.5, ras-cas delay 2, cas-before-ras 2.5, dram write 4, read 3, idle 0, burst, ras down, 16bits, 9-bit address
	// Automatic refresh every 436 cycles, cas-before-ras
	map(0x01000000, 0x0107ffff).ram(); // dram
}

static INPUT_PORTS_START( psr540 )
	PORT_START("SUSTAIN")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_OTHER) PORT_NAME("Sustain Pedal")

	PORT_START("PITCH_BEND")
	PORT_BIT(0x3ff, 0x200, IPT_PADDLE) PORT_NAME("Pitch Bend") PORT_SENSITIVITY(30)

INPUT_PORTS_END

ROM_START( psr540 )
	// Hitachi HI-7 RTOS, version 1.6.  Internal to the sh-2
	ROM_REGION32_BE( 0x400000, "kernel", 0 )
	ROM_LOAD( "hi_7_v1_6.bin", 0, 0x20000, CRC(1f1992d9) SHA1(4f037cc5d7928ace240e31c65dfdff7ce91bd33a))

	ROM_REGION32_BE( 0x400000, "program_rom", 0 )
	ROM_LOAD16_WORD_SWAP( "xw25320.ic310", 0, 0x400000, CRC(e8d29e49) SHA1(079e0ccf6cf5d5bd2d2d82076b09dd702fcd1421))

	ROM_REGION16_LE( 0x600000, "swx00", 0)
	ROM_LOAD16_WORD_SWAP( "xw25410.ic210", 0, 0x400000, CRC(c7c4736d) SHA1(ff1052eb076557071ed8652e6c2fc0925144fbd5))
	ROM_LOAD16_WORD_SWAP( "xw25520.ic220", 0x400000, 0x200000, CRC(9ef56c4e) SHA1(f26b588f9bcfd7bdbf1c0b38e4a1ea57e2f29f10))

	// Warning: will change, only the grid is mapped at this point
	ROM_REGION(800000, "screen", ROMREGION_ERASE00)
	ROM_LOAD("psr540-lcd.svg", 0, 0x95f72, CRC(15adfc4d) SHA1(92c5bb43ef253bb33ec2b3a77c11d23db8322dc1))
ROM_END

SYST( 1999, psr540, 0, 0, psr540, psr540, psr540_state, empty_init, "Yamaha", "PSR540", MACHINE_IS_SKELETON )
