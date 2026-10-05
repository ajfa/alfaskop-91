// license:BSD-3-Clause
// copyright-holders:Mattis Lind
/*
	Ericsson Alfaskop 91 controller.

	Preliminary memory map reconstructed from E34058909112212302 boot ROM.
	Main board E340587091/000-2: MC68000G8, HD68450-8, MK68564N-4A,
	MC68230P8, WD2797A-PL, TMS4500A, 32 x 4164 (256 KiB).
	TCC board E340587093: MC6800P, MC6840P, MC6844P, 4 x MC6854P,
	6264 SRAM, including the 16 KiB shared store. No EPROM on the TCC board.

	a91du adds an Alfaskop System 41 DU 4110 display unit on one of the
	four TCC two-wire lines (selected in the machine configuration).
	Line 1 (MK68564 channel A and the X.21 adapter at FF8C00) is connected
	to a model of an X.21 network and an SNA host with one 3270 session.
	With -bitb socket.<address>:<port> the SDLC frames go instead to a line
	of the IBM 3705 emulator (IBM3705_R5), so a real NCP, VTAM and TSO under
	Hercules are the host; the X.21 call and the first XID stay modelled.

	TODO:
	- Verify clocks/dividers, PI/T port wiring, DMA arbitration and IRQ gating.
	- Verify TCC vector/host control latch decode and interrupt handshakes.
	- Trace the X.21 adapter; its register bits are inferred from PHYSLIB.
	- Decode the optional expansion boards probed by the ROM.

	TCC memory/register addresses and reset release are reconstructed from
	the TCC91OS and FD91BOOT files loaded by the boot ROM.
*/

#include "emu.h"
#include "cpu/m6800/m6800.h"
#include "cpu/m68000/m68000.h"
#include "machine/68230pit.h"
#include "machine/6821pia.h"
#include "machine/6840ptm.h"
#include "machine/6850acia.h"
#include "machine/hd63450.h"
#include "machine/input_merger.h"
#include "machine/mc6844.h"
#include "machine/mc6854.h"
#include "machine/sdlc.h"
#include "machine/wd_fdc.h"
#include "machine/z80sio.h"
#include "video/mc6845.h"
#include "bus/rs232/rs232.h"
#include "imagedev/bitbngr.h"
#include "imagedev/floppy.h"
#include "../skeleton/alfaskop_s41_kb.h"
#include "screen.h"

#include "a91du.lh"

#include <deque>

#define LOG_SS3 (1U << 1)
#define LOG_DUIRQ (1U << 2)
#define LOG_X21 (1U << 3)

#define VERBOSE (LOG_SS3 | LOG_X21)
#include "logmacro.h"

#define LOGSS3(...)   LOGMASKED(LOG_SS3,   __VA_ARGS__)
#define LOGDUIRQ(...) LOGMASKED(LOG_DUIRQ, __VA_ARGS__)
#define LOGX21(...)   LOGMASKED(LOG_X21,   __VA_ARGS__)

namespace {

static constexpr u8 NO_IRQ = 0xff;

class a91_state : public driver_device
{
public:
	a91_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_tcccpu(*this, "tcccpu")
		, m_dmac(*this, "dmac")
		, m_pit(*this, "pit")
		, m_fdc(*this, "fdc")
		, m_floppy(*this, "fdc:%u", 0U)
		, m_ram(*this, "ram")
		, m_rom(*this, "bootrom")
		, m_tccram(*this, "tccram")
		, m_tccdma(*this, "tccdma")
		, m_tccadlc(*this, "adlc%u", 0U)
		, m_digits(*this, "digit%u", 0U)
		, m_ducpu(*this, "ducpu")
		, m_du_vram(*this, "du_vram")
		, m_du_kbd_acia(*this, "du_kbd_acia")
		, m_du_kbd(*this, "du_kbd")
		, m_du_mic_pia(*this, "du_mic_pia")
		, m_du_dia_pia(*this, "du_dia_pia")
		, m_du_crtc(*this, "du_crtc")
		, m_du_screen(*this, "du_screen")
		, m_du_chargen(*this, "duchargen")
		, m_du_rom(*this, "duroms")
		, m_du_adlc(*this, "du_adlc")
		, m_du_dma(*this, "du_dma")
		, m_config(*this, "CONFIG")
		, m_sio(*this, "sio")
		, m_host_link(*this, "host3705")
	{ }

	void a91(machine_config &config);
	void a91du(machine_config &config);

protected:
	virtual void machine_start() override ATTR_COLD;
	virtual void machine_reset() override ATTR_COLD;

private:
	void main_map(address_map &map) ATTR_COLD;
	void tcc_map(address_map &map) ATTR_COLD;
	void du_mem_map(address_map &map) ATTR_COLD;
	u16 vectors_r(offs_t offset);
	void vectors_w(offs_t offset, u16 data, u16 mem_mask);
	u8 pit_r(offs_t offset);
	void pit_w(offs_t offset, u8 data);
	void floppy_control_w(u8 data);
	void leds_w(u8 data);
	void media_changed(floppy_image_device *floppy);
	void check_media();
	void fdc_irq_w(int state);
	void dma_irq_w(int state);
	void timer_irq_w(int state);
	void update_irq();
	void cpu_reset_w(int state);
	u8 tcc_control_r(offs_t offset);
	void tcc_control_w(offs_t offset, u8 data);
	u8 tcc_shared_r(offs_t offset);
	void tcc_shared_w(offs_t offset, u8 data);
	static void floppy_drives(device_slot_interface &device);

	template <unsigned CH> void tcc_wire_channel(machine_config &config);
	template <unsigned CH> void tcc_frame_out(u8 *data, int length);
	void du_frame_out(u8 *data, int length);
	TIMER_CALLBACK_MEMBER(ss3_to_du);
	TIMER_CALLBACK_MEMBER(ss3_to_tcc);
	TIMER_CALLBACK_MEMBER(du_carrier_tail);
	TIMER_CALLBACK_MEMBER(du_tick);
	TIMER_CALLBACK_MEMBER(du_acia_clk);

	template <unsigned N> void du_irq_w(int state);
	u8 du_pending_level() const;
	void du_update_irq();
	void du_set_imsk(u8 level);
	MC6845_UPDATE_ROW(du_crtc_update_row);

	required_device<m68000_device> m_maincpu;
	required_device<m6800_cpu_device> m_tcccpu;
	required_device<hd63450_device> m_dmac;
	required_device<pit68230_device> m_pit;
	required_device<wd2797_device> m_fdc;
	required_device_array<floppy_connector, 2> m_floppy;
	required_shared_ptr<u16> m_ram;
	required_region_ptr<u16> m_rom;
	required_shared_ptr<u16> m_tccram;
	required_device<mc6844_device> m_tccdma;
	required_device_array<mc6854_device, 4> m_tccadlc;
	output_finder<2> m_digits;

	optional_device<m6800_cpu_device> m_ducpu;
	optional_shared_ptr<u8> m_du_vram;
	optional_device<acia6850_device> m_du_kbd_acia;
	optional_device<alfaskop_s41_keyboard_device> m_du_kbd;
	optional_device<pia6821_device> m_du_mic_pia;
	optional_device<pia6821_device> m_du_dia_pia;
	optional_device<mc6845_device> m_du_crtc;
	optional_device<screen_device> m_du_screen;
	optional_region_ptr<u8> m_du_chargen;
	optional_region_ptr<u8> m_du_rom;
	optional_device<mc6854_device> m_du_adlc;
	optional_device<mc6844_device> m_du_dma;
	optional_ioport m_config;

	bool m_boot_vectors = true;
	bool m_fdc_irq = false;
	bool m_dma_irq = false;
	bool m_timer_irq = false;
	bool m_soft_irq = false;
	u8 m_pc_in = 0xff;
	u8 m_pgcr = 0;
	u8 m_floppy_control = 0;
	u8 m_media_pending = 0;
	u8 m_tcc_control[16]{};

	bool m_tcc_rdsr[4]{};
	bool m_tcc_tdsr[4]{};
	bool m_tcc_rx_tail[4]{};

	unsigned m_du_line = 0;
	std::deque<std::vector<u8>> m_q_to_du, m_q_to_tcc;
	emu_timer *m_ss3_to_du_timer = nullptr;
	emu_timer *m_ss3_to_tcc_timer = nullptr;
	emu_timer *m_du_dcd_timer = nullptr;
	emu_timer *m_du_tick_timer = nullptr;
	emu_timer *m_du_acia_clk_timer = nullptr;
	bool m_du_tick = false;
	bool m_du_acia_clk_on = false;
	bool m_du_acia_clk_level = false;
	bool m_du_kbd_line_kbd = true;
	bool m_du_kbd_line_acia = true;
	bool m_du_rx_tail = false;
	u8 m_du_irq = 0;
	u8 m_du_imsk = 0;

	// host line: X.21 adapter at FF8C00, MK68564 channel A, and a model of the DCE, network and host
	enum : u8 { R_ONE, R_ZERO, R_ALT, R_CHARS, R_HDLC };
	enum : u8 { DCE_READY, DCE_PROCEED, DCE_SELECTED, DCE_CONNECTING, DCE_DATA, DCE_CLEARING, DCE_INCOMING };
	u8 x21_r(offs_t offset);
	void x21_w(offs_t offset, u8 data);
	u8 sio_r(offs_t offset);
	void sio_w(offs_t offset, u8 data);
	void x21_dte_changed();
	void x21_set_dce(u8 state, u8 r, bool i);
	void x21_report();
	u8 x21_next_rbit();
	void x21_tbit(bool bit);
	void x21_send_frame(std::vector<u8> const &frame);
	void x21_frame_in(std::vector<u8> const &frame);
	TIMER_CALLBACK_MEMBER(x21_clock);
	TIMER_CALLBACK_MEMBER(x21_dce_step);
	TIMER_CALLBACK_MEMBER(x21_report_cb);

	required_device<mk68564_device> m_sio;
	emu_timer *m_x21_clk_timer = nullptr;
	emu_timer *m_x21_dce_timer = nullptr;
	emu_timer *m_x21_report_timer = nullptr;
	u8 m_x21_ctl1 = 0;
	u8 m_x21_ctl3[2]{};
	u8 m_x21_stat1 = 0;
	u8 m_x21_stat3 = 0;
	bool m_x21_irq = false;
	bool m_x21_clk = false;
	int m_x21_txd = 1;
	u8 m_dce = DCE_READY;
	u8 m_x21_rmode = R_ONE;
	bool m_x21_i = false;
	bool m_x21_alt = false;
	std::vector<u8> m_x21_rchars;
	unsigned m_x21_rpos = 0;
	std::string m_x21_dialled;
	// R line bit queue (call control characters or HDLC), T line HDLC deframer
	std::deque<u8> m_x21_rbits; // 0/1 bits, 2 = end of a frame
	unsigned m_x21_ones_out = 0;
	u16 m_x21_tsr = 0;
	unsigned m_x21_ones_in = 0;
	bool m_x21_in_frame = false;
	std::vector<u8> m_x21_tframe;
	u8 m_x21_tbyte = 0;
	unsigned m_x21_tbitn = 0;
	std::deque<std::vector<u8>> m_x21_txq;

	// host: SDLC primary for station C1, SSCP and one application
	enum : u8 { SDLC_DISC, SDLC_XID, SDLC_XID_DONE, SDLC_SNRM, SDLC_NRM };
	TIMER_CALLBACK_MEMBER(host_poll);
	void host_sdlc_send_i(std::vector<u8> const &piu);
	void host_piu_in(std::vector<u8> const &piu);
	void host_link_up();
	void host_send(u8 daf, u8 oaf, u8 rh0, u8 rh1, u8 rh2, std::vector<u8> const &ru);
	void host_screen(u8 lu, std::string const &line1, std::vector<u8> const &line2);
	TIMER_CALLBACK_MEMBER(host_bind);
	emu_timer *m_host_bind_timer = nullptr;
	u16 m_host_snf[256][2][2]{}; // destination, origin SSCP/PLU, expedited/normal flow
	u8 m_host_lu = 0;
	bool m_host_in_bracket = false;
	bool m_host_bound = false;
	emu_timer *m_host_timer = nullptr;
	u8 m_host_addr = 0xc1;
	u8 m_sdlc = SDLC_DISC;
	u8 m_host_vs = 0;
	u8 m_host_vr = 0;
	u8 m_host_va = 0;
	bool m_host_wait_final = false;
	attotime m_host_poll_time;
	std::deque<std::vector<u8>> m_host_out;
	std::deque<std::vector<u8>> m_host_unacked;

	// bridge to the line socket of an IBM 3705 emulator (IBM3705_R5) instead of the host model:
	// frames travel as 7E address control data 47 0F 7E; the call setup and the first XID stay here
	optional_device<bitbanger_device> m_host_link;
	TIMER_CALLBACK_MEMBER(bridge_poll);
	void bridge_from_a91(std::vector<u8> const &frame);
	emu_timer *m_bridge_timer = nullptr;
	bool m_bridge = false;
	bool m_bridge_xid_done = false;
	std::vector<u8> m_bridge_rx;
};

void a91_state::machine_start()
{
	m_digits.resolve();
	save_item(NAME(m_boot_vectors));
	save_item(NAME(m_fdc_irq));
	save_item(NAME(m_pgcr));
	save_item(NAME(m_dma_irq));
	save_item(NAME(m_timer_irq));
	save_item(NAME(m_floppy_control));
	save_item(NAME(m_media_pending));
	save_item(NAME(m_tcc_control));
	for (auto &connector : m_floppy)
		if (connector->get_device())
		{
			connector->get_device()->setup_load_cb(floppy_image_device::load_cb(&a91_state::media_changed, this));
			connector->get_device()->setup_unload_cb(floppy_image_device::unload_cb(&a91_state::media_changed, this));
		}

	m_ss3_to_du_timer = timer_alloc(FUNC(a91_state::ss3_to_du), this);
	m_ss3_to_tcc_timer = timer_alloc(FUNC(a91_state::ss3_to_tcc), this);
	m_du_dcd_timer = timer_alloc(FUNC(a91_state::du_carrier_tail), this);
	m_du_tick_timer = timer_alloc(FUNC(a91_state::du_tick), this);
	m_du_acia_clk_timer = timer_alloc(FUNC(a91_state::du_acia_clk), this);
	m_x21_clk_timer = timer_alloc(FUNC(a91_state::x21_clock), this);
	m_x21_dce_timer = timer_alloc(FUNC(a91_state::x21_dce_step), this);
	m_x21_report_timer = timer_alloc(FUNC(a91_state::x21_report_cb), this);
	m_host_timer = timer_alloc(FUNC(a91_state::host_poll), this);
	m_host_bind_timer = timer_alloc(FUNC(a91_state::host_bind), this);
	m_bridge_timer = timer_alloc(FUNC(a91_state::bridge_poll), this);
	m_x21_clk_timer->adjust(attotime::from_hz(9600 * 2), 0, attotime::from_hz(9600 * 2));
	m_x21_dce_timer->adjust(attotime::from_seconds(50), DCE_READY);
	if (m_ducpu)
	{
		m_du_tick_timer->adjust(attotime::from_msec(10), 0, attotime::from_msec(10));
		if (char const *gap = getenv("A91_KBGAP"))
			m_du_kbd_acia->set_tx_gap(atoi(gap));
	}
}

void a91_state::machine_reset()
{
	m_boot_vectors = true;
	m_bridge = m_host_link && m_host_link->exists();
	m_bridge_xid_done = false;
	m_bridge_rx.clear();
	if (m_bridge)
		m_bridge_timer->adjust(attotime::from_msec(1), 0, attotime::from_msec(1));
	m_pgcr = 0;
	m_fdc_irq = false;
	m_dma_irq = false;
	m_timer_irq = false;
	m_soft_irq = false;
	m_maincpu->set_input_line(M68K_IRQ_1, CLEAR_LINE);
	m_media_pending = 0;
	std::fill(std::begin(m_tcc_control), std::end(m_tcc_control), 0);
	m_tcccpu->set_input_line(INPUT_LINE_RESET, ASSERT_LINE);
	floppy_control_w(0xff);
	update_irq();

	for (int i = 0; i < 4; i++)
	{
		m_tcc_rdsr[i] = m_tcc_tdsr[i] = m_tcc_rx_tail[i] = false;
		m_tccadlc[i]->set_cts(0);
		m_tccadlc[i]->set_dcd(0);
	}
	m_q_to_du.clear();
	m_q_to_tcc.clear();
	m_x21_ctl1 = m_x21_ctl3[0] = m_x21_ctl3[1] = 0;
	m_x21_stat1 = m_x21_stat3 = 0;
	m_x21_irq = false;
	m_x21_rbits.clear();
	m_x21_txq.clear();
	m_x21_in_frame = false;
	m_dce = DCE_READY;
	m_x21_rmode = R_ONE;
	m_x21_i = false;
	m_sio->ctsa_w(0);
	m_sio->dcda_w(0);
	m_du_line = m_config ? (m_config->read() & 3) : 0;

	if (m_ducpu)
	{
		m_du_acia_clk_on = false;
		m_du_acia_clk_timer->adjust(attotime::never);
		m_du_adlc->set_cts(0);
		m_du_adlc->set_dcd(0);
		m_du_irq = 0;
		m_du_imsk = 0;
		m_du_rx_tail = false;
	}
}

u16 a91_state::vectors_r(offs_t offset)
{
	// Only the reset-vector fetches see ROM. The ROM then writes a RAM vector table.
	const u16 result = m_boot_vectors ? m_rom[offset] : m_ram[offset];
	if (offset == 3 && !machine().side_effects_disabled())
		m_boot_vectors = false;
	return result;
}

void a91_state::vectors_w(offs_t offset, u16 data, u16 mem_mask)
{
	COMBINE_DATA(&m_ram[offset]);
}

void a91_state::main_map(address_map &map)
{
	// Missing RAM and optional boards must time out: boot deliberately probes them.
	map(0x000000, 0xffffff).rw(m_maincpu, FUNC(m68000_device::berr_r), FUNC(m68000_device::berr_w));
	// 512 KiB: the EM diskette loads PHYSLIB and X21EMUL between 58000 and 68000
	map(0x000000, 0x07ffff).ram().share("ram");
	map(0x000000, 0x000007).rw(FUNC(a91_state::vectors_r), FUNC(a91_state::vectors_w));
	map(0xc00000, 0xc03fff).rom().region("bootrom", 0);
	map(0xf08000, 0xf0bfff).ram().share("tccram");
	map(0xf0fff0, 0xf0ffff).rw(FUNC(a91_state::tcc_control_r), FUNC(a91_state::tcc_control_w));
	map(0xff8000, 0xff80ff).rw(m_dmac, FUNC(hd63450_device::read), FUNC(hd63450_device::write));
	map(0xff8800, 0xff883f).rw(FUNC(a91_state::pit_r), FUNC(a91_state::pit_w)).umask16(0x00ff);
	map(0xff8a00, 0xff8a3f).rw(FUNC(a91_state::sio_r), FUNC(a91_state::sio_w)).umask16(0x00ff);
	// expansion slots: X.21 adapter of the host line at FF8C00; nothing at FF8E00
	map(0xff8c00, 0xff8c03).rw(FUNC(a91_state::x21_r), FUNC(a91_state::x21_w)).umask16(0x00ff);
	map(0xff8c04, 0xff8dff).noprw();
	map(0xff8e00, 0xff8fff).noprw();
	map(0xff9000, 0xff9007).rw(m_fdc, FUNC(wd2797_device::read), FUNC(wd2797_device::write)).umask16(0x00ff);
}

void a91_state::tcc_map(address_map &map)
{
	map.unmap_value_high();
	// TCC91OS writes 0000-3fff and verifies through the 4000-7fff alias.
	map(0x0000, 0x3fff).mirror(0x4000).ram();
	map(0x8000, 0xbfff).rw(FUNC(a91_state::tcc_shared_r), FUNC(a91_state::tcc_shared_w));
	map(0xc000, 0xc01f).rw(m_tccdma, FUNC(mc6844_device::read), FUNC(mc6844_device::write));
	map(0xc020, 0xc027).rw("tccptm", FUNC(ptm6840_device::read), FUNC(ptm6840_device::write));
	map(0xc040, 0xc043).rw(m_tccadlc[0], FUNC(mc6854_device::read), FUNC(mc6854_device::write));
	map(0xc044, 0xc047).rw(m_tccadlc[1], FUNC(mc6854_device::read), FUNC(mc6854_device::write));
	map(0xc048, 0xc04b).rw(m_tccadlc[2], FUNC(mc6854_device::read), FUNC(mc6854_device::write));
	map(0xc04c, 0xc04f).rw(m_tccadlc[3], FUNC(mc6854_device::read), FUNC(mc6854_device::write));
	map(0xfff0, 0xffff).lrw8(
		NAME([this](offs_t offset) { return m_tcc_control[offset]; }),
		NAME([this](offs_t offset, u8 data) {
			m_tcc_control[offset] = data;
			if (offset == 3)
				subdevice<input_merger_device>("tccirq")->in_w<6>(BIT(data, 4));
			if (offset == 1)
				update_irq();
		}));
}

u8 a91_state::tcc_shared_r(offs_t offset)
{
	return m_tccram[offset >> 1] >> (BIT(offset, 0) ? 0 : 8);
}

void a91_state::tcc_shared_w(offs_t offset, u8 data)
{
	u16 &word = m_tccram[offset >> 1];
	if (BIT(offset, 0))
		word = (word & 0xff00) | data;
	else
		word = (word & 0x00ff) | (u16(data) << 8);
}

u8 a91_state::pit_r(offs_t offset)
{
	u8 data = m_pit->read(offset);
	// The generic PI/T currently stores PSR writes rather than implementing
	// write-one-to-clear handshake status. H1/H2 track disk presence changes,
	// with their sense bits reversed by the ROM after each event.
	// H1/H2 levels are disk present: FD91OS mounts a drive at start only if its level reads 1.
	if (offset == PIT_68230_PSR)
	{
		data = (data & ~0x33) | m_media_pending;
		for (unsigned i = 0; i < 2; ++i)
			if (m_floppy[i]->get_device() && m_floppy[i]->get_device()->exists())
				data |= 0x10 << i;
	}
	if (offset == PIT_68230_PADR)
		data = (data & ~0x80) | (m_fdc_irq ? 0x80 : 0);
	return data;
}

void a91_state::pit_w(offs_t offset, u8 data)
{
	m_pit->write(offset, data);
	if (offset == PIT_68230_PGCR)
	{
		m_pgcr = data;
		check_media();
	}
	if (offset == PIT_68230_PSR)
	{
		m_media_pending &= ~(data & 0x03);
		update_irq();
	}
	// PC7 low requests the deferred level 1 interrupt: the level 2 handlers clear it, the level 1 handler sets it
	if (offset == PIT_68230_PCDR)
	{
		m_soft_irq = !BIT(data, 7);
		m_maincpu->set_input_line(M68K_IRQ_1, m_soft_irq ? ASSERT_LINE : CLEAR_LINE);
	}
}

void a91_state::floppy_control_w(u8 data)
{
	m_floppy_control = data;
	// Inferred from boot drive selection: PA0/PA1 active-low motor enables,
	// PA2/PA3 active-low drive selects; PA4 identifies the selected drive.
	// FD91OS leaves PA2 and PA3 both low and selects with PA4 (0 = drive 1, 1 = drive 2).
	for (unsigned i = 0; i < 2; ++i)
		if (floppy_image_device *const floppy = m_floppy[i]->get_device())
			floppy->mon_w(BIT(data, i));
	bool const s0 = !BIT(data, 2), s1 = !BIT(data, 3);
	int const sel = (s0 && !s1) ? 0 : (s1 && !s0) ? 1 : (s0 && s1) ? BIT(data, 4) : -1;
	m_fdc->set_floppy(sel >= 0 ? m_floppy[sel]->get_device() : nullptr);
}

void a91_state::leds_w(u8 data)
{
	// front panel: two BCD digits, high nibble on the left; F blanks (decoder inferred from the code, not traced)
	static constexpr u8 SEG[10] = { 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f };
	m_digits[0] = (data >> 4) < 10 ? SEG[data >> 4] : 0;
	m_digits[1] = (data & 15) < 10 ? SEG[data & 15] : 0;
}

void a91_state::media_changed(floppy_image_device *floppy)
{
	check_media();
}

void a91_state::check_media()
{
	for (unsigned i = 0; i < 2; ++i)
	{
		floppy_image_device *const floppy = m_floppy[i]->get_device();
		const bool present = floppy && floppy->exists();
		if (present == bool(BIT(m_pgcr, i)))
			m_media_pending |= 1U << i;
	}
	update_irq();
}

void a91_state::fdc_irq_w(int state)
{
	m_fdc_irq = bool(state);
	update_irq();
}

void a91_state::dma_irq_w(int state)
{
	m_dma_irq = bool(state);
	update_irq();
}

void a91_state::timer_irq_w(int state)
{
	m_timer_irq = bool(state);
	update_irq();
}

void a91_state::update_irq()
{
	// Boot installs the common floppy/DMA/timer handler at vector 0x1a (IRQ2).
	// the TCC mailbox at f0fff1 is serviced first by the level 2 handler
	m_maincpu->set_input_line(M68K_IRQ_2, (m_fdc_irq || m_media_pending || m_dma_irq || m_timer_irq || m_tcc_control[1]) ? ASSERT_LINE : CLEAR_LINE);
	m_maincpu->set_input_line(M68K_IRQ_3, m_x21_irq ? ASSERT_LINE : CLEAR_LINE);
}

void a91_state::cpu_reset_w(int state)
{
	if (state)
	{
		// The 68000 RESET instruction resets peripherals, not the CPU/vector overlay.
		m_dmac->reset();
		m_pit->reset();
		m_fdc->reset();
		subdevice<mk68564_device>("sio")->reset();
		m_tcccpu->set_input_line(INPUT_LINE_RESET, ASSERT_LINE);
		m_media_pending = 0;
		update_irq();
	}
}

u8 a91_state::tcc_control_r(offs_t offset)
{
	return m_tcc_control[offset];
}

void a91_state::tcc_control_w(offs_t offset, u8 data)
{
	m_tcc_control[offset] = data;
	// Mailbox interrupt routing inferred from CP91OS/TCC91OS polling and acknowledgement.
	if (offset == 3)
		subdevice<input_merger_device>("tccirq")->in_w<6>(BIT(data, 4));
	if (offset == 1)
		update_irq();
	// FD91BOOT writes the entry pointer to f0fffc, then 0080 to f0fff4.
	// The low word of that pointer supplies the 6800 reset vector at fffe.
	if (offset == 5)
		m_tcccpu->set_input_line(INPUT_LINE_RESET, BIT(data, 7) ? CLEAR_LINE : ASSERT_LINE);
}

void a91_state::floppy_drives(device_slot_interface &device)
{
	device.option_add("525qd", FLOPPY_525_QD);
	device.option_add("35dd", FLOPPY_35_DD);
}

/* ---- SS3 two-wire, frame level, between a TCC line and the display unit ---- */

static std::string hexdump(const u8 *data, int length)
{
	std::string s;
	for (int i = 0; i < length && i < 64; i++)
		s += util::string_format("%02X ", data[i]);
	return s;
}

template <unsigned CH> void a91_state::tcc_frame_out(u8 *data, int length)
{
	LOGSS3("%.6f TCC%u -> %d: %s\n", machine().time().as_double(), CH, length, hexdump(data, length));
	m_tccadlc[CH]->set_cts(1);
	// the four ADLCs share the display pair (TCC91OS polls on one and runs IPL sessions on another)
	if (!m_ducpu)
		return;
	m_q_to_tcc.clear();
	m_ss3_to_tcc_timer->adjust(attotime::never);
	m_du_dcd_timer->adjust(attotime::never);
	m_q_to_du.emplace_back(data, data + length);
	m_ss3_to_du_timer->adjust(attotime::from_nsec(26667));
}

void a91_state::du_frame_out(u8 *data, int length)
{
	LOGSS3("%.6f DU -> %d: %s\n", machine().time().as_double(), length, hexdump(data, length));
	m_du_adlc->set_cts(1);
	m_q_to_du.clear();
	m_ss3_to_du_timer->adjust(attotime::never);
	m_du_dcd_timer->adjust(attotime::from_msec(60));
	m_q_to_tcc.emplace_back(data, data + length);
	m_ss3_to_tcc_timer->adjust(attotime::from_nsec(26667));
}

TIMER_CALLBACK_MEMBER(a91_state::ss3_to_du)
{
	if (m_q_to_du.empty())
		return;
	auto &f = m_q_to_du.front();
	if (m_du_adlc->send_frame(f.data(), f.size()) != 0)
	{
		m_ss3_to_du_timer->adjust(attotime::from_msec(2));
		return;
	}
	m_du_adlc->set_cts(0);
	m_du_dcd_timer->adjust(attotime::never);
	m_q_to_du.pop_front();
	if (!m_q_to_du.empty())
		m_ss3_to_du_timer->adjust(attotime::from_nsec(26667));
}

TIMER_CALLBACK_MEMBER(a91_state::ss3_to_tcc)
{
	if (m_q_to_tcc.empty())
		return;
	auto &f = m_q_to_tcc.front();
	// every armed receiver on the pair hears the frame
	bool delivered = false;
	for (unsigned i = 0; i < 4; i++)
		if (m_tccadlc[i]->send_frame(f.data(), f.size()) == 0)
		{
			m_tccadlc[i]->set_cts(0);
			LOGSS3("%.6f -> TCC%u delivered %d\n", machine().time().as_double(), i, int(f.size()));
			delivered = true;
		}
	if (!delivered)
	{
		m_ss3_to_tcc_timer->adjust(attotime::from_msec(2));
		return;
	}
	m_q_to_tcc.pop_front();
	if (!m_q_to_tcc.empty())
		m_ss3_to_tcc_timer->adjust(attotime::from_nsec(26667));
}

TIMER_CALLBACK_MEMBER(a91_state::du_carrier_tail)
{
	m_du_adlc->set_dcd(1);
	m_du_adlc->set_dcd(0);
}

/* ---- DU 4110, from the alfaskop4120 driver ---- */

void a91_state::du_mem_map(address_map &map)
{
	map.unmap_value_high();
	map(0x0000, 0x7fff).ram();
	map(0x7800, 0x7fff).ram().share(m_du_vram);
	map(0x8000, 0xefff).ram();
	map(0xf600, 0xf6ff).lr8(NAME([]() -> u8 { return 0; }));
	map(0xf700, 0xf71f).rw(m_du_dma, FUNC(mc6844_device::read), FUNC(mc6844_device::write));
	map(0xf720, 0xf727).lrw8(NAME([this](offs_t offset) -> u8 { return m_du_adlc->read(offset & 3); }),
				 NAME([this](offs_t offset, u8 data) { m_du_adlc->write(offset & 3, data); }));
	map(0xf7c0, 0xf7c1).mirror(0x02).lrw8(NAME([this](offs_t offset) -> u8 { return m_du_kbd_acia->read(offset & 1); }),
				 NAME([this](offs_t offset, u8 data)
				 {
					m_du_kbd_acia->write(offset & 1, data);
					// bit clock locked to the keyboard's 3.579545 MHz grid, started on first configuration
					if ((offset & 1) == 0 && (data & 3) != 3 && !m_du_acia_clk_on)
					{
						m_du_acia_clk_on = true;
						attotime const half = attotime::from_ticks(754, 3'579'545 / 4 * 64 * 2);
						m_du_acia_clk_timer->adjust(half, 0, half);
					}
				 }));
	map(0xf7c4, 0xf7c7).rw(m_du_mic_pia, FUNC(pia6821_device::read), FUNC(pia6821_device::write));
	map(0xf7d0, 0xf7d3).mirror(0x04).rw(m_du_dia_pia, FUNC(pia6821_device::read), FUNC(pia6821_device::write));
	map(0xf7d8, 0xf7d8).mirror(0x06).w(m_du_crtc, FUNC(mc6845_device::address_w));
	map(0xf7d9, 0xf7d9).mirror(0x06).rw(m_du_crtc, FUNC(mc6845_device::register_r), FUNC(mc6845_device::register_w));
	map(0xf7fc, 0xf7fc).lr8(NAME([]() -> u8 { return 0x00; }));

	map(0xf800, 0xffe7).rom().region("duroms", 0);
	map(0xffe8, 0xfff7).lrw8(NAME([this](offs_t offset) -> u8
					{
						if (!machine().side_effects_disabled()) du_set_imsk(offset >> 1);
						return m_du_rom[0x7e8 + offset];
					}),
					NAME([this](offs_t offset, u8 data) { du_set_imsk(offset >> 1); }));
	map(0xfff8, 0xfff9).lr8(NAME([this](offs_t offset) -> u8
					{
						u8 const level = du_pending_level();
						offs_t const src = (level == NO_IRQ) ? 0x7f8 : (0x7e8 + (level << 1));
						return m_du_rom[src + offset];
					}));
	map(0xfffa, 0xffff).rom().region("duroms", 0x7fa);
}

u8 a91_state::du_pending_level() const
{
	for (int level = 7; level >= 0; level--)
		if (BIT(m_du_irq, level) && level >= m_du_imsk)
			return u8(level);
	return NO_IRQ;
}

void a91_state::du_update_irq()
{
	m_ducpu->set_input_line(M6800_IRQ_LINE, du_pending_level() != NO_IRQ ? ASSERT_LINE : CLEAR_LINE);
}

void a91_state::du_set_imsk(u8 level)
{
	if (m_du_imsk != (level & 7))
	{
		m_du_imsk = level & 7;
		du_update_irq();
	}
}

template <unsigned N> void a91_state::du_irq_w(int state)
{
	m_du_irq = (m_du_irq & ~(1 << N)) | ((state ? 1 : 0) << N);
	LOGDUIRQ("DU IRQ %d: %d ==> %02x\n", N, state, m_du_irq);
	du_update_irq();
}

TIMER_CALLBACK_MEMBER(a91_state::du_tick)
{
	m_du_tick = !m_du_tick;
	m_du_mic_pia->ca1_w(m_du_tick ? 1 : 0);
}

TIMER_CALLBACK_MEMBER(a91_state::du_acia_clk)
{
	m_du_acia_clk_level = !m_du_acia_clk_level;
	m_du_kbd_acia->write_txc(m_du_acia_clk_level ? 1 : 0);
	m_du_kbd_acia->write_rxc(m_du_acia_clk_level ? 1 : 0);
}

//**************************************************************************
//  HOST LINE: X.21 ADAPTER, DCE AND HOST
//**************************************************************************

// Measured from PHYSLIB/X21.  FF8C01 write: bit 6 routes FF8C03 writes to the line register, bit 1
// enables the adapter interrupt.  Line register: bit 0 = C on, bit 1 = T steady 1, bit 2 = T from
// the MK68564, bit 7 = R/I detector on.  FF8C01 read: bit 7 interrupt, bit 6 event, bit 3 with
// bits 0-2 = R steady 1/0/0101 while I is off; FF8C03 read bit 7 = R steady 1 with I on.

u8 a91_state::x21_r(offs_t offset)
{
	if (offset)
		return m_x21_stat3;
	u8 const data = m_x21_stat1;
	if (!machine().side_effects_disabled() && m_x21_irq)
	{
		m_x21_stat1 &= ~0xc0;
		m_x21_irq = false;
		update_irq();
	}
	return data;
}

void a91_state::x21_w(offs_t offset, u8 data)
{
	LOGX21("%.6f X21 %s %02X\n", machine().time().as_double(), offset ? (BIT(m_x21_ctl1, 6) ? "line" : "aux") : "ctl", data);
	if (!offset)
		m_x21_ctl1 = data;
	else
	{
		m_x21_ctl3[BIT(m_x21_ctl1, 6)] = data;
		if (BIT(m_x21_ctl1, 6))
			x21_dte_changed();
	}
}

u8 a91_state::sio_r(offs_t offset)
{
	return m_sio->read(offset);
}

void a91_state::sio_w(offs_t offset, u8 data)
{
	if (offset == 0x09 && m_dce == DCE_PROCEED)
	{
		char const ch = data & 0x7f;
		if (ch != 0x16)
		{
			m_x21_dialled += ch;
			if (ch == '+')
			{
				LOGX21("%.6f X21 selection %s\n", machine().time().as_double(), m_x21_dialled);
				m_dce = DCE_SELECTED;
				m_x21_dce_timer->adjust(attotime::from_msec(100), DCE_CONNECTING);
			}
		}
	}
	m_sio->write(offset, data);
}

void a91_state::x21_dte_changed()
{
	u8 const v = m_x21_ctl3[1];
	bool const c = BIT(v, 0);
	bool const t_zero = !BIT(v, 2) && !BIT(v, 1);

	if (!c && t_zero && m_dce != DCE_READY && m_dce != DCE_CLEARING)
	{
		x21_set_dce(DCE_CLEARING, R_ZERO, false);
		m_x21_dce_timer->adjust(attotime::from_msec(50), DCE_READY);
	}
	else if (m_dce == DCE_INCOMING && c && !BIT(v, 2) && BIT(v, 1))
	{
		LOGX21("%.6f X21 call accepted\n", machine().time().as_double());
		m_dce = DCE_SELECTED;
		m_x21_dce_timer->adjust(attotime::from_msec(100), DCE_CONNECTING);
	}
	else if ((m_dce == DCE_READY || m_dce == DCE_INCOMING) && c && t_zero)
	{
		m_x21_dialled.clear();
		m_x21_dce_timer->adjust(attotime::from_msec(20), DCE_PROCEED);
	}
	else if (m_dce == DCE_CONNECTING && c && BIT(v, 2))
	{
		x21_set_dce(DCE_DATA, R_HDLC, true);
		LOGX21("%.6f X21 data phase\n", machine().time().as_double());
		m_sdlc = SDLC_DISC;
		m_host_out.clear();
		m_host_unacked.clear();
		m_host_timer->adjust(attotime::from_msec(200));
	}
	if (BIT(v, 7))
		m_x21_report_timer->adjust(attotime::from_hz(9600) * 24);
}

void a91_state::x21_set_dce(u8 state, u8 r, bool i)
{
	LOGX21("%.6f X21 DCE state %u r %u i %u\n", machine().time().as_double(), state, r, i);
	m_dce = state;
	if (r != m_x21_rmode)
		m_x21_rbits.clear();
	m_x21_rmode = r;
	m_x21_i = i;
	if (BIT(m_x21_ctl3[1], 7))
		m_x21_report_timer->adjust(attotime::from_hz(9600) * 24);
}

TIMER_CALLBACK_MEMBER(a91_state::x21_dce_step)
{
	switch (param)
	{
	case DCE_READY:
		x21_set_dce(DCE_READY, R_ONE, false);
		// the host calls in
		m_x21_dce_timer->adjust(attotime::from_seconds(2), DCE_INCOMING);
		break;
	case DCE_INCOMING:
		if (m_dce != DCE_READY)
			break;
		m_x21_rchars = { 0x16, 0x16, 0x07 };
		m_x21_rpos = 0;
		x21_set_dce(DCE_INCOMING, R_CHARS, false);
		break;
	case DCE_PROCEED:
		m_x21_rchars = { 0x16, 0x16, 0xab };
		m_x21_rpos = 0;
		x21_set_dce(DCE_PROCEED, R_CHARS, false);
		break;
	case DCE_CONNECTING:
		// connection in progress, then ready for data
		x21_set_dce(DCE_CONNECTING, R_ONE, false);
		m_x21_report_timer->adjust(attotime::from_msec(200), 1);
		break;
	}
}

TIMER_CALLBACK_MEMBER(a91_state::x21_report_cb)
{
	if (param && m_dce == DCE_CONNECTING)
		m_x21_i = true;
	if (!BIT(m_x21_ctl3[1], 7))
		return;
	u8 stat;
	if (m_x21_rmode == R_ONE)
		stat = m_x21_i ? 0xc0 : 0xc9;
	else if (m_x21_rmode == R_ZERO && !m_x21_i)
		stat = 0xca;
	else if (m_x21_rmode == R_ALT && !m_x21_i)
		stat = 0xcc;
	else
		return;
	m_x21_stat1 = stat;
	m_x21_stat3 = (m_x21_stat3 & 0x10) | ((m_x21_rmode == R_ONE && m_x21_i) ? 0x80 : 0x00);
	LOGX21("%.6f X21 report %02X %02X\n", machine().time().as_double(), m_x21_stat1, m_x21_stat3);
	if (BIT(m_x21_ctl1, 1))
	{
		m_x21_irq = true;
		update_irq();
	}
}

TIMER_CALLBACK_MEMBER(a91_state::x21_clock)
{
	m_x21_clk = !m_x21_clk;
	if (!m_x21_clk)
	{
		// T is sampled at the end of the bit cell, before the MK68564 shifts out the next one
		if (m_dce == DCE_DATA && BIT(m_x21_ctl3[1], 2))
			x21_tbit(m_x21_txd);
		m_sio->rxa_w(x21_next_rbit());
		m_sio->rxca_w(0);
		m_sio->txca_w(0);
	}
	else
	{
		m_sio->rxca_w(1);
		m_sio->txca_w(1);
	}
}

u8 a91_state::x21_next_rbit()
{
	switch (m_x21_rmode)
	{
	case R_ONE:  return 1;
	case R_ZERO: return 0;
	case R_ALT:  m_x21_alt = !m_x21_alt; return m_x21_alt;
	case R_CHARS:
		if (m_x21_rbits.empty())
		{
			u8 const ch = (m_x21_rpos < m_x21_rchars.size()) ? m_x21_rchars[m_x21_rpos++] : m_x21_rchars.back();
			for (int i = 0; i < 8; i++)
				m_x21_rbits.push_back(BIT(ch, i));
		}
		break;
	case R_HDLC:
		if (m_x21_rbits.empty())
		{
			auto raw = [this] (u8 b) { for (int i = 0; i < 8; i++) m_x21_rbits.push_back(BIT(b, i)); };
			raw(0x7e);
			if (!m_x21_txq.empty())
			{
				std::vector<u8> const frame = std::move(m_x21_txq.front());
				m_x21_txq.pop_front();
				unsigned ones = 0;
				auto stuffed = [this, &ones] (bool b)
				{
					m_x21_rbits.push_back(b);
					ones = b ? ones + 1 : 0;
					if (ones == 5)
					{
						m_x21_rbits.push_back(false);
						ones = 0;
					}
				};
				u16 crc = 0xffff;
				for (u8 const b : frame)
					for (int i = 0; i < 8; i++)
					{
						crc = device_sdlc_consumer_interface::update_frame_check(device_sdlc_consumer_interface::POLY_SDLC, crc, BIT(b, i));
						stuffed(BIT(b, i));
					}
				crc = ~crc;
				for (int i = 15; i >= 0; i--)
					stuffed(BIT(crc, i));
				raw(0x7e);
				m_x21_rbits.push_back(2);
			}
		}
		break;
	}
	u8 const bit = m_x21_rbits.front();
	m_x21_rbits.pop_front();
	if (!m_x21_rbits.empty() && m_x21_rbits.front() == 2)
	{
		// inferred: FF8C03 bit 4 toggles at each closing flag; PHYSLIB adds a byte when it reads the same at the first character and at end of frame
		m_x21_stat3 ^= 0x10;
		m_x21_rbits.pop_front();
	}
	return bit;
}

void a91_state::x21_tbit(bool bit)
{
	if (m_x21_ones_in == 5 && !bit)
	{
		m_x21_ones_in = 0;
		return;
	}
	if (bit)
	{
		if (++m_x21_ones_in >= 7)
		{
			m_x21_in_frame = false;
			m_x21_tframe.clear();
			m_x21_tbitn = 0;
			return;
		}
	}
	else if (m_x21_ones_in == 6)
	{
		m_x21_ones_in = 0;
		// the flag's leading zero and six ones went in as data bits
		unsigned const nbits = m_x21_tbitn >= 7 ? m_x21_tbitn - 7 : 0;
		if (m_x21_in_frame && nbits >= 32 && !(nbits & 7))
		{
			std::vector<u8> frame(m_x21_tframe.begin(), m_x21_tframe.begin() + nbits / 8);
			u16 crc = 0xffff;
			for (u8 const b : frame)
				for (int i = 0; i < 8; i++)
					crc = device_sdlc_consumer_interface::update_frame_check(device_sdlc_consumer_interface::POLY_SDLC, crc, BIT(b, i));
			if (crc == 0x1d0f)
			{
				frame.resize(frame.size() - 2);
				x21_frame_in(frame);
			}
			else
				LOGX21("%.6f X21 T frame of %u bytes, bad FCS\n", machine().time().as_double(), unsigned(frame.size()));
		}
		m_x21_in_frame = true;
		m_x21_tframe.clear();
		m_x21_tbitn = 0;
		return;
	}
	else
		m_x21_ones_in = 0;

	if (!m_x21_in_frame)
		return;
	if (!(m_x21_tbitn & 7))
		m_x21_tframe.push_back(0);
	if (bit)
		m_x21_tframe.back() |= 1 << (m_x21_tbitn & 7);
	m_x21_tbitn++;
	if (m_x21_tbitn > 8 * 4096)
		m_x21_in_frame = false;
}

void a91_state::x21_send_frame(std::vector<u8> const &frame)
{
	LOGX21("%.6f HOST -> %d: %s\n", machine().time().as_double(), int(frame.size()), hexdump(frame.data(), frame.size()));
	m_x21_txq.push_back(frame);
}

void a91_state::x21_frame_in(std::vector<u8> const &frame)
{
	LOGX21("%.6f A91 -> %d: %s\n", machine().time().as_double(), int(frame.size()), hexdump(frame.data(), frame.size()));
	if (m_bridge)
	{
		bridge_from_a91(frame);
		return;
	}
	if (frame.size() < 2 || frame[0] != m_host_addr)
		return;
	u8 const ctl = frame[1];
	bool const final = BIT(ctl, 4);
	if (!BIT(ctl, 0) || (ctl & 3) == 1)
	{
		// I or S frame: acknowledge what the A91 has received
		u8 const nr = ctl >> 5;
		while (m_host_va != nr && !m_host_unacked.empty())
		{
			m_host_unacked.pop_front();
			m_host_va = (m_host_va + 1) & 7;
		}
		if (!BIT(ctl, 0) && ((ctl >> 1) & 7) == m_host_vr)
		{
			m_host_vr = (m_host_vr + 1) & 7;
			host_piu_in(std::vector<u8>(frame.begin() + 2, frame.end()));
		}
	}
	else if ((ctl & 0xef) == 0xaf && m_sdlc == SDLC_XID)
	{
		LOGX21("%.6f HOST XID from station: %s\n", machine().time().as_double(), hexdump(frame.data() + 2, frame.size() - 2));
		m_sdlc = SDLC_XID_DONE;
	}
	else if ((ctl & 0xef) == 0x0f)
	{
		// DM: start over with XID
		m_sdlc = SDLC_DISC;
	}
	else if ((ctl & 0xef) == 0x63 && m_sdlc == SDLC_SNRM)
	{
		LOGX21("%.6f HOST SDLC link up\n", machine().time().as_double());
		m_sdlc = SDLC_NRM;
		m_host_vs = m_host_vr = m_host_va = 0;
		m_host_unacked.clear();
		host_link_up();
	}
	if (final)
	{
		m_host_wait_final = false;
		m_host_timer->adjust(attotime::from_msec(20));
	}
}

TIMER_CALLBACK_MEMBER(a91_state::host_poll)
{
	if (m_dce != DCE_DATA)
		return;
	if (m_bridge)
	{
		// the A91 answers DM to everything until it has seen a format 1 XID; a leased NCP line sends none
		if (!m_bridge_xid_done)
		{
			x21_send_frame({ m_host_addr, 0xbf,
					0x14, 0x16, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x09,
					0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00 });
			m_host_timer->adjust(attotime::from_seconds(1));
		}
		return;
	}
	if (m_sdlc == SDLC_DISC || m_sdlc == SDLC_XID)
	{
		// format 1 XID from a PU type 4: the A91 accepts SNRM only after one of these
		m_sdlc = SDLC_XID;
		x21_send_frame({ m_host_addr, 0xbf,
				0x14, 0x16, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x09,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00 });
	}
	else if (m_sdlc != SDLC_NRM)
	{
		m_sdlc = SDLC_SNRM;
		x21_send_frame({ m_host_addr, 0x93 });
	}
	else
	{
		// send what the window allows, the last frame polls; otherwise poll with RR
		unsigned sent = 0;
		while (!m_host_out.empty() && m_host_unacked.size() < 7)
		{
			std::vector<u8> piu = std::move(m_host_out.front());
			m_host_out.pop_front();
			bool const last = m_host_out.empty() || m_host_unacked.size() == 6;
			std::vector<u8> frame{ m_host_addr, u8((m_host_vr << 5) | (last ? 0x10 : 0) | (m_host_vs << 1)) };
			frame.insert(frame.end(), piu.begin(), piu.end());
			m_host_unacked.push_back(piu);
			m_host_vs = (m_host_vs + 1) & 7;
			x21_send_frame(frame);
			sent++;
		}
		if (!sent)
			x21_send_frame({ m_host_addr, u8((m_host_vr << 5) | 0x11) });
	}
	m_host_wait_final = true;
	// no final within a second: poll again
	m_host_timer->adjust(attotime::from_seconds(1));
}

void a91_state::bridge_from_a91(std::vector<u8> const &frame)
{
	if (!m_bridge_xid_done)
	{
		if (frame.size() >= 2 && (frame[1] & 0xef) == 0xaf)
		{
			LOGX21("%.6f BRIDGE XID from the station, frames now go to the 3705\n", machine().time().as_double());
			m_bridge_xid_done = true;
		}
		return;
	}
	// one write per frame: the 3705 hands each read to its scanner, and a frame split across reads breaks
	std::vector<u8> out{ 0x7e };
	out.insert(out.end(), frame.begin(), frame.end());
	out.insert(out.end(), { 0x47, 0x0f, 0x7e });
	if (m_host_link->exists())
		m_host_link->fwrite(out.data(), out.size());
}

TIMER_CALLBACK_MEMBER(a91_state::bridge_poll)
{
	u8 buf[1024];
	u32 const n = m_host_link->input(buf, sizeof(buf));
	if (!n)
		return;
	m_bridge_rx.insert(m_bridge_rx.end(), buf, buf + n);
	for (;;)
	{
		size_t start = 0;
		while (start < m_bridge_rx.size() && (m_bridge_rx[start] == 0x7e || m_bridge_rx[start] == 0x00 || m_bridge_rx[start] == 0xaa))
			start++;
		size_t end = start;
		while (end + 2 < m_bridge_rx.size() && !(m_bridge_rx[end] == 0x47 && m_bridge_rx[end + 1] == 0x0f && m_bridge_rx[end + 2] == 0x7e))
			end++;
		if (end + 2 >= m_bridge_rx.size())
		{
			m_bridge_rx.erase(m_bridge_rx.begin(), m_bridge_rx.begin() + start);
			break;
		}
		std::vector<u8> frame(m_bridge_rx.begin() + start, m_bridge_rx.begin() + end);
		m_bridge_rx.erase(m_bridge_rx.begin(), m_bridge_rx.begin() + end + 3);
		if (frame.size() >= 2 && m_dce == DCE_DATA && m_bridge_xid_done)
			x21_send_frame(frame);
		else
			LOGX21("%.6f BRIDGE dropped %s\n", machine().time().as_double(), hexdump(frame.data(), frame.size()));
	}
}

void a91_state::host_sdlc_send_i(std::vector<u8> const &piu)
{
	LOGX21("%.6f HOST PIU %d: %s\n", machine().time().as_double(), int(piu.size()), hexdump(piu.data(), piu.size()));
	m_host_out.push_back(piu);
}

// a minimal SNA host: SSCP for the A91's PU and LUs, and one application that echoes the screen

static u8 const s_ebcdic[128] = {
	0x00, 0x01, 0x02, 0x03, 0x37, 0x2d, 0x2e, 0x2f, 0x16, 0x05, 0x25, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x3c, 0x3d, 0x32, 0x26, 0x18, 0x19, 0x3f, 0x27, 0x1c, 0x1d, 0x1e, 0x1f,
	0x40, 0x5a, 0x7f, 0x7b, 0x5b, 0x6c, 0x50, 0x7d, 0x4d, 0x5d, 0x5c, 0x4e, 0x6b, 0x60, 0x4b, 0x61,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0x7a, 0x5e, 0x4c, 0x7e, 0x6e, 0x6f,
	0x7c, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
	0xd7, 0xd8, 0xd9, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xba, 0xe0, 0xbb, 0xb0, 0x6d,
	0x79, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,
	0x97, 0x98, 0x99, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xc0, 0x4f, 0xd0, 0xa1, 0x07 };

static u8 const s_3270addr[64] = {
	0x40, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f };

// the A91 speaks EBCDIC 278 (Swedish/Finnish); this is only for the log
static std::string ebcdic278_to_utf8(u8 c)
{
	switch (c)
	{
	case 0x5b: return "\u00c5";
	case 0x7b: return "\u00c4";
	case 0x7c: return "\u00d6";
	case 0xd0: return "\u00e5";
	case 0xc0: return "\u00e4";
	case 0x6a: return "\u00f6";
	case 0x79: return "\u00e9";
	case 0xe0: return "\u00c9";
	case 0xa1: return "\u00fc";
	case 0x5a: return "\u00a4";
	case 0x4a: return "#";
	case 0x4f: return "!";
	}
	for (int i = 0x20; i < 0x7f; i++)
		if (s_ebcdic[i] == c)
			return std::string(1, char(i));
	return c ? "." : " ";
}

void a91_state::host_send(u8 daf, u8 oaf, u8 rh0, u8 rh1, u8 rh2, std::vector<u8> const &ru)
{
	// session control and network control go expedited; FMD and DFC count the normal flow from SDT
	bool const normal = !(rh0 & 0x20);
	u16 const snf = ++m_host_snf[daf][oaf ? 1 : 0][normal ? 1 : 0];
	std::vector<u8> piu{ 0x2c, 0x00, daf, oaf, u8(snf >> 8), u8(snf), rh0, rh1, rh2 };
	piu.insert(piu.end(), ru.begin(), ru.end());
	host_sdlc_send_i(piu);
}

void a91_state::host_link_up()
{
	memset(m_host_snf, 0, sizeof(m_host_snf));
	m_host_lu = 0;
	m_host_bound = false;
	m_host_in_bracket = false;
	// ACTPU, cold, FM profile 0 / TS profile 1, SSCP ID of a PU type 5
	host_send(0x00, 0x00, 0x6b, 0x80, 0x00, { 0x11, 0x01, 0x01, 0x05, 0x00, 0x00, 0x00, 0x00, 0x01 });
}

TIMER_CALLBACK_MEMBER(a91_state::host_bind)
{
	if (m_sdlc != SDLC_NRM || !m_host_lu || m_host_bound)
		return;
	host_send(m_host_lu, 0x01, 0x6b, 0x80, 0x00, {
			0x31, 0x01, 0x03, 0x03, 0xb1, 0x90, 0x30, 0x80, 0x00, 0x00, 0x87, 0x87, 0x00, 0x00,
			0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x50, 0x18, 0x50, 0x7e, 0x00, 0x00, 0x00, 0x00 });
}

void a91_state::host_screen(u8 lu, std::string const &line1, std::vector<u8> const &line2)
{
	std::vector<u8> ru{ 0xf5, 0xc3 };
	auto sba = [&ru] (int row, int col)
	{
		int const a = row * 80 + col;
		ru.push_back(0x11);
		ru.push_back(s_3270addr[(a >> 6) & 0x3f]);
		ru.push_back(s_3270addr[a & 0x3f]);
	};
	auto text = [&ru] (std::string const &t) { for (char c : t) ru.push_back(s_ebcdic[c & 0x7f]); };
	sba(0, 0);
	ru.insert(ru.end(), { 0x1d, 0xf8 });
	text("ALFASKOP 91 HOST - SNA OVER X.21 - EMULATED HOST AT DIAL NUMBER 100852");
	sba(2, 0);
	ru.insert(ru.end(), { 0x1d, 0xf0 });
	text(line1);
	sba(3, 0);
	ru.insert(ru.end(), { 0x1d, 0xf0 });
	ru.insert(ru.end(), line2.begin(), line2.end());
	sba(6, 0);
	ru.insert(ru.end(), { 0x1d, 0xf0 });
	text("TYPE HERE:");
	sba(6, 11);
	ru.insert(ru.end(), { 0x1d, 0x40, 0x13 });
	sba(6, 79);
	ru.insert(ru.end(), { 0x1d, 0xf0 });
	sba(22, 0);
	ru.insert(ru.end(), { 0x1d, 0xf0 });
	text("ENTER SENDS THE FIELD. PF AND PA KEYS ARE ECHOED TOO.");
	// FMD, only chain, exception response, change direction
	// the A91 wants definite response; the first chain opens the bracket, replies stay inside it
	u8 const rh2 = m_host_in_bracket ? 0x20 : 0xa0;
	m_host_in_bracket = true;
	host_send(lu, 0x01, 0x03, 0x80, rh2, ru);
}

void a91_state::host_piu_in(std::vector<u8> const &piu)
{
	LOGX21("%.6f A91 PIU %d: %s\n", machine().time().as_double(), int(piu.size()), hexdump(piu.data(), piu.size()));
	if (piu.size() < 9 || (piu[0] & 0xf0) != 0x20)
		return;
	u8 const daf = piu[2], oaf = piu[3];
	u8 const rh0 = piu[6], rh1 = piu[7];
	std::vector<u8> const ru(piu.begin() + 9, piu.end());
	bool const response = BIT(rh0, 7);

	if (response)
	{
		bool const negative = BIT(rh1, 4);
		u8 const code = ru.empty() ? 0 : ru[0];
		if (negative)
		{
			LOGX21("%.6f HOST negative response from %02X: %s\n", machine().time().as_double(), oaf, hexdump(ru.data(), ru.size()));

			if (!oaf && !m_host_lu)
				return;
		}
		if (!oaf && code == 0x11)
		{
			// PU active: activate the LUs
			for (u8 lu = 0x02; lu <= 0x05; lu++)
				host_send(lu, 0x00, 0x6b, 0x80, 0x00, { 0x0d, 0x01, 0x01 });
		}
		else if (daf == 0x00 && code == 0x0d && !negative && !m_host_lu)
		{
			// first LU that answers ACTLU gets a session with the application
			m_host_lu = oaf;
			m_host_bind_timer->adjust(attotime::zero);
		}
		else if (daf == 0x01 && negative && ru.size() >= 5 && ru[4] == 0x31)
		{
			// BIND rejected: the display may not be up yet
			m_host_bind_timer->adjust(attotime::from_seconds(5));
		}
		else if (daf == 0x01 && code == 0x31 && !negative)
			host_send(oaf, 0x01, 0x6b, 0x80, 0x00, { 0xa0 });
		else if (daf == 0x01 && code == 0xa0 && !negative)
		{
			m_host_snf[oaf][1][1] = 0;
			m_host_bound = true;
			host_screen(oaf, "SESSION BOUND. THE A91 ANSWERED ACTPU, ACTLU, BIND AND SDT.", {});
		}
		return;
	}

	// a request: answer it if it asks for a definite response
	if (BIT(rh1, 7) && !BIT(rh1, 4))
	{
		std::vector<u8> rsp;
		if ((rh0 & 0x60) != 0x00 && !ru.empty())
			rsp.push_back(ru[0]);
		else if (BIT(rh0, 3) && ru.size() >= 3)
			rsp.insert(rsp.end(), ru.begin(), ru.begin() + 3);
		std::vector<u8> out{ 0x2c, 0x00, oaf, daf, piu[4], piu[5], u8(0x80 | (rh0 & 0x7b) | 0x03), u8(rh1 & 0xa0), 0x00 };
		out.insert(out.end(), rsp.begin(), rsp.end());
		host_sdlc_send_i(out);
	}

	// 3270 data from the LU to the application
	if (daf == 0x01 && (rh0 & 0x60) == 0x00 && !ru.empty() && m_host_bound)
	{
		u8 const aid = ru[0];
		std::vector<u8> typed;
		std::string logged;
		for (size_t i = 3; i < ru.size(); i++)
		{
			if (ru[i] == 0x11 && i + 2 < ru.size())
			{
				i += 2;
				if (!typed.empty())
				{
					typed.push_back(0x40);
					logged += ' ';
				}
				continue;
			}
			typed.push_back(ru[i]);
			logged += ebcdic278_to_utf8(ru[i]);
		}
		std::string key;
		if (aid == 0x7d) key = "ENTER";
		else if (aid >= 0xf1 && aid <= 0xf9) key = util::string_format("PF%d", aid - 0xf0);
		else if (aid >= 0x7a && aid <= 0x7c) key = util::string_format("PF%d", aid - 0x7a + 10);
		else if (aid >= 0xc1 && aid <= 0xc9) key = util::string_format("PF%d", aid - 0xc1 + 13);
		else if (aid >= 0x4a && aid <= 0x4c) key = util::string_format("PF%d", aid - 0x4a + 22);
		else if (aid == 0x6c) key = "PA1";
		else if (aid == 0x6e) key = "PA2";
		else if (aid == 0x6b) key = "PA3";
		else if (aid == 0x6d) key = "CLEAR";
		else key = util::string_format("AID %02X", aid);
		LOGX21("%.6f HOST got %s: %s\n", machine().time().as_double(), key, logged);
		std::vector<u8> line2;
		for (char c : std::string("YOU TYPED: "))
			line2.push_back(s_ebcdic[u8(c)]);
		line2.insert(line2.end(), typed.begin(), typed.end());
		host_screen(oaf, "LAST KEY: " + key, line2);
	}
}

// DUOS 5.6 stores the 8-bit internal code (C3 = Ä, F3 = ö, ...) in VRAM, but this chargen is the
// 128-glyph Swedish 7-bit one.  National characters go through the internal-to-display part of
// Ericsson's own table LGG/L00010A; everything else keeps the low seven bits.
static u8 du_display_code(u8 code)
{
	switch (code)
	{
	case 0x8f: return 0x24;
	case 0xb2: return 0x23;
	case 0xc3: return 0x5b;
	case 0xc5: return 0x5d;
	case 0xc7: return 0x40;
	case 0xd3: return 0x5c;
	case 0xe3: return 0x7b;
	case 0xe5: return 0x7d;
	case 0xe7: return 0x60;
	case 0xf3: return 0x7c;
	case 0xf8: return 0x7e;
	default:   return code & 0x7f;
	}
}

// 3270 field attributes sit in VRAM as 0x80 | attribute (bit 1 of an attribute is always 0): shown
// blank, and a nondisplay field hides its text
static bool du_is_attribute(u8 code)
{
	return (code & 0xc2) == 0x80;
}

MC6845_UPDATE_ROW(a91_state::du_crtc_update_row)
{
	offs_t const base = ma + 0x4000;
	u32 *px = &bitmap.pix(y);

	// row 24 is the DUOS status line, outside the 3270 presentation space
	bool const status = y >= 24 * 16;
	bool hidden = false;
	for (int k = 1; !status && k <= 1920; k++)
	{
		u8 const code = m_du_vram[(base - k) & 0x07ff];
		if (du_is_attribute(code))
		{
			hidden = (code & 0x0c) == 0x0c;
			break;
		}
	}

	for (int i = 0; i < x_count; i++)
	{
		u8 const code = m_du_vram[(base + i) & 0x07ff];
		u8 chr;
		if (!status && du_is_attribute(code))
		{
			hidden = (code & 0x0c) == 0x0c;
			chr = 0x20;
		}
		else
			chr = hidden ? 0x20 : du_display_code(code);
		u8 dots = m_du_chargen[chr * 16 + ra];
		for (int n = 8; n > 0; n--, dots <<= 1)
			*px++ = BIT(dots, 7) ? rgb_t::black() : rgb_t::white();
		*px++ = rgb_t::black();
	}
}

static INPUT_PORTS_START(a91)
INPUT_PORTS_END

static INPUT_PORTS_START(a91du)
	PORT_START("CONFIG")
	PORT_CONFNAME(0x03, 0x03, "Display unit on TCC line")
	PORT_CONFSETTING(0x00, "0")
	PORT_CONFSETTING(0x01, "1")
	PORT_CONFSETTING(0x02, "2")
	PORT_CONFSETTING(0x03, "3")
INPUT_PORTS_END

template <unsigned CH> void a91_state::tcc_wire_channel(machine_config &config)
{
	// one 6844 channel per ADLC, both directions: TCC91OS flips the channel's direction bit
	m_tccadlc[CH]->out_irq_cb().set("tccirq", FUNC(input_merger_device::in_w<CH>));
	m_tccadlc[CH]->out_rdsr_cb().set([this](int state) { m_tcc_rdsr[CH] = state; m_tccdma->dreq_w<CH>((m_tcc_rdsr[CH] || m_tcc_tdsr[CH]) ? 1 : 0); });
	m_tccadlc[CH]->out_tdsr_cb().set([this](int state) { m_tcc_tdsr[CH] = state; m_tccdma->dreq_w<CH>((m_tcc_rdsr[CH] || m_tcc_tdsr[CH]) ? 1 : 0); });
	m_tccadlc[CH]->out_rts_cb().set([this](int state) { if (state) m_tccadlc[CH]->set_cts(0); });
	m_tccadlc[CH]->set_out_frame_callback(FUNC(a91_state::tcc_frame_out<CH>));
	m_tccdma->in_ior_callback<CH>().set([this]() -> u8
			{
				u8 const data = m_tccadlc[CH]->dma_r();
				bool const fv = BIT(m_tccadlc[CH]->read(1), 1);
				if (!fv || !m_tcc_rx_tail[CH])
				{
					m_tcc_rx_tail[CH] = fv;
					m_tccdma->dreq_w<CH>(1);
				}
				else
				{
					m_tcc_rx_tail[CH] = false;
					m_tcc_rdsr[CH] = false;
					m_tccdma->dreq_w<CH>(m_tcc_tdsr[CH] ? 1 : 0);
					m_tccadlc[CH]->set_cts(0);
				}
				return data;
			});
	m_tccdma->out_iow_callback<CH>().set([this](u8 data)
			{
				u16 const remaining = (m_tccdma->read(0x02 + (CH << 2)) << 8) | m_tccdma->read(0x03 + (CH << 2));
				m_tccadlc[CH]->write(remaining == 1 ? 3 : 2, data);
			});
}

void a91_state::a91(machine_config &config)
{
	// Main board oscillator is 32 MHz; CPU is an 8 MHz part. Dividers provisional.
	M68000(config, m_maincpu, 32_MHz_XTAL / 4);
	m_maincpu->set_addrmap(AS_PROGRAM, &a91_state::main_map);
	m_maincpu->reset_cb().set(FUNC(a91_state::cpu_reset_w));

	HD63450(config, m_dmac, 32_MHz_XTAL / 4, m_maincpu, AS_PROGRAM);
	const attotime dma_clock = attotime::from_hz(8'000'000);
	m_dmac->set_clocks(dma_clock, dma_clock, dma_clock, dma_clock);
	m_dmac->set_burst_clocks(dma_clock, dma_clock, dma_clock, dma_clock);
	m_dmac->irq_callback().set(FUNC(a91_state::dma_irq_w));
	m_dmac->dma8_read<3>().set(m_fdc, FUNC(wd2797_device::data_r));
	m_dmac->dma8_write<3>().set(m_fdc, FUNC(wd2797_device::data_w));

	PIT68230(config, m_pit, 32_MHz_XTAL / 4);
	m_pit->pa_out_callback().set(FUNC(a91_state::floppy_control_w));
	m_pit->pb_out_callback().set(FUNC(a91_state::leds_w));
	m_pit->timer_irq_callback().set(FUNC(a91_state::timer_irq_w));
	// unconnected port C inputs: the device leaves its input latch uninitialised
	m_pit->pc_in_callback().set([this]() -> u8 { return m_pc_in; });

	mk68564_device &sio(MK68564(config, "sio", 32_MHz_XTAL / 8));
	sio.set_xtal(3.6864_MHz_XTAL);
	// channel A is the host line; the CP's level 4 autovector reads the vector register
	sio.out_int_callback().set_inputline(m_maincpu, M68K_IRQ_4);
	sio.out_txda_callback().set([this](int state) { m_x21_txd = state; });
	// RxRDY and TxRDY of channel A request DMAC channels 0 and 1 (DAR = data register)
	sio.out_rxdrqa_callback().set(m_dmac, FUNC(hd63450_device::drq0_w));
	sio.out_txdrqa_callback().set(m_dmac, FUNC(hd63450_device::drq1_w));
	sio.out_txdb_callback().set("rs232b", FUNC(rs232_port_device::write_txd));
	sio.out_dtrb_callback().set("rs232b", FUNC(rs232_port_device::write_dtr));
	sio.out_rtsb_callback().set("rs232b", FUNC(rs232_port_device::write_rts));
	rs232_port_device &rs232b(RS232_PORT(config, "rs232b", default_rs232_devices, nullptr));
	rs232b.rxd_handler().set("sio", FUNC(mk68564_device::rxb_w));
	rs232b.cts_handler().set("sio", FUNC(mk68564_device::ctsb_w));
	rs232b.dcd_handler().set("sio", FUNC(mk68564_device::dcdb_w));

	WD2797(config, m_fdc, 32_MHz_XTAL / 32);
	m_fdc->intrq_wr_callback().set(FUNC(a91_state::fdc_irq_w));
	m_fdc->drq_wr_callback().set(m_dmac, FUNC(hd63450_device::drq3_w));
	m_fdc->sso_wr_callback().set([this](int state) {
		for (auto &connector : m_floppy)
			if (connector->get_device())
				connector->get_device()->ss_w(state);
	});
	FLOPPY_CONNECTOR(config, "fdc:0", floppy_drives, "525qd", floppy_image_device::default_mfm_floppy_formats);
	FLOPPY_CONNECTOR(config, "fdc:1", floppy_drives, "525qd", floppy_image_device::default_mfm_floppy_formats);

	// TCC clock source: 19.170 MHz oscillator. Actual dividers unknown.
	M6800(config, m_tcccpu, 19.17_MHz_XTAL / 18);
	m_tcccpu->set_addrmap(AS_PROGRAM, &a91_state::tcc_map);
	input_merger_device &tccirq(INPUT_MERGER_ANY_HIGH(config, "tccirq"));
	tccirq.output_handler().set_inputline(m_tcccpu, M6800_IRQ_LINE);
	ptm6840_device &ptm(PTM6840(config, "tccptm", 19.17_MHz_XTAL / 18));
	ptm.irq_callback().set("tccirq", FUNC(input_merger_device::in_w<4>));
	MC6844(config, m_tccdma, 19.17_MHz_XTAL / 18);
	m_tccdma->out_int_callback().set("tccirq", FUNC(input_merger_device::in_w<5>));
	// TCC91OS uses mode 2 (HALT steal), which requests the bus on DRQ2; grant either request at once
	m_tccdma->out_drq1_callback().set(m_tccdma, FUNC(mc6844_device::dgrnt_w));
	m_tccdma->out_drq2_callback().set(m_tccdma, FUNC(mc6844_device::dgrnt_w));
	m_tccdma->in_memr_callback().set([this](offs_t offset) { return m_tcccpu->space(AS_PROGRAM).read_byte(offset); });
	m_tccdma->out_memw_callback().set([this](offs_t offset, u8 data) { m_tcccpu->space(AS_PROGRAM).write_byte(offset, data); });
	for (int i = 0; i < 4; i++)
		MC6854(config, m_tccadlc[i], 19.17_MHz_XTAL / 18);
	tcc_wire_channel<0>(config);
	tcc_wire_channel<1>(config);
	tcc_wire_channel<2>(config);
	tcc_wire_channel<3>(config);
}

void a91_state::a91du(machine_config &config)
{
	a91(config);
	config.set_default_layout(layout_a91du);
	BITBANGER(config, m_host_link, 0);

	M6800(config, m_ducpu, 19.17_MHz_XTAL / 18);
	m_ducpu->set_addrmap(AS_PROGRAM, &a91_state::du_mem_map);

	PIA6821(config, m_du_mic_pia);
	m_du_mic_pia->ca2_handler().set([this](int state) { du_irq_w<0>(state ? 0 : 1); });
	m_du_mic_pia->irqa_handler().set(FUNC(a91_state::du_irq_w<1>));
	m_du_mic_pia->writepa_handler().set([this](u8 data) { m_du_kbd->rst_line_w(BIT(data, 1) ? ASSERT_LINE : CLEAR_LINE); });
	PIA6821(config, m_du_dia_pia);
	ACIA6850(config, m_du_kbd_acia, 0);
	m_du_kbd_acia->irq_handler().set(FUNC(a91_state::du_irq_w<3>));

	// the keyboard link is one shared wired-AND pair: every station hears itself
	ALFASKOP_S41_KB(config, m_du_kbd, 0);
	// A91 R4A only carries keyboard tables for the KBU 4143 (library KG4143)
	m_du_kbd->set_4143_identity(true);
	m_du_kbd->set_shift_pf13(true);
	m_du_kbd->txd_cb().set([this](int state)
			{
				m_du_kbd_line_kbd = state != 0;
				int const line = (m_du_kbd_line_kbd && m_du_kbd_line_acia) ? 1 : 0;
				m_du_kbd_acia->write_rxd(line);
				m_du_kbd->rxd_w(line);
			});
	m_du_kbd_acia->txd_handler().set([this](int state)
			{
				m_du_kbd_line_acia = state != 0;
				int const line = (m_du_kbd_line_kbd && m_du_kbd_line_acia) ? 1 : 0;
				m_du_kbd_acia->write_rxd(line);
				m_du_kbd->rxd_w(line);
			});

	MC6845(config, m_du_crtc, 19.17_MHz_XTAL / 9);
	m_du_crtc->set_screen(m_du_screen);
	m_du_crtc->set_show_border_area(false);
	m_du_crtc->set_char_width(9);
	m_du_crtc->set_update_row_callback(FUNC(a91_state::du_crtc_update_row));
	SCREEN(config, m_du_screen, SCREEN_TYPE_RASTER);
	m_du_screen->set_raw(19'170'000, 900, 0, 720, 426, 0, 400);
	m_du_screen->set_screen_update(m_du_crtc, FUNC(mc6845_device::screen_update));

	// TIA board: ADLC + DMAC, channel 0 = tx, channel 1 = rx
	MC6854(config, m_du_adlc, 19.17_MHz_XTAL / 18);
	m_du_adlc->out_irq_cb().set(FUNC(a91_state::du_irq_w<7>));
	m_du_adlc->out_rdsr_cb().set([this](int state) { m_du_dma->dreq_w<1>(state); });
	m_du_adlc->out_tdsr_cb().set([this](int state) { m_du_dma->dreq_w<0>(state); });
	m_du_adlc->out_rts_cb().set([this](int state) { if (state) m_du_adlc->set_cts(0); });
	m_du_adlc->set_out_frame_callback(FUNC(a91_state::du_frame_out));

	MC6844(config, m_du_dma, 19.17_MHz_XTAL / 18);
	m_du_dma->out_drq1_callback().set([this](bool state) { m_du_dma->dgrnt_w(state); });
	m_du_dma->out_drq2_callback().set([this](bool state) { m_du_dma->dgrnt_w(state); });
	m_du_dma->in_memr_callback().set([this](offs_t offset) { return m_ducpu->space(AS_PROGRAM).read_byte(offset); });
	m_du_dma->out_memw_callback().set([this](offs_t offset, u8 data) { m_ducpu->space(AS_PROGRAM).write_byte(offset, data); });
	m_du_dma->in_ior_callback<1>().set([this]() -> u8
			{
				u8 const data = m_du_adlc->dma_r();
				bool const fv = BIT(m_du_adlc->read(1), 1);
				if (!fv || !m_du_rx_tail)
				{
					m_du_rx_tail = fv;
					m_du_dma->dreq_w<1>(1);
				}
				else
				{
					m_du_rx_tail = false;
					m_du_dma->dreq_w<1>(0);
					m_du_adlc->set_cts(0);
				}
				return data;
			});
	m_du_dma->out_iow_callback<0>().set([this](u8 data)
			{
				u16 const remaining = (m_du_dma->read(0x02) << 8) | m_du_dma->read(0x03);
				m_du_adlc->write(remaining == 1 ? 3 : 2, data);
			});
}

ROM_START(a91)
	ROM_REGION16_BE(0x4000, "bootrom", 0)
	ROM_LOAD16_BYTE("e34058709112202.bin", 0x0000, 0x2000, CRC(fff22d87) SHA1(e4f15b31d076e0141bacda68fd89cf828de09dff))
	ROM_LOAD16_BYTE("e34058709112302.bin", 0x0001, 0x2000, CRC(852c3c67) SHA1(6c5ba633ce4fb604b4b6bf8794d4dadb8a811ad4))
ROM_END

ROM_START(a91du)
	ROM_REGION16_BE(0x4000, "bootrom", 0)
	ROM_LOAD16_BYTE("e34058709112202.bin", 0x0000, 0x2000, CRC(fff22d87) SHA1(e4f15b31d076e0141bacda68fd89cf828de09dff))
	ROM_LOAD16_BYTE("e34058709112302.bin", 0x0001, 0x2000, CRC(852c3c67) SHA1(6c5ba633ce4fb604b4b6bf8794d4dadb8a811ad4))

	ROM_REGION(0x800, "duroms", ROMREGION_ERASEFF)
	ROM_LOAD("e3405870205201.bin", 0x0000, 0x0800, CRC(23f20f7f) SHA1(6ed008e309473ab966c6b0d42a4f87c76a7b1d6e))
	ROM_REGION(0x800, "duchargen", ROMREGION_ERASEFF)
	ROM_LOAD("e3405972067500.bin", 0x0000, 0x0400, CRC(fb12b549) SHA1(53783f62c5e51320a53e053fbcf8b3701d8a805f))
	ROM_LOAD("e3405972067600.bin", 0x0400, 0x0400, CRC(c7069d65) SHA1(587efcbee036d4c0c5b936cc5d7b1f97b6fe6dba))
ROM_END

} // anonymous namespace

COMP(198?, a91,   0,   0, a91,   a91,   a91_state, empty_init, "Ericsson", "Alfaskop 91 Controller", MACHINE_NOT_WORKING | MACHINE_NO_SOUND)
COMP(198?, a91du, a91, 0, a91du, a91du, a91_state, empty_init, "Ericsson", "Alfaskop 91 Controller with DU 4110", MACHINE_NO_SOUND)
