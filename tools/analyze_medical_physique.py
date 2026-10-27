"""Read-only disassembly using the installed KenshiLib stub/RVA mapping."""
from pathlib import Path
import struct
import sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_64


class PE:
    def __init__(self, path):
        self.data = Path(path).read_bytes()
        pe = self.u32(0x3c)
        optional = pe + 24
        self.base = struct.unpack_from('<Q', self.data, optional + 24)[0]
        self.directories = [struct.unpack_from('<II', self.data, optional + 112 + 8*i) for i in range(16)]
        self.sections = []
        table = optional + self.u16(pe + 20)
        for i in range(self.u16(pe + 6)):
            off = table + i*40
            size, rva, raw_size, raw = struct.unpack_from('<IIII', self.data, off + 8)
            self.sections.append((rva, max(size, raw_size), raw))

    def u16(self, off):
        return struct.unpack_from('<H', self.data, off)[0]

    def u32(self, off):
        return struct.unpack_from('<I', self.data, off)[0]

    def offset(self, rva):
        for start, size, raw in self.sections:
            if start <= rva < start + size:
                return raw + rva - start
        raise ValueError(hex(rva))

    def exports(self):
        off = self.offset(self.directories[0][0])
        count = self.u32(off + 24)
        functions = self.offset(self.u32(off + 28))
        names = self.offset(self.u32(off + 32))
        ordinals = self.offset(self.u32(off + 36))
        for i in range(count):
            name_off = self.offset(self.u32(names + 4*i))
            name = self.data[name_off:self.data.index(b'\0', name_off)].decode('ascii')
            yield name, self.u32(functions + 4*self.u16(ordinals + 2*i))

    def end(self, rva):
        start, size = self.directories[3]
        off = self.offset(start)
        root = None
        entries = []
        for i in range(size // 12):
            begin, end, unwind = struct.unpack_from('<III', self.data, off + i*12)
            entries.append((begin, end, unwind))
            if begin <= rva < end:
                root = (begin, end, unwind)
        if root:
            # MSVC splits functions into chained RUNTIME_FUNCTION records;
            # the first record may cover only the register-saving prologue.
            end = root[1]
            parents = {root}
            changed = True
            while changed:
                changed = False
                for entry in entries:
                    if entry in parents:
                        continue
                    u = self.offset(entry[2])
                    if (self.data[u] >> 3) & 4:
                        tail = u + 4 + ((self.data[u+2] + 1)//2)*4
                        parent = struct.unpack_from('<III', self.data, tail)
                        if parent in parents:
                            parents.add(entry)
                            end = max(end, entry[1])
                            changed = True
            return end
        # Leaf functions have no unwind record. Stop at their first RET;
        # this fallback is sufficient for the tiny health getters below.
        code = self.data[self.offset(rva):self.offset(rva)+192]
        for ins in Cs(CS_ARCH_X86, CS_MODE_64).disasm(code, self.base+rva):
            if ins.mnemonic in ('ret', 'int3'):
                return ins.address - self.base + (ins.size if ins.mnemonic == 'ret' else 0)
        return rva + len(code)


if __name__ == '__main__':
    game = Path(r'D:/Program Files (x86)/Steam/steamapps/common/Kenshi')
    lib = PE(game / 'KenshiLib.dll')
    exe = PE(game / 'RE_Kenshi/Kenshi_x64.exe')
    rvas = (game / 'RE_Kenshi/RVAs/Steam_1.0.65.br').read_bytes()
    patterns = sys.argv[1:] or ['updatePysique', 'updatePhysiqueMuscle', 'updateProportions', '_NV_updateAppearance@AppearanceHuman', '_NV_periodicUpdate@AppearanceHuman', '_setHealth@MedicalSystem', 'updateDerivedHealths', 'maxHealth@HealthPartStatus']
    disassembler = Cs(CS_ARCH_X86, CS_MODE_64)
    for name, stub in lib.exports():
        if not any(pattern in name for pattern in patterns):
            continue
        index = (stub - 0x15b46) // 6
        if stub < 0x15b46 or (stub - 0x15b46) % 6 or index*4 + 4 > len(rvas):
            print('Skipping non-function export:', name)
            continue
        rva = struct.unpack_from('<I', rvas, index*4)[0]
        end = exe.end(rva)
        print('\n%s stub=%#x index=%d rva=%#x end=%#x' % (name, stub, index, rva, end))
        off = exe.offset(rva)
        for ins in disassembler.disasm(exe.data[off:off+end-rva], exe.base+rva):
            print('%08x  %-8s %s' % (ins.address-exe.base, ins.mnemonic, ins.op_str))
