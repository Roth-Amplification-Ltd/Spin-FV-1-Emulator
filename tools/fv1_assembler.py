#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Small, self-contained FV-1 assembler used by the factory-bank build.

This intentionally supports the SpinASM subset used by the eight programs in
this directory. It emits one 512-byte program image, padded to 128 FV-1
instructions. The implementation follows the public Spin Semiconductor
instruction encoding.
"""
from __future__ import annotations

import ast
import re
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Sequence

PROG_INSTRUCTIONS = 128
PROGRAM_BYTES = 512
DELAY_SAMPLES = 32767

M1 = 0x01
M2 = 0x03
M5 = 0x1F
M6 = 0x3F
M9 = 0x1FF
M11 = 0x7FF
M15 = 0x7FFF
M16 = 0xFFFF
M24 = 0xFFFFFF

SYMBOLS: Dict[str, int | float] = {
    "SIN0_RATE": 0x00, "SIN0_RANGE": 0x01,
    "SIN1_RATE": 0x02, "SIN1_RANGE": 0x03,
    "RMP0_RATE": 0x04, "RMP0_RANGE": 0x05,
    "RMP1_RATE": 0x06, "RMP1_RANGE": 0x07,
    "POT0": 0x10, "POT1": 0x11, "POT2": 0x12,
    "ADCL": 0x14, "ADCR": 0x15, "DACL": 0x16, "DACR": 0x17,
    "ADDR_PTR": 0x18,
    "SIN0": 0x00, "SIN1": 0x01, "RMP0": 0x02, "RMP1": 0x03,
    "RDA": 0x00, "SOF": 0x02, "RDAL": 0x03,
    "SIN": 0x00, "COS": 0x01, "REG": 0x02, "COMPC": 0x04,
    "COMPA": 0x08, "RPTR2": 0x10, "NA": 0x20,
    "RUN": 0x10, "ZRC": 0x08, "ZRO": 0x04, "GEZ": 0x02, "NEG": 0x01,
}
for i in range(32):
    SYMBOLS[f"REG{i}"] = 0x20 + i


class AssemblyError(RuntimeError):
    pass


@dataclass
class ParsedInstruction:
    mnemonic: str
    args: List[str]
    line_no: int
    source_line: str
    address: int


class SafeExpressionEvaluator(ast.NodeVisitor):
    """Evaluate the arithmetic/bitwise expression subset accepted here."""

    def __init__(self, symbols: Dict[str, int | float]):
        self.symbols = symbols

    def visit_Expression(self, node: ast.Expression):
        return self.visit(node.body)

    def visit_Constant(self, node: ast.Constant):
        if isinstance(node.value, (int, float)):
            return node.value
        raise AssemblyError(f"Unsupported constant: {node.value!r}")

    def visit_Name(self, node: ast.Name):
        key = node.id.upper()
        if key not in self.symbols:
            raise AssemblyError(f"Undefined symbol: {node.id}")
        return self.symbols[key]

    def visit_UnaryOp(self, node: ast.UnaryOp):
        value = self.visit(node.operand)
        if isinstance(node.op, ast.USub):
            return -value
        if isinstance(node.op, ast.UAdd):
            return +value
        if isinstance(node.op, ast.Invert):
            if not isinstance(value, int):
                raise AssemblyError("Bitwise invert requires an integer")
            return ~value
        raise AssemblyError("Unsupported unary operator")

    def visit_BinOp(self, node: ast.BinOp):
        left = self.visit(node.left)
        right = self.visit(node.right)
        op = node.op
        if isinstance(op, ast.Add): return left + right
        if isinstance(op, ast.Sub): return left - right
        if isinstance(op, ast.Mult): return left * right
        if isinstance(op, ast.Div): return left / right
        if isinstance(op, ast.FloorDiv): return left // right
        if isinstance(op, ast.Pow): return left ** right
        if isinstance(op, ast.BitOr): return int(left) | int(right)
        if isinstance(op, ast.BitAnd): return int(left) & int(right)
        if isinstance(op, ast.BitXor): return int(left) ^ int(right)
        if isinstance(op, ast.LShift): return int(left) << int(right)
        if isinstance(op, ast.RShift): return int(left) >> int(right)
        raise AssemblyError("Unsupported binary operator")

    def visit_Call(self, node: ast.Call):
        if not isinstance(node.func, ast.Name) or node.func.id.lower() != "int" or len(node.args) != 1:
            raise AssemblyError("Only int(expression) calls are supported")
        return int(round(self.visit(node.args[0])))

    def generic_visit(self, node):
        raise AssemblyError(f"Unsupported expression node: {type(node).__name__}")


def normalize_expression(expr: str) -> str:
    expr = expr.strip().replace("$", "0x")
    expr = re.sub(r"%([01_]+)", lambda m: "0b" + m.group(1).replace("_", ""), expr)
    expr = re.sub(r"\b([A-Za-z_][A-Za-z0-9_]*)\^", r"\1__MID", expr)
    expr = re.sub(r"\b([A-Za-z_][A-Za-z0-9_]*)#", r"\1__END", expr)
    return expr


def eval_expr(expr: str, symbols: Dict[str, int | float]):
    normalized = normalize_expression(expr)
    try:
        tree = ast.parse(normalized, mode="eval")
        return SafeExpressionEvaluator(symbols).visit(tree)
    except (SyntaxError, ValueError) as exc:
        raise AssemblyError(f"Invalid expression {expr!r}: {exc}") from exc


def fixed(value, ref: int, minimum: float, maximum: float, mask: int, name: str) -> int:
    if isinstance(value, int):
        if value < 0:
            return value & mask
        if value > mask:
            raise AssemblyError(f"{name} integer operand {value} exceeds 0x{mask:X}")
        return value
    f = float(value)
    if not minimum <= f <= maximum:
        raise AssemblyError(f"{name} real operand {f} outside {minimum}..{maximum}")
    return int(round(f * ref)) & mask


def s1_9(v): return fixed(v, 512, -2.0, 1.998046875, M11, "S1.9")
def s1_14(v): return fixed(v, 16384, -2.0, 1.99993896484375, M16, "S1.14")
def s_10(v): return fixed(v, 1024, -1.0, 0.9990234375, M11, "S.10")
def s_15(v): return fixed(v, 32768, -1.0, 0.999969482421875, M16, "S.15")
def s_23(v): return fixed(v, 8388608, -1.0, 0.9999998807907104, M24, "S.23")


def parse_source(text: str) -> tuple[List[ParsedInstruction], Dict[str, int | float], Dict[str, int]]:
    symbols: Dict[str, int | float] = dict(SYMBOLS)
    labels: Dict[str, int] = {}
    instructions: List[ParsedInstruction] = []
    delay_cursor = 0

    for line_no, raw in enumerate(text.splitlines(), start=1):
        source_line = raw.rstrip("\n")
        line = raw.split(";", 1)[0].strip()
        if not line:
            continue

        # One or more labels may prefix a directive/instruction.
        while True:
            match = re.match(r"^([A-Za-z_][A-Za-z0-9_]*):\s*(.*)$", line)
            if not match:
                break
            label = match.group(1).upper()
            if label in labels:
                raise AssemblyError(f"Line {line_no}: duplicate target {label}")
            labels[label] = len(instructions)
            line = match.group(2).strip()
            if not line:
                break
        if not line:
            continue

        parts = line.split(None, 2)
        p0 = parts[0].upper()
        p1 = parts[1].upper() if len(parts) > 1 else ""

        if p0 == "MEM" or p1 == "MEM":
            if p0 == "MEM":
                if len(parts) < 3:
                    raise AssemblyError(f"Line {line_no}: MEM requires name and length")
                name, expr = parts[1], parts[2]
            else:
                if len(parts) < 3:
                    raise AssemblyError(f"Line {line_no}: MEM requires length")
                name, expr = parts[0], parts[2]
            key = name.upper()
            length = eval_expr(expr, symbols)
            if int(length) != length or length < 0:
                raise AssemblyError(f"Line {line_no}: invalid MEM length {length}")
            length = int(length)
            if delay_cursor + length > DELAY_SAMPLES:
                raise AssemblyError(
                    f"Line {line_no}: delay RAM exhausted ({delay_cursor + length}>{DELAY_SAMPLES})"
                )
            symbols[key] = delay_cursor
            symbols[f"{key}__MID"] = delay_cursor + length // 2
            symbols[f"{key}__END"] = delay_cursor + length
            delay_cursor += length + 1
            continue

        if p0 == "EQU" or p1 == "EQU":
            if p0 == "EQU":
                if len(parts) < 3:
                    raise AssemblyError(f"Line {line_no}: EQU requires name and expression")
                name, expr = parts[1], parts[2]
            else:
                if len(parts) < 3:
                    raise AssemblyError(f"Line {line_no}: EQU requires expression")
                name, expr = parts[0], parts[2]
            symbols[name.upper()] = eval_expr(expr, symbols)
            continue

        mnemonic, _, arg_text = line.partition(" ")
        mnemonic = mnemonic.upper()
        args = [a.strip() for a in arg_text.split(",")] if arg_text.strip() else []
        instructions.append(ParsedInstruction(mnemonic, args, line_no, source_line, len(instructions)))

    if len(instructions) > PROG_INSTRUCTIONS:
        raise AssemblyError(f"Program has {len(instructions)} instructions; FV-1 limit is 128")
    return instructions, symbols, labels


def encode_instruction(ins: ParsedInstruction, symbols: Dict[str, int | float], labels: Dict[str, int]) -> int:
    m = ins.mnemonic
    a = ins.args

    def need(n: int):
        if len(a) != n:
            raise AssemblyError(f"Line {ins.line_no}: {m} expects {n} operands, got {len(a)}")

    def ex(i: int):
        return eval_expr(a[i], symbols)

    def reg(i: int):
        value = ex(i)
        if int(value) != value or not 0 <= int(value) <= 0x3F:
            raise AssemblyError(f"Line {ins.line_no}: invalid register {a[i]}")
        return int(value)

    def addr15(i: int):
        value = ex(i)
        if isinstance(value, int):
            if not -0x8000 <= value <= 0x7FFF:
                raise AssemblyError(f"Line {ins.line_no}: address out of range: {value}")
            return value & M15
        return s_15(value) & M15

    if m == "RDA":
        need(2); return (s1_9(ex(1)) << 21) | (addr15(0) << 5) | 0x00
    if m == "RMPA":
        need(1); return (s1_9(ex(0)) << 21) | 0x01
    if m == "WRA":
        need(2); return (s1_9(ex(1)) << 21) | (addr15(0) << 5) | 0x02
    if m == "WRAP":
        need(2); return (s1_9(ex(1)) << 21) | (addr15(0) << 5) | 0x03
    if m == "RDAX":
        need(2); return (s1_14(ex(1)) << 16) | (reg(0) << 5) | 0x04
    if m == "RDFX":
        need(2); return (s1_14(ex(1)) << 16) | (reg(0) << 5) | 0x05
    if m == "LDAX":
        need(1); return (reg(0) << 5) | 0x05
    if m == "WRAX":
        need(2); return (s1_14(ex(1)) << 16) | (reg(0) << 5) | 0x06
    if m == "WRHX":
        need(2); return (s1_14(ex(1)) << 16) | (reg(0) << 5) | 0x07
    if m == "WRLX":
        need(2); return (s1_14(ex(1)) << 16) | (reg(0) << 5) | 0x08
    if m == "MAXX":
        need(2); return (s1_14(ex(1)) << 16) | (reg(0) << 5) | 0x09
    if m == "ABSA":
        need(0); return 0x09
    if m == "MULX":
        need(1); return (reg(0) << 5) | 0x0A
    if m == "LOG":
        need(2); return (s1_14(ex(0)) << 16) | (s_10(ex(1)) << 5) | 0x0B
    if m == "EXP":
        need(2); return (s1_14(ex(0)) << 16) | (s_10(ex(1)) << 5) | 0x0C
    if m == "SOF":
        need(2); return (s1_14(ex(0)) << 16) | (s_10(ex(1)) << 5) | 0x0D
    if m == "AND":
        need(1); return (s_23(ex(0)) << 8) | 0x0E
    if m == "CLR":
        need(0); return 0x0E
    if m == "OR":
        need(1); return (s_23(ex(0)) << 8) | 0x0F
    if m == "XOR":
        need(1); return (s_23(ex(0)) << 8) | 0x10
    if m == "NOT":
        need(0); return (M24 << 8) | 0x10
    if m in {"SKP", "JMP"}:
        if m == "SKP":
            need(2); cond = int(ex(0)) & M5; target_expr = a[1]
        else:
            need(1); cond = 0; target_expr = a[0]
        target_key = target_expr.strip().upper()
        if re.fullmatch(r"[A-Z_][A-Z0-9_]*", target_key) and target_key in labels:
            offset = labels[target_key] - ins.address - 1
        else:
            offset = int(eval_expr(target_expr, symbols))
        if not 0 <= offset <= M6:
            raise AssemblyError(f"Line {ins.line_no}: skip offset {offset} out of range")
        return (cond << 27) | (offset << 21) | 0x11
    if m == "NOP":
        need(0); return 0x11
    if m == "WLDS":
        need(3)
        lfo = int(ex(0))
        freq = int(ex(1))
        if not 0 <= lfo <= 1 or not 0 <= freq <= M9:
            raise AssemblyError(f"Line {ins.line_no}: invalid WLDS operands")
        amp = s_15(ex(2)) & M15
        return (lfo << 29) | (freq << 20) | (amp << 5) | 0x12
    if m == "WLDR":
        need(3)
        lfo = int(ex(0))
        if lfo in (2, 3):
            lfo -= 2
        if not 0 <= lfo <= 1:
            raise AssemblyError(f"Line {ins.line_no}: invalid RMP LFO")
        freq_value = ex(1)
        if isinstance(freq_value, int):
            if not -0x8000 <= freq_value <= 0x7FFF:
                raise AssemblyError(f"Line {ins.line_no}: invalid ramp rate")
            freq = freq_value & M16
        else:
            freq = s_15(freq_value)
        amp_value = int(ex(2))
        amp_map = {4096: 0, 2048: 1, 1024: 2, 512: 3, 0: 0, 1: 1, 2: 2, 3: 3}
        if amp_value not in amp_map:
            raise AssemblyError(f"Line {ins.line_no}: invalid ramp range {amp_value}")
        return ((lfo | 0x2) << 29) | (freq << 13) | (amp_map[amp_value] << 5) | 0x12
    if m == "JAM":
        need(1)
        lfo = int(ex(0))
        if lfo in (2, 3): lfo -= 2
        if not 0 <= lfo <= 1:
            raise AssemblyError(f"Line {ins.line_no}: invalid JAM LFO")
        return ((lfo | 0x2) << 6) | 0x13
    if m == "CHO":
        if len(a) not in (2, 3, 4):
            raise AssemblyError(f"Line {ins.line_no}: CHO expects 2-4 operands")
        chotype = a[0].strip().upper()
        lfo = int(eval_expr(a[1], symbols))
        if not 0 <= lfo <= 3:
            raise AssemblyError(f"Line {ins.line_no}: invalid CHO LFO")
        if chotype == "RDAL":
            flags = int(eval_expr(a[2], symbols)) if len(a) >= 3 and a[2] else int(symbols["REG"])
            address = 0
            type_code = 3
        else:
            if len(a) != 4:
                raise AssemblyError(f"Line {ins.line_no}: CHO {chotype} expects LFO, flags, value")
            flags = int(eval_expr(a[2], symbols)) if a[2] else 0
            value = eval_expr(a[3], symbols)
            address = s_15(value) if not isinstance(value, int) else value & M16
            type_code = {"RDA": 0, "SOF": 2}.get(chotype, -1)
            if type_code < 0:
                raise AssemblyError(f"Line {ins.line_no}: invalid CHO type {chotype}")
        return (type_code << 30) | ((flags & M6) << 24) | ((lfo & M2) << 21) | ((address & M16) << 5) | 0x14
    if m == "RAW":
        need(1); return int(ex(0)) & 0xFFFFFFFF

    raise AssemblyError(f"Line {ins.line_no}: unsupported mnemonic {m}")


def assemble_text(text: str) -> tuple[bytes, int, int]:
    instructions, symbols, labels = parse_source(text)
    words = [encode_instruction(ins, symbols, labels) for ins in instructions]
    words.extend([0x00000011] * (PROG_INSTRUCTIONS - len(words)))
    return b"".join(struct.pack(">I", word) for word in words), len(instructions), max(
        [int(v) for k, v in symbols.items() if k.endswith("__END")] + [0]
    )


def assemble_file(source: Path, output: Path) -> tuple[int, int]:
    binary, instruction_count, max_delay = assemble_text(source.read_text(encoding="utf-8"))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(binary)
    return instruction_count, max_delay


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="Assemble one FV-1 SpinASM source file")
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    count, max_delay = assemble_file(args.source, args.output)
    print(f"Assembled {args.source}: {count} instructions, highest delay address {max_delay}, {PROGRAM_BYTES} bytes")
