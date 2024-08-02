# DB options table

Following table contains list of all options application uses. All options are stored in options database table, this table is shared between client and mailbox service in case they are using the same database. All client related options start with `client_` and mailbox related strat with `mailbox_`.

Other options may be added as needed in the future.

| Key                          | Type |
|------------------------------|:----:|
| onion_address                | Text |
| onion_public_key             | Bin  |
| onion_private_key            | Bin  |
| client_nickname              | Text |
| client_mailbox_id            | Bin  |
| client_mailbox_onion_address | Text |
| client_mailbox_sig_pub_key   | Bin  |
| client_mailbox_sig_priv_key  | Bin  |

**I am not sure if this is up to date...**
