bits 16
mov bx,61443 ; bx:0->f003
mov cx,3841 ; cx:0->f01
sub bx,cx ; bx:f003->e102 ; flags:->S
mov sp,998 ; sp:0->3e6
mov bp,999 ; bp:0->3e7
cmp bp,sp ; flags:S->
add bp,1027 ; bp:3e7->7ea
sub bp,2026 ; bp:7ea->0 ; flags:->Z
