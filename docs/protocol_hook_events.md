# Protocol hook events

List of all hook events that can occur on protocol main object. These events can be fired by main protocol object or one of the message type objects.

```txt
PROT_MAIN_EV_DONE
Protocol handler is done processing all messages

PROT_MAIN_EV_CLOSE
Connection closed for any reason (error code provided in pmain->status)

PROT_MB_ACC_DELETE_EV_OK
Mailbox account deleted successfully

PROT_MB_ACC_DELETE_EV_FAIL
Failed to delete mailbox account

PROT_MB_ACC_REGISTER_EV_OK
Mailbox account created succesfully

PROT_MB_ACC_REGISTER_EV_FAIL
Failed to create mailbox account

PROT_MB_SET_CONTACTS_EV_OK
Managed to successfully update contacts list on the mailbox server

PROT_MB_SET_CONTACTS_EV_FAIL
Failed to update contacts list on the mailbox server
```

**I am not sure if this is up to date...**
