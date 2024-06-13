# Deep Messenger

Deep messenger is simple console chat application that uses [TOR network](https://www.torproject.org) to create connections between clients and exchange messages. Clients are connected peer-to-peer.

Application can be started in one of two modes, client mode and mailbox mode. In client mode application will start `ncurses` UI which will allow you to chat with other clients. Each client can choose one instance of application running in mailbox mode to act as it's mailbox while the client is offline. This will allow client to pull messages from the mailbox when it comes back online.

Modes and other settings are set using command line switches, for the whole list use `--help` or `-h` switch.

```txt
Usage: deep-messenger [options]
  -h, --help                Show this help message
  -m, --mailbox             Run mailbox service
  -d, --dir <dir>           Set config directory path
  -p, --port <port>         Listen for incomming connections on this port
  -P, --mailbox-port <port> This is the port mailboxes should listen on
  -t, --tor <file>          Path to tor binary (default: /bin/tor)
  -u, --manual              Run messenger in manual mode (for debugging)
  -g, --keygen <uses>       Generate new mailbox access key
  -k, --keys                Show list of all available mailbox access keys
  -r, --keydel <key>        Delete given mailbox access key
  -v, --version             Show application version
```

When you run messenger in client mode additional configuration can be performed using in app commands that can be executed in console tab. To get the whole list use `help` command.

```txt
List of all available commands:
  help                Prints this help message
  sync <onion>        Fetch new messages from given client
  friends             List all friends
  friendadd <onion>   Send friend request to add a new friend
  friendrm <onion>    Remove given friend
  info                Print your account info
  nickname <nick>     Change your nickname to <nick>
  mbreg <onion> <key> Register to given mailbox server
  mbrm                Delete account on your current mailbox server
  mbrmlocal           Remove mailbox account locally
  mbcontacts          Upload contact list to mailbox server
  mbsync              Fetch new messages from mailbox server
  tor                 Start tor client (manual mode)
  version             Prints app and protocol version
```

## Using the app

This app is not production ready, you are free to use it as you wish, but I don't recommend to use it for anything important. It is fairly untested and probably has a lot of bugs that are still to be fixed in the future.

Also, since this app is my gratuation project I had to finish it quickly, so codebase is prone to change in the future.

## Building

Deep Messenger can be built using gnu make (gcc) by running make command.

```bash
make
```

However, to build Deep Messsenger you must first install following dependencies.

```txt
Name        Debian package
----------------------------------------------------
libevent => libevent-dev
sqlite3  => libsqlite3-dev
ncurses  => libncursesw6 (wide character support)
openssl  => libssl-dev (libcrypto)
```

## Project tasks

- [X] Create required UI components (maybe some more, but we'll see)
- [X] Write process start/stop handler
- [X] Implement basic `SOCKS5` client used for communication with TOR client
- [X] Create main protocol handler
- [ ] Create class (structure) for each message type
- [X] Write simple generic queue implementation
- [X] Design some kind of notification mechanism (hooks)
- [X] Write official project documentation
- [X] Connect backend and frontend together
- [ ] Test the whole application

## Fully implemented message types

1. [X] TRANSACTION REQUEST (0x01)
2. [X] TRANSACTION RESPONSE (0x02)
3. [X] FRIEND REQUEST (0x81)
4. [X] ACK ONION (0x82)
5. [X] ACK SIGNATURE (0x83)
6. [X] MESSAGE CONTAINER (0x84)
7. [X] MAILBOX REGISTER (0x85)
8. [X] MAILBOX GRANTED (0x86)
9. [X] MAILBOX FETCH (0x87)
10. [X] MAILBOX SET CONTACTS (0x88)
11. [X] MAILBOX DEL ACCOUNT (0x89)
12. [ ] MAILBOX DEL MESSAGES (0x8A)
13. [X] CLIENT FETCH (0x8B)
14. [X] MESSAGE LIST (0x8C)

## Running tests

Tests are not actual unit tests, they are just files with separate main functions used to test each part of the application. Tests are not guaranteed to work, you will probably need to change something in the test files to make them work for you.

To see the list of all available tests run

```bash
make test.ls
```

To compile and run one of the given tests type

```bash
make tets.run.test_name
```
