all : ext2_rm ext2_cp ext2_mkdir ext2_ls ext2_ln

ext2_rm:
	gcc -Wall -g -o ext2_rm ext2_rm.c helper.c

ext2_cp:
	gcc -Wall -g -o ext2_cp ext2_cp.c helper.c

ext2_mkdir:
	gcc -Wall -g -o ext2_mkdir ext2_mkdir.c helper.c

ext2_ls:
	gcc -Wall -g -o ext2_ls ext2_ls.c helper.c

ext2_ln:
	gcc -Wall -g -o ext2_ln ext2_ln.c helper.c

clean:
	rm -f ext2_rm ext2_cp ext2_mkdir ext2_ls ext2_ln
