"""Disassemble guest code straight from the XBE: python scripts/disx.py <va hex> [bytes hex]"""
import capstone, struct, sys
d=open(__import__('os').path.join(__import__('os').path.dirname(__file__),'..','game','default.xbe'),'rb').read()
base=0x10000
nsec=struct.unpack_from('<I',d,0x11C)[0]; sh=struct.unpack_from('<I',d,0x120)[0]-base
def raw(va):
    for i in range(nsec):
        o=sh+i*56; fl,vaddr,vsz,raddr,rsz=struct.unpack_from('<5I',d,o)
        if vaddr<=va<vaddr+rsz: return raddr+va-vaddr
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
va=int(sys.argv[1],16); n=int(sys.argv[2],16) if len(sys.argv)>2 else 0x200
off=raw(va)
for ins in md.disasm(d[off:off+n],va):
    print(f"{ins.address:08X}  {ins.mnemonic} {ins.op_str}")
