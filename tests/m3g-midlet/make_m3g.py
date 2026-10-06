# Tạo file .m3g thử nghiệm theo đặc tả JSR-184 (little-endian)
import struct, zlib, math
objs = []
def add(t, data):
    objs.append((t, data)); return len(objs)   # chỉ số (header = 1)
u8 = lambda v: struct.pack('<B', v)
u16 = lambda v: struct.pack('<H', v)
i32 = lambda v: struct.pack('<i', v)
u32 = lambda v: struct.pack('<I', v)
f32 = lambda v: struct.pack('<f', v)
b = lambda v: u8(1 if v else 0)
def obj3d(uid=0, tracks=()):
    return u32(uid) + u32(len(tracks)) + b''.join(u32(t) for t in tracks) + u32(0)
def transformable(uid=0, tracks=(), t=(0,0,0), s=(1,1,1), ang=0, axis=(0,0,1)):
    return obj3d(uid, tracks) + b(True) + b''.join(f32(x) for x in t) + b''.join(f32(x) for x in s) + f32(ang) + b''.join(f32(x) for x in axis) + b(False)
def node(uid=0, tracks=(), **kw):
    return transformable(uid, tracks, **kw) + b(True) + b(True) + u8(255) + i32(-1) + b(False)
rgb = lambda c: bytes([(c>>16)&255, (c>>8)&255, c&255])
rgba = lambda c: bytes([(c>>16)&255, (c>>8)&255, c&255, (c>>24)&255])

add(0, u8(1)+u8(0) + b(False) + u32(0) + u32(0) + b'test\x00')
# Ảnh 8x8 RGB sọc
px = b''.join(bytes([255,80,40]) if ((x//2+y//2)&1) else bytes([40,220,90]) for y in range(8) for x in range(8))
img = add(10, obj3d() + u8(99) + b(False) + u32(8) + u32(8) + u32(0) + u32(len(px)) + px)
tex = add(17, transformable() + u32(img) + rgb(0) + u8(227) + u8(241) + u8(241) + u8(208) + u8(210))
mat = add(13, obj3d() + rgb(0x333333) + rgba(0xffffffff) + rgb(0) + rgb(0x808080) + f32(20) + b(False))
pm  = add(8, obj3d() + u8(162) + u8(165) + u8(168) + b(False) + b(False) + b(True))
app = add(3, obj3d() + u8(0) + u32(0) + u32(0) + u32(pm) + u32(mat) + u32(1) + u32(tex))
# Hình chóp 4 mặt (mỗi mặt 1 strip 3 đỉnh) + đáy
P = [(0,100,0)]; base = [(-80,-60,-80),(80,-60,-80),(80,-60,80),(-80,-60,80)]
tris = []
verts = []; nrms = []; tcs = []
def face(a, bb, c):
    ux,uy,uz = (bb[0]-a[0], bb[1]-a[1], bb[2]-a[2]); vx,vy,vz = (c[0]-a[0], c[1]-a[1], c[2]-a[2])
    nx,ny,nz = uy*vz-uz*vy, uz*vx-ux*vz, ux*vy-uy*vx; l = math.sqrt(nx*nx+ny*ny+nz*nz)
    for p, t in ((a,(0,0)),(bb,(255,0)),(c,(128,255))):
        verts.append(p); nrms.append((int(nx/l*127), int(ny/l*127), int(nz/l*127))); tcs.append(t)
for i in range(4):
    face(base[(i+1)%4], base[i], P[0])
pos = b''.join(struct.pack('<hhh', *v) for v in verts)
va_pos = add(20, obj3d() + u8(2) + u8(3) + u8(0) + u16(len(verts)) + pos)
va_nrm = add(20, obj3d() + u8(1) + u8(3) + u8(0) + u16(len(verts)) + b''.join(struct.pack('<bbb', *n) for n in nrms))
va_tc  = add(20, obj3d() + u8(2) + u8(2) + u8(0) + u16(len(verts)) + b''.join(struct.pack('<hh', *t) for t in tcs))
vb = add(21, obj3d() + rgba(0xffffffff) + u32(va_pos) + f32(0) + f32(0) + f32(0) + f32(0.01) + u32(va_nrm) + u32(0)
         + u32(1) + u32(va_tc) + f32(0) + f32(0) + f32(0) + f32(1/255.0))
tsa = add(11, obj3d() + u8(0) + u32(0) + u32(4) + u32(3)*4)
# Animation: xoay quanh trục Y, 4 khoá trong 2000ms, lặp
keys = []
for k in range(4):
    ang = math.radians(k * 90) / 2
    keys.append((k * 500, (0, math.sin(ang), 0, math.cos(ang))))
ks = add(19, obj3d() + u8(177) + u8(193) + u8(0) + u32(2000) + u32(0) + u32(3) + u32(4) + u32(4)
         + b''.join(u32(t) + b''.join(f32(x) for x in q) for t, q in keys))
ac = add(1, obj3d() + f32(1) + f32(1) + i32(0) + i32(0) + f32(0) + i32(0))
tr = add(2, obj3d() + u32(ks) + u32(ac) + u32(268))
mesh = add(14, node(uid=42, tracks=(tr,)) + u32(vb) + u32(1) + u32(tsa) + u32(app))
cam = add(5, node(t=(0, 0.3, 2.6)) + u8(50) + f32(60) + f32(0.75) + f32(0.1) + f32(50))
light = add(12, node(ang=-40, axis=(1,0,0)) + f32(1) + f32(0) + f32(0) + rgb(0xffffff) + u8(129) + f32(1) + f32(45) + f32(0))
amb = add(12, node() + f32(1) + f32(0) + f32(0) + rgb(0xffffff) + u8(128) + f32(0.35) + f32(45) + f32(0))
bg = add(4, obj3d() + rgba(0xff102030) + u32(0) + u8(32) + u8(32) + i32(0) + i32(0) + i32(0) + i32(0) + b(True) + b(True))
world = add(22, node(uid=1) + u32(4) + u32(mesh) + u32(cam) + u32(light) + u32(amb) + u32(cam) + u32(bg))

ident = bytes([0xAB,0x4A,0x53,0x52,0x31,0x38,0x34,0xBB,0x0D,0x0A,0x1A,0x0A])
def section(objlist, compress):
    body = b''.join(u8(t) + u32(len(d)) + d for t, d in objlist)
    data = zlib.compress(body) if compress else body
    total = 9 + len(data) + 4
    head = u8(1 if compress else 0) + u32(total) + u32(len(body))
    return head + data + u32(zlib.adler32(head + data))
out = ident + section(objs[:1], False) + section(objs[1:], True)
open(__import__('os').path.join(__import__('os').path.dirname(__file__) or '.', 'res', 'scene.m3g'), 'wb').write(out)
print(len(out), 'bytes,', len(objs), 'objects')
