Steps to follow to run myapp in gem5:

1)If not done already, build gem5 binary;
  a) cd gem5 directory
  b) scons build/X86/gem5.opt (or gem5.debug for DPRINTFs to work), you need
     to have scons installed for this

2) Download linux binary image from: 
  http://www.m5sim.org/dist/current/x86/x86-system.tar.bz2
  and untar. (Not in git due to its large size).

2) Inorder to build myapp,
  a) On Ubuntu 14, run 'Make' inside the sync directory. This will
     create 'myapp' binary compatible with kernel 2.6.24
  b) Copy the myapp binary to the disk image that has file system:
    
    i) mkdir /mntPoint  
    ii) mount -o loop,offset=32256 "path_to_dwnlded_linux_image" /mntPoint
    iii) cp "path_to_myapp" /mntPoint
    iv) umount /mntPoint

3) export M5_PATH="directory of downloaded linux-86.img" (or set it in .bashrc)
4) Inside gem5 dir, run
   
  ./build/X86/gem5.opt configs/example/fs.py --kernel=../binaries/vmlinux-2.6.25 
 --disk-image="path_to_dwnlded_image" -n num_processors

5) In another terminal window, inside gem5/util/term dir,
   a) make
   b) ./m5term localhost 3456

   This would give root shell in the new window. Run 'myapp' in this.

