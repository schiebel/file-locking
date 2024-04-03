
DEBUG=
all: controller locker

controller: controller.o
	c++ -std=c++20 $(DEBUG) -o $@ $^ -lzmq

locker: locker.o FileLocker.o
	c++ -std=c++20 $(DEBUG) -o $@ $^ -lzmq

package:
	cd .. && tar zcf file-locking.tar.gz file-locking/makefile file-locking/controller.cc file-locking/locker.cc \
                                         file-locking/FileLocker.h file-locking/FileLocker.cc file-locking/README.rst \
                                         file-locking/LICENSE

clean:
	rm locker controller locker.o controller.o FileLocker.o
.cc.o:
	c++ -std=c++20 $(DEBUG) -c $<
