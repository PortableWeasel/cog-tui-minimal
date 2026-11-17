"""Screen module cell/cursor checks independent of Cog and readline."""
import ctypes as c
import ctypes.util
import subprocess
import sys
lib = ctypes.util.find_library('vterm')
if not lib:
    sys.exit(77)
v = c.CDLL(lib)
class Rect(c.Structure):
    _fields_ = [(key,c.c_int) for key in ('top','bottom','left','right')]
class Pos(c.Structure):
    _fields_ = [('row',c.c_int),('col',c.c_int)]
def api(name,args,result=None):
    f=getattr(v,name);f.argtypes=args;f.restype=result;return f
new=api('vterm_new',[c.c_int,c.c_int],c.c_void_p)
free=api('vterm_free',[c.c_void_p])
screen=api('vterm_obtain_screen',[c.c_void_p],c.c_void_p)
state=api('vterm_obtain_state',[c.c_void_p],c.c_void_p)
reset=api('vterm_screen_reset',[c.c_void_p,c.c_int])
utf8=api('vterm_set_utf8',[c.c_void_p,c.c_int])
feed=api('vterm_input_write',[c.c_void_p,c.c_char_p,c.c_size_t],c.c_size_t)
text=api('vterm_screen_get_text',[c.c_void_p,c.c_void_p,c.c_size_t,Rect],c.c_size_t)
cursor=api('vterm_state_get_cursorpos',[c.c_void_p,c.POINTER(Pos)])
paste=api('vterm_keyboard_start_paste',[c.c_void_p])
paste_end=api('vterm_keyboard_end_paste',[c.c_void_p])
read_output=api('vterm_output_read',[c.c_void_p,c.c_void_p,c.c_size_t],c.c_size_t)
for case in ('basic','unicode','closed'):
    terminal=new(12,40);utf8(terminal,1);display=screen(terminal);reset(display,1)
    data=subprocess.check_output([sys.argv[1],case]);feed(terminal,data,len(data))
    if case in ('basic','closed'):
        pending=c.create_string_buffer(256)
        read_output(terminal,pending,len(pending))
        paste(terminal)
        count=read_output(terminal,pending,len(pending))
        if case=='basic':
            assert pending.raw[:count]==b'\x1b[200~',pending.raw[:count]
        else:
            assert count==0,pending.raw[:count]
            free(terminal)
            continue
        paste_end(terminal)
        read_output(terminal,pending,len(pending))
    lines=[]
    for row in range(12):
        b=c.create_string_buffer(256);n=text(display,b,len(b),Rect(row,row+1,0,40))
        lines.append(b.raw[:n].decode('utf-8').rstrip())
    p=Pos();cursor(state(terminal),c.byref(p))
    assert lines[0]=='assistant:',lines
    assert lines[1]=='Hello',lines
    assert lines[10]=='--- input ---',lines
    assert p.row==11,(p.row,p.col)
    if case=='basic':
        assert lines[11]=='cog> draft',lines
        assert p.col==8,p.col
    else:
        assert lines[11]=='cog> \\x1b\\u009b界e\\u0301',lines
        assert p.col==15,p.col
    free(terminal)
