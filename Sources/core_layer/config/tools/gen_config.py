#!/usr/bin/env python3
"""
River FW SDK - build-time peripheral-config code generator.

Emits ONE River-style C header per product, core_layer/config/generated_code/<product>/inc/rr_hw_cfg.h,
from a choice of input (see --source). Every driver/bsp includes this single header - there is no
per-peripheral config file to manage. Drivers include it instead of carrying hardcoded
baud/timeout/clock/channel/period literals. NO runtime XML parse ever runs on the MCU - all
resolution happens here, at build time.

Two input sources feed one shared normalized dict, which feeds one shared emit stage so the
emitted RR_* token contract, validation and generation hash are identical regardless of source:
  --source xml    (default) parse core_layer/config/variants/<product>.xml (the source of truth).
  --source vendor          parse a vendor IDE's codegen tree into the same normalized dict.
                           XML write-back is a SEPARATE opt-in flag, --write-xml (default OFF):
                           with it, the dict is rewritten back to <product>.xml so the XML stays
                           the source of truth; without it, only rr_hw_cfg.h is (re)generated and
                           the XML is left untouched. Write-back is gated on parser completeness -
                           a partial parse refuses to overwrite rather than silently drop fields.

Usage:
    python gen_config.py --product vcu
    python gen_config.py --all                                   # every variants/*.xml
    python gen_config.py --product vcu --source vendor \
        --vendor nxp --vendor-dir /path/to/rx_03_ry_01_vcu_nxp/Generated_Code
    python gen_config.py --product vcu --source vendor --vendor nxp \
        --vendor-dir <...> --write-xml                           # also refresh variants/vcu.xml

Idempotent: same input -> byte-identical output.
"""

import argparse
import datetime
import hashlib
import json
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

HERE = Path(__file__).resolve().parent          # core_layer/config/tools
CONFIG_DIR = HERE.parent / "variants"           # core_layer/config/variants
GEN_ROOT = HERE.parent / "generated_code"       # core_layer/config/generated_code

BANNER_DASHES = 95
AUTHOR = "vishalagarwal_rideri"
TABW = 8                                         # tab stop width the River style aligns to
CFG_FILE = "rr_hw_cfg.h"

# Peripheral child element tags (repeatable rows under a peripheral). Used by the XML loader to
# decide list-vs-scalar and by the XML writer to re-nest rows. Order-independent.
CHILD_TAGS = ("instance", "group", "engine", "source")

# --- Part-1 vendor-neutral token maps (public rr_*.h enums; NO vendor symbol here) -------------
# FTM timer counter clock source / prescaler tokens (rr_timer.h: timer_clk_src_te / timer_prescaler_te).
TIMER_CLK_SRC_TOK = {
    "SYSTEM": "TIMER_CLK_SRC_SYSTEM",
    "FIXED": "TIMER_CLK_SRC_FIXED",
    "EXTERNAL": "TIMER_CLK_SRC_EXTERNAL",
}
TIMER_PRESCALER_TOK = {str(d): "TIMER_PRESCALE_{}".format(d) for d in (1, 2, 4, 8, 16, 32, 64, 128)}
TIMER_PRESCALER_VALID = {1, 2, 4, 8, 16, 32, 64, 128}

# LPSPI line-format tokens (rr_spi.h: spi_cs_te / spi_cpha_te / spi_cpol_te / spi_bit_order_te).
SPI_CS_TOK = {"0": "SPI_CS_0", "1": "SPI_CS_1", "2": "SPI_CS_2", "3": "SPI_CS_3"}
SPI_CPHA_TOK = {"1ST_EDGE": "SPI_CPHA_FIRST_EDGE", "2ND_EDGE": "SPI_CPHA_SECOND_EDGE"}
SPI_CPOL_TOK = {"IDLE_LOW": "SPI_CPOL_IDLE_LOW", "IDLE_HIGH": "SPI_CPOL_IDLE_HIGH"}
SPI_BIT_ORDER_TOK = {"MSB": "SPI_MSB_FIRST", "LSB": "SPI_LSB_FIRST"}

# Permille ceiling (rr_pwm setDuty is 0..1000; feeds Polyspace Dir 4.14 pre-validation).
PWM_PERMILLE_MAX = 1000


def banner(title):
    """River .h section banner: '/* <Title> ' + 95 dashes + '*/'."""
    return "/* {} {}*/".format(title, "-" * BANNER_DASHES)


def subsection(title):
    """Peripheral sub-section marker inside the Public Macros section."""
    return "/* --- {} --- */".format(title)


def file_header(fname, brief, product, date_str, gen_hash=""):
    return (
        "/**\n"
        " * @file {fname}\n"
        " * @author {author}\n"
        " * @brief {brief}\n"
        " * @date {date}\n"
        " *\n"
        " * @copyright Copyright (c) River Moblity Pvt Ltd. All Rights Reserved 2026\n"
        " *\n"
        " * @note GENERATED FILE - DO NOT EDIT BY HAND. Produced by core_layer/config/tools/gen_config.py\n"
        " *       from core_layer/config/variants/{product}.xml. Edit the XML and re-run the generator instead.\n"
        " * @note Generation hash ({product}.xml): {gen_hash}. A mismatch on re-run signals a stale tree.\n"
        " */\n"
    ).format(fname=fname, author=AUTHOR, brief=brief, date=date_str, product=product, gen_hash=gen_hash)


def guard_open(fname):
    g = "CORE_LAYER_CONFIG_" + fname.replace(".", "_").upper() + "_"
    return "#ifndef {g}\n#define {g}\n".format(g=g), g


# --- Tab alignment ---------------------------------------------------------------------------
# Macro rows are laid out with tabs only (no space padding), aligned to TABW tab stops so a
# generated header sits column-for-column with hand-written River files (e.g. rr_adc_nxp.c).

def _col(s):
    """Visual column reached after string s (expanding tabs to TABW stops)."""
    c = 0
    for ch in s:
        c = (c // TABW + 1) * TABW if "\t" == ch else c + 1
    return c


def _pad(s, target):
    """Append tabs to s until its visual column reaches the target tab stop (>= 1 tab)."""
    c = _col(s)
    if c >= target:
        target = (c // TABW + 1) * TABW
    out = s
    while c < target:
        out += "\t"
        c = (c // TABW + 1) * TABW
    return out


def M(name, value, comment):
    """Parenthesised-value macro row (e.g. numeric build-time constants)."""
    return ("m", name, "({})".format(value), comment)


def MT(name, value, comment):
    """Bare-value macro row (enum tokens / string literals - no wrapping parentheses)."""
    return ("m", name, "{}".format(value), comment)


def render(entries):
    """Turn a section's entry list into text lines, aligning every macro row's value and
    comment columns to shared tab stops. Non-tuple entries (blank lines, standalone comments,
    #error lines) pass through verbatim."""
    macros = [e for e in entries if isinstance(e, tuple) and "m" == e[0]]
    vcol = ccol = 0
    if macros:
        vcol = max(_col("#define " + m[1]) for m in macros)
        vcol = (vcol // TABW + 1) * TABW
        ccol = max(_col(_pad("#define " + m[1], vcol) + m[2]) for m in macros)
        ccol = (ccol // TABW + 1) * TABW
    lines = []
    for e in entries:
        if isinstance(e, tuple) and "m" == e[0]:
            _, name, val, comment = e
            prefix = _pad("#define " + name, vcol) + val
            lines.append(_pad(prefix, ccol) + "/*!< {} */".format(comment))
        else:
            lines.append(e)
    return lines


# --- Normalized-dict accessors ---------------------------------------------------------------
# The shared IR is a plain nested dict: product attributes plus one key per peripheral tag whose
# value is that peripheral's attribute dict; repeatable rows (CHILD_TAGS) live as a list under
# their tag key inside the peripheral dict. find()/a()/kids() read that shape identically to how
# the old ElementTree accessors read the parsed XML, so the emitters below are source-agnostic.

def find(product_ir, tag):
    """Return the peripheral dict for tag, or None if absent on this product."""
    v = product_ir.get(tag)
    return v if isinstance(v, dict) else None


def a(el, key, default=None):
    """Attribute lookup on a peripheral / row dict (None-safe)."""
    return el.get(key, default) if el is not None else default


def kids(el, tag):
    """Repeatable child rows (list of dicts) under a peripheral dict."""
    return el.get(tag, []) if el is not None else []


def u(v):
    return "{}U".format(int(v))


# --- Per-peripheral section builders (each returns (title, entries) or None) ------------------

def sec_clock(p):
    el = find(p, "clock")
    return ("Clock", [
        M("RR_CLOCK_SRC_HZ", u(a(el, "src_hz")), "System oscillator source clock, Hz"),
        M("RR_CLOCK_FIRC_HZ", u(a(el, "firc_hz")), "Fast IRC clock, Hz"),
        M("RR_CLOCK_DIVCORE", u(a(el, "divcore")), "Core clock divider"),
        M("RR_CLOCK_DIVBUS", u(a(el, "divbus")), "Bus clock divider"),
        M("RR_CLOCK_DIVSLOW", u(a(el, "divslow")), "Slow (flash) clock divider"),
    ])


def sec_spi(p):
    el = find(p, "spi")
    insts = kids(el, "instance")
    L = [
        M("RR_SPI_PRESENT", u(a(el, "present")), "1 if the SPI peripheral is used on this product"),
        M("RR_SPI_SRC_CLK_HZ", u(a(el, "src_clk_hz")), "LPSPI functional source clock, Hz"),
        M("RR_SPI_TIMEOUT_MS", u(a(el, "timeout_ms")), "Blocking transfer timeout, ms"),
        M("RR_SPI_INSTANCE_COUNT", u(len(insts)), "Number of enabled LPSPI instances"),
        "",
    ]
    for it in insts:
        i = a(it, "id")
        L.append(M("RR_SPI_BAUD_{}".format(i), u(a(it, "baud")), "Instance {} master clock rate, bits/s".format(i)))
        L.append(M("RR_SPI_FRAME_BITS_{}".format(i), u(a(it, "frame_bits")), "Instance {} frame size, bits".format(i)))
        cs = str(a(it, "cs", "0"))
        L.append(MT("RR_SPI_CS_{}".format(i), SPI_CS_TOK[cs], "Instance {} chip-select selector".format(i)))
        cpha = str(a(it, "phase", "1ST_EDGE")).upper()
        L.append(MT("RR_SPI_CPHA_{}".format(i), SPI_CPHA_TOK[cpha], "Instance {} clock phase (CPHA)".format(i)))
        cpol = str(a(it, "polarity", "IDLE_LOW")).upper()
        L.append(MT("RR_SPI_CPOL_{}".format(i), SPI_CPOL_TOK[cpol], "Instance {} clock polarity (CPOL)".format(i)))
        order = str(a(it, "bit_order", "MSB")).upper()
        L.append(MT("RR_SPI_BIT_ORDER_{}".format(i), SPI_BIT_ORDER_TOK[order], "Instance {} frame bit order".format(i)))
    return ("SPI", L)


def sec_i2c(p):
    el = find(p, "i2c")
    present = int(a(el, "present"))
    L = [
        M("RR_I2C_PRESENT", u(present), "1 if the I2C peripheral is used on this product"),
        M("RR_I2C_TIMEOUT_MS", u(a(el, "timeout_ms")), "Blocking transfer timeout, ms"),
        M("RR_I2C_BAUD_HZ", u(a(el, "baud", 100000)), "LPI2C bus baud rate, bits/s"),
    ]
    if present:
        L.append(M("RR_I2C_SLAVE_ADDR", u(a(el, "slave_addr")), "LPI2C slave address"))
    return ("I2C", L)


def sec_adc(p):
    el = find(p, "adc")
    groups = kids(el, "group")
    gmax = max((int(a(g, "channels")) for g in groups), default=0)
    L = [
        M("RR_ADC_PRESENT", u(a(el, "present")), "1 if the ADC peripheral is used on this product"),
        M("RR_ADC_RESOLUTION_BITS", u(str(a(el, "resolution")).rstrip("BIT").rstrip("bit") or 12), "Converter resolution, bits (bsp maps to SDK enum)"),
        M("RR_ADC_CLOCK_DIVIDE", u(a(el, "clock_divide")), "ADC clock divider (bsp maps to SDK enum)"),
        M("RR_ADC_MAX_CHANNELS", u(a(el, "max_channels")), "Hardware channel ceiling"),
        M("RR_ADC_AVG_DEPTH", u(a(el, "avg_depth")), "Software averaging depth per channel"),
        M("RR_ADC_GROUP_COUNT", u(len(groups)), "Number of conversion groups"),
        M("RR_ADC_GROUP_MAX_CHANNELS", u(gmax), "Largest group's channel count"),
        "",
        "/* @note Voltage reference token; bsp maps to the SDK adc_voltage_reference_t value. */",
        M("RR_ADC_VOLTAGE_REF_{}".format(str(a(el, "voltage_ref")).upper()), "1U", "Selected ADC voltage reference"),
        "",
    ]
    for g in groups:
        gi = a(g, "id")
        L.append(M("RR_ADC_GROUP{}_CHANNELS".format(gi), u(a(g, "channels")), "Group {} channel count".format(gi)))
    return ("ADC", L)


def sec_timer(p):
    el = find(p, "timer")
    insts = kids(el, "instance")
    L = [
        M("RR_TIMER_PRESENT", u(a(el, "present")), "1 if the FTM timer is used on this product"),
        M("RR_TIMER_CHANNEL_MAX", u(a(el, "channel_max")), "Output-compare channels per timer instance"),
        M("RR_TIMER_INSTANCE_COUNT", u(len(insts)), "Number of enabled FTM instances"),
        "",
    ]
    for n, it in enumerate(insts):
        L.append(M("RR_TIMER_INST{}_HW".format(n), u(a(it, "hw")), "Instance {} FTM hardware index".format(n)))
        L.append(M("RR_TIMER_INST{}_CHANNELS".format(n), u(a(it, "channels")), "Instance {} active channel count".format(n)))
        oneshot = 1 if str(a(it, "mode")).upper() == "ONESHOT" else 0
        L.append(M("RR_TIMER_INST{}_ONESHOT".format(n), u(oneshot), "Instance {} 1=oneshot 0=continuous".format(n)))
        clk = str(a(it, "clk_src", "SYSTEM")).upper()
        if clk not in TIMER_CLK_SRC_TOK:
            sys.exit("error: timer instance {} clk_src '{}' invalid (use SYSTEM/FIXED/EXTERNAL)".format(n, clk))
        L.append(M("RR_TIMER_INST{}_CLK_SRC".format(n), TIMER_CLK_SRC_TOK[clk], "Instance {} counter clock source token".format(n)))
        ps = str(a(it, "prescaler", "1"))
        if not ps.isdigit() or int(ps) not in TIMER_PRESCALER_VALID:
            sys.exit("error: timer instance {} prescaler '{}' invalid (use 1/2/4/8/16/32/64/128)".format(n, ps))
        L.append(M("RR_TIMER_INST{}_PRESCALER".format(n), TIMER_PRESCALER_TOK[ps], "Instance {} counter clock prescaler token".format(n)))
    return ("Timer", L)


def sec_can(p):
    el = find(p, "can")
    insts = kids(el, "instance")
    L = [
        M("RR_CAN_PRESENT", u(a(el, "present")), "1 if FlexCAN is used on this product"),
        M("RR_CAN_INSTANCE_COUNT", u(len(insts)), "Number of enabled CAN instances"),
        "",
    ]
    for it in insts:
        i = a(it, "id")
        L.append(M("RR_CAN{}_PROP_SEG".format(i), u(a(it, "prop_seg")), "CAN{} propagation segment".format(i)))
        L.append(M("RR_CAN{}_PSEG1".format(i), u(a(it, "pseg1")), "CAN{} phase segment 1".format(i)))
        L.append(M("RR_CAN{}_PSEG2".format(i), u(a(it, "pseg2")), "CAN{} phase segment 2".format(i)))
        L.append(M("RR_CAN{}_PRE_DIVIDER".format(i), u(a(it, "pre_divider")), "CAN{} clock pre-divider".format(i)))
        L.append(M("RR_CAN{}_RJW".format(i), u(a(it, "rjw")), "CAN{} resync jump width".format(i)))
    return ("CAN", L)


def sec_watchdog(p):
    el = find(p, "watchdog")
    insts = kids(el, "instance")
    ewm = next((i for i in insts if str(a(i, "type")) == "external_ewm"), None)
    internal = next((i for i in insts if str(a(i, "type")) == "internal"), None)
    return ("Watchdog", [
        M("RR_WDOG_PRESENT", u(a(el, "present")), "1 if a watchdog is used on this product"),
        M("RR_WDOG_INTERNAL_TIMEOUT", u(a(internal, "timeout") if internal is not None else 0), "Internal WDOG timeout, ticks"),
        M("RR_WDOG_WINDOW_PERCENT", u(a(internal, "window_percent", 0) if internal is not None else 50), "Windowed-refresh opening, % of timeout"),
        M("RR_WDOG_EWM_PRESENT", u(1 if ewm is not None else 0), "1 if the external EWM watchdog is used"),
        M("RR_WDOG_EWM_TIMEOUT", u(a(ewm, "timeout") if ewm is not None else 0), "External EWM timeout window"),
    ])


def sec_crc(p):
    el = find(p, "crc")
    present = int(a(el, "present"))
    engines = kids(el, "engine") if present else []
    hw = next((e for e in engines if str(a(e, "type")) == "hw"), None)
    sw = next((e for e in engines if str(a(e, "type")) == "sw"), None)
    L = [
        M("RR_CRC_PRESENT", u(present), "1 if any CRC engine is used on this product"),
        M("RR_CRC_HW_PRESENT", u(1 if hw is not None else 0), "1 if the hardware CRC engine is used"),
    ]
    if hw is not None:
        L.append(M("RR_CRC_HW_WIDTH", u(a(hw, "width")), "HW CRC width, bits"))
        L.append(M("RR_CRC_HW_POLY", a(hw, "poly") + "U", "HW CRC polynomial"))
        L.append(M("RR_CRC_HW_SEED", a(hw, "seed") + "U", "HW CRC seed"))
    L.append(M("RR_CRC_SW_PRESENT", u(1 if sw is not None else 0), "1 if the software CRC engine is used"))
    if sw is not None:
        L.append(M("RR_CRC_SW_WIDTH", u(a(sw, "width")), "SW CRC width, bits"))
        L.append(M("RR_CRC_SW_POLY", a(sw, "poly") + "U", "SW CRC polynomial"))
    return ("CRC", L)


def sec_pwm(p):
    el = find(p, "pwm")
    return ("PWM", [
        M("RR_PWM_PRESENT", u(a(el, "present")), "1 if the PWM PAL is used on this product"),
        M("RR_PWM_PERIOD", u(a(el, "period")), "PWM period, timer ticks"),
    ])


def sec_rtc(p):
    el = find(p, "rtc")
    present = int(a(el, "present"))
    external = 1 if (present and str(a(el, "type")) == "external") else 0
    return ("RTC", [
        M("RR_RTC_PRESENT", u(present), "1 if an RTC is used on this product"),
        M("RR_RTC_EXTERNAL", u(external), "1 if the RTC is an external (SPI) device"),
    ])


def sec_nvic(p):
    """NVIC source-token section, emitted ONLY when the XML carries an <nvic> element.

    Decision-E double-enable guard (misra_cert_deviation_record.md 1.E): the FlexCAN PAL
    self-installs its own interrupt vectors, so a CAN source listed here would be enabled
    twice. If CAN is enabled and any <source> names a CAN interrupt, an #error is emitted
    into the generated header so the build fails loudly rather than double-enabling vectors.
    Absent an <nvic> element (current products) the section is omitted - the guard is dormant.
    """
    el = find(p, "nvic")
    if el is None:
        return None
    srcs = kids(el, "source")
    can_el = find(p, "can")
    can_present = can_el is not None and int(a(can_el, "present", "0")) != 0
    can_srcs = [str(a(s, "token", a(s, "id", ""))) for s in srcs
                if "CAN" in str(a(s, "token", a(s, "id", ""))).upper()]
    L = [M("RR_NVIC_SRC_COUNT_CFG", u(len(srcs)), "Number of NVIC sources registered from config")]
    if can_present and can_srcs:
        L.append("")
        L.append('#error "NVIC config lists CAN interrupt source(s) {} while FlexCAN is enabled. The '
                 'FlexCAN PAL self-installs its own vectors (see misra_cert_deviation_record.md 1.E); '
                 'listing them here double-enables the vectors. Remove the CAN <source> rows."'
                 .format(",".join(can_srcs)))
    L.append("")
    for s in srcs:
        tok = str(a(s, "token", a(s, "id", "")))
        L.append(MT("RR_NVIC_CFG_{}".format(tok.replace("RR_NVIC_SRC_", "")), tok,
                    "Registered interrupt source {}".format(tok)))
    return ("NVIC", L)


def validate(p):
    """Cross-field numeric validation (fail fast at generation time)."""
    pwm = find(p, "pwm")
    if pwm is not None:
        duty = a(pwm, "duty_permille")
        if duty is not None and int(duty) > PWM_PERMILLE_MAX:
            sys.exit("error: pwm duty_permille {} exceeds {} (0..1000 permille)".format(duty, PWM_PERMILLE_MAX))


SECTION_FNS = [sec_clock, sec_spi, sec_i2c, sec_adc, sec_timer, sec_can,
               sec_watchdog, sec_crc, sec_pwm, sec_rtc, sec_nvic]


# --- Source loaders: each returns the shared normalized dict ---------------------------------

def load_from_xml(xml_path):
    """Parse variants/<product>.xml into the normalized dict. Attribute strings are preserved
    verbatim (no int coercion) so a subsequent emit is byte-identical to parsing the tree directly.
    Product attributes sit at the top level; each peripheral element becomes {tag: {attrs, <child
    tag>: [row dicts]}}."""
    root = ET.parse(str(xml_path)).getroot()
    ir = dict(root.attrib)                       # product name / mcu / cpu
    for periph in root:
        pd = dict(periph.attrib)
        for row in periph:
            pd.setdefault(row.tag, []).append(dict(row.attrib))
        ir[periph.tag] = pd
    return ir


def load_from_vendor(vendor_dir, vendor):
    """Dispatch to the vendor codegen parser. Returns (ir, complete, notes):
      ir       - normalized dict (same shape as load_from_xml)
      complete - True only if every field the XML would carry was recovered; gates --write-xml
      notes    - human-readable list of fields defaulted / not recoverable from vendor code
    """
    if not vendor_dir.exists():
        sys.exit("error: --vendor-dir {} does not exist".format(vendor_dir))
    if "nxp" == vendor:
        return parse_nxp(vendor_dir)
    elif "renesas" == vendor:
        return parse_renesas(vendor_dir)
    elif "stm" == vendor:
        return parse_stm(vendor_dir)
    else:
        sys.exit("error: unknown --vendor '{}' (use nxp/renesas/stm)".format(vendor))


def parse_nxp(vendor_dir):
    """NXP S32K Processor Expert Generated_Code parser (round-tripped against vcu.xml/bms.xml).

    Regex-scrapes the PEx-generated .c files directly. Always returns complete=False: several XML
    fields (avg_depth, max_channels, timeout_ms, channel_max, ...) are River abstractions with no
    vendor-code equivalent, so a vendor parse can never fully round-trip the curated XML. Every
    defaulted / unrecoverable field is listed in the returned notes.
    """
    import re

    notes = []

    def read(p):
        path = p if isinstance(p, Path) else (vendor_dir / p)
        return path.read_text(encoding="utf-8", errors="replace")

    # --- clock (clockMan1.c) ----------------------------------------------------------------
    if not (vendor_dir / "clockMan1.c").exists():
        sys.exit("error: clockMan1.c not found in {} (required for --vendor nxp)".format(vendor_dir))
    ctext = read("clockMan1.c")

    src_hz = firc_hz = divcore = divbus = divslow = None
    m = re.search(r'\.soscConfig\s*=[^{]*\{([^}]*)\}', ctext)
    if m:
        mm = re.search(r'\.freq\s*=\s*(\d+)U?', m.group(1))
        src_hz = mm.group(1) if mm else None
    m = re.search(r'SCG_FIRC_RANGE_(\d+)M', ctext)
    firc_hz = str(int(m.group(1)) * 1000000) if m else None
    m = re.search(r'\.rccrConfig\s*=[^{]*\{([^}]*)\}', ctext)
    if m:
        body = m.group(1)
        dc = re.search(r'\.divCore\s*=\s*SCG_SYSTEM_CLOCK_DIV_BY_(\d+)', body)
        db = re.search(r'\.divBus\s*=\s*SCG_SYSTEM_CLOCK_DIV_BY_(\d+)', body)
        ds = re.search(r'\.divSlow\s*=\s*SCG_SYSTEM_CLOCK_DIV_BY_(\d+)', body)
        divcore = dc.group(1) if dc else None
        divbus = db.group(1) if db else None
        divslow = ds.group(1) if ds else None
    if src_hz is None:
        src_hz = "8000000"
        notes.append("clock.src_hz not recoverable from clockMan1.c, defaulted to 8000000")
    if firc_hz is None:
        firc_hz = "48000000"
        notes.append("clock.firc_hz not recoverable from clockMan1.c, defaulted to 48000000")
    if divcore is None:
        divcore = "2"
        notes.append("clock.divcore not recoverable from clockMan1.c, defaulted to 2")
    if divbus is None:
        divbus = "2"
        notes.append("clock.divbus not recoverable from clockMan1.c, defaulted to 2")
    if divslow is None:
        divslow = "4"
        notes.append("clock.divslow not recoverable from clockMan1.c, defaulted to 4")
    clock_el = {"src_hz": src_hz, "firc_hz": firc_hz, "divcore": divcore, "divbus": divbus, "divslow": divslow}

    # --- spi: lpspi_*.c (PAL) + lpspiCom*.c (RAW) --------------------------------------------
    pal_cpha = {"READ_ON_ODD_EDGE": "1ST_EDGE", "READ_ON_EVEN_EDGE": "2ND_EDGE",
                "SPI_CLOCK_PHASE_1ST_EDGE": "1ST_EDGE", "SPI_CLOCK_PHASE_2ND_EDGE": "2ND_EDGE"}
    raw_cpha = {"LPSPI_CLOCK_PHASE_1ST_EDGE": "1ST_EDGE", "LPSPI_CLOCK_PHASE_2ND_EDGE": "2ND_EDGE"}
    pal_cpol = {"SPI_ACTIVE_HIGH": "IDLE_LOW", "SPI_ACTIVE_LOW": "IDLE_HIGH"}
    raw_cpol = {"LPSPI_SCK_ACTIVE_HIGH": "IDLE_LOW", "LPSPI_SCK_ACTIVE_LOW": "IDLE_HIGH"}
    bit_order_tok = {"SPI_TRANSFER_MSB_FIRST": "MSB", "SPI_TRANSFER_LSB_FIRST": "LSB"}

    spi_rows = []
    src_clk_hz = None
    for f in sorted(vendor_dir.glob("lpspi_*.c")):
        text = read(f)
        m_idx = re.search(r'\.instIdx\s*=\s*(\d+)', text)
        m_cfg = re.search(r'spi_master_t\s+\w+\s*=\s*\{([^}]*)\}', text)
        if not m_idx or not m_cfg:
            continue
        hw = int(m_idx.group(1))
        body = m_cfg.group(1)
        row = {"hw": str(hw)}
        mb = re.search(r'\.baudRate\s*=\s*(\d+)', body)
        mf = re.search(r'\.frameSize\s*=\s*(\d+)', body)
        row["baud"] = mb.group(1) if mb else "0"
        row["frame_bits"] = mf.group(1) if mf else "8"
        mp = re.search(r'\.clockPhase\s*=\s*(\w+)', body)
        if mp and "2ND_EDGE" == pal_cpha.get(mp.group(1)):
            row["phase"] = "2ND_EDGE"
        mo = re.search(r'\.clockPolarity\s*=\s*(\w+)', body)
        if mo and "IDLE_HIGH" == pal_cpol.get(mo.group(1)):
            row["polarity"] = "IDLE_HIGH"
        mc = re.search(r'\.ssPin\s*=\s*(\d+)', body)
        if mc and "0" != mc.group(1):
            row["cs"] = mc.group(1)
        mbo = re.search(r'\.bitOrder\s*=\s*(\w+)', body)
        if mbo and "LSB" == bit_order_tok.get(mbo.group(1)):
            row["bit_order"] = "LSB"
        spi_rows.append((hw, row))

    for f in sorted(vendor_dir.glob("lpspiCom*.c")):
        text = read(f)
        m_brief = re.search(r'LPSPI(\d+)', text)
        m_cfg = re.search(r'lpspi_master_config_t\s+\w+\s*=\s*\{([^}]*)\}', text)
        if not m_cfg:
            continue
        hw = int(m_brief.group(1)) if m_brief else 0
        body = m_cfg.group(1)
        row = {"hw": str(hw)}
        mb = re.search(r'\.bitsPerSec\s*=\s*(\d+)', body)
        mf = re.search(r'\.bitcount\s*=\s*(\d+)', body)
        row["baud"] = mb.group(1) if mb else "0"
        row["frame_bits"] = mf.group(1) if mf else "8"
        mp = re.search(r'\.clkPhase\s*=\s*(\w+)', body)
        if mp and "2ND_EDGE" == raw_cpha.get(mp.group(1)):
            row["phase"] = "2ND_EDGE"
        mo = re.search(r'\.clkPolarity\s*=\s*(\w+)', body)
        if mo and "IDLE_HIGH" == raw_cpol.get(mo.group(1)):
            row["polarity"] = "IDLE_HIGH"
        mc = re.search(r'\.whichPcs\s*=\s*LPSPI_PCS(\d+)', body)
        if mc and "0" != mc.group(1):
            row["cs"] = mc.group(1)
        ml = re.search(r'\.lsbFirst\s*=\s*(true|false)', body)
        if ml and "true" == ml.group(1):
            row["bit_order"] = "LSB"
        mk = re.search(r'\.lpspiSrcClk\s*=\s*(\d+)', body)
        if mk and src_clk_hz is None:
            src_clk_hz = mk.group(1)
        spi_rows.append((hw, row))

    spi_rows.sort(key=lambda t: t[0])
    spi_instances = []
    for i, (hw, row) in enumerate(spi_rows):
        row["id"] = str(i)
        spi_instances.append(row)
    if src_clk_hz is None:
        src_clk_hz = "8000000"
        notes.append("spi.src_clk_hz: no RAW LPSPI instance carries lpspiSrcClk, defaulted to 8000000")
    notes.append("spi.timeout_ms not recoverable from vendor code, defaulted to 100")
    spi_el = {"present": "1" if spi_instances else "0", "src_clk_hz": src_clk_hz,
              "timeout_ms": "100", "instance": spi_instances}

    # --- i2c: lpi2c*.c (VCU) / i2c*.c (BMS) --------------------------------------------------
    i2c_el = {"present": "0", "timeout_ms": "50"}
    for f in sorted(list(vendor_dir.glob("lpi2c*.c")) + list(vendor_dir.glob("i2c*.c"))):
        text = read(f)
        m_cfg = (re.search(r'i2c_master_t\s+\w+\s*=\s*\{([^}]*)\}', text) or
                 re.search(r'lpi2c_master_user_config_t\s+\w+\s*=\s*\{([^}]*)\}', text))
        if not m_cfg:
            continue
        body = m_cfg.group(1)
        ma = re.search(r'\.slaveAddress\s*=\s*(\d+)', body)
        mb = re.search(r'\.baudRate\s*=\s*(\d+)', body)
        i2c_el = {"present": "1", "timeout_ms": "50",
                  "slave_addr": ma.group(1) if ma else "0",
                  "baud": mb.group(1) if mb else "0"}
        break
    notes.append("i2c.timeout_ms not recoverable from vendor code, defaulted to 50")

    # --- adc: adc_pal*.c (groups concatenated across instances, sorted by ADC hw index) ------
    adc_files = []
    for f in sorted(vendor_dir.glob("adc_pal*.c")):
        text = read(f)
        m_idx = re.search(r'ADC_INST_TYPE_ADC_S32K1xx\s*,\s*(\d+)', text)
        if m_idx:
            adc_files.append((int(m_idx.group(1)), text))
    adc_files.sort(key=lambda t: t[0])
    if adc_files:
        resolution = clock_divide = voltage_ref = None
        groups = []
        for _, text in adc_files:
            if resolution is None:
                m = re.search(r'\.resolution\s*=\s*ADC_RESOLUTION_(\w+)', text)
                resolution = m.group(1) if m else None
            if clock_divide is None:
                m = re.search(r'\.clockDivide\s*=\s*ADC_CLK_DIVIDE_(\w+)', text)
                clock_divide = m.group(1) if m else None
            if voltage_ref is None:
                m = re.search(r'\.voltageRef\s*=\s*ADC_VOLTAGEREF_(\w+)', text)
                voltage_ref = m.group(1) if m else None
            groups.extend(re.findall(r'(\d+),\s*/\*\s*numChannels\s*\*/', text))
        adc_el = {"present": "1", "resolution": resolution or "12BIT",
                  "clock_divide": clock_divide or "1", "voltage_ref": voltage_ref or "VREF",
                  "avg_depth": "10", "max_channels": "16",
                  "group": [{"id": str(i), "channels": c} for i, c in enumerate(groups)]}
    else:
        adc_el = {"present": "0", "resolution": "12BIT", "clock_divide": "1", "voltage_ref": "VREF",
                  "avg_depth": "10", "max_channels": "16", "group": []}
    notes.append("adc.avg_depth not recoverable from vendor code, defaulted to 10")
    notes.append("adc.max_channels not recoverable from vendor code, defaulted to 16")

    # --- timer: timing_pal*.c, sorted by FTM hw index ----------------------------------------
    timer_rows = []
    for f in sorted(vendor_dir.glob("timing_pal*.c")):
        text = read(f)
        m_hw = re.search(r'timing_instance_t\s+\w+\s*=\s*\{\s*TIMING_INST_TYPE_FTM\s*,\s*(\d+)', text)
        m_nc = re.search(r'\.numChan\s*=\s*(\d+)', text)
        if not m_hw or not m_nc:
            continue
        hw = int(m_hw.group(1))
        row = {"hw": str(hw), "channels": m_nc.group(1)}
        m_ct = re.search(r'\.chanType\s*=\s*TIMER_CHAN_TYPE_(\w+)', text)
        if m_ct:
            row["mode"] = m_ct.group(1)
        m_cs = re.search(r'\.clockSelect\s*=\s*FTM_CLOCK_SOURCE_(\w+)CLK', text)
        if m_cs and "SYSTEM" != m_cs.group(1):
            row["clk_src"] = m_cs.group(1)
        m_pr = re.search(r'\.prescaler\s*=\s*FTM_CLOCK_DIVID_BY_(\d+)', text)
        if m_pr and "1" != m_pr.group(1):
            row["prescaler"] = m_pr.group(1)
        timer_rows.append((hw, row))
    timer_rows.sort(key=lambda t: t[0])
    timer_instances = [r for _, r in timer_rows]
    timer_el = {"present": "1" if timer_instances else "0", "channel_max": "8", "instance": timer_instances}
    notes.append("timer.channel_max not recoverable from vendor code (HW ceiling, not per-config), defaulted to 8")

    # --- can: can_pal_*.c, sorted by CAN instance id -----------------------------------------
    can_rows = []
    for f in sorted(vendor_dir.glob("can_pal_*.c")):
        text = read(f)
        m_id = re.search(r'can_instance_t\s+\w+\s*=\s*\{\s*CAN_INST_TYPE_FLEXCAN\s*,\s*(\d+)', text)
        m_nom = re.search(r'\.nominalBitrate\s*=\s*\{([^}]*)\}', text)
        if not m_id or not m_nom:
            continue
        cid = int(m_id.group(1))
        body = m_nom.group(1)
        mp = re.search(r'\.propSeg\s*=\s*(\d+)', body)
        m1 = re.search(r'\.phaseSeg1\s*=\s*(\d+)', body)
        m2 = re.search(r'\.phaseSeg2\s*=\s*(\d+)', body)
        mdv = re.search(r'\.preDivider\s*=\s*(\d+)', body)
        mrj = re.search(r'\.rJumpwidth\s*=\s*(\d+)', body)
        row = {"id": str(cid),
               "prop_seg": mp.group(1) if mp else "0",
               "pseg1": m1.group(1) if m1 else "0",
               "pseg2": m2.group(1) if m2 else "0",
               "pre_divider": mdv.group(1) if mdv else "0",
               "rjw": mrj.group(1) if mrj else "0"}
        can_rows.append((cid, row))
    can_rows.sort(key=lambda t: t[0])
    can_instances = [r for _, r in can_rows]
    can_el = {"present": "1" if can_instances else "0", "instance": can_instances}

    # --- watchdog: watchdog1.c (internal) + wdg_pal*.c (external EWM, if present) ------------
    wd_rows = []
    if (vendor_dir / "watchdog1.c").exists():
        text = read("watchdog1.c")
        m_to = re.search(r'\.timeoutValue\s*=\s*(\d+)', text)
        m_clk = re.search(r'\.clkSource\s*=\s*WDOG_(\w+)_CLOCK', text)
        row = {"type": "internal", "timeout": m_to.group(1) if m_to else "0"}
        if m_clk:
            row["clk"] = m_clk.group(1)
        wd_rows.append(row)
    for f in sorted(vendor_dir.glob("wdg_pal1*.c")):
        text = read(f)
        if "WDG_INST_TYPE_WDOG" in text:
            m_to = re.search(r'\.timeoutValue\s*=\s*(\d+)', text)
            wd_rows.append({"type": "internal", "timeout": m_to.group(1) if m_to else "0"})
    watchdog_el = {"present": "1" if wd_rows else "0", "instance": wd_rows}

    # --- crc: crc1.c, HW engine only - no SW CRC equivalent in vendor code -------------------
    if (vendor_dir / "crc1.c").exists():
        text = read("crc1.c")
        m_w = re.search(r'\.crcWidth\s*=\s*CRC_BITS_(\d+)', text)
        m_p = re.search(r'\.polynomial\s*=\s*(0x[0-9A-Fa-f]+)U?', text)
        m_s = re.search(r'\.seed\s*=\s*(0x[0-9A-Fa-f]+)U?', text)
        engine = {"type": "hw"}
        if m_w:
            engine["width"] = m_w.group(1)
        if m_p:
            engine["poly"] = m_p.group(1)
        if m_s:
            engine["seed"] = m_s.group(1)
        crc_el = {"present": "1", "engine": [engine]}
        notes.append("crc.sw: no software CRC engine in vendor code (only HW CRC recovered from crc1.c)")
    else:
        crc_el = {"present": "0"}
        notes.append("crc: no crc1.c in vendor tree, present=0")

    # --- pwm: pwm_pal*.c, first file (by name) wins ------------------------------------------
    pwm_files = sorted(vendor_dir.glob("pwm_pal*.c"))
    if pwm_files:
        text = read(pwm_files[0])
        m_per = re.search(r'\.period\s*=\s*(\d+)', text)
        pwm_el = {"present": "1", "period": m_per.group(1) if m_per else "0"}
    else:
        pwm_el = {"present": "0", "period": "0"}

    # --- rtc: clockMan1.c .sourceRtcClk clock-routing heuristic (no RTC driver in vendor code) -
    m_rtc = re.search(r'\.sourceRtcClk\s*=\s*SIM_RTCCLK_SEL_(\w+)', ctext)
    if m_rtc and m_rtc.group(1).startswith("SOSC"):
        rtc_el = {"present": "1", "type": "external"}
    else:
        rtc_el = {"present": "0"}

    ir = {
        "clock": clock_el, "spi": spi_el, "i2c": i2c_el, "adc": adc_el, "timer": timer_el,
        "can": can_el, "watchdog": watchdog_el, "crc": crc_el, "pwm": pwm_el, "rtc": rtc_el,
    }
    return ir, False, notes


def parse_renesas(vendor_dir):
    """Renesas RA (FSP) ra_gen/ra_cfg parser - EXPERIMENTAL (validated only against DTU).

    No dtu.xml exists to round-trip against, so completeness cannot be measured the way the NXP
    parser is; always returns complete=False. Goal is only a clean, non-crashing rr_hw_cfg.h -
    every emit-required field is filled, with a note wherever a default stands in for a value RA's
    GPT/SPI/ADC/CAN config style does not expose the way the S32K PAL layer does.
    """
    import re

    notes = []
    hal_path = vendor_dir / "hal_data.c"
    if not hal_path.exists():
        sys.exit("error: hal_data.c not found in {} (required for --vendor renesas)".format(vendor_dir))
    text = hal_path.read_text(encoding="utf-8", errors="replace")

    # --- clock: bsp_clock_cfg.h dividers; src/firc have no RA equivalent (XTAL/HOCO tree) ----
    clk_path = vendor_dir / "bsp_clock_cfg.h"
    clk_text = clk_path.read_text(encoding="utf-8", errors="replace") if clk_path.exists() else ""

    def div_of(macro):
        m = re.search(macro + r'\s*\(BSP_CLOCKS_SYS_CLOCK_DIV_(\d+)\)', clk_text)
        return m.group(1) if m else None

    divcore = div_of("BSP_CFG_ICLK_DIV")
    divbus = div_of("BSP_CFG_PCLKB_DIV")
    divslow = div_of("BSP_CFG_FCLK_DIV")
    if divcore is None:
        divcore = "2"
        notes.append("clock.divcore not derivable from bsp_clock_cfg.h, defaulted to 2 (VCU-like)")
    if divbus is None:
        divbus = "2"
        notes.append("clock.divbus not derivable from bsp_clock_cfg.h, defaulted to 2 (VCU-like)")
    if divslow is None:
        divslow = "4"
        notes.append("clock.divslow not derivable from bsp_clock_cfg.h, defaulted to 4 (VCU-like)")
    notes.append("clock.src_hz: RA XTAL/HOCO clock tree has no SIRC-equivalent field, defaulted to 8000000 (VCU-like)")
    notes.append("clock.firc_hz: RA clock tree has no FIRC-equivalent field, defaulted to 48000000 (VCU-like)")
    clock_el = {"src_hz": "8000000", "firc_hz": "48000000", "divcore": divcore, "divbus": divbus, "divslow": divslow}

    # --- spi: g_spi*_cfg / *_ext_cfg blocks in hal_data.c ------------------------------------
    spi_rows = []
    for i, m in enumerate(re.finditer(r'spi_cfg_t\s+g_\w+_cfg\s*=\s*\{([^}]*)\}', text)):
        body = m.group(1)
        m_ch = re.search(r'\.channel\s*=\s*(\d+)', body)
        hw = m_ch.group(1) if m_ch else str(i)
        window = text[max(0, m.start() - 1500):m.start()]
        m_baud = re.search(r'Actual calculated bitrate:\s*(\d+)', window)
        row = {"id": str(i), "hw": hw, "frame_bits": "8"}
        if m_baud:
            row["baud"] = m_baud.group(1)
        else:
            row["baud"] = "1000000"
            notes.append("spi instance {} baud not derivable from hal_data.c, defaulted to 1000000".format(i))
        spi_rows.append(row)
    if spi_rows:
        notes.append("spi.frame_bits: RA SPI bit width is set at runtime (r_spi_write_read_common), "
                      "not in the static cfg struct; defaulted to 8 for all instances")
        notes.append("spi.src_clk_hz not modeled by this parser, defaulted to 8000000")
        notes.append("spi.timeout_ms not recoverable from vendor code, defaulted to 100")
        spi_el = {"present": "1", "src_clk_hz": "8000000", "timeout_ms": "100", "instance": spi_rows}
    else:
        spi_el = {"present": "0", "src_clk_hz": "8000000", "timeout_ms": "100", "instance": []}

    # --- i2c: no I2C peripheral instantiated in the DTU vendor tree -------------------------
    i2c_el = {"present": "0", "timeout_ms": "50"}
    notes.append("i2c: no I2C peripheral found in DTU vendor tree, present=0")

    # --- adc: g_adc0_cfg / g_adc0_channel_cfg ------------------------------------------------
    m_res = re.search(r'\.resolution\s*=\s*ADC_RESOLUTION_(\d+)_BIT', text)
    resolution = (m_res.group(1) + "BIT") if m_res else "12BIT"
    if not m_res:
        notes.append("adc.resolution not recoverable from hal_data.c, defaulted to 12BIT")
    m_scan = re.search(r'\.scan_mask\s*=\s*([^,]*),', text)
    channels = len(re.findall(r'ADC_MASK_CHANNEL_\d+', m_scan.group(1))) if m_scan else 0
    if 0 == channels:
        channels = 1
        notes.append("adc.channels not derivable from scan_mask, defaulted to 1")
    notes.append("adc.clock_divide/voltage_ref not modeled for RA's ADC clock architecture, defaulted to 1/VREF")
    notes.append("adc.avg_depth not recoverable from vendor code, defaulted to 10")
    notes.append("adc.max_channels not recoverable from vendor code, defaulted to 16")
    adc_el = {"present": "1", "resolution": resolution, "clock_divide": "1", "voltage_ref": "VREF",
              "avg_depth": "10", "max_channels": "16", "group": [{"id": "0", "channels": str(channels)}]}

    # --- timer: g_timer*_cfg periodic/one-shot GPT instances; PWM-mode instance feeds pwm ----
    timer_rows = []
    pwm_period = None
    for m in re.finditer(r'timer_cfg_t\s+g_timer\d+_cfg\s*=\s*\{\s*\.mode\s*=\s*TIMER_MODE_(\w+),', text):
        mode_tok = m.group(1)
        window = text[m.start():m.start() + 800]
        m_ch = re.search(r'\.channel\s*=\s*(\d+)', window)
        hw = m_ch.group(1) if m_ch else "0"
        if "PWM" == mode_tok:
            m_per = re.search(r'\.period_counts\s*=\s*(?:\(uint32_t\)\s*)?(0x[0-9A-Fa-f]+|\d+)', window)
            if m_per:
                pwm_period = str(int(m_per.group(1), 0))
            continue
        mode = "ONESHOT" if "ONE_SHOT" == mode_tok else "CONTINUOUS"
        timer_rows.append({"hw": hw, "channels": "1", "mode": mode})
    notes.append("timer.channel_max: GPT is a single-channel-per-unit timer with no fixed channel "
                 "ceiling analogous to FTM, defaulted to 8")
    timer_el = {"present": "1" if timer_rows else "0", "channel_max": "8", "instance": timer_rows}

    # --- can: g_can0_bit_timing_cfg; first #if/#else duplicate definition wins --------------
    m_can = re.search(r'can_bit_timing_cfg_t\s+g_can0_bit_timing_cfg\s*=\s*\{([^}]*)\}', text)
    if m_can:
        body = m_can.group(1)
        m_pre = re.search(r'\.baud_rate_prescaler\s*=\s*(\d+)', body)
        m_p1 = re.search(r'\.time_segment_1\s*=\s*(\d+)', body)
        m_p2 = re.search(r'\.time_segment_2\s*=\s*(\d+)', body)
        m_rjw = re.search(r'\.synchronization_jump_width\s*=\s*(\d+)', body)
        notes.append("can.prop_seg not present in RA bit-timing config, defaulted to 0")
        can_el = {"present": "1", "instance": [{
            "id": "0", "prop_seg": "0",
            "pseg1": m_p1.group(1) if m_p1 else "0",
            "pseg2": m_p2.group(1) if m_p2 else "0",
            "pre_divider": m_pre.group(1) if m_pre else "0",
            "rjw": m_rjw.group(1) if m_rjw else "0"}]}
    else:
        can_el = {"present": "0", "instance": []}

    # --- watchdog: g_wdt0_cfg -----------------------------------------------------------------
    m_wdt = re.search(r'wdt_cfg_t\s+g_wdt0_cfg\s*=\s*\{([^}]*)\}', text)
    if m_wdt:
        m_to = re.search(r'\.timeout\s*=\s*WDT_TIMEOUT_(\d+)', m_wdt.group(1))
        watchdog_el = {"present": "1", "instance": [{"type": "internal", "timeout": m_to.group(1) if m_to else "0"}]}
    else:
        watchdog_el = {"present": "0", "instance": []}

    # --- crc: no CRC peripheral instantiated in the DTU vendor tree -------------------------
    crc_el = {"present": "0"}
    notes.append("crc: no CRC peripheral found in DTU vendor tree, present=0")

    # --- pwm: fed by the GPT PWM-mode timer instance found above -----------------------------
    if pwm_period is not None:
        pwm_el = {"present": "1", "period": pwm_period}
    else:
        pwm_el = {"present": "0", "period": "0"}
        notes.append("pwm: no GPT PWM-mode timer found in hal_data.c, present=0")

    # --- rtc: not modeled by this parser (see note) ------------------------------------------
    rtc_el = {"present": "0"}
    notes.append("rtc: present=0 per parser spec, even though hal_data.c instantiates g_rtc0 "
                 "(RTC round-trip is out of scope for this experimental parser)")

    ir = {
        "clock": clock_el, "spi": spi_el, "i2c": i2c_el, "adc": adc_el, "timer": timer_el,
        "can": can_el, "watchdog": watchdog_el, "crc": crc_el, "pwm": pwm_el, "rtc": rtc_el,
    }
    return ir, False, notes


def parse_stm(vendor_dir):
    """STM32 / CubeMX vendor parser - explicit erroring stub.

    No STM32 product or reference codegen exists in this SDK yet, so there is nothing to parse
    or round-trip against. Errors clearly rather than shipping a speculative, untestable parser
    (plan.md Phase 2 decision). Add an STM product and a CubeMX parser before using this path.
    """
    sys.exit("error: --vendor stm is not implemented. No STM32 product/reference codegen exists "
             "in this SDK yet; add one plus a CubeMX parser before using --source vendor --vendor stm.")


# --- XML write-back (opt-in, --write-xml) ----------------------------------------------------

def ir_to_xml_bytes(ir):
    """Serialize the normalized dict back into a <product> XML document (UTF-8 bytes). Used only
    by --write-xml; not byte-identical to the hand-annotated variants file (loses rich comments),
    but re-parses to the same normalized dict."""
    prod = ET.Element("product")
    for k in ("name", "mcu", "cpu"):
        if k in ir:
            prod.set(k, str(ir[k]))
    for tag, val in ir.items():
        if not isinstance(val, dict):
            continue                             # top-level product attribute, already set
        el = ET.SubElement(prod, tag)
        for ak, av in val.items():
            if isinstance(av, list):
                for row in av:
                    ce = ET.SubElement(el, ak)   # ak is the child tag (instance/group/...)
                    for ck, cv in row.items():
                        ce.set(ck, str(cv))
            else:
                el.set(ak, str(av))
    ET.indent(prod, space="\t")
    xml_body = ET.tostring(prod, encoding="unicode")
    return (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        "<!--\n"
        "  River FW SDK - peripheral configuration (generated from vendor codegen via\n"
        "  core_layer/config/tools/gen_config.py --write-xml). Re-annotate by hand as needed;\n"
        "  this file remains the source of truth for --source xml builds.\n"
        "-->\n"
        + xml_body + "\n"
    ).encode("utf-8")


def write_xml_file(ir, xml_path):
    xml_path.write_bytes(ir_to_xml_bytes(ir))


# --- Shared emit stage -----------------------------------------------------------------------

def emit(product, ir, gen_hash, date_str):
    """Render the normalized dict into rr_hw_cfg.h (source-agnostic)."""
    validate(ir)
    upper = product.upper()

    sections = [("Product", [
        M("RR_PRODUCT_{}".format(upper), "1U", "Selected product build"),
        MT("RR_PRODUCT_NAME", '"{}"'.format(product), "Product name string"),
    ])]
    for fn in SECTION_FNS:
        s = fn(ir)
        if s is not None:
            sections.append(s)

    parts = []
    for title, entries in sections:
        parts.append(subsection(title))
        parts.append("")
        parts.extend(render(entries))
        parts.append("")
    body = "\n".join(parts).rstrip("\n")

    brief = "Consolidated build-time peripheral configuration for the {} product.".format(upper)
    hdr, guard = guard_open(CFG_FILE)
    text = (
        file_header(CFG_FILE, brief, product, date_str, gen_hash)
        + "\n" + hdr + "\n"
        + banner("Public Macros") + "\n\n"
        + body + "\n\n"
        + "#endif /* {} */\n".format(guard)
    )

    inc = GEN_ROOT / product / "inc"
    inc.mkdir(parents=True, exist_ok=True)
    for old in inc.glob("rr_*cfg.h"):            # sweep legacy per-driver / umbrella / renamed headers
        if old.name != CFG_FILE:
            old.unlink()
    (inc / CFG_FILE).write_text(text, encoding="utf-8", newline="\n")
    print("{}: generated {} -> {}".format(product, CFG_FILE, inc / CFG_FILE))


def generate(product, date_str, source="xml", vendor=None, vendor_dir=None, do_write_xml=False):
    """Load the product config from the chosen source, then emit rr_hw_cfg.h."""
    xml_path = CONFIG_DIR / "{}.xml".format(product)

    if "xml" == source:
        if not xml_path.exists():
            sys.exit("error: no config XML at {}".format(xml_path))
        ir = load_from_xml(xml_path)
        gen_hash = hashlib.sha256(xml_path.read_bytes()).hexdigest()[:16]
    else:
        if vendor is None:
            sys.exit("error: --source vendor requires --vendor {nxp,renesas,stm}")
        if vendor_dir is None:
            sys.exit("error: --source vendor requires --vendor-dir <path to vendor codegen>")
        ir, complete, notes = load_from_vendor(Path(vendor_dir), vendor)
        ir.setdefault("name", product)
        gen_hash = hashlib.sha256(json.dumps(ir, sort_keys=True).encode("utf-8")).hexdigest()[:16]
        for n in notes:
            print("  note[{}/{}]: {}".format(vendor, product, n))
        if do_write_xml:
            if not complete:
                sys.exit("error: --write-xml refused - the {} parser did not recover every field "
                         "(see notes above). A partial parse must not overwrite {}.".format(vendor, xml_path))
            write_xml_file(ir, xml_path)
            print("{}: wrote XML {}".format(product, xml_path))

    emit(product, ir, gen_hash, date_str)


def _flatten_ir_for_diff(ir):
    """Flatten the normalized dict IR into {(peripheral, field): value} for the --round-trip diff."""
    flat = {}
    for k, v in ir.items():
        if not isinstance(v, dict):
            flat[("product", k)] = str(v)
            continue
        for ak, av in v.items():
            if isinstance(av, list):
                for i, row in enumerate(av):
                    for rk, rv in row.items():
                        flat[(k, "{}[{}].{}".format(ak, i, rk))] = str(rv)
            else:
                flat[(k, ak)] = str(av)
    return flat


def round_trip_report(product, vendor, vendor_dir):
    """Parse the vendor tree and (if it exists) variants/<product>.xml, then print a field-by-field
    divergence report grouped by peripheral. Diagnostic only - never emits or writes XML."""
    ir_vendor, complete, notes = load_from_vendor(Path(vendor_dir), vendor)
    ir_vendor.setdefault("name", product)
    xml_path = CONFIG_DIR / "{}.xml".format(product)
    ir_xml = load_from_xml(xml_path) if xml_path.exists() else {}

    flat_vendor = _flatten_ir_for_diff(ir_vendor)
    flat_xml = _flatten_ir_for_diff(ir_xml)
    peripherals = sorted(set(k[0] for k in flat_vendor) | set(k[0] for k in flat_xml))

    print("=== round-trip report: {} (vendor={}) ===".format(product, vendor))
    if not ir_xml:
        print("(no variants/{}.xml found - nothing to compare against; all fields VENDOR-ONLY)".format(product))
    for periph in peripherals:
        keys = sorted(set(k[1] for k in flat_vendor if k[0] == periph) |
                       set(k[1] for k in flat_xml if k[0] == periph))
        print("-- {} --".format(periph))
        for key in keys:
            xk = vk = (periph, key)
            in_x, in_v = xk in flat_xml, vk in flat_vendor
            if in_x and in_v:
                if flat_xml[xk] == flat_vendor[vk]:
                    print("  MATCH        {} = {}".format(key, flat_vendor[vk]))
                else:
                    print("  DIFFER       {}: xml={} vendor={}".format(key, flat_xml[xk], flat_vendor[vk]))
            elif in_x:
                print("  XML-ONLY     {} = {}".format(key, flat_xml[xk]))
            else:
                print("  VENDOR-ONLY  {} = {}".format(key, flat_vendor[vk]))
    print("-- notes --")
    for n in notes:
        print("  note: {}".format(n))
    print("complete: {}".format(complete))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--product", help="product name (matches variants/<product>.xml)")
    ap.add_argument("--all", action="store_true", help="regenerate every variants/*.xml (xml source only)")
    ap.add_argument("--date", help="override @date stamp (DD-Mon-YYYY)")
    ap.add_argument("--source", choices=("xml", "vendor"), default="xml",
                    help="config input: xml (default, variants/<product>.xml) or vendor codegen")
    ap.add_argument("--vendor", choices=("nxp", "renesas", "stm"),
                    help="vendor codegen dialect (required with --source vendor)")
    ap.add_argument("--vendor-dir", help="path to the vendor IDE Generated_Code tree (with --source vendor)")
    ap.add_argument("--write-xml", action="store_true",
                    help="with --source vendor: rewrite variants/<product>.xml from the parsed config "
                         "(default OFF; gated on parser completeness). Otherwise only rr_hw_cfg.h is emitted.")
    ap.add_argument("--round-trip", action="store_true",
                    help="with --product/--vendor/--vendor-dir: print a vendor-vs-xml field-by-field "
                         "divergence report and exit (no emit, no write-xml)")
    args = ap.parse_args()

    date_str = args.date or datetime.date.today().strftime("%d-%b-%Y")

    if args.round_trip:
        if not (args.product and args.vendor and args.vendor_dir):
            ap.error("--round-trip requires --product, --vendor and --vendor-dir")
        round_trip_report(args.product, args.vendor, args.vendor_dir)
        return

    if args.all:
        if "xml" != args.source:
            ap.error("--all is only valid with --source xml (vendor mode needs an explicit --product and --vendor-dir)")
        products = sorted(x.stem for x in CONFIG_DIR.glob("*.xml"))
        if not products:
            sys.exit("error: no config XML files in {}".format(CONFIG_DIR))
        for prod in products:
            generate(prod, date_str)
    elif args.product:
        generate(args.product, date_str, source=args.source, vendor=args.vendor,
                 vendor_dir=args.vendor_dir, do_write_xml=args.write_xml)
    else:
        ap.error("pass --product <name> or --all")


if __name__ == "__main__":
    main()
