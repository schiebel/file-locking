// Server
#include <map>
#include <tuple>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <algorithm>
#include <iostream>
#include <zmq.hpp>
#include <chrono>
#include<functional>
#include <cstdlib>

using namespace std::chrono;
using std::stringstream;
using std::function;
using std::format;
using std::string;
using std::cout;
using std::cerr;
using std::endl;
using std::get;
using std::map;
using std::atol;
using std::tuple;
using std::pair;

inline auto now( ) { return high_resolution_clock::now( ); }
// lock message for starting a lock at 'start' time_point
inline string at( time_point<system_clock> start, const string &action  ) {
    return format("{}@{:%Y-%m-%d %H:%M:%S}", action, start);
}

//tuple<pid_t,string> unpack_reply( const string &reply ) {
pair<pid_t,string> unpack_reply( const string &reply ) {
//void unpack_reply( const string &reply ) {
    char resultBuffer[reply.size( )+1];
    const char *str = reply.c_str( );
    const char *ptr = str;
    for ( char *dest=resultBuffer; *ptr && *ptr != '@'; ++ptr ) {
        *dest++ = *ptr;
    }
    resultBuffer[ptr++-str] = '\0';
    return pair<pid_t,string>(atol(resultBuffer),string(ptr,reply.size( )-(ptr - str)));
}

//*********************************************************************************************
//*** -std=c++20 (gcc version 13.1.1) required for the chrono stuff to work...  ¯\_(ツ)_/¯  ***
//*** https://stackoverflow.com/a/64459832/2903943                                          ***
//*********************************************************************************************
int main( int argc, char *argv[] ) {
    if ( argc != 3 ) {
        cerr << "Expected two arguments, file to lock and number of lockers to create." << endl;
        exit(1);
    }

    struct stat statbuf;
    string lock_file(argv[1]);
    if ( stat( lock_file.c_str(), &statbuf ) != 0 ) {
        cerr << "File to lock is expected to be the first argument." << endl;
        exit(1);
    }
    
    int locker_count = atoi( argv[2] );
    if ( locker_count <= 0 ) {
        cerr << "Locker count argument must be a natural number (i.e. greater than zero)." << endl;
        exit(1);
    }
    if ( locker_count > 100 ) {
        cerr << "Sorry " << locker_count << " is too many agents for me to create." << endl;
        exit(1);
    }

    cout << "Controller will create " << locker_count << " locking agents." << endl;
    std::vector<zmq::context_t*> ctrl_contexts(locker_count);
    std::vector<zmq::socket_t*> ctrl_sockets(locker_count);

    auto send = [&ctrl_sockets]( string message ) {
                    cout << "Controller sending: '" << message  << "'" << endl;
                    for_each( ctrl_sockets.begin( ), ctrl_sockets.end( ),
                              [message](zmq::socket_t *sock) {
                                  // Send a request
                                  zmq::message_t r1( message );
                                  sock->send(r1); } );
                    fflush(stdout);
                };

    auto recv = [&ctrl_sockets]( function<bool(const pair<pid_t,string> &)> filter = [](const pair<pid_t,string> &) -> bool { return false; } ) -> map<pid_t,string> {
                    map<pid_t,string> result;
                    cout << "Controller Receiving results..." << endl;
                    fflush(stdout);
                    for_each( ctrl_sockets.begin( ), ctrl_sockets.end( ),
                              [&result,&filter](zmq::socket_t *sock) {
                                  // Recieve reply...
                                  zmq::message_t reply;
                                  sock->recv(reply);
                                  pair<pid_t,string> nv = unpack_reply(string(static_cast<const char*>(reply.data( )),reply.size( )));
                                  if ( filter(nv) == false ) result.insert(nv);
                              } );
                    return result;
                };

    // Create a context
    zmq::context_t context(1);

    // Create a REP socket
    zmq::socket_t socket(context, zmq::socket_type::rep);
    try { socket.bind("tcp://*:*"); }
    catch ( zmq::error_t &e ) {
        cerr << "couldn't bind to socket: " << e.what();
        exit(1);
    }

    char port[1024]; // make this sufficiently large.
                     // otherwise an error will be thrown because of invalid argument.
    size_t size = sizeof(port);
    socket.getsockopt( ZMQ_LAST_ENDPOINT, &port, &size );
    cout << "Controller listening on port " << port << endl;

    for ( int i=0; i < locker_count; ++i ) {
        pid_t pid = fork( );
        if ( pid ) {
            // PARENT: Receive a request
            zmq::message_t request;
            socket.recv(request);
            string child_port( static_cast<const char*>(request.data( )), request.size( )-1 );
            cout << "Controller received locker " << pid << " port: " << child_port << endl;

            // Send a reply
            zmq::message_t reply("ready");
            socket.send(reply);

            // Create a context
            ctrl_contexts[i] = new zmq::context_t(1);

            // Create a PAIR socket
            ctrl_sockets[i] = new zmq::socket_t( *ctrl_contexts[i], zmq::socket_type::pair);

            // Connect the socket to controller
            ctrl_sockets[i]->connect(child_port);

        } else {

            cout << "Controller starting locker " << getpid() << "." << endl;
            fflush(stdout);
            execlp( "./locker", "locker", port, NULL );

        }
    }

    send( "file@"+lock_file );
    send( at(now( ) + seconds(1),"write") );
    send( "releaseif" );
    recv( []( const pair<pid_t,string> &r ) -> bool {
              cout << "Controller received: " << get<1>(r) << " from " << get<0>(r) << endl;
              return true;   /*** true means don't retain result in return ***/
          } );
    send( "stop" );

    int status = 0;
    pid_t cpid = 0;
    while ( (cpid = waitpid(-1, &status, 0)) > 0 ) {
        cout << "Locker " << cpid << " exited." << endl;
    }

    return 0;
}
