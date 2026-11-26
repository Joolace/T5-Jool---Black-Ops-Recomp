"""Generate src/stringed/game_encodings.inc."""
import argparse
from pathlib import Path

OUTPUT = Path(__file__).resolve().parents[2] / "src" / "stringed" / "game_encodings.inc"
SJIS_LEADS = [*range(0x81, 0xA0), *range(0xE0, 0xF0)]
SJIS_TRAILS = [*range(0x40, 0x7F), *range(0x80, 0xFD)]


def code(data, encoding):
    try:
        text = data.decode(encoding)
    except UnicodeDecodeError:
        return 0
    return ord(text) if len(text) == 1 and ord(text) <= 0xFFFF else 0


def table(name, values, per_line=16):
    lines = [f"static constexpr unsigned short {name}[{len(values)}] = {{"]
    for start in range(0, len(values), per_line):
        lines.append("    " + ", ".join(f"0x{value:04X}" for value in values[start:start + per_line]) + ",")
    lines.append("};")
    return lines


def generate():
    lines = []
    for name, encoding in (("cp1252High", "cp1252"), ("cp1251High", "cp1251"), ("cp1250High", "cp1250"),
                           ("cp932High", "cp932")):
        values = [code(bytes([byte]), encoding) for byte in range(0x80, 0x100)]
        lines += table(name, values) + [""]
    korean = [code(bytes([lead, trail]), "cp949") for lead in range(0xB0, 0xC9) for trail in range(0xA1, 0xFF)]
    lines += table("koreanHangul", korean) + [""]
    japanese = [code(bytes([lead, trail]), "cp932") for lead in SJIS_LEADS for trail in SJIS_TRAILS]
    lines += table("japaneseShiftJis", japanese)
    return "\n".join(lines) + "\n"


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--check", action="store_true")
args = parser.parse_args()
generated = generate()
if args.check:
    if not OUTPUT.exists() or OUTPUT.read_text(encoding="ascii") != generated:
        parser.exit(1, "game_encodings.inc is out of date\n")
else:
    OUTPUT.write_text(generated, encoding="ascii", newline="\n")
