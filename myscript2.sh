echo test 1 will test basic cd, pwd, and executing to make sure the shell is working
cd sandbox
cd session1
pwd
cd abcd
cd ..
pwd
cd session1
pwd
./badexecutable
gcc helloworld.c -o helloworld
./helloworld
./helloworld > output.txt
exit
