#!/usr/bin/env python3
"""Translate the subset of Microsoft armasm used by the CE 5.0 ARM kernel into GNU as syntax.

usage: armasm2gas.py in.s out.S

Only what armtrap.s, ExVector.s, intrlock.s and ksarm.h need is handled: EQU, the ^/# map directives, MACRO/MEND,
IF/ELSE/ENDIF, DCD/DCW/DCB, %, numeric local labels (%F/%B), IMPORT/EXPORT, AREA, INCLUDE, LTORG, CODE16/32.
The output is meant to be edited by hand afterwards (the MMU parts need rewriting anyway).
"""
import re
import sys

OPS = [(":LOR:", "||"), (":LAND:", "&&"), (":OR:", "|"), (":AND:", "&"), (":EOR:", "^"), (":SHL:", "<<"),
       (":SHR:", ">>"), (":NOT:", "~"), (":MOD:", "%"), ("{TRUE}", "1"), ("{FALSE}", "0")]


DEFINED = {"ARMV4I", "ARMV4T", "ARM", "_ARM_"}


def expr(e):
    e = re.sub(r":DEF:(\w+)", lambda m: "1" if m.group(1).upper() in DEFINED else "0", e, flags=re.I)
    for a, b in OPS:
        e = re.sub(re.escape(a), b, e, flags=re.I)
    e = re.sub(r"\b2_([01]+)\b", lambda m: "0b" + m.group(1), e)
    e = re.sub(r"\b8_([0-7]+)\b", lambda m: "0" + m.group(1), e)
    e = re.sub(r"\|([A-Za-z_.][\w.$]*)\|", r"\1", e)   # |name| quoting
    return e


def strip_comment(line):
    out, q = [], False
    for i, ch in enumerate(line):
        if ch == '"':
            q = not q
        if ch == ";" and not q:
            return line[:i].rstrip(), line[i:]
        out.append(ch)
    return line.rstrip(), ""


def local_refs(s):
    s = re.sub(r"%F(\d+)", r"\1f", s, flags=re.I)
    s = re.sub(r"%B(\d+)", r"\1b", s, flags=re.I)
    return s


def convert(lines):
    out = []
    cur = "0"          # '^' map cursor
    in_macro = None     # list of param names while inside a macro body
    macro_hdr = False
    for raw in lines:
        raw = raw.rstrip("\n").replace("\t", "    ")
        code, comment = strip_comment(raw)
        cm = ("@" + comment[1:]) if comment else ""
        if not code.strip():
            out.append(cm)
            continue
        indented = code[0] == " "
        toks = code.split(None, 2)
        if macro_hdr:
            # prototype line: [label] name $p1, $p2
            macro_hdr = False
            body = code.strip()
            parts = re.split(r"[\s,]+", body)
            name, params = parts[0], [p[1:] for p in parts[1:] if p.startswith("$")]
            if parts[0].startswith("$"):   # label-parameter form: "$lab name"
                name = parts[1]
                params = [parts[0][1:]] + [p[1:] for p in parts[2:] if p.startswith("$")]
            in_macro = params
            out.append(f".macro {name} {', '.join(params)}")
            continue
        up = toks[0].upper() if toks else ""
        if indented and up == "MACRO":
            macro_hdr = True
            continue
        if indented and up == "MEND":
            out.append(".endm")
            in_macro = None
            continue

        def fix(s):
            s = local_refs(s)
            if in_macro is not None:
                for p in in_macro:
                    s = re.sub(r"\$" + re.escape(p) + r"\b", "\\\\" + p, s)
            return s

        line = code.strip()
        if indented:
            label, rest = "", line
        else:
            m = re.match(r"(\S+)\s*(.*)", line)
            label, rest = m.group(1), m.group(2)
        w0 = rest.split(None, 1)
        op0 = w0[0].upper() if w0 else ""
        arg0 = w0[1] if len(w0) > 1 else ""
        if label and op0 == "EQU":
            out.append(f".equ {label}, {expr(fix(arg0))} {cm}")
            continue
        if op0 == "#" or (rest.startswith("#") and not indented):
            size = expr(rest[1:].strip()) if op0 != "#" else expr(arg0)
            if label:
                out.append(f".equ {label}, {cur}")
            cur = f"({cur})+({size})"
            continue
        if op0 == "%":
            if label:
                out.append(f"{label}:")
            out.append(f"    .space {expr(arg0)} {cm}")
            continue
        if label:
            if re.fullmatch(r"\d+", label):
                out.append(f"{label}:")
            else:
                out.append(f"{label.strip('|')}:")
        if not rest:
            continue
        w = rest.split(None, 1)
        op, args = w[0], (w[1] if len(w) > 1 else "")
        U = op.upper()
        if U == "^":
            cur = expr(args)
            continue
        if U == "#":
            out.append(f"@ map skip {args}")
            cur = f"({cur})+({expr(args)})"
            continue
        if U in ("OPT", "TTL", "SUBT", "END", "KEEP", "ROUT", "ENTRY"):
            continue
        if U == "INCLUDE":
            out.append(f'.include "{args.strip().rsplit(".",1)[0]}.inc"')
            continue
        if U == "IF":
            out.append(f".if {expr(fix(args))}")
            continue
        if U == "ELSE":
            out.append(".else")
            continue
        if U == "ENDIF":
            out.append(".endif")
            continue
        if U == "IMPORT":
            out.append(f".extern {args.strip()}")
            continue
        if U == "EXPORT":
            out.append(f".global {args.split('[')[0].strip().strip('|')}")
            continue
        if U == "AREA":
            nm = args.split(",")[0].strip().strip("|")
            if "NOINIT" in args.upper():
                out.append(f'.section {nm},"aw",%nobits')
            elif "DATA" in args.upper():
                out.append(f'.section {nm},"aw"')
            else:
                out.append(".text")
            continue
        if U == "LTORG":
            out.append(".ltorg")
            continue
        if U == "CODE32":
            out.append(".arm")
            continue
        if U == "CODE16":
            out.append(".thumb")
            continue
        if U == "DCD":
            out.append(f"    .word {expr(fix(args))}")
            continue
        if U == "DCW":
            out.append(f"    .hword {expr(fix(args))}")
            continue
        if U == "DCB":
            out.append(f"    .byte {expr(fix(args))}")
            continue
        if U == "ALIGN":
            out.append("    .balign 4")
            continue
        if U == "SPACE":
            out.append(f"    .space {expr(args)}")
            continue
        # plain instruction or macro invocation
        out.append(f"    {op} {expr(fix(args))} {cm}".rstrip())
    return out


def main():
    src = open(sys.argv[1], encoding="latin-1").read().splitlines()
    out = convert(src)
    open(sys.argv[2], "w", encoding="latin-1").write("\n".join(out) + "\n")


if __name__ == "__main__":
    main()
