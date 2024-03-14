bits 16
mov ax,1 ; ax:0->1
mov bx,2 ; bx:0->2
mov cx,3 ; cx:0->3
mov dx,4 ; dx:0->4
mov sp,ax ; dl:0->1
mov bp,bx ; dx:0->2
mov si,cx ; bl:0->3
mov di,dx ; bx:0->4
mov dx,sp ; cx:4->1
mov cx,bp ; cl:3->2
mov bx,si ; ax:2->3
mov ax,di ; al:1->4
