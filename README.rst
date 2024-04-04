Test Concurrent Locking of Single File by Multiple Processes
============================================================

This directory contains C++ test code for testing the locking of a single file by multiple processes. It was developed
to test the behavior of the Lustre "global locking" feature which can be configured on or off. Problems arose when testing
global locking with `casacore <https://github.com/casacore/casacore>`_. In these tests, it appeared as though using
global locking with `CASA <https://casadocs.readthedocs.io/en/latest/index.html>`_ resulted in deadlock.

Implementation
--------------

This test code is written in C++ (:code:`-std=c++20`) and it uses `cppzmq <https://github.com/zeromq/cppzmq>`_ for queueing
up actions for the parallel processes. The locking options are:

* read lock on file immediately
* write lock on file immediately
* read lock on file at a specific time
* write lock on file at a specific time
* release lock immediately (whether held or not)
* release lock immediately (if held)
* release lock at a specific time (whether held or not)
* release lock at a specific time (if held)

The only other command that the locking processes accepts is :code:`stop` which cause them to exit.

Building
--------

First install `cppzmq <https://github.com/zeromq/cppzmq>`_. Then running :code:`make` should build the test executables,
running :code:`make clean` should remove the built files and :code:`make package` should create a tar file in :code:`..`

Running
-------

The test can be run using :code:`./controller /tmp/lock-file 10` which will start :code:`10` locking processes which
will try to lock :code:`/tmp/lock-file` at the same time.

The current :code:`controller.cc` code has all of the locking processes just attempt to acquire a write lock and then
release it. This is done with the code::

      send( "file@"+lock_file );
      send( at(now( ) + seconds(1),"write") );
      send( "releaseif" );
      recv( []( const pair<pid_t,string> &r ) -> bool {
                cout << "Controller received: " << get<1>(r) << " from " << get<0>(r) << endl;
                return true;   /*** true means don't retain result in return ***/
            } );
      send( "stop" );

The locking processes only send a result back to the controller when they encounter an empty command queue. This
implementation is flexible and allows for testing complicated locking behavior.

Mon Mar 18 13:36:34 EDT 2024
------------------------------------------------------------------------------------------------------------------------
Lustre "global" file locking (i.e. the locking across nodes that a distributed filesystem is expected to do) has not
worked reliably. Due to this "local" locking has been used, i.e. file locking which is limited to single node. Switching
to "global" file locking resulted in deadlock.
