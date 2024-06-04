# Deep Messenger

`TODO: Write app description`

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
