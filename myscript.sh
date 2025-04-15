echo testing for redirection
cd testfiles/test2
pwd
gcc testpipe1.0.c -o pipe10
gcc testpipe1.1.c -o pipe11
echo now we will pipe the programs
./pipe10 | ./pipe11
echo next test
gcc testpipe2.0.c -o pipe20
gcc testpipe2.1.c -o pipe21
./pipe20 | ./pipe21
echo test 2 done
exit
