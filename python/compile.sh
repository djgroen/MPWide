rm _MPWide.so
python setup.py build_ext --inplace
#gcc -pthread -fno-strict-aliasing -DNDEBUG -g -fwrapv -O2 -Wall -Wstrict-prototypes -fPIC -I/usr/include/python2.6 -c ../MPWide.cpp -o build/temp.linux-x86_64-2.6/../MPWide.o -g -O0
#gcc -pthread -fno-strict-aliasing -DNDEBUG -g -fwrapv -O2 -Wall -Wstrict-prototypes -fPIC -I/usr/include/python2.6 -c ../Socket.cpp -o build/temp.linux-x86_64-2.6/../Socket.o -g -O0
#gcc -pthread -fno-strict-aliasing -DNDEBUG -g -fwrapv -O2 -Wall -Wstrict-prototypes -fPIC -I/usr/include/python2.6 -c MPWide_wrap.cxx -o build/temp.linux-x86_64-2.6/MPWide_wrap.o -g -O0
#g++ -pthread -shared -Wl,-O1 -Wl,-Bsymbolic-functions build/temp.linux-x86_64-2.6/../MPWide.o build/temp.linux-x86_64-2.6/../Socket.o build/temp.linux-x86_64-2.6/MPWide_wrap.o -o /home/derek/Codes/MPWide/python/_MPWide.so

