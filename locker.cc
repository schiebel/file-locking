// Client
#include <algorithm>
#include <unistd.h>
#include <iostream>
#include <iterator>
#include <zmq.hpp>
#include <stdio.h>
#include <tuple>
#include <vector>
#include <string>
#include <initializer_list>
#include <stdio.h>     /*** sscanf ***/
#include <format>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "FileLocker.h"

using namespace std::chrono;
using std::ostream_iterator;
using std::shared_ptr;
using std::tuple;
using std::string;
using std::vector;
using std::make_tuple;
using std::get;
using std::copy;
using std::prev;
using std::cout;
using std::endl;
using std::format;

pid_t pid = getpid( );

enum Command { WRITELOCK, WRITELOCKNOW, READLOCK, READLOCKNOW,
               RELEASELOCK, RELEASELOCKIF, RELEASELOCKNOW, RELEASELOCKNOWIF,
               LOCKFILE, STOP, ERROR };
enum Action { WRITE, READ, RELEASE };

inline std::ostream& operator<<( std::ostream &os, Action act ) {
    switch (act) {
    case WRITELOCK: os << "write lock";
               break;
    case READ: os << "read lock";
               break;
    case RELEASE: os << "release lock";
               break;
    default: os << "unknown";
    }
    return os;
}

inline std::ostream& operator<<( std::ostream &os, Command cmd ) {
    switch (cmd) {
    case WRITELOCK: os << "write lock";
               break;
    case WRITELOCKNOW: os << "immediate write lock";
               break;
    case READLOCK: os << "read lock";
               break;
    case READLOCKNOW: os << "immediate read lock";
               break;
    case RELEASELOCK: os << "release lock";
               break;
    case RELEASELOCKIF: os << "release lock if held";
               break;
    case RELEASELOCKNOW: os << "immediate release lock";
               break;
    case RELEASELOCKNOWIF: os << "immediate release lock if held";
               break;
    case LOCKFILE: os << "set lock file";
               break;
    case STOP: os << "stop";
               break;
    case ERROR: os << "ERROR";
               break;
    }
    return os;
}

class FDLock {
public:
    FDLock( ) { }
    FDLock( const FDLock &other ) {
        lock = other.lock;
    }
    FDLock( const string &p ) {
        lock.reset(new LockInfo(p,open( p.c_str( ), O_RDWR )));
    }
    operator bool( ) { return lock ? true : false; }
    FDLock &operator=( FDLock other) {
        lock = other.lock;
        return *this;
    }
    FDLock &operator=( const string &p ) {
        lock.reset(new LockInfo(p,open( p.c_str( ), O_RDWR )));
        return *this;
    }
    bool action( Action act ) {
        bool result = true;
        last_local_error = "";
        if ( lock ) {
            if ( act == RELEASE ) {
                result = lock->lock.release( );
            } else {
                result = lock->lock.acquire( act == READ ? FileLocker::Read : FileLocker::Write, 60 );
            }
        } else {
            last_local_error = "lock not initialized";
            result = false;
        }
        if ( result == false )
            cout << "Locker " << pid << " ACTION ERROR " << error_message( ) << endl;
        else
            cout << "Locker " << pid << " ACTION SUCCESS: " << act << endl;
        return result;
    }
    bool locked( ) const {
        return lock->lock.hasLock( FileLocker::Read ) || lock->lock.hasLock( FileLocker::Write );
    }
    string error_message( ) const {
        if ( last_local_error.size( ) != 0 ) return last_local_error;
        else return lock->lock.lastMessage( );
    }
private:
    struct LockInfo {
        string path;
        FileLocker lock;
        int fd;
        LockInfo( const string &p, int d ) : path(p), fd(d), lock(d) { }
        ~LockInfo( ) {
            if ( fd != -1 ) close(fd);
        }
    };
    string last_local_error;
    shared_ptr<LockInfo> lock;
};

inline auto now( ) { return high_resolution_clock::now( ); }

time_point<system_clock> parsetime( const std::string &s ) {
    // 2024-03-21 17:04:22.022828043
    long Y=0, m=0, d=0, H=0, M=0, S=0;
    double S_fraction=0;
    sscanf( s.c_str( ), "%4d-%2d-%2d %d:%2d:%2d%lf",
            &Y, &m, &d, &H, &M, &S, &S_fraction );

    // Create a tm struct to represent the date and time.
    tm t = {0};
    t.tm_year = Y - 1900;
    t.tm_mon = m - 1;
    t.tm_mday = d;
    t.tm_hour = H;
    t.tm_min = M;
    t.tm_sec = S;
    

    // Create a time_t value from the tm struct...
    // Note that 'mktime(&t)' assumes that 't' is expressed in local time (instead of UTC).
    // The output of a "time_point" is expressed in UTC. So this 't' is already UTC.
    time_t time = timegm(&t);

    // Create a time_point from the time_t value.
    return high_resolution_clock::from_time_t(time) + nanoseconds( (long long) (S_fraction * 1000000000.0) );
}

Action ctoa( Command cmd ) {
    switch ( cmd ) {
    case WRITELOCK:
    case WRITELOCKNOW:
        return WRITE;
    case READLOCK:
    case READLOCKNOW:
        return READ;
    default:
        return RELEASE;
    }
}

std::ostream& operator<<( std::ostream &os, const tuple<Command,vector<string>> &cmd ) {
    os << get<0>(cmd);
    const vector<string> &vec = get<1>(cmd);
    if( vec.empty( ) ) return os;
    os << ": ";
    copy(vec.begin( ), prev(vec.end( )), ostream_iterator<string>( os, ", " ));
    os << vec.back( );
    return os;
}

template<class... Args>
inline tuple<Command,vector<string>> make_result( Command c, Args... args ) {
    vector<string> argsvec { args... };
    return tuple<Command,vector<string>>( c, argsvec );
}

tuple<Command,vector<string>> unpack_command( const string &cmd ) {
    if ( cmd == "stop" ) { 
        return make_result( STOP );
    } else if ( cmd.rfind("write@", 0) == 0 ) {
        if ( cmd.size( ) > 6 ) {   // expect cmd == "write@<time>"
            return make_result( WRITELOCK, cmd.substr(6) );
        } else {
            return make_result( ERROR, "write lock command contains too few arguments" );
        }
    } else if ( cmd.rfind("read@", 0) == 0 ) {
        if ( cmd.size( ) > 5 ) {   // expect cmd == "read@<time>"
            return make_result( READLOCK, cmd.substr(5) );
        } else {
            return make_result( ERROR, "read lock command contains too few arguments" );
        }
    } else if ( cmd.rfind("release@", 0) == 0 ) {
        if ( cmd.size( ) > 8 ) {   // expect cmd == "release@<time>"
            return make_result( RELEASELOCK, cmd.substr(8) );
        } else {
            return make_result( ERROR, "release lock command contains too few arguments" );
        }
    } else if ( cmd.rfind("releaseif@", 0) == 0 ) {
        if ( cmd.size( ) > 10 ) {   // expect cmd == "releaseif@<time>"
            return make_result( RELEASELOCKIF, cmd.substr(5) );
        } else {
            return make_result( ERROR, "release lock if held command contains too few arguments" );
        }
    } else if ( cmd == "write" ) {
            return make_result( WRITELOCKNOW );
    } else if ( cmd == "read" ) {
            return make_result( READLOCKNOW );
    } else if ( cmd == "release" ) {
            return make_result( RELEASELOCKNOW );
    } else if ( cmd == "releaseif" ) {
            return make_result( RELEASELOCKNOWIF );
    } else if ( cmd.rfind("file@", 0) == 0 ) {
        if ( cmd.size( ) > 5 ) {   // expect cmd == "file@<full-qualified-path>"
            return make_result( LOCKFILE, cmd.substr(5) );
        } else {
            return make_result( ERROR, "file command contains too few arguments" );
        }
    }
    return make_result( ERROR, "command not recognized" );
}


inline void microsleep( microseconds t ) {
    if ( t > microseconds(0) ) usleep( t.count() );
    else cout << "Locker " << pid << " ERROR negative sleep value " << t << endl;
}

inline bool message_waiting( zmq::socket_t &socket ) {
    zmq::pollitem_t items[] = { { socket, 0, ZMQ_POLLIN } };    
    return zmq_poll(items, 1, 0) > 0;
}



//void execute_command_now( Action cmd, shared_ptr<FileLocker> lock ) {
void execute_command_now( Action cmd, FDLock &lock ) {
    cout << "Locker " << pid << " doing action " << cmd << endl;
    fflush(stdout);
    if ( lock.action( cmd ) == false ) {
        cout << "Locker " << pid << " ERROR " << lock.error_message( ) << endl;
    }
}

int main( int argc, char *argv[] ) {

    if ( argc != 2 ) {
        std::cerr << "Expected one argument with port address." << endl;
        exit(1);
    }

    cout << "Locker " << pid << " starting." << endl;
    fflush(stdout);

    //********************************************************************************
    //** Create command port
    //********************************************************************************
    // Create a context
    zmq::context_t cmd_context(1);

    // Create a REP socket
    zmq::socket_t cmd_socket(cmd_context, zmq::socket_type::pair);
    try { cmd_socket.bind("tcp://*:*"); }
    catch ( zmq::error_t &e ) {
        std::cerr << "In locker " << getpid() << " couldn't bind to command socket: " << e.what();
        exit(1);
    }

    char port[1024]; // make this sufficiently large.
                     // otherwise an error will be thrown because of invalid argument.
    size_t size = sizeof(port);
    cmd_socket.getsockopt( ZMQ_LAST_ENDPOINT, &port, &size );
    cout << "Locker " << pid << " listening on port " << port << " [" << strlen(port) << "]" << endl;
    fflush(stdout);

    //********************************************************************************
    //** Send command port to controller
    //********************************************************************************
    // Create a context
    zmq::context_t context(1);

    // Create a REQ socket
    zmq::socket_t socket(context, zmq::socket_type::req);

    // Connect the socket to controller
    socket.connect(argv[1]);

    // Send a request
    zmq::message_t request(port, strlen(port)+1);
    socket.send(request);

    // Receive a reply
    zmq::message_t reply;
    socket.recv(reply);

    // Check reply...
    std::string init((const char*) reply.data( ));
    if ( init != "ready" ) {
        socket.close( );
        cout << "Locker " << pid << " did not receive the expected response." << endl;
        exit(1);
    }

    FDLock lock;
    string reply_prefix = format("{}@",pid);
    bool continue_looping = true;
    while ( continue_looping ) {
        zmq::message_t request;
        cmd_socket.recv(request);
        auto command = unpack_command(string( static_cast<const char*>(request.data()), request.size( ) ));
        cout << "Locker " << pid << " received command " << command << endl;
        fflush(stdout);

        auto command_type = get<0>(command);
        switch ( command_type ) {
        case LOCKFILE:
            lock = get<1>(command)[0];
            //lock.reset(new FileLocker(get<1>(command)[0]));
            break;
        case RELEASELOCKIF:
            if ( ! lock.locked( ) ) {
                cout << now( ) << ": Locker " << pid << " NOT executing " << command_type << " no lock held" << endl;
                break;
            }
        case RELEASELOCK:
        case READLOCK:
        case WRITELOCK:
            //if ( lock ) {
            if ( lock ) {
                auto lt = parsetime(get<1>(command)[0]);
                //microsleep( now( ) - parsetime(get<1>(command)) );
                //sleep_until(parsetime(get<1>(command)[0]));
                //cout << "Locker " << pid << " locking now." << endl;
                cout << now( ) << ": Locker " << pid << " executing " << command_type << " in " << (duration_cast<microseconds>(lt - now( ))) << endl;
                fflush(stdout);
                microsleep( duration_cast<microseconds>(lt - now( )) );
                cout << now( ) << ": Locker " << pid << " after sleep " << endl;
                //execute_command_now( ctoa(command_type), lock );
                execute_command_now( ctoa(command_type), lock );
            } else {
                cout << "Locker " << pid << " ERROR no file specified " << now( ) << endl;
                zmq::message_t reply(reply_prefix + "ERROR no file specified");
                cmd_socket.send(reply);
            }
            break;
        case RELEASELOCKNOWIF:
            if ( ! lock.locked( ) ) {
                cout << now( ) << ": Locker " << pid << " NOT executing " << command_type << " no lock held" << endl;
                break;
            }
        case RELEASELOCKNOW:
        case READLOCKNOW:
        case WRITELOCKNOW:
            //execute_command_now( ctoa(command_type), lock );
            execute_command_now( ctoa(command_type), lock );
            break;
        case STOP:
            cmd_socket.close( );
            continue_looping = false;
            break;
        }

        if ( continue_looping && message_waiting( cmd_socket ) == false ) {
            // send back update when there are no messages waiting
            zmq::message_t reply(reply_prefix + "OK");
            cout << "Locker " << pid << " reply: " << string( static_cast<const char*>(reply.data()), reply.size( ) ) << endl;
            cmd_socket.send(reply);
        }
        
    }

    return 0;
}
